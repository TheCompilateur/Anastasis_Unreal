#pragma once

#include "CoreMinimal.h"

namespace AnastasisVillage { class FVillage; }

/** Read-only observation of what buildings currently enable and what villagers actually did. */
struct FAnastasisBuildingCapacitySnapshot
{
	double Time = 0.0;
	int32 People = 0;
	int32 CompletedHouses = 0;
	int32 OccupiedHouses = 0;
	int32 PeopleWithHome = 0;
	int32 PeopleWithShelter = 0;
	int32 Homeless = 0;
	int32 CompletedGranaries = 0;
	int32 FoodPhysical = 0;
	int32 FoodAvailable = 0;
	int32 FoodReserved = 0;
	int32 ActiveSites = 0;
	int32 SitePieces = 0;
	int32 GoalBuild = 0;
	int32 GoalGather = 0;
	int32 GoalDeliver = 0;
	int32 ActiveBuildSessions = 0;
	int32 ActiveFarmSessions = 0;
	int32 Meals = 0;
	int32 Rests = 0;
	int32 GatheredFood = 0;
	int32 DeliveredFood = 0;
	int32 PiecesPlaced = 0;
	double MeanEnergy = 0.0;
	double MeanHunger = 0.0;
	double MeanMorale = 0.0;
	int32 CriticalHunger = 0;
	int32 CriticalEnergy = 0;

	static FAnastasisBuildingCapacitySnapshot Capture(const AnastasisVillage::FVillage& Village, double AtTime);
	FString ToJson() const;
};
