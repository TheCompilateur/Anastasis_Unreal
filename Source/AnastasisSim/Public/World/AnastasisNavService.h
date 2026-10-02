// Service de navigation — file de requetes budgetee et cache partage de chemins.
//
// Portage de `src/sim/navService.js`, plus les morceaux de `src/sim/navGrid.js`
// dont il a besoin et que la couche terrain n'avait pas pris: les metriques de
// navigation (`createNavMetrics`), l'anneau de trace (`recordNavTransition`,
// `navTraceSnapshot`) et la cle de cible (`navigationTargetKey`).
//
// MODULE SEUL. Le village ne l'appelle pas encore: il cherche toujours ses
// chemins par `AnastasisPath::FindPath` directement. Le branchement (dans
// `FVillage::UpdateNpc`, `beginNavTick` + deux `processNavQueue` autour de la
// boucle des PNJ, comme `simulation.js` le fait) est une mission a part.
//
// --- CE QUI DECIDE, ET QU'IL NE FAUT PAS "AMELIORER" --------------------------
//
// 1. Le cache est une `Map` JavaScript: ORDONNEE par insertion, et `set` sur une
//    cle existante garde sa place. Le garde-fou memoire efface les 80 PREMIERES
//    cles quand la table depasse 480. Un `TMap` n'a pas d'ordre: il effacerait
//    d'autres chemins, et le PNJ suivant recevrait un chemin different. D'ou
//    `FNavCache`, qui tient l'ordre expres.
//
// 2. Le cache de ZONE sert un chemin calcule pour un autre depart (meme case
//    8x8, premier noeud a 9 cases au plus). C'est approximatif, c'est voulu, et
//    c'est ce qui rend le cache observable: le chemin servi depend de QUI a
//    demande en premier. La sauvegarde JS emporte d'ailleurs le cache pour cette
//    raison (`navCache`, voir `save.js` et RESUME_CAUSALITY_001).
//
// 3. Le budget est un NOMBRE de calculs A*, jamais une duree. Couper au
//    chronometre ferait dependre la simulation de la machine.
//
// 4. Le tri de la file est STABLE (`Array.prototype.sort` l'est depuis V8 7.0):
//    deux requetes de meme priorite et de meme date gardent leur ordre d'arrivee.
//
// 5. Un chemin VIDE n'est pas un echec pour `findPath` (depart = arrivee), mais
//    `applyPathToActor` le range comme `null`, donc comme un echec. Le cache, lui,
//    le garde et le sert (`exact?.path` est vrai pour `[]`), et `requestPath`
//    rend alors un resultat « non nul ». Copie fidele, bizarrerie comprise.
//
// --- CE QUI N'EST PAS ICI ------------------------------------------------------
//
// - `job.actor`, la reference d'objet: le C++ garde l'identifiant et demande
//   l'acteur vivant a l'hote a chaque resolution. Le JS fait de meme des que
//   l'objet n'est plus dans `sim.actors`; tant qu'il y est, l'objet et la
//   recherche par identifiant designent le meme acteur (identifiants uniques).
// - `actor.navigation.path` / `pathIndex`: dans le JS, des copies de
//   `actor.path` / `actor.pathStep` resynchronisees a chaque ecriture
//   (`syncNavigationFromActor`). Le C++ n'a qu'un stockage.
// - Le journal `ARRIVAL` de l'anneau de trace: l'anneau est porte entier, mais
//   seul le service de navigation l'alimente pour l'instant.

#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisPathfinding.h"

namespace AnastasisNavService
{
	using FPoint = AnastasisPath::FPoint;

	/** Taille de zone pour mutualiser les departs proches (cases). */
	inline constexpr int32 NavZone = 8;

	/** Duree de vie du cache en temps sim (s) a 1x, en plus de `navVersion`. */
	inline constexpr double NavCacheTtl = 20.0;

	/** Plafond du TTL sim (s). A 10x: 10 x 10 = 100. */
	inline constexpr double NavCacheTtlMax = 200.0;

	/** Sweep complet du cache au plus toutes N secondes sim a 1x. */
	inline constexpr double NavCacheSweepInterval = 4.0;

	/** Plafond A* par fat step (calculs). */
	inline constexpr int32 NavPathBudgetMaxCalcs = 36;

	/** Garde-fou memoire du cache: au-dela, on efface les plus anciennes cles. */
	inline constexpr int32 NavCacheMaxEntries = 480;
	inline constexpr int32 NavCacheEvictCount = 80;

	/** Capacite de l'anneau de trace (`NAV_TRACE_CAP`). */
	inline constexpr int32 NavTraceCap = 64;

	/** `NAV_PRIORITY`. Plus petit = plus urgent. */
	namespace Priority
	{
		inline constexpr int32 Urgent = 0;
		inline constexpr int32 High = 1;
		inline constexpr int32 Normal = 2;
		inline constexpr int32 Low = 3;
	}

	// --- Fonctions pures --------------------------------------------------------

	/** `navStepMultForSpeed` — multiplicateur de pas fat-step, >= 1. */
	ANASTASISSIM_API double NavStepMultForSpeed(double SpeedScale = 1.0);

	/** `navCacheTtlForSpeed` — TTL du cache en secondes sim. */
	ANASTASISSIM_API double NavCacheTtlForSpeed(double SpeedScale = 1.0);

	/** `navCacheSweepIntervalForSpeed` — intervalle de sweep amorti. */
	ANASTASISSIM_API double NavCacheSweepIntervalForSpeed(double SpeedScale = 1.0);

	/** `pathBudgetForSpeed(...).maxCalcs` — calculs A* par tick. */
	ANASTASISSIM_API int32 PathBudgetForSpeed(double SpeedScale = 1.0);

	/**
	 * `cacheKeyFor` — `"sx,sy:tx,ty:v"`, ou `"zX,Y:tx,ty:v"` en zone.
	 * Les coordonnees passent par `Math.floor` et s'ecrivent comme JS ecrit un
	 * nombre (`-0` devient `0`, NaN devient `NaN`).
	 */
	ANASTASISSIM_API FString CacheKeyFor(const FPoint& Start, const FPoint& Target, int32 NavVersion, bool bZone);

	/**
	 * `navigationTargetKey` — `"x,y"` des cases, ou chaine VIDE la ou le JS rend
	 * `null` (coordonnee non finie). Le JS ne rend jamais `""`: la chaine vide
	 * peut donc tenir le role de `null` sans ambiguite.
	 */
	ANASTASISSIM_API FString NavigationTargetKey(const FPoint& Target);
	ANASTASISSIM_API FString NavigationTargetKey(const FPoint* Target);

	/**
	 * `priorityForGoal`. `Energy` suit `actor.energy || 100`: 0 compte comme 100.
	 */
	ANASTASISSIM_API int32 PriorityForGoal(const FString& Goal, double Hunger, double Thirst, double Energy);

	// --- Metriques et anneau de trace (navGrid.js) ------------------------------

	/** Une entree de l'anneau (`recordNavTransition`). Chaine vide = `null`. */
	struct FNavTraceSample
	{
		FString Type;
		double Time = 0.0;
		FString ActorId;
		FString Goal;
		FString TargetKey;
		int32 NavVersion = 0;
		FString From;
		FString To;
		FString Reason;
		/** < 0 = `null`. */
		int32 PathLength = -1;
		int32 QueueDepth = 0;
	};

	/**
	 * `createNavMetrics` — seuls les compteurs que la navigation ecrit
	 * aujourd'hui. Les compteurs des PNJ (`stuck*`, `pathDoorWaits`...) suivront
	 * le portage de la marche.
	 */
	struct ANASTASISSIM_API FNavMetrics
	{
		int32 PathRequests = 0;
		int32 PathHits = 0;
		int32 PathFails = 0;
		int32 CacheHits = 0;
		int32 CacheMisses = 0;
		int32 CacheSize = 0;
		int32 QueueDepth = 0;
		int32 QueueSolved = 0;
		int32 CalcsThisTick = 0;
		int32 PathQueueEnqueued = 0;
		int32 PathCacheHits = 0;

		TArray<FNavTraceSample> NavTrace;
		int32 NavTraceNext = 0;
		int32 NavTraceTotal = 0;
		/** `navLastTransitionKey` — ne sert qu'au dedoublonnage des `ARRIVAL`. */
		FString NavLastTransitionKey;

		/** `navTraceSnapshot` — l'anneau dans l'ordre chronologique. */
		TArray<FNavTraceSample> TraceSnapshot() const;
	};

	// --- L'acteur vu par le service ---------------------------------------------

	/**
	 * Les champs d'un acteur que le service lit ou ecrit, et rien d'autre.
	 *
	 * Le chemin vide tient le role de `actor.path = null`: le service n'y ecrit
	 * jamais un tableau vide (`applyPathToActor` range `[]` comme `null`).
	 * `NavTargetKey` vide = `navigation.targetKey = null`.
	 */
	struct FNavAgent
	{
		FString Id;
		double X = 0.0;
		double Y = 0.0;
		FString Goal;
		double Hunger = 0.0;
		double Thirst = 0.0;
		double Energy = 100.0;
		/** `actor.target` — sert a jeter une requete dont la cible a change. */
		bool bHasTarget = false;
		FPoint Target;

		TArray<FPoint> Path;
		int32 PathStep = 0;
		bool bHasPathGoal = false;
		FPoint PathGoal;
		bool bPathFailed = false;
		int32 PathFailStreak = 0;
		double PathCooldown = 0.0;

		/** `actor.navigation`. */
		FString NavTargetKey;
		double NavRequestedAt = 0.0;
		int32 NavVersion = -1;

		/**
		 * Compte les `applyPathToActor` (nav-service-001) : l'hote sait ainsi qu'un chemin a ete pose,
		 * donc que `syncNavigationFromActor` a recopie `navigation.path` / `pathIndex`.
		 */
		int32 ApplyCount = 0;
	};

	/**
	 * Le monde vu par le service: ce que le JS lit sur `sim`.
	 */
	class ANASTASISSIM_API INavServiceHost
	{
	public:
		virtual ~INavServiceHost() = default;
		/** `sim.time`. */
		virtual double GetTime() const = 0;
		/** `sim.speedScale` (0 et NaN valent 1, comme `|| 1`). */
		virtual double GetSpeedScale() const = 0;
		/** `sim.navVersion | 0`. */
		virtual int32 GetNavVersion() const = 0;
		/** Ce que `findPath(sim, ...)` interroge. */
		virtual const AnastasisPath::INavSource& GetNavSource() const = 0;
		/** `sim.actors.find(a => a.id === id)` — nullptr si l'acteur n'est plus la. */
		virtual FNavAgent* FindLiveAgent(const FString& Id) = 0;
		/** `sim.navMetrics` — nullptr si la simulation n'en tient pas. */
		virtual FNavMetrics* GetMetrics() { return nullptr; }
	};

	/** `recordNavTransition`. Rend false si rien n'a ete enregistre. */
	struct FNavTransitionEvent
	{
		const TCHAR* Type = TEXT("NAV_TRANSITION");
		const FNavAgent* Actor = nullptr;
		const FPoint* Target = nullptr;
		const TCHAR* From = nullptr;
		const TCHAR* To = nullptr;
		const TCHAR* Reason = nullptr;
		/** < 0 = absent. */
		int32 PathLength = -1;
	};

	// --- Le cache ---------------------------------------------------------------

	/** Une entree du cache. Le chemin n'est jamais mute apres calcul. */
	struct FNavCacheEntry
	{
		TArray<FPoint> Path;
		int32 NavVersion = 0;
		double StoredAt = 0.0;
	};

	/**
	 * Table ordonnee a la semantique d'une `Map` JavaScript: iteration dans
	 * l'ordre d'insertion, `Set` sur une cle existante garde sa place, `Remove`
	 * decale. Quelques centaines d'entrees: le decalage lineaire est le prix de
	 * l'ordre, et l'ordre est observable (voir l'en-tete du fichier).
	 */
	class ANASTASISSIM_API FNavCache
	{
	public:
		int32 Num() const { return Keys.Num(); }
		const FNavCacheEntry* Find(const FString& Key) const;
		void Set(const FString& Key, const FNavCacheEntry& Entry);
		bool Remove(const FString& Key);
		void Empty();

		/** Cles dans l'ordre d'insertion. */
		const TArray<FString>& GetKeys() const { return Keys; }
		const FNavCacheEntry& GetEntry(int32 Index) const { return Entries[Index]; }

		/** Retire les entrees qui ne passent pas `Keep`, ordre conserve. Rend le nombre retire. */
		int32 RemoveIf(TFunctionRef<bool(const FNavCacheEntry&)> ShouldRemove);
		/** Retire les N premieres entrees. */
		void RemoveFirst(int32 Count);

	private:
		void RebuildIndex();

		TArray<FString> Keys;
		TArray<FNavCacheEntry> Entries;
		TMap<FString, int32> IndexByKey;
	};

	// --- La file -----------------------------------------------------------------

	struct FNavJob
	{
		FString ActorId;
		FPoint Start;
		FPoint Destination;
		/** `navigationTargetKey(target)`; vide = `null`. */
		FString GoalKey;
		int32 Priority = Priority::Normal;
		bool bAllowBlockedTarget = false;
		double RequestedAt = 0.0;
	};

	/** Options de `requestPath`. Les `TOptional` vides valent `undefined`. */
	struct FRequestOptions
	{
		TOptional<int32> Priority;
		TOptional<bool> bAllowBlockedTarget;
	};

	/** Options de `processNavQueue`. */
	struct FProcessOptions
	{
		/** `favorActorId` — vide = absent. */
		FString FavorActorId;
		/** `maxJobs` — < 0 = `Infinity`. */
		int32 MaxJobs = -1;
	};

	/** `createNavService()` — l'etat que la simulation garde dans `sim.navService`. */
	struct ANASTASISSIM_API FNavService
	{
		TArray<FNavJob> Queue;
		FNavCache Cache;
		/** `pendingByActor` — identifiant -> rang dans `Queue`. Peut etre perime, comme en JS. */
		TMap<FString, int32> PendingByActor;
		int32 CalcThisTick = 0;
		int32 MaxCalcs = 6;
		double CacheTtl = NavCacheTtl;
		double LastSweepAt = -999.0;

		FNavService();

		/** `beginNavTick(sim, { budgetMul })`. */
		void BeginNavTick(INavServiceHost& Host, double BudgetMul = 1.0);

		/** `sweepNavCache` — rend le nombre d'entrees retirees. */
		int32 SweepNavCache(INavServiceHost& Host);

		/**
		 * `lookupCachedPath` — exact, puis zone. Rend false la ou le JS rend
		 * `null`. Le chemin rendu peut etre vide (voir l'en-tete, point 5).
		 */
		bool LookupCachedPath(INavServiceHost& Host, const FPoint& Start, const FPoint& Target, TArray<FPoint>& OutPath);

		/** `storeCachedPath` — sous la cle exacte ET la cle de zone. */
		void StoreCachedPath(INavServiceHost& Host, const FPoint& Start, const FPoint& Target, const TArray<FPoint>& Path);

		/**
		 * `requestPath` — cache immediat, sinon file, et resolution de CE job si
		 * le budget le permet. Rend true la ou le JS rend un chemin (non `null`),
		 * et le copie dans `OutPath` si fourni.
		 */
		bool RequestPath(
			INavServiceHost& Host,
			FNavAgent& Actor,
			const FPoint& Target,
			const FRequestOptions& Options = FRequestOptions(),
			TArray<FPoint>* OutPath = nullptr);

		/** `processNavQueue` — rend le nombre de jobs resolus par un A*. */
		int32 ProcessNavQueue(INavServiceHost& Host, const FProcessOptions& Options = FProcessOptions());

		/** `clearNavCache`. */
		void ClearNavCache(INavServiceHost& Host);

		/**
		 * Restauration de `data.navCache` (`save.js`): les entrees d'une autre
		 * `navVersion` sont ecartees, l'ordre des autres est garde.
		 */
		void RestoreCache(const TArray<TPair<FString, FNavCacheEntry>>& SavedEntries, int32 NavVersion);

	private:
		bool BudgetExhausted() const { return CalcThisTick >= MaxCalcs; }
		bool ResolveNavJob(INavServiceHost& Host, FNavJob& Job);
		void RemoveJobFromQueue(int32 JobIndex);
	};

	/** `applyPathToActor`. `bFromCache` = `source === "cache"`. */
	ANASTASISSIM_API void ApplyPathToActor(
		INavServiceHost& Host,
		const FNavService& Service,
		FNavAgent& Actor,
		const FPoint& Target,
		const TArray<FPoint>& Path,
		bool bFromCache);

	/** `recordNavTransition` — preuve seulement, ne touche jamais au gameplay. */
	ANASTASISSIM_API bool RecordNavTransition(
		INavServiceHost& Host,
		const FNavService& Service,
		const FNavTransitionEvent& Event);
}
