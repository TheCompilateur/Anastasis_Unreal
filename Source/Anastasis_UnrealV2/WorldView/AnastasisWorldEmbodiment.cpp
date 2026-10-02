#include "WorldView/AnastasisWorldEmbodiment.h"
#include "ProceduralMeshComponent.h"
#include "WorldView/AnastasisPresentationRegistry.h"
#include "WorldView/AnastasisPresentationResolver.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisTerrainHorizon.h"
#include "WorldView/AnastasisHumanGeography.h"
#include "WorldView/AnastasisDrainage.h"
#include "WorldView/AnastasisPlaces.h"
#include "WorldView/AnastasisGroundCover.h"
#include "WorldView/AnastasisForestStructure.h"
#include "WorldView/AnastasisHeroCanopy.h"
#include "WorldView/AnastasisUnderstory.h"
#include "WorldView/AnastasisTrunkContact.h"
#include "WorldView/AnastasisMicroEcology.h"
#include "WorldView/AnastasisRiverbank.h"

#include "Anastasis_UnrealV2.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Containers/Ticker.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include "UObject/ConstructorHelpers.h"
#include "WorldView/AnastasisWorldDebugVisual.h"

static TAutoConsoleVariable<float> CVarWorldScale(TEXT("anastasis.WorldView.Scale"), 5.0f, TEXT("Physical scale of forged full-world presentation; 1=380m corrected source, 5=1900m. Applied on embodiment."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarHumanGeography(TEXT("anastasis.Terrain.HumanGeography"), 1, TEXT("Human_Geography_V2: reversible authored macro terrain for seed 12345. 0=original forms, 1=V2. Applied on embodiment."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarEcologicalDressing(
    TEXT("anastasis.Dressing.Ecology"), 1,
    TEXT("0=legacy tile dressing, 1=forest grammar on continuous terrain; applied on embodiment."), ECVF_Default);

// WORLD_DRESSING_01. Coupable pour les captures A/B : meme monde, meme dressing, sans les lieux.
static TAutoConsoleVariable<int32> CVarPlaces(
    TEXT("anastasis.Dressing.Places"), 1,
    TEXT("0=aucun lieu compose, 1=lieux lus dans la geographie (source, col, guet, hameau...) ; applique a l'incarnation."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarMacroForest(
    TEXT("anastasis.Dressing.MacroForest"), 1,
    TEXT("0=original ecological dressing, 1=large forest masses conditioned by rendered relief; applied on embodiment."), ECVF_Default);

// FOREST_TERRAIN_P1. Coupable pour l'A/B : memes troncs, memes positions ; 0 rend la grammaire
// pontique d'origine (famille tiree de Shade/Wetness, echelles multipliees).
static TAutoConsoleVariable<int32> CVarTreeSpecies(
    TEXT("anastasis.Dressing.TreeSpecies"), 1,
    TEXT("0=Pontic conifer/broadleaf grammar, 1=Mediterranean and Greek mountain species zoned by altitude, slope and water, at real heights; applied on embodiment."), ECVF_Default);

// GROUND_COVER_001. Coupable pour l'A/B : meme monde, memes arbres, sans la strate herbacee.
static TAutoConsoleVariable<int32> CVarGroundCover(
    TEXT("anastasis.Dressing.GroundCover"), 1,
    TEXT("0=sol nu entre les arbres, 1=prairies haute, basse et humide sur les espaces ouverts de la vallee ; applique a l'incarnation."), ECVF_Default);

// Ombres portees des touffes proches (les lointaines n'en portent jamais). Bouton de mesure du cout.
static TAutoConsoleVariable<int32> CVarGroundCoverShadows(
    TEXT("anastasis.GroundCover.Shadows"), 1,
    TEXT("0=herbe sans ombres portees, 1=ombres des touffes proches (< 55 m) ; applique a l'incarnation."), ECVF_Default);

// Pas d'herbe pendant les tests d'automatisation. La suite incarne le monde a chaque test qui fait
// apparaitre l'acteur (16 fois le 2026-10-01), ~1,08 M de touffes chaque fois, jamais rendues entre
// deux tests : 12 Go de memoire virtuelle et la suite tuee au test EmbodimentSpawn ("fichier de
// pagination insuffisant"). Les regles de placement se testent sans incarnation
// (Anastasis.GroundCover.*, Build pur) ; 1 = herbe aussi sous automatisation.
static TAutoConsoleVariable<int32> CVarGroundCoverInAutomation(
    TEXT("anastasis.GroundCover.InAutomation"), 0,
    TEXT("0=pas d'herbe pendant les tests d'automatisation (defaut), 1=herbe aussi sous automatisation ; applique a l'incarnation."), ECVF_Default);

// FOREST_TERRAIN_P3. Coupables pour l'A/B : memes arbres, meme herbe, sans maquis ni rochers.
static TAutoConsoleVariable<int32> CVarUnderstory(
    TEXT("anastasis.Dressing.Understory"), 1,
    TEXT("0=no shrub layer, brambles or scattered rocks, 1=maquis, brambles and rocks by slope, altitude and water; applied on embodiment."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarUnderstoryInAutomation(
    TEXT("anastasis.Understory.InAutomation"), 0,
    TEXT("0=pas de maquis ni de rochers pendant les tests d'automatisation (defaut, comme l'herbe), 1=aussi sous automatisation."), ECVF_Default);

// Huit specimens a taille reelle, et (anastasis.Dressing.CanopyShell) une enveloppe de canopee au-dela de 70 m. 0 = la foret de production seule.
static TAutoConsoleVariable<int32> CVarHeroCanopy(
	TEXT("anastasis.Dressing.HeroCanopy"), 1,
	TEXT("0=meshes de production seuls, 1=huit heros (pin, cypres, chene, olivier) et enveloppe lointaine ; applique a l'incarnation."), ECVF_Default);
// CANOPY_SHELL_FIX_001. L'enveloppe ne s'ajoute plus par defaut : les arbres de production ne
// sont jamais coupes et forment deja la masse lointaine (A/B vue oblique : 2,09 % de pixels,
// sous la variance de capture), et entre 70 et 200 m elle se lit comme une galette sans tronc.
static TAutoConsoleVariable<int32> CVarCanopyShell(
	TEXT("anastasis.Dressing.CanopyShell"), 0,
	TEXT("1=enveloppe de canopee au-dela de 70 m sur chaque massif (avec anastasis.Dressing.HeroCanopy 1), 0=non (defaut) ; applique a l'incarnation."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarHeroCanopyInAutomation(
	TEXT("anastasis.HeroCanopy.InAutomation"), 0,
	TEXT("0=pas de heros ni d'enveloppe pendant les tests d'automatisation (defaut), 1=aussi sous automatisation."), ECVF_Default);
// Pied de tronc. Quelques milliers de pastilles, coupees a 16 m : rien a voir avec le million de touffes.
static TAutoConsoleVariable<int32> CVarTrunkContact(
	TEXT("anastasis.Dressing.TrunkContact"), 1,
	TEXT("0=pied de tronc nu, 1=litiere et mousse a moins d'un metre du tronc, visibles a une dizaine de metres ; applique a l'incarnation."), ECVF_Default);

// SOL SOUS L'HERBE. Coupable pour l'A/B : memes touffes, sol teinte ou non.
// MICRO_ECOLOGY_001. Poches de berge, lisiere, sous-bois. 0 = le dressing deja en place, sans cette couche.
static TAutoConsoleVariable<int32> CVarMicroEcology(
	TEXT("anastasis.Dressing.MicroEcology"), 1,
	TEXT("0=pas de micro-ecologie, 1=berges en poches, lisiere et sous-bois (defaut) ; applique a l'incarnation."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarMicroEcologySoil(
	TEXT("anastasis.MicroEcology.Soil"), 1,
	TEXT("0=couleur de sol inchangee, 1=poches de boue, roche, sol nu et secheresse (defaut) ; applique a l'incarnation."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarMicroEcologyInAutomation(
	TEXT("anastasis.MicroEcology.InAutomation"), 0,
	TEXT("0=pas de micro-ecologie pendant les tests d'automatisation (defaut), 1=aussi sous automatisation."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarGroundCoverSoilTint(
    TEXT("anastasis.GroundCover.SoilTint"), 1,
    TEXT("0=sol non teinte sous l'herbe, 1=sol fonce et verdi sous la prairie, brun sous les laiches, terre sous la lande (defaut) ; applique a l'incarnation."), ECVF_Default);

// RIVERBANK_LIFE_001 : rives vivantes. Coupable pour l'A/B : meme eau, rives peintes et peuplees ou non.
static TAutoConsoleVariable<int32> CVarRiverbank(
    TEXT("anastasis.Dressing.Riverbank"), 1,
    TEXT("1=rives vivantes : vase et roseaux en eau calme, gravier et galets en eau vive (defaut), 0=la prairie touche l'eau ; applique a l'incarnation."), ECVF_Default);
static TAutoConsoleVariable<int32> CVarRiverbankInAutomation(
    TEXT("anastasis.Riverbank.InAutomation"), 0,
    TEXT("0=pas d'instances de rive pendant les tests d'automatisation (defaut, comme l'herbe), 1=aussi sous automatisation ; applique a l'incarnation."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarTerrainSurface(TEXT("anastasis.Terrain.Surface"), 2, TEXT("Center-sampled terrain. 0=legacy DEBUG slabs, 1=sealed 32x32 canonical slice, 2=surface over the whole embodied crop (default); applied on embodiment."), ECVF_Default);

static TAutoConsoleVariable<int32> CVarTerrainForge(
	TEXT("anastasis.Terrain.Forge"),
	1,
	TEXT("0=raw tile-center surface. 1=TERRAIN_FORGE tessellated morphology (default). Does not change simulation Alt."),
	ECVF_Default);

// SHORELINE_FORGE_001. Le bouton qui rend la preuve possible : les deux chemins
// batissent EXACTEMENT la meme geometrie -- memes sommets, memes triangles, meme
// nappe plate au niveau de la mer -- et ne different que par ce que la nappe SAIT
// d'elle-meme. A camera, graine, soleil et exposition identiques, une capture A/B
// ne mesure donc que le traitement de rive.
//   0 = nappe historique : une seule couleur opaque, aucun canal.
//   1 = rive graduee : profondeur, platitude de berge, courant (defaut).
static TAutoConsoleVariable<int32> CVarShoreline(
    TEXT("anastasis.Terrain.Shoreline"), 1,
    TEXT("0=nappe d'eau opaque historique, 1=rive graduee M_AnastasisShoreWater (defaut); applique a l'incarnation."),
    ECVF_Default);

// WATER_LOOK_001 : l'eau se lisait comme de la peinture (aplat translucide, a fleur
// d'herbe, bords en escalier, immobile). 1 = M_AnastasisWater (Single Layer Water :
// absorption selon la profondeur, reflets, vagues advectees par le courant), rubans
// d'eau lisses par riviere (section 2), berges marquees et fond de vase. 0 = l'eau d'avant.
static TAutoConsoleVariable<int32> CVarWaterLook(
    TEXT("anastasis.Terrain.WaterLook"), 1,
    TEXT("1=eau Single Layer Water + rubans de riviere + berges marquees (defaut), 0=nappe de rive d'avant; applique a l'incarnation."),
    ECVF_Default);

// Le bouton qui rend la comparaison possible. Les deux chemins batissent EXACTEMENT
// la meme geometrie et les memes canaux de sommet : seule change la fonction qui les
// lit. Une capture A/B a camera, graine, soleil et exposition identiques ne mesure
// donc que le materiau -- c'est la seule facon de prouver un gain de sol.
//
// Orthogonal a anastasis.Terrain.Forge : celle-ci decide de la FORME du sol, celle-la
// de sa MATIERE. Les quatre combinaisons ont un sens et se capturent separement.
static TAutoConsoleVariable<int32> CVarGroundMaterial(
    TEXT("anastasis.Terrain.GroundMaterial"), 1,
    TEXT("0=materiau de tranche historique (couleur de sommet plate), 1=sol morphologique MI_AnastasisGround (defaut); applique a l'incarnation."),
    ECVF_Default);

// HORIZON_RING_001. Le bord du monde rendu : 0 = rien au-dela des 96 tuiles (le sol de
// planete du SkyAtmosphere, presque noir, remplit le bas de l'horizon), 1 = anneau de
// terrain lointain raccorde au bord forge. N'existe que sur le monde ENTIER forge : un
// decoupage partiel a des voisins reels, pas un horizon.
static TAutoConsoleVariable<int32> CVarTerrainHorizon(
    TEXT("anastasis.Terrain.Horizon"), 1,
    TEXT("0=nothing beyond the map edge, 1=distant terrain ring around the forged world (default); applied on embodiment."),
    ECVF_Default);

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

	/**
	 * Presentation-only per-tree draw, for height, crown width and tint. The salt is mixed in
	 * FIRST and the result fully finalised: with the salt xored at the last step only, two
	 * salts of the same tree differ by a constant and height, crown and tint come out correlated.
	 */
	double TreeUnit(uint32 VisualSeed, int32 TileX, int32 TileY, uint32 Salt)
	{
		uint32 H = VisualSeed ^ 0x2C1B3C6Du ^ (Salt * 0x9E3779B9u);
		H = (H ^ static_cast<uint32>(TileX)) * 0x85EBCA6Bu;
		H = (H ^ static_cast<uint32>(TileY)) * 0xC2B2AE35u;
		H ^= H >> 16; H *= 0x7FEB352Du;
		H ^= H >> 15; H *= 0x846CA68Bu;
		H ^= H >> 16;
		return static_cast<double>(H) / static_cast<double>(MAX_uint32);
	}

	/** An emergent is the old tail of the canopy: taller than its species' usual range. */
	constexpr double EmergentHeightBonus = 1.15;
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

	// Anneau lointain : decor pur, jamais marche -- ni collision ni navigation.
	HorizonSurface = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HorizonTerrain"));
	HorizonSurface->SetupAttachment(Root);
	HorizonSurface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HorizonSurface->SetCanEverAffectNavigation(false);
	HorizonSurface->SetCastShadow(true);
	HorizonSurface->SetVisibility(false);

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
	if (!OwningWorld || OwningWorld->IsGameWorld() || bApplyingOccupationTread)
	{
		return;
	}

	EmbodyFromConsoleVariables();
	ScheduleOccupationTread();
}
#endif

void AAnastasisWorldEmbodiment::ScheduleOccupationTread()
{
	if (bOccupationTreadScheduled || bApplyingOccupationTread) return;
	UWorld* World = GetWorld();
	if (!World || World->IsGameWorld()) return;
	bOccupationTreadScheduled = true;
	TWeakObjectPtr<AAnastasisWorldEmbodiment> Self(this);
	FTSTicker::GetCoreTicker().AddTicker(TEXT("AnastasisOccupationTread"), 0.0f, [Self](float) -> bool
	{
		AAnastasisWorldEmbodiment* Actor = Self.Get();
		if (!Actor || !Actor->GetWorld()) return false;
		bool bTrodden = false;
		for (TActorIterator<AActor> It(Actor->GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(TEXT("HO01_Tread")))
			{
				bTrodden = true;
				break;
			}
		}
		if (!bTrodden) return false;
		Actor->bApplyingOccupationTread = true;
		Actor->EmbodyFromConsoleVariables();
		Actor->bApplyingOccupationTread = false;
		return false;
	});
}

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
	for (UHierarchicalInstancedStaticMeshComponent* Mesh : HeroCanopyMeshes)
	{
		if (IsValid(Mesh)) Mesh->ClearInstances();
	}

	DressingInstanceCount = 0;
	int32 UngroundedTiles = 0;
    const double DressingStart = FPlatformTime::Seconds();
    const bool bEcology = SurfaceCrop && ForestDressing.bEnabled && CVarEcologicalDressing.GetValueOnGameThread() != 0;
    // Les lieux sont composes AVANT le dressing par tuile : une ruine composee (hameau,
    // vestiges) remplace le moignon generique de ses tuiles au lieu de s'y superposer.
    AnastasisPlaces::FInputs PlaceInputs;
    AnastasisPlaces::FPlan Places;
    const bool bPlaces = ComposePlaces(SurfaceCrop, CanonicalSource, PlaceInputs, Places);
    int32 SupersededRuins = 0;
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
        // FOREST_TERRAIN_P1 : deux flottants par instance, lus par M_AnastasisVegetation
        // (PerInstanceCustomData 0 = secheresse du site, 1 = ecart individuel de valeur) pour
        // teinter la couronne. Nuls par defaut : la couleur de sommet, comme avant.
        if (M->NumCustomDataFloats != 2) M->SetNumCustomDataFloats(2);
        // Slots 1..N. A tree mesh carries two: foliage on 0, wood on 1. Left unset, slot 1
        // would fall back to the engine's default material -- a grey checkerboard trunk,
        // which is worse than a badly shaded one.
        for (int32 Slot = 0; Slot < R.AdditionalMaterials.Num(); ++Slot)
        {
            if (R.AdditionalMaterials[Slot]) M->SetMaterial(Slot + 1, R.AdditionalMaterials[Slot]);
        }
        return M;
    };
	for (int32 Index = 0; Index < Plan.TileCount; ++Index)
	{
		const AnastasisWorldView::FVisualTile& SourceTile = Snapshot.Tiles[Index];
        if (bEcology && SourceTile.Type == AnastasisWorld::ETileType::Forest) continue;
        if (bPlaces && SourceTile.Type == AnastasisWorld::ETileType::Ruin && AnastasisPlaces::SupersedesTile(Places, SourceTile.SourceIndex))
        {
            ++SupersededRuins;
            continue;
        }
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
		FVector Placed = InstanceTransform.GetLocation();
		Placed.X *= Plan.SpatialScale;
		Placed.Y *= Plan.SpatialScale;
		const double TileGroundZ = Plan.Alts[Index] * AnastasisWorldView::AltitudeScale;
		const double PivotLift = Placed.Z - TileGroundZ;

		double GroundZ = 0.0;
		if (SurfaceCrop)
		{
			const bool bHit = AnastasisTerrainForge::SampleActive(Placed.X, Placed.Y, GroundZ)
				|| AnastasisTerrainSurface::SampleHeight(*SurfaceCrop, Placed.X, Placed.Y, GroundZ);
			if (!bHit)
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

		double WaterZ = AnastasisTerrainSurface::WaterPlaneZ;
		if (Snapshot.bHumanGeography && AnastasisTerrainForge::SampleActiveWater(Placed.X, Placed.Y, WaterZ) && GroundZ <= WaterZ + 25.0) continue;
		InstanceTransform.SetLocation(FVector(Placed.X, Placed.Y, GroundZ + PivotLift));
		Mesh->AddInstance(InstanceTransform, false);
		++DressingInstanceCount;
	}
    // Couronnes reellement posees (X, Y, rayon) : la prairie s'arrete ou commence le sous-bois.
    TArray<FVector> Canopy;
    if (bEcology)
    {
        AnastasisEcologicalDressing::FPlan ForestPlan;
        FString Error;
        AnastasisEcologicalDressing::FRenderedHabitat Habitat;
        Habitat.SampleHeight = [&](double X, double Y, double& Z)
        {
            return AnastasisTerrainForge::SampleActive(X, Y, Z)
                || AnastasisTerrainSurface::SampleHeight(*SurfaceCrop, X, Y, Z);
        };
        Habitat.Basin = ForgeBasin;
        Habitat.bHasBasin = !ForgeBasin.IsZero();
        Habitat.SampleWaterHeight = [&](double X,double Y,double& Z)
        {
            if (AnastasisTerrainForge::SampleActiveWater(X,Y,Z)) return true;
            Z=AnastasisTerrainSurface::WaterPlaneZ;
            return true;
        };
        Habitat.SampleRiparian = [](double X, double Y, double& W) { return AnastasisDrainage::RiparianAt(X, Y, W); };
        const bool bMacro = ForestDressing.bMacroForest && CVarMacroForest.GetValueOnGameThread() != 0;
        if (!AnastasisEcologicalDressing::Build(CanonicalSource, ForestDressing, ForestPlan, Error,
            bMacro ? &Habitat : nullptr))
        {
            UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_ECOLOGY rejected=%s"), *Error);
        }
        else
        {
            AnastasisForestStructure::FReport StructureReport;
            TArray<AnastasisForestStructure::FNote> StructureNotes;
            FString StructureError;
            if (!AnastasisForestStructure::Shape(CanonicalSource, ForestDressing, bMacro,
                ForestPlan, StructureNotes, StructureReport, StructureError))
            {
                UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_FOREST_STRUCTURE rejected=%s"), *StructureError);
            }
            else
            {
                UE_LOG(LogAnastasis_UnrealV2, Display,
                    TEXT("ANASTASIS_FOREST_STRUCTURE before=%d after=%d young=%d mature=%d old=%d disturbed=%d clearing=%d large=%d moved=%d"),
                    StructureReport.Before, StructureReport.After,
                    StructureReport.ByStand[0], StructureReport.ByStand[1], StructureReport.ByStand[2],
                    StructureReport.ByStand[3], StructureReport.ByStand[4],
                    StructureReport.LargeClearings, StructureReport.Moved);
            }
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
            // FOREST_TERRAIN_P1 : etages d'altitude RELATIFS au relief reellement rendu -- la
            // carte n'est pas refaite, son relief existant est zone.
            const bool bSpecies = CVarTreeSpecies.GetValueOnGameThread() != 0;
            const double AltitudeSpan = FMath::Max(ActiveFootprintBounds.IsValid
                ? ActiveFootprintBounds.Max.Z - AnastasisTerrainSurface::WaterPlaneZ : 0.0, 100.0);
            int32 SpeciesCounts[8] = {};
            int32 RealHeightTrees = 0;
            struct FHeld
            {
                FTransform Pose;
                UHierarchicalInstancedStaticMeshComponent* Mass = nullptr;
                EAnastasisTreeSpecies Species = EAnastasisTreeSpecies::Any;
                double HeightM = 0.0;
                double Crown = 1.0;
                double GroundZ = 0.0;
                double HeightCm = 0.0;
                double Dryness = 0.0;
                double Jitter = 0.0;
                FVector2D Ground = FVector2D::ZeroVector;
                FBox MassBounds;
            };
            TArray<FHeld> Held;
            Held.Reserve(ForestPlan.Instances.Num());
            for (const auto& P : ForestPlan.Instances)
            {
                double GroundZ;
                if (!(AnastasisTerrainForge::SampleActive(P.Ground.X, P.Ground.Y, GroundZ)
                    || AnastasisTerrainSurface::SampleHeight(*SurfaceCrop, P.Ground.X, P.Ground.Y, GroundZ)))
                    continue;
                double WaterZ = AnastasisTerrainSurface::WaterPlaneZ;
                if (Snapshot.bHumanGeography && AnastasisTerrainForge::SampleActiveWater(P.Ground.X, P.Ground.Y, WaterZ) && GroundZ <= WaterZ + 25.0) continue;
                const auto& T = CanonicalSource.Tiles[P.SourceIndex];
                const EAnastasisStatureClass Stature = StatureForLayer(P.Layer, P.VisualSeed, T.X, T.Y);
                // Species comes from the site, not from a blind draw. Nothing here moves a tree.
                // FOREST_TERRAIN_P1 : altitude relative, pente et riviere RENDUES, plus Shade et
                // Wetness de la simulation. Sans la CVar, la famille pontique d'origine.
                AnastasisPresentation::FTreeSite Site;
                Site.AltitudeFraction = (GroundZ - AnastasisTerrainSurface::WaterPlaneZ) / AltitudeSpan;
                Site.SlopeDegrees = P.SlopeDegrees;
                double Riparian = 0.0;
                if (AnastasisDrainage::RiparianAt(P.Ground.X, P.Ground.Y, Riparian)) Site.Riparian = Riparian;
                Site.Wetness = T.Wetness;
                Site.Shade = T.Shade;
                Site.bOpenGround = P.bLone;
                const EAnastasisTreeSpecies Species = bSpecies
                    ? AnastasisPresentation::SelectTreeSpecies(Site, P.VisualSeed, T.X, T.Y)
                    : EAnastasisTreeSpecies::Any;
                const EAnastasisFoliageFamily Family = Species != EAnastasisTreeSpecies::Any
                    ? AnastasisPresentation::FamilyOfSpecies(Species)
                    : AnastasisPresentation::SelectFoliageFamily(T.Shade, T.Wetness, P.VisualSeed, T.X, T.Y);
                SiteShade.Add(T.Shade);
                SiteWetness.Add(T.Wetness);
                SiteConiferousness.Add(AnastasisPresentation::Coniferousness(T.Shade, T.Wetness));
                AnastasisPresentation::FResolvedPresentation R;
                if (!AnastasisPresentation::ResolvePresentation(AnastasisWorld::ETileType::Forest,
                    P.VisualSeed, T.X, T.Y, R, Stature, Family, Species)) continue;
                auto* M = Prepare(R);
                if (!M) continue;
                FTransform Pose = AnastasisPresentation::ResolveInstanceTransform(*R.Entry,
                    P.VisualSeed, T.X, T.Y, T.Alt, R.ScaleBias);
                // Actual mesh bounds, not the resolver's 100uu primitive pivot convention.
                const FBox MeshBounds = R.Mesh->GetBoundingBox();
                const double MinZ = MeshBounds.Min.Z;
                double HeightM = 0.0;
                double Crown = 1.0;
                if (R.HeightRangeM.Y > 0.0)
                {
                    // FOREST_TERRAIN_P1 : UNE hauteur reelle -- l'etendue de l'espece, en metres,
                    // fois la maturite de l'arbre -- au lieu de trois enveloppes multipliees. La
                    // couronne varie en largeur independamment (+-12 %) : deux arbres de meme
                    // taille n'ont pas la meme silhouette.
                    HeightM = FMath::Lerp(static_cast<double>(R.HeightRangeM.X), static_cast<double>(R.HeightRangeM.Y),
                        TreeUnit(P.VisualSeed, T.X, T.Y, 0x31u)) * P.Maturity;
                    if (Stature == EAnastasisStatureClass::Emergent) HeightM *= EmergentHeightBonus;
                    const double Vertical = HeightM * 100.0 / FMath::Max(MeshBounds.Max.Z - MinZ, 1.0);
                    Crown = FMath::Lerp(0.88, 1.12, TreeUnit(P.VisualSeed, T.X, T.Y, 0x32u));
                    Pose.SetScale3D(FVector(Vertical * Crown, Vertical * Crown, Vertical));
                    ++RealHeightTrees;
                }
                else
                {
                    Pose.SetScale3D(Pose.GetScale3D() * P.ScaleMultiplier);
                }
                // Large trunks stay plumb on steep ground; random yaw still varies the skyline.
                if (bMacro) Pose.SetRotation(FRotator(0.0, Pose.Rotator().Yaw, 0.0).Quaternion());
                Pose.SetLocation(FVector(P.Ground.X, P.Ground.Y, GroundZ - MinZ * Pose.GetScale3D().Z));
                FHeld Tree;
                Tree.Pose = Pose;
                Tree.Mass = M;
                Tree.Species = Species;
                Tree.HeightM = HeightM;
                Tree.Crown = Crown;
                Tree.GroundZ = GroundZ;
                Tree.Dryness = FMath::Clamp(1.0 - FMath::Max(Site.Riparian, 1.5 * T.Wetness), 0.0, 1.0)
                    * FMath::Lerp(0.55, 1.0, TreeUnit(P.VisualSeed, T.X, T.Y, 0x33u));
                Tree.Jitter = TreeUnit(P.VisualSeed, T.X, T.Y, 0x34u) * 2.0 - 1.0;
                Tree.Ground = FVector2D(P.Ground.X, P.Ground.Y);
                Tree.MassBounds = MeshBounds;
                Tree.HeightCm = (MeshBounds.Max.Z - MinZ) * Pose.GetScale3D().Z;
                Held.Add(Tree);
                ++SpeciesCounts[static_cast<uint8>(R.Species) & 7];
                ++ForestLayerCounts[static_cast<uint8>(P.Layer)];
                ++StatureCounts[static_cast<uint8>(Stature)];
                ++FamilyCounts[static_cast<uint8>(Family)];
                const double HeightUU = Tree.HeightCm;
                TallestUU = FMath::Max(TallestUU, HeightUU);
                ShortestUU = FMath::Min(ShortestUU, HeightUU);
            }
            const bool bHero = CVarHeroCanopy.GetValueOnGameThread() != 0
                && !(GIsAutomationTesting && CVarHeroCanopyInAutomation.GetValueOnGameThread() == 0);
            TArray<AnastasisHeroCanopy::FCandidate> HeroCandidates;
            TArray<int32> HeroAt;
            HeroAt.Init(INDEX_NONE, Held.Num());
            int32 HeroPlaced = 0;
            int32 ShellPlaced = 0;
            int32 HeroMissing = 0;
            if (bHero && Held.Num() > 0)
            {
                HeroCandidates.Reserve(Held.Num());
                for (int32 Index = 0; Index < Held.Num(); ++Index)
                {
                    AnastasisHeroCanopy::FCandidate Candidate;
                    Candidate.Index = Index;
                    Candidate.Ground = Held[Index].Ground;
                    Candidate.Species = static_cast<uint8>(Held[Index].Species);
                    Candidate.Score = Held[Index].HeightM;
                    Candidate.GroundZ = Held[Index].GroundZ;
                    Candidate.HeightCm = Held[Index].HeightCm;
                    Candidate.Dryness = Held[Index].Dryness;
                    HeroCandidates.Add(Candidate);
                }
                TArray<AnastasisHeroCanopy::FHero> Heroes;
                TArray<AnastasisHeroCanopy::FShell> Shells;
                AnastasisHeroCanopy::FReport HeroReport;
                FString HeroError;
                if (!AnastasisHeroCanopy::Build(HeroCandidates, Heroes, Shells, HeroReport, HeroError))
                {
                    UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_HERO_CANOPY rejected=%s"), *HeroError);
                }
                else
                {
                    auto MeshOf = [](uint8 Species) -> const TCHAR*
                    {
                        switch (Species)
                        {
                        case static_cast<uint8>(EAnastasisTreeSpecies::AleppoPine):
                            return TEXT("/Game/Anastasis/Vegetation/Hero/SM_Hero_AleppoPine.SM_Hero_AleppoPine");
                        case static_cast<uint8>(EAnastasisTreeSpecies::Cypress):
                            return TEXT("/Game/Anastasis/Vegetation/Hero/SM_Hero_Cypress.SM_Hero_Cypress");
                        case static_cast<uint8>(EAnastasisTreeSpecies::HolmOak):
                            return TEXT("/Game/Anastasis/Vegetation/Hero/SM_Hero_HolmOak.SM_Hero_HolmOak");
                        case static_cast<uint8>(EAnastasisTreeSpecies::Olive):
                            return TEXT("/Game/Anastasis/Vegetation/Hero/SM_Hero_Olive.SM_Hero_Olive");
                        default: return nullptr;
                        }
                    };
                    TMap<FName, UHierarchicalInstancedStaticMeshComponent*> Existing;
                    for (UHierarchicalInstancedStaticMeshComponent* Mesh : HeroCanopyMeshes)
                    {
                        if (IsValid(Mesh)) Existing.Add(Mesh->GetFName(), Mesh);
                    }
                    auto Ensure = [&](FName Name, UStaticMesh* Mesh, bool bCollide, bool bShadow, float MinDraw, float MaxDraw)
                    {
                        UHierarchicalInstancedStaticMeshComponent* Hism = Existing.FindRef(Name);
                        if (!Hism)
                        {
                            Hism = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, Name);
                            Hism->SetFlags(RF_Transient);
                            Hism->SetupAttachment(GetRootComponent());
                            Hism->SetMobility(EComponentMobility::Movable);
                            Hism->SetGenerateOverlapEvents(false);
                            Hism->SetCanEverAffectNavigation(false);
                            Hism->RegisterComponent();
                            HeroCanopyMeshes.Add(Hism);
                            Existing.Add(Name, Hism);
                        }
                        Hism->SetStaticMesh(Mesh);
                        Hism->SetCollisionEnabled(bCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
                        if (bCollide) Hism->SetCollisionProfileName(TEXT("BlockAll"));
                        Hism->SetCastShadow(bShadow);
                        Hism->MinDrawDistance = MinDraw;
                        Hism->LDMaxDrawDistance = MaxDraw;
                        const int32 FarEnd = FMath::TruncToInt(MaxDraw);
                        Hism->SetCullDistances(FarEnd > 0 ? FarEnd * 3 / 4 : 0, FarEnd);
                        if (Hism->NumCustomDataFloats != 2) Hism->SetNumCustomDataFloats(2);
                        return Hism;
                    };
                    TMap<uint8, UHierarchicalInstancedStaticMeshComponent*> HeroMesh;
                    for (const AnastasisHeroCanopy::FHero& Hero : Heroes)
                    {
                        const TCHAR* Path = MeshOf(Hero.Species);
                        UStaticMesh* Mesh = Path ? LoadObject<UStaticMesh>(nullptr, Path) : nullptr;
                        if (!Mesh)
                        {
                            ++HeroMissing;
                            continue;
                        }
                        UHierarchicalInstancedStaticMeshComponent*& Slot = HeroMesh.FindOrAdd(Hero.Species);
                        if (!Slot)
                        {
                            Slot = Ensure(*FString::Printf(TEXT("HeroSpecies_%d"), Hero.Species), Mesh, true, true, 0.0f, 0.0f);
                        }
                        const FHeld& Tree = Held[Hero.Index];
                        const FBox Bounds = Mesh->GetBoundingBox();
                        const double MeshHeight = FMath::Max(Bounds.Max.Z - Bounds.Min.Z, 1.0);
                        const double Scale = Tree.HeightM * 100.0 / MeshHeight;
                        FTransform Pose = Tree.Pose;
                        Pose.SetScale3D(FVector(Scale * Tree.Crown, Scale * Tree.Crown, Scale));
                        Pose.SetLocation(FVector(Tree.Ground.X, Tree.Ground.Y, Tree.GroundZ - Bounds.Min.Z * Scale));
                        const int32 Instance = Slot->AddInstance(Pose, false);
                        if (Instance != INDEX_NONE)
                        {
                            Slot->SetCustomDataValue(Instance, 0, static_cast<float>(Tree.Dryness), false);
                            Slot->SetCustomDataValue(Instance, 1, static_cast<float>(Tree.Jitter), false);
                        }
                        HeroAt[Hero.Index] = Instance;
                        Canopy.Add(FVector(Tree.Ground.X, Tree.Ground.Y,
                            FVector2D(Bounds.GetExtent().X, Bounds.GetExtent().Y).GetMax() * Pose.GetScale3D().X));
                        ++HeroPlaced;
                        ++DressingInstanceCount;
                    }
                    const bool bShells = CVarCanopyShell.GetValueOnGameThread() != 0;
                    UStaticMesh* ShellMesh = bShells ? LoadObject<UStaticMesh>(nullptr,
                        TEXT("/Game/Anastasis/Vegetation/Hero/SM_CanopyShell.SM_CanopyShell")) : nullptr;
                    if (ShellMesh)
                    {
                        const FBox ShellBounds = ShellMesh->GetBoundingBox();
                        const double ShellRadius = FMath::Max(FVector2D(ShellBounds.GetExtent().X, ShellBounds.GetExtent().Y).GetMax(), 1.0);
                        const double ShellHeight = FMath::Max(ShellBounds.Max.Z - ShellBounds.Min.Z, 1.0);
                        TMap<uint64, UHierarchicalInstancedStaticMeshComponent*> Chunks;
                        for (const AnastasisHeroCanopy::FShell& Shell : Shells)
                        {
                            double CenterZ = 0.0;
                            double WaterZ = 0.0;
                            if (!AnastasisTerrainForge::SampleActive(Shell.Center.X, Shell.Center.Y, CenterZ)) continue;
                            if (AnastasisTerrainForge::SampleActiveWater(Shell.Center.X, Shell.Center.Y, WaterZ) && WaterZ > CenterZ + 4.0) continue;
                            const int32 CX = FMath::FloorToInt(Shell.Center.X / 6000.0);
                            const int32 CY = FMath::FloorToInt(Shell.Center.Y / 6000.0);
                            const uint64 Key = (static_cast<uint64>(static_cast<uint32>(CX)) << 32) | static_cast<uint32>(CY);
                            UHierarchicalInstancedStaticMeshComponent*& Chunk = Chunks.FindOrAdd(Key);
                            if (!Chunk)
                            {
                                Chunk = Ensure(*FString::Printf(TEXT("CanopyShell_%d_%d"), CX, CY), ShellMesh, false, false, 7000.0f, 120000.0f);
                            }
                            // Wide as the stand, tall as its crowns: the two scales are independent,
                            // or a 30 m stand grows a 30 m dome over 10 m trees.
                            const double Wide = Shell.RadiusCm / ShellRadius;
                            const double Tall = FMath::Max(Shell.TopCm - Shell.BaseCm, 100.0) / ShellHeight;
                            const FVector Location(Shell.Center.X, Shell.Center.Y,
                                Shell.GroundZ + Shell.BaseCm - ShellBounds.Min.Z * Tall);
                            const double Yaw = FMath::Frac(FMath::Sin(Shell.Center.X * 0.0123 + Shell.Center.Y * 0.0457) * 43758.5453) * 360.0;
                            const int32 Instance = Chunk->AddInstance(FTransform(FRotator(0.0, Yaw, 0.0), Location, FVector(Wide, Wide, Tall)), false);
                            // Custom data 0 is the crown's site dryness: left at zero, every shell
                            // took the lushest green of the vegetation material, brighter than any tree under it.
                            if (Instance != INDEX_NONE) Chunk->SetCustomDataValue(Instance, 0, static_cast<float>(Shell.Dryness), false);
                            ++ShellPlaced;
                        }
                    }
                    else if (bShells && Shells.Num() > 0)
                    {
                        ++HeroMissing;
                    }
                    UE_LOG(LogAnastasis_UnrealV2, Display,
                        TEXT("ANASTASIS_HERO_CANOPY enabled=1 heroes=%d stands=%d shells=%d shell_cvar=%d placed_heroes=%d placed_shells=%d missing=%d"),
                        HeroReport.Heroes, HeroReport.Stands, HeroReport.Shells, bShells ? 1 : 0, HeroPlaced, ShellPlaced, HeroMissing);
                }
            }
            else
            {
                UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_HERO_CANOPY enabled=0"));
            }
            for (int32 Index = 0; Index < Held.Num(); ++Index)
            {
                if (HeroAt.IsValidIndex(Index) && HeroAt[Index] != INDEX_NONE) continue;
                const FHeld& Tree = Held[Index];
                const int32 Instance = Tree.Mass->AddInstance(Tree.Pose, false);
                if (Instance != INDEX_NONE && Tree.Mass->NumCustomDataFloats >= 2)
                {
                    Tree.Mass->SetCustomDataValue(Instance, 0, static_cast<float>(Tree.Dryness), false);
                    Tree.Mass->SetCustomDataValue(Instance, 1, static_cast<float>(Tree.Jitter), false);
                }
                Canopy.Add(FVector(Tree.Ground.X, Tree.Ground.Y,
                    FVector2D(Tree.MassBounds.GetExtent().X, Tree.MassBounds.GetExtent().Y).GetMax() * Tree.Pose.GetScale3D().X));
                ++DressingInstanceCount;
            }
            for (UHierarchicalInstancedStaticMeshComponent* Mesh : HeroCanopyMeshes)
            {
                if (IsValid(Mesh)) Mesh->MarkRenderStateDirty();
            }
            UE_LOG(LogAnastasis_UnrealV2, Display,
                TEXT("ANASTASIS_ECOLOGY young=%d secondary=%d canopy=%d full_plan=%d refused_water_or_footprint=%d refused_slope=%d refused_spacing=%d macro=%d reserved_open=%d"),
                ForestLayerCounts[0], ForestLayerCounts[1], ForestLayerCounts[2], ForestPlan.Instances.Num(),
                ForestPlan.RejectedWaterOrFootprint, ForestPlan.RejectedSlope, ForestPlan.RejectedSpacing,
                bMacro, ForestPlan.RejectedOpenGround);
            // FOREST_TERRAIN_P2 : arbres isoles, bosquets et galerie de berge hors des masses.
            UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_FOREST_OPEN lone_trees=%d of=%d"),
                ForestPlan.LoneTrees, ForestPlan.Instances.Num());
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
            // FOREST_TERRAIN_P1 : l'essence REELLEMENT posee (apres repli eventuel sur la grammaire
            // non etiquetee, qui compte en untagged), et combien d'arbres ont une hauteur reelle.
            UE_LOG(LogAnastasis_UnrealV2, Display,
                TEXT("ANASTASIS_TREE_TAXA enabled=%d aleppo_pine=%d cypress=%d holm_oak=%d olive=%d plane_tree=%d black_pine=%d greek_fir=%d untagged=%d real_height=%d altitude_span_uu=%.0f"),
                bSpecies ? 1 : 0,
                SpeciesCounts[static_cast<uint8>(EAnastasisTreeSpecies::AleppoPine)],
                SpeciesCounts[static_cast<uint8>(EAnastasisTreeSpecies::Cypress)],
                SpeciesCounts[static_cast<uint8>(EAnastasisTreeSpecies::HolmOak)],
                SpeciesCounts[static_cast<uint8>(EAnastasisTreeSpecies::Olive)],
                SpeciesCounts[static_cast<uint8>(EAnastasisTreeSpecies::PlaneTree)],
                SpeciesCounts[static_cast<uint8>(EAnastasisTreeSpecies::BlackPine)],
                SpeciesCounts[static_cast<uint8>(EAnastasisTreeSpecies::GreekFir)],
                SpeciesCounts[static_cast<uint8>(EAnastasisTreeSpecies::Any)],
                RealHeightTrees, AltitudeSpan);
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
	// Les troncs, avant que le maquis n'ajoute buissons et rochers a Canopy.
	const TArray<FVector> Trunks = Canopy;
	PlaceUnderstory(CanonicalSource, Canopy, bEcology);
	PlaceTrunkContact(CanonicalSource, Trunks, bEcology);
	PlaceGroundCover(CanonicalSource, Places, Canopy, bEcology);
	PlaceMicroEcology(CanonicalSource, Places, Canopy, bEcology);
	PlaceRiverbank(CanonicalSource, true);
	EmbodyPlaces(PlaceInputs, Places, bPlaces, CanonicalSource, SupersededRuins);
}

namespace AnastasisGroundCoverEmbody
{
/** Ouverture d'une tuile de simulation : ou une prairie a le droit d'exister, avant pente, eau et couronnes. */
double TileOpenness(AnastasisWorld::ETileType Type)
{
	using AnastasisWorld::ETileType;
	switch (Type)
	{
	case ETileType::Grass: case ETileType::Field: return 1.0;
	// Foret : les couronnes posees excluent deja le sous-bois ; une trouee de foret est une clairiere.
	case ETileType::Scrub: case ETileType::Forest: return 0.8;
	// Eau de simulation : le drainage en a rendu l'essentiel a la terre (HYDRO_NETWORK_001) ;
	// la vraie nappe rendue est refusee par la regle d'eau, pas par le type de tuile.
	case ETileType::Water: return 0.8;
	case ETileType::Ruin: return 0.4;
	// Roche : la lande d'eboulis (H6) viendra ; une prairie n'y est qu'une exception.
	case ETileType::Stone: return 0.2;
	default: return 0.0;
	}
}

/** Habitat de lande d'une tuile : la roche y est chez elle, au contraire de la prairie (EZ1). */
double LandeOpenness(AnastasisWorld::ETileType Type)
{
	using AnastasisWorld::ETileType;
	switch (Type)
	{
	case ETileType::Stone: case ETileType::Grass: case ETileType::Scrub: return 1.0;
	case ETileType::Forest: return 0.7;
	case ETileType::Water: case ETileType::Ruin: return 0.6;
	case ETileType::Field: return 0.5;
	default: return 0.0;
	}
}

/** Ouverture interpolee entre centres de tuiles : pas de marche de 20 m a la frontiere d'une tuile. */
double OpennessAt(const AnastasisWorldView::FWorldVisualSnapshot& S, double X, double Y,
	double (*Openness)(AnastasisWorld::ETileType) = &TileOpenness)
{
	const double T = AnastasisWorldView::TileWorldSize * S.SpatialScale;
	const double U = X / T - 0.5, V = Y / T - 0.5;
	const int32 IX = FMath::FloorToInt(U), IY = FMath::FloorToInt(V);
	const double FX = U - IX, FY = V - IY;
	double Sum = 0.0, Weight = 0.0;
	for (int32 DY = 0; DY <= 1; ++DY)
	{
		for (int32 DX = 0; DX <= 1; ++DX)
		{
			const AnastasisWorldView::FVisualTile* Tile = AnastasisWorldView::FindTile(S, IX + DX, IY + DY);
			if (!Tile) continue;
			const double W = (DX ? FX : 1.0 - FX) * (DY ? FY : 1.0 - FY);
			Sum += W * Openness(Tile->Type);
			Weight += W;
		}
	}
	return Weight > 0.0 ? Sum / Weight : 0.0;
}
}

void AAnastasisWorldEmbodiment::PlaceUnderstory(const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource,
	TArray<FVector>& Canopy, bool bEnabled)
{
	namespace US = AnastasisUnderstory;
	// Vider, jamais detruire : meme regle que l'herbe et les lieux composes.
	for (UHierarchicalInstancedStaticMeshComponent* M : UnderstoryMeshes)
	{
		if (IsValid(M)) M->ClearInstances();
	}
	UnderstoryMeshes.RemoveAll([](const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& M) { return !IsValid(M); });
	const double T = AnastasisWorldView::TileWorldSize * CanonicalSource.SpatialScale;
	double Probe;
	const bool bGround = AnastasisTerrainForge::SampleActive((CanonicalSource.OriginX + CanonicalSource.W * 0.5) * T,
		(CanonicalSource.OriginY + CanonicalSource.H * 0.5) * T, Probe);
	const bool bAutomation = GIsAutomationTesting && CVarUnderstoryInAutomation.GetValueOnGameThread() == 0;
	if (!bEnabled || CVarUnderstory.GetValueOnGameThread() == 0 || !bGround || bAutomation)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_UNDERSTORY enabled=0 ecology=%d rendered_ground=%d automation=%d"),
			bEnabled, bGround, bAutomation);
		return;
	}
	const double Start = FPlatformTime::Seconds();
	US::FInputs In;
	In.Source = &CanonicalSource;
	In.SampleHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActive(X, Y, Z); };
	In.SampleWaterHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActiveWater(X, Y, Z); };
	In.SampleRiparian = [](double X, double Y, double& W) { return AnastasisDrainage::RiparianAt(X, Y, W); };
	In.Canopy = Canopy;
	In.Basin = ForgeBasin;
	In.bHasBasin = !ForgeBasin.IsZero();
	In.WaterPlaneZ = AnastasisTerrainSurface::WaterPlaneZ;
	In.AltitudeSpanUU = FMath::Max(ActiveFootprintBounds.IsValid
		? ActiveFootprintBounds.Max.Z - AnastasisTerrainSurface::WaterPlaneZ : 0.0, 100.0);
	US::FPlan UnderPlan;
	FString Error;
	if (!US::Build(In, US::FSettings(), UnderPlan, Error))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_UNDERSTORY rejected=%s"), *Error);
		return;
	}
	const double PlanMs = (FPlatformTime::Seconds() - Start) * 1000.0;

	UMaterialInterface* RockMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Anastasis/Materials/M_AnastasisRock.M_AnastasisRock"));
	if (!RockMaterial && BaseShapeMaterial)
	{
		// Repli : l'aplat de pierre des lieux composes, tant que create_tree_asset.py n'a pas tourne.
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(BaseShapeMaterial, this);
		Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.105f, 0.101f, 0.096f, 1.0f));
		RockMaterial = Mid;
	}
	TMap<FName, UHierarchicalInstancedStaticMeshComponent*> Existing;
	for (UHierarchicalInstancedStaticMeshComponent* M : UnderstoryMeshes) Existing.Add(M->GetFName(), M);
	TMap<FString, UHierarchicalInstancedStaticMeshComponent*> ByPath;
	TSet<FString> MissingPaths;
	int32 Placed = 0;
	for (const US::FInstance& P : UnderPlan.Instances)
	{
		const bool bRock = P.Kind == US::EKind::Rock;
		const FString Path = bRock ? US::RockMeshPath(P.Rock, P.Variant) : US::ShrubMeshPath(P.Kind, P.Variant);
		UHierarchicalInstancedStaticMeshComponent* Hism = ByPath.FindRef(Path);
		if (!Hism)
		{
			if (MissingPaths.Contains(Path)) continue;
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
			if (!Mesh) { MissingPaths.Add(Path); continue; }
			const FName Name(*FString::Printf(TEXT("Understory_%s"), *FPaths::GetBaseFilename(Path)));
			Hism = Existing.FindRef(Name);
			if (!Hism)
			{
				Hism = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,
					MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), Name));
				Hism->SetFlags(RF_Transient);
				Hism->SetupAttachment(GetRootComponent());
				Hism->SetMobility(EComponentMobility::Movable);
				Hism->SetGenerateOverlapEvents(false);
				Hism->SetCanEverAffectNavigation(false);
				Hism->RegisterComponent();
				UnderstoryMeshes.Add(Hism);
			}
			Hism->SetStaticMesh(Mesh);
			// Un rocher se heurte ; on traverse un buisson.
			Hism->SetCollisionEnabled(bRock ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			if (bRock) Hism->SetCollisionProfileName(TEXT("BlockAll"));
			Hism->SetCastShadow(true);
			// Coupe par instance : maquis lisible a 300 m (la texture des versants), ronces 150 m,
			// rochers 400 m. Le fondu commence aux trois quarts.
			const int32 CullEnd = bRock ? 40000 : (P.Kind == US::EKind::Bramble ? 15000 : 30000);
			Hism->SetCullDistances(CullEnd * 3 / 4, CullEnd);
			if (bRock && RockMaterial)
			{
				for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot) Hism->SetMaterial(Slot, RockMaterial);
			}
			if (!bRock && Hism->NumCustomDataFloats != 2) Hism->SetNumCustomDataFloats(2);
			ByPath.Add(Path, Hism);
		}
		const FBox Bounds = Hism->GetStaticMesh()->GetBoundingBox();
		const FVector Size = Bounds.GetSize();
		double Scale;
		FVector Up;
		double Sink;
		FVector Scale3;
		if (bRock)
		{
			// Taille = plus grande dimension : un rocher bas et large reste un rocher de cette taille.
			Scale = P.HeightM * 100.0 / FMath::Max(Size.GetMax(), 1.0);
			Up = FMath::Lerp(FVector::UpVector, P.Normal, 0.8).GetSafeNormal();
			const FVector2D Lean = FVector2D(FMath::Cos(P.Jitter * 6.2832), FMath::Sin(P.Jitter * 6.2832)) * 0.18 * P.Jitter;
			Up = (Up + FVector(Lean, 0.0)).GetSafeNormal();
			// Enfoui de 15 a 40 % : un rocher pose sur l'herbe se lit comme un objet, pas comme le sol.
			Sink = (0.15 + 0.25 * P.Jitter) * Size.Z * Scale
				+ 0.5 * FVector2D(Size.X, Size.Y).GetMax() * Scale * FMath::Tan(FMath::DegreesToRadians(FMath::Min(P.SlopeDegrees, 40.0)) * 0.2);
			Scale3 = FVector(Scale);
		}
		else
		{
			Scale = P.HeightM * 100.0 / FMath::Max(Size.Z, 1.0);
			Up = FMath::Lerp(FVector::UpVector, P.Normal, 0.5).GetSafeNormal();
			const double Residual = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, P.Normal), -1.0, 1.0));
			Sink = 0.03 * Size.Z * Scale + 0.5 * FVector2D(Size.X, Size.Y).GetMax() * Scale * FMath::Tan(Residual);
			const double Crown = FMath::Lerp(0.85, 1.15, P.Jitter);
			Scale3 = FVector(Scale * Crown, Scale * Crown, Scale);
		}
		const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Up) * FQuat(FVector::UpVector, FMath::DegreesToRadians(P.Yaw));
		const FVector Location(P.Ground.X, P.Ground.Y, P.Ground.Z - Bounds.Min.Z * Scale3.Z - Sink);
		const int32 Index = Hism->AddInstance(FTransform(Rotation, Location, Scale3), false);
		if (!bRock && Index != INDEX_NONE && Hism->NumCustomDataFloats >= 2)
		{
			Hism->SetCustomDataValue(Index, 0, static_cast<float>(P.Dryness * FMath::Lerp(0.55, 1.0, P.Jitter)), false);
			Hism->SetCustomDataValue(Index, 1, static_cast<float>(P.Jitter * 2.0 - 1.0), false);
		}
		// L'herbe s'ecarte des buissons et ne traverse pas les rochers.
		Canopy.Add(FVector(P.Ground.X, P.Ground.Y, 0.5 * FVector2D(Size.X, Size.Y).GetMax() * Scale3.X * (bRock ? 0.7 : 0.9)));
		++Placed;
	}
	for (UHierarchicalInstancedStaticMeshComponent* M : UnderstoryMeshes)
	{
		if (IsValid(M)) M->MarkRenderStateDirty();
	}
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_UNDERSTORY enabled=1 lentisk=%d kermes_oak=%d broom=%d bramble=%d rock=%d placed=%d cells=%d refused_water=%d refused_reserved=%d components=%d missing_meshes=%d truncated=%d plan_ms=%.1f total_ms=%.1f"),
		UnderPlan.Counts[static_cast<int32>(US::EKind::Lentisk)], UnderPlan.Counts[static_cast<int32>(US::EKind::KermesOak)],
		UnderPlan.Counts[static_cast<int32>(US::EKind::Broom)], UnderPlan.Counts[static_cast<int32>(US::EKind::Bramble)],
		UnderPlan.Counts[static_cast<int32>(US::EKind::Rock)], Placed, UnderPlan.Cells, UnderPlan.RejectedWater, UnderPlan.RejectedReserved,
		UnderstoryMeshes.Num(), MissingPaths.Num(), UnderPlan.bTruncated ? 1 : 0, PlanMs, (FPlatformTime::Seconds() - Start) * 1000.0);
}

void AAnastasisWorldEmbodiment::PlaceTrunkContact(const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource,
	const TArray<FVector>& Trunks, bool bEnabled)
{
	for (UHierarchicalInstancedStaticMeshComponent* M : TrunkContactMeshes)
	{
		if (IsValid(M)) M->ClearInstances();
	}
	TrunkContactMeshes.RemoveAll([](const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& M) { return !IsValid(M); });
	const double Tile = AnastasisWorldView::TileWorldSize * CanonicalSource.SpatialScale;
	double Probe = 0.0;
	const bool bGround = AnastasisTerrainForge::SampleActive(
		(CanonicalSource.OriginX + CanonicalSource.W * 0.5) * Tile,
		(CanonicalSource.OriginY + CanonicalSource.H * 0.5) * Tile, Probe);
	if (!bEnabled || CVarTrunkContact.GetValueOnGameThread() == 0 || !bGround)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_TRUNK_CONTACT enabled=0 ecology=%d rendered_ground=%d"),
			bEnabled, bGround);
		return;
	}
	namespace TC = AnastasisTrunkContact;
	TArray<TC::FPatch> Patches;
	TC::FReport Report;
	FString Error;
	if (!TC::Build(Trunks, CanonicalSource.Seed, Patches, Report, Error))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_TRUNK_CONTACT rejected=%s"), *Error);
		return;
	}
	const TCHAR* Paths[2] = {
		TEXT("/Game/Anastasis/GroundCover/SM_Grass_MeadowShort_01.SM_Grass_MeadowShort_01"),
		TEXT("/Game/Anastasis/GroundCover/SM_Grass_Sedge_01.SM_Grass_Sedge_01"),
	};
	UHierarchicalInstancedStaticMeshComponent* ByKind[2] = {nullptr, nullptr};
	double MeshRadius[2] = {20.0, 20.0};
	int32 Missing = 0;
	TMap<FName, UHierarchicalInstancedStaticMeshComponent*> Existing;
	for (UHierarchicalInstancedStaticMeshComponent* M : TrunkContactMeshes) Existing.Add(M->GetFName(), M);
	for (int32 Kind = 0; Kind < 2; ++Kind)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Paths[Kind]);
		if (!Mesh) { ++Missing; continue; }
		const FBox Bounds = Mesh->GetBoundingBox();
		MeshRadius[Kind] = FVector2D(Bounds.GetExtent().X, Bounds.GetExtent().Y).GetMax();
		const FName Name = Kind == 0 ? TEXT("TrunkContact_Litter") : TEXT("TrunkContact_Moss");
		UHierarchicalInstancedStaticMeshComponent* Hism = Existing.FindRef(Name);
		if (!Hism)
		{
			Hism = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,
				MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), Name));
			Hism->SetFlags(RF_Transient);
			Hism->SetupAttachment(GetRootComponent());
			Hism->SetMobility(EComponentMobility::Movable);
			Hism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Hism->SetGenerateOverlapEvents(false);
			Hism->SetCanEverAffectNavigation(false);
			Hism->RegisterComponent();
			TrunkContactMeshes.Add(Hism);
		}
		Hism->SetStaticMesh(Mesh);
		Hism->SetCastShadow(false);
		// Fondu entre 10 et 16 m : le pied se lit a hauteur d'homme, pas depuis la crete.
		Hism->SetCullDistances(1000, 1600);
		ByKind[Kind] = Hism;
	}
	int32 Placed = 0, RefusedGround = 0, RefusedWater = 0;
	for (const TC::FPatch& Patch : Patches)
	{
		UHierarchicalInstancedStaticMeshComponent* Hism = ByKind[Patch.Kind == 1 ? 1 : 0];
		if (!Hism) continue;
		double Z = 0.0, Zx = 0.0, Zy = 0.0, Water = 0.0;
		if (!AnastasisTerrainForge::SampleActive(Patch.Position.X, Patch.Position.Y, Z)) { ++RefusedGround; continue; }
		if (AnastasisTerrainForge::SampleActiveWater(Patch.Position.X, Patch.Position.Y, Water) && Water > Z + 4.0)
		{
			++RefusedWater;
			continue;
		}
		const bool bX = AnastasisTerrainForge::SampleActive(Patch.Position.X + 40.0, Patch.Position.Y, Zx);
		const bool bY = AnastasisTerrainForge::SampleActive(Patch.Position.X, Patch.Position.Y + 40.0, Zy);
		const FVector Normal = (bX && bY)
			? FVector::CrossProduct(FVector(0.0, 40.0, Zy - Z), FVector(40.0, 0.0, Zx - Z)).GetSafeNormal()
			: FVector::UpVector;
		const FVector Up = FMath::Lerp(FVector::UpVector, Normal.Z < 0.0 ? -Normal : Normal, 0.85).GetSafeNormal();
		const double Residual = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, Normal.Z < 0.0 ? -Normal : Normal), -1.0, 1.0));
		const double Sink = 3.0 + MeshRadius[Patch.Kind] * Patch.ScaleXY * FMath::Tan(Residual);
		const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Up)
			* FQuat(FVector::UpVector, FMath::DegreesToRadians(Patch.YawDegrees));
		Hism->AddInstance(FTransform(Rotation,
			FVector(Patch.Position.X, Patch.Position.Y, Z - Sink),
			FVector(Patch.ScaleXY, Patch.ScaleXY, Patch.ScaleZ)), false);
		++Placed;
	}
	for (UHierarchicalInstancedStaticMeshComponent* M : TrunkContactMeshes)
	{
		if (IsValid(M)) M->MarkRenderStateDirty();
	}
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_TRUNK_CONTACT enabled=1 trunks=%d patches=%d litter=%d moss=%d placed=%d refused_ground=%d refused_water=%d missing_meshes=%d"),
		Report.Trunks, Report.Patches, Report.Litter, Report.Moss, Placed, RefusedGround, RefusedWater, Missing);
}

void AAnastasisWorldEmbodiment::PlaceGroundCover(const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource,
	const AnastasisPlaces::FPlan& Places, const TArray<FVector>& Canopy, bool bEnabled)
{
	namespace GC = AnastasisGroundCover;
	// Vider, jamais detruire : un HISM recree sous le meme nom pendant que son arbre asynchrone
	// se construit tue l'editeur (assertion InstanceReorderTable, cf. AnastasisPlaces::Embody).
	for (UHierarchicalInstancedStaticMeshComponent* M : GroundCoverMeshes)
	{
		if (IsValid(M)) M->ClearInstances();
	}
	GroundCoverMeshes.RemoveAll([](const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& M) { return !IsValid(M); });
	// Le sol rendu est la condition : sans forge active, il n'y a pas de sol ou poser une touffe.
	const double T = AnastasisWorldView::TileWorldSize * CanonicalSource.SpatialScale;
	double Probe;
	const bool bGround = AnastasisTerrainForge::SampleActive((CanonicalSource.OriginX + CanonicalSource.W * 0.5) * T,
		(CanonicalSource.OriginY + CanonicalSource.H * 0.5) * T, Probe);
	const bool bAutomation = GIsAutomationTesting && CVarGroundCoverInAutomation.GetValueOnGameThread() == 0;
	if (!bEnabled || CVarGroundCover.GetValueOnGameThread() == 0 || !bGround || bAutomation)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_GROUND_COVER enabled=0 ecology=%d rendered_ground=%d automation=%d"),
			bEnabled, bGround, bAutomation);
		return;
	}
	const double Start = FPlatformTime::Seconds();
	GC::FInputs In;
	// Le sol et l'eau REELLEMENT rendus (forge + drainage), comme la foret macro.
	In.SampleHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActive(X, Y, Z); };
	In.SampleWaterHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActiveWater(X, Y, Z); };
	In.SampleWetness = [](double X, double Y, double& W) { return AnastasisDrainage::RiparianAt(X, Y, W); };
	// Toute la carte : l'ouverture des tuiles, relevee a 1 dans la vallee ecrite (Human_Geography_V2).
	// v1-v3 ne lisaient que la vallee : les espaces ouverts hors vallees restaient nus.
	In.Mask = [&CanonicalSource](double X, double Y)
	{
		return FMath::Max(AnastasisPlaces::ValleyWeightAt(CanonicalSource, X, Y),
			AnastasisGroundCoverEmbody::OpennessAt(CanonicalSource, X, Y));
	};
	// Versants 20-45 deg : la lande (H6) lit son propre habitat, roche comprise.
	In.LandeMask = [&CanonicalSource](double X, double Y)
	{
		return AnastasisGroundCoverEmbody::OpennessAt(CanonicalSource, X, Y, &AnastasisGroundCoverEmbody::LandeOpenness);
	};
	// Fond de vallee habitable de la forge : la callune se mesure depuis lui.
	In.bHasValleyFloor = !ForgeBasin.IsZero();
	In.ValleyFloorZ = ForgeBasin.Z;
	In.Bounds = FBox2D(FVector2D(CanonicalSource.OriginX * T, CanonicalSource.OriginY * T),
		FVector2D((CanonicalSource.OriginX + CanonicalSource.W) * T, (CanonicalSource.OriginY + CanonicalSource.H) * T));
	In.Canopy = Canopy;
	In.Seed = CanonicalSource.Seed;
	for (const AnastasisPlaces::FPlace& P : Places.Places)
	{
		// Le hameau : sol pietine (planche lisiere apres defrichement), herbe rase et clairsemee.
		if (P.Kind == AnastasisPlaces::EKind::Hamlet) In.Clearings.Add({P.Center, P.Radius, 0.3});
	}
	// Micro-implantation : le sentier (HO01_Tread) est de la terre battue, la cour du puits
	// (HO01_Yard) une herbe rase. Aucun acteur tague : la carte principale ne change pas.
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const FVector Loc = It->GetActorLocation();
			if (It->ActorHasTag(TEXT("HO01_Tread"))) In.Clearings.Add({FVector2D(Loc.X, Loc.Y), 100.0, 0.0});
			else if (It->ActorHasTag(TEXT("HO01_Yard"))) In.Clearings.Add({FVector2D(Loc.X, Loc.Y), 380.0, 0.22});
		}
	}
	GC::FPlan Cover;
	FString Error;
	if (!GC::Build(In, GC::FSettings(), Cover, Error))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_GROUND_COVER rejected=%s"), *Error);
		return;
	}
	const double PlanMs = (FPlatformTime::Seconds() - Start) * 1000.0;

	// ECLAIRCIE DE DISTANCE. Deux tiers par famille (chacun decoupe en tuiles, plus bas) :
	//   proche   : 1 - FarShare des touffes, ombres portees, fondues vers leur pivot entre 40 et 55 m ;
	//   lointain : FarShare des touffes, agrandies, sans ombres, fondues entre 70 et 105 m.
	// Pres de l'oeil, toutes les touffes ; au loin, une sur trois, plus grande : a incidence
	// rasante les touffes se recouvrent, la couverture a l'ecran tient, et le nombre d'instances
	// soumises au-dela de 55 m est divise par trois. Les bornes de fondu passent au materiau par
	// MID (FadeStart / FadeEnd de M_AnastasisGrass) ; la coupe du HISM suit la fin du fondu.
	constexpr double FarShare = 0.34, FarScale = 1.25;
	struct FTier { const TCHAR* Suffix; double FadeStart, FadeEnd; int32 CullEnd; bool bShadow; };
	const bool bShadows = CVarGroundCoverShadows.GetValueOnGameThread() != 0;
	const FTier Tiers[2] = {{TEXT("Near"), 4000.0, 5500.0, 5700, bShadows}, {TEXT("Far"), 7000.0, 10500.0, 10800, false}};

	// TUILES DE 160 M. v4 posait ~1 M d'instances dans six HISM couvrant toute la carte : +12 ms de
	// frame (1,5 ms de GPU seulement), les memes dans chaque vue, hameau compris -- le cout suivait
	// le NOMBRE d'instances, pas ce qui est visible : chaque frame parcourait les six arbres de
	// clusters entiers. Un HISM par tuile, famille et tier, avec une distance d'affichage de
	// primitive : le moteur ecarte d'un bloc les tuiles loin de l'oeil, seules les voisines sont
	// parcourues. La distance est mesuree au centre des bornes : coupe + demi-diagonale.
	constexpr double ChunkUU = 16000.0;
	const double ChunkReach = ChunkUU * 0.5 * UE_SQRT_2;
	UStaticMesh* Meshes[GC::FamilyCount] = {};
	UMaterialInstanceDynamic* Mids[GC::FamilyCount][2] = {};
	double MeshRadius[GC::FamilyCount] = {};
	int32 Missing = 0, Placed = 0, OutsideValley = 0, Chunks = 0;
	for (int32 F = 0; F < GC::FamilyCount; ++F)
	{
		Meshes[F] = LoadObject<UStaticMesh>(nullptr, *GC::MeshPath(static_cast<GC::EFamily>(F)));
		if (!Meshes[F]) { ++Missing; continue; }
		const FBox Bounds = Meshes[F]->GetBoundingBox();
		MeshRadius[F] = FVector2D(Bounds.GetExtent().X, Bounds.GetExtent().Y).GetMax();
		if (UMaterialInterface* Base = Meshes[F]->GetMaterial(0))
		{
			for (int32 K = 0; K < 2; ++K)
			{
				Mids[F][K] = UMaterialInstanceDynamic::Create(Base, this);
				Mids[F][K]->SetScalarParameterValue(TEXT("FadeStart"), Tiers[K].FadeStart);
				Mids[F][K]->SetScalarParameterValue(TEXT("FadeEnd"), Tiers[K].FadeEnd);
			}
		}
	}
	// Cle : famille, tier, tuile. Ordre de remplissage = ordre du plan : deterministe.
	const auto KeyOf = [](int32 F, int32 K, int32 CX, int32 CY)
	{
		return (static_cast<uint64>(F * 2 + K) << 32) | (static_cast<uint64>(CX & 0xFFFF) << 16) | static_cast<uint64>(CY & 0xFFFF);
	};
	TMap<uint64, TArray<FTransform>> Batches;
	for (const GC::FPlacement& P : Cover.Instances)
	{
		const int32 F = static_cast<int32>(P.Family);
		if (!Meshes[F]) continue;
		const int32 K = P.Thin < FarShare ? 1 : 0;
		const double Scale = P.Scale * (K == 1 ? FarScale : 1.0);
		// La touffe suit 70 % de la pente : l'herbe pousse vers le ciel, mais une touffe
		// droite sur 20 degres flotterait cote aval. Le reste est rattrape en l'enfoncant.
		const FVector Up = FMath::Lerp(FVector::UpVector, P.Normal, 0.7).GetSafeNormal();
		const double Residual = FMath::Acos(FMath::Clamp(FVector::DotProduct(Up, P.Normal), -1.0, 1.0));
		const double Sink = 2.0 + MeshRadius[F] * Scale * FMath::Tan(Residual);
		const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Up) * FQuat(FVector::UpVector, FMath::DegreesToRadians(P.Yaw));
		const int32 CX = FMath::FloorToInt(P.Ground.X / ChunkUU), CY = FMath::FloorToInt(P.Ground.Y / ChunkUU);
		Batches.FindOrAdd(KeyOf(F, K, CX, CY)).Add(FTransform(Rotation, P.Ground - FVector(0, 0, Sink), FVector(Scale)));
		OutsideValley += AnastasisPlaces::ValleyWeightAt(CanonicalSource, P.Ground.X, P.Ground.Y) < 0.05 ? 1 : 0;
	}
	TMap<FName, UHierarchicalInstancedStaticMeshComponent*> Existing;
	for (UHierarchicalInstancedStaticMeshComponent* M : GroundCoverMeshes) Existing.Add(M->GetFName(), M);
	int32 PerTier[2] = {};
	for (TPair<uint64, TArray<FTransform>>& Batch : Batches)
	{
		const int32 FK = static_cast<int32>(Batch.Key >> 32), F = FK / 2, K = FK % 2;
		const int32 CX = static_cast<int16>((Batch.Key >> 16) & 0xFFFF), CY = static_cast<int16>(Batch.Key & 0xFFFF);
		const FTier& Tier = Tiers[K];
		const FName Name(*FString::Printf(TEXT("GroundCover_%s_%s_%d_%d"), GC::FamilyName(static_cast<GC::EFamily>(F)), Tier.Suffix, CX, CY));
		UHierarchicalInstancedStaticMeshComponent* Made = Existing.FindRef(Name);
		if (!Made)
		{
			Made = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,
				MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), Name));
			Made->SetFlags(RF_Transient);
			Made->SetupAttachment(GetRootComponent());
			Made->SetMobility(EComponentMobility::Movable);
			// On traverse une prairie : ni collision, ni navigation.
			Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Made->SetGenerateOverlapEvents(false);
			Made->SetCanEverAffectNavigation(false);
			Made->RegisterComponent();
			GroundCoverMeshes.Add(Made);
		}
		Made->SetStaticMesh(Meshes[F]);
		Made->SetCastShadow(Tier.bShadow);
		Made->SetCullDistances(static_cast<int32>(Tier.FadeStart), Tier.CullEnd);
		Made->LDMaxDrawDistance = static_cast<float>(Tier.CullEnd + ChunkReach);
		Made->SetCachedMaxDrawDistance(Made->LDMaxDrawDistance);
		if (Mids[F][K]) Made->SetMaterial(0, Mids[F][K]);
		Made->AddInstances(Batch.Value, false, false, false);
		Made->MarkRenderStateDirty();
		Placed += Batch.Value.Num();
		PerTier[K] += Batch.Value.Num();
		++Chunks;
	}
	// SOL SOUS L'HERBE : la couleur de sommet de la section de sol (la chromie large, cf.
	// GROUND_HYDROLOGY_ARBITRATION.md) est teintee par le champ de couverture. Relue depuis la
	// section elle-meme, que EmbodyCrop vient de recreer : aucune teinte ne s'accumule d'une
	// incarnation a l'autre. Les sommets non teintes repartent a l'octet pres.
	int32 TintedVertices = 0;
	double TintSum = 0.0;
	if (CVarGroundCoverSoilTint.GetValueOnGameThread() != 0 && ExperimentalSurface)
	{
		GC::FCoverField Field;
		GC::BuildCoverField(Cover, In.Bounds, 400.0, GC::FSettings().CellUU, Field);
		if (FProcMeshSection* Section = ExperimentalSurface->GetProcMeshSection(0))
		{
			const GC::FSoilTint Tint;
			const int32 N = Section->ProcVertexBuffer.Num();
			TArray<FColor> Colors;
			TArray<FVector> Positions;
			Colors.SetNumUninitialized(N);
			Positions.SetNumUninitialized(N);
			for (int32 I = 0; I < N; ++I)
			{
				const FProcMeshVertex& V = Section->ProcVertexBuffer[I];
				Colors[I] = V.Color;
				Positions[I] = V.Position;
				// La section est creee SANS conversion sRGB (CreateMeshSection_LinearColor,
				// bSRGBConversion=false) : ses octets sont lineaires. Les relire en sRGB
				// assombrirait et fausserait toute la teinte.
				double Amount = 0.0;
				const FLinearColor Tinted = GC::TintSoil(V.Color.ReinterpretAsLinear(), Field.Sample(V.Position.X, V.Position.Y), Tint, &Amount);
				if (Amount <= 0.001) continue;
				Colors[I] = Tinted.ToFColor(false);
				++TintedVertices;
				TintSum += Amount;
			}
			if (TintedVertices > 0)
			{
				// Les positions sont REQUISES, inchangees : UpdateMeshSection ne recopie rien --
				// couleurs comprises -- si le tableau de positions n'a pas le nombre de sommets
				// de la section (tint-v1 : 135 805 sommets "teintes", sol identique a l'image).
				ExperimentalSurface->UpdateMeshSection(0, Positions, TArray<FVector>(), TArray<FVector2D>(),
					TArray<FVector2D>(), TArray<FVector2D>(), TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>());
			}
		}
	}
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_SOIL_TINT enabled=%d tinted_vertices=%d mean_amount=%.3f"),
		CVarGroundCoverSoilTint.GetValueOnGameThread() != 0, TintedVertices, TintedVertices > 0 ? TintSum / TintedVertices : 0.0);

	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_GROUND_COVER enabled=1 tall=%d short=%d sedge=%d heath=%d heather=%d fern=%d harts=%d herb=%d understory=%d placed=%d near=%d far=%d chunks=%d outside_valley=%d shadows=%d candidates=%d refused_mask=%d refused_ground=%d refused_water=%d refused_slope=%d refused_canopy=%d refused_density=%d crowns=%d clearings=%d truncated=%d missing_meshes=%d plan_ms=%.1f total_ms=%.1f"),
		Cover.Counts[0], Cover.Counts[1], Cover.Counts[2], Cover.Counts[3], Cover.Counts[4],
		Cover.Counts[5], Cover.Counts[6], Cover.Counts[7], Cover.Understory, Placed, PerTier[0], PerTier[1], Chunks, OutsideValley, bShadows,
		Cover.Candidates, Cover.RejectedMask, Cover.RejectedGround, Cover.RejectedWater, Cover.RejectedSlope,
		Cover.RejectedCanopy, Cover.RejectedDensity, Canopy.Num(), In.Clearings.Num(), Cover.bTruncated, Missing,
		PlanMs, (FPlatformTime::Seconds() - Start) * 1000.0);
	// La lande se juge sur le relief reellement mesure : ou sont les pentes, et ce qui les refuse.
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_GROUND_SLOPES dry_candidates_by_slope 0-10=%d 10-20=%d 20-30=%d 30-45=%d 45-60=%d 60+=%d steep_refused_canopy=%d lande_above_floor_m=[%.1f %.1f %.1f] valley_floor_z=%.0f"),
		Cover.SlopeBins[0], Cover.SlopeBins[1], Cover.SlopeBins[2], Cover.SlopeBins[3], Cover.SlopeBins[4], Cover.SlopeBins[5],
		Cover.RejectedCanopySteep, Cover.LandeAboveFloor[0] / 100.0, Cover.LandeAboveFloor[1] / 100.0, Cover.LandeAboveFloor[2] / 100.0,
		In.ValleyFloorZ);
}

void AAnastasisWorldEmbodiment::PlaceMicroEcology(const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource,
	const AnastasisPlaces::FPlan& Places, const TArray<FVector>& Canopy, bool bEnabled)
{
	for (UHierarchicalInstancedStaticMeshComponent* M : MicroEcologyMeshes)
	{
		if (IsValid(M)) M->ClearInstances();
	}
	MicroEcologyMeshes.RemoveAll([](const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& M) { return !IsValid(M); });
	const bool bAutomation = GIsAutomationTesting && CVarMicroEcologyInAutomation.GetValueOnGameThread() == 0;
	if (!bEnabled || CVarMicroEcology.GetValueOnGameThread() == 0 || bAutomation)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_MICRO_ECOLOGY enabled=0 ecology=%d automation=%d"), bEnabled, bAutomation);
		return;
	}
	const double Start = FPlatformTime::Seconds();
	const double T = AnastasisWorldView::TileWorldSize * CanonicalSource.SpatialScale;
	AnastasisMicroEcology::FInputs In;
	In.SampleHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActive(X, Y, Z); };
	In.SampleWaterHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActiveWater(X, Y, Z); };
	In.SampleWetness = [](double X, double Y, double& W) { return AnastasisDrainage::RiparianAt(X, Y, W); };
	In.Mask = [&CanonicalSource](double X, double Y)
	{
		return FMath::Max(AnastasisPlaces::ValleyWeightAt(CanonicalSource, X, Y),
			AnastasisGroundCoverEmbody::OpennessAt(CanonicalSource, X, Y));
	};
	In.Bounds = FBox2D(FVector2D(CanonicalSource.OriginX * T, CanonicalSource.OriginY * T),
		FVector2D((CanonicalSource.OriginX + CanonicalSource.W) * T, (CanonicalSource.OriginY + CanonicalSource.H) * T));
	In.Canopy = Canopy;
	In.Seed = CanonicalSource.Seed;
	for (const AnastasisPlaces::FPlace& P : Places.Places)
	{
		if (P.Kind == AnastasisPlaces::EKind::Hamlet) In.Clearings.Add({P.Center, P.Radius});
	}
	AnastasisMicroEcology::FPlan Eco;
	FString Error;
	if (!AnastasisMicroEcology::Build(In, AnastasisMicroEcology::FSettings(), Eco, Error))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_MICRO_ECOLOGY rejected=%s"), *Error);
		return;
	}
	const double PlanMs = (FPlatformTime::Seconds() - Start) * 1000.0;
	const AnastasisMicroEcology::FEmbodyResult Embodied = AnastasisMicroEcology::Embody(*this, Eco, BaseShapeMaterial, MicroEcologyMeshes);
	int32 Tinted = 0;
	if (CVarMicroEcologySoil.GetValueOnGameThread() != 0)
	{
		Tinted = AnastasisMicroEcology::ApplySoil(ExperimentalSurface, Eco.Soil, 1.0);
	}
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_MICRO_ECOLOGY enabled=1 pebble=%d reed=%d tuft=%d drift=%d branch=%d stone=%d bush=%d edge_bush=%d edge_sapling=%d log=%d stump=%d under_branch=%d roots=%d under_sapling=%d instances=%d components=%d missing_meshes=%d tinted=%d truncated=%d plan_ms=%.1f total_ms=%.1f"),
		Eco.Counts[0], Eco.Counts[1], Eco.Counts[2], Eco.Counts[3], Eco.Counts[4], Eco.Counts[5], Eco.Counts[6],
		Eco.Counts[7], Eco.Counts[8], Eco.Counts[9], Eco.Counts[10], Eco.Counts[11], Eco.Counts[12], Eco.Counts[13],
		Embodied.Instances, Embodied.Components, Embodied.MissingMeshes, Tinted, Eco.bTruncated,
		PlanMs, (FPlatformTime::Seconds() - Start) * 1000.0);
}

void AAnastasisWorldEmbodiment::PlaceRiverbank(const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource, bool bEnabled)
{
	namespace RB = AnastasisRiverbank;
	// Vider, jamais detruire (assertion InstanceReorderTable, cf. PlaceGroundCover).
	for (UHierarchicalInstancedStaticMeshComponent* M : RiverbankMeshes)
	{
		if (IsValid(M)) M->ClearInstances();
	}
	RiverbankMeshes.RemoveAll([](const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& M) { return !IsValid(M); });
	const AnastasisDrainage::FNetwork& Network = AnastasisDrainage::GetActive();
	const double T = AnastasisWorldView::TileWorldSize * CanonicalSource.SpatialScale;
	double Probe;
	const bool bGround = AnastasisTerrainForge::SampleActive((CanonicalSource.OriginX + CanonicalSource.W * 0.5) * T,
		(CanonicalSource.OriginY + CanonicalSource.H * 0.5) * T, Probe);
	const bool bAutomation = GIsAutomationTesting && CVarRiverbankInAutomation.GetValueOnGameThread() == 0;
	if (!bEnabled || CVarRiverbank.GetValueOnGameThread() == 0 || !bGround || Network.GridW <= 0 || bAutomation)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_RIVERBANK enabled=0 rendered_ground=%d drainage=%d automation=%d"),
			bGround, Network.GridW > 0, bAutomation);
		return;
	}
	const double Start = FPlatformTime::Seconds();
	RB::FSpeedField Speed;
	RB::BuildSpeedField(Network, Speed);
	RB::FInputs In;
	In.SampleHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActive(X, Y, Z); };
	In.SampleWaterHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActiveWater(X, Y, Z); };
	In.Bounds = FBox2D(FVector2D(CanonicalSource.OriginX * T, CanonicalSource.OriginY * T),
		FVector2D((CanonicalSource.OriginX + CanonicalSource.W) * T, (CanonicalSource.OriginY + CanonicalSource.H) * T));
	In.Seed = CanonicalSource.Seed;
	RB::FPlan BankPlan;
	FString Error;
	if (!RB::Build(In, Speed, RB::FSettings(), BankPlan, Error))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_RIVERBANK rejected=%s"), *Error);
		return;
	}

	// Maillages existants : roseau (Ecotone, son materiau), galets et blocs
	// (Rock, materiau de forme teinte en pierre mouillee, plus sombre que celle des affleurements).
	struct FKind { AnastasisPlaces::EFamily Source; int32 Variants; int32 Cull; bool bShadow; bool bStone; };
	const FKind Kinds[RB::FamilyCount] = {
		{AnastasisPlaces::EFamily::Reed, 1, 9000, true, false},
		{AnastasisPlaces::EFamily::RockLow, 3, 6000, false, true},
		{AnastasisPlaces::EFamily::RockBoulder, 3, 20000, true, true},
	};
	UStaticMesh* Meshes[RB::FamilyCount][3] = {};
	FBox MeshBounds[RB::FamilyCount][3];
	int32 Missing = 0;
	for (int32 F = 0; F < RB::FamilyCount; ++F)
	{
		for (int32 V = 0; V < Kinds[F].Variants; ++V)
		{
			Meshes[F][V] = LoadObject<UStaticMesh>(nullptr, *AnastasisPlaces::MeshPath(Kinds[F].Source, V));
			if (Meshes[F][V]) MeshBounds[F][V] = Meshes[F][V]->GetBoundingBox();
			else ++Missing;
		}
	}
	UMaterialInstanceDynamic* WetStone = nullptr;
	if (BaseShapeMaterial)
	{
		WetStone = UMaterialInstanceDynamic::Create(BaseShapeMaterial, this);
		WetStone->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.088f, 0.084f, 0.077f, 1.0f));
	}
	constexpr double ChunkUU = 16000.0;
	const double ChunkReach = ChunkUU * 0.5 * UE_SQRT_2;
	TMap<FName, TArray<FTransform>> Batches;
	TMap<FName, TPair<int32, int32>> BatchKind;
	for (const RB::FPlacement& P : BankPlan.Instances)
	{
		const int32 F = static_cast<int32>(P.Family);
		const int32 V = FMath::Clamp(P.Variant, 0, Kinds[F].Variants - 1);
		UStaticMesh* Mesh = Meshes[F][V];
		if (!Mesh) continue;
		const FBox& MB = MeshBounds[F][V];
		const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(P.Yaw));
		const FVector TiltAxis(FMath::Cos(FMath::DegreesToRadians(P.TiltYaw)), FMath::Sin(FMath::DegreesToRadians(P.TiltYaw)), 0.0);
		const FQuat Rotation = FQuat(TiltAxis, FMath::DegreesToRadians(P.Tilt)) * Yaw;
		double Scale = P.Size;
		FVector At = P.Location;
		if (Kinds[F].bStone)
		{
			// Size = diametre vise : converti avec les bornes du maillage ; la pierre est posee par
			// sa base puis enfoncee de Sink de sa hauteur -- un galet sort du sol, il n'y est pas pose.
			const double Diameter = 2.0 * FMath::Max(MB.GetExtent().X, MB.GetExtent().Y);
			Scale = Diameter > 1.0 ? P.Size / Diameter : 1.0;
			At.Z = P.Location.Z - MB.Min.Z * Scale - P.Sink * MB.GetSize().Z * Scale;
		}
		const int32 CX = FMath::FloorToInt(At.X / ChunkUU), CY = FMath::FloorToInt(At.Y / ChunkUU);
		const FName Name(*FString::Printf(TEXT("Riverbank_%s_%d_%d_%d"), RB::FamilyName(P.Family), V, CX, CY));
		Batches.FindOrAdd(Name).Add(FTransform(Rotation, At, FVector(Scale)));
		BatchKind.Add(Name, TPair<int32, int32>(F, V));
	}
	TMap<FName, UHierarchicalInstancedStaticMeshComponent*> Existing;
	for (UHierarchicalInstancedStaticMeshComponent* M : RiverbankMeshes) Existing.Add(M->GetFName(), M);
	int32 Placed = 0, Chunks = 0;
	for (TPair<FName, TArray<FTransform>>& Batch : Batches)
	{
		const TPair<int32, int32> FV = BatchKind.FindChecked(Batch.Key);
		const FKind& Kind = Kinds[FV.Key];
		UHierarchicalInstancedStaticMeshComponent* Made = Existing.FindRef(Batch.Key);
		if (!Made)
		{
			Made = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,
				MakeUniqueObjectName(this, UHierarchicalInstancedStaticMeshComponent::StaticClass(), Batch.Key));
			Made->SetFlags(RF_Transient);
			Made->SetupAttachment(GetRootComponent());
			Made->SetMobility(EComponentMobility::Movable);
			Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Made->SetGenerateOverlapEvents(false);
			Made->SetCanEverAffectNavigation(false);
			Made->RegisterComponent();
			RiverbankMeshes.Add(Made);
		}
		Made->SetStaticMesh(Meshes[FV.Key][FV.Value]);
		if (Kind.bStone && WetStone) Made->SetMaterial(0, WetStone);
		Made->SetCastShadow(Kind.bShadow);
		Made->SetCullDistances(static_cast<int32>(Kind.Cull * 0.8), Kind.Cull);
		Made->LDMaxDrawDistance = static_cast<float>(Kind.Cull + ChunkReach);
		Made->SetCachedMaxDrawDistance(Made->LDMaxDrawDistance);
		Made->AddInstances(Batch.Value, false, false, false);
		Made->MarkRenderStateDirty();
		Placed += Batch.Value.Num();
		++Chunks;
	}
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_RIVERBANK enabled=1 reeds=%d reed_clumps=%d cobbles=%d boulders=%d placed=%d chunks=%d calm_shore=%d fast_shore=%d truncated=%d missing_meshes=%d plan_ms=%.1f total_ms=%.1f"),
		BankPlan.Counts[0], BankPlan.ReedClumps, BankPlan.Counts[1], BankPlan.Counts[2], Placed, Chunks, BankPlan.CalmShore, BankPlan.FastShore,
		BankPlan.bTruncated, Missing, BankPlan.MilliSeconds, (FPlatformTime::Seconds() - Start) * 1000.0);
}

FVector AAnastasisWorldEmbodiment::GetFrameTimingsMs() const
{
	return FVector(FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime),
		FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
}

bool AAnastasisWorldEmbodiment::ComposePlaces(const AnastasisWorldView::FWorldVisualSnapshot* SurfaceCrop,
	const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource, AnastasisPlaces::FInputs& In, AnastasisPlaces::FPlan& Places)
{
	In.Source = &CanonicalSource;
	// Le sol et l'eau REELLEMENT rendus : un lieu se lit sur ce qu'on voit, pas sur la tuile.
	In.Ground = [SurfaceCrop](double X, double Y, double& Z)
	{
		return AnastasisTerrainForge::SampleActive(X, Y, Z)
			|| (SurfaceCrop && AnastasisTerrainSurface::SampleHeight(*SurfaceCrop, X, Y, Z));
	};
	In.Water = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActiveWater(X, Y, Z); };
	// Bassin et point haut ne valent que si la forge a tourne pour CETTE incarnation :
	// forge coupee, les membres gardent les valeurs du passage precedent.
	double Probe = 0.0;
	const bool bForged = AnastasisTerrainForge::SampleActive(ForgeLandmark.X, ForgeLandmark.Y, Probe);
	In.bLandmark = bForged && !ForgeLandmark.IsZero();
	In.Landmark = ForgeLandmark;
	In.bBasin = bForged && !ForgeBasin.IsZero();
	In.Basin = ForgeBasin;
	FString Error;
	const bool bEnabled = SurfaceCrop && bComposePlaces && CVarPlaces.GetValueOnGameThread() != 0;
	if (bEnabled && !AnastasisPlaces::Compose(In, Places, Error))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_PLACES rejected=%s"), *Error);
	}
	return bEnabled;
}

void AAnastasisWorldEmbodiment::EmbodyPlaces(const AnastasisPlaces::FInputs& In, const AnastasisPlaces::FPlan& Places,
	bool bEnabled, const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource, int32 SupersededRuins)
{
	PlaceReport.Reset();
	// Plan vide si coupe : Embody vide alors les composants du passage precedent.
	const AnastasisPlaces::FEmbodyResult Result = AnastasisPlaces::Embody(*this, Places, In, BaseShapeMaterial, PlaceMeshes);
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_PLACES enabled=%d places=%d pieces=%d instances=%d components=%d ungrounded=%d missing_meshes=%d superseded_ruin_tiles=%d missing=[%s]"),
		bEnabled, Places.Places.Num(), Places.Pieces.Num(), Result.Instances, PlaceMeshes.Num(), Result.Ungrounded,
		Result.MissingMeshes, SupersededRuins, *FString::Join(Places.Missing, TEXT(",")));
	const double T = AnastasisWorldView::TileWorldSize * CanonicalSource.SpatialScale;
	for (int32 I = 0; I < Places.Places.Num(); ++I)
	{
		const AnastasisPlaces::FPlace& P = Places.Places[I];
		const FVector L = Result.PlaceLocations.IsValidIndex(I) ? Result.PlaceLocations[I] : FVector(P.Center, 0.0);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLACE id=%s name=\"%s\" at=(%.0f,%.0f,%.0f) tile=(%.1f,%.1f) radius_uu=%.0f pieces=%d"),
			*P.Id, *P.Name, L.X, L.Y, L.Z, P.Center.X / T, P.Center.Y / T, P.Radius, P.NumPieces);
		PlaceReport.Add(FString::Printf(TEXT("%s|%.0f|%.0f|%.0f|%.0f"), *P.Id, L.X, L.Y, L.Z, P.Radius));
	}
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

	auto CanonicalSource = AnastasisWorldView::CaptureCanonicalWorld(Seed);
	if (CVarTerrainSurface.GetValueOnGameThread() == 2 && CVarTerrainForge.GetValueOnGameThread() != 0)
	{
		CanonicalSource.SpatialScale = FMath::Clamp(static_cast<double>(CVarWorldScale.GetValueOnGameThread()), 1.0, 20.0);
		CanonicalSource.bHumanGeography = CVarHumanGeography.GetValueOnGameThread() != 0;
	}
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
    if (HorizonSurface) { HorizonSurface->ClearAllMeshSections(); HorizonSurface->SetVisibility(false); }
    for (auto& Mesh : TerrainMeshes) if (Mesh) { Mesh->SetVisibility(true); Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); }
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
            // TERRAIN_FORGE lit pente/Laplacien au bord fin de Crop. Mode 2 incarne un
            // decoupage LIBRE du monde canonique : ses bords sont de vrais bords de chunk,
            // pas le bord du monde, et CanonicalSource a les tuiles voisines pour les
            // couvrir -- sans elles, ANASTASIS_TERRAIN_FORGE clampait un voisin manquant sur
            // lui-meme et pouvait y lire une convexite fictive (pic sur pente raide, cf.
            // TERRAIN_FORGE_CHUNK_SEAM). Mode 1 est la tranche scellee WORLD_SLICE_006 et
            // doit rester bit-a-bit identique : elle ne recoit jamais de halo.
            AnastasisWorldView::FWorldVisualSnapshot HaloCrop;
            bool bHaveHaloCrop = false;
            if (SurfaceMode == 2)
            {
                const int32 Margin = AnastasisTerrainForge::HaloTiles;
                const int32 HaloX0 = FMath::Max(0, Crop.OriginX - Margin);
                const int32 HaloY0 = FMath::Max(0, Crop.OriginY - Margin);
                const int32 HaloX1 = FMath::Min(CanonicalSource.W, Crop.OriginX + Crop.W + Margin);
                const int32 HaloY1 = FMath::Min(CanonicalSource.H, Crop.OriginY + Crop.H + Margin);
                HaloCrop = AnastasisWorldView::CropSnapshot(
                    CanonicalSource, HaloX0, HaloY0, HaloX1 - HaloX0, HaloY1 - HaloY0);
                bHaveHaloCrop = HaloCrop.Tiles.Num() == (HaloX1 - HaloX0) * (HaloY1 - HaloY0);
            }
            AnastasisTerrainForge::FMesh ForgeMesh;
            const bool bForged = CVarTerrainForge.GetValueOnGameThread() != 0
                && AnastasisTerrainForge::Apply(Crop, Geometry, ForgeMesh, bHaveHaloCrop ? &HaloCrop : nullptr);
            if (!bForged)
            {
                AnastasisTerrainForge::ClearActive();
            }
            else
            {
                // TERRAIN_FORGE a remplace la geometrie entiere, canaux de rive compris :
                // il rebatit la nappe a la resolution fine et n'a aucune raison de
                // connaitre SHORELINE_FORGE_001. On les remplit donc ici, sur le maillage
                // REELLEMENT rendu -- ce qui vaut mieux que l'ancien : la marge suit
                // desormais le relief tessele, pas le pas de tuile de 100 uu.
                // HYDRO_NETWORK_001 : le reseau de drainage remplace les tranchees au niveau de
                // la mer par des rivieres qui descendent. Mode 2 seulement : la tranche scellee
                // du mode 1 reste bit a bit celle de WORLD_SLICE_006.
                AnastasisDrainage::FNetwork Drainage;
                if (SurfaceMode == 2 && AnastasisDrainage::IsEnabled())
                {
                    AnastasisDrainage::FParams DrainageParams;
                    DrainageParams.bWaterLook = CVarWaterLook.GetValueOnGameThread() != 0;
                    if (ForgeMesh.bBasinFound || ForgeMesh.bHumanGeography) DrainageParams.Protected.Add(FVector2D(ForgeMesh.BasinX, ForgeMesh.BasinY));
                    if (ForgeMesh.bLandmarkFound) DrainageParams.Protected.Add(FVector2D(ForgeMesh.LandmarkX, ForgeMesh.LandmarkY));
                    if (AnastasisDrainage::Apply(Crop, ForgeMesh, Drainage, DrainageParams))
                    {
                        Geometry = ForgeMesh.Geometry;
                        AnastasisTerrainForge::SetActive(ForgeMesh);
                        const AnastasisDrainage::FCheck Check = AnastasisDrainage::Check(Drainage, ForgeMesh);
                        UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_DRAINAGE enabled=1 %s"), *AnastasisDrainage::Describe(Drainage));
                        UE_LOG(LogAnastasis_UnrealV2, Display,
                            TEXT("ANASTASIS_DRAINAGE check uphill=%d narrowing=%d confluence_narrower=%d dangling_mouths=%d isolated_water=%d lakes_without_role=%d bank_containment=%.3f"),
                            Check.UphillSteps, Check.NarrowingSteps, Check.ConfluenceNarrower, Check.DanglingMouths,
                            Check.IsolatedWaterBodies, Check.LakesWithoutRole, Check.BankContainment);
                        for (int32 RiverIndex = 0; RiverIndex < Drainage.Rivers.Num(); ++RiverIndex)
                        {
                            const AnastasisDrainage::FRiver& River = Drainage.Rivers[RiverIndex];
                            const auto& A = River.Points[0];
                            const auto& B = River.Points.Last();
                            UE_LOG(LogAnastasis_UnrealV2, Display,
                                TEXT("ANASTASIS_DRAINAGE river=%d order=%d mouth=%s parent=%d source_lake=%d mouth_lake=%d authored=%d length_m=%.0f width_m=%.1f->%.1f depth_m=%.2f->%.2f velocity_ms=%.2f->%.2f water_z=%.0f->%.0f"),
                                RiverIndex, River.Order,
                                River.Mouth == AnastasisDrainage::EMouth::River ? TEXT("river") : River.Mouth == AnastasisDrainage::EMouth::Lake ? TEXT("lake") : TEXT("border"),
                                River.Parent, River.SourceLake, River.MouthLake, River.bAuthored ? 1 : 0, River.LengthM,
                                A.Width / 100.0, B.Width / 100.0, A.Depth / 100.0, B.Depth / 100.0, A.Velocity, B.Velocity,
                                A.Location.Z, B.Location.Z);
                        }
                        for (int32 LakeIndex = 0; LakeIndex < Drainage.Lakes.Num(); ++LakeIndex)
                        {
                            const AnastasisDrainage::FLake& Lake = Drainage.Lakes[LakeIndex];
                            UE_LOG(LogAnastasis_UnrealV2, Display,
                                TEXT("ANASTASIS_DRAINAGE lake=%d border=%d surface_z=%.0f cells=%d inflows=%d outflows=%d at=(%.0f,%.0f)"),
                                LakeIndex, Lake.bBorder ? 1 : 0, Lake.SurfaceZ, Lake.Cells, Lake.Inflows, Lake.Outflows, Lake.Centroid.X, Lake.Centroid.Y);
                        }
                    }
                }
                else
                {
                    UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_DRAINAGE enabled=0"));
                }
                AnastasisDrainage::SetActive(Drainage);
                AnastasisDrainage::DumpIfRequested(Drainage);
                AnastasisDrainage::DrawDebug(GetWorld(), Drainage, AnastasisDrainage::DebugMode());
                AnastasisTerrainSurface::FillShorelineChannels(Crop, Geometry);
                ForgeBasin = FVector(ForgeMesh.BasinX, ForgeMesh.BasinY, ForgeMesh.BasinZ);
                ForgeLandmark = FVector(ForgeMesh.LandmarkX, ForgeMesh.LandmarkY, ForgeMesh.LandmarkZ);
                UE_LOG(LogAnastasis_UnrealV2, Display,
                    TEXT("ANASTASIS_TERRAIN_FORGE subdiv=%d fine=%dx%d vertices=%d triangles=%d z=[%.0f,%.0f] basin=(%.0f,%.0f,%.0f) landmark=(%.0f,%.0f,%.0f)"),
                    ForgeMesh.Subdiv, ForgeMesh.FineW, ForgeMesh.FineH,
                    ForgeMesh.Geometry.Vertices.Num(), ForgeMesh.Geometry.Triangles.Num() / 3,
                    ForgeMesh.MinZ, ForgeMesh.MaxZ,
                    ForgeMesh.BasinX, ForgeMesh.BasinY, ForgeMesh.BasinZ,
                    ForgeMesh.LandmarkX, ForgeMesh.LandmarkY, ForgeMesh.LandmarkZ);
            }
            UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_HUMAN_GEOGRAPHY layer=Human_Geography_V2 enabled=%d spatial_scale=%.1f extent_m=%.1fx%.1f simulation_unchanged=1"),
                ForgeMesh.bHumanGeography ? 1 : 0, Crop.SpatialScale, (Crop.W-1)*AnastasisWorldView::TileWorldSize*Crop.SpatialScale/100.0, (Crop.H-1)*AnastasisWorldView::TileWorldSize*Crop.SpatialScale/100.0);
            // Section 0 : relief. La couleur de sommet porte la TEINTE semantique du sol ;
            // depuis GROUND_SURFACE_001 elle ne porte plus seule toute la semantique --
            // les familles de surface et l'humidite passent par UV0/UV1. Le Forge ayant
            // pu remplacer Geometry par un maillage tessele, ces canaux doivent avoir
            // suivi la subdivision : c'est AnastasisTerrainForge::Apply qui s'en charge.
            // bCreateCollision=true : c'est ce qui empeche le pawn de tomber a travers.
            // UV0/UV1 portent la morphologie lue par le materiau de sol (cf. FGeometry).
            // La surcharge a quatre canaux est la seule qui les accepte ; UV2/UV3 restent
            // vides parce que rien d'honnete ne reste a y mettre.
            // RIVERBANK_LIFE_001 : bandes de rive peintes dans la couleur de sommet AVANT la
            // section de sol -- la teinte sous l'herbe la relit ensuite depuis la section.
            // Seulement si la grille du drainage actif est bien celle de ce sol.
            {
                const AnastasisDrainage::FNetwork& BankNetwork = AnastasisDrainage::GetActive();
                if (CVarRiverbank.GetValueOnGameThread() != 0 && BankNetwork.GridW > 0
                    && Geometry.Vertices.Num() == BankNetwork.GridW * BankNetwork.GridH)
                {
                    AnastasisRiverbank::FSpeedField Speed;
                    AnastasisRiverbank::BuildSpeedField(BankNetwork, Speed);
                    const AnastasisRiverbank::FPaintResult Paint = AnastasisRiverbank::PaintBanks(Speed, AnastasisRiverbank::FSettings(), Geometry);
                    UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_RIVERBANK_PAINT enabled=1 mud_vertices=%d gravel_vertices=%d"),
                        Paint.MudVertices, Paint.GravelVertices);
                }
                else
                {
                    UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_RIVERBANK_PAINT enabled=0"));
                }
            }
            ExperimentalSurface->CreateMeshSection_LinearColor(0, Geometry.Vertices, Geometry.Triangles,
                Geometry.Normals, Geometry.UV0, Geometry.UV1, TArray<FVector2D>{}, TArray<FVector2D>{},
                Geometry.Colors, TArray<FProcMeshTangent>{}, true);
            // Section 1 : nappe d'eau plate au niveau de la mer, encastree dans le relief.
            ExperimentalSurface->ClearMeshSection(1);
            ExperimentalSurface->ClearMeshSection(2);
            // WATER_LOOK_001 : seulement si le drainage a tourne -- ses rubans et sa liste de
            // triangles sans rivieres en dependent.
            const AnastasisDrainage::FNetwork& ActiveDrainage = AnastasisDrainage::GetActive();
            const bool bWaterLook = CVarWaterLook.GetValueOnGameThread() != 0 && ActiveDrainage.GridW > 0;
            const TArray<int32>& WaterTriangles = bWaterLook ? ActiveDrainage.LakeWaterTriangles : Geometry.WaterTriangles;
            bWaterSurfaceBuilt = WaterTriangles.Num() > 0;
            const bool bShoreline = CVarShoreline.GetValueOnGameThread() != 0;
            if (bWaterSurfaceBuilt)
            {
                TArray<FLinearColor> WaterColors;
                WaterColors.Init(FLinearColor(0.043f, 0.176f, 0.290f, 1.0f), Geometry.WaterVertices.Num());
                // SHORELINE_FORGE_001 : UV0=(Depth,Flatness), UV1=(Flow,0). La couleur de
                // sommet reste celle d'avant -- c'est le repli, et c'est ce que rend le
                // mode 0. La geometrie est identique dans les deux cas : seuls les canaux
                // et le materiau changent, sinon l'A/B ne prouverait rien.
                ExperimentalSurface->CreateMeshSection_LinearColor(1, Geometry.WaterVertices, WaterTriangles,
                    Geometry.WaterNormals,
                    bShoreline ? Geometry.WaterUV0 : TArray<FVector2D>{},
                    bShoreline ? Geometry.WaterUV1 : TArray<FVector2D>{},
                    TArray<FVector2D>{}, TArray<FVector2D>{},
                    WaterColors, TArray<FProcMeshTangent>{}, false);
            }
            // Section 0 = le sol, section 1 = la nappe d'eau. DEUX missions ont conclu
            // separement qu'elles ne peuvent pas partager un materiau : GROUND_SURFACE_001
            // parce que le sol n'a pas a lire un drapeau d'eau, SHORELINE_FORGE_001 parce
            // que le bord d'eau doit etre translucide et gradue. Chacune resout sa section.
            //
            // Repli de rive : le materiau de tranche HISTORIQUE, pas le materiau de sol --
            // anastasis.Terrain.Shoreline 0 doit rendre l'eau comme avant, pas la peindre
            // en terre.
            UMaterialInterface* SurfaceMaterial = ResolveGroundMaterial();
            UMaterialInterface* ShoreMaterial = bWaterLook ? ResolveWaterLookMaterial()
                : bShoreline ? ResolveWaterMaterial() : ResolveSliceMaterial();
            // Section 2 : rubans d'eau des rivieres, meme materiau que la nappe.
            if (bWaterLook)
            {
                AnastasisDrainage::FWaterRibbons Ribbons;
                AnastasisDrainage::BuildRiverRibbons(ActiveDrainage, Ribbons);
                if (Ribbons.Triangles.Num() > 0)
                {
                    ExperimentalSurface->CreateMeshSection_LinearColor(2, Ribbons.Vertices, Ribbons.Triangles, Ribbons.Normals,
                        Ribbons.UV0, Ribbons.UV1, Ribbons.UV2, Ribbons.UV3,
                        Ribbons.Colors, TArray<FProcMeshTangent>{}, false);
                    if (ShoreMaterial) ExperimentalSurface->SetMaterial(2, ShoreMaterial);
                }
                UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_WATER_LOOK enabled=1 material=%s ribbons=%d ribbon_triangles=%d still_water_triangles=%d"),
                    ShoreMaterial ? *ShoreMaterial->GetName() : TEXT("none"), ActiveDrainage.Rivers.Num(),
                    Ribbons.Triangles.Num() / 3, WaterTriangles.Num() / 3);
            }
            else
            {
                UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_WATER_LOOK enabled=0"));
            }
            if (SurfaceMaterial)
            {
                ExperimentalSurface->SetMaterial(0, SurfaceMaterial);
            }
            if (ShoreMaterial)
            {
                ExperimentalSurface->SetMaterial(1, ShoreMaterial);
            }
            ExperimentalSurface->SetVisibility(true);
            // HORIZON_RING_001 : seulement quand Crop EST le monde -- ses bords sont alors
            // les vrais bords, et le maillage forge porte le pourtour que l'anneau reprend.
            const bool bWholeWorld = Crop.OriginX == 0 && Crop.OriginY == 0
                && Crop.W == Crop.SourceW && Crop.H == Crop.SourceH;
            if (HorizonSurface && bForged && SurfaceMode == 2 && bWholeWorld
                && CVarTerrainHorizon.GetValueOnGameThread() != 0)
            {
                AnastasisTerrainHorizon::FRing Ring;
                // Geometry, pas ForgeMesh.Geometry : c'est elle qui porte les canaux de rive
                // (FillShorelineChannels ci-dessus), que l'anneau 0 doit reprendre.
                if (AnastasisTerrainHorizon::Build(ForgeMesh, Crop.Seed, Ring, &Geometry))
                {
                    const auto& RG = Ring.Geometry;
                    HorizonSurface->CreateMeshSection_LinearColor(0, RG.Vertices, RG.Triangles,
                        RG.Normals, RG.UV0, RG.UV1, TArray<FVector2D>{}, TArray<FVector2D>{},
                        RG.Colors, TArray<FProcMeshTangent>{}, false);
                    if (SurfaceMaterial)
                    {
                        HorizonSurface->SetMaterial(0, SurfaceMaterial);
                    }
                    // Section 1 : la rivière qui sort de la carte, meme nappe et meme
                    // materiau que la section 1 de la carte, qu'elle prolonge.
                    if (RG.WaterTriangles.Num() > 0)
                    {
                        TArray<FLinearColor> RingWaterColors;
                        RingWaterColors.Init(FLinearColor(0.043f, 0.176f, 0.290f, 1.0f), RG.WaterVertices.Num());
                        HorizonSurface->CreateMeshSection_LinearColor(1, RG.WaterVertices, RG.WaterTriangles, RG.WaterNormals,
                            bShoreline ? RG.WaterUV0 : TArray<FVector2D>{},
                            bShoreline ? RG.WaterUV1 : TArray<FVector2D>{},
                            TArray<FVector2D>{}, TArray<FVector2D>{},
                            RingWaterColors, TArray<FProcMeshTangent>{}, false);
                        if (ShoreMaterial)
                        {
                            HorizonSurface->SetMaterial(1, ShoreMaterial);
                        }
                    }
                    HorizonSurface->SetVisibility(true);
                    UE_LOG(LogAnastasis_UnrealV2, Display,
                        TEXT("ANASTASIS_TERRAIN_HORIZON enabled=1 perimeter=%d rings=%d vertices=%d triangles=%d water_triangles=%d edge_water=%d outer_m=%.0f skirt_m=%.0f z=[%.0f,%.0f]"),
                        Ring.Perimeter, Ring.Rings, RG.Vertices.Num(), RG.Triangles.Num() / 3, RG.WaterTriangles.Num() / 3,
                        Ring.EdgeWater, Ring.Distances[Ring.Rings - 2] / 100.0, Ring.Distances.Last() / 100.0, Ring.MinZ, Ring.MaxZ);
                }
            }
            else
            {
                UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_TERRAIN_HORIZON enabled=0 forged=%d whole_world=%d"),
                    bForged ? 1 : 0, bWholeWorld ? 1 : 0);
            }
            for (auto& Mesh : TerrainMeshes) if (Mesh) { Mesh->SetVisibility(false); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
            BuiltSurfaceCrop = Crop;
            bSurfaceBuilt = true;
            // Emprise reelle = ce qui est reellement rendu, pas Plan : en mode 1 la tranche
            // canonique meme si Plan couvre le monde, en mode 2 l emprise incarnee entiere.
            ActiveFootprintBounds = AnastasisWorldView::SnapshotBounds(Crop);
            if (bForged)
            {
                FBox ForgedBounds(ForceInit);
                for (const FVector& V : Geometry.Vertices)
                {
                    ForgedBounds += V;
                }
                if (ForgedBounds.IsValid)
                {
                    ActiveFootprintBounds.Min.Z = ForgedBounds.Min.Z;
                    ActiveFootprintBounds.Max.Z = ForgedBounds.Max.Z;
                }
            }
            // Emprise reportee telle qu'elle est batie : en mode 1 cette ligne imprime
            // exactement la chaine scellee (source=96x96 crop=(0,0) 32x32 tiles=1024).
            UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_TERRAIN source=%dx%d crop=(%d,%d) %dx%d tiles=%d vertices=%d triangles=%d water_triangles=%d material=%s water_material=%s boundary=tile_centers legacy_visible=0"),
                Crop.SourceW, Crop.SourceH, Crop.OriginX, Crop.OriginY, Crop.W, Crop.H, Crop.Tiles.Num(),
                Geometry.Vertices.Num(), Geometry.Triangles.Num()/3, Geometry.WaterTriangles.Num()/3,
            SurfaceMaterial ? *SurfaceMaterial->GetName() : TEXT("none"),
            ShoreMaterial ? *ShoreMaterial->GetName() : TEXT("none"));
            // Ligne SEPAREE, deliberement : la ligne ANASTASIS_TERRAIN ci-dessus imprime en
            // mode 1 la chaine scellee WORLD_SLICE_006 caractere pour caractere. Une mission
            // de rive n'a pas a la reecrire pour se rapporter elle-meme.
            UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_SHORELINE enabled=%d water_material=%s channels=%d water_vertices=%d span_uu=%.0f forged=%d"),
                bShoreline ? 1 : 0,
                ShoreMaterial ? *ShoreMaterial->GetName() : TEXT("none"),
                bShoreline ? Geometry.WaterUV0.Num() : 0,
                Geometry.WaterVertices.Num(),
                AnastasisTerrainSurface::ShoreDepthSpan,
                bForged ? 1 : 0);
        }
        else
        {
            bWaterSurfaceBuilt = false;
            ActiveFootprintBounds = AnastasisWorldView::PlanBounds(Plan);
            AnastasisTerrainForge::ClearActive();
            UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_TERRAIN rejected crop; legacy DEBUG retained"));
        }
    }
    else
    {
        bWaterSurfaceBuilt = false;
        ActiveFootprintBounds = AnastasisWorldView::PlanBounds(Plan);
        AnastasisTerrainForge::ClearActive();
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
	// Le materiau de tranche historique. Il n'habille plus rien par defaut depuis
	// GROUND_SURFACE_001 et SHORELINE_FORGE_001 : il est le REPLI des deux, celui qui
	// rend le monde comme avant quand un asset manque ou qu'une CVar est a 0.
	if (!SliceMaterial)
	{
		SliceMaterial = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Anastasis/Materials/M_AnastasisSlice.M_AnastasisSlice"));
	}
	return SliceMaterial ? SliceMaterial.Get() : BaseShapeMaterial.Get();
}

UMaterialInterface* AAnastasisWorldEmbodiment::ResolveGroundMaterial()
{
	// L'INSTANCE d'abord, pas le materiau maitre : ce sont ses parametres qui portent
	// les valeurs artistiques du sol (teintes, echelles de variation, rugosites). Une
	// retouche de sol se fait donc dans l'instance, sans recompiler ce fichier.
	if (CVarGroundMaterial.GetValueOnGameThread() != 0)
	{
		if (!GroundMaterial)
		{
			GroundMaterial = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Game/Anastasis/Materials/MI_AnastasisGround.MI_AnastasisGround"));
		}
		if (GroundMaterial)
		{
			return GroundMaterial.Get();
		}
		// Absente : on retombe sur la tranche historique plutot que sur rien. Le nom du
		// materiau reellement pose part dans ANASTASIS_TERRAIN, donc le repli se voit.
	}
	if (!SliceMaterial)
	{
		SliceMaterial = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Anastasis/Materials/M_AnastasisSlice.M_AnastasisSlice"));
	}
	return SliceMaterial ? SliceMaterial.Get() : BaseShapeMaterial.Get();
}

UMaterialInterface* AAnastasisWorldEmbodiment::ResolveWaterLookMaterial()
{
	if (!WaterLookMaterial)
	{
		WaterLookMaterial = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Anastasis/Materials/M_AnastasisWater.M_AnastasisWater"));
	}
	return WaterLookMaterial ? WaterLookMaterial.Get() : ResolveWaterMaterial();
}

UMaterialInterface* AAnastasisWorldEmbodiment::ResolveWaterMaterial()
{
	if (!WaterMaterial)
	{
		WaterMaterial = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Anastasis/Materials/M_AnastasisShoreWater.M_AnastasisShoreWater"));
	}
	// Repli explicite et non silencieux : sans l'asset de rive, la nappe reprend le
	// materiau de tranche et le monde rend comme avant. Le log ANASTASIS_SHORELINE
	// imprime le nom reellement applique, donc un repli se lit dans la preuve.
	return WaterMaterial ? WaterMaterial.Get() : ResolveSliceMaterial();
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
