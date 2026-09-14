// A* deterministe sur la grille du monde — portage de `src/sim/pathfinding.js`.
//
// PORTAGE.md pose l'exigence en une phrase: « l'ordre d'exploration de l'A* doit
// etre identique : deux chemins de meme cout, et les PNJ ne prennent pas la
// meme rue ». Un chemin "aussi bon" n'est pas un chemin bon.
//
// Ce qui rend le chemin reproductible est LE COMPARATEUR, et lui seul: f, puis
// h, puis y, puis x. Comme (y, x) identifie une case, et que deux entrees d'une
// meme case ont des g distincts — on ne pousse que strictement mieux — aucune
// egalite n'est possible. La suite des `pop` est donc entierement determinee,
// quel que soit le tas et quel que soit l'ordre des poussees.
//
// Corollaire mesure, pas suppose: echanger deux voisins dans `NEIGHBORS` ne
// change aucun chemin. La batterie de vecteurs ne le detecte pas, et c'est
// correct — il n'y a rien a detecter. Retirer le departage par `h`, en
// revanche, casse `Anastasis.Sim.Parite.NavChemin` immediatement. Les deux ont
// ete verifies par mutation le 2026-09-13.
//
// L'ordre des voisins est malgre tout copie tel quel: il ne coute rien, et une
// copie fidele se relit contre la reference sans avoir a refaire ce
// raisonnement.
//
// LES ENTREES PERIMEES NE SONT PAS RETIREES du tas. Le JS pousse un nouveau
// noeud et laisse l'ancien pourrir, puis l'ecarte au `pop` par une comparaison
// d'identite (`records.get(key) !== current`). Le portage garde ce
// fonctionnement: nettoyer le tas changerait le nombre d'expansions, donc la
// rencontre du budget `maxExpanded`, donc le resultat.

#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisNavGrid.h"

namespace AnastasisWorld { struct FWorld; }

namespace AnastasisPath
{
	inline constexpr double StraightCost = 10.0;
	inline constexpr double DiagonalCost = 14.0;

	/**
	 * Budgets larges: un village densifie force des detours longs autour des
	 * pates de batiments. Trop bas, le chemin est nul, et le PNJ retombe sur la
	 * marche droite dans les murs.
	 */
	inline constexpr double DefaultMaxCost = 320.0;
	inline constexpr int32 DefaultMaxExpanded = 3200;

	struct FPoint
	{
		double X = 0.0;
		double Y = 0.0;
	};

	struct FOptions
	{
		double MaxCost = DefaultMaxCost;
		int32 MaxExpanded = DefaultMaxExpanded;
		bool bAllowBlockedStart = true;
		bool bAllowBlockedTarget = false;
	};

	/**
	 * Le monde vu par le pathfinder: trois questions, pas une de plus.
	 *
	 * La reference interroge `sim` a travers des fonctions optionnelles
	 * (`footBlockedAt`, `tileTraversalCost`, `blockedAt`...), avec des
	 * replis successifs. Cette interface tient le meme contrat en le rendant
	 * explicite — et elle permet de tester l'A* sur une grille posee a la main,
	 * sans monde genere.
	 */
	class ANASTASISSIM_API INavSource
	{
	public:
		virtual ~INavSource() = default;
		virtual int32 GetWidth() const = 0;
		virtual int32 GetHeight() const = 0;
		/** `footBlockedAt` — hors bornes doit rendre true. */
		virtual bool IsBlocked(int32 X, int32 Y) const = 0;
		/** `tileTraversalCost` — rend `AnastasisNav::Infinity` si infranchissable. */
		virtual double TraversalCost(int32 X, int32 Y, double BaseCost) const = 0;
	};

	/** Source branchee sur un monde genere et sa grille de navigation. */
	class ANASTASISSIM_API FWorldNavSource final : public INavSource
	{
	public:
		FWorldNavSource(const AnastasisNav::FNavGrid& InGrid, const AnastasisWorld::FWorld& InWorld)
			: Grid(InGrid), World(InWorld) {}

		virtual int32 GetWidth() const override { return Grid.W; }
		virtual int32 GetHeight() const override { return Grid.H; }
		virtual bool IsBlocked(int32 X, int32 Y) const override;
		virtual double TraversalCost(int32 X, int32 Y, double BaseCost) const override;

	private:
		const AnastasisNav::FNavGrid& Grid;
		const AnastasisWorld::FWorld& World;
	};

	/**
	 * `findPath`. Rend false quand la reference rend `null` — pas de chemin,
	 * depart ou arrivee hors bornes, budget epuise.
	 *
	 * Depart et arrivee confondus rendent true avec un chemin VIDE, comme le JS:
	 * c'est un succes, pas un echec, et la difference compte pour l'appelant.
	 *
	 * Les points rendus sont des CENTRES de case (`x + 0.5`), dans l'ordre du
	 * depart vers l'arrivee, le depart exclu.
	 */
	ANASTASISSIM_API bool FindPath(
		const INavSource& Source,
		const FPoint& Start,
		const FPoint& Target,
		const FOptions& Options,
		TArray<FPoint>& OutPath);
}
