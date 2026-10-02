#include "Misc/AutomationTest.h"

#include "Sim/AnastasisBuildingCapacity.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisBuildingCapacityTest
{
	using namespace AnastasisVillage;
	constexpr double Dt = 1.0 / 60.0;
	constexpr int32 Days = 3;

	enum class EIntervention : uint8 { House, Granary, Site };

	AnastasisWorld::FWorld MakeWorld()
	{
		AnastasisWorld::FWorld W;
		W.W = 40;
		W.H = 40;
		W.Tiles.SetNum(W.W * W.H);
		for (int32 Y = 0; Y < W.H; ++Y)
		{
			for (int32 X = 0; X < W.W; ++X)
			{
				auto& T = W.Tiles[Y * W.W + X];
				T.X = X;
				T.Y = Y;
				T.Type = AnastasisWorld::ETileType::Grass;
				T.Alt = 0.5;
				T.Wetness = 0.3;
			}
		}
		for (int32 Y = 11; Y <= 14; ++Y)
		{
			for (int32 X = 9; X <= 12; ++X)
			{
				auto& T = W.Tiles[Y * W.W + X];
				T.Type = AnastasisWorld::ETileType::Field;
				T.Resource = AnastasisWorld::EResource::Food;
				T.Amount = 20;
				T.CropId = AnastasisWorld::ECropId::Grain;
				T.Fertility = 1.0;
			}
		}
		return W;
	}

	AnastasisNeeds::FNeeds InitialNeeds()
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 22.0;
		N.Energy = 80.0;
		N.Thirst = 10.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		N.Social = 80.0;
		N.Leisure = 80.0;
		N.Hygiene = 80.0;
		return N;
	}

	struct FRun
	{
		bool bSetup = false;
		TArray<FAnastasisBuildingCapacitySnapshot> Days;
		TArray<double> BuildSessionPersonSeconds;
		TArray<double> FarmSessionPersonSeconds;
	};

	FRun Run(EIntervention Kind, bool bEnabled)
	{
		AnastasisWorld::FWorld World = MakeWorld();
		FVillage V;
		V.Bind(World);
		FRun R;
		const FString Well = V.AddBuilding(WellType, 18, 16);
		const bool bHasGranary = Kind != EIntervention::Granary || bEnabled;
		const FString Granary = bHasGranary ? V.AddBuilding(GranaryType, 16, 12) : FString();
		if (Well.IsEmpty() || (bHasGranary && Granary.IsEmpty())) return R;
		if (bHasGranary && V.CreditFood(Granary, 12) != 12) return R;
		TArray<FString> People;
		for (int32 I = 0; I < 4; ++I)
		{
			People.Add(V.SpawnNpc(15.5 + (I % 2), 14.5 + (I / 2), InitialNeeds()));
			if (People.Last().IsEmpty()) return R;
			if (I < 2)
			{
				// The granary supplies both a storage point and a valid farmer workplace.
				if (bHasGranary && !V.AssignWorkplace(People.Last(), AnastasisGather::JobFarmer, Granary)) return R;
				if (!bHasGranary && !V.SetJob(People.Last(), AnastasisGather::JobFarmer)) return R;
			}
		}
		if (Kind == EIntervention::House && bEnabled)
		{
			const FString House = V.AddBuilding(HouseType, 20, 17);
			if (House.IsEmpty() || !V.AssignHome(People[0], House)) return R;
		}
		if (Kind == EIntervention::Site && bEnabled && V.OpenSite(HouseType, 22, 16, true).IsEmpty()) return R;
		R.bSetup = true;
		double Time = 27.0;
		R.Days.Add(FAnastasisBuildingCapacitySnapshot::Capture(V, Time));
		R.BuildSessionPersonSeconds.Add(0.0);
		R.FarmSessionPersonSeconds.Add(0.0);
		constexpr int32 TicksPerDay = 90 * 60;
		for (int32 Day = 1; Day <= Days; ++Day)
		{
			double BuildSeconds = 0.0;
			double FarmSeconds = 0.0;
			for (int32 Tick = 0; Tick < TicksPerDay; ++Tick)
			{
				Time += Dt;
				V.UpdateActors(Time, Dt);
				for (const FNpc& N : V.GetActors())
				{
					BuildSeconds += N.WorkSession.bActive && N.WorkSession.CraftId == AnastasisBuild::CraftBuild ? Dt : 0.0;
					FarmSeconds += N.WorkSession.bActive && N.WorkSession.CraftId == TEXT("farm") ? Dt : 0.0;
				}
			}
			R.Days.Add(FAnastasisBuildingCapacitySnapshot::Capture(V, Time));
			R.BuildSessionPersonSeconds.Add(BuildSeconds);
			R.FarmSessionPersonSeconds.Add(FarmSeconds);
		}
		return R;
	}

	const TCHAR* Name(EIntervention Kind)
	{
		switch (Kind)
		{
		case EIntervention::House: return TEXT("house");
		case EIntervention::Granary: return TEXT("granary");
		case EIntervention::Site: return TEXT("site");
		}
		return TEXT("unknown");
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisBuildingCapacityThreeDayABTest,
	"Anastasis.Gameplay.BuildingCapacities.ThreeDayAB",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisBuildingCapacityThreeDayABTest::RunTest(const FString&)
{
	using namespace AnastasisBuildingCapacityTest;
	for (const EIntervention Kind : { EIntervention::House, EIntervention::Granary, EIntervention::Site })
	{
		const FRun Control = Run(Kind, false);
		const FRun Intervention = Run(Kind, true);
		if (!TestTrue(*FString::Printf(TEXT("%s: two scenarios constructed"), Name(Kind)), Control.bSetup && Intervention.bSetup)) return false;
		TestEqual(TEXT("control has day 0..3"), Control.Days.Num(), Days + 1);
		TestEqual(TEXT("intervention has day 0..3"), Intervention.Days.Num(), Days + 1);
		if (Control.Days.Num() != Days + 1 || Intervention.Days.Num() != Days + 1) return false;
		TestEqual(TEXT("same population"), Control.Days[0].People, Intervention.Days[0].People);
		if (Kind == EIntervention::House) TestEqual(TEXT("a home is assigned"), Intervention.Days[0].PeopleWithHome, 1);
		if (Kind == EIntervention::Granary) TestTrue(TEXT("food available only with granary"),
			Control.Days[0].FoodAvailable == 0 && Intervention.Days[0].FoodAvailable == 12);
		if (Kind == EIntervention::Site) TestEqual(TEXT("one active site"), Intervention.Days[0].ActiveSites, 1);
		for (int32 Day = 0; Day <= Days; ++Day)
		{
			const auto& A = Control.Days[Day];
			const auto& B = Intervention.Days[Day];
			AddInfo(FString::Printf(TEXT("BUILDING_CAPACITY_AB {\"intervention\":\"%s\",\"day\":%d,\"arm\":\"control\",\"snapshot\":%s}"), Name(Kind), Day, *A.ToJson()));
			AddInfo(FString::Printf(TEXT("BUILDING_CAPACITY_AB {\"intervention\":\"%s\",\"day\":%d,\"arm\":\"building\",\"snapshot\":%s}"), Name(Kind), Day, *B.ToJson()));
			AddInfo(FString::Printf(
				TEXT("BUILDING_CAPACITY_DELTA {\"intervention\":\"%s\",\"day\":%d,\"meals\":%d,\"rests\":%d,\"gatheredFood\":%d,\"deliveredFood\":%d,\"piecesPlaced\":%d,\"buildSessionPersonSeconds\":%.3f,\"farmSessionPersonSeconds\":%.3f,\"meanEnergy\":%.3f,\"meanHunger\":%.3f,\"meanMorale\":%.3f}"),
				Name(Kind), Day, B.Meals - A.Meals, B.Rests - A.Rests, B.GatheredFood - A.GatheredFood,
				B.DeliveredFood - A.DeliveredFood, B.PiecesPlaced - A.PiecesPlaced,
				Intervention.BuildSessionPersonSeconds[Day] - Control.BuildSessionPersonSeconds[Day],
				Intervention.FarmSessionPersonSeconds[Day] - Control.FarmSessionPersonSeconds[Day],
				B.MeanEnergy - A.MeanEnergy, B.MeanHunger - A.MeanHunger, B.MeanMorale - A.MeanMorale));
		}
	}
	return true;
}

#endif
