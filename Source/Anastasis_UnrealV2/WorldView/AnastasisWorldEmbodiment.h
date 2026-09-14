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

#if WITH_EDITOR
	/**
	 * Editor worlds only: rebuild the embodiment when the level loads or the actor changes,
	 * so the map shows simulation truth in the viewport without entering PIE. Game worlds
	 * are driven by BeginPlay instead -- running both would embody twice on every PIE start.
	 */
	virtual void OnConstruction(const FTransform& Transform) override;
#endif

	/** EmbodyCrop driven by the anastasis.WorldView.* console variables. Single source for BeginPlay and OnConstruction. */
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

protected:
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ExperimentalSurface;
	UPROPERTY()
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TerrainMeshes[7];

	/** One HISM per AnastasisPresentation::EArchetype, indexed by static_cast<int32>(Archetype). Slot 0 (None) is unused. Literal bound (not EArchetype::Count) because UHT requires a literal array size on a UPROPERTY; see the matching static_assert in the .cpp. */
	UPROPERTY()
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> DressingMeshes[3];

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
	 * Emprise reellement visible/solide de l'embodiment courant. Les deux modes
	 * couvrent desormais tout le crop demande -- la surface n'est plus bornee au
	 * 32x32 canonique -- donc cette emprise suit Plan. Elle reste un champ distinct
	 * parce que c'est ce que GetSafeRespawnLocation vise : si un mode futur rend
	 * moins que le Plan, c'est ici que la divergence doit etre enregistree, pas
	 * dans un appelant qui supposerait Plan solide.
	 */
	FBox ActiveFootprintBounds = FBox(ForceInit);
};

