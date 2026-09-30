// Village — les batiments fonctionnels et les habitants qui s'en servent.
//
// Tranche de `src/sim/simulation.js` + `src/sim/npc.js` + `src/sim/navGrid.js`
// + `src/life/villageRhythm.js` + `src/life/domestic.js`, limitee a deux
// boucles completes, celles que la reference ferme avec le moins de systemes :
//
//   PUITS  (first-building-001)             MAISON  (house-rest-001)
//   la soif monte                           la fatigue monte ; la nuit tombe
//   `drink` gagne                           `rest` gagne (phaseBias : ~+91 la nuit avec un toit)
//   puits le plus proche -> seuil           foyer, abri, logement libre -> seuil
//   A* + marche                             A* + marche
//   une seconde sur place, satisfyDrink     ENTREE : npc.inside ; tickNeeds branche sommeil ;
//                                           a `until`, satisfyRest puis SORTIE
//   npc.thirst baisse                       npc.energy remonte, selon la qualite du lit
//
// La maison apporte le mecanisme d'INTERIEUR (enterBuilding / updateInside /
// exitBuilding) que reutiliseront manger, se soulager, se detendre, socialiser.
// Elle apporte aussi le foyer (npc.home, npc.shelter, proprietaire, capacite)
// et le rythme du jour, sans lequel personne ne se couche.
//
// ECARTS DECLARES — ce qui n'est pas la reference, et pourquoi :
//
//  1. Table de decision reduite. La reference classe ~25 buts ; seuls `rest` et
//     `drink` ont leur boucle. Chaque but NON porte est tenu pour valoir
//     `UnportedGoalsFloor` (42) AVANT le rythme, puis recoit son vrai
//     `phaseBias` (porte, prouve) : le plancher est le meilleur de ces buts a la
//     phase courante. Les lignes `rest` et `drink` sont celles de la reference
//     (needGoalScores + jobPriority / bonus puits + phaseBias), sans `goalNoise`
//     (le bruit consomme `sim.rng()` dans l'ordre de TOUTE la table), sans
//     `statusBias` (misere = or <= 2 et sans toit : l'or n'existe pas encore),
//     sans mode de vie, district, meteo, age, memoire, prevision de survie.
//  2. Pas de reconsideration aleatoire (`sim.rng() < chance`) ni de collant de
//     but (`goalStickinessBonus`) : un habitant qui a une cible la garde jusqu'a
//     l'arrivee, l'echec ou la disparition ; il redecide des qu'il n'en a plus.
//  3. Points d'acces sans intention urbaine (sim/urban/intent.js, vague 5) :
//     l'anneau 1 oriente vers le camp, qui est exactement le repli de la reference.
//  4. Pilotage reduit : pas de file de porte (crowdNav), pas d'hesitation, pas
//     de facteur de vitesse, pas de contournement local ; escalade anti-blocage
//     en trois paliers ; pas de verrou de seuil domestique en route.
//  5. Pas de cadence LOD : chaque habitant est « proche du point de vue ».
//  6. RemoveBuilding n'existe pas dans la reference — elle ne demolit jamais.
//  7. Foyer minimal : `assignHomeToHousehold` sans famille (le proprietaire seul),
//     `assignSheltersDaily` a minuit ; pas d'achat de maison (or), pas
//     d'agrandissement (phase 1, capacite 3), pas d'hospitalite, pas de dortoir.
//     `redirectDomesticDoorFailure` bascule sur `explore`, non porte : ici `observer`.
//  8. Tous les habitants sont des adultes sans metier de garde, sans famille, sans
//     mode de vie. `jobPriority(rest)` vaut 18 - 2,5 = 15,5 pour TOUS les metiers
//     du catalogue (rest y est toujours au rang 1) : la constante est reprise telle quelle.
//
// Parite bit a bit : prouvee pour les besoins, le rythme et la qualite du repos
// (Anastasis.Sim.Parite.Besoins / .Rythme). La boucle assemblee est
// deterministe, elle n'est PAS la trajectoire JS — l'ecart 1 suffit a l'interdire.

#pragma once

#include "CoreMinimal.h"
#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"
#include "World/AnastasisEntityTable.h"
#include "World/AnastasisNavGrid.h"
#include "World/AnastasisPathfinding.h"

namespace AnastasisWorld { struct FWorld; }

namespace AnastasisVillage
{
	using FPoint = AnastasisPath::FPoint;

	/** Types de batiment que ce portage sait poser. Chaine = vocabulaire de la reference. */
	inline const TCHAR* const WellType = TEXT("well");
	inline const TCHAR* const HouseType = TEXT("house");

	/** Buts portes. `observer` est l'etat initial de createNpc. */
	inline const TCHAR* const GoalObserver = TEXT("observer");
	inline const TCHAR* const GoalDrink = TEXT("drink");
	inline const TCHAR* const GoalRest = TEXT("rest");

	/** `NPC_AI` de npc.js. */
	inline constexpr double ThinkEvery = 0.12;
	inline constexpr double ThinkEveryCritical = 0.045;

	/** Rayon de puisage autour du CENTRE d'un puits (`WELL_REACH`). */
	inline constexpr double WellReach = 2.5;
	/** Seuil de berge qui vaut point d'eau (`SHORE_REACH`). */
	inline constexpr double ShoreReach = 0.3;
	/** `reachedMoveTarget` : arrivee a moins de 0,75 tuile. */
	inline constexpr double ArrivalDistance = 0.75;

	/** `NPC_UNSTICK` de npc.js — la porte du foyer. */
	inline constexpr double DoorWaitSeconds = 2.8;
	inline constexpr double DoorApproachSeconds = 5.5;
	inline constexpr double DoorAccessRadius = 1.05;

	/** `buildingNearActor(actor, 3.2)` dans buildingForIndoorAction. */
	inline constexpr double IndoorBuildingRadius = 3.2;

	/**
	 * ECART DECLARE n°1 — le score d'un but non porte AVANT le rythme.
	 *
	 * 42 = `urgeScore(thirstUrge)`, la valeur a laquelle la reference dit que la
	 * soif « deborde largement les scores de metier (souvent 40-90) ». Chaque but
	 * non porte recoit ensuite son vrai `phaseBias` : le matin le travail pese
	 * 42 + 62, la nuit se soulager pese 42 + 35. Ce n'est pas une valeur de la
	 * reference ; c'est la place qu'occupent les buts qui ne sont pas encore portes.
	 */
	inline constexpr double UnportedGoalsFloor = 42.0;

	/** `jobPriority(npc, "rest") * 0.35` : rest est au rang 1 de chaque metier du catalogue. */
	inline constexpr double RestJobPriorityBias = (18.0 - 1.0 * 2.5) * 0.35;

	/** `HOUSE_PHASES[0].capacity` — une maison de phase 1 loge trois personnes. */
	inline constexpr int32 HousePhaseOneCapacity = 3;

	/** Enregistrement de batiment — les champs d'`addBuilding` que les boucles lisent. */
	struct FBuilding
	{
		FString Id;
		FString Type;
		/** Case du batiment (entiere dans la reference ; le centre est x + 0,5). */
		double X = 0.0;
		double Y = 0.0;
		/** 1 = acheve. Un batiment inacheve n'est ni compte, ni puise, ni habite. */
		double Progress = 1.0;
		int32 CreatedDay = 0;
		/** Maison : identifiant du proprietaire (`npc-N`), vide si libre. */
		FString Owner;
		/** Maison : `housePhase` (1..6) ; la capacite en decoule. */
		int32 HousePhase = 1;
		/** Seuils persistes : le PNJ vise une porte, jamais le centre bloque. */
		TArray<FPoint> AccessPoints;

		bool IsCompleted() const { return Progress >= 1.0; }
	};

	/** `npc.inside` — present pendant qu'un habitant est DANS un batiment. */
	struct FInside
	{
		bool bActive = false;
		FString BuildingId;
		FString Activity;
		FString Goal;
		double EnteredAt = 0.0;
		double Until = 0.0;
		double ExitX = 0.0;
		double ExitY = 0.0;
	};

	/**
	 * Pourquoi un habitant a choisi ce qu'il fait — OBSERVATION, pas etat.
	 * N'entre pas dans l'empreinte ; sert au debug et aux tests.
	 */
	struct FDecisionTrace
	{
		double Time = -1.0;
		FString Phase;
		AnastasisNeeds::FNeedGoalScores NeedScores;
		/** Ligne `rest` : needs.rest + jobPriority + phaseBias. */
		double RestRowScore = 0.0;
		/** Ligne `drink` : needs.drink + bonus puits + phaseBias. */
		double DrinkRowScore = 0.0;
		/** Meilleur but non porte a cette phase, et son score (plancher + phaseBias). */
		FString FloorGoal;
		double FloorScore = 0.0;
		FString Winner;
		/** Batiment vise par ce choix, vide si aucun. */
		FString BuildingId;
		/** D'ou vient la cible : well, shore, home, shelter, housing, open-shelter, settlement, none. */
		FString TargetSource;
	};

	/** Habitant — les champs de createNpc que les boucles lisent ou ecrivent. */
	struct FNpc
	{
		FString Id;
		double X = 0.0;
		double Y = 0.0;
		double Speed = 4.0;
		AnastasisNeeds::FNeeds Needs;

		FString Goal = GoalObserver;
		FString Activity;
		/** `npc.target` : un POINT, jamais un batiment. */
		bool bHasTarget = false;
		FPoint Target;
		double WorkTimer = 0.0;
		/** `npc.aiThinkAt` ; < 0 = null. */
		double AiThinkAt = -1.0;
		int32 FailedActions = 0;

		/** `npc.home` / `npc.shelter` — des IDENTIFIANTS, pas des objets. */
		FString HomeId;
		FString ShelterId;
		FInside Inside;
		double DoorStuckAt = 0.0;
		double DoorApproachAt = 0.0;

		// Navigation (`npc.navigation` + champs de chemin).
		TArray<FPoint> Path;
		int32 PathStep = 0;
		bool bHasPathGoal = false;
		FPoint PathGoal;
		bool bPathFailed = false;
		double PathCooldown = 0.0;
		FString NavTargetKey;
		int32 NavVersion = -1;
		/** `navigation.destBuildingId` — une reference PAR IDENTIFIANT. */
		FString DestBuildingId;
		double StuckTimer = 0.0;
		int32 StuckStage = 0;

		/** Actes accomplis. Observation (la reference les compte ailleurs). */
		int32 DrinksTaken = 0;
		int32 RestsTaken = 0;
		FDecisionTrace LastDecision;

		/** `livingHome(npc)` = home || shelter. */
		const FString& LivingHomeId() const { return !HomeId.IsEmpty() ? HomeId : ShelterId; }
	};

	/**
	 * L'etat du village et ses regles. Lie a un monde genere, qu'il ne possede
	 * pas : l'hote de simulation garde le monde et le village cote a cote.
	 */
	class ANASTASISSIM_API FVillage
	{
	public:
		/** Lie le village a un monde et reconstruit la grille de navigation. Vide les tables. */
		void Bind(const AnastasisWorld::FWorld& InWorld);
		bool IsBound() const { return World != nullptr; }

		/** `sim.settlement` : l'origine vers laquelle les portes s'ouvrent. Centre du monde par defaut. */
		void SetSettlement(double InX, double InY) { Settlement = { InX, InY }; }
		FPoint GetSettlement() const { return Settlement; }

		/**
		 * `addBuilding(type, x, y)` — identifiant `building-N`, case bloquee, cout
		 * infini, seuils calcules, version de navigation incrementee.
		 * Rend l'identifiant, vide si la case est hors bornes ou deja bloquee.
		 */
		FString AddBuilding(const FString& Type, int32 TileX, int32 TileY, double Progress = 1.0, int32 Day = 0);

		/**
		 * EXTENSION — la reference ne demolit jamais un batiment.
		 *
		 * Garanties : la case redevient ce que le terrain dit, la version de
		 * navigation change, et AUCUN habitant ne garde une reference vers lui —
		 * ceux qui le visaient perdent cible, chemin et but ; ceux qui etaient
		 * DEDANS en sortent par leur seuil d'entree ; ceux dont c'etait le foyer
		 * ou l'abri le perdent.
		 */
		bool RemoveBuilding(const FString& Id);

		/** `spawnNpc`, reduit : position, besoins et vitesse fournis, identifiant `npc-N`, sans toit. */
		FString SpawnNpc(double InX, double InY, const AnastasisNeeds::FNeeds& Needs, double Speed = 4.0);

		/** `actors.splice` — decalage, l'ordre des autres survit. */
		bool RemoveNpc(const FString& Id);

		/**
		 * `assignHomeToHousehold`, branche sans famille : la maison appartient a
		 * l'habitant, elle devient son foyer, moral +12. Refuse si ce n'est pas une
		 * maison achevee libre (ou deja a lui).
		 */
		bool AssignHome(const FString& NpcId, const FString& HouseId);

		/** `assignSheltersDaily` — appele par l'hote a minuit. Rend le nombre d'abrites. */
		int32 AssignSheltersDaily();

		/** `for (const npc of this.actors) updateNpc(this, npc, dt)` — Time = temps de sim APRES avance. */
		void UpdateActors(double Time, double Dt);

		/** `countBuildings(type)` — acheves seulement. */
		int32 CountBuildings(const FString& Type) const;

		/** `shelterCapacity` — maison : capacite de sa phase ; sinon `housing` du catalogue. */
		int32 ShelterCapacity(const FBuilding& Building) const;

		/** `countShelterOccupants` — habitants dont c'est le foyer OU l'abri (pas ceux qui sont dedans). */
		int32 CountShelterOccupants(const FString& BuildingId) const;

		/** Habitants physiquement dedans en ce moment (`npc.inside.buildingId`). */
		TArray<FString> InsideOf(const FString& BuildingId) const;

		/** `sleepQuality(npc)`. */
		static double SleepQualityOf(const FNpc& Npc);

		const TArray<FBuilding>& GetBuildings() const { return Buildings.GetItems(); }
		const TArray<FNpc>& GetActors() const { return Actors.GetItems(); }
		const FBuilding* FindBuilding(const FString& Id) const { return Buildings.FindById(Id); }
		const FNpc* FindNpc(const FString& Id) const { return Actors.FindById(Id); }
		/** Acces d'ecriture pour les tests et les outils de debug (besoins imposes). */
		FNpc* FindNpcMutable(const FString& Id) { return Actors.FindById(Id); }

		const AnastasisNav::FNavGrid& GetNavGrid() const { return Nav; }
		int32 GetNavVersion() const { return NavVersion; }

		/** `footBlockedAt(floor(x), floor(y))` — eau, bati, arbres debout. */
		bool IsFootBlocked(double InX, double InY) const;

		/** `isBlocked(x, y)` — eau et bati seulement (enterBuilding / exitBuilding). */
		bool IsBlocked(double InX, double InY) const;

		/** `atDrinkSpot` : eau, berge, ou moins de 2,5 tuiles du centre d'un puits acheve. */
		bool AtDrinkSpot(double InX, double InY) const;

		/**
		 * Habitants qui se servent de ce batiment en ce moment : ceux qui le visent
		 * (`destBuildingId`) pour boire ou dormir, et ceux qui sont dedans. Derive,
		 * jamais stocke — la reference n'a pas de liste d'occupants.
		 */
		TArray<FString> UsersOf(const FString& BuildingId) const;

		/** Projection canonique (FStateWriter) : batiments puis acteurs, dans l'ordre. */
		uint64 Digest() const;

	private:
		void UpdateNpc(FNpc& Npc, double Dt);
		void ChooseGoal(FNpc& Npc);
		void Act(FNpc& Npc, double Dt);
		bool Perform(FNpc& Npc);
		void RedirectAfterFailure(FNpc& Npc);
		void RedirectDomesticDoorFailure(FNpc& Npc);

		AnastasisRhythm::FPhaseSubject PhaseSubjectOf(const FNpc& Npc) const;
		bool IsNight() const;

		const FBuilding* NearestWell(double FromX, double FromY) const;
		bool DrinkTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		bool RestTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		const FBuilding* NearestHousing(const FNpc& Npc) const;
		const FBuilding* FindOpenShelter(const FNpc& Npc) const;
		bool IsEnterableHousing(const FBuilding& Building) const;
		const FBuilding* NearLivingHome(const FNpc& Npc) const;

		bool TryEnterIndoorAction(FNpc& Npc);
		const FBuilding* BuildingForIndoorAction(const FNpc& Npc) const;
		const FBuilding* BuildingNearActor(const FNpc& Npc, double Radius) const;
		bool EnterBuilding(FNpc& Npc, const FBuilding& Building, const FString& InActivity, double Duration);
		void UpdateInside(FNpc& Npc);
		bool ExitBuilding(FNpc& Npc);

		bool BuildingAccessPoint(FBuilding& Building, FNpc* Actor, FPoint& Out, const FPoint* Exclude = nullptr);
		bool BuildingAccessPointById(const FString& BuildingId, FNpc* Actor, FPoint& Out);
		bool PickBuildingAccessPoint(FBuilding& Building, const FNpc* Actor, FPoint& Out, const FPoint* Exclude);
		const TArray<FPoint>& EnsureBuildingAccessPoints(FBuilding& Building);
		TArray<FPoint> ComputeBuildingAccessPoints(const FBuilding& Building) const;
		bool AccessPointNear(double PosX, double PosY, const FNpc* Actor, FPoint& Out) const;
		bool NearestFreePoint(double InX, double InY, int32 MaxRadius, FPoint& Out) const;
		TMap<FIntPoint, double> LocalOccupancy(int32 CX, int32 CY, int32 Radius, const FNpc* Ignored) const;
		bool IsFreeCell(int32 TX, int32 TY) const;

		bool ReachedMoveTarget(const FNpc& Npc, const FPoint& Target) const;
		void MoveActor(FNpc& Npc, const FPoint& Target, double Dt);
		FPoint NextWaypoint(FNpc& Npc, const FPoint& Target, double Dt);
		void ResolveStuckActor(FNpc& Npc, const FPoint& Target);
		void ClearNavigation(FNpc& Npc);

		const AnastasisWorld::FWorld* World = nullptr;
		AnastasisNav::FNavGrid Nav;
		int32 NavVersion = 0;
		FPoint Settlement;
		int32 NextBuildingId = 0;
		int32 NextNpcId = 0;
		/** `sim.time` du tick en cours (pose par UpdateActors). */
		double Now = 0.0;
		TAnastasisEntityTable<FBuilding> Buildings;
		TAnastasisEntityTable<FNpc> Actors;
	};

	/** `aiThinkStagger` — decalage FNV-1a de l'identifiant, dans [0, thinkEvery). */
	ANASTASISSIM_API double AiThinkStagger(const FString& Id);

	/** `needsCritical`. */
	ANASTASISSIM_API bool NeedsCritical(const AnastasisNeeds::FNeeds& Needs);

	/** `BUILDINGS[type].housing` — places de logement du catalogue (0 = pas un logement). */
	ANASTASISSIM_API int32 HousingOfType(const FString& Type);

	/** Buts de la table adulte qui n'ont PAS de boucle portee (ecart n°1). */
	ANASTASISSIM_API const TArray<FString>& UnportedGoals();
}
