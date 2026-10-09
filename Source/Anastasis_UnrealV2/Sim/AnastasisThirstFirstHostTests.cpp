// soif-dabord-001 (ecart n°58) -- le monde 1204, ou l'etude d'une annee (annee-valmire-001) a vu tout le village mourir de
// soif en six jours a cote d'un puits qui marchait : huit jours, la regle allumee (le jeu) puis eteinte (la reference).

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Sim/AnastasisTimeWarp.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisThirstFirstHostTest
{
	struct FResult
	{
		bool bSeeded = false;
		int32 Alive = 0;
		int32 ThirstDeaths = 0;
		int32 Deaths = 0;
		int32 Drinks = 0;
		FString Causes;
	};

	FResult Play(bool bThirstFirst, uint32 Seed, int32 Days)
	{
		FResult R;
		IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Village.ThirstFirst"));
		const int32 Before = Var ? Var->GetInt() : 1;
		if (Var) Var->Set(bThirstFirst ? 1 : 0, ECVF_SetByCode);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		if (UAnastasisSimulationSubsystem* Host = World->GetSubsystem<UAnastasisSimulationSubsystem>())
		{
			Host->ResetCanonical(Seed);
			const AnastasisVillage::FPoint S = Host->GetSimulation().GetVillage().GetSettlement();
			R.bSeeded = !Host->SeedStartVillage(12, FMath::FloorToInt32(S.X), FMath::FloorToInt32(S.Y)).IsEmpty();
			FAnastasisSimulation& Sim = Host->GetSimulation();
			for (int32 I = 0; R.bSeeded && I < Days * 6; ++I)
			{
				AnastasisTimeWarp::Advance(Sim, FAnastasisSimulation::DayLength / 6.0);
			}
			const AnastasisVillage::FVillage& V = Sim.GetVillage();
			R.Alive = V.GetActors().Num();
			R.Deaths = V.GetDeaths().Num();
			TMap<FString, int32> ByCause;
			for (const AnastasisVillage::FVillage::FDeath& D : V.GetDeaths())
			{
				if (D.Cause.Contains(TEXT("soif"))) ++R.ThirstDeaths;
				++ByCause.FindOrAdd(D.Cause);
			}
			for (const TPair<FString, int32>& C : ByCause) R.Causes += FString::Printf(TEXT("%s%s=%d"), R.Causes.IsEmpty() ? TEXT("") : TEXT(","), *C.Key, C.Value);
			for (const AnastasisVillage::FNpc& N : V.GetActors()) R.Drinks += N.DrinksTaken;
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		if (Var) Var->Set(Before, ECVF_SetByCode);
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisThirstFirstWorldTest,
	"Anastasis.Village.Soif.Monde1204",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisThirstFirstWorldTest::RunTest(const FString&)
{
	using namespace AnastasisThirstFirstHostTest;
	const FResult On = Play(true, 1204u, 8);
	const FResult Off = Play(false, 1204u, 8);
	if (!TestTrue(TEXT("monde 1204 : village pose"), On.bSeeded && Off.bSeeded)) return false;
	AddInfo(FString::Printf(TEXT("THIRST_FIRST_1204 regle alive=%d deaths=%d de_soif=%d causes=[%s] | reference alive=%d deaths=%d de_soif=%d causes=[%s]"),
		On.Alive, On.Deaths, On.ThirstDeaths, *On.Causes, Off.Alive, Off.Deaths, Off.ThirstDeaths, *Off.Causes));
	TestEqual(TEXT("la regle allumee : personne ne meurt de soif a cote du puits"), On.ThirstDeaths, 0);
	// La reference en meurt (le defaut que la regle corrige) ; si elle n'en mourait plus, ce test ne prouverait plus rien.
	TestTrue(TEXT("temoin : sans la regle, on meurt de soif dans ce monde"), Off.ThirstDeaths > 0);
	return true;
}

#endif
