#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldView/AnastasisWorldView.h"
#include "AnastasisWorldEmbodiment.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

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

	int32 GetInstanceCount() const;
	FVector GetEmbodiedLocation(int32 TileIndex) const;
	bool GetInstanceWorldTransform(int32 TileIndex, FTransform& OutTransform) const;
	const AnastasisWorldView::FPlan& GetPlan() const { return Plan; }
	const AnastasisWorldView::FWorldVisualSnapshot& GetSnapshot() const { return Snapshot; }
	void LogEmbodiment() const;

protected:
	UPROPERTY()
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TerrainMeshes[7];

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseShapeMaterial;

	AnastasisWorldView::FWorldVisualSnapshot Snapshot;
	AnastasisWorldView::FPlan Plan;
	TArray<int32> LocalInstanceIndex;
};
