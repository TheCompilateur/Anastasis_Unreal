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
	auto Add = [&Candidates](double X, double Y, uint8 Species, double Score, double GroundZ = 0.0, double HeightCm = -1.0)
	{
		FCandidate C;
		C.Index = Candidates.Num();
		C.Ground = FVector2D(X, Y);
		C.Species = Species;
		C.Score = Score;
		C.GroundZ = GroundZ;
		C.HeightCm = HeightCm < 0.0 ? Score * 100.0 : HeightCm;
		C.Dryness = 0.6;
		Candidates.Add(C);
	};
	Add(0, 0, 4, 7.0);
	Add(1000, 0, 4, 12.0);
	Add(5000, 0, 1, 16.0);
	Add(9000, 4000, 5, 30.0);
	Add(200, 200, 2, 4.0);
	// A stand of eight on ground at 5 m, 8 to 11 m tall, and one trunk at its cell's far corner.
	for (int32 I = 0; I < 8; ++I) Add(6400.0 + (I % 3) * 400.0, 6400.0 + (I / 3) * 400.0, 3, 9.0, 500.0, 800.0 + I * 40.0);
	Add(8900.0, 8900.0, 3, 9.0, 500.0, 1100.0);
	// A thinner stand of five, and a stand whose trees have no rendered height.
	for (int32 I = 0; I < 5; ++I) Add(30500.0 + I * 500.0, 600.0, 3, 9.0, -200.0, 1000.0);
	for (int32 I = 0; I < 6; ++I) Add(60500.0 + I * 300.0, 600.0, 3, 9.0, 0.0, 0.0);
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
	TestEqual(TEXT("two stands grow a shell"), Shells.Num(), 2);
	TestEqual(TEXT("three dense cells"), Report.Stands, 3);
	if (Shells.Num() == 2)
	{
		const FShell& Dense = Shells[0];
		TestEqual(TEXT("densest stand first"), Dense.Trees, 9);
		TestTrue(TEXT("the stand's own ground"), FMath::IsNearlyEqual(Dense.GroundZ, 500.0, 1.0));
		TestTrue(TEXT("no taller than the median tree"), Dense.TopCm <= 960.0 + 1.0);
		TestTrue(TEXT("crowns, not the trunks"), Dense.BaseCm > 0.0 && Dense.BaseCm < Dense.TopCm);
		TestTrue(TEXT("tinted like its trees"), FMath::IsNearlyEqual(Dense.Dryness, 0.6, 1e-6));
		// The far trunk is 2500 cm from the cluster: a shell reaching it would cover open ground.
		TestTrue(TEXT("inside the stand"), Dense.RadiusCm < 1500.0);
		TestTrue(TEXT("radius bounds"), Dense.RadiusCm >= 600.0 && Dense.RadiusCm <= 1800.0);
		TestEqual(TEXT("thinner stand second"), Shells[1].Trees, 5);
		TestTrue(TEXT("thinner stand ground"), FMath::IsNearlyEqual(Shells[1].GroundZ, -200.0, 1.0));
		TestTrue(TEXT("thinner stand top"), Shells[1].TopCm <= 1000.0 && Shells[1].TopCm > Shells[1].BaseCm);
	}
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
