#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Village/AnastasisBuildingMetabolism.h"
#include "Village/AnastasisArchitecture.h"
#include "Village/AnastasisSettlementLedger.h"
#include "Village/AnastasisSettlementPaths.h"
#include "SmartObjectTypes.h"
#include "SmartObjectRuntime.h"

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

	/** Altitude (cm) de la surface rendue du terrain en (X, Y) Unreal ; faux hors maillage ou sans terrain. */
	static bool TraceGround(UWorld* PresentationWorld, double X, double Y, double& OutZ);

	/**
	 * ARCHITECTURE_SCALE_001 : pose un batiment a l'echelle humaine. Typologie (AnastasisArchitecture::ChooseVariant),
	 * cour terrassee a la mediane du terrain sous l'emprise (ARCH-10), puis la parcelle defrichee : les instances
	 * d'herbe, de sous-bois et d'arbres dont le pied tombe dans l'emprise passent a l'echelle zero (indices
	 * inchanges, rien n'est detruit). Rend le nombre d'instances ecartees ; -1 si l'archetype n'est pas charge.
	 */
	static int32 SettleArchitecture(AAnastasisVillageBuilding& Actor, const AnastasisVillage::FVillage& Village,
		const FString& BuildingId, UWorld* PresentationWorld, AnastasisArchitecture::EVariant Variant);

	/**
	 * SETTLEMENT_MORPHOGENESIS_001 : la biographie de chaque batiment (qui l'a fonde, pour quel foyer, ce qui
	 * lui est arrive), ecrite en observant la simulation. Elle fixe le programme (la forme) du batiment.
	 */
	const AnastasisSettlement::FLedger& GetLedger() const { return Ledger; }
	const FAnastasisSettlementPaths& GetPaths() const { return Paths; }

	/** Trace de debogage du peuplement : sentiers, passage, biographie (anastasis.Village.Debug). */
	void DrawSettlementDebug(UWorld* World, const AnastasisVillage::FVillage& Village, const AnastasisWorld::FWorld& SimWorld, int32 Day) const;

	/**
	 * Aligne les acteurs sur les enregistrements. Rend le nombre d'acteurs crees + detruits.
	 * ICEBERG_001 : `Daylight` (0 nuit, 1 plein jour : celui du ciel que le joueur voit), `Day` (jour de la simulation) et `Mode`
	 * donnent a chaque maison le foyer que la simulation lui prete (AnastasisMetabolism). Le defaut
	 * (plein jour) n'allume rien : les appelants qui ne connaissent pas le ciel gardent l'ancien rendu.
	 */
	int32 Sync(
		const AnastasisVillage::FVillage& Village,
		const AnastasisWorld::FWorld& World,
		UAnastasisVillageInteractionSubsystem& Rooms,
		double Daylight = 1.0,
		AnastasisMetabolism::EMode Mode = AnastasisMetabolism::EMode::Truth,
		int32 Day = 1);

	/** Detruit tous les acteurs refletes. */
	void Clear(UAnastasisVillageInteractionSubsystem* Rooms);
	/** Miroir des usages interieurs : claim/use a l'entree, release a la sortie. */
	void SyncInteractions(const AnastasisVillage::FVillage& Village, UAnastasisVillageInteractionSubsystem& Rooms);
	bool HasInteractionClaim(const FString& NpcId) const;

	AAnastasisVillageBuilding* FindActor(const FString& SimId) const;
	int32 Num() const { return Actors.Num(); }

	/**
	 * VILLAGER_PNG_001 -- une carte portrait par habitant simule, meme sens unique que les
	 * batiments : un habitant present -> une carte a ses pieds, son portrait choisi par
	 * AnastasisVillagerLooks dans le registre de presentation, parmi ceux de son METIER simule
	 * (l'objet peint est celui du metier) ; un metier qui change -> la carte redessinee ; un habitant disparu -> la carte
	 * detruite ; un habitant dedans -> la carte cachee. `bEnabled` faux retire toutes les cartes.
	 * Rend le nombre de cartes creees + detruites.
	 *
	 * Interpolation (point 4) : la simulation avance par pas fixes ; ralentie, un pas ne tombe que
	 * toutes les ~27 frames. La carte est posee entre la position d'avant le dernier pas et la
	 * position courante, a `StepAlpha` (fraction du pas en cours) ; `bStepped` = au moins un pas
	 * simule cette frame. Retard visuel : au plus un pas. Un saut de plus de 2 tuiles (scenario,
	 * remise a zero) est pris tel quel. Par defaut (1, vrai) : la position simulee exacte.
	 */
	int32 SyncVillagers(
		const AnastasisVillage::FVillage& Village,
		const AnastasisWorld::FWorld& World,
		UWorld* PresentationWorld,
		const UAnastasisPresentationRegistry& Registry,
		bool bEnabled,
		double StepAlpha = 1.0,
		bool bStepped = true);

	AAnastasisVillagerVisual* FindVillager(const FString& NpcId) const;
	int32 NumVillagers() const { return Villagers.Num(); }

	/**
	 * Debug : volumes seulement si le mesh du batiment manque, seuils, habitants (sphere coloree par la soif),
	 * trait vers la cible, texte « id  but  activite  soif  -> batiment ».
	 */
	static void DrawDebug(UWorld* World, const AnastasisVillage::FVillage& Village, const AnastasisWorld::FWorld& SimWorld);

	/** Une ligne par batiment puis par habitant : l'etat, qui s'en sert, pourquoi. */
	static void LogStatus(const AnastasisVillage::FVillage& Village, double Time);

private:
	TMap<FString, TWeakObjectPtr<AAnastasisVillageBuilding>> Actors;
	TMap<FString, TWeakObjectPtr<AAnastasisVillagerVisual>> Villagers;
	/** The simulated job each card was drawn for: a change of job redraws the card. */
	TMap<FString, FName> VillagerJobs;
	/** Position simulee avant le dernier pas, et courante (tuiles), pour l'interpolation. */
	struct FVillagerTrack { FVector2D Prev = FVector2D::ZeroVector; FVector2D Curr = FVector2D::ZeroVector; };
	TMap<FString, FVillagerTrack> VillagerTracks;
	struct FInteractionUse
	{
		FString BuildingId;
		FSmartObjectClaimHandle Claim;
		double NextAttemptAt = 0.0;
	};
	TMap<FString, FInteractionUse> InteractionUses;
	bool bWarnedNoLooks = false;
	bool bWarnedNoBody = false;
	AnastasisSettlement::FLedger Ledger;
	FAnastasisSettlementPaths Paths;
};
