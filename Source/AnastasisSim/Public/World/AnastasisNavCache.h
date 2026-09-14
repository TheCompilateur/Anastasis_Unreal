// Cache de chemins A* — moitié cache de `src/sim/navService.js`.
//
// --- POURQUOI LE CACHE AVANT LA FILE ----------------------------------------
//
// Un cache de chemins ressemble à une optimisation. Ce n'en est pas une, et
// `src/sim/save.js` l'écrit noir sur blanc à propos du champ `navCache` :
//
//   « un acteur repris avec un cache vide peut donc recevoir un chemin
//     différent — même coût, mêmes règles — de celui qu'une partie continue
//     aurait servi depuis son propre cache encore chaud. Sans ce champ, la
//     trajectoire divergeait dès la première requête de chemin post-reprise. »
//
// Un cache qui ne sert pas les mêmes chemins fait marcher les habitants
// ailleurs. Il appartient à la causalité.
//
// --- LE PIÈGE DE L'ORDRE ----------------------------------------------------
//
// L'éviction retire **les 80 premières clés dans l'ordre d'insertion** — c'est
// l'ordre d'itération d'une `Map` JavaScript. `TMap` n'a pas d'ordre : deux
// exécutions pourraient évincer des entrées différentes, donc servir des
// chemins différents, donc envoyer des PNJ ailleurs. D'où l'ordre tenu à la
// main, dans `Order`.
//
// Deux détails de la `Map` JS que cette classe reproduit :
//   - réécrire une clé existante NE la déplace PAS en fin d'ordre ;
//   - supprimer une clé conserve l'ordre relatif des autres.
//
// --- CE QUI N'EST PAS ICI ---------------------------------------------------
//
// La file budgétée (`requestPath`, `processNavQueue`, `applyPathToActor`) ne
// manipule que des acteurs. Elle suivra les acteurs.

#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisPathfinding.h"

namespace AnastasisNavCache
{
	/** Taille de zone pour mutualiser les départs proches (cases). */
	inline constexpr int32 NavZone = 8;

	/** Durée de vie en temps sim (secondes) à 1×, au-delà de `NavVersion`. */
	inline constexpr double CacheTtl = 20.0;

	/** Plafond de TTL sim. À 10× : 10 × 20 = 200 — même durée mur qu'à 1×. */
	inline constexpr double CacheTtlMax = 200.0;

	/** Balayage complet au plus toutes N secondes sim à 1×. */
	inline constexpr double SweepInterval = 4.0;

	/** Plafond d'A* par fat-step. En calculs, jamais en durée. */
	inline constexpr int32 PathBudgetMaxCalcs = 36;

	/** Au-delà, on évince les `EvictBatch` plus anciennes clés insérées. */
	inline constexpr int32 EvictAbove = 480;
	inline constexpr int32 EvictBatch = 80;

	/** Multiplicateur de fat-step, source unique du TTL et du budget. */
	ANASTASISSIM_API double StepMultForSpeed(double SpeedScale = 1.0);

	/** TTL en secondes sim ; suit le fat-step pour une durée mur ~stable. */
	ANASTASISSIM_API double CacheTtlForSpeed(double SpeedScale = 1.0);

	/** Intervalle de balayage amorti, aligné sur le TTL. */
	ANASTASISSIM_API double SweepIntervalForSpeed(double SpeedScale = 1.0);

	/**
	 * Budget d'A* par tick, **en nombre de calculs**.
	 *
	 * La règle vient de la référence et elle est délibérée : l'horloge peut
	 * choisir la quantité de simulation, elle ne doit jamais changer son
	 * résultat via un coupe-durée. Un budget en millisecondes rendrait le monde
	 * dépendant de la machine.
	 */
	ANASTASISSIM_API int32 PathBudgetForSpeed(double SpeedScale = 1.0);

	/**
	 * Clé de cache. Sa forme exacte fait partie du contrat : deux portages qui
	 * la composeraient autrement ne partageraient jamais une entrée.
	 */
	ANASTASISSIM_API FString CacheKeyFor(
		const AnastasisPath::FPoint& Start,
		const AnastasisPath::FPoint& Target,
		int32 NavVersion,
		bool bZone);

	struct FEntry
	{
		TArray<AnastasisPath::FPoint> Path;
		int32 NavVersion = 0;
		double StoredAt = 0.0;
	};

	/** Fraîcheur : bonne version, et pas au-delà du TTL. */
	ANASTASISSIM_API bool IsEntryFresh(const FEntry& Entry, int32 NavVersion, double Time, double Ttl);

	/**
	 * Le cache, avec l'ordre d'insertion d'une `Map` JavaScript.
	 *
	 * `Lookup` n'est pas une lecture pure : il SUPPRIME les entrées périmées
	 * qu'il rencontre, comme la référence. Le taire rendrait la taille du cache
	 * différente au coup d'après, donc l'éviction différente.
	 */
	class ANASTASISSIM_API FCache
	{
	public:
		int32 Num() const { return Order.Num(); }
		const TArray<FString>& GetOrder() const { return Order; }

		/** Range le chemin sous sa clé exacte ET sa clé de zone, puis évince. */
		void Store(
			const AnastasisPath::FPoint& Start,
			const AnastasisPath::FPoint& Target,
			int32 NavVersion,
			double Time,
			const TArray<AnastasisPath::FPoint>& Path);

		/**
		 * Cherche exact, puis par zone. Un résultat de zone n'est servi que si
		 * le départ est assez proche du premier nœud du chemin — sinon
		 * l'habitant partirait en marche arrière.
		 */
		bool Lookup(
			const AnastasisPath::FPoint& Start,
			const AnastasisPath::FPoint& Target,
			int32 NavVersion,
			double Time,
			double Ttl,
			TArray<AnastasisPath::FPoint>& OutPath);

		/** Expire les entrées périmées. Rend le nombre retiré. */
		int32 Sweep(int32 NavVersion, double Time, double Ttl);

		void Clear();

		/** Pour les tests et le diagnostic : l'entrée brute sous une clé. */
		const FEntry* Find(const FString& Key) const;

	private:
		void Set(const FString& Key, const TSharedRef<FEntry>& Entry);
		void RemoveKey(const FString& Key);
		void EvictIfNeeded();

		/** L'ordre d'insertion. Il fait partie de l'état, pas de la mise en œuvre. */
		TArray<FString> Order;
		TMap<FString, TSharedRef<FEntry>> Entries;
	};
}
