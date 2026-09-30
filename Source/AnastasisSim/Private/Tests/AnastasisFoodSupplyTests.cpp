#include "Misc/AutomationTest.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisFoodSupplyTest
{
	using namespace AnastasisVillage;
	AnastasisWorld::FWorld MakeWorld(int32 Portions)
	{
		AnastasisWorld::FWorld W; W.W = 32; W.H = 32; W.Tiles.SetNum(1024);
		for (int32 I = 0; I < W.Tiles.Num(); ++I)
		{
			auto& T = W.Tiles[I]; T.X = I % 32; T.Y = I / 32; T.Alt = 0.5;
		}
		auto& Food = W.Tiles[16 * 32 + 18];
		Food.Resource = AnastasisWorld::EResource::Food; Food.Amount = Portions;
		return W;
	}
	FString Worker(FVillage& V, double Hunger = 10, double X = 16.5, double Y = 16.5)
	{
		AnastasisNeeds::FNeeds N; N.Hunger = Hunger; N.Energy = 95; N.Hygiene = 95; N.Social = 95; N.Leisure = 95;
		return V.SpawnNpc(X, Y, N);
	}
	int32 Total(const FVillage& V)
	{
		int32 Sum = 0;
		for (const auto& S : V.GetFoodSources()) Sum += S.Remaining;
		for (const auto& B : V.GetBuildings()) Sum += B.FoodPhysical;
		for (const auto& N : V.GetActors()) Sum += N.InventoryFood + N.MealsTaken;
		return Sum;
	}
	bool Advance(FVillage& V, double& T, double Seconds, int32 Expected)
	{
		for (int32 I = 0; I < FMath::CeilToInt(Seconds * 60); ++I)
		{
			T += 1.0 / 60.0; V.UpdateActors(T, 1.0 / 60.0);
			if (Total(V) != Expected) return false;
			for (const auto& N : V.GetActors()) if (N.InventoryFood < 0 || (!N.Inside.bActive && V.IsFootBlocked(N.X, N.Y))) return false;
			for (const auto& B : V.GetBuildings()) if (B.FoodPhysical < B.FoodReserved || B.FoodPhysical > GranaryFoodCap) return false;
		}
		return true;
	}
}
#define FOOD_TEST(Class, Name) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, "Anastasis.Sim.Village.FoodSupply." Name, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
FOOD_TEST(FFoodSupplyLoopTest, "ConservationAndDepletion")
bool FFoodSupplyLoopTest::RunTest(const FString&)
{
	using namespace AnastasisFoodSupplyTest;
	const auto W = MakeWorld(6); FVillage V; V.Bind(W);
	const FString Depot = V.AddBuilding(GranaryType, 14, 16);
	TestTrue(TEXT("Activate actual resource"), V.ActivateFoodSource(18, 16));
	TestFalse(TEXT("No food injected on grass"), V.ActivateFoodSource(17, 16));
	const FString Id = Worker(V);
	double T = 37.8;
	bool SawCargo = false, SawDepot = false, SawMeal = false;
	double CarryDistance=0;
	for (int32 I = 0; I < 180 * 60; ++I)
	{
		const FNpc Before=*V.FindNpc(Id);
		if (!Advance(V, T, 1.0 / 60, 6)) return AddError(TEXT("Conservation/navigation failed")), false;
		const auto* N = V.FindNpc(Id);
		if(Before.InventoryFood>0) CarryDistance += FMath::Sqrt(FMath::Square(N->X-Before.X)+FMath::Square(N->Y-Before.Y));
		SawCargo |= N->InventoryFood > 0; SawDepot |= V.FindBuilding(Depot)->FoodPhysical > 0; SawMeal |= N->MealsTaken > 0;
	}
	TestTrue(TEXT("Observed carried food"), SawCargo);
	TestTrue(TEXT("Observed real deposit"), SawDepot);
	TestTrue(TEXT("Actual travel while carrying food"), CarryDistance>ArrivalDistance);
	TestTrue(TEXT("Observed meal"), SawMeal);
	TestEqual(TEXT("Source depleted"), V.GetFoodSources()[0].Remaining, 0);
	TestEqual(TEXT("All six harvested once"), V.FindNpc(Id)->GatheredFood, 6);
	V.ActivateFoodSource(18, 16);
	TestEqual(TEXT("Reactivation cannot refill"), V.GetFoodSources()[0].Remaining, 0);
	TestEqual(TEXT("Immutable generation preserved"), W.Tiles[16 * 32 + 18].Amount, 6);
	TestEqual(TEXT("All finite food consumed"),V.FindNpc(Id)->MealsTaken,6);
	const double HungryBefore=V.FindNpc(Id)->Needs.Hunger;
	TestTrue(TEXT("Empty circuit still conserves food"),Advance(V,T,5,6));
	TestTrue(TEXT("Hunger rises after depletion"),V.FindNpc(Id)->Needs.Hunger>HungryBefore);
	TestEqual(TEXT("No further meal after depletion"),V.FindNpc(Id)->MealsTaken,6);
	AddInfo(FString::Printf(TEXT("gathered=%d delivered=%d meals=%d stock=%d bag=%d hunger=%.2f"), V.FindNpc(Id)->GatheredFood, V.FindNpc(Id)->DeliveredFood, V.FindNpc(Id)->MealsTaken, V.FindBuilding(Depot)->FoodPhysical, V.FindNpc(Id)->InventoryFood, V.FindNpc(Id)->Needs.Hunger));
	return true;
}
FOOD_TEST(FFoodSupplyCompetitionTest, "CompetitionLastPortion")
bool FFoodSupplyCompetitionTest::RunTest(const FString&)
{
	using namespace AnastasisFoodSupplyTest;
	const auto W = MakeWorld(1); FVillage V; V.Bind(W); V.AddBuilding(GranaryType,14,16); V.ActivateFoodSource(18,16);
	Worker(V); Worker(V,10,17.5,16.5); Worker(V,10,18.5,17.5);
	double T=37.8; TestTrue(TEXT("Every tick conserves last portion"),Advance(V,T,45,1));
	int32 Gathered=0; for(const auto& N:V.GetActors()) Gathered+=N.GatheredFood;
	TestEqual(TEXT("Only one portion taken by competing gatherers"),Gathered,1);
	return true;
}
FOOD_TEST(FFoodSupplyFullDepotTest, "FullDepotAndRemovalKeepCargo")
bool FFoodSupplyFullDepotTest::RunTest(const FString&)
{
	using namespace AnastasisFoodSupplyTest;
	const auto W=MakeWorld(6); FVillage V; V.Bind(W); const FString B=V.AddBuilding(GranaryType,14,16); V.ActivateFoodSource(18,16);
	const FString Id=Worker(V); double T=37.8;
	for(int32 I=0;I<1200 && V.FindNpc(Id)->InventoryFood==0;++I) Advance(V,T,1.0/60,6);
	TestEqual(TEXT("Cargo collected before depot fills"),V.FindNpc(Id)->InventoryFood,2);
	V.CreditFood(B,GranaryFoodCap); // Only the adversarial fixture injects stock.
	// Isolate the deposit action: otherwise Noûs can legitimately eat the retained bag.
	auto* Carrier=V.FindNpcMutable(Id);
	const auto Door=V.FindBuilding(B)->AccessPoints[0];
	Carrier->X=Door.X; Carrier->Y=Door.Y; Carrier->Target=Door;
	Carrier->Goal=TEXT("deliver"); Carrier->DestBuildingId=B;
	Carrier->bHasTarget=true; Carrier->WorkTimer=0; Carrier->AiThinkAt=1000;
	TestTrue(TEXT("Full depot never loses/duplicates cargo"),Advance(V,T,1.1,306));
	TestEqual(TEXT("Cargo retained"),V.FindNpc(Id)->InventoryFood,2);
	V.RemoveBuilding(B);
	TestEqual(TEXT("Destroyed depot leaves bag intact"),V.FindNpc(Id)->InventoryFood,2);
	TestTrue(TEXT("No stale destination"),V.FindNpc(Id)->DestBuildingId.IsEmpty());
	return true;
}
FOOD_TEST(FFoodSupplyKnowledgeTest, "UnseenAndUnreachable")
bool FFoodSupplyKnowledgeTest::RunTest(const FString&)
{
	using namespace AnastasisFoodSupplyTest;
	auto W=MakeWorld(4);
	for(int32 Y=0;Y<32;++Y) W.Tiles[Y*32+17].Type=AnastasisWorld::ETileType::Water;
	FVillage V; V.Bind(W); V.AddBuilding(GranaryType,14,16); V.ActivateFoodSource(18,16); Worker(V);
	double T=37.8; TestTrue(TEXT("Unreachable source conserves stock"),Advance(V,T,20,4));
	TestEqual(TEXT("No gathering across water"),V.GetFoodSources()[0].Remaining,4);
	V.Bind(W); V.AddBuilding(GranaryType,2,2); V.ActivateFoodSource(18,16); const FString Id=Worker(V,10,3.5,3.5);
	T=37.8; TestTrue(TEXT("Unseen stock conserved"),Advance(V,T,20,4));
	TestEqual(TEXT("Source not magically known"),V.FindNpc(Id)->KnownFoodSources.Num(),0);
	TestEqual(TEXT("Unseen source untouched"),V.GetFoodSources()[0].Remaining,4);
	return true;
}
FOOD_TEST(FFoodSupplyEmptyTest, "EmptyGranaryDoesNotFeed")
bool FFoodSupplyEmptyTest::RunTest(const FString&)
{
	using namespace AnastasisFoodSupplyTest;
	const auto W=MakeWorld(1); FVillage V; V.Bind(W); const FString B=V.AddBuilding(GranaryType,14,16); V.ActivateFoodSource(18,16);
	const FString Id=Worker(V,80,14.5,15.5); auto* N=V.FindNpcMutable(Id);
	N->Goal=GoalEat; N->bHasTarget=true; N->Target={14.5,15.5}; N->AiThinkAt=1000;
	double T=37.8; TestTrue(TEXT("No matter created"),Advance(V,T,5,1));
	TestTrue(TEXT("Empty granary does not reduce hunger"),V.FindNpc(Id)->Needs.Hunger>=80);
	TestEqual(TEXT("No fictional meal"),V.FindNpc(Id)->MealsTaken,0);
	return true;
}
#undef FOOD_TEST
#endif
