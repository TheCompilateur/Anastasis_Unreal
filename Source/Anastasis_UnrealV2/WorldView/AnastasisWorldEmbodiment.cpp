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

static TAutoConsoleVariable<int32> CVarEcologicalDressing(
    TEXT("anastasis.Dressing.Ecology"), 1,
    TEXT("0=legacy tile dressing, 1=forest grammar on continuous terrain; applied on embodiment."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarTerrainSurface(TEXT("anastasis.Terrain.Surface"), 2, TEXT("Center-sampled terrain. 0=legacy DEBUG slabs, 1=sealed 32x32 canonical slice, 2=surface over the whole embodied crop (default); applied on embodiment."), ECVF_Default);

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

	/**
	 * Ecological layer -> stature the presentation should dress it as.
	 *
	 * AnastasisEcologicalDressing decides three layers because that is what its support and
	 * clustering fields can justify. The reference plate names four strata, the fourth being
	 * the emergents -- the few ancient trees that stand out of the canopy. They are not a
	 * separate ecological decision, they are the oldest tail of the canopy itself, so they
	 * are split off HERE, in presentation, from the same deterministic hash. Nothing about
	 * where a tree grows changes; only how tall and how old the one already there looks.
	 *
	 * A canopy of uniform height reads as a hedge. This is what gives it a skyline.
	 */
	constexpr double EmergentShareOfCanopy = 0.18;

	EAnastasisStatureClass StatureForLayer(
		AnastasisEcologicalDressing::ELayer Layer, uint32 VisualSeed, int32 TileX, int32 TileY)
	{
		using AnastasisEcologicalDressing::ELayer;
		switch (Layer)
		{
		case ELayer::Young:
			return EAnastasisStatureClass::Understory;
		case ELayer::Secondary:
			return EAnastasisStatureClass::Subcanopy;
		case ELayer::Canopy:
		default:
			break;
		}
		uint32 H = VisualSeed ^ 0x7F4A7C15u;
		H = (H ^ static_cast<uint32>(TileX)) * 0x85EBCA6Bu;
		H = (H ^ static_cast<uint32>(TileY)) * 0xC2B2AE35u;
		H ^= H >> 15;
		const double Unit = static_cast<double>(H) / static_cast<double>(MAX_uint32);
		return Unit < EmergentShareOfCanopy ? EAnastasisStatureClass::Emergent : EAnastasisStatureClass::Canopy;
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

	// Default subobject rather than a NewObject at embody time: OnConstruction reruns would
	// otherwise create and register a component mid-construction, which the engine is free to
	// tear down between runs.
	ExperimentalSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ExperimentalTerrain"));
	ExperimentalSurface->SetupAttachment(Root);
	ExperimentalSurface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ExperimentalSurface->SetCollisionProfileName(TEXT("BlockAll"));
	ExperimentalSurface->SetCanEverAffectNavigation(false);
	ExperimentalSurface->SetCastShadow(true);
	ExperimentalSurface->SetVisibility(false);

	// The level holds no world truth: every tile is regenerated from the seed at load. Transient
	// keeps the instances OnConstruction builds in the editor out of the .umap, which would
	// otherwise bake simulation output into the map the first time anyone saves it.
	for (UHierarchicalInstancedStaticMeshComponent* Mesh : TerrainMeshes)
	{
		Mesh->SetFlags(RF_Transient);
	}
	ExperimentalSurface->SetFlags(RF_Transient);
}

UHierarchicalInstancedStaticMeshComponent* AAnastasisWorldEmbodiment::GetOrCreateDressingMesh(
	const AnastasisPresentation::FResolvedPresentation& Resolved)
{
	const FName Key(*FString::Printf(TEXT("Dressing_%s_v%d"),
		*Resolved.Entry->ArchetypeId.ToString(), Resolved.VariantIndex));

	// The cached component can be stale: a construction-script rerun is free to destroy
	// components an earlier run created, leaving this map pointing at nothing. Reuse only what
	// is still valid, and fall through to rebuild otherwise instead of returning null dressing.
	const int32* Existing = DressingSlotByKey.Find(Key);
	if (Existing && DressingMeshes.IsValidIndex(*Existing))
	{
		if (UHierarchicalInstancedStaticMeshComponent* Cached = DressingMeshes[*Existing].Get())
		{
			if (IsValid(Cached))
			{
				return Cached;
			}
		}
	}

	UHierarchicalInstancedStaticMeshComponent* Mesh =
		NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Key);
	// Same reason as the ground meshes: never serialized into the level.
	Mesh->SetFlags(RF_Transient);
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(true);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->RegisterComponent();

	if (Existing && DressingMeshes.IsValidIndex(*Existing))
	{
		DressingMeshes[*Existing] = Mesh;
	}
	else
	{
		DressingSlotByKey.Add(Key, DressingMeshes.Add(Mesh));
	}
	return Mesh;
}

void AAnastasisWorldEmbodiment::BeginPlay()
{
	Super::BeginPlay();
	EmbodyFromConsoleVariables();
}

#if WITH_EDITOR
void AAnastasisWorldEmbodiment::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const UWorld* OwningWorld = GetWorld();
	if (!OwningWorld || OwningWorld->IsGameWorld())
	{
		return;
	}

	EmbodyFromConsoleVariables();
}
#endif

bool AAnastasisWorldEmbodiment::EmbodyFromConsoleVariables()
{
	return EmbodyCrop(
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
	uint32 Seed, const AnastasisWorldView::FWorldVisualSnapshot* SurfaceCrop,
    const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource)
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
    const double DressingStart = FPlatformTime::Seconds();
    const bool bEcology = SurfaceCrop && ForestDressing.bEnabled && CVarEcologicalDressing.GetValueOnGameThread() != 0;
    TSet<UHierarchicalInstancedStaticMeshComponent*> Prepared;
    auto Prepare = [&](const AnastasisPresentation::FResolvedPresentation& R)
    {
        auto* M = GetOrCreateDressingMesh(R);
        if (!M || Prepared.Contains(M)) return M;
        Prepared.Add(M);
        M->SetStaticMesh(R.Mesh);
        if (R.MaterialOverride) M->SetMaterial(0, R.MaterialOverride);
        else if (BaseShapeMaterial)
        {
            auto* Mid = UMaterialInstanceDynamic::Create(BaseShapeMaterial, this);
            Mid->SetVectorParameterValue(TEXT("Color"), R.Entry->Tint);
            M->SetMaterial(0, Mid);
        }
        return M;
    };
	for (int32 Index = 0; Index < Plan.TileCount; ++Index)
	{
		const AnastasisWorldView::FVisualTile& SourceTile = Snapshot.Tiles[Index];
        if (bEcology && SourceTile.Type == AnastasisWorld::ETileType::Forest) continue;
		AnastasisPresentation::FResolvedPresentation Resolved;
		if (!AnastasisPresentation::ResolvePresentation(
				Plan.Types[Index], Seed, SourceTile.X, SourceTile.Y, Resolved))
		{
			continue;
		}

		auto* Mesh = Prepare(Resolved);
		if (!Mesh) continue;

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
    if (bEcology)
    {
        AnastasisEcologicalDressing::FPlan ForestPlan;
        FString Error;
        if (!AnastasisEcologicalDressing::Build(CanonicalSource, ForestDressing, ForestPlan, Error))
        {
            UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_ECOLOGY rejected=%s"), *Error);
        }
        else
        {
            int32 ForestLayerCounts[3] = {};
            int32 StatureCounts[5] = {};
            int32 FamilyCounts[3] = {};
            double TallestUU = 0.0;
            double ShortestUU = TNumericLimits<double>::Max();
            // The gradient's own inputs, gathered as they are actually sampled. The species
            // weights in the resolver are only defensible against the distribution they
            // actually see, so the distribution is reported rather than assumed.
            TArray<double> SiteShade;
            TArray<double> SiteWetness;
            TArray<double> SiteConiferousness;
            SiteShade.Reserve(ForestPlan.Instances.Num());
            SiteWetness.Reserve(ForestPlan.Instances.Num());
            SiteConiferousness.Reserve(ForestPlan.Instances.Num());
            for (const auto& P : ForestPlan.Instances)
            {
                double GroundZ;
                if (!AnastasisTerrainSurface::SampleHeight(*SurfaceCrop, P.Ground.X, P.Ground.Y, GroundZ))
                    continue;
                const auto& T = CanonicalSource.Tiles[P.SourceIndex];
                const EAnastasisStatureClass Stature = StatureForLayer(P.Layer, P.VisualSeed, T.X, T.Y);
                // Species comes from the site, not from a blind draw: Shade carries altitude
                // and exposure, Wetness carries moisture, and both are simulation truth this
                // layer only reads. Nothing here moves a tree.
                const EAnastasisFoliageFamily Family = AnastasisPresentation::SelectFoliageFamily(
                    T.Shade, T.Wetness, P.VisualSeed, T.X, T.Y);
                SiteShade.Add(T.Shade);
                SiteWetness.Add(T.Wetness);
                SiteConiferousness.Add(AnastasisPresentation::Coniferousness(T.Shade, T.Wetness));
                AnastasisPresentation::FResolvedPresentation R;
                if (!AnastasisPresentation::ResolvePresentation(AnastasisWorld::ETileType::Forest,
                    P.VisualSeed, T.X, T.Y, R, Stature, Family)) continue;
                auto* M = Prepare(R);
                if (!M) continue;
                FTransform Pose = AnastasisPresentation::ResolveInstanceTransform(*R.Entry,
                    P.VisualSeed, T.X, T.Y, T.Alt, R.ScaleBias);
                Pose.SetScale3D(Pose.GetScale3D() * P.ScaleMultiplier);
                // Actual mesh bounds, not the resolver's 100uu primitive pivot convention.
                const FBox MeshBounds = R.Mesh->GetBoundingBox();
                const double MinZ = MeshBounds.Min.Z;
                Pose.SetLocation(FVector(P.Ground.X, P.Ground.Y, GroundZ - MinZ * Pose.GetScale3D().Z));
                M->AddInstance(Pose, false);
                ++ForestLayerCounts[static_cast<uint8>(P.Layer)];
                ++StatureCounts[static_cast<uint8>(Stature)];
                ++FamilyCounts[static_cast<uint8>(Family)];
                const double HeightUU = (MeshBounds.Max.Z - MinZ) * Pose.GetScale3D().Z;
                TallestUU = FMath::Max(TallestUU, HeightUU);
                ShortestUU = FMath::Min(ShortestUU, HeightUU);
                ++DressingInstanceCount;
            }
            UE_LOG(LogAnastasis_UnrealV2, Display,
                TEXT("ANASTASIS_ECOLOGY young=%d secondary=%d canopy=%d full_plan=%d refused_water_or_footprint=%d refused_slope=%d refused_spacing=%d"),
                ForestLayerCounts[0], ForestLayerCounts[1], ForestLayerCounts[2], ForestPlan.Instances.Num(),
                ForestPlan.RejectedWaterOrFootprint, ForestPlan.RejectedSlope, ForestPlan.RejectedSpacing);
            // The stature profile is the visual claim of this pass, so it is measured rather
            // than asserted: a forest that has collapsed back onto one height says so here.
            UE_LOG(LogAnastasis_UnrealV2, Display,
                TEXT("ANASTASIS_TREE_STATURE understory=%d subcanopy=%d canopy=%d emergent=%d height_uu=[%.0f,%.0f]"),
                StatureCounts[static_cast<uint8>(EAnastasisStatureClass::Understory)],
                StatureCounts[static_cast<uint8>(EAnastasisStatureClass::Subcanopy)],
                StatureCounts[static_cast<uint8>(EAnastasisStatureClass::Canopy)],
                StatureCounts[static_cast<uint8>(EAnastasisStatureClass::Emergent)],
                ShortestUU == TNumericLimits<double>::Max() ? 0.0 : ShortestUU, TallestUU);

            // The species claim, measured the same way. A mix that has quietly collapsed to
            // one family, or a gradient that has saturated, is visible in this one line.
            SiteShade.Sort();
            SiteWetness.Sort();
            const auto At = [](const TArray<double>& V, double Q)
            {
                return V.Num() == 0 ? 0.0 : V[FMath::Clamp(FMath::FloorToInt(Q * (V.Num() - 1)), 0, V.Num() - 1)];
            };
            // p_conifer is the gradient as REAL SITES see it, not as its extreme corners
            // would. Reporting Coniferousness(worst shade, worst wetness) described a
            // combination that may exist nowhere on the map, and showed a clamp that no
            // tree ever met -- a diagnostic that raises a false alarm is worse than none.
            SiteConiferousness.Sort();
            UE_LOG(LogAnastasis_UnrealV2, Display,
                TEXT("ANASTASIS_TREE_SPECIES conifer=%d broadleaf=%d shade=[%.2f %.2f %.2f] wetness=[%.2f %.2f %.2f] p_conifer=[%.2f %.2f %.2f]"),
                FamilyCounts[static_cast<uint8>(EAnastasisFoliageFamily::Conifer)],
                FamilyCounts[static_cast<uint8>(EAnastasisFoliageFamily::Broadleaf)],
                At(SiteShade, 0.0), At(SiteShade, 0.5), At(SiteShade, 1.0),
                At(SiteWetness, 0.0), At(SiteWetness, 0.5), At(SiteWetness, 1.0),
                At(SiteConiferousness, 0.0), At(SiteConiferousness, 0.5), At(SiteConiferousness, 1.0));
        }
    }
    UE_LOG(LogAnastasis_UnrealV2, Display,
        TEXT("ANASTASIS_ECOLOGY_COST enabled=%d generation_ms=%.3f components=%d instances=%d"),
        bEcology, (FPlatformTime::Seconds() - DressingStart) * 1000.0, DressingMeshes.Num(), DressingInstanceCount);
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

	const auto CanonicalSource = AnastasisWorldView::CaptureCanonicalWorld(Seed);
	Snapshot = AnastasisWorldView::CropSnapshot(
		CanonicalSource,
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
    PlaceDressing(Seed, bSurfaceBuilt ? &BuiltSurfaceCrop : nullptr, CanonicalSource);

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
