// Village — les batiments fonctionnels et les habitants qui s'en servent.
//
// Tranche de `src/sim/simulation.js` + `src/sim/npc.js` + `src/sim/navGrid.js`
// + `src/life/villageRhythm.js` + `src/life/domestic.js` + `src/ai/algorithmic/*`
// + `src/sim/transport/stockLedger.js`, limitee a trois boucles completes :
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
//   GRENIER (granary-eat-001) — Noûs, le chemin ACTIF PAR DEFAUT de la reference
//   la faim monte ; Noûs (toutes les 2,2 s) score manger / chercher / travailler...
//   `eat` gagne (table complete + biais Noûs + porte de commit)
//   RESERVATION d'une portion dans le stock du grenier connu (croyance vue a 7 tuiles)
//   cible : couches rythme (mealPlace) et domestique — le FOYER passe avant le grenier
//   ENTREE (2,1 s) ; tickNeeds branche repas ; a `until` : confirmMeal (stock -1),
//   inventaire +1 -1, satisfyEat
//   building.stock.food.physical baisse ; npc.hunger baisse
//
// FOOD SUPPLY (food-supply-001), extension opt-in : ActivateFoodSource lit une
// tuile Food generee et ouvre un registre fini (la generation reste immuable).
// Perception locale -> gatherFood (3 s, 2 portions) -> inventaire -> deliver
// au seuil du grenier -> CreditFood accepte / debit du sac -> repas reserve.
// Ecarts explicites : pas de craft session / outil / metier / progression JS ;
// scores bornes de collecte/livraison (85 et 100+5*sac), pas de regeneration.
// Ce circuit est une extension deterministe, PAS un portage de trajectoire JS.
// Quand une source est active, manger dedans exige une reservation : le repas
// a vide de la reference ne doit pas masquer une rupture d'approvisionnement.
//
// La maison apporte le mecanisme d'INTERIEUR (enterBuilding / updateInside /
// exitBuilding) que reutiliseront manger, se soulager, se detendre, socialiser.
// Elle apporte aussi le foyer (npc.home, npc.shelter, proprietaire, capacite)
// et le rythme du jour, sans lequel personne ne se couche.
//
// ECARTS DECLARES — ce qui n'est pas la reference, et pourquoi :
//
//  1. Table de decision : les 25 lignes de adultScores, dans l'ordre de la
//     reference, triees de facon stable. `eat`, `rest`, `drink` sont calculees
//     (needGoalScores + jobPriority / bonus puits + phaseBias) ; chaque but NON
//     porte vaut `UnportedGoalsFloor` (42) + son vrai `phaseBias`. Noûs biaise
//     TOUTES les lignes (applyAlgorithmicScoreBias) puis la porte de commit peut
//     forcer `eat` ou `rest`. Un but non porte qui gagne donne `observer`. Sans `goalNoise`
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
//  5. Pas de cadence LOD : chaque habitant est « proche du point de vue ». La
//     cadence de pensee est celle de Noûs (2,2 s ; 0,55 s en besoin critique),
//     avec la bascule de phase qui force une pensee, comme la reference.
//  6. RemoveBuilding n'existe pas dans la reference — elle ne demolit jamais.
//  7. Foyer minimal : `assignHomeToHousehold` sans famille (le proprietaire seul),
//     `assignSheltersDaily` a minuit ; pas d'achat de maison (or), pas
//     d'agrandissement (phase 1, capacite 3), pas d'hospitalite, pas de dortoir.
//     `redirectDomesticDoorFailure` bascule sur `explore`, non porte : ici `observer`.
//  8. Tous les habitants sont des adultes sans metier de garde, sans famille, sans
//     mode de vie. `jobPriority(rest)` vaut 18 - 2,5 = 15,5 pour TOUS les metiers
//     du catalogue (rest y est toujours au rang 1) : la constante est reprise telle quelle.
//     Idem `jobPriority(eat)` = 18 (rang 0 partout).
//  9. Noûs, ce qui n'est pas porte : pas d'or ni de marche (buy_food ne gagne
//     jamais ; `believedStock` = 0), pas de danger (flee ne gagne jamais), pas
//     d'oubli des croyances (`staleAfterDays`), pas de mendicite (`begForFood`),
//     pas de memoire d'echec (`noteGoalFailure`), pas d'agregat `market.stock`,
//     pas de gisements (`spots`). Le stock ne porte que la nourriture.
//     COMPORTEMENT DE LA REFERENCE, reproduit tel quel : un habitant qui a un
//     foyer ou un abri est envoye CHEZ LUI pour `eat` (couches rythme et
//     domestique), meme si sa reservation est au grenier ; il y mange « a vide »
//     (la branche repas de tickNeeds baisse sa faim), et sa reservation expire.
//
// Parite bit a bit : prouvee pour les besoins, le rythme, la qualite du repos et
// la decision Noûs (Anastasis.Sim.Parite.Besoins / .Rythme / .Nous). La boucle assemblee est
// deterministe, elle n'est PAS la trajectoire JS — l'ecart 1 suffit a l'interdire.

#pragma once

#include "CoreMinimal.h"
#include "Ai/AnastasisNous.h"
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
	inline const TCHAR* const GranaryType = TEXT("granary");

	/** Buts portes. `observer` est l'etat initial de createNpc. */
	inline const TCHAR* const GoalObserver = TEXT("observer");
	inline const TCHAR* const GoalDrink = TEXT("drink");
	inline const TCHAR* const GoalRest = TEXT("rest");
	inline const TCHAR* const GoalEat = TEXT("eat");

	/** `NPC_AI` de npc.js (chemin classique) — conserve pour `aiThinkStagger`. */
	inline constexpr double ThinkEvery = 0.12;
	inline constexpr double ThinkEveryCritical = 0.045;

	/** `jobPriority(npc, "eat") * 0.35` : eat est au rang 0 de chaque metier du catalogue. */
	inline constexpr double EatJobPriorityBias = 18.0 * 0.35;

	/** `DEPOT_PROFILES.granary.cap.food`. */
	inline constexpr int32 GranaryFoodCap = 300;

	/** `PERCEPTION` de ai/memory.js. */
	inline constexpr double PerceptionRadius = 7.0;
	inline constexpr double PerceptionScanInterval = 9.0;

	/** `MEAL_RESERVATION` de mealReservation.js. */
	namespace Meal
	{
		inline constexpr double TtlSeconds = 45.0;
		inline constexpr double TtlMaxSeconds = 180.0;
		inline constexpr double AbsoluteMaxSeconds = 120.0;
		inline constexpr int32 MaxRenewals = 3;
		inline constexpr double RenewalSeconds = 35.0;
		inline constexpr double ProgressStallSeconds = 18.0;
		inline constexpr double InteractionRadius = 2.2;
	}

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
		/** `building.stock.food` — `{physical, reserved}`, reserved <= physical. Grenier seulement. */
		int32 FoodPhysical = 0;
		int32 FoodReserved = 0;

		bool IsCompleted() const { return Progress >= 1.0; }
		int32 FoodAvailable() const { return FMath::Max(0, FoodPhysical - FoodReserved); }
	};

	/** Finite food ledger over an immutable generated tile. Only this ledger is consumed.
	 * No regrowth; reactivating a tile never refills it. */
	struct FFoodSource
	{
		int32 TileIndex = INDEX_NONE;
		FPoint Position;
		int32 Initial = 0;
		int32 Remaining = 0;
	};

	/** Une portion reservee (`sim.mealReservations.byId[...]`). */
	struct FMealReservation
	{
		FString Id;
		FString NpcId;
		/** Vide pour une reservation d'inventaire. */
		FString BuildingId;
		/** "colony" ou "inventory". */
		FString Source;
		int32 Amount = 1;
		double CreatedAt = 0.0;
		double ExpiresAt = 0.0;
		double AbsoluteExpiresAt = 0.0;
		int32 Renewals = 0;
		double LastProgressAt = 0.0;
		double LastDistance = 0.0;
	};

	/** `npc.hungerAction` — la machine a etats du repas. */
	struct FHungerAction
	{
		FString State = TEXT("pending");
		FString TargetId;
		double StartedAt = 0.0;
		FString LastFailure;
		double CooldownUntil = 0.0;
		double Progress = 0.0;
		FString ReservationId;
		FString SourceBuildingId;
		FString ExcludedType;
	};

	/** Une croyance de stock (`mind.beliefs.knownStocks[key]`) — ce que l'habitant CROIT. */
	struct FStockBelief
	{
		FString Key;
		FString Resource;
		FString BuildingId;
		FString Kind;
		double X = 0.0;
		double Y = 0.0;
		double EstimatedAmount = 0.0;
		double Confidence = 0.0;
		int32 Day = 0;
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
		/** Ligne `eat` : needs.eat + jobPriority + phaseBias, puis biais Noûs. */
		double EatRowScore = 0.0;
		/** Tete de table apres tri, avant la porte de commit (peut etre un but non porte). */
		FString TableWinner;
		/** Decision Noûs au moment du choix, et ce que la porte a fait. */
		FString NousType;
		double NousScore = 0.0;
		double NousUrgency = 0.0;
		FString CommitGate;
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

		/** `npc.inventory.food`. */
		int32 InventoryFood = 0;
		int32 GatheredFood = 0;
		int32 DeliveredFood = 0;
		int32 FoodSourceIndex = INDEX_NONE;
		/** Locally perceived source quantities; zero means observed exhausted. */
		TMap<int32, int32> KnownFoodSources;
		FHungerAction HungerAction;
		/** `mind.beliefs.knownStocks`, dans l'ordre d'insertion (celui d'un objet JS). */
		TArray<FStockBelief> KnownStocks;
		double LastScan = -999.0;
		/** `npc.villagePhase` — la bascule force une pensee. */
		FString VillagePhase;
		double GoalSince = 0.0;
		/** `npc._algoDebug` : la decision Noûs courante et son contexte. */
		bool bHasAlgoDecision = false;
		AnastasisNous::FDecision AlgoDecision;
		AnastasisNous::FFoodContext AlgoContext;
		bool bAlgoInertiaKeep = false;
		FString AlgoInertiaReason;
		FString AlgoMappedGoal;

		/** Actes accomplis. Observation (la reference les compte ailleurs). */
		int32 DrinksTaken = 0;
		int32 RestsTaken = 0;
		int32 MealsTaken = 0;
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

		/** `creditStock(building, "food", n)` — respecte la capacite. Rend la quantite ajoutee. */
		int32 CreditFood(const FString& BuildingId, int32 Amount);

		/** Activate an existing generated Food tile, once; no injected stock. */
		bool ActivateFoodSource(int32 TileX, int32 TileY);
		const TArray<FFoodSource>& GetFoodSources() const { return FoodSources; }

		/** Registre des reservations, dans l'ordre d'insertion. */
		const TArray<FMealReservation>& GetMealReservations() const { return MealReservations; }
		const FMealReservation* FindMealReservation(const FString& NpcId) const;

		/** `perceive(sim, npc, true)` — balayage force (utilise par SpawnNpc et les tests). */
		void PerceiveNow(const FString& NpcId);

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
		bool FoodSupplyTarget(FNpc& Npc, FPoint& Out, FString& Source);
		bool PerformFoodSupply(FNpc& Npc);
		bool HasKnownFoodSource(const FNpc& Npc) const;
		const FBuilding* KnownFoodDepot(const FNpc& Npc) const;
		void CommitGoal(FNpc& Npc, const FString& Next, FDecisionTrace& Trace);
		bool AssignTarget(FNpc& Npc, FDecisionTrace& Trace);

		// Perception (ai/memory.js, branche nourriture).
		void Perceive(FNpc& Npc, bool bForce);
		AnastasisNous::FFoodContext PerceiveFoodContext(const FNpc& Npc) const;
		int32 Day() const;

		// Noûs (ai/algorithmic).
		void ComputeAlgorithmicDecision(FNpc& Npc);
		void ApplyAlgorithmicScoreBias(FNpc& Npc, TArray<TPair<FString, double>>& Rows);
		FString ApplyAlgorithmicCommitGate(FNpc& Npc, const FString& Next, const FString& Previous, FString& OutGate);
		void OnAlgorithmicGoalCommitted(FNpc& Npc, const FString& Previous, const FString& Next);
		void TickAlgorithmicNpc();
		/** Rend 1 (mange), 0 (echec), -1 (en route : `null` de la reference). */
		int32 TryAlgorithmicEat(FNpc& Npc);
		int32 RunHungerActionStep(FNpc& Npc, bool bAtFoodAccess, const FString& SourceBuildingId);
		void FailHungerAction(FNpc& Npc, const FString& Reason, const FString& ExcludedType);
		void CancelHungerAction(FNpc& Npc, const FString& Reason);
		bool Eat(FNpc& Npc);

		// Reservations (mealReservation.js).
		bool ReserveMeal(FNpc& Npc, const FString& BuildingId, double TravelSeconds, double TtlSeconds, FString& OutReason);
		FMealReservation* GetMealReservation(const FNpc& Npc);
		bool ReleaseMeal(FNpc& Npc, const FString& Reason);
		bool ConfirmMeal(FNpc& Npc, FString& OutReason, FString& OutSourceId);
		FString MaybeRenewMealReservation(FNpc& Npc);
		int32 ExpireMealReservations();
		bool MealSourceAccessPoint(FNpc& Npc, FPoint& Out, bool& bOutBuilding);
		bool IsNpcAtMealSource(FNpc& Npc);

		bool EatTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		const FBuilding* MealPlace(const FNpc& Npc) const;
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
		const FBuilding* BuildingForIndoorAction(const FNpc& Npc, const FString& Goal) const;
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
		/** `sim.mealReservations` : ordre d'insertion = ordre de `Object.keys`. */
		TArray<FMealReservation> MealReservations;
		TArray<FFoodSource> FoodSources;
		int32 MealSeq = 0;
		double ReservationSweepAt = 0.0;
		TAnastasisEntityTable<FBuilding> Buildings;
		TAnastasisEntityTable<FNpc> Actors;
	};

	/** `aiThinkStagger` — decalage FNV-1a de l'identifiant, dans [0, thinkEvery). */
	ANASTASISSIM_API double AiThinkStagger(const FString& Id);

	/** `needsCritical`. */
	ANASTASISSIM_API bool NeedsCritical(const AnastasisNeeds::FNeeds& Needs);

	/** `BUILDINGS[type].housing` — places de logement du catalogue (0 = pas un logement). */
	ANASTASISSIM_API int32 HousingOfType(const FString& Type);

	/** `BUILDINGS[type].group === "food"` (grenier). */
	ANASTASISSIM_API bool IsFoodGroupType(const FString& Type);

	/** `computeMealTtlSeconds` ; TravelSeconds NaN = trajet inconnu. */
	ANASTASISSIM_API double ComputeMealTtlSeconds(double TravelSeconds, double Base = Meal::TtlSeconds);

	/** Buts de la table adulte qui n'ont PAS de boucle portee (ecart n°1). */
	ANASTASISSIM_API const TArray<FString>& UnportedGoals();
}
