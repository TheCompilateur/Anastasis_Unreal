#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"

#include "Harness/AnastasisHarnessTrace.h"
#include "Sim/AnastasisSimulation.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * L'emetteur de trace Unreal (mission sim-digest-emitter-001).
 *
 * Le scenario `endurance` est lu, REPRIS par l'hote (monde, horloge, village,
 * registre des repas), puis tourne ; chaque tick, l'etat vivant du C++ est
 * projete sur le perimetre du scenario et ecrit en JSONL au format de
 * emit-state-digests.mjs, dans Saved/HarnessTraces/.
 *
 * Ce test prouve l'instrument, pas la parite : (1) au tick 0, apres reprise par
 * l'hote, les empreintes sont celles de la REFERENCE (vecteurs de
 * gen-scenario-vectors.mjs) — la reprise ne perd rien de ce qui est lu ;
 * (2) la trace est complete ; (3) deux executions rendent les memes bits. La
 * comparaison avec la trace JS se fait par compare-digests.mjs.
 *
 * ANASTASIS_HARNESS_TICKS fixe la duree (defaut 600 ticks) ; ANASTASIS_HARNESS_DRILL=<tick>
 * ecrit les sections projetees a ce tick (forage, tools/migration/diff-states.mjs).
 */
namespace AnastasisHarnessTraceTest
{
	#include "AnastasisScenarioVectors.inl"
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisHarnessTraceTest,
	"Anastasis.Sim.Harnais.Trace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisHarnessTraceTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisHarnessTraceTest;

	const FString Scenario = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), ScenarioPath));
	const FString Out = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("HarnessTraces"),
		FString::Printf(TEXT("%s-unreal.jsonl"), ScenarioName)));

	int32 Ticks = 600;
	const FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("ANASTASIS_HARNESS_TICKS"));
	if (!Env.IsEmpty())
	{
		Ticks = FCString::Atoi(*Env);
	}
	// ANASTASIS_HARNESS_DRILL=<tick> : sections projetees a ce tick, en JSON, a cote de la trace.
	int32 Drill = -1;
	const FString DrillEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("ANASTASIS_HARNESS_DRILL"));
	if (!DrillEnv.IsEmpty())
	{
		Drill = FCString::Atoi(*DrillEnv);
	}

	// 0. La reprise elle-meme : l'hote porte les entites lues.
	{
		AnastasisHarnessTrace::FScenarioInfo Info;
		AnastasisJsSave::FState Read;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("scenario lu (%s)"), *Error), AnastasisHarnessTrace::LoadScenario(Scenario, Info, Read, Error)))
		{
			return false;
		}
		TestEqual(TEXT("scenario des vecteurs"), Info.Empreinte, FString(ScenarioEmpreinte));
		FAnastasisSimulation Sim;
		TestTrue(FString::Printf(TEXT("reprise par l'hote (%s)"), *Error), AnastasisHarnessTrace::Restore(Read, Sim, Error));
		TestEqual(TEXT("batiments repris"), Sim.GetVillage().GetBuildings().Num(), ScenarioBuildings);
		TestEqual(TEXT("habitants repris"), Sim.GetVillage().GetActors().Num(), ScenarioActors);
		TestEqual(TEXT("horloge reprise"), Sim.GetTime(), Read.Time);
		TestEqual(TEXT("jour repris"), Sim.GetDay(), Read.Day);
	}

	// 1. La trace, et son tick 0.
	AnastasisHarnessTrace::FRunResult Result;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("trace ecrite (%s)"), *Error), AnastasisHarnessTrace::Run(Scenario, Out, Ticks, 1, Result, Error, Drill)))
	{
		return false;
	}
	TestEqual(TEXT("un echantillon par tick, tick 0 compris"), Result.Samples, Ticks + 1);
	int32 Perimetre = 0;
	TArray<FString> Changed;
	for (const FScenarioSection& S : ScenarioSections)
	{
		if (!S.bPerimetre) continue;
		++Perimetre;
		const uint64* Zero = Result.TickZero.Find(S.Name);
		if (!Zero)
		{
			AddError(FString::Printf(TEXT("section %s absente de la trace"), S.Name));
			continue;
		}
		if (*Zero != S.Digest)
		{
			AddError(FString::Printf(TEXT("tick 0 apres reprise, section %s : attendu %016llx, obtenu %016llx"), S.Name, S.Digest, *Zero));
		}
		if (Result.Last.FindRef(S.Name) != S.Digest) Changed.Add(S.Name);
	}
	TestEqual(TEXT("toutes les sections du perimetre tracees"), Result.TickZero.Num(), Perimetre);

	// 2. Deux executions, memes bits.
	{
		const int32 Short = FMath::Min(Ticks, 300);
		AnastasisHarnessTrace::FRunResult A;
		AnastasisHarnessTrace::FRunResult B;
		const FString TmpA = FPaths::Combine(FPaths::GetPath(Out), TEXT("_determinisme_a.jsonl"));
		const FString TmpB = FPaths::Combine(FPaths::GetPath(Out), TEXT("_determinisme_b.jsonl"));
		TestTrue(TEXT("execution A"), AnastasisHarnessTrace::Run(Scenario, TmpA, Short, 1, A, Error));
		TestTrue(TEXT("execution B"), AnastasisHarnessTrace::Run(Scenario, TmpB, Short, 1, B, Error));
		TestEqual(FString::Printf(TEXT("deux executions de %d ticks : meme empreinte"), Short), A.LastGlobal, B.LastGlobal);
	}

	AddInfo(FString::Printf(TEXT("trace : %s — %d echantillons ; sections changees entre le tick 0 et le tick %d : %s"),
		*Out, Result.Samples, Ticks, Changed.Num() ? *FString::Join(Changed, TEXT(", ")) : TEXT("aucune")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
