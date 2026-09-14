// Grille de navigation — coûts de terrain et blocage au pied.
//
// Portage de `src/sim/navGrid.js` (couche terrain seule) et des accesseurs de
// `simulation.js` dont le pathfinding depend: `blockedAt`, `footBlockedAt`,
// `tileTraversalCost`.
//
// Ce qui n'est PAS ici, et pourquoi: les metriques de navigation, l'anneau de
// trace, les points d'acces des batiments et la memoire de navigation des PNJ.
// Les points d'acces dependent de `src/sim/urban/intent.js` (chantier
// urbanisme, vague 5) et les trois autres de structures d'acteurs qui n'existent
// pas encore. Porter la couche terrain seule donne un morceau qui se prouve
// entierement aujourd'hui; le reste suivra ses systemes.
//
// --- LE PIEGE DU FLOAT ------------------------------------------------------
//
// `MoveCost` est un tableau de `float`, alors que le contrat du module interdit
// le float. Ce n'est pas un oubli et ce n'est pas une optimisation: la
// reference stocke ses multiplicateurs dans un `Float32Array`
// (`rebuildMoveCosts`). Un `double` ici serait PLUS PRECIS, donc FAUX — le
// produit `baseCost * mult` ne tomberait pas sur les memes bits, et deux
// chemins de cout voisin basculeraient l'un dans l'autre.
//
// La regle du module reste entiere: tout ce qui n'est pas une copie fidele
// d'un stockage f32 de la reference se calcule en double.

#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWorld.h"

#include <limits>

namespace AnastasisNav
{
	/** Multiplicateurs relatifs (x cout ortho A* = 10). */
	namespace TerrainMoveCost
	{
		inline constexpr double Road = 0.65;
		inline constexpr double Grass = 1.0;
		inline constexpr double Field = 1.15;
		inline constexpr double Scrub = 1.28;
		inline constexpr double Forest = 1.6;
		inline constexpr double Stone = 1.35;
		inline constexpr double Ruin = 1.45;
		inline constexpr double Marsh = 2.4;
	}

	/** Wetness au-dela de laquelle une prairie compte comme marecage. */
	inline constexpr double MarshWetness = 0.72;

	/** Plancher de cout d'une case, applique par `tileTraversalCost`. */
	inline constexpr double MinTraversalCost = 3.0;

	/**
	 * L'infini de JavaScript, pas une grande valeur sentinelle.
	 *
	 * `Float32Array` stocke l'infini tel quel, et la reference teste
	 * `Number.isFinite`. Une sentinelle du genre `DBL_MAX` obligerait a se
	 * souvenir du seuil partout, et se ferait oublier quelque part.
	 */
	inline constexpr double Infinity = std::numeric_limits<double>::infinity();

	/**
	 * Peuplement forestier = obstacle au pied (PNJ, joueur, charrettes).
	 * Scrub et souches restent traversables; la faune, elle, ne regarde que
	 * l'eau et le bati.
	 */
	ANASTASISSIM_API bool IsStandingTreeTile(const AnastasisWorld::FTile& Tile);

	/**
	 * Multiplicateur de terrain d'une case. `RoadPathCost` correspond au
	 * parametre optionnel `roadPathCost` du JS: quand une case porte une route,
	 * son profil impose son propre cout, plancher a 0.35.
	 */
	ANASTASISSIM_API double TerrainMoveCostOf(
		const AnastasisWorld::FTile* Tile,
		const double* RoadPathCost = nullptr);

	/**
	 * L'etat de navigation d'un monde: ce que la simulation tient a jour a cote
	 * des tuiles.
	 *
	 * `Blocked` reproduit `sim.blocked`: l'eau a la construction, les batiments
	 * ensuite. Le portage des batiments (vague 5) remplira la seconde moitie;
	 * en attendant, une case posee ici a la main suffit a tester.
	 */
	struct ANASTASISSIM_API FNavGrid
	{
		int32 W = 0;
		int32 H = 0;
		TArray<uint8> Blocked;
		TArray<float> MoveCost;

		bool IsInBounds(int32 X, int32 Y) const
		{
			return X >= 0 && Y >= 0 && X < W && Y < H;
		}
	};

	/** Alloue la grille et marque l'eau, comme le fait `rebuildIndexes`. */
	ANASTASISSIM_API void InitFromWorld(FNavGrid& Grid, const AnastasisWorld::FWorld& World);

	/** `rebuildMoveCosts` — cases bloquees a l'infini, sinon le terrain. */
	ANASTASISSIM_API void RebuildMoveCosts(FNavGrid& Grid, const AnastasisWorld::FWorld& World);

	/** `blockedAt` — hors bornes compte comme bloque. */
	ANASTASISSIM_API bool BlockedAt(const FNavGrid& Grid, int32 X, int32 Y);

	/** `footBlockedAt` — eau, bati, plus les arbres debout. */
	ANASTASISSIM_API bool FootBlockedAt(
		const FNavGrid& Grid,
		const AnastasisWorld::FWorld& World,
		int32 X,
		int32 Y);

	/** `moveCostAt` — multiplicateur relu depuis le stockage f32. */
	ANASTASISSIM_API double MoveCostAt(const FNavGrid& Grid, int32 X, int32 Y);

	/**
	 * `tileTraversalCost` — cout reel d'un pas.
	 *
	 * La congestion des transports (`congestionPathMul`) n'est pas portee: elle
	 * appartient au chantier transport, vague 5. Sa place est marquee ici pour
	 * que le jour venu on l'ajoute au bon endroit plutot qu'a cote.
	 */
	ANASTASISSIM_API double TileTraversalCost(const FNavGrid& Grid, int32 X, int32 Y, double BaseCost);
}
