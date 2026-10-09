// faim-champs-001 (ecart n°59) -- le vrai village du lancement, soixante jours, la regle allumee (le jeu) puis eteinte (la
// reference), dans les mondes 12345 (la famille du Scribe y mourait de faim) et 1204 (le grenier y restait vide).

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Sim/AnastasisTimeWarp.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisGather.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisFieldHandsHostTest
{
	struct FResult
	{
		bool bSeeded = false;
		int32 Alive = 0;
		int32 HungerDeaths = 0;
		int32 Deaths = 0;
		int32 EmptyEvenings = 0;
		int32 Farmers = 0;
		int32 Hired = 0;
		FString Causes;
	};

	FResult Play(bool bFieldHands, uint32 Seed, int32 Days)
	{
		FResult R;
		IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Village.FieldHands"));
		const int32 Before = Var ? Var->GetInt() : 1;
		if (Var) Var->Set(bFieldHands ? 1 : 0, ECVF_SetByCode);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		if (UAnastasisSimulationSubsystem* Host = World->GetSubsystem<UAnastasisSimulationSubsystem>())
		{
			Host->ResetCanonical(Seed);
			const AnastasisVillage::FPoint S = Host->GetSimulation().GetVillage().GetSettlement();
			R.bSeeded = !Host->SeedStartVillage(12, FMath::FloorToInt32(S.X), FMath::FloorToInt32(S.Y)).IsEmpty();
			FAnastasisSimulation& Sim = Host->GetSimulation();
			for (int32 Day = 0; R.bSeeded && Day < Days; ++Day)
			{
				for (int32 I = 0; I < 6; ++I) AnastasisTimeWarp::Advance(Sim, FAnastasisSimulation::DayLength / 6.0);
				int32 Stock = 0;
				for (const AnastasisVillage::FBuilding& B : Sim.GetVillage().GetBuildings())
				{
					if (B.Type == AnastasisVillage::GranaryType && B.IsCompleted()) Stock += B.FoodPhysical;
				}
				if (Stock == 0) ++R.EmptyEvenings;
			}
			const AnastasisVillage::FVillage& V = Sim.GetVillage();
			R.Alive = V.GetActors().Num();
			R.Deaths = V.GetDeaths().Num();
			R.Hired = V.GetFieldHandsHired();
			TMap<FString, int32> ByCause;
			for (const AnastasisVillage::FVillage::FDeath& D : V.GetDeaths())
			{
				if (D.Cause.Contains(TEXT("faim"))) ++R.HungerDeaths;
				++ByCause.FindOrAdd(D.Cause);
			}
			for (const TPair<FString, int32>& C : ByCause) R.Causes += FString::Printf(TEXT("%s%s=%d"), R.Causes.IsEmpty() ? TEXT("") : TEXT(","), *C.Key, C.Value);
			for (const AnastasisVillage::FNpc& N : V.GetActors()) if (N.JobId == AnastasisGather::JobFarmer) ++R.Farmers;
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		if (Var) Var->Set(Before, ECVF_SetByCode);
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisFieldHandsWorldTest,
	"Anastasis.Village.Faim.SoixanteJours",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisFieldHandsWorldTest::RunTest(const FString&)
{
	using namespace AnastasisFieldHandsHostTest;
	for (const uint32 Seed : { 12345u, 1204u })
	{
		const FResult On = Play(true, Seed, 60);
		const FResult Off = Play(false, Seed, 60);
		if (!TestTrue(FString::Printf(TEXT("monde %u : village pose"), Seed), On.bSeeded && Off.bSeeded)) continue;
		AddInfo(FString::Printf(TEXT("FIELD_HANDS_60 seed=%u regle alive=%d deaths=%d faim=%d soirs_vides=%d cultivateurs=%d embauches=%d causes=[%s] | reference alive=%d deaths=%d faim=%d soirs_vides=%d cultivateurs=%d causes=[%s]"),
			Seed, On.Alive, On.Deaths, On.HungerDeaths, On.EmptyEvenings, On.Farmers, On.Hired, *On.Causes,
			Off.Alive, Off.Deaths, Off.HungerDeaths, Off.EmptyEvenings, Off.Farmers, *Off.Causes));
		TestEqual(FString::Printf(TEXT("monde %u : la reference n'embauche personne"), Seed), Off.Hired, 0);
		TestTrue(FString::Printf(TEXT("monde %u : pas plus de morts de faim avec la regle"), Seed), On.HungerDeaths <= Off.HungerDeaths);
		TestTrue(FString::Printf(TEXT("monde %u : pas plus de soirs a grenier vide avec la regle"), Seed), On.EmptyEvenings <= Off.EmptyEvenings);
	}
	return true;
}

#endif
