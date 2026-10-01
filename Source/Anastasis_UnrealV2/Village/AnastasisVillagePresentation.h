#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AAnastasisVillageBuilding;
class AAnastasisVillagerVisual;
class UAnastasisPresentationRegistry;
class UAnastasisVillageInteractionSubsystem;
class UWorld;
namespace AnastasisVillage { class FVillage; }
namespace AnastasisWorld { struct FWorld; }

/**
 * Le lien entre un batiment de SIMULATION et sa representation Unreal.
 *
 * Sens unique : la simulation (`AnastasisVillage::FVillage`) possede les
 * batiments, leur identite (`building-N`) et leur etat. Ce reconciliateur ne
 * fait que refleter : un enregistrement present -> un acteur dont `SimId` est
 * l'identifiant ; un enregistrement retire -> l'acteur detruit, et avec lui
 * son Smart Object. Il ne cree jamais un batiment et n'ecrit jamais dans la
 * simulation — l'acteur est remplacable, l'enregistrement ne l'est pas.
 *
 * Le spawn passe par `UAnastasisVillageInteractionSubsystem::SpawnBuilding`,
 * le chemin de `main` : pas de seconde facon de poser un batiment.
 */
class FAnastasisVillagePresentation
{
public:
	/** Simulation (tuiles, continu) -> Unreal (cm). Z = surface rendue si PresentationWorld est fourni, altitude semantique sinon. */
	static FVector SimToUnreal(const AnastasisWorld::FWorld& World, double SimX, double SimY, UWorld* PresentationWorld = nullptr);

	/** Aligne les acteurs sur les enregistrements. Rend le nombre d'acteurs crees + detruits. */
	int32 Sync(const AnastasisVillage::FVillage& Village, const AnastasisWorld::FWorld& World, UAnastasisVillageInteractionSubsystem& Rooms);

	/** Detruit tous les acteurs refletes. */
	void Clear(UAnastasisVillageInteractionSubsystem* Rooms);

	AAnastasisVillageBuilding* FindActor(const FString& SimId) const;
	int32 Num() const { return Actors.Num(); }

	/**
	 * VILLAGER_PNG_001 -- une carte portrait par habitant simule, meme sens unique que les
	 * batiments : un habitant present -> une carte a ses pieds, son portrait choisi par
	 * AnastasisVillagerLooks dans le registre de presentation ; un habitant disparu -> la carte
	 * detruite ; un habitant dedans -> la carte cachee. `bEnabled` faux retire toutes les cartes.
	 * Rend le nombre de cartes creees + detruites.
	 */
	int32 SyncVillagers(
		const AnastasisVillage::FVillage& Village,
		const AnastasisWorld::FWorld& World,
		UWorld* PresentationWorld,
		const UAnastasisPresentationRegistry& Registry,
		bool bEnabled);

	AAnastasisVillagerVisual* FindVillager(const FString& NpcId) const;
	int32 NumVillagers() const { return Villagers.Num(); }

	/**
	 * Debug : puits (cylindre), seuils, habitants (sphere coloree par la soif),
	 * trait vers la cible, texte « id  but  activite  soif  -> batiment ».
	 */
	static void DrawDebug(UWorld* World, const AnastasisVillage::FVillage& Village, const AnastasisWorld::FWorld& SimWorld);

	/** Une ligne par batiment puis par habitant : l'etat, qui s'en sert, pourquoi. */
	static void LogStatus(const AnastasisVillage::FVillage& Village, double Time);

private:
	TMap<FString, TWeakObjectPtr<AAnastasisVillageBuilding>> Actors;
	TMap<FString, TWeakObjectPtr<AAnastasisVillagerVisual>> Villagers;
	bool bWarnedNoLooks = false;
};
