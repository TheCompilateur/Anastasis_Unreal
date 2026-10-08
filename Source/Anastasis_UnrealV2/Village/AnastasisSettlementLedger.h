#pragma once

#include "CoreMinimal.h"
#include "Village/AnastasisArchitecture.h"

#include "Village/AnastasisVillage.h"

/**
 * SETTLEMENT_MORPHOGENESIS_001 -- la biographie des batiments, vue par la presentation.
 *
 * Depuis save-history-001 (ecart n°46), les FAITS de la biographie vivent dans la simulation
 * (AnastasisVillage::FBuildingBiography, observee a chaque pas, dans l'empreinte d'etat et la sauvegarde) :
 * qui l'a fonde, pour quel foyer, quand il a change de mains, combien de nuits il a ete plein. Ce
 * registre n'observe plus rien lui-meme : il recopie ces faits et en DEDUIT la forme, le programme
 * d'architecture que lui a donne son fondateur.
 *
 *     BUILDING_VISUAL_STATE = f(BIOGRAPHIE, ETAT COURANT)    -- jamais f(graine)
 *
 * Une maison prend sa forme le jour ou un foyer la prend (metier du fondateur, taille de son foyer,
 * phase de la maison), et la GARDE : un nouveau proprietaire herite des murs d'un autre. Une partie
 * rechargee rend donc les memes formes.
 */
namespace AnastasisSettlement
{
	using EEvent = AnastasisVillage::EBiographyEvent;
	using FEvent = AnastasisVillage::FBiographyEvent;

	const TCHAR* EventName(EEvent Kind);

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
		 * Recopie les biographies de la simulation et en deduit la forme ; journalise chaque evenement
		 * nouveau (`ANASTASIS_SETTLEMENT event`). Rend le nombre d'evenements nouveaux.
		 */
		int32 Observe(const AnastasisVillage::FVillage& Village, int32 Day);

		const FBiography* Find(const FString& Id) const { return Bios.Find(Id); }
		const TMap<FString, FBiography>& GetAll() const { return Bios; }
		void Reset() { Bios.Reset(); LoggedEvents.Reset(); }

		/** Une ligne `ANASTASIS_SETTLEMENT bio ...` par batiment, plus ses evenements. */
		void Log(int32 Day) const;

	private:
		TMap<FString, FBiography> Bios;
		/** Evenements deja journalises par batiment (la presentation ne repete pas ce qu'elle a dit). */
		TMap<FString, int32> LoggedEvents;
	};
}
