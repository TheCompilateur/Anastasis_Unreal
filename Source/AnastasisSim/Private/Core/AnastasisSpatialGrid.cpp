#include "Core/AnastasisSpatialGrid.h"

namespace AnastasisSpatialGrid
{
	void FGrid::Rebuild(
		TArrayView<const FVector2D> Points,
		double InCellSize,
		TFunctionRef<bool(int32)> Predicate)
	{
		CellSize = (InCellSize > 0.0) ? InCellSize : DefaultCellSize;
		ResetKeepingMemory();

		const int32 Count = Points.Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!Predicate(Index))
			{
				continue;
			}

			const FVector2D& Point = Points[Index];
			// Un acteur sans position exploitable est ignore plutot que
			// bucketise en case 0: le JS faisait pareil (Number.isFinite), et
			// une case 0 surchargee ralentit toutes les requetes de l'origine.
			if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y))
			{
				continue;
			}

			const int32 CellX = static_cast<int32>(FMath::FloorToDouble(Point.X / CellSize));
			const int32 CellY = static_cast<int32>(FMath::FloorToDouble(Point.Y / CellSize));
			Cells.FindOrAdd(PackCellKey(CellX, CellY)).Add(Index);
		}
	}

	void FGrid::Rebuild(TArrayView<const FVector2D> Points, double InCellSize)
	{
		Rebuild(Points, InCellSize, [](int32) { return true; });
	}

	void FGrid::ForEachNear(double X, double Y, double Radius, TFunctionRef<void(int32)> Visit) const
	{
		if (Cells.Num() == 0)
		{
			return;
		}

		const int32 Span = static_cast<int32>(FMath::CeilToDouble(Radius / FMath::Max(0.0001, CellSize)));
		const int32 CenterX = static_cast<int32>(FMath::FloorToDouble(X / CellSize));
		const int32 CenterY = static_cast<int32>(FMath::FloorToDouble(Y / CellSize));

		// Ordre dy puis dx, comme en JS. Les heuristiques qui s'arretent au
		// premier voisin acceptable dependent de cet ordre.
		for (int32 DY = -Span; DY <= Span; ++DY)
		{
			for (int32 DX = -Span; DX <= Span; ++DX)
			{
				const TArray<int32>* Bucket = Cells.Find(PackCellKey(CenterX + DX, CenterY + DY));
				if (!Bucket)
				{
					continue;
				}
				for (const int32 ActorIndex : *Bucket)
				{
					Visit(ActorIndex);
				}
			}
		}
	}
}
