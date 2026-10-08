#pragma once

#include "CoreMinimal.h"
#include "Village/AnastasisArchitecture.h"

namespace AnastasisVillage { class FVillage; struct FBuilding; }

/**
 * SETTLEMENT_MORPHOGENESIS_001 -- la biographie des batiments.
 *
 * La simulation possede la verite sociale (qui possede, qui dort ou, quel metier, quand le chantier
 * s'acheve, depuis quand une maison est vide). Elle ne garde pas l'HISTOIRE d'un batiment : qui l'a
 * fonde, pour quel foyer, quand il a change de mains, combien de nuits il a ete trop plein. Ce registre
 * l'ecrit en OBSERVANT les transitions de la simulation, jour apres jour ; il ne decide rien pour elle
 * et n'y ecrit rien.
 *
 * Il fixe une chose que la simulation n'a pas : le PROGRAMME du batiment, la forme que lui a donnee son
 * fondateur. Une maison prend sa forme le jour ou un foyer la prend (metier du fondateur, taille de son
 * foyer, phase de la maison), et la GARDE : un nouveau proprietaire herite des murs d'un autre.
 *
 *     BUILDING_VISUAL_STATE = f(BIOGRAPHIE, ETAT COURANT)    -- jamais f(graine)
 *
 * Persistance : aucune (comme la simulation, qui n'a pas de sauvegarde Unreal). Le registre est
 * RECONSTRUCTIBLE : rejouer la meme simulation (graine, commandes) reecrit la meme biographie, puisqu'il
 * ne tire rien et ne lit que l'etat simule. Le jour ou la simulation se sauvegarde, la biographie doit
 * l'etre avec elle (elle n'est pas derivable d'un instantane : un fondateur mort ne s'y lit plus).
 */
namespace AnastasisSettlement
{
	enum class EEvent : uint8
	{
		Seen,         // le batiment apparait (chantier ou pose directe)
		Completed,    // le chantier s'acheve
		Founded,      // un foyer le prend pour la premiere fois : le programme se fixe
		OwnerChanged, // il passe a un autre foyer
		OwnerLost,    // son proprietaire disparait (mort, depart) : la maison se vide
		Crowded,      // premiere nuit ou le foyer remplit la maison (pression d'agrandissement)
		Vacated,      // plus personne n'y dort
		Reoccupied,   // quelqu'un y dort de nouveau
	};

	const TCHAR* EventName(EEvent Kind);

	struct FEvent
	{
		int32 Day = 0;
		EEvent Kind = EEvent::Seen;
		FString Detail;
	};

	struct FBiography
	{
		FString Id;
		FString Type;
		int32 CellX = 0;
		int32 CellY = 0;
		int32 FirstSeenDay = 0;
		/** Jour d'achevement (FBuilding::CompletedDay, sinon le premier jour vu acheve). -1 = chantier. */
		int32 CompletedDay = -1;
		/** Jour ou un foyer l'a prise : le programme est fixe depuis ce jour. -1 = jamais habitee. */
		int32 FoundedDay = -1;
		FString Founder;
		FString FounderJob;
		int32 FounderHousehold = 0;
		AnastasisArchitecture::EVariant Program = AnastasisArchitecture::EVariant::HousePoor;
		bool bProgramFixed = false;
		/** Pourquoi cette forme : une phrase lisible, pas un code. */
		FString ProgramCause;
		FString Owner;
		int32 Occupants = 0;
		int32 PeakOccupants = 0;
		/** Nuits ou le foyer remplissait la maison (`occupants >= capacite - 1`, la pression de `resolveHouseUpgrades`). */
		int32 CrowdedDays = 0;
		int32 OwnerChanges = 0;
		int32 VacancyEpisodes = 0;
		bool bWasOccupied = false;
		int32 LastObservedDay = -1;
		TArray<FEvent> Events;

		int32 AgeDays(int32 Day) const { return CompletedDay < 0 ? 0 : FMath::Max(0, Day - CompletedDay); }
	};

	/**
	 * Le programme d'une maison d'apres CE QUE LA SIMULATION SAIT de son foyer. Pur, sans tirage.
	 * Puits -> puits ; grenier -> grenier communautaire ; maison sans foyer -> abri provisoire (pauvre).
	 * Maison prise : le metier du fondateur et la taille de son foyer (proprietaire + abrites) decident,
	 * la phase de la maison (agrandissements de la reference) l'emporte si elle est plus haute.
	 * `OutCause` dit pourquoi, en clair. Faux si le type n'a pas de forme.
	 */
	bool ProgramFor(const FString& Type, int32 HousePhase, const FString& OwnerJob, int32 Household,
		AnastasisArchitecture::EVariant& OutProgram, FString& OutCause);

	class FLedger
	{
	public:
		/**
		 * Observe la simulation : nouveaux batiments, achevements, prises, changements de mains, pleins,
		 * vides. Les compteurs journaliers (nuits pleines) ne comptent qu'une fois par jour simule.
		 * Rend le nombre d'evenements ecrits.
		 */
		int32 Observe(const AnastasisVillage::FVillage& Village, int32 Day);

		const FBiography* Find(const FString& Id) const { return Bios.Find(Id); }
		const TMap<FString, FBiography>& GetAll() const { return Bios; }
		void Reset() { Bios.Reset(); }

		/** Une ligne `ANASTASIS_SETTLEMENT bio ...` par batiment, plus ses evenements. */
		void Log(int32 Day) const;

	private:
		TMap<FString, FBiography> Bios;
	};
}
