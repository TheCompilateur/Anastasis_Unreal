#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Village/AnastasisVillageFabric.h"
#include "AnastasisVillageFabricActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * VILLAGE_FABRIC_001 -- le tissu rendu : calades et marches, placette, murets de pierre seche, platane.
 * Geometrie construite a l'execution (aucun asset genere : le tissu se refait a chaque changement du
 * village). Transitoire, jamais sauve. L'herbe et le sous-bois poses sur la chaussee sont mis a
 * l'echelle zero et RENDUS a l'identique quand le tissu change ou disparait.
 */
UCLASS(Transient, NotPlaceable)
class AAnastasisVillageFabric : public AActor
{
	GENERATED_BODY()

public:
	AAnastasisVillageFabric();

	/**
	 * Remplace toute la geometrie par celle de `Fabric`. `Height` donne le sol sous un point (pour
	 * coller les bords de calade et la placette au relief). Rend le nombre d'instances defrichees.
	 */
	int32 Apply(const AnastasisVillageFabric::FFabric& Fabric, AnastasisVillageFabric::FHeightFn Height,
		UMaterialInterface* PavingMaterial, UMaterialInterface* WallMaterial, UStaticMesh* PlazaTree, bool bClear);

	/** Rend a l'herbe ce qui lui a ete pris. Rend le nombre d'instances restaurees. */
	int32 RestoreCleared();

	int32 GetClearedCount() const { return Cleared.Num(); }
	int32 GetStoneCount() const { return Stones; }
	int32 GetTriangleCount() const { return Triangles; }

	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	int32 ClearUnder(const AnastasisVillageFabric::FFabric& Fabric);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Paving;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Walls;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Tree;

	struct FClearedInstance
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Component;
		int32 Index = INDEX_NONE;
		FTransform Original;
	};
	TArray<FClearedInstance> Cleared;
	int32 Stones = 0;
	int32 Triangles = 0;
};
