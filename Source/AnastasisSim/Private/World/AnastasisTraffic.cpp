#include "World/AnastasisTraffic.h"

#include "Core/AnastasisJsNumeric.h"

namespace AnastasisTraffic
{
	namespace
	{
		// `ROAD_PROFILES.path / lane / main` (simulation.js:738).
		const FRoadProfile Profiles[] = {
			{ TEXT("path"), 2, 0.86, 36.0 },
			{ TEXT("lane"), 3, 0.74, 46.0 },
			{ TEXT("main"), 4, 0.52, 88.0 },
		};
	}

	const FRoadProfile& ProfileOf(ERoadClass Class)
	{
		return Profiles[FMath::Clamp(static_cast<int32>(Class), 0, 2)];
	}

	ERoadClass RoadClassForTraffic(double Value)
	{
		if (Value >= ProfileOf(ERoadClass::Main).Traffic) return ERoadClass::Main;
		if (Value >= ProfileOf(ERoadClass::Lane).Traffic) return ERoadClass::Lane;
		return ERoadClass::Path;
	}

	ERoadClass StrongerRoadClass(ERoadClass Current, ERoadClass Next)
	{
		return ProfileOf(Next).Rank >= ProfileOf(Current).Rank ? Next : Current;
	}

	bool IsRoadReclaimable(const AnastasisWorld::FTile& Tile, bool bBuilding)
	{
		using AnastasisWorld::ETileType;
		if (Tile.Type == ETileType::Water || Tile.Type == ETileType::Road) return false;
		if (Tile.Type == ETileType::Stone || Tile.Type == ETileType::Ruin) return false;
		if (bBuilding) return false;
		// ecart n°42 : la reference rase culture et bois (`claimRoadFootprint`) ; le C++ garde la ressource.
		if (Tile.Resource != AnastasisWorld::EResource::None && Tile.Amount > 0) return false;
		return Tile.Type == ETileType::Grass
			|| Tile.Type == ETileType::Scrub
			|| Tile.Type == ETileType::Field
			|| Tile.Type == ETileType::Forest;
	}

	int32 RoadClearEffortDays(const AnastasisWorld::FTile& Tile)
	{
		using AnastasisWorld::ETileType;
		int32 Days = PathDays;
		if (Tile.Type == ETileType::Field) Days += FieldClearExtraDays;
		if (Tile.Type == ETileType::Forest) Days += ForestBaseExtraDays;
		if (Tile.Resource == AnastasisWorld::EResource::Wood && Tile.Amount > 0)
		{
			Days += FMath::CeilToInt32(static_cast<double>(Tile.Amount) / WoodClearPerDay);
		}
		return Days;
	}

	int32 DecayFootTraffic(TArray<float>& Traffic)
	{
		int32 Touched = 0;
		for (float& Cell : Traffic)
		{
			const double Prev = Cell;
			if (!(Prev > 0.0)) continue;
			double Next = Prev * FootMul - FootSub;
			if (Next < FootEpsilon) Next = 0.0;
			if (Next != Prev)
			{
				Cell = AnastasisJs::StoreF32(Next);
				++Touched;
			}
		}
		return Touched;
	}

	float AddPassage(float Previous)
	{
		return AnastasisJs::StoreF32(FMath::Min(PassageCap, static_cast<double>(Previous) + 1.0));
	}

	bool AdvanceDesireEffort(double Traffic, int32 ClearDays, double& InOutEffort)
	{
		if (Traffic < DesireTraffic)
		{
			if (InOutEffort > 0.0) InOutEffort *= PathEffortDecay;
			if (InOutEffort < 0.25) InOutEffort = -1.0;
			return false;
		}
		InOutEffort = FMath::Max(0.0, InOutEffort) + 1.0 + FMath::Min(0.75, (Traffic - DesireTraffic) / 42.0);
		return InOutEffort >= ClearDays;
	}
}
