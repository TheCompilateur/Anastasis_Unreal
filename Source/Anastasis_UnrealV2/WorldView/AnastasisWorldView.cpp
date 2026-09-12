#include "WorldView/AnastasisWorldView.h"

namespace AnastasisWorldView
{
	namespace
	{
		void RecountSnapshot(FWorldVisualSnapshot& Snapshot)
		{
			for (int32 Type = 0; Type < AnastasisWorld::TileTypeCount; ++Type)
			{
				Snapshot.TerrainCounts[Type] = 0;
			}

			double MinAlt = TNumericLimits<double>::Max();
			double MaxAlt = TNumericLimits<double>::Lowest();
			for (const FVisualTile& Tile : Snapshot.Tiles)
			{
				Snapshot.TerrainCounts[static_cast<uint8>(Tile.Type)] += 1;
				MinAlt = FMath::Min(MinAlt, Tile.Alt);
				MaxAlt = FMath::Max(MaxAlt, Tile.Alt);
			}

			if (Snapshot.Tiles.Num() > 0)
			{
				Snapshot.MinAlt = MinAlt;
				Snapshot.MaxAlt = MaxAlt;
			}
			else
			{
				Snapshot.MinAlt = 0.0;
				Snapshot.MaxAlt = 0.0;
			}
		}
	}

	FVisualTile MakeVisualTile(const AnastasisWorld::FTile& Tile, int32 SourceIndex)
	{
		FVisualTile Visual;
		Visual.SourceIndex = SourceIndex;
		Visual.X = Tile.X;
		Visual.Y = Tile.Y;
		Visual.Type = Tile.Type;
		Visual.Resource = Tile.Resource;
		Visual.Amount = Tile.Amount;
		Visual.Alt = Tile.Alt;
		Visual.Shade = Tile.Shade;
		Visual.Shore = Tile.Shore;
		Visual.Wetness = Tile.Wetness;
		Visual.FlowX = Tile.FlowX;
		Visual.FlowZ = Tile.FlowZ;
		Visual.FlowAmt = Tile.FlowAmt;
		Visual.CropId = Tile.CropId;
		Visual.Fertility = Tile.Fertility;
		Visual.ForestMargin = Tile.ForestMargin;
		Visual.bHasForestMargin = Tile.bHasForestMargin;
		return Visual;
	}

	FWorldVisualSnapshot CaptureSnapshot(uint32 Seed, const AnastasisWorld::FWorld& World)
	{
		FWorldVisualSnapshot Snapshot;
		Snapshot.Seed = Seed;
		Snapshot.SourceW = World.W;
		Snapshot.SourceH = World.H;
		Snapshot.OriginX = 0;
		Snapshot.OriginY = 0;
		Snapshot.W = World.W;
		Snapshot.H = World.H;
		Snapshot.Tiles.SetNum(World.Tiles.Num());

		for (int32 Index = 0; Index < World.Tiles.Num(); ++Index)
		{
			const AnastasisWorld::FTile& Tile = World.Tiles[Index];
			const int32 ExpectedX = World.W > 0 ? (Index % World.W) : 0;
			const int32 ExpectedY = World.W > 0 ? (Index / World.W) : 0;
			check(Tile.X == ExpectedX);
			check(Tile.Y == ExpectedY);
			Snapshot.Tiles[Index] = MakeVisualTile(Tile, Index);
		}

		RecountSnapshot(Snapshot);
		return Snapshot;
	}

	FWorldVisualSnapshot CaptureCanonicalWorld(uint32 Seed)
	{
		const AnastasisWorld::FWorld World = AnastasisWorld::GenerateWorld(Seed, ReferenceWidth, ReferenceHeight);
		return CaptureSnapshot(Seed, World);
	}

	FWorldVisualSnapshot CropSnapshot(
		const FWorldVisualSnapshot& Source,
		int32 OriginX,
		int32 OriginY,
		int32 CropW,
		int32 CropH)
	{
		FWorldVisualSnapshot Cropped;
		Cropped.Seed = Source.Seed;
		Cropped.SourceW = Source.SourceW;
		Cropped.SourceH = Source.SourceH;
		Cropped.OriginX = OriginX;
		Cropped.OriginY = OriginY;

		const int32 LocalOriginX = OriginX - Source.OriginX;
		const int32 LocalOriginY = OriginY - Source.OriginY;
		if (CropW <= 0 || CropH <= 0
			|| LocalOriginX < 0 || LocalOriginY < 0
			|| LocalOriginX + CropW > Source.W
			|| LocalOriginY + CropH > Source.H)
		{
			return Cropped;
		}

		Cropped.W = CropW;
		Cropped.H = CropH;
		Cropped.Tiles.SetNum(CropW * CropH);

		for (int32 LocalY = 0; LocalY < CropH; ++LocalY)
		{
			for (int32 LocalX = 0; LocalX < CropW; ++LocalX)
			{
				const int32 SourceIndex = (LocalOriginY + LocalY) * Source.W + (LocalOriginX + LocalX);
				const int32 CropIndex = LocalY * CropW + LocalX;
				Cropped.Tiles[CropIndex] = Source.Tiles[SourceIndex];
			}
		}

		RecountSnapshot(Cropped);
		return Cropped;
	}

	const FVisualTile* FindTile(const FWorldVisualSnapshot& Snapshot, int32 TileX, int32 TileY)
	{
		const int32 LocalX = TileX - Snapshot.OriginX;
		const int32 LocalY = TileY - Snapshot.OriginY;
		if (LocalX < 0 || LocalY < 0 || LocalX >= Snapshot.W || LocalY >= Snapshot.H)
		{
			return nullptr;
		}
		return &Snapshot.Tiles[LocalY * Snapshot.W + LocalX];
	}

	FPlan BuildPlan(const FWorldVisualSnapshot& Snapshot)
	{
		FPlan Plan;
		Plan.Seed = Snapshot.Seed;
		Plan.SourceW = Snapshot.SourceW;
		Plan.SourceH = Snapshot.SourceH;
		Plan.OriginX = Snapshot.OriginX;
		Plan.OriginY = Snapshot.OriginY;
		Plan.W = Snapshot.W;
		Plan.H = Snapshot.H;
		Plan.TileCount = Snapshot.Tiles.Num();
		Plan.MinAlt = Snapshot.MinAlt;
		Plan.MaxAlt = Snapshot.MaxAlt;
		Plan.Locations.SetNum(Plan.TileCount);
		Plan.Types.SetNum(Plan.TileCount);
		Plan.Alts.SetNum(Plan.TileCount);

		for (int32 Type = 0; Type < AnastasisWorld::TileTypeCount; ++Type)
		{
			Plan.TerrainCounts[Type] = Snapshot.TerrainCounts[Type];
		}

		for (int32 Index = 0; Index < Plan.TileCount; ++Index)
		{
			const FVisualTile& Tile = Snapshot.Tiles[Index];
			Plan.Locations[Index] = TileToUnreal(Tile.X, Tile.Y, Tile.Alt);
			Plan.Types[Index] = Tile.Type;
			Plan.Alts[Index] = Tile.Alt;
		}

		return Plan;
	}

	FPlan BuildPlan(uint32 Seed, const AnastasisWorld::FWorld& World)
	{
		return BuildPlan(CaptureSnapshot(Seed, World));
	}

	FTilePose SamplePose(const FPlan& Plan, int32 TileX, int32 TileY)
	{
		FTilePose Pose;
		Pose.X = TileX;
		Pose.Y = TileY;
		const int32 LocalX = TileX - Plan.OriginX;
		const int32 LocalY = TileY - Plan.OriginY;
		if (Plan.W <= 0 || LocalX < 0 || LocalY < 0 || LocalX >= Plan.W || LocalY >= Plan.H)
		{
			Pose.Index = INDEX_NONE;
			return Pose;
		}

		Pose.Index = LocalY * Plan.W + LocalX;
		Pose.Type = Plan.Types[Pose.Index];
		Pose.Alt = Plan.Alts[Pose.Index];
		Pose.UnrealLocation = Plan.Locations[Pose.Index];
		return Pose;
	}

	FBox PlanBounds(const FPlan& Plan)
	{
		if (Plan.TileCount <= 0)
		{
			return FBox(ForceInit);
		}

		FBox Box(ForceInit);
		for (const FVector& Location : Plan.Locations)
		{
			Box += Location;
		}
		const FVector Half(TileWorldSize * 0.5, TileWorldSize * 0.5, 0.0);
		Box.Min -= Half;
		Box.Max += Half;
		return Box;
	}

	FBox SnapshotBounds(const FWorldVisualSnapshot& Snapshot)
	{
		return PlanBounds(BuildPlan(Snapshot));
	}
}
