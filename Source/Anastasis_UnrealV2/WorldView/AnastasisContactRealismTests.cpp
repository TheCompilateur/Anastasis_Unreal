#include "WorldView/AnastasisContactRealism.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisContactRealismTestSupport
{
using namespace AnastasisContactRealism;

/**
 * Plan incline : le sol monte de 4 % vers +X, le lac occupe X < 5000 (nappe a 200 uu), la terre
 * seche commence apres. Les points secs loin de l'eau portent la sentinelle sol - 100 comme le drainage.
 */
inline FInputs MakeShoreInputs(double Calm)
{
	FInputs In;
	In.SampleHeight = [](double X, double Y, double& Z) { Z = X * 0.04; return X >= 0.0 && X <= 20000.0 && Y >= 0.0 && Y <= 12000.0; };
	In.SampleWaterHeight = [](double X, double Y, double& Z) { const double G = X * 0.04; Z = G <= 200.0 ? 200.0 : G - 100.0; return true; };
	In.SampleCalm = [Calm](double, double) { return Calm; };
	In.Bounds = FBox2D(FVector2D(0.0, 0.0), FVector2D(20000.0, 12000.0));
	In.Seed = 12345;
	return In;
}

inline FSettings TestSettings()
{
	FSettings S;
	S.MaxDecals = 4000;
	return S;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisContactShoreStates, "Anastasis.ContactRealism.ShoreStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisContactShoreStates::RunTest(const FString&)
{
	using namespace AnastasisContactRealism;
	TestEqual(TEXT("flat calm bank low on the water is mud"), ClassifyShore(100, 40, 0.05, 0.9, 0.5, 0, 300), EShore::Muddy);
	TestEqual(TEXT("real reeds make a reed bed"), ClassifyShore(100, 40, 0.05, 0.9, 0.5, 5, 300), EShore::Reed);
	TestEqual(TEXT("a steep bank is stone"), ClassifyShore(100, 40, 0.40, 0.9, 0.5, 0, 300), EShore::Stony);
	TestEqual(TEXT("fast water is stone"), ClassifyShore(100, 40, 0.05, 0.2, 0.5, 0, 300), EShore::Stony);
	TestEqual(TEXT("a high bank is only damp"), ClassifyShore(100, 200, 0.05, 0.9, 0.5, 0, 300), EShore::Damp);
	TestEqual(TEXT("hard bank stretches stay dry"), ClassifyShore(100, 40, 0.05, 0.9, 0.1, 0, 300), EShore::Dry);
	TestEqual(TEXT("beyond the band is the flood bed"), ClassifyShore(500, 40, 0.05, 0.9, 0.5, 0, 300), EShore::Dry);
	TestEqual(TEXT("far from water is nothing"), ClassifyShore(5000, 40, 0.05, 0.9, 0.5, 0, 300), EShore::None);
	TestEqual(TEXT("reeds in fast water do not grow"), ClassifyShore(100, 40, 0.05, 0.1, 0.5, 5, 300), EShore::Stony);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisContactShorePlan, "Anastasis.ContactRealism.ShorePlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisContactShorePlan::RunTest(const FString&)
{
	using namespace AnastasisContactRealism;
	using namespace AnastasisContactRealismTestSupport;
	const FSettings S = TestSettings();
	FPlan A, B;
	FString Error;
	TestTrue(TEXT("plan"), Build(MakeShoreInputs(0.9), S, A, Error));
	TestTrue(TEXT("repeat"), Build(MakeShoreInputs(0.9), S, B, Error));
	TestEqual(TEXT("same decal count"), A.Decals.Num(), B.Decals.Num());
	TestTrue(TEXT("the shore gets decals"), A.Decals.Num() > 20);
	TestTrue(TEXT("lake cells were found"), A.WetCells > 100);
	for (int32 I = 0; I < FMath::Min(A.Decals.Num(), B.Decals.Num()); ++I)
	{
		TestTrue(TEXT("same decal"), A.Decals[I].Location.Equals(B.Decals[I].Location, 1.0e-4) && A.Decals[I].Kind == B.Decals[I].Kind);
	}
	int32 Examined = 0, Decaled = 0;
	for (int32 I = 1; I < ShoreCount; ++I) { Examined += A.ShoreExamined[I]; Decaled += A.ShoreDecaled[I]; }
	TestTrue(TEXT("candidates were examined"), Examined > 50);
	// Le vide est requis : une rive n'est pas un anneau.
	TestTrue(TEXT("empty space is left on the bank"), Decaled < Examined);
	for (const FDecal& D : A.Decals)
	{
		if (D.Shore == EShore::None) continue;
		TestTrue(TEXT("shore decals sit in the band, never in the meadow"), D.Location.X - 5000.0 <= S.ShoreBandUU + 1.0 && D.Location.X >= 4000.0);
		TestTrue(TEXT("sizes stay bounded"), D.HalfLong <= 1500.0 && D.HalfWide <= 1500.0 && D.HalfDepth >= 70.0 && D.HalfDepth <= 260.0);
		TestTrue(TEXT("no decal on the lake bed's far side"), D.Location.X < 12000.0);
	}
	TestEqual(TEXT("a calm lake has no stone state"), A.ShoreDecaled[static_cast<int32>(EShore::Stony)], 0);

	FPlan Fast;
	TestTrue(TEXT("fast plan"), Build(MakeShoreInputs(0.1), S, Fast, Error));
	TestTrue(TEXT("fast water makes stone bands"), Fast.ShoreDecaled[static_cast<int32>(EShore::Stony)] > 0);
	TestEqual(TEXT("fast water has no mud"), Fast.ShoreDecaled[static_cast<int32>(EShore::Muddy)], 0);
	TestTrue(TEXT("stone shores get pebbles"), Fast.Pebbles.Num() > 0);
	for (const FPebble& P : Fast.Pebbles)
	{
		TestTrue(TEXT("a pebble is partly buried"), P.Sink >= 0.3 && P.Sink <= 0.7);
		TestTrue(TEXT("a pebble is small"), P.Diameter >= S.PebbleMinUU * 0.99 && P.Diameter <= S.PebbleMaxUU * 1.01 * 1.3);
	}

	FPlan Capped;
	FSettings Small = S;
	Small.MaxDecals = 12;
	TestTrue(TEXT("capped plan"), Build(MakeShoreInputs(0.9), Small, Capped, Error));
	TestTrue(TEXT("the decal cap holds"), Capped.Decals.Num() <= 12);
	TestTrue(TEXT("the cap is reported"), Capped.bTruncated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisContactAnchors, "Anastasis.ContactRealism.Anchors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisContactAnchors::RunTest(const FString&)
{
	using namespace AnastasisContactRealism;
	using namespace AnastasisContactRealismTestSupport;
	const FSettings S = TestSettings();
	FString Error;
	auto Anchor = [](EAnchor Kind, double X, double Y, double Radius, double Height)
	{
		FAnchor A;
		A.Kind = Kind;
		A.Location = FVector(X, Y, X * 0.04);
		A.Radius = Radius;
		A.Height = Height;
		return A;
	};
	{
		FInputs In = MakeShoreInputs(0.9);
		for (int32 I = 0; I < 40; ++I) In.Anchors.Add(Anchor(EAnchor::Tree, 12000.0 + 150.0 * I, 3000.0 + 37.0 * I, 300.0, 1600.0));
		for (int32 I = 0; I < 12; ++I) In.Anchors.Add(Anchor(EAnchor::Sapling, 15000.0 + 200.0 * I, 9000.0, 50.0, 120.0));
		FPlan Plan;
		TestTrue(TEXT("tree plan"), Build(In, S, Plan, Error));
		TestTrue(TEXT("trees get a dark root zone"), Plan.DecalCounts[static_cast<int32>(EDecal::ContactDark)] >= 20);
		TestTrue(TEXT("trees get litter"), Plan.DecalCounts[static_cast<int32>(EDecal::Litter)] >= 10);
		// Pas d'anneau : tous les arbres ne sont pas choisis, et le raccord est decale / irregulier.
		TestTrue(TEXT("not every tree is chosen"), Plan.DecalCounts[static_cast<int32>(EDecal::ContactDark)] < 40 + 12);
		TestEqual(TEXT("trees count as anchors"), Plan.AnchorCounts[static_cast<int32>(EAnchor::Tree)], 40);
		for (const FDecal& D : Plan.Decals)
		{
			if (D.Kind != EDecal::ContactDark && D.Kind != EDecal::Litter) continue;
			TestTrue(TEXT("a root zone is metres wide, not a crown"), D.HalfLong <= 700.0 && D.HalfWide <= 700.0);
		}
	}
	{
		FInputs In = MakeShoreInputs(0.9);
		In.Anchors.Add(Anchor(EAnchor::Rock, 14000.0, 6000.0, 90.0, 120.0));
		In.Anchors.Add(Anchor(EAnchor::Rock, 16000.0, 6000.0, 8.0, 10.0));
		FPlan Plan;
		TestTrue(TEXT("rock plan"), Build(In, S, Plan, Error));
		TestEqual(TEXT("a boulder gets a sediment collar, a pebble does not"), Plan.DecalCounts[static_cast<int32>(EDecal::RockDirt)], 1);
		TestTrue(TEXT("a boulder gets stones around its base"), Plan.Pebbles.Num() >= 2);
		for (const FPebble& P : Plan.Pebbles)
		{
			const double Ring = FVector2D::Distance(FVector2D(P.Location.X, P.Location.Y), FVector2D(14000.0, 6000.0));
			TestTrue(TEXT("stones sit just outside the rock"), Ring >= 90.0 * 0.9 && Ring <= 90.0 * 2.0);
		}
	}
	{
		FInputs In = MakeShoreInputs(0.9);
		// Roseaux reels au bord du lac : la rive voisine est une roseliere, et un lit se pose dessous.
		for (int32 I = 0; I < 99; ++I) In.Anchors.Add(Anchor(EAnchor::Reed, 5200.0 + 100.0 * (I % 9), 3500.0 + 100.0 * (I / 9), 0.0, 150.0));
		FPlan Plan;
		TestTrue(TEXT("reed plan"), Build(In, S, Plan, Error));
		TestTrue(TEXT("reeds get a wet bed"), Plan.DecalCounts[static_cast<int32>(EDecal::ReedBed)] >= 1);
		TestTrue(TEXT("the bank next to real reeds reads as reeds"), Plan.ShoreExamined[static_cast<int32>(EShore::Reed)] > 0);
	}
	{
		FInputs In = MakeShoreInputs(0.9);
		FPlan Plan;
		FInputs Big = In;
		for (int32 I = 0; I < 5000; ++I) Big.Anchors.Add(Anchor(EAnchor::Tree, 6000.0 + 2.5 * I, 1000.0 + 2.0 * (I % 400), 300.0, 1600.0));
		FSettings Cap = S;
		Cap.MaxDecals = 200;
		TestTrue(TEXT("dense plan"), Build(Big, Cap, Plan, Error));
		TestTrue(TEXT("global decal cap"), Plan.Decals.Num() <= 200);
		TestTrue(TEXT("truncated and said so"), Plan.bTruncated);
		FInputs Bad = MakeShoreInputs(0.9);
		Bad.Anchors.Add(Anchor(EAnchor::Tree, std::numeric_limits<double>::quiet_NaN(), 0.0, 300.0, 1600.0));
		TestFalse(TEXT("rejects a broken anchor"), Build(Bad, S, Plan, Error));
		TestEqual(TEXT("no partial plan"), Plan.Decals.Num(), 0);
		FInputs NoWater = MakeShoreInputs(0.9);
		NoWater.SampleWaterHeight = nullptr;
		TestFalse(TEXT("rejects missing samplers"), Build(NoWater, S, Plan, Error));
		FInputs Empty = MakeShoreInputs(0.9);
		Empty.SampleHeight = [](double, double, double&) { return false; };
		TestTrue(TEXT("no rendered ground is an empty plan"), Build(Empty, S, Plan, Error));
		TestEqual(TEXT("nothing is placed on no ground"), Plan.Decals.Num(), 0);
	}
	return true;
}
#endif
