#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnastasisFieldGrainVisual.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
namespace AnastasisVillage { class FVillage; }
namespace AnastasisWorld { struct FWorld; }

/** Read-only crop witness. FVillage owns each plot's crop and amount; this actor owns only HISM instances. */
UCLASS()
class AAnastasisFieldGrainVisual : public AActor
{
	GENERATED_BODY()
public:
	AAnastasisFieldGrainVisual();
	virtual void Tick(float DeltaSeconds) override;
	void Reset();

	UFUNCTION(BlueprintPure, Category="Anastasis|Fields")
	int32 GetTileClumps(int32 TileX, int32 TileY) const;
	UFUNCTION(BlueprintPure, Category="Anastasis|Fields")
	FVector GetFullestField() const;
	UFUNCTION(BlueprintPure, Category="Anastasis|Fields")
	int32 GetTotalClumps() const;
	UFUNCTION(BlueprintPure, Category="Anastasis|Fields")
	int32 GetRenderedClumps() const;
	UFUNCTION(BlueprintPure, Category="Anastasis|Fields")
	int32 GetPendingChunkCount() const { return DirtyChunks.Num(); }

private:
	void Poll(const AnastasisVillage::FVillage& Village, const AnastasisWorld::FWorld& SimWorld);
	void RebuildChunk(int32 Chunk, const AnastasisWorld::FWorld& SimWorld);
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Chunks;
	TArray<int32> Densities;
	TSet<int32> DirtyChunks;
	int32 Width = 0;
	int32 Height = 0;
	int32 ChunkColumns = 0;
	float PollAccumulator = 0.0f;
	bool bMissingMeshLogged = false;
};
