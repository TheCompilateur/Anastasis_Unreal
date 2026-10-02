#include "Sim/AnastasisBuildingCapacity.h"

#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"

FAnastasisBuildingCapacitySnapshot FAnastasisBuildingCapacitySnapshot::Capture(const AnastasisVillage::FVillage& Village, double AtTime)
{
	using namespace AnastasisVillage;
	FAnastasisBuildingCapacitySnapshot S;
	S.Time = AtTime;
	for (const FBuilding& B : Village.GetBuildings())
	{
		if (!B.IsCompleted())
		{
			++S.ActiveSites;
			S.SitePieces += B.PiecesPlaced;
			continue;
		}
		if (B.Type == HouseType)
		{
			++S.CompletedHouses;
			S.OccupiedHouses += !B.Owner.IsEmpty() ? 1 : 0;
		}
		else if (B.Type == GranaryType)
		{
			++S.CompletedGranaries;
			S.FoodPhysical += B.FoodPhysical;
			S.FoodAvailable += B.FoodAvailable();
			S.FoodReserved += B.FoodReserved;
		}
	}
	for (const FNpc& N : Village.GetActors())
	{
		++S.People;
		if (!N.HomeId.IsEmpty()) ++S.PeopleWithHome;
		else if (!N.ShelterId.IsEmpty()) ++S.PeopleWithShelter;
		else ++S.Homeless;
		S.GoalBuild += N.Goal == AnastasisBuild::GoalBuild ? 1 : 0;
		S.GoalGather += N.Goal == AnastasisGather::GoalGatherFood ? 1 : 0;
		S.GoalDeliver += N.Goal == AnastasisGather::GoalDeliver ? 1 : 0;
		S.ActiveBuildSessions += N.WorkSession.bActive && N.WorkSession.CraftId == AnastasisBuild::CraftBuild ? 1 : 0;
		S.ActiveFarmSessions += N.WorkSession.bActive && N.WorkSession.CraftId == TEXT("farm") ? 1 : 0;
		S.Meals += N.MealsTaken;
		S.Rests += N.RestsTaken;
		S.GatheredFood += N.GatheredFood;
		S.DeliveredFood += N.DeliveredFood;
		S.PiecesPlaced += N.PiecesPlaced;
		S.MeanEnergy += N.Needs.Energy;
		S.MeanHunger += N.Needs.Hunger;
		S.MeanMorale += N.Needs.Morale;
		S.CriticalHunger += N.Needs.Hunger >= AnastasisNeeds::Constants::HungerCritical ? 1 : 0;
		S.CriticalEnergy += N.Needs.Energy <= 100.0 - AnastasisNeeds::Constants::FatigueCritical ? 1 : 0;
	}
	if (S.People > 0)
	{
		S.MeanEnergy /= S.People;
		S.MeanHunger /= S.People;
		S.MeanMorale /= S.People;
	}
	return S;
}

FString FAnastasisBuildingCapacitySnapshot::ToJson() const
{
	return FString::Printf(
		TEXT("{\"time\":%.3f,\"people\":%d,\"houses\":%d,\"occupiedHouses\":%d,\"peopleWithHome\":%d,\"peopleWithShelter\":%d,\"homeless\":%d,")
		TEXT("\"granaries\":%d,\"foodPhysical\":%d,\"foodAvailable\":%d,\"foodReserved\":%d,\"activeSites\":%d,\"sitePieces\":%d,")
		TEXT("\"intentBuild\":%d,\"intentGather\":%d,\"intentDeliver\":%d,\"activeBuildSessions\":%d,\"activeFarmSessions\":%d,\"meals\":%d,\"rests\":%d,\"gatheredFood\":%d,\"deliveredFood\":%d,\"piecesPlaced\":%d,")
		TEXT("\"meanEnergy\":%.3f,\"meanHunger\":%.3f,\"meanMorale\":%.3f,\"criticalHunger\":%d,\"criticalEnergy\":%d}"),
		Time, People, CompletedHouses, OccupiedHouses, PeopleWithHome, PeopleWithShelter, Homeless,
		CompletedGranaries, FoodPhysical, FoodAvailable, FoodReserved, ActiveSites, SitePieces,
		GoalBuild, GoalGather, GoalDeliver, ActiveBuildSessions, ActiveFarmSessions, Meals, Rests, GatheredFood, DeliveredFood, PiecesPlaced,
		MeanEnergy, MeanHunger, MeanMorale, CriticalHunger, CriticalEnergy);
}
