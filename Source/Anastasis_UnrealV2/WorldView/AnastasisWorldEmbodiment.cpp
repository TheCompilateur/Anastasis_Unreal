#include "WorldView/AnastasisWorldEmbodiment.h"
#include "ProceduralMeshComponent.h"
#include "WorldView/AnastasisPresentationRegistry.h"
#include "WorldView/AnastasisPresentationResolver.h"
#include "WorldView/AnastasisTerrainSurface.h"


#include "Anastasis_UnrealV2.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "WorldView/AnastasisWorldDebugVisual.h"

static TAutoConsoleVariable<int32> CVarTerrainSurface(TEXT("anastasis.Terrain.Surface"), 0, TEXT("Center-sampled terrain. 0=legacy DEBUG slabs, 1=sealed 32x32 canonical slice, 2=surface over the whole embodied crop; applied on embodiment."), ECVF_Default);

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
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionProfileName(TEXT("BlockAll"));
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
	// Dressing components are NOT created here: which meshes exist is presentation data
	// (see UAnastasisPresentationRegistry), read at EmbodyCrop time, not compile time.
}

UHierarchicalInstancedStaticMeshComponent* AAnastasisWorldEmbodiment::GetOrCreateDressingMesh(
	const AnastasisPresentation::FResolvedPresentation& Resolved)
{
	const FName Key(*FString::Printf(TEXT("Dressing_%s_v%d"),
		*Resolved.Entry->ArchetypeId.ToString(), Resolved.VariantIndex));

	if (const int32* Existing = DressingSlotByKey.Find(Key))
	{
		return DressingMeshes.IsValidIndex(*Existing) ? DressingMeshes[*Existing].Get() : nullptr;
	}

	UHierarchicalInstancedStaticMeshComponent* Mesh =
		NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Key);
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(true);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->RegisterComponent();

	DressingSlotByKey.Add(Key, DressingMeshes.Add(Mesh));
	return Mesh;
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

void AAnastasisWorldEmbodiment::PlaceDressing(
	uint32 Seed, const AnastasisWorldView::FWorldVisualSnapshot* SurfaceCrop)
{
	for (UHierarchicalInstancedStaticMeshComponent* Mesh : DressingMeshes)
	{
		if (Mesh)
		{
			Mesh->ClearInstances();
		}
	}

	DressingInstanceCount = 0;
	int32 UngroundedTiles = 0;
	for (int32 Index = 0; Index < Plan.TileCount; ++Index)
	{
		const AnastasisWorldView::FVisualTile& SourceTile = Snapshot.Tiles[Index];
		AnastasisPresentation::FResolvedPresentation Resolved;
		if (!AnastasisPresentation::ResolvePresentation(
				Plan.Types[Index], Seed, SourceTile.X, SourceTile.Y, Resolved))
		{
			continue;
		}

		UHierarchicalInstancedStaticMeshComponent* Mesh = GetOrCreateDressingMesh(Resolved);
		if (!Mesh)
		{
			continue;
		}
		// Re-applied every embodiment: the data asset may have changed since the last one.
		if (Mesh->GetStaticMesh() != Resolved.Mesh)
		{
			Mesh->SetStaticMesh(Resolved.Mesh);
		}
		if (Resolved.MaterialOverride)
		{
			Mesh->SetMaterial(0, Resolved.MaterialOverride);
		}
		else if (BaseShapeMaterial)
		{
			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseShapeMaterial, this);
			Mid->SetVectorParameterValue(TEXT("Color"), Resolved.Entry->Tint);
			Mesh->SetMaterial(0, Mid);
		}

		FTransform InstanceTransform = AnastasisPresentation::ResolveInstanceTransform(
			*Resolved.Entry, Seed, SourceTile.X, SourceTile.Y, Plan.Alts[Index]);

		// Le resolver a place l'instance a l'altitude de la TUILE, plus son lift de pivot.
		// Le jitter XY, lui, l'a deplacee jusqu'a 30 UU sur une tuile de 100 : sur une pente
		// elle n'est donc plus au-dessus du sol qu'elle vise. On releve le lift depuis la
		// transform du resolver -- on ne le recalcule pas, pour ne pas creer une deuxieme
		// source de verite sur le pivot -- et on rebase ce lift sur le sol reel.
		const FVector Placed = InstanceTransform.GetLocation();
		const double TileGroundZ = Plan.Alts[Index] * AnastasisWorldView::AltitudeScale;
		const double PivotLift = Placed.Z - TileGroundZ;

		double GroundZ = 0.0;
		if (SurfaceCrop)
		{
			if (!AnastasisTerrainSurface::SampleHeight(*SurfaceCrop, Placed.X, Placed.Y, GroundZ))
			{
				// Pas de sol rendu sous ce point : on ne pose rien. Une instance suspendue
				// au-dessus du vide serait un mensonge visuel, pas un placeholder.
				++UngroundedTiles;
				continue;
			}
		}
		else
		{
			GroundZ = TileGroundZ + AnastasisWorldDebugVisual::SlabTopOffsetZ;
		}

		InstanceTransform.SetLocation(FVector(Placed.X, Placed.Y, GroundZ + PivotLift));
		Mesh->AddInstance(InstanceTransform, false);
		++DressingInstanceCount;
	}
	for (UHierarchicalInstancedStaticMeshComponent* Mesh : DressingMeshes)
	{
		if (Mesh)
		{
			Mesh->MarkRenderStateDirty();
		}
	}
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_DRESSING ground=%s instances=%d refused_ungrounded=%d"),
		SurfaceCrop ? TEXT("surface") : TEXT("slab"), DressingInstanceCount, UngroundedTiles);
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

	// Dressing: discrete instances (trees, ruins) whose look comes from the presentation
	// registry, not from this file. Orthogonal to the DEBUG/Surface ground toggle below --
	// presence is decided by Plan.Types (simulation truth) plus the data entry's bEnabled
	// flag, never by which ground representation is currently active.
    // L'emprise sur laquelle la surface est REELLEMENT batie, s'il y en a une : c'est
    // elle qui dit ou se trouve le sol, donc ce sur quoi le dressing sera pose.
    AnastasisWorldView::FWorldVisualSnapshot BuiltSurfaceCrop;
    bool bSurfaceBuilt = false;

    if (ExperimentalSurface) ExperimentalSurface->SetVisibility(false);
    for (auto& Mesh : TerrainMeshes) if (Mesh) Mesh->SetVisibility(true);
    const int32 SurfaceMode = CVarTerrainSurface.GetValueOnGameThread();
    if (SurfaceMode == 1 || SurfaceMode == 2)
    {
        // Mode 1 : la tranche scellee WORLD_SLICE_006, inchangee, seule emprise dont
        // le TERRAIN_CONTRACT (1024 sommets / 1922 triangles) fait foi.
        // Mode 2 : la meme surface, la meme couleur de sommet, sur toute l'emprise
        // incarnee -- c'est ce mode qui donne une carte au lieu d'une eprouvette.
        const auto Crop = (SurfaceMode == 2)
            ? Snapshot
            : AnastasisWorldView::CropSnapshot(
                Snapshot, 0, 0,
                AnastasisWorldView::CanonicalCropWidth,
                AnastasisWorldView::CanonicalCropHeight);
        AnastasisTerrainSurface::FGeometry Geometry;
        if (AnastasisTerrainSurface::Build(Crop, Geometry))
        {
            if (!ExperimentalSurface)
            {
                ExperimentalSurface = NewObject<UProceduralMeshComponent>(this, TEXT("ExperimentalTerrain"));
                ExperimentalSurface->SetupAttachment(GetRootComponent());
                ExperimentalSurface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                ExperimentalSurface->SetCollisionProfileName(TEXT("BlockAll"));
                ExperimentalSurface->SetCanEverAffectNavigation(false);
                ExperimentalSurface->SetCastShadow(true);
                ExperimentalSurface->RegisterComponent();
            }
            // Section 0 : relief. La couleur de sommet porte toute la semantique du sol.
            // bCreateCollision=true : c'est ce qui empeche le pawn de tomber a travers.
            ExperimentalSurface->CreateMeshSection_LinearColor(0, Geometry.Vertices, Geometry.Triangles,
                Geometry.Normals, TArray<FVector2D>{}, Geometry.Colors, TArray<FProcMeshTangent>{}, true);
            // Section 1 : nappe d'eau plate au niveau de la mer, encastree dans le relief.
            ExperimentalSurface->ClearMeshSection(1);
            bWaterSurfaceBuilt = Geometry.WaterTriangles.Num() > 0;
            if (bWaterSurfaceBuilt)
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
            BuiltSurfaceCrop = Crop;
            bSurfaceBuilt = true;
            // Emprise reelle = ce qui est reellement rendu, pas Plan : en mode 1 la tranche
            // canonique meme si Plan couvre le monde, en mode 2 l emprise incarnee entiere.
            ActiveFootprintBounds = AnastasisWorldView::SnapshotBounds(Crop);
            // Emprise reportee telle qu'elle est batie : en mode 1 cette ligne imprime
            // exactement la chaine scellee (source=96x96 crop=(0,0) 32x32 tiles=1024).
            UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_TERRAIN source=%dx%d crop=(%d,%d) %dx%d tiles=%d vertices=%d triangles=%d water_triangles=%d material=%s boundary=tile_centers legacy_visible=0"),
                Crop.SourceW, Crop.SourceH, Crop.OriginX, Crop.OriginY, Crop.W, Crop.H, Crop.Tiles.Num(),
                Geometry.Vertices.Num(), Geometry.Triangles.Num()/3, Geometry.WaterTriangles.Num()/3,
                SliceMaterial ? TEXT("slice") : TEXT("fallback"));
        }
        else
        {
            bWaterSurfaceBuilt = false;
            ActiveFootprintBounds = AnastasisWorldView::PlanBounds(Plan);
            UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_TERRAIN rejected crop; legacy DEBUG retained"));
        }
    }
    else
    {
        bWaterSurfaceBuilt = false;
        ActiveFootprintBounds = AnastasisWorldView::PlanBounds(Plan);
        UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_TERRAIN disabled legacy_visible=1"));
    }
    // Le dressing vient APRES la decision de terrain : on ne pose pas un objet sur un
    // sol dont on ignore encore la forme.
    PlaceDressing(Seed, bSurfaceBuilt ? &BuiltSurfaceCrop : nullptr);

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

FVector AAnastasisWorldEmbodiment::GetSafeRespawnLocation() const
{
	// ActiveFootprintBounds, PAS PlanBounds(Plan) : en mode surface le rendu
	// visible/solide est le crop 32x32 fixe, qui peut etre bien plus petit que
	// le Plan complet demande par BeginPlay (jusqu'a 96x96). Viser Plan atterrit
	// hors de tout ce qui existe.
	const FBox& Bounds = ActiveFootprintBounds;
	if (!Bounds.IsValid)
	{
		return GetActorLocation();
	}
	const FVector Center = Bounds.GetCenter();
	// Assez haut au-dessus du relief le plus eleve pour ne jamais reapparaitre encastre dedans.
	return FVector(Center.X, Center.Y, Bounds.Max.Z + 300.0);
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

	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_PRESENTATION dressing_instances=%d tree_tiles=%d ruin_tiles=%d source=%s components=%d"),
		DressingInstanceCount,
		Plan.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Forest)],
		Plan.TerrainCounts[static_cast<uint8>(AnastasisWorld::ETileType::Ruin)],
		AnastasisPresentation::IsRegistryDataDriven() ? TEXT("asset") : TEXT("code_defaults"),
		DressingMeshes.Num());

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



