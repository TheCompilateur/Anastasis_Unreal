#include "World/AnastasisPercolation.h"

#include "World/AnastasisWorld.h"

namespace AnastasisPercolation
{
	namespace
	{
		struct FOffset { int32 DX; int32 DY; };

		// Memes listes, dans le meme ordre, que la reference. L'ordre n'entre
		// pas dans le resultat d'un remplissage — un voisin atteint l'est quel
		// que soit l'ordre — mais il entre dans `ApproachableFromSettlement`,
		// qui rend true au PREMIER voisin trouve.
		constexpr FOffset Straight[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		constexpr FOffset Diagonal[] = { { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 } };

		int32 FloorToInt(double V)
		{
			return static_cast<int32>(FMath::FloorToDouble(V));
		}
	}

	FWalkComponents BuildWalkComponents(
		const AnastasisNav::FNavGrid& Grid,
		const AnastasisWorld::FWorld& World)
	{
		FWalkComponents Out;
		Out.W = Grid.W;
		Out.H = Grid.H;
		const int32 Size = Grid.W * Grid.H;
		Out.Ids.Init(NoComponent, Size);

		auto Bloque = [&Grid, &World](int32 X, int32 Y)
		{
			return AnastasisNav::FootBlockedAt(Grid, World, X, Y);
		};

		// Pile explicite: une carte de 108x114 deborderait une recursion.
		TArray<int32> Stack;
		Stack.SetNumUninitialized(Size);

		for (int32 Start = 0; Start < Size; ++Start)
		{
			if (Out.Ids[Start] != NoComponent)
			{
				continue;
			}
			const int32 SX = Start % Grid.W;
			const int32 SY = Start / Grid.W;
			if (Bloque(SX, SY))
			{
				continue;
			}

			// L'identifiant est le rang de decouverte: c'est ce qui rend les
			// identifiants reproductibles d'un portage a l'autre.
			const int32 Id = Out.Sizes.Num();
			int32 Count = 0;
			int32 Top = 0;
			Stack[Top++] = Start;
			Out.Ids[Start] = Id;

			while (Top > 0)
			{
				const int32 Cur = Stack[--Top];
				Count += 1;
				const int32 CX = Cur % Grid.W;
				const int32 CY = Cur / Grid.W;

				for (const FOffset& Step : Straight)
				{
					const int32 NX = CX + Step.DX;
					const int32 NY = CY + Step.DY;
					if (!Out.IsInBounds(NX, NY))
					{
						continue;
					}
					const int32 NI = NY * Grid.W + NX;
					if (Out.Ids[NI] != NoComponent || Bloque(NX, NY))
					{
						continue;
					}
					Out.Ids[NI] = Id;
					Stack[Top++] = NI;
				}

				for (const FOffset& Step : Diagonal)
				{
					const int32 NX = CX + Step.DX;
					const int32 NY = CY + Step.DY;
					if (!Out.IsInBounds(NX, NY))
					{
						continue;
					}
					const int32 NI = NY * Grid.W + NX;
					if (Out.Ids[NI] != NoComponent || Bloque(NX, NY))
					{
						continue;
					}
					// Meme regle que la coupe de coin de l'A*: on ne se faufile
					// pas en diagonale entre deux obstacles.
					if (Bloque(CX + Step.DX, CY) || Bloque(CX, CY + Step.DY))
					{
						continue;
					}
					Out.Ids[NI] = Id;
					Stack[Top++] = NI;
				}
			}

			Out.Sizes.Add(Count);
		}

		return Out;
	}

	int32 ComponentAt(const FWalkComponents& Components, double X, double Y)
	{
		const int32 TX = FloorToInt(X);
		const int32 TY = FloorToInt(Y);
		if (!Components.IsInBounds(TX, TY))
		{
			return NoComponent;
		}
		return Components.Ids[TY * Components.W + TX];
	}

	int32 SettlementComponent(const FWalkComponents& Components, double SettlementX, double SettlementY)
	{
		const int32 Direct = ComponentAt(Components, SettlementX, SettlementY);
		if (Direct != NoComponent)
		{
			return Direct;
		}

		// Carres concentriques, et non anneaux: la reference reparcourt le
		// carre entier a chaque rayon. La difference ne se voit pas sur le
		// resultat — le carre interieur a deja ete examine au rayon precedent —
		// mais la copier coute moins cher que de prouver qu'elle ne se voit pas.
		int32 Best = NoComponent;
		for (int32 R = 1; R <= 3 && Best == NoComponent; ++R)
		{
			for (int32 DY = -R; DY <= R; ++DY)
			{
				for (int32 DX = -R; DX <= R; ++DX)
				{
					const int32 Id = ComponentAt(Components, SettlementX + DX, SettlementY + DY);
					if (Id == NoComponent)
					{
						continue;
					}
					// Comparaison STRICTE: a taille egale, le premier trouve
					// garde la main. C'est l'ordre de parcours qui tranche.
					if (Best == NoComponent || Components.Sizes[Id] > Components.Sizes[Best])
					{
						Best = Id;
					}
				}
			}
		}
		return Best;
	}

	bool ReachableFromSettlement(
		const FWalkComponents& Components, double SettlementX, double SettlementY,
		double X, double Y)
	{
		const int32 Home = SettlementComponent(Components, SettlementX, SettlementY);
		if (Home == NoComponent)
		{
			// Monde sans hameau: ne rien interdire.
			return true;
		}
		return ComponentAt(Components, X, Y) == Home;
	}

	bool ApproachableFromSettlement(
		const FWalkComponents& Components, double SettlementX, double SettlementY,
		double X, double Y)
	{
		const int32 Home = SettlementComponent(Components, SettlementX, SettlementY);
		if (Home == NoComponent)
		{
			return true;
		}
		const int32 TX = FloorToInt(X);
		const int32 TY = FloorToInt(Y);
		if (ComponentAt(Components, TX, TY) == Home)
		{
			return true;
		}
		for (const FOffset& Step : Straight)
		{
			if (ComponentAt(Components, TX + Step.DX, TY + Step.DY) == Home)
			{
				return true;
			}
		}
		for (const FOffset& Step : Diagonal)
		{
			if (ComponentAt(Components, TX + Step.DX, TY + Step.DY) == Home)
			{
				return true;
			}
		}
		return false;
	}

	FStats ComputeStats(const FWalkComponents& Components, double SettlementX, double SettlementY)
	{
		FStats Out;
		Out.Components = Components.Count();
		Out.HomeComponent = SettlementComponent(Components, SettlementX, SettlementY);

		for (const int32 Size : Components.Sizes)
		{
			Out.WalkableTiles += Size;
			if (Size > Out.LargestTiles)
			{
				Out.LargestTiles = Size;
			}
		}

		Out.HomeTiles = Out.HomeComponent == NoComponent ? 0 : Components.Sizes[Out.HomeComponent];

		const int32 Total = Components.W * Components.H;
		Out.WalkableFrac = Total ? static_cast<double>(Out.WalkableTiles) / static_cast<double>(Total) : 0.0;
		Out.MainFrac = Out.WalkableTiles
			? static_cast<double>(Out.HomeTiles) / static_cast<double>(Out.WalkableTiles)
			: 0.0;
		Out.bHomeIsLargest = Out.HomeTiles > 0 && Out.HomeTiles == Out.LargestTiles;

		// Iles = toute composante AUTRE que celle du hameau. Comparer les
		// identifiants, jamais les tailles: deux composantes peuvent avoir la
		// meme taille sans etre la meme.
		Out.Islands = Out.HomeComponent == NoComponent
			? Out.Components
			: FMath::Max(0, Out.Components - 1);

		return Out;
	}
}
