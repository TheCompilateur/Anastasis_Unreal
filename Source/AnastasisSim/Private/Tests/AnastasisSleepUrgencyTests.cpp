#include "Misc/AutomationTest.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisSleepUrgencyTest
{
	using namespace AnastasisVillage;
	constexpr double Dt = 1.0 / 60.0;

	// Captured placement from the four-person PIE fixture (seed 12345, 96x96).
	// Same starting needs, building order, owner, shelters, stock and simulation clock.
	bool Setup(FAnastasisSimulation& Sim)
	{
		Sim.Reset(12345, 96, 96);
		auto& V = Sim.GetVillage();
		const FString Home = V.AddBuilding(HouseType, 47, 47);
		const FString Shelter = V.AddBuilding(HouseType, 53, 47);
		const FPoint Positions[] = {{52.5,46.5}, {47.5,53.5}, {41.5,47.5}, {47.5,41.5}};
		for (int32 I = 0; I < 4; ++I)
		{
			AnastasisNeeds::FNeeds N;
			N.Hunger=10; N.Energy=70-20*I; N.Social=70; N.Leisure=70;
			N.Hygiene=60; N.Thirst=10; N.Health=90; N.Morale=55;
			if (V.IsFootBlocked(Positions[I].X, Positions[I].Y)) return false;
			const FString Id=V.SpawnNpc(Positions[I].X, Positions[I].Y, N, 4);
			if (I==0 && !V.AssignHome(Id, Home)) return false;
		}
		if (V.AssignSheltersDaily()!=3) return false;
		const FString Well=V.AddBuilding(WellType,47,46);
		const FString Depot=V.AddBuilding(GranaryType,51,49);
		return !Home.IsEmpty() && !Shelter.IsEmpty() && !Well.IsEmpty() && !Depot.IsEmpty()
			&& V.CreditFood(Depot,96)==96;
	}

	bool Valid(const FVillage& V)
	{
		int32 Total=0;
		for (const auto& B:V.GetBuildings())
		{
			if (B.FoodPhysical < B.FoodReserved || B.FoodReserved < 0) return false;
			Total+=B.FoodPhysical;
		}
		for (const auto& N:V.GetActors())
		{
			if (!N.Inside.bActive && V.IsFootBlocked(N.X,N.Y)) return false;
			if (N.InventoryFood<0) return false;
			Total+=N.InventoryFood+N.MealsTaken;
		}
		return Total==96;
	}

	struct FObservation
	{
		int32 StaleSleepThoughts=0;
		int32 HighEnergyRestEntries=0;
		void Observe(const FNpc& Before,const FNpc& After)
		{
			// Thoughts only: an indoor sleeper does not reconsider every tick.
			if (After.AiThinkAt!=Before.AiThinkAt && After.Needs.Energy>=90
				&& After.AlgoDecision.Type==TEXT("sleep") && After.AlgoDecision.Urgency>=0.85)
				++StaleSleepThoughts;
			if (!Before.Inside.bActive && After.Inside.bActive && After.Inside.Goal==GoalRest
				&& After.Needs.Energy>=95) ++HighEnergyRestEntries;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRetainedSleepScalarTest,
	"Anastasis.Sim.Village.SleepUrgency.RetainedScalar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRetainedSleepScalarTest::RunTest(const FString&)
{
	using namespace AnastasisSleepUrgencyTest;
	AnastasisWorld::FWorld W; W.W=16; W.H=16; W.Tiles.SetNum(256);
	for (int32 I=0;I<256;++I) { W.Tiles[I].X=I%16; W.Tiles[I].Y=I/16; W.Tiles[I].Alt=0.5; }
	for (bool bRefresh : {false,true})
	{
		for (double Energy : {100.0,10.0})
		{
			FVillage V; V.Bind(W); V.SetRefreshRetainedSleepUrgency(bRefresh);
			AnastasisNeeds::FNeeds Needs; Needs.Hunger=40; Needs.Energy=Energy;
			const FString Id=V.SpawnNpc(8.5,8.5,Needs);
			auto* N=V.FindNpcMutable(Id);
			N->AiThinkAt=0; N->bHasAlgoDecision=true;
			auto& D=N->AlgoDecision;
			D.Type=TEXT("sleep"); D.Score=0.35; D.Urgency=0.9;
			D.CreatedAt=17; D.TargetId=TEXT("retained-target"); D.Reason=TEXT("retained-reason");
			D.Raw=49; D.ExpectedDuration=11.5;
			V.UpdateActors(37.8,0);
			TestTrue(TEXT("Inertia retained the decision"),N->bAlgoInertiaKeep);
			TestEqual(TEXT("Action unchanged"),D.Type,FString(TEXT("sleep")));
			TestEqual(TEXT("Only urgency: score unchanged"),D.Score,0.35);
			TestEqual(TEXT("Only urgency: creation time unchanged"),D.CreatedAt,17.0);
			TestEqual(TEXT("Only urgency: target unchanged"),D.TargetId,FString(TEXT("retained-target")));
			TestEqual(TEXT("Only urgency: reason unchanged"),D.Reason,FString(TEXT("retained-reason")));
			TestEqual(TEXT("Only urgency: raw unchanged"),D.Raw,49.0);
			TestEqual(TEXT("Only urgency: duration unchanged"),D.ExpectedDuration,11.5);
			TestEqual(TEXT("Urgency follows energy only when enabled"),D.Urgency,bRefresh ? (100-Energy)/100 : 0.9);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedSleepVillageTest,
	"Anastasis.Sim.Village.SleepUrgency.FourPeopleTwoDaysAB",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedSleepVillageTest::RunTest(const FString&)
{
	using namespace AnastasisSleepUrgencyTest;
	FAnastasisSimulation Default, Legacy, Refresh;
	if (!Setup(Default) || !Setup(Legacy) || !Setup(Refresh))
		return AddError(TEXT("Fixture no longer matches the recorded generated world")),false;
	Legacy.GetVillage().SetRefreshRetainedSleepUrgency(false);
	Refresh.GetVillage().SetRefreshRetainedSleepUrgency(true);
	FObservation A[4], B[4];
	for (int32 Tick=0;Tick<10800;++Tick)
	{
		const auto BeforeA=Legacy.GetVillage().GetActors();
		const auto BeforeB=Refresh.GetVillage().GetActors();
		Default.Tick(Dt); Legacy.Tick(Dt); Refresh.Tick(Dt);
		if (Default.GetVillage().Digest()!=Legacy.GetVillage().Digest())
			return AddError(TEXT("Default behavior differs from explicit legacy mode")),false;
		if (!Valid(Legacy.GetVillage()) || !Valid(Refresh.GetVillage()))
			return AddError(FString::Printf(TEXT("Food conservation or navigation failed at tick %d"),Tick)),false;
		for (int32 I=0;I<4;++I)
		{
			A[I].Observe(BeforeA[I],Legacy.GetVillage().GetActors()[I]);
			B[I].Observe(BeforeB[I],Refresh.GetVillage().GetActors()[I]);
		}
	}
	for (int32 I=0;I<4;++I)
	{
		for (int32 Mode=0;Mode<2;++Mode)
		{
			const auto& N=(Mode ? Refresh : Legacy).GetVillage().GetActors()[I];
			const auto& O=Mode ? B[I] : A[I];
			AddInfo(FString::Printf(TEXT("SLEEP_AB mode=%s npc=%d staleThoughts=%d highEnergyRestEntries=%d rests=%d drinks=%d meals=%d energy=%.9f hunger=%.9f thirst=%.9f health=%.9f urgency=%.9f"),
				Mode ? TEXT("refresh") : TEXT("legacy"),I,O.StaleSleepThoughts,O.HighEnergyRestEntries,
				N.RestsTaken,N.DrinksTaken,N.MealsTaken,N.Needs.Energy,N.Needs.Hunger,N.Needs.Thirst,N.Needs.Health,N.AlgoDecision.Urgency));
		}
	}
	TestTrue(TEXT("Baseline reproduces recovered npc-3 with stale urgent sleep"),A[3].StaleSleepThoughts>0);
	TestEqual(TEXT("Refresh removes stale urgency on npc-3 thoughts"),B[3].StaleSleepThoughts,0);
	TestTrue(TEXT("Single-scalar intervention reduces npc-3 high-energy rest entries"),B[3].HighEnergyRestEntries<A[3].HighEnergyRestEntries);
	TestTrue(TEXT("Npc-3 completes fewer repeated rests"),Refresh.GetVillage().GetActors()[3].RestsTaken<Legacy.GetVillage().GetActors()[3].RestsTaken);
	AddInfo(FString::Printf(TEXT("SLEEP_AB duration=180 ticks=10800 stockLegacy=%d stockRefresh=%d defaultMatchesLegacyEveryTick=1 conservationEveryTick=1"),
		Legacy.GetVillage().FindBuilding(TEXT("building-3"))->FoodPhysical,Refresh.GetVillage().FindBuilding(TEXT("building-3"))->FoodPhysical));
	return true;
}
#endif
