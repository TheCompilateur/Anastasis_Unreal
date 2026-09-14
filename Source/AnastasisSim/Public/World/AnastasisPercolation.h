// Percolation du monde — quelles terres se tiennent, à pied, d'un seul tenant.
//
// Portage de `src/sim/percolation.js`.
//
// Une carte n'est pas « traversable » ou « bloquée » : elle est un ensemble de
// COMPOSANTES connexes. Le hameau vit dans l'une d'elles ; tout ce qui n'y est
// pas est un autre monde, même à trois tuiles de distance.
//
// La référence raconte la mesure qui a fait naître ce module (12/08/2026) : des
// chantiers ouvraient au cœur d'un massif, entièrement cernés par des arbres
// debout depuis que ceux-ci bloquent le pied. Matériaux livrés, l'emplacement
// déclaré valide, et pas une pièce posée pendant 55 jours — le bâtisseur ne
// pouvait pas s'approcher. Le chantier confisquait son créneau pour toute la
// partie.
//
// --- LA CONTRAINTE QUI COMMANDE TOUT ----------------------------------------
//
// La connexité est STRICTEMENT celle de `AnastasisPathfinding` : huit voisins,
// diagonale refusée si elle coupe un coin bloqué. La référence le dit sans
// détour — « un champ qui mentirait sur ce point serait pire que pas de champ
// du tout ». Un champ trop permissif enverrait un bâtisseur vers un chantier
// qu'il ne peut pas atteindre ; trop strict, il condamnerait des terres qui se
// marchent.
//
// C'est pourquoi le test croise les deux systèmes : sur un vrai monde, deux
// cases d'une même composante DOIVENT se rejoindre par l'A*, et deux cases de
// composantes différentes ne le doivent jamais.
//
// --- CE QUI N'EST PAS PORTÉ -------------------------------------------------
//
// Le cache (`sim._walkComponents`, invalidé par `navVersion`) reste à
// l'appelant : c'est une question de coût, pas de causalité, et le C++ n'a pas
// encore le compteur qui porte l'invalidation.

#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisNavGrid.h"

namespace AnastasisWorld { struct FWorld; }

namespace AnastasisPercolation
{
	/** Une case sans composante : le pied n'y passe pas. */
	inline constexpr int32 NoComponent = -1;

	/**
	 * Composantes connexes des terres franchissables à pied.
	 *
	 * Les identifiants sont attribués dans l'ordre de balayage (raster), et
	 * tout le reste s'y réfère : deux portages qui balaieraient différemment
	 * rendraient des identifiants différents pour les mêmes terres.
	 */
	struct ANASTASISSIM_API FWalkComponents
	{
		int32 W = 0;
		int32 H = 0;
		/** Un identifiant par tuile, `NoComponent` si bloquée. */
		TArray<int32> Ids;
		/** Nombre de tuiles de chaque composante, indexé par identifiant. */
		TArray<int32> Sizes;

		int32 Count() const { return Sizes.Num(); }
		bool IsInBounds(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < W && Y < H; }
	};

	/** État de percolation du monde. `MainFrac` est la part des terres au hameau. */
	struct FStats
	{
		int32 Components = 0;
		int32 WalkableTiles = 0;
		double WalkableFrac = 0.0;
		int32 HomeComponent = NoComponent;
		int32 HomeTiles = 0;
		/**
		 * Sous ~0.85, la carte est en archipel : des régions entières sont
		 * inatteignables et tout ce qu'on y planifiera mourra sur place.
		 */
		double MainFrac = 0.0;
		int32 LargestTiles = 0;
		bool bHomeIsLargest = false;
		int32 Islands = 0;
	};

	ANASTASISSIM_API FWalkComponents BuildWalkComponents(
		const AnastasisNav::FNavGrid& Grid,
		const AnastasisWorld::FWorld& World);

	/** Composante d'une case, ou `NoComponent`. Les coordonnées sont tronquées. */
	ANASTASISSIM_API int32 ComponentAt(const FWalkComponents& Components, double X, double Y);

	/**
	 * Composante du hameau.
	 *
	 * Une case bloquée n'a pas de composante propre — et le camp EST un
	 * bâtiment. On prend alors la meilleure des cases voisines, comme un
	 * habitant qui sort de chez lui : carrés concentriques de rayon 1 à 3, la
	 * plus grande composante l'emporte, les égalités tranchées par l'ordre de
	 * parcours. Cet ordre fait partie du résultat.
	 */
	ANASTASISSIM_API int32 SettlementComponent(
		const FWalkComponents& Components, double SettlementX, double SettlementY);

	/**
	 * Cette case est-elle dans le même monde piétonnier que le hameau ?
	 *
	 * Un monde sans hameau rend **true** : ne rien interdire vaut mieux que tout
	 * interdire quand la question n'a pas de sens.
	 */
	ANASTASISSIM_API bool ReachableFromSettlement(
		const FWalkComponents& Components, double SettlementX, double SettlementY,
		double X, double Y);

	/**
	 * Une case bloquée — un chantier, un arbre — est-elle BORDÉE par le monde du
	 * hameau ? C'est la vraie question pour un emplacement de bâtiment : on ne
	 * se tient pas dessus, on se tient à côté.
	 */
	ANASTASISSIM_API bool ApproachableFromSettlement(
		const FWalkComponents& Components, double SettlementX, double SettlementY,
		double X, double Y);

	ANASTASISSIM_API FStats ComputeStats(
		const FWalkComponents& Components, double SettlementX, double SettlementY);
}
