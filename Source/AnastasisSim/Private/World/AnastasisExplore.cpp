#include "World/AnastasisExplore.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisRng.h"
#include "Core/AnastasisSimMath.h"

namespace AnastasisExplore
{
	using AnastasisMath::Clamp;

	int32 CellIndex(int32 W, int32 H, int32 X, int32 Y)
	{
		if (X < 0 || Y < 0 || X >= W || Y >= H)
		{
			return -1;
		}
		const int32 Cols = FMath::DivideAndRoundUp(W, CellSize);
		return (Y / CellSize) * Cols + (X / CellSize);
	}

	bool MarkCell(TSet<int32>& Cells, int32& CellCount, int32 W, int32 H, int32 X, int32 Y)
	{
		const int32 Index = CellIndex(W, H, X, Y);
		if (Index < 0 || Cells.Contains(Index))
		{
			return false;
		}
		Cells.Add(Index);
		CellCount += 1;
		return true;
	}

	FExploreResult RandomWalkTarget(const FExploreWorld& World, FAnastasisRng& Rng, double X, double Y)
	{
		FExploreResult Out;
		const double MaxX = static_cast<double>(World.W - 3);
		const double MaxY = static_cast<double>(World.H - 3);
		for (int32 I = 0; I < RandomWalkTries; ++I)
		{
			// `clamp(actor.x + this.rng() * 24 - 12, 2, this.w - 3)`, x puis y.
			const double PX = Clamp(X + Rng.Next() * 24.0 - 12.0, 2.0, MaxX);
			const double PY = Clamp(Y + Rng.Next() * 24.0 - 12.0, 2.0, MaxY);
			Out.Draws += 2;
			if (!World.IsFootBlocked(PX, PY))
			{
				Out.Point = { PX, PY };
				Out.bRandomWalk = true;
				return Out;
			}
		}
		Out.Point = World.Settlement;
		Out.bRandomWalk = true;
		Out.bSettlement = true;
		return Out;
	}

	FExploreResult ExploreTarget(
		const FExploreWorld& World,
		FAnastasisRng& Rng,
		double X,
		double Y,
		const TSet<int32>& KnownCells)
	{
		const double Cell = static_cast<double>(CellSize);
		const int32 Cols = FMath::DivideAndRoundUp(World.W, CellSize);
		const int32 Rows = FMath::DivideAndRoundUp(World.H, CellSize);
		const double OriginX = AnastasisJs::Floor(X / Cell);
		const double OriginY = AnastasisJs::Floor(Y / Cell);
		FExploreResult Out;
		for (int32 I = 0; I < ExploreTries; ++I)
		{
			// `clamp(originX + Math.floor(sim.rng() * 7) - 3, 0, cols - 1)`, colonne puis rangee.
			const double CX = Clamp(OriginX + AnastasisJs::Floor(Rng.Next() * 7.0) - 3.0, 0.0, static_cast<double>(Cols - 1));
			const double CY = Clamp(OriginY + AnastasisJs::Floor(Rng.Next() * 7.0) - 3.0, 0.0, static_cast<double>(Rows - 1));
			Out.Draws += 2;
			const int32 Index = static_cast<int32>(CY) * Cols + static_cast<int32>(CX);
			if (KnownCells.Contains(Index))
			{
				continue;
			}
			const double PX = Clamp(CX * Cell + Cell / 2.0, 2.0, static_cast<double>(World.W - 3));
			const double PY = Clamp(CY * Cell + Cell / 2.0, 2.0, static_cast<double>(World.H - 3));
			if (!World.IsBlocked(PX, PY))
			{
				Out.Point = { PX, PY };
				return Out;
			}
		}
		// « tout est connu alentour : simple promenade ».
		const FExploreResult Walk = RandomWalkTarget(World, Rng, X, Y);
		Out.Point = Walk.Point;
		Out.Draws += Walk.Draws;
		Out.bRandomWalk = true;
		Out.bSettlement = Walk.bSettlement;
		return Out;
	}
}
