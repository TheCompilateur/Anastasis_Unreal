#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldView/AnastasisPresentationResolver.h"
#include "WorldView/AnastasisWorldView.h"
#include "WorldView/AnastasisEcologicalDressing.h"
#include "WorldView/AnastasisPlaces.h"
#include "AnastasisWorldEmbodiment.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UProceduralMeshComponent;

/**
 * DEBUG / METROLOGY OWNER.
 * Diagnostic HISMC cubes. Not the player renderer.
 * Consumes FWorldVisualSnapshot via WorldView. Does not own worldgen.
 */
UCLASS()
class AAnastasisWorldEmbodiment : public AActor
{
	GENERATED_BODY()

public:
	AAnastasisWorldEmbodiment();

	virtual void BeginPlay() override;

    /** Forest grammar only; mesh references stay in the presentation registry. */
    UPROPERTY(EditAnywhere, Category="Anastasis|Ecology")
    FAnastasisForestDressingSettings ForestDressing;

    /** WORLD_DRESSING_01 : lieux composes (AnastasisPlaces). Coupable par acteur, comme la foret. */
    UPROPERTY(EditAnywhere, Category="Anastasis|Places")
    bool bComposePlaces = true;

#if WITH_EDITOR
	/**
	 * Editor worlds only: rebuild the embodiment when the level loads or the actor changes, so
	 * the map shows simulation truth in the viewport without entering PIE. Game worlds are
	 * driven by BeginPlay -- running both would embody twice on every PIE start.
	 */
	virtual void OnConstruction(const FTransform& Transform) override;
#endif

	/** EmbodyCrop driven by the anastasis.WorldView.* console variables. One source for BeginPlay and OnConstruction. */
	bool EmbodyFromConsoleVariables();

	bool Embody(uint32 Seed, int32 Width, int32 Height);
	bool EmbodyCrop(uint32 Seed, int32 OriginX, int32 OriginY, int32 Width, int32 Height);

	/** Observation depuis un script editeur : incarne le monde canonique complet. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Anastasis|Debug")
	bool EmbodyCanonical(int32 Seed = 12345);

	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug")
	int32 GetInstanceCount() const;

	/** Discrete presentation instances (trees, ruins) placed by AnastasisPresentationResolver on top of the ground representation. Independent of GetInstanceCount(), which is legacy DEBUG cubes only. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Debug")
	int32 GetDressingInstanceCount() const { return DressingInstanceCount; }

	/** Bouton Details : surface continue coloree + nappe d'eau. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Anastasis|Debug")
	void ShowSliceSurface();

	/** Bouton Details : retour au terrain DEBUG historique (cubes HISM). */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Anastasis|Debug")
	void ShowLegacyDebug();
	FVector GetEmbodiedLocation(int32 TileIndex) const;
	bool GetInstanceWorldTransform(int32 TileIndex, FTransform& OutTransform) const;

	/** Position sure au-dessus du centre du terrain incarne, pour un reset de position debug. */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Debug")
	FVector GetSafeRespawnLocation() const;

	const AnastasisWorldView::FPlan& GetPlan() const { return Plan; }
	const AnastasisWorldView::FWorldVisualSnapshot& GetSnapshot() const { return Snapshot; }
	/** True once EmbodyCrop has built a flat sea-level water section (ANASTASIS_TERRAIN surface mode). */
	bool HasWaterSurface() const { return bWaterSurfaceBuilt; }
	/** The actually rendered/collidable footprint (see ActiveFootprintBounds below) — not Plan, which can be larger. */
	const FBox& GetActiveFootprintBounds() const { return ActiveFootprintBounds; }
	void LogEmbodiment() const;

	UFUNCTION(BlueprintPure, Category = "Anastasis|Terrain")
	FVector GetTerrainForgeBasin() const { return ForgeBasin; }

	UFUNCTION(BlueprintPure, Category = "Anastasis|Terrain")
	FVector GetTerrainForgeLandmark() const { return ForgeLandmark; }

	/** WORLD_DRESSING_01 : un "id|x|y|z|rayon" par lieu compose, pour viser les cameras de preuve. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Places")
	TArray<FString> GetPlaceReport() const { return PlaceReport; }

	/**
	 * GROUND_COVER_001, metrologie : temps game thread, render thread et GPU de la derniere frame
	 * (ms), ceux de stat unit. La duree de frame de l'editeur mesure surtout la charge des autres
	 * processus de la machine ; ces trois-la mesurent ce que la scene coute.
	 */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Metrology")
	FVector GetFrameTimingsMs() const;

protected:
	/** Lieux composes (AnastasisPlaces) : un HISM par piece, transitoires, rebatis a chaque incarnation. */
	UPROPERTY()
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> PlaceMeshes;
	TArray<FString> PlaceReport;
	/** GROUND_COVER_001 : un HISM par famille d'herbe, transitoires, vides puis reremplis a chaque incarnation. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GroundCoverMeshes;
	/** Strate herbacee des espaces ouverts, posee apres les arbres (Canopy = couronnes posees : X, Y, rayon). */
	void PlaceGroundCover(const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource,
		const AnastasisPlaces::FPlan& Places, const TArray<FVector>& Canopy, bool bEnabled);
	/**
	 * L'incarnation d'ouverture peut tourner avant que les decals du sentier soient enregistres.
	 * Un tick plus tard, s'ils sont la, on refait l'herbe. Sans tag, aucun second passage.
	 */
	void ScheduleOccupationTread();
	bool bApplyingOccupationTread = false;
	bool bOccupationTreadScheduled = false;
	/** FOREST_TERRAIN_P3 : maquis, ronces et rochers ; un HISM par mesh, transitoires, vides puis reremplis. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> UnderstoryMeshes;
	/** Strate arbustive et rochers, apres les arbres et avant l'herbe ; ajoute leurs emprises a Canopy. */
	void PlaceUnderstory(const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource, TArray<FVector>& Canopy, bool bEnabled);
	/** MICRO_ECOLOGY_001 : poches de berge, prairie, lisiere et sous-bois, apres l'herbe. */
	void PlaceMicroEcology(const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource,
		const AnastasisPlaces::FPlan& Places, const TArray<FVector>& Canopy, bool bEnabled);
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> MicroEcologyMeshes;
	/** RIVERBANK_LIFE_001 : un HISM par famille de rive et tuile, vides puis reremplis a chaque incarnation. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> RiverbankMeshes;
	/** Roseaux, galets et blocs, poses sur le sol et l'eau rendus (AnastasisRiverbank). */
	void PlaceRiverbank(const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource, bool bEnabled);
	/** Compose les lieux sur le sol rendu ; false = lieux coupes (plan vide). */
	bool ComposePlaces(const AnastasisWorldView::FWorldVisualSnapshot* SurfaceCrop,
		const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource, AnastasisPlaces::FInputs& In, AnastasisPlaces::FPlan& Places);
	void EmbodyPlaces(const AnastasisPlaces::FInputs& In, const AnastasisPlaces::FPlan& Places, bool bEnabled,
		const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource, int32 SupersededRuins);

	// Components are RF_Transient: persisting their references writes null into the map.
	// Keep constructor-created subobjects on reload; rebuild generated dressing instead.
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> ExperimentalSurface;
	/** HORIZON_RING_001 : terrain lointain raccorde au bord forge, sans collision. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> HorizonSurface;
	UPROPERTY(Transient)
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TerrainMeshes[7];

	/**
	 * One HISM per (archetype, variant) actually used by the current embodiment. Built on
	 * demand from the presentation registry instead of in the constructor: the mesh set is
	 * data now, so it is not known until EmbodyCrop reads it.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> DressingMeshes;

	/** "<ArchetypeId>_v<VariantIndex>" -> index into DressingMeshes. */
	TMap<FName, int32> DressingSlotByKey;

	/** The HISM for one resolved look, created and registered on first use. */
	/**
	 * Pose les instances de dressing sur le sol REELLEMENT rendu.
	 *
	 * SurfaceCrop non nul = la surface continue est batie sur cette emprise : chaque
	 * instance lit la hauteur interpolee sous sa position jittee, et une tuile hors de
	 * l'emprise rendue ne recoit rien -- elle n'a pas de sol.
	 * SurfaceCrop nul = chemin DEBUG : le sol est le dessus de la dalle de la tuile.
	 *
	 * Appelee APRES la decision de terrain, pas avant : on ne peut pas poser un objet
	 * sur un sol dont on ignore encore la forme.
	 */
	void PlaceDressing(uint32 Seed, const AnastasisWorldView::FWorldVisualSnapshot* SurfaceCrop,
        const AnastasisWorldView::FWorldVisualSnapshot& CanonicalSource);

	UHierarchicalInstancedStaticMeshComponent* GetOrCreateDressingMesh(
		const AnastasisPresentation::FResolvedPresentation& Resolved);

	int32 DressingInstanceCount = 0;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseShapeMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SliceMaterial;

	/** Instance de sol morphologique (MI_AnastasisGround) : porte les valeurs artistiques. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> GroundMaterial;

	/**
	 * Materiau pose sur la surface. Ordre : instance de sol (anastasis.Terrain.GroundMaterial 1),
	 * puis materiau de tranche historique, puis BaseShapeMaterial. Le nom retenu est journalise
	 * dans ANASTASIS_TERRAIN : un repli silencieux n'existe pas.
	 */
	UMaterialInterface* ResolveGroundMaterial();

	/** Materiau de tranche historique : le repli commun du sol et de la rive. */
	UMaterialInterface* ResolveSliceMaterial();

	UPROPERTY()
	TObjectPtr<UMaterialInterface> WaterMaterial;

	/**
	 * Materiau de la NAPPE D'EAU (section 1) -- SHORELINE_FORGE_001.
	 *
	 * Section 0 (le sol) et section 1 (l'eau) ne portent plus le meme materiau : le
	 * bord d'eau a besoin d'etre translucide et gradue, le sol non. Repli sur le
	 * materiau de tranche si l'asset de rive est absent : un monde sans M_AnastasisShoreWater
	 * rend alors exactement comme avant, ce qui est aussi le chemin A de la preuve A/B.
	 */
	UMaterialInterface* ResolveWaterMaterial();

	UPROPERTY()
	TObjectPtr<UMaterialInterface> WaterLookMaterial;

	/**
	 * WATER_LOOK_001 -- M_AnastasisWater (Single Layer Water). Repli sur le materiau de rive
	 * si l'asset manque : l'eau rend alors comme avant, et le log le dit.
	 */
	UMaterialInterface* ResolveWaterLookMaterial();

	AnastasisWorldView::FWorldVisualSnapshot Snapshot;
	AnastasisWorldView::FPlan Plan;
	TArray<int32> LocalInstanceIndex;
	bool bWaterSurfaceBuilt = false;

	/**
	 * Emprise reellement visible/solide de l'embodiment courant. En mode
	 * surface (anastasis.Terrain.Surface=1), c'est le crop 32x32 fixe, PAS
	 * Plan (qui peut couvrir tout le monde 96x96 demande par BeginPlay) :
	 * les deux divergent des que Width/Height > 32, sinon GetSafeRespawnLocation
	 * vise un point hors de tout ce qui est visible ou solide.
	 */
	FBox ActiveFootprintBounds = FBox(ForceInit);
	FVector ForgeBasin = FVector::ZeroVector;
	FVector ForgeLandmark = FVector::ZeroVector;
};
