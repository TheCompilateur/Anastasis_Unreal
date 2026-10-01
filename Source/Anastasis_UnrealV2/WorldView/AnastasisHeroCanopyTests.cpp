#include "WorldView/AnastasisHeroCanopy.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisHeroCanopyPlan, "Anastasis.HeroCanopy.Select",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisHeroCanopyPlan::RunTest(const FString&)
{
	using namespace AnastasisHeroCanopy;
	TArray<FCandidate> Candidates;
	auto Add = [&Candidates](double X, double Y, uint8 Species, double Score)
	{
		FCandidate C;
		C.Index = Candidates.Num();
		C.Ground = FVector2D(X, Y);
		C.Species = Species;
		C.Score = Score;
		Candidates.Add(C);
	};
	Add(0, 0, 4, 7.0);
	Add(1000, 0, 4, 12.0);
	Add(5000, 0, 1, 16.0);
	Add(9000, 4000, 5, 30.0);
	Add(200, 200, 2, 4.0);
	for (int32 I = 0; I < 8; ++I) Add(8000.0 + (I % 3) * 400.0, 8000.0 + (I / 3) * 400.0, 3, 9.0);
	TArray<FHero> Heroes;
	TArray<FShell> Shells;
	FReport Report;
	FString Error;
	TestTrue(TEXT("plan"), Build(Candidates, Heroes, Shells, Report, Error));
	TArray<FHero> Again;
	TArray<FShell> ShellsAgain;
	FReport ReportAgain;
	TestTrue(TEXT("repeat"), Build(Candidates, Again, ShellsAgain, ReportAgain, Error));
	TestEqual(TEXT("same heroes"), Heroes.Num(), Again.Num());
	TestEqual(TEXT("same shells"), Shells.Num(), ShellsAgain.Num());
	TestTrue(TEXT("a stand grows a shell"), Shells.Num() >= 1);
	TestTrue(TEXT("cap"), Heroes.Num() <= 8);
	for (int32 I = 0; I < Heroes.Num(); ++I)
	{
		TestEqual(TEXT("same choice"), Heroes[I].Index, Again[I].Index);
		TestTrue(TEXT("hero species only"), Heroes[I].Species >= 1 && Heroes[I].Species <= 4);
		TestTrue(TEXT("tall enough"), Candidates[Heroes[I].Index].Score >= 6.0);
		for (int32 J = I + 1; J < Heroes.Num(); ++J)
		{
			TestTrue(TEXT("specimens stay apart"),
				FVector2D::Distance(Candidates[Heroes[I].Index].Ground, Candidates[Heroes[J].Index].Ground) >= 2200.0 - 1.0);
		}
	}
	bool bPlane = false;
	for (const FHero& Hero : Heroes) bPlane |= Hero.Index == 3;
	TestFalse(TEXT("a plane tree is not a hero"), bPlane);
	bool bSapling = false;
	for (const FHero& Hero : Heroes) bSapling |= Hero.Index == 4;
	TestFalse(TEXT("a short tree is not a hero"), bSapling);
	TArray<FCandidate> Empty;
	TestTrue(TEXT("empty"), Build(Empty, Heroes, Shells, Report, Error));
	TestEqual(TEXT("empty heroes"), Heroes.Num(), 0);
	TestEqual(TEXT("empty shells"), Shells.Num(), 0);
	Candidates[0].Ground.X = std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("rejects a broken trunk"), Build(Candidates, Heroes, Shells, Report, Error));
	TestEqual(TEXT("no partial plan"), Heroes.Num(), 0);
	return true;
}
#endif
