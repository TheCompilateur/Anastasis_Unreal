// Memoire des regions et cible d'exploration — portage de `markCell` / `cellIndex` /
// `exploreTarget` (`src/ai/memory.js`) et de `Simulation.randomWalkTarget`
// (`src/sim/simulation.js`), mission perception-explore-001.
//
// Ce qui decide :
//
// - `exploreTarget` TIRE dans le flux partage `sim.rng` : deux tirages par essai
//   (colonne puis rangee), jusqu'a 14 essais ; un essai sur une region DEJA CONNUE
//   (`mind.cells`) ou sur une case bloquee est rate, mais ses deux tirages sont
//   consommes. Quatorze rates : `randomWalkTarget`, encore deux tirages par essai,
//   jusqu'a 16 essais, puis le centre du village. Le nombre de tirages depend donc de
//   la memoire et du terrain : c'est ce qui interdit de brancher les bruits de la
//   table de decision sans lui (releve `docs/migration/phase3/P3_RNG_RELEVE.md`).
// - `exploreTarget` teste `sim.isBlocked` (eau, bati), `randomWalkTarget`
//   `sim.isFootBlocked` (plus les arbres debout) : ce ne sont pas les memes cases.
// - `markCell` recoit des coordonnees DEJA arrondies (`Math.floor(npc.x)`).

#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisPathfinding.h"

struct FAnastasisRng;

namespace AnastasisExplore
{
	using FPoint = AnastasisPath::FPoint;

	/** `PERCEPTION.cellSize` — taille d'une region memorisee. */
	inline constexpr int32 CellSize = 8;
	/** Essais de `exploreTarget` et de `randomWalkTarget`. */
	inline constexpr int32 ExploreTries = 14;
	inline constexpr int32 RandomWalkTries = 16;

	/** `cellIndex(sim, x, y)` — -1 hors du monde. */
	ANASTASISSIM_API int32 CellIndex(int32 W, int32 H, int32 X, int32 Y);

	/** `markCell` : rend true si la region etait inconnue (et la marque). */
	ANASTASISSIM_API bool MarkCell(TSet<int32>& Cells, int32& CellCount, int32 W, int32 H, int32 X, int32 Y);

	/** Le monde que l'exploration interroge. */
	struct FExploreWorld
	{
		int32 W = 0;
		int32 H = 0;
		/** `sim.isBlocked(x, y)` — `blockedAt(floor(x), floor(y))`. */
		TFunction<bool(double, double)> IsBlocked;
		/** `sim.isFootBlocked(x, y)`. */
		TFunction<bool(double, double)> IsFootBlocked;
		/** `sim.settlement` — le repli de `randomWalkTarget`. */
		FPoint Settlement;
	};

	/** Combien de tirages, et par quel chemin, une cible a coute. */
	struct FExploreResult
	{
		FPoint Point;
		int32 Draws = 0;
		bool bRandomWalk = false;
		bool bSettlement = false;
	};

	/** `randomWalkTarget(actor)`. */
	ANASTASISSIM_API FExploreResult RandomWalkTarget(const FExploreWorld& World, FAnastasisRng& Rng, double X, double Y);

	/** `exploreTarget(sim, npc)`. */
	ANASTASISSIM_API FExploreResult ExploreTarget(
		const FExploreWorld& World,
		FAnastasisRng& Rng,
		double X,
		double Y,
		const TSet<int32>& KnownCells);
}
