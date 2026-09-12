#include "WorldView/AnastasisWorldEmbodiment.h"
#include "ProceduralMeshComponent.h"
#include "WorldView/AnastasisTerrainSurface.h"


#include "Anastasis_UnrealV2.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "WorldView/AnastasisWorldDebugVisual.h"

static TAutoConsoleVariable<int32> CVarTerrainSurface(TEXT("anastasis.Terrain.Surface"), 0, TEXT("Experimental center-sampled terrain. 0=legacy DEBUG, 1=32x32 surface; applied on embodiment."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarWorldViewSeed(
	TEXT("anastasis.WorldView.Seed"),
	12345,
	TEXT("Seed for CaptureCanonicalWorld (always GenerateWorld(seed, 96, 96))."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWorldViewWidth(
	TEXT("anastasis.WorldView.Width"),
	96,
	TEXT("Crop width in tiles applied to the canonical 96x96 world. Not GenerateWorld width."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWorldViewHeight(
	TEXT("anastasis.WorldView.Height"),
	96,
	TEXT("Crop height in tiles applied to the canonical 96x96 world. Not GenerateWorld height."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWorldViewCropX(
	TEXT("anastasis.WorldView.CropX"),
	0,
	TEXT("Crop origin X in canonical tile coordinates."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarWorldViewCropY(
	TEXT("anastasis.WorldView.CropY"),
	0,
	TEXT("Crop origin Y in canonical tile coordinates."),
	ECVF_Default);

namespace
{
	constexpr int32 SampleXY[][2] = {
		{0, 0}, {1, 1}, {3, 7}, {10, 20}, {15, 15}, {31, 31}, {30, 4}, {8, 29}, {95, 95}
	};

	FName TerrainComponentName(int32 TypeIndex)
	{
		return FName(*FString::Printf(
			TEXT("Tiles_%s"),
			AnastasisWorld::TileTypeName(static_cast<AnastasisWorld::ETileType>(TypeIndex))));
	}
}

AAnastasisWorldEmbodiment::AAnastasisWorldEmbodiment()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMat.Succeeded())
	{
		BaseShapeMaterial = ShapeMat.Object;
	}

	static_assert(AnastasisWorld::TileTypeCount == 7, "TerrainMeshes[7] must match ETileType");

	for (int32 TypeIndex = 0; TypeIndex < AnastasisWorld::TileTypeCount; ++TypeIndex)
	{
		UHierarchicalInstancedStaticMeshComponent* Mesh = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
			TerrainComponentName(TypeIndex));
		Mesh->SetupAttachment(Root);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCastShadow(false);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetCanEverAffectNavigation(false);
		if (CubeMesh.Succeeded())
		{
			Mesh->SetStaticMesh(CubeMesh.Object);
		}
		TerrainMeshes[TypeIndex] = Mesh;
	}
}

void AAnastasisWorldEmbodiment::BeginPlay()
{
	Super::BeginPlay();
	EmbodyCrop(
		static_cast<uint32>(FMath::Max(0, CVarWorldViewSeed.GetValueOnGameThread())),
		FMath::Max(0, CVarWorldViewCropX.GetValueOnGameThread()),
		FMath::Max(0, CVarWorldViewCropY.GetValueOnGameThread()),
		FMath::Max(1, CVarWorldViewWidth.GetValueOnGameThread()),
		FMath::Max(1, CVarWorldViewHeight.GetValueOnGameThread()));
}

bool AAnastasisWorldEmbodiment::Embody(uint32 Seed, int32 Width, int32 Height)
{
	return EmbodyCrop(Seed, 0, 0, Width, Height);
}

bool AAnastasisWorldEmbodiment::EmbodyCrop(uint32 Seed, int32 OriginX, int32 OriginY, int32 Width, int32 Height)
{
	if (Width <= 0 || Height <= 0)
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_WORLDVIEW embody rejected: invalid crop %dx%d"), Width, Height);
		return false;
	}

	for (int32 TypeIndex = 0; TypeIndex < AnastasisWorld::TileTypeCount; ++TypeIndex)
	{
		if (UHierarchicalInstancedStaticMeshComponent* Mesh = TerrainMeshes[TypeIndex])
		{
			Mesh->ClearInstances();
			if (BaseShapeMaterial)
			{
				UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseShapeMaterial, this);
				Mid->SetVectorParameterValue(
					TEXT("Color"),
					AnastasisWorldDebugVisual::TerrainDebugColor(
						static_cast<AnastasisWorld::ETileType>(TypeIndex)));
				Mesh->SetMaterial(0, Mid);
			}
		}
	}

	Snapshot = AnastasisWorldView::CropSnapshot(
		AnastasisWorldView::CaptureCanonicalWorld(Seed),
		OriginX,
		OriginY,
		Width,
		Height);
	Plan = AnastasisWorldView::BuildPlan(Snapshot);
	LocalInstanceIndex.Init(INDEX_NONE, Plan.TileCount);

	if (Plan.TileCount != Width * Height)
	{
		UE_LOG(
			LogAnastasis_UnrealV2,
			Error,
			TEXT("ANASTASIS_WORLDVIEW crop rejected: origin=(%d,%d) size=%dx%d source=%dx%d tiles=%d"),
			OriginX,
			OriginY,
			Width,
			Height,
			Snapshot.SourceW,
			Snapshot.SourceH,
			Plan.TileCount);
		LogEmbodiment();
		return false;
	}

	for (int32 Index = 0; Index < Plan.TileCount; ++Index)
	{
		const uint8 TypeIndex = static_cast<uint8>(Plan.Types[Index]);
		UHierarchicalInstancedStaticMeshComponent* Mesh = TerrainMeshes[TypeIndex];
		if (!Mesh || !Mesh->GetStaticMesh())
		{
			continue;
		}

		const FTransform Transform = AnastasisWorldDebugVisual::TileTransform(Plan.Locations[Index]);
		LocalInstanceIndex[Index] = Mesh->AddInstance(Transform, false);
	}

	for (int32 TypeIndex = 0; TypeIndex < AnastasisWorld::TileTypeCount; ++TypeIndex)
	{
		if (UHierarchicalInstancedStaticMeshComponent* Mesh = TerrainMeshes[TypeIndex])
		{
			Mesh->MarkRenderStateDirty();
		}
	}

    if (ExperimentalSurface) ExperimentalSurface->SetVisibility(false);
    for (auto& Mesh : TerrainMeshes) if (Mesh) Mesh->SetVisibility(true);
    if (CVarTerrainSurface.GetValueOnGameThread() == 1)
    {
        const auto Crop = AnastasisWorldView::CropSnapshot(Snapshot, 0, 0, 32, 32);
        AnastasisTerrainSurface::FGeometry Geometry;
        if (AnastasisTerrainSurface::Build(Crop, Geometry))
        {
            if (!ExperimentalSurface)
            {
                ExperimentalSurface = NewObject<UProceduralMeshComponent>(this, TEXT("ExperimentalTerrain"));
                ExperimentalSurface->SetupAttachment(GetRootComponent());
                ExperimentalSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                ExperimentalSurface->SetCanEverAffectNavigation(false);
                ExperimentalSurface->SetCastShadow(true);
                ExperimentalSurface->RegisterComponent();
            }
            // Section 0 : relief. La couleur de sommet porte toute la semantique du sol.
            ExperimentalSurface->CreateMeshSection_LinearColor(0, Geometry.Vertices, Geometry.Triangles,
                Geometry.Normals, TArray<FVector2D>{}, Geometry.Colors, TArray<FProcMeshTangent>{}, false);
            // Section 1 : nappe d'eau plate au niveau de la mer, encastree dans le relief.
            ExperimentalSurface->ClearMeshSection(1);
            if (Geometry.WaterTriangles.Num() > 0)
            {
                TArray<FLinearColor> WaterColors;
                WaterColors.Init(FLinearColor(0.043f, 0.176f, 0.290f, 1.0f), Geometry.WaterVertices.Num());
                ExperimentalSurface->CreateMeshSection_LinearColor(1, Geometry.WaterVertices, Geometry.WaterTriangles,
                    Geometry.WaterNormals, TArray<FVector2D>{}, WaterColors, TArray<FProcMeshTangent>{}, false);
            }
            if (UMaterialInterface* SurfaceMaterial = ResolveSliceMaterial())
            {
                ExperimentalSurface->SetMaterial(0, SurfaceMaterial);
                ExperimentalSurface->SetMaterial(1, SurfaceMaterial);
            }
            ExperimentalSurface->SetVisibility(true);
            for (auto& Mesh : TerrainMeshes) if (Mesh) Mesh->SetVisibility(false);
            UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_TERRAIN source=96x96 crop=(0,0) 32x32 tiles=1024 vertices=%d triangles=%d water_triangles=%d material=%s boundary=tile_centers legacy_visible=0"),
                Geometry.Vertices.Num(), Geometry.Triangles.Num()/3, Geometry.WaterTriangles.Num()/3,
                SliceMaterial ? TEXT("slice") : TEXT("fallback"));
        }
        else UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_TERRAIN rejected crop; legacy DEBUG retained"));
    }
    else UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_TERRAIN disabled legacy_visible=1"));
    LogEmbodiment();
    return GetInstanceCount() == Plan.TileCount;
}

bool AAnastasisWorldEmbodiment::EmbodyCanonical(int32 Seed)
{
	return Embody(
		static_cast<uint32>(FMath::Max(0, Seed)),
		AnastasisWorldView::ReferenceWidth,
		AnastasisWorldView::ReferenceHeight);
}

UMaterialInterface* AAnastasisWorldEmbodiment::ResolveSliceMaterial()
{
	if (!SliceMaterial)
	{
		SliceMaterial = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Anastasis/Materials/M_AnastasisSlice.M_AnastasisSlice"));
	}
	return SliceMaterial ? SliceMaterial.Get() : BaseShapeMaterial.Get();
}

void AAnastasisWorldEmbodiment::ShowSliceSurface()
{
	CVarTerrainSurface->Set(1, ECVF_SetByCode);
	EmbodyCanonical(static_cast<int32>(AnastasisWorldView::ReferenceSeed));
}

void AAnastasisWorldEmbodiment::ShowLegacyDebug()
{
	CVarTerrainSurface->Set(0, ECVF_SetByCode);
	EmbodyCanonical(static_cast<int32>(AnastasisWorldView::ReferenceSeed));
}

int32 AAnastasisWorldEmbodiment::GetInstanceCount() const
{
	int32 Count = 0;
	for (int32 TypeIndex = 0; TypeIndex < AnastasisWorld::TileTypeCount; ++TypeIndex)
	{
		if (const UHierarchicalInstancedStaticMeshComponent* Mesh = TerrainMeshes[TypeIndex])
		{
			Count += Mesh->GetInstanceCount();
		}
	}
	return Count;
}

FVector AAnastasisWorldEmbodiment::GetEmbodiedLocation(int32 TileIndex) const
{
	if (!Plan.Locations.IsValidIndex(TileIndex))
	{
		return FVector::ZeroVector;
	}
	return Plan.Locations[TileIndex];
}

bool AAnastasisWorldEmbodiment::GetInstanceWorldTransform(int32 TileIndex, FTransform& OutTransform) const
{
	if (!LocalInstanceIndex.IsValidIndex(TileIndex) || !Plan.Types.IsValidIndex(TileIndex))
	{
		return false;
	}

	const int32 LocalIndex = LocalInstanceIndex[TileIndex];
	const uint8 TypeIndex = static_cast<uint8>(Plan.Types[TileIndex]);
	const UHierarchicalInstancedStaticMeshComponent* Mesh = TerrainMeshes[TypeIndex];
	if (!Mesh || LocalIndex == INDEX_NONE)
	{
		return false;
	}

	return Mesh->GetInstanceTransform(LocalIndex, OutTransform, true);
}

void AAnastasisWorldEmbodiment::LogEmbodiment() const
{
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_WORLDVIEW seed=%u source=%dx%d crop=(%d,%d) %dx%d tiles=%d instances=%d minAlt=%.6f maxAlt=%.6f counts=%d,%d,%d,%d,%d,%d,%d"),
		Plan.Seed,
		Plan.SourceW,
		Plan.SourceH,
		Plan.OriginX,
		Plan.OriginY,
		Plan.W,
		Plan.H,
		Plan.TileCount,
		GetInstanceCount(),
		Plan.MinAlt,
		Plan.MaxAlt,
		Plan.TerrainCounts[0],
		Plan.TerrainCounts[1],
		Plan.TerrainCounts[2],
		Plan.TerrainCounts[3],
		Plan.TerrainCounts[4],
		Plan.TerrainCounts[5],
		Plan.TerrainCounts[6]);

	for (const int32* XY : SampleXY)
	{
		const AnastasisWorldView::FTilePose Pose = AnastasisWorldView::SamplePose(Plan, XY[0], XY[1]);
		if (Pose.Index == INDEX_NONE)
		{
			continue;
		}

		FTransform InstanceTransform;
		const bool bHasInstance = GetInstanceWorldTransform(Pose.Index, InstanceTransform);
		const FVector InstanceLocation = bHasInstance ? InstanceTransform.GetLocation() : FVector(-1.0, -1.0, -1.0);
		UE_LOG(
			LogAnastasis_UnrealV2,
			Display,
			TEXT("ANASTASIS_WORLDVIEW_SAMPLE i=%d x=%d y=%d type=%u alt=%.17g expected=(%.6f,%.6f,%.6f) instance=(%.6f,%.6f,%.6f)"),
			Pose.Index,
			Pose.X,
			Pose.Y,
			static_cast<uint32>(Pose.Type),
			Pose.Alt,
			Pose.UnrealLocation.X,
			Pose.UnrealLocation.Y,
			Pose.UnrealLocation.Z,
			InstanceLocation.X,
			InstanceLocation.Y,
			InstanceLocation.Z);
	}
}



