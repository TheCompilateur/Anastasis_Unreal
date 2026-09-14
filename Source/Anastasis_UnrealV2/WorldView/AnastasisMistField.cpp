#include "WorldView/AnastasisMistField.h"

namespace AnastasisMist
{

TArray<FMistPocket> BuildMistField(
	const AnastasisWorldView::FWorldVisualSnapshot& Snapshot,
	const FMistParams& Params,
	bool* bOutTruncated)
{
	if (bOutTruncated)
	{
		*bOutTruncated = false;
	}

	TArray<FMistPocket> Pockets;

	// A refusal, not a correction: a cell size of zero or a world with no tiles is a caller
	// error, and quietly substituting a default would hide it behind plausible-looking fog.
	if (Params.CellTiles <= 0 || Snapshot.W <= 0 || Snapshot.H <= 0 || Snapshot.Tiles.Num() == 0)
	{
		return Pockets;
	}

	const double Threshold = FMath::Clamp(Params.WetnessThreshold, 0.0, 0.999);
	const int32 CellsX = FMath::DivideAndRoundUp(Snapshot.W, Params.CellTiles);
	const int32 CellsY = FMath::DivideAndRoundUp(Snapshot.H, Params.CellTiles);

	for (int32 CellY = 0; CellY < CellsY; ++CellY)
	{
		for (int32 CellX = 0; CellX < CellsX; ++CellX)
		{
			const int32 FirstX = CellX * Params.CellTiles;
			const int32 FirstY = CellY * Params.CellTiles;
			const int32 LastX = FMath::Min(FirstX + Params.CellTiles, Snapshot.W);
			const int32 LastY = FMath::Min(FirstY + Params.CellTiles, Snapshot.H);

			int32 TileCount = 0;
			int32 WetTileCount = 0;
			double WetnessSum = 0.0;
			double WeightSum = 0.0;
			double WeightedX = 0.0;
			double WeightedY = 0.0;
			double WetAltSum = 0.0;

			for (int32 LocalY = FirstY; LocalY < LastY; ++LocalY)
			{
				for (int32 LocalX = FirstX; LocalX < LastX; ++LocalX)
				{
					const int32 Index = LocalY * Snapshot.W + LocalX;
					if (!Snapshot.Tiles.IsValidIndex(Index))
					{
						continue;
					}
					const AnastasisWorldView::FVisualTile& Tile = Snapshot.Tiles[Index];

					++TileCount;
					// The mean is over EVERY tile of the cell, dry ones included: a cell is
					// foggy because it is mostly wet, not because it touches one puddle.
					WetnessSum += Tile.Wetness;

					if (Tile.Wetness > 0.0)
					{
						++WetTileCount;
						WeightSum += Tile.Wetness;
						WeightedX += Tile.Wetness * (static_cast<double>(Tile.X) + 0.5);
						WeightedY += Tile.Wetness * (static_cast<double>(Tile.Y) + 0.5);
						WetAltSum += Tile.Wetness * Tile.Alt;
					}
				}
			}

			if (TileCount == 0 || WeightSum <= 0.0)
			{
				continue;
			}

			const double MeanWetness = WetnessSum / static_cast<double>(TileCount);
			if (MeanWetness < Threshold)
			{
				continue;
			}

			FMistPocket Pocket;
			Pocket.CellX = CellX;
			Pocket.CellY = CellY;
			Pocket.WetTileCount = WetTileCount;
			Pocket.MeanWetness = MeanWetness;
			Pocket.Density01 = FMath::Clamp((MeanWetness - Threshold) / (1.0 - Threshold), 0.0, 1.0);

			// Wetness-weighted centroid: the pocket sits over the water, not over the cell's
			// geometric middle, which on a cell straddling a ridge would put fog on the ridge.
			const double CentroidTileX = WeightedX / WeightSum;
			const double CentroidTileY = WeightedY / WeightSum;
			const double CentroidAlt = WetAltSum / WeightSum;
			Pocket.Location = FVector(
				CentroidTileX * AnastasisWorldView::TileWorldSize,
				CentroidTileY * AnastasisWorldView::TileWorldSize,
				CentroidAlt * AnastasisWorldView::AltitudeScale);

			Pocket.RadiusUU =
				static_cast<double>(Params.CellTiles) * AnastasisWorldView::TileWorldSize * Params.VolumeRadiusFraction;

			Pockets.Add(Pocket);
		}
	}

	if (Params.MaxVolumes > 0 && Pockets.Num() > Params.MaxVolumes)
	{
		// Keep the wettest. The tie-break on cell coordinates is not cosmetic: without it two
		// cells with equal means could swap places between runs and the mist would move
		// without the world changing.
		Pockets.Sort([](const FMistPocket& A, const FMistPocket& B)
		{
			if (A.MeanWetness != B.MeanWetness)
			{
				return A.MeanWetness > B.MeanWetness;
			}
			if (A.CellY != B.CellY)
			{
				return A.CellY < B.CellY;
			}
			return A.CellX < B.CellX;
		});
		Pockets.SetNum(Params.MaxVolumes);
		if (bOutTruncated)
		{
			*bOutTruncated = true;
		}
	}

	return Pockets;
}

}
