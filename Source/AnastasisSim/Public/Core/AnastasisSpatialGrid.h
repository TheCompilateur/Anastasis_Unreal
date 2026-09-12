#pragma once

#include "CoreMinimal.h"

/**
 * GRILLE SPATIALE SIM — requetes de voisinage sans scan O(n^2).
 * Port de src/sim/spatialGrid.js.
 *
 * Bucketise les acteurs par case. ForEachNear ne visite que les buckets a
 * portee; l'appelant garde la distance exacte. Utilisee par la sim (besoins,
 * social, congestion) et relue par la couche de presentation.
 *
 * DIFFERENCE ASSUMEE AVEC LE JS: les buckets stockent des INDEX (int32) dans le
 * tableau d'acteurs, pas des references. En JS une reference d'objet est stable
 * et gratuite; en C++ un pointeur vers un element de TArray meurt au premier
 * realloc. L'index survit, tient dans une ligne de cache, et se serialise tel
 * quel dans une sauvegarde. L'appelant reindexe lui-meme.
 *
 * L'ordre de visite est deterministe: cases parcourues dy puis dx croissants,
 * acteurs dans l'ordre d'insertion. Ne pas remplacer par un parcours de TMap
 * non ordonne — l'ordre de visite change le resultat des heuristiques qui
 * s'arretent au premier voisin trouve.
 */
namespace AnastasisSpatialGrid
{
	/** Taille de case par defaut (unites monde de la sim). */
	inline constexpr double DefaultCellSize = 2.5;

	/** Offset pour packer cx/cy signes dans une cle 32 bits. */
	inline constexpr int32 CellKeyBias = 4096;

	/**
	 * Cle de case sans allocation de chaine.
	 * Le JS packait `((cx + 4096) << 16) | ((cy + 4096) & 0xffff)`.
	 *
	 * LIMITE HERITEE, VOLONTAIREMENT CONSERVEE: cy est masque sur 16 bits, donc
	 * deux cases distantes de 65536 en Y partagent une cle. A 2,5 unites par
	 * case cela represente 163 840 unites — hors de portee de toute carte
	 * jouable. Si la carte grandit un jour a ce point, c'est ici que ca casse.
	 */
	inline uint32 PackCellKey(int32 CellX, int32 CellY)
	{
		const int32 Packed = ((CellX + CellKeyBias) << 16) | ((CellY + CellKeyBias) & 0xffff);
		return static_cast<uint32>(Packed);
	}

	/**
	 * Grille reconstruite en place, une fois par tick — jamais par requete.
	 *
	 * Les buckets sont conserves d'un rebuild a l'autre (Reset garde la memoire
	 * allouee): a 60 Hz sur plusieurs centaines d'acteurs, reallouer chaque
	 * frame coute plus cher que la requete elle-meme.
	 */
	struct ANASTASISSIM_API FGrid
	{
		/** Cle de case -> index des acteurs de cette case. */
		TMap<uint32, TArray<int32>> Cells;

		/** Taille de case effective de ce rebuild. */
		double CellSize = DefaultCellSize;

		/** Vide le contenu en gardant les allocations des buckets. */
		void ResetKeepingMemory()
		{
			for (TPair<uint32, TArray<int32>>& Pair : Cells)
			{
				Pair.Value.Reset();
			}
		}

		/**
		 * Reconstruit la grille a partir de points deja extraits.
		 *
		 * @param Points   position planaire de chaque acteur, indexee comme le
		 *                 tableau d'acteurs de l'appelant.
		 * @param InCellSize taille de case; <= 0 retombe sur DefaultCellSize.
		 * @param Predicate filtre optionnel (ex: ignorer les animaux morts).
		 *                 Un point non fini est toujours ignore, comme en JS.
		 */
		void Rebuild(
			TArrayView<const FVector2D> Points,
			double InCellSize,
			TFunctionRef<bool(int32)> Predicate);

		/** Meme rebuild, sans filtre: tous les points finis sont indexes. */
		void Rebuild(TArrayView<const FVector2D> Points, double InCellSize = DefaultCellSize);

		/**
		 * Visite chaque acteur dont la CASE est a portee de Radius autour de
		 * (X, Y). Sur-approxime aux bords: l'appelant verifie la distance exacte.
		 */
		void ForEachNear(double X, double Y, double Radius, TFunctionRef<void(int32)> Visit) const;
	};
}
