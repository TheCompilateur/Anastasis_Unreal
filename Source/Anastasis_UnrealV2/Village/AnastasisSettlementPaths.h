#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class UHierarchicalInstancedStaticMeshComponent;
class UWorld;
namespace AnastasisVillage { class FVillage; }
namespace AnastasisWorld { struct FWorld; }

/**
 * SETTLEMENT_MORPHOGENESIS_001 -- les sentiers nes du passage, rendus.
 *
 * Source UNIQUE : les cases que la simulation a changees en `Road` parce qu'on y a marche
 * (`FVillage::GetRoads`, ecart n°42). Rien n'est trace a la main, rien n'est tire au hasard.
 * Une case de 20 m n'est pas un chemin de 20 m : le sentier est une bande de terre battue de
 * 1,2 a 1,7 m qui relie les centres des cases foulees voisines, et chaque porte de parcelle a la
 * case-sentier qui la dessert. L'herbe sous la bande est ecartee (echelle zero, restauree a Clear).
 *
 * Cadence : rebatie seulement quand l'ensemble des sentiers change (un nouveau sentier par nuit au
 * plus), jamais par image. Purement visuel : rien n'est ecrit dans la simulation.
 */
class FAnastasisSettlementPaths
{
public:
	/** Une porte de parcelle a desservir : le seuil (monde, cm) et la case d'acces de la simulation. */
	struct FDoorLink
	{
		FVector Entry = FVector::ZeroVector;
		int32 AccessTileX = 0;
		int32 AccessTileY = 0;
	};

	/** Rebatit si les sentiers ont change. Rend le nombre de segments poses (0 si rien n'a change). */
	int32 Sync(const AnastasisVillage::FVillage& Village, const AnastasisWorld::FWorld& World, UWorld* PresentationWorld,
		const TArray<FDoorLink>& Doors);

	/** Retire la bande et rend l'herbe. */
	void Clear();

	int32 NumSegments() const { return Segments; }
	int32 NumRoadTiles() const { return LastRoadCount; }
	int32 NumHiddenGrass() const { return Hidden.Num(); }
	AActor* GetActor() const { return Actor.Get(); }

private:
	struct FHiddenInstance
	{
		TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;
		int32 Index = INDEX_NONE;
		FTransform Original;
	};

	TWeakObjectPtr<AActor> Actor;
	TArray<FHiddenInstance> Hidden;
	uint32 LastSignature = 0;
	int32 LastRoadCount = -1;
	int32 Segments = 0;
};
