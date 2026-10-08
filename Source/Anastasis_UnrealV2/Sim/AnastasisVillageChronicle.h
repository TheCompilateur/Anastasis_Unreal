#pragma once

// CHRONIQUE_VILLAGE_001 -- la chronique du village, en francais, lisible sans image.
//
// Bible canonique, §36 : « La chronique est produite au commit, derivee du ledger, et lisible sans
// rendu. C'est ce qui permet de juger le jeu avant qu'il ait des images. » Le ledger de la Bible
// n'existe pas encore dans AnastasisSim : cette chronique en tient lieu en OBSERVANT le village.
// Elle compare l'etat lu a chaque passage avec le precedent et raconte ce qui a change : arrivees,
// morts et leur cause, chantiers ouverts, qui y a mis la main, maisons achevees, qui s'installe ou,
// metiers, amities, faim et soif, grenier vide ou plein, et un bilan a la fin de chaque jour.
//
// Elle ne fait que LIRE : elle prend la simulation en const et n'ecrit rien dedans. C'est pourquoi
// elle vit dans ce module et non dans AnastasisSim (PROTOCOLE_ECARTS : ce qui lit sans ecrire n'est
// pas un ecart de portage). Le test Anastasis.Chronique.LectureSeule le verifie sur l'empreinte d'etat.
//
// Ce qu'elle ne sait pas, parce que la simulation ne le porte pas encore : les familles, les vrais
// noms, les demandes d'aide et leurs refus, les dettes. Les noms sont donc PROVISOIRES : tires d'une
// liste de prenoms byzantins, stables pour une partie, et accordes au portrait quand l'hote sait
// lequel l'habitant porte. Les familles et leurs noms viennent avec la mission 2 (docs/unreal/CHRONIQUE_VILLAGE_001.md).

#include "CoreMinimal.h"

class FAnastasisSimulation;

namespace AnastasisVillage
{
	struct FNpc;
}

namespace AnastasisChronicle
{
	enum class EKind : uint8
	{
		Founding,
		Arrival,
		Departure,
		Death,
		SiteOpened,
		BuildingPlaced,
		Helped,
		BuildingDone,
		BuildingGone,
		Home,
		HomeLost,
		Shelter,
		Job,
		Friendship,
		Quarrel,
		Hunger,
		Fed,
		Thirst,
		Drank,
		Weak,
		Recovered,
		FoodOut,
		FoodBack,
		FoodLow,
		/** Un chantier qui n'avance plus depuis 3, 7, 15 ou 30 jours. Toujours le dernier : StatusJson s'arrete la. */
		Stalled,
	};

	/** Nom stable d'un evenement, pour le JSON et les tests. */
	ANASTASIS_UNREALV2_API const TCHAR* KindName(EKind Kind);

	/** Une ligne de la chronique : quand, quoi, qui, et la phrase. */
	struct FEntry
	{
		int32 Day = 0;
		/** Heure du jour simule, 0..23. */
		int32 Hour = 0;
		EKind Kind = EKind::Founding;
		/** Identifiants des habitants concernes (`npc-N`) : sert au recit « ce qu'a vecu chacun ». */
		TArray<FString> People;
		FString Text;
		/** Faux : la ligne ne sert qu'au recit de la personne (le jour la dit deja en une phrase). */
		bool bDayLog = true;
	};

	/** L'etat du village a la fin d'un jour. */
	struct FDaySummary
	{
		int32 Day = 0;
		int32 Inhabitants = 0;
		int32 Houses = 0;
		int32 Sites = 0;
		int32 Food = 0;
		bool bHasGranary = false;
		int32 Homeless = 0;
		int32 Deaths = 0;
		int32 Events = 0;
		/** Jours d'affilee, celui-ci compris, ou le grenier etait vide le soir. */
		int32 EmptyDays = 0;
	};

	/** Ce que l'hote sait de l'apparence d'un habitant (son portrait) : de quoi accorder un nom. */
	struct FPersonLook
	{
		bool bKnown = false;
		bool bFemale = false;
		bool bElder = false;
	};

	/** Libelle francais d'une heure : « la nuit », « a l'aube », « le matin »... */
	ANASTASIS_UNREALV2_API FString HourLabel(int32 Hour);

	class ANASTASIS_UNREALV2_API FVillageChronicle
	{
	public:
		using FLookResolver = TFunction<FPersonLook(const AnastasisVillage::FNpc&)>;

		/** Tout oublier : le prochain Observe ouvre une nouvelle chronique (fondation). */
		void Reset(uint32 Seed);

		/** Facultatif : l'hote dit quel portrait porte un habitant, pour accorder son nom. */
		void SetLookResolver(FLookResolver InResolver) { LookResolver = MoveTemp(InResolver); }

		/** Lit la simulation et ecrit ce qui a change depuis le dernier passage. Ne la modifie jamais. */
		void Observe(const FAnastasisSimulation& Sim);

		/** La chronique entiere, en francais : habitants, jours, puis ce qu'a vecu chacun. */
		FString Render() const;

		/** Resume machine (JSON) pour les preuves : jours, lignes par type, habitants, morts. */
		FString StatusJson() const;

		bool HasStarted() const { return bStarted; }
		int32 GetFirstDay() const { return FirstDay; }
		/** Dernier jour CLOS (bilan ecrit) ; 0 tant qu'aucun jour n'est fini. */
		int32 GetLastClosedDay() const { return Days.Num() ? Days.Last().Day : 0; }
		const TArray<FEntry>& GetEntries() const { return Entries; }
		const TArray<FDaySummary>& GetDays() const { return Days; }
		int32 CountOf(EKind Kind) const;

		/** Nom provisoire d'un habitant deja vu ; l'identifiant brut sinon. */
		FString NameOf(const FString& NpcId) const;

	private:
		struct FPersonState
		{
			FString Name;
			FPersonLook Look;
			bool bAlive = true;
			bool bGone = false;
			FString HomeId;
			/** L'abri que `assignSheltersDaily` donne aux sans-maison ; on ne raconte qu'un abri nouveau. */
			FString ShelterId;
			TSet<FString> SheltersTold;
			FString JobId;
			FString WorkplaceId;
			bool bHungry = false;
			bool bThirsty = false;
			bool bWeak = false;
			int32 Pieces = 0;
			int32 Materials = 0;
			int32 FirstDay = 0;
			int32 DeathDay = 0;
			FString DeathCause;
			TSet<FString> Friends;
			TSet<FString> Foes;
		};

		struct FBuildingState
		{
			FString Type;
			FString Label;
			bool bDone = false;
			bool bGone = false;
			FString Owner;
			/** Ceux qui y ont pose une piece ou apporte des materiaux, dans l'ordre ou ils sont venus. */
			TArray<FString> Helpers;
			/** Avancement lu, et le jour ou il a bouge pour la derniere fois : un chantier qui stagne se dit. */
			double Progress = 0.0;
			int32 ProgressDay = 0;
			int32 StallTold = 0;
		};

		struct FStats
		{
			int32 Inhabitants = 0;
			int32 Houses = 0;
			int32 Sites = 0;
			int32 Food = 0;
			bool bHasGranary = false;
			int32 Homeless = 0;
		};

		void Add(int32 Day, int32 Hour, EKind Kind, TArray<FString> People, FString Text, bool bDayLog = true);
		void Found(const FAnastasisSimulation& Sim, int32 Day, int32 Hour);
		FPersonState& Meet(const AnastasisVillage::FNpc& Npc, int32 Day);
		FString PickName(const FString& NpcId, const FPersonLook& Look);
		FString LabelFor(const FString& Type);
		FString BuildingLabel(const FString& BuildingId) const;
		FString Names(const TArray<FString>& Ids) const;
		FStats ReadStats(const FAnastasisSimulation& Sim) const;
		void CloseDay(int32 Day);
		/** Rend 0 sans batiment acheve de ce type, 1 si aucun chemin n'en atteint un seuil, 2 s'il est atteignable. */
		int32 Reach(const FAnastasisSimulation& Sim, const AnastasisVillage::FNpc& Npc, const FString& Type) const;

		uint32 Seed = 0;
		bool bStarted = false;
		int32 FirstDay = 0;
		int32 CurrentDay = 0;
		int32 DeathsSeen = 0;
		int32 DeathsToday = 0;
		int32 FoodLowDay = 0;
		FStats LastStats;
		FLookResolver LookResolver;
		TArray<FString> PersonOrder;
		TMap<FString, FPersonState> People;
		TArray<FString> BuildingOrder;
		TMap<FString, FBuildingState> Buildings;
		TMap<FString, int32> TypeCounts;
		TSet<FString> UsedNames;
		TArray<FEntry> Entries;
		TArray<FDaySummary> Days;
	};
}
