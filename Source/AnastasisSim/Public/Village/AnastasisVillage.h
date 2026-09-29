// Village — le premier batiment fonctionnel et les habitants qui s'en servent.
//
// Tranche de `src/sim/simulation.js` + `src/sim/npc.js` + `src/sim/navGrid.js`
// + `src/life/villageRhythm.js`, limitee a UNE boucle complete, celle que la
// reference ferme avec le moins de systemes : le PUITS.
//
//   tickNeeds       la soif monte                      (needs.js, prouve bit a bit)
//   chooseGoal      `drink` gagne                      (needGoalScores, prouve ; table reduite, voir plus bas)
//   assignTarget    puits le plus proche -> seuil      (nearestWell, buildingAccessPoint, pickBuildingAccessPoint)
//   moveActor       A* puis pas de marche              (Pathfinding deja porte ; pas de marche fidele, pilotage reduit)
//   act/perform     une seconde sur place, puis boit   (satisfyDrink, prouve)
//   effet           npc.thirst / hygiene / morale / health
//
// Pourquoi le puits et pas la maison ou le grenier : dans la reference, `drink`
// ne passe ni par l'interieur (`buildingForIndoorAction` n'a pas de branche
// drink), ni par une reservation, ni par un stock, ni par un metier. Le puits
// lui-meme n'a AUCUN etat propre : ce que la boucle modifie, ce sont les
// besoins de l'habitant. La maison exige le foyer et l'interieur, le grenier
// le registre des repas (438 lignes) et le grand livre des stocks.
//
// ECARTS DECLARES — ce qui n'est pas la reference, et pourquoi :
//
//  1. Table de decision reduite. La reference classe ~25 buts (metiers, faim,
//     repos, social...) et `drink` ne gagne que s'il les depasse tous. Seul
//     `drink` a sa boucle portee. Le reste de la table est remplace par UN
//     plancher declare, `UnportedGoalsFloor` : le meilleur score qu'aurait un
//     but non porte. Le score de `drink` lui-meme est celui de la reference
//     (needGoalScores + 6 si un puits existe), sans `goalNoise` : le bruit
//     consomme `sim.rng()` dans l'ordre de TOUTE la table, et un flux rng
//     partiel serait faux plus subtilement qu'un flux absent.
//  2. Pas de reconsideration aleatoire (`sim.rng() < chance`) : un habitant
//     qui a une cible la garde jusqu'a l'arrivee, l'echec ou la disparition.
//  3. Points d'acces sans intention urbaine : `urbanIntentAccessCandidates`
//     (sim/urban/intent.js, vague 5) n'est pas porte. On tient l'anneau 1
//     oriente vers le camp, qui est exactement le repli de la reference.
//  4. Pilotage reduit : pas de file de porte (crowdNav), pas d'hesitation, pas
//     de facteur de vitesse, pas de contournement local pendant l'attente d'un
//     chemin, escalade anti-blocage en trois paliers au lieu de resolveStuckActor.
//  5. Pas de cadence LOD : chaque habitant pense a chaque tick ou son horloge
//     `aiThinkAt` est echue (0,12 s, 0,045 s en besoin critique), comme un PNJ
//     proche du point de vue.
//  6. RemoveBuilding n'existe pas dans la reference — elle ne demolit jamais.
//     Voir la fonction : c'est une extension, et elle dit ce qu'elle garantit.
//
// Parite bit a bit : prouvee pour les fonctions de besoins seulement. La
// boucle assemblee est deterministe (meme entree -> meme empreinte), elle
// n'est PAS une reproduction de la trajectoire JS — la table reduite suffit a
// l'interdire.

#pragma once

#include "CoreMinimal.h"
#include "Life/AnastasisNeeds.h"
#include "World/AnastasisEntityTable.h"
#include "World/AnastasisNavGrid.h"
#include "World/AnastasisPathfinding.h"

namespace AnastasisWorld { struct FWorld; }

namespace AnastasisVillage
{
	using FPoint = AnastasisPath::FPoint;

	/** Types de batiment que ce portage sait poser. Chaine = vocabulaire de la reference. */
	inline const TCHAR* const WellType = TEXT("well");

	/** Buts. Seul `drink` a sa boucle ; `observer` est l'etat initial de createNpc. */
	inline const TCHAR* const GoalObserver = TEXT("observer");
	inline const TCHAR* const GoalDrink = TEXT("drink");

	/** `NPC_AI` de npc.js. */
	inline constexpr double ThinkEvery = 0.12;
	inline constexpr double ThinkEveryCritical = 0.045;

	/** Rayon de puisage autour du CENTRE d'un puits (`WELL_REACH`). */
	inline constexpr double WellReach = 2.5;
	/** Seuil de berge qui vaut point d'eau (`SHORE_REACH`). */
	inline constexpr double ShoreReach = 0.3;
	/** `reachedMoveTarget` : arrivee a moins de 0,75 tuile. */
	inline constexpr double ArrivalDistance = 0.75;

	/**
	 * ECART DECLARE n°1 — le meilleur score d'un but non porte.
	 *
	 * 42 = `urgeScore(thirstUrge)`, la valeur a laquelle la reference dit que la
	 * soif « deborde largement les scores de metier (souvent 40-90) ». Avec le
	 * bonus puits (+4 +6), `drink` depasse ce plancher exactement quand la soif
	 * atteint `thirstUrge` (40) : sous le seuil l'habitant vaque, au-dessus il va
	 * boire. Ce n'est pas une valeur de la reference ; c'est la place qu'occupent
	 * les ~24 buts qui ne sont pas encore portes.
	 */
	inline constexpr double UnportedGoalsFloor = 42.0;

	/** Enregistrement de batiment — les champs d'`addBuilding` que la boucle lit. */
	struct FBuilding
	{
		FString Id;
		FString Type;
		/** Case du batiment (entiere dans la reference ; le centre est x + 0,5). */
		double X = 0.0;
		double Y = 0.0;
		/** 1 = acheve. Un puits inacheve n'est ni compte, ni puise. */
		double Progress = 1.0;
		int32 CreatedDay = 0;
		/** Seuils persistes : le PNJ vise une porte, jamais le centre bloque. */
		TArray<FPoint> AccessPoints;

		bool IsCompleted() const { return Progress >= 1.0; }
	};

	/**
	 * Pourquoi un habitant a choisi ce qu'il fait — OBSERVATION, pas etat.
	 * N'entre pas dans l'empreinte ; sert au debug et aux tests.
	 */
	struct FDecisionTrace
	{
		double Time = -1.0;
		AnastasisNeeds::FNeedGoalScores NeedScores;
		/** Score de la ligne `drink` de adultScores, bonus puits compris. */
		double DrinkRowScore = 0.0;
		FString Winner;
		/** Batiment vise par ce choix, vide si aucun. */
		FString BuildingId;
		/** "well", "shore", "none" — d'ou vient la cible. */
		FString TargetSource;
	};

	/** Habitant — les champs de createNpc que la boucle du puits lit ou ecrit. */
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

		/** Nombre d'actes `drink` accomplis. Observation (markDrink compte ailleurs). */
		int32 DrinksTaken = 0;
		FDecisionTrace LastDecision;
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
		 * Garanties : la case redevient ce que le terrain dit (eau bloquee, sol
		 * libre), la version de navigation change (tous les chemins sont
		 * recalcules), et AUCUN habitant ne garde une reference vers lui — ceux
		 * qui le visaient perdent cible, chemin et but, et redecident a leur
		 * prochaine pensee. Les seuils des autres batiments sont recalcules au
		 * prochain usage (`ensureBuildingAccessPoints` filtre les seuils morts).
		 */
		bool RemoveBuilding(const FString& Id);

		/** `spawnNpc`, reduit : position, besoins et vitesse fournis, identifiant `npc-N`. */
		FString SpawnNpc(double InX, double InY, const AnastasisNeeds::FNeeds& Needs, double Speed = 4.0);

		/** `actors.splice` — decalage, l'ordre des autres survit. */
		bool RemoveNpc(const FString& Id);

		/** `for (const npc of this.actors) updateNpc(this, npc, dt)` — Time = temps de sim APRES avance. */
		void UpdateActors(double Time, double Dt);

		/** `countBuildings(type)` — acheves seulement. */
		int32 CountBuildings(const FString& Type) const;

		const TArray<FBuilding>& GetBuildings() const { return Buildings.GetItems(); }
		const TArray<FNpc>& GetActors() const { return Actors.GetItems(); }
		const FBuilding* FindBuilding(const FString& Id) const { return Buildings.FindById(Id); }
		const FNpc* FindNpc(const FString& Id) const { return Actors.FindById(Id); }
		/** Acces d'ecriture pour les tests et les outils de debug (besoins imposes). */
		FNpc* FindNpcMutable(const FString& Id) { return Actors.FindById(Id); }

		const AnastasisNav::FNavGrid& GetNavGrid() const { return Nav; }
		int32 GetNavVersion() const { return NavVersion; }

		/** `footBlockedAt(floor(x), floor(y))`. */
		bool IsFootBlocked(double InX, double InY) const;

		/** `atDrinkSpot` : eau, berge, ou moins de 2,5 tuiles du centre d'un puits acheve. */
		bool AtDrinkSpot(double InX, double InY) const;

		/**
		 * Habitants qui se servent de ce batiment en ce moment : ceux qui le visent
		 * (`destBuildingId`) pour boire. Derive, jamais stocke — la reference n'a
		 * pas de liste d'occupants, et une liste stockee serait une seconde verite.
		 */
		TArray<FString> UsersOf(const FString& BuildingId) const;

		/** Projection canonique (FStateWriter) : batiments puis acteurs, dans l'ordre. */
		uint64 Digest() const;

	private:
		void UpdateNpc(FNpc& Npc, double Time, double Dt);
		void ChooseGoal(FNpc& Npc, double Time);
		void Act(FNpc& Npc, double Dt);
		bool Perform(FNpc& Npc);
		void RedirectAfterFailure(FNpc& Npc);

		const FBuilding* NearestWell(double FromX, double FromY) const;
		bool DrinkTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource);
		bool BuildingAccessPoint(FBuilding& Building, FNpc* Actor, FPoint& Out, const FPoint* Exclude = nullptr);
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
		TAnastasisEntityTable<FBuilding> Buildings;
		TAnastasisEntityTable<FNpc> Actors;
	};

	/** `aiThinkStagger` — decalage FNV-1a de l'identifiant, dans [0, thinkEvery). */
	ANASTASISSIM_API double AiThinkStagger(const FString& Id);

	/** `needsCritical`. */
	ANASTASISSIM_API bool NeedsCritical(const AnastasisNeeds::FNeeds& Needs);
}
