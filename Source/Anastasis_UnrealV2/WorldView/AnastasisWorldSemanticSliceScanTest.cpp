#include "Misc/AutomationTest.h"

#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr uint32 ScanSeed = 12345u;
	constexpr int32 WorldWidth = 96;
	constexpr int32 WorldHeight = 96;

	struct FWindowScore
	{
		int32 X = 0;
		int32 Y = 0;
		int32 Size = 0;
		int32 Score = -1;
		int32 Water = 0;
		int32 Forest = 0;
		int32 Field = 0;
		int32 Grass = 0;
		int32 Scrub = 0;
		int32 Stone = 0;
		int32 ShoreContacts = 0;
		int32 ForestEdgeContacts = 0;
		double MinAlt = 0.0;
		double MaxAlt = 0.0;
		double MeanWetness = 0.0;
		double MeanShore = 0.0;
	};

	int32 Index(int32 X, int32 Y)
	{
		return Y * WorldWidth + X;
	}

	bool IsInside(int32 X, int32 Y)
	{
		return X >= 0 && Y >= 0 && X < WorldWidth && Y < WorldHeight;
	}

	void Consider(FWindowScore& Best, const FWindowScore& Candidate)
	{
		if (Candidate.Score > Best.Score)
		{
			Best = Candidate;
		}
	}

	FWindowScore ScoreWindow(const AnastasisWorld::FWorld& World, int32 X0, int32 Y0, int32 Size)
	{
		FWindowScore Result;
		Result.X = X0;
		Result.Y = Y0;
		Result.Size = Size;
		Result.MinAlt = TNumericLimits<double>::Max();
		Result.MaxAlt = TNumericLimits<double>::Lowest();

		for (int32 Y = Y0; Y < Y0 + Size; ++Y)
		{
			for (int32 X = X0; X < X0 + Size; ++X)
			{
				const AnastasisWorld::FTile& Tile = World.Tiles[Index(X, Y)];
				switch (Tile.Type)
				{
				case AnastasisWorld::ETileType::Water: ++Result.Water; break;
				case AnastasisWorld::ETileType::Forest: ++Result.Forest; break;
				case AnastasisWorld::ETileType::Field: ++Result.Field; break;
				case AnastasisWorld::ETileType::Grass: ++Result.Grass; break;
				case AnastasisWorld::ETileType::Scrub: ++Result.Scrub; break;
				case AnastasisWorld::ETileType::Stone: ++Result.Stone; break;
				default: break;
				}

				Result.MinAlt = FMath::Min(Result.MinAlt, Tile.Alt);
				Result.MaxAlt = FMath::Max(Result.MaxAlt, Tile.Alt);
				Result.MeanWetness += Tile.Wetness;
				Result.MeanShore += Tile.Shore;

				const int32 NeighbourX[4] = { X - 1, X + 1, X, X };
				const int32 NeighbourY[4] = { Y, Y, Y - 1, Y + 1 };
				for (int32 Neighbour = 0; Neighbour < 4; ++Neighbour)
				{
					if (!IsInside(NeighbourX[Neighbour], NeighbourY[Neighbour]))
					{
						continue;
					}

					const AnastasisWorld::FTile& Adjacent = World.Tiles[Index(NeighbourX[Neighbour], NeighbourY[Neighbour])];
					if (Tile.Type == AnastasisWorld::ETileType::Water && Adjacent.Type != AnastasisWorld::ETileType::Water)
					{
						++Result.ShoreContacts;
					}
					if (Tile.Type == AnastasisWorld::ETileType::Forest && Adjacent.Type != AnastasisWorld::ETileType::Forest)
					{
						++Result.ForestEdgeContacts;
					}
				}
			}
		}

		const double Area = static_cast<double>(Size * Size);
		Result.MeanWetness /= Area;
		Result.MeanShore /= Area;

		const int32 RequiredPresence =
			(Result.Water > 0 ? 20 : 0) +
			(Result.Forest > 0 ? 15 : 0) +
			(Result.Field > 0 ? 10 : 0) +
			(Result.Grass + Result.Scrub > 0 ? 10 : 0);
		const int32 WaterEdge = FMath::Min(Result.ShoreContacts, 20);
		const int32 ForestEdge = FMath::Min(Result.ForestEdgeContacts / 2, 10);
		const int32 Relief = FMath::Clamp(FMath::RoundToInt((Result.MaxAlt - Result.MinAlt) * 20.0), 0, 10);
		const int32 Moisture = FMath::Clamp(FMath::RoundToInt(Result.MeanWetness * 20.0), 0, 10);
		Result.Score = RequiredPresence + WaterEdge + ForestEdge + Relief + Moisture;
		return Result;
	}

	void LogCandidate(const TCHAR* Label, const FWindowScore& Candidate)
	{
		UE_LOG(LogTemp, Display,
			TEXT("ANASTASIS_VISUAL_SLICE_%s seed=%u size=%d origin=(%d,%d) score=%d types(water=%d forest=%d field=%d grass=%d scrub=%d stone=%d) contacts(shore=%d forestEdge=%d) alt=(%.6f,%.6f) meanWetness=%.6f meanShore=%.6f"),
			Label, ScanSeed, Candidate.Size, Candidate.X, Candidate.Y, Candidate.Score,
			Candidate.Water, Candidate.Forest, Candidate.Field, Candidate.Grass, Candidate.Scrub, Candidate.Stone,
			Candidate.ShoreContacts, Candidate.ForestEdgeContacts, Candidate.MinAlt, Candidate.MaxAlt,
			Candidate.MeanWetness, Candidate.MeanShore);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldSemanticSliceScanTest,
	"Anastasis.WorldVisual.SemanticSliceScan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldSemanticSliceScanTest::RunTest(const FString&)
{
	const AnastasisWorld::FWorld World = AnastasisWorld::GenerateWorld(ScanSeed, WorldWidth, WorldHeight);
	TestEqual(TEXT("canonical tile count"), World.Tiles.Num(), WorldWidth * WorldHeight);

	for (const int32 Size : { 16, 32 })
	{
		FWindowScore Best;
		for (int32 Y = 0; Y <= WorldHeight - Size; ++Y)
		{
			for (int32 X = 0; X <= WorldWidth - Size; ++X)
			{
				Consider(Best, ScoreWindow(World, X, Y, Size));
			}
		}

		const TCHAR* Label = Size == 16 ? TEXT("BEST16") : TEXT("BEST32");
		LogCandidate(Label, Best);
		TestTrue(*FString::Printf(TEXT("best %dx%d has water"), Size, Size), Best.Water > 0);
		TestTrue(*FString::Printf(TEXT("best %dx%d has forest"), Size, Size), Best.Forest > 0);
		TestTrue(*FString::Printf(TEXT("best %dx%d has field"), Size, Size), Best.Field > 0);
		TestTrue(*FString::Printf(TEXT("best %dx%d has non-forest clearing material"), Size, Size), Best.Grass + Best.Scrub > 0);
		TestTrue(*FString::Printf(TEXT("best %dx%d has shore contact"), Size, Size), Best.ShoreContacts > 0);
	}

	return true;
}

#endif
