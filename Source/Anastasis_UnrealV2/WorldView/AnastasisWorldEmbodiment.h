#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldView/AnastasisPresentationResolver.h"
#include "WorldView/AnastasisWorldView.h"
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

protected:
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ExperimentalSurface;
	UPROPERTY()
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TerrainMeshes[7];

	/**
	 * One HISM per (archetype, variant) actually used by the current embodiment. Built on
	 * demand from the presentation registry instead of in the constructor: the mesh set is
	 * data now, so it is not known until EmbodyCrop reads it.
	 */
	UPROPERTY()
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> DressingMeshes;

	/** "<ArchetypeId>_v<VariantIndex>" -> index into DressingMeshes. */
	TMap<FName, int32> DressingSlotByKey;

	/** The HISM for one resolved look, created and registered on first use. */
	UHierarchicalInstancedStaticMeshComponent* GetOrCreateDressingMesh(
		const AnastasisPresentation::FResolvedPresentation& Resolved);

	int32 DressingInstanceCount = 0;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseShapeMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SliceMaterial;

	/** Materiau de tranche s'il est present dans Content, sinon repli sur BaseShapeMaterial. */
	UMaterialInterface* ResolveSliceMaterial();

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
};

