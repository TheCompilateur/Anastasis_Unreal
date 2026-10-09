// annee-valmire-001 -- l'instrument de l'annee, sur six jours : il pose le vrai village, releve ce qu'il annonce,
// fait arriver le joueur la ou il le dit, et rend deux fois le meme releve pour la meme graine. L'annee entiere ne
// tourne pas dans la suite (quatre fois deux ans) : `Anastasis.Etude.Annee`, via tools/unreal/year-study.ps1.

#include "Misc/AutomationTest.h"

#include "Sim/AnastasisYearStudy.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisYearStudyInstrumentTest,
	"Anastasis.Village.Etude.Instrument",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisYearStudyInstrumentTest::RunTest(const FString&)
{
	using namespace AnastasisYearStudy;
	constexpr uint32 Seed = 12345u;
	const TArray<FScenario> Scenarios = DefaultScenarios();
	if (!TestEqual(TEXT("quatre scenarios"), Scenarios.Num(), 4)) return false;

	TArray<FRun> Runs;
	for (const FScenario& Scenario : Scenarios)
	{
		Runs.Add(Run(Scenario, 6, 3, Seed));
		const FRun& R = Runs.Last();
		if (!TestTrue(FString::Printf(TEXT("%s : village du lancement pose"), *Scenario.Name), R.bSeeded)) return false;
		TestEqual(FString::Printf(TEXT("%s : releves aux jours 0, 3, 6"), *Scenario.Name), R.Samples.Num(), 3);
		TestTrue(FString::Printf(TEXT("%s : des habitants"), *Scenario.Name), R.Samples[0].Population >= 12);
		TestEqual(FString::Printf(TEXT("%s : joueur la ou le scenario le dit"), *Scenario.Name),
			!R.PlayerId.IsEmpty(), Scenario.Player != EPlayer::None);
		TestTrue(FString::Printf(TEXT("%s : une chronique"), *Scenario.Name), !R.Chronicle.IsEmpty());
		const FSample& Last = R.Samples.Last();
		AddInfo(FString::Printf(TEXT("YEAR_STUDY_TEST %s pop=%d houses=%d granary=%d field=%d meals=%d friends=%d memories=%d player=%d"),
			*Scenario.Name, Last.Population, Last.Houses, Last.GranaryFood, Last.FieldFood, Last.Meals, Last.Friendships, Last.Memories,
			Last.bPlayerAlive ? 1 : 0));
	}
	TestTrue(TEXT("le village mange en six jours"), Runs[0].Samples.Last().Meals > 0);

	// Meme graine, meme scenario : meme releve, ligne pour ligne.
	const FRun Again = Run(Scenarios[0], 6, 3, Seed);
	TestEqual(TEXT("meme graine : meme releve"), ToCsv(Again), ToCsv(Runs[0]));

	const FString Csv = ToCsv(Runs[2]);
	TestTrue(TEXT("csv : en-tete et trois lignes"), Csv.StartsWith(TEXT("jour;saison;habitants")) && Csv.Contains(TEXT("\n6;")));
	TestTrue(TEXT("resume : quatre scenarios"), SummaryJson(Runs, Seed).Contains(TEXT("sans-joueur-village-ferme")));
	return true;
}

#endif
