#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	/** Bouton Details : surface continue coloree + nappe d'eau. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Anastasis|Debug")
	void ShowSliceSurface();

	/** Bouton Details : retour au terrain DEBUG historique (cubes HISM). */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Anastasis|Debug")
	void ShowLegacyDebug();
	FVector GetEmbodiedLocation(int32 TileIndex) const;
	bool GetInstanceWorldTransform(int32 TileIndex, FTransform& OutTransform) const;
	const AnastasisWorldView::FPlan& GetPlan() const { return Plan; }
	const AnastasisWorldView::FWorldVisualSnapshot& GetSnapshot() const { return Snapshot; }
	void LogEmbodiment() const;

protected:
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ExperimentalSurface;
	UPROPERTY()
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TerrainMeshes[7];

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseShapeMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SliceMaterial;

	/** Materiau de tranche s'il est present dans Content, sinon repli sur BaseShapeMaterial. */
	UMaterialInterface* ResolveSliceMaterial();

	AnastasisWorldView::FWorldVisualSnapshot Snapshot;
	AnastasisWorldView::FPlan Plan;
	TArray<int32> LocalInstanceIndex;
};

