#include "World/AnastasisPathfinding.h"

#include "World/AnastasisNavGrid.h"
#include "World/AnastasisWorld.h"

namespace AnastasisPath
{
	bool FWorldNavSource::IsBlocked(int32 X, int32 Y) const
	{
		return AnastasisNav::FootBlockedAt(Grid, World, X, Y);
	}

	double FWorldNavSource::TraversalCost(int32 X, int32 Y, double BaseCost) const
	{
		return AnastasisNav::TileTraversalCost(Grid, X, Y, BaseCost);
	}

	namespace
	{
		struct FStep
		{
			int32 DX;
			int32 DY;
			double Cost;
		};

		/**
		 * Copie de `NEIGHBORS` du JS, dans son ordre.
		 *
		 * Cet ordre ne change AUCUN chemin — verifie par mutation: le
		 * comparateur du tas etant un ordre total, la suite des `pop` ne depend
		 * pas de l'ordre des poussees. On le copie parce qu'une copie fidele se
		 * relit contre la reference sans refaire ce raisonnement, pas parce
		 * qu'il porte une decision.
		 */
		constexpr FStep Neighbors[] = {
			{  1,  0, StraightCost },
			{  0,  1, StraightCost },
			{ -1,  0, StraightCost },
			{  0, -1, StraightCost },
			{  1,  1, DiagonalCost },
			{ -1,  1, DiagonalCost },
			{ -1, -1, DiagonalCost },
			{  1, -1, DiagonalCost },
		};

		struct FNode
		{
			int32 X = 0;
			int32 Y = 0;
			int32 Key = 0;
			double G = 0.0;
			double H = 0.0;
			double F = 0.0;
			int32 CameFrom = -1;
			/**
			 * Le JS compare l'identite de l'objet (`records.get(key) !== current`)
			 * pour ecarter une entree perimee. En C++ il n'y a pas d'identite a
			 * comparer une fois la valeur copiee dans le tas: ce numero la
			 * remplace. Chaque poussee en recoit un neuf, et seul le dernier
			 * enregistre pour une case est vivant.
			 */
			int32 Serial = 0;
			bool bClosed = false;
		};

		/** `compareNodes`: f, puis h, puis y, puis x. Ordre total, sans egalite. */
		bool IsLess(const FNode& A, const FNode& B)
		{
			if (A.F != B.F) return A.F < B.F;
			if (A.H != B.H) return A.H < B.H;
			if (A.Y != B.Y) return A.Y < B.Y;
			return A.X < B.X;
		}

		/**
		 * Le tas binaire du JS, copie geste pour geste.
		 *
		 * Avec un comparateur qui est un ordre total, n'importe quel tas correct
		 * rendrait la meme suite — `TArray::HeapPush` ferait l'affaire. Le
		 * copier coute trente lignes et achete ceci: si le comparateur cessait
		 * un jour d'etre total, le portage divergerait de la reference
		 * exactement comme elle, et pas autrement.
		 */
		class FMinHeap
		{
		public:
			int32 Num() const { return Items.Num(); }

			void Push(const FNode& Item)
			{
				Items.Add(Item);
				BubbleUp(Items.Num() - 1);
			}

			FNode Pop()
			{
				const FNode First = Items[0];
				const FNode Last = Items.Pop(EAllowShrinking::No);
				if (Items.Num() > 0)
				{
					Items[0] = Last;
					SinkDown(0);
				}
				return First;
			}

		private:
			void BubbleUp(int32 Index)
			{
				while (Index > 0)
				{
					const int32 Parent = (Index - 1) / 2;
					if (!IsLess(Items[Index], Items[Parent]))
					{
						break;
					}
					Items.Swap(Index, Parent);
					Index = Parent;
				}
			}

			void SinkDown(int32 Index)
			{
				for (;;)
				{
					const int32 Left = Index * 2 + 1;
					const int32 Right = Left + 1;
					int32 Best = Index;
					if (Left < Items.Num() && IsLess(Items[Left], Items[Best])) Best = Left;
					if (Right < Items.Num() && IsLess(Items[Right], Items[Best])) Best = Right;
					if (Best == Index)
					{
						break;
					}
					Items.Swap(Index, Best);
					Index = Best;
				}
			}

			TArray<FNode> Items;
		};

		double Heuristic(int32 X, int32 Y, int32 TX, int32 TY)
		{
			const double DX = FMath::Abs(static_cast<double>(TX - X));
			const double DY = FMath::Abs(static_cast<double>(TY - Y));
			const double Diagonal = FMath::Min(DX, DY);
			const double Straight = FMath::Max(DX, DY) - Diagonal;
			return Diagonal * DiagonalCost + Straight * StraightCost;
		}
	}

	bool FindPath(
		const INavSource& Source,
		const FPoint& Start,
		const FPoint& Target,
		const FOptions& Options,
		TArray<FPoint>& OutPath)
	{
		OutPath.Reset();

		const int32 W = Source.GetWidth();
		const int32 H = Source.GetHeight();
		const int32 SX = static_cast<int32>(FMath::FloorToDouble(Start.X));
		const int32 SY = static_cast<int32>(FMath::FloorToDouble(Start.Y));
		const int32 TX = static_cast<int32>(FMath::FloorToDouble(Target.X));
		const int32 TY = static_cast<int32>(FMath::FloorToDouble(Target.Y));

		auto InBounds = [W, H](int32 X, int32 Y)
		{
			return X >= 0 && Y >= 0 && X < W && Y < H;
		};

		if (!InBounds(SX, SY) || !InBounds(TX, TY))
		{
			return false;
		}
		if (SX == TX && SY == TY)
		{
			// Chemin vide, mais succes: l'appelant doit pouvoir distinguer
			// « deja arrive » de « pas de chemin ».
			return true;
		}

		const double MaxCost = FMath::Max(1.0, Options.MaxCost) * StraightCost;
		const int32 MaxExpanded = FMath::Max(1, Options.MaxExpanded);
		if ((!Options.bAllowBlockedStart && Source.IsBlocked(SX, SY))
			|| (!Options.bAllowBlockedTarget && Source.IsBlocked(TX, TY)))
		{
			return false;
		}

		FMinHeap Open;
		// `records`: une entree vivante par case, creuse comme la Map du JS. Un
		// tableau plat de la taille du monde couterait une douzaine de Mo par
		// appel pour explorer au plus quelques milliers de cases.
		TMap<int32, FNode> Records;
		Records.Reserve(FMath::Min(MaxExpanded, 4096));

		int32 NextSerial = 1;
		const int32 StartKey = SY * W + SX;
		const double StartH = Heuristic(SX, SY, TX, TY);

		FNode First;
		First.X = SX;
		First.Y = SY;
		First.Key = StartKey;
		First.G = 0.0;
		First.H = StartH;
		First.F = StartH;
		First.CameFrom = -1;
		First.Serial = NextSerial++;
		Records.Add(StartKey, First);
		Open.Push(First);

		int32 Expanded = 0;
		while (Open.Num() > 0 && Expanded < MaxExpanded)
		{
			const FNode Current = Open.Pop();

			// Entree perimee: une meilleure a remplace celle-ci depuis.
			FNode* Live = Records.Find(Current.Key);
			if (Live == nullptr || Live->Serial != Current.Serial)
			{
				continue;
			}
			if (Live->bClosed)
			{
				continue;
			}
			Live->bClosed = true;
			Expanded += 1;

			if (Current.X == TX && Current.Y == TY)
			{
				// `reconstructPath` remonte les `cameFrom` a travers `records`,
				// donc a travers l'entree VIVANTE de chaque case — pas
				// forcement celle qui etait courante a la creation du noeud. Le
				// portage fait la meme chose, y compris dans ce detail.
				TArray<FPoint> Reversed;
				const FNode* Node = Live;
				// Garde-fou: une chaine plus longue que la grille est un cycle,
				// et la reference tournerait dessus indefiniment. Aucun chemin
				// legitime ne l'atteint — un chemin sans repetition ne depasse
				// pas le nombre de cases.
				const int32 MaxChain = W * H + 1;
				int32 Guard = 0;
				while (Node != nullptr && Node->CameFrom >= 0 && Guard++ < MaxChain)
				{
					Reversed.Add(FPoint{ Node->X + 0.5, Node->Y + 0.5 });
					Node = Records.Find(Node->CameFrom);
				}
				if (Node == nullptr || Guard >= MaxChain)
				{
					OutPath.Reset();
					return false;
				}
				OutPath.Reserve(Reversed.Num());
				for (int32 Index = Reversed.Num() - 1; Index >= 0; --Index)
				{
					OutPath.Add(Reversed[Index]);
				}
				return true;
			}

			if (Current.G > MaxCost)
			{
				continue;
			}

			for (const FStep& Step : Neighbors)
			{
				const int32 NX = Current.X + Step.DX;
				const int32 NY = Current.Y + Step.DY;
				if (!InBounds(NX, NY))
				{
					continue;
				}
				if (Source.IsBlocked(NX, NY) && !(Options.bAllowBlockedTarget && NX == TX && NY == TY))
				{
					continue;
				}
				// Coupe de coin: une diagonale ne passe pas entre deux
				// obstacles adjacents.
				if (Step.DX != 0 && Step.DY != 0
					&& (Source.IsBlocked(Current.X + Step.DX, Current.Y)
						|| Source.IsBlocked(Current.X, Current.Y + Step.DY)))
				{
					continue;
				}

				const double BaseStep = Source.TraversalCost(NX, NY, Step.Cost);
				if (!(BaseStep < AnastasisNav::Infinity))
				{
					continue;
				}
				const double NextG = Current.G + BaseStep;
				if (NextG > MaxCost)
				{
					continue;
				}

				const int32 NextKey = NY * W + NX;
				if (const FNode* Previous = Records.Find(NextKey))
				{
					if (NextG >= Previous->G)
					{
						continue;
					}
				}

				FNode Node;
				Node.X = NX;
				Node.Y = NY;
				Node.Key = NextKey;
				Node.G = NextG;
				Node.H = Heuristic(NX, NY, TX, TY);
				Node.F = NextG + Node.H;
				Node.CameFrom = Current.Key;
				Node.Serial = NextSerial++;
				// Remplacement, `bClosed` compris: `records.set` du JS ecrase
				// l'entree precedente, elle ne la met pas a jour.
				Records.Add(NextKey, Node);
				Open.Push(Node);
			}
		}

		return false;
	}
}
