#include "WorldView/AnastasisMicroEcology.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisMicroEcologyTest
{
AnastasisMicroEcology::FInputs Channel(double SlopeDegrees = 0.0)
{
	AnastasisMicroEcology::FInputs In;
	const double Tan = FMath::Tan(FMath::DegreesToRadians(SlopeDegrees));
	In.SampleHeight = [Tan](double X, double Y, double& Z)
	{
		// Montee courte hors de l'eau, puis terrasse plane : la prairie n'est pas un talus.
		Z = FMath::Clamp(X, 0.0, 280.0) * 0.35 + Y * Tan;
		return true;
	};
	In.SampleWaterHeight = [](double, double, double& W) { W = 0.0; return true; };
	In.Mask = [](double, double) { return 1.0; };
	In.Bounds = FBox2D(FVector2D(-200.0, 0.0), FVector2D(8000.0, 24000.0));
	In.Seed = 12345u;
	return In;
}

int32 Count(const AnastasisMicroEcology::FPlan& P, AnastasisMicroEcology::ERole Role)
{
	return P.Counts[static_cast<int32>(Role)];
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMicroEcologyDeterminism, "Anastasis.MicroEcology.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMicroEcologyDeterminism::RunTest(const FString&)
{
	using namespace AnastasisMicroEcology;
	const FInputs In = AnastasisMicroEcologyTest::Channel();
	FPlan A, B;
	FString Error;
	TestTrue(TEXT("first"), Build(In, FSettings(), A, Error));
	TestTrue(TEXT("second"), Build(In, FSettings(), B, Error));
	TestTrue(TEXT("placed"), A.Instances.Num() > 0);
	TestEqual(TEXT("same count"), A.Instances.Num(), B.Instances.Num());
	bool bSame = A.Instances.Num() == B.Instances.Num();
	for (int32 I = 0; bSame && I < A.Instances.Num(); ++I)
	{
		bSame = A.Instances[I].Ground.Equals(B.Instances[I].Ground, 0.0)
			&& A.Instances[I].Role == B.Instances[I].Role
			&& A.Instances[I].Pocket == B.Instances[I].Pocket
			&& A.Instances[I].Scale == B.Instances[I].Scale;
	}
	TestTrue(TEXT("identical plans"), bSame);
	FInputs Other = In;
	Other.Seed = 99u;
	FPlan C;
	TestTrue(TEXT("other seed"), Build(Other, FSettings(), C, Error));
	TestTrue(TEXT("seed changes the plan"), C.Instances.Num() != A.Instances.Num()
		|| (C.Instances.Num() > 0 && !C.Instances[0].Ground.Equals(A.Instances[0].Ground, 1.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMicroEcologyBankPockets, "Anastasis.MicroEcology.BankPockets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMicroEcologyBankPockets::RunTest(const FString&)
{
	using namespace AnastasisMicroEcology;
	const FSettings Settings;
	int32 Hist[PocketCount] = {};
	int32 Disagree = 0, Pairs = 0;
	for (double Y = 200.0; Y < 23800.0; Y += 280.0)
	{
		const EPocket Inner = BankPocket(12345u, 80.0, Y, Settings);
		const EPocket Outer = BankPocket(12345u, 220.0, Y, Settings);
		++Hist[static_cast<int32>(Inner)];
		++Pairs;
		if (Inner != Outer) ++Disagree;
	}
	int32 Kinds = 0, Peak = 0, Total = 0;
	for (int32 I = static_cast<int32>(EPocket::Clean); I <= static_cast<int32>(EPocket::Drift); ++I)
	{
		if (Hist[I] > 0) ++Kinds;
		Peak = FMath::Max(Peak, Hist[I]);
		Total += Hist[I];
	}
	AddInfo(FString::Printf(TEXT("shore states clean=%d rocky=%d muddy=%d vegetated=%d drift=%d disagree=%.2f"),
		Hist[1], Hist[2], Hist[3], Hist[4], Hist[5], Pairs > 0 ? static_cast<double>(Disagree) / Pairs : 0.0));
	TestTrue(TEXT("several bank states along one shore"), Kinds >= 4);
	TestTrue(TEXT("no state owns the whole shore"), Total > 0 && Peak * 100 < Total * 72);
	TestTrue(TEXT("inner and outer bank disagree"), Disagree * 100 > Pairs * 8);

	FPlan Plan;
	FString Error;
	TestTrue(TEXT("channel builds"), Build(AnastasisMicroEcologyTest::Channel(), Settings, Plan, Error));
	const int32 Pebbles = AnastasisMicroEcologyTest::Count(Plan, ERole::BankPebble);
	const int32 Reeds = AnastasisMicroEcologyTest::Count(Plan, ERole::BankReed);
	const int32 Tufts = AnastasisMicroEcologyTest::Count(Plan, ERole::BankTuft);
	const int32 Drift = AnastasisMicroEcologyTest::Count(Plan, ERole::BankDrift);
	const int32 Branches = AnastasisMicroEcologyTest::Count(Plan, ERole::BankBranch);
	AddInfo(FString::Printf(TEXT("bank props pebble=%d reed=%d tuft=%d drift=%d branch=%d"), Pebbles, Reeds, Tufts, Drift, Branches));
	TestTrue(TEXT("pebbles and reeds both occur"), Pebbles > 0 && Reeds > 0);
	TestTrue(TEXT("mud or drift is present"), Tufts + Drift + Branches > 0);
	bool bHonest = true;
	for (const FPlacement& P : Plan.Instances)
	{
		const bool bBankRole = static_cast<uint8>(P.Role) <= static_cast<uint8>(ERole::BankBranch);
		if (!bBankRole) continue;
		bHonest &= P.Ground.X > 0.0 && P.Ground.X < 500.0;
		bHonest &= P.AboveWater >= Settings.BankMinAboveUU && P.AboveWater <= Settings.BankMaxAboveUU;
		bHonest &= P.Pocket != EPocket::Clean;
		if (P.Role == ERole::BankReed) bHonest &= P.Pocket == EPocket::Vegetated;
		if (P.Role == ERole::BankPebble) bHonest &= P.Pocket == EPocket::Rocky;
		bHonest &= P.Scale > 0.2 && P.Scale < 1.6;
	}
	TestTrue(TEXT("bank props stay in the wet band and match their pocket"), bHonest);
	const int32 ShoreCells = Plan.PocketCells[1] + Plan.PocketCells[2] + Plan.PocketCells[3] + Plan.PocketCells[4] + Plan.PocketCells[5];
	TestTrue(TEXT("soil field records more than one shore state"),
		ShoreCells > 0 && (Plan.PocketCells[1] > 0) + (Plan.PocketCells[2] > 0) + (Plan.PocketCells[3] > 0) + (Plan.PocketCells[4] > 0) >= 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMicroEcologyMeadowClusters, "Anastasis.MicroEcology.MeadowClusters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMicroEcologyMeadowClusters::RunTest(const FString&)
{
	using namespace AnastasisMicroEcology;
	FPlan Plan;
	FString Error;
	TestTrue(TEXT("meadow builds"), Build(AnastasisMicroEcologyTest::Channel(), FSettings(), Plan, Error));
	int32 Stones = 0, Bushes = 0, Neighbours = 0;
	TArray<FVector2D> Points;
	for (const FPlacement& P : Plan.Instances)
	{
		if (P.Role != ERole::MeadowStone && P.Role != ERole::MeadowBush) continue;
		if (P.Role == ERole::MeadowStone) ++Stones;
		else ++Bushes;
		TestTrue(TEXT("meadow prop is inland"), P.AboveWater > FSettings().BankMaxAboveUU);
		Points.Add(FVector2D(P.Ground));
	}
	AddInfo(FString::Printf(TEXT("meadow stone=%d bush=%d bare_cells=%d dry_cells=%d wet_cells=%d"),
		Stones, Bushes, Plan.PocketCells[6], Plan.PocketCells[7], Plan.PocketCells[9]));
	TestTrue(TEXT("stones are occasional"), Stones >= 4);
	TestTrue(TEXT("bushes are rarer than stones"), Bushes > 0 && Bushes < Stones);
	for (int32 I = 0; I < Points.Num(); ++I)
	{
		for (int32 J = 0; J < Points.Num(); ++J)
		{
			if (I != J && FVector2D::Distance(Points[I], Points[J]) < 2200.0)
			{
				++Neighbours;
				break;
			}
		}
	}
	TestTrue(TEXT("meadow props cluster"), Points.Num() > 0 && Neighbours * 100 >= Points.Num() * 45);
	TestTrue(TEXT("bare and wet soil both exist"), Plan.PocketCells[6] > 0 && Plan.PocketCells[9] > 0);
	const int32 Open = Plan.PocketCells[0];
	TestTrue(TEXT("most of the field stays untouched"), Open > Plan.PocketCells[6] + Plan.PocketCells[7] + Plan.PocketCells[8] + Plan.PocketCells[9]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMicroEcologyForestEdge, "Anastasis.MicroEcology.ForestEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMicroEcologyForestEdge::RunTest(const FString&)
{
	using namespace AnastasisMicroEcology;
	FInputs In;
	In.SampleHeight = [](double, double, double& Z) { Z = 400.0; return true; };
	In.SampleWaterHeight = [](double, double, double& W) { W = -4000.0; return true; };
	In.Mask = [](double, double) { return 1.0; };
	In.Bounds = FBox2D(FVector2D(0.0, 0.0), FVector2D(8000.0, 8000.0));
	In.Seed = 12345u;
	const FVector Crown(4000.0, 4000.0, 900.0);
	In.Canopy.Add(Crown);
	FPlan Plan;
	FString Error;
	TestTrue(TEXT("edge builds"), Build(In, FSettings(), Plan, Error));
	int32 Bushes = 0, Saplings = 0, Sectors[8] = {};
	double BushR = 0.0, SaplingR = 0.0;
	int32 Under = 0, UnderSectors[8] = {};
	for (const FPlacement& P : Plan.Instances)
	{
		const double Dist = FVector2D::Distance(FVector2D(P.Ground), FVector2D(Crown)) / Crown.Z;
		const int32 Sector = FMath::Clamp(FMath::FloorToInt((FMath::Atan2(P.Ground.Y - Crown.Y, P.Ground.X - Crown.X) + PI) / (2.0 * PI) * 8.0), 0, 7);
		if (P.Role == ERole::EdgeBush || P.Role == ERole::EdgeSapling)
		{
			const bool bOk = Dist >= 1.0 && Dist <= 2.5 && P.Scale > 0.4 && P.Scale < 1.6;
			if (!bOk) AddError(FString::Printf(TEXT("edge prop out of fringe d=%.2f scale=%.2f"), Dist, P.Scale));
			++Sectors[Sector];
			if (P.Role == ERole::EdgeBush) { ++Bushes; BushR += Dist; }
			else { ++Saplings; SaplingR += Dist; }
		}
		else if (static_cast<uint8>(P.Role) >= static_cast<uint8>(ERole::UnderLog))
		{
			if (Dist < 0.2 || Dist > 0.95) AddError(FString::Printf(TEXT("understory out of crown d=%.2f"), Dist));
			++Under;
			++UnderSectors[Sector];
		}
	}
	int32 Empty = 0, Occupied = 0, UnderEmpty = 0;
	for (int32 S : Sectors) { if (S == 0) ++Empty; else ++Occupied; }
	for (int32 S : UnderSectors) if (S == 0) ++UnderEmpty;
	AddInfo(FString::Printf(TEXT("edge bush=%d sapling=%d empty_sectors=%d occupied=%d under=%d under_empty=%d"),
		Bushes, Saplings, Empty, Occupied, Under, UnderEmpty));
	TestTrue(TEXT("edge has bushes and saplings"), Bushes > 0 && Saplings > 0);
	TestTrue(TEXT("saplings sit closer than bushes"), Saplings > 0 && Bushes > 0 && SaplingR / Saplings < BushR / Bushes);
	TestTrue(TEXT("the edge is not a full ring"), Empty >= 1 && Occupied >= 3);
	TestTrue(TEXT("understory exists and leaves gaps"), Under > 0 && UnderEmpty >= 1);
	TestTrue(TEXT("dead wood is part of the interior"),
		AnastasisMicroEcologyTest::Count(Plan, ERole::UnderLog) + AnastasisMicroEcologyTest::Count(Plan, ERole::UnderStump)
		+ AnastasisMicroEcologyTest::Count(Plan, ERole::UnderBranch) + AnastasisMicroEcologyTest::Count(Plan, ERole::UnderRoots) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMicroEcologySlopeAndClearing, "Anastasis.MicroEcology.SlopeAndClearing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMicroEcologySlopeAndClearing::RunTest(const FString&)
{
	using namespace AnastasisMicroEcology;
	FPlan Steep;
	FString Error;
	TestTrue(TEXT("steep builds"), Build(AnastasisMicroEcologyTest::Channel(40.0), FSettings(), Steep, Error));
	TestEqual(TEXT("no reeds on a 40 degree bank"), AnastasisMicroEcologyTest::Count(Steep, ERole::BankReed), 0);
	TestEqual(TEXT("no meadow bushes on a 40 degree slope"), AnastasisMicroEcologyTest::Count(Steep, ERole::MeadowBush), 0);
	TestEqual(TEXT("no meadow stones on a 40 degree slope"), AnastasisMicroEcologyTest::Count(Steep, ERole::MeadowStone), 0);
	TestTrue(TEXT("rock can still catch the steep bank"), AnastasisMicroEcologyTest::Count(Steep, ERole::BankPebble) > 0);

	FInputs Cleared = AnastasisMicroEcologyTest::Channel();
	Cleared.Clearings.Add({FVector2D(120.0, 4000.0), 1800.0});
	FPlan Open;
	TestTrue(TEXT("clearing builds"), Build(Cleared, FSettings(), Open, Error));
	bool bKept = true;
	for (const FPlacement& P : Open.Instances)
	{
		bKept &= FVector2D::Distance(FVector2D(P.Ground), FVector2D(120.0, 4000.0)) > 1800.0;
	}
	TestTrue(TEXT("a clearing stays empty"), bKept && Open.RejectedClearing > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMicroEcologySoilTint, "Anastasis.MicroEcology.SoilTint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMicroEcologySoilTint::RunTest(const FString&)
{
	using namespace AnastasisMicroEcology;
	const FLinearColor Grass(0.070f, 0.110f, 0.040f, 1.0f);
	TestTrue(TEXT("untouched pocket keeps the colour"), TintSoil(Grass, EPocket::None, 1.0).Equals(Grass));
	TestTrue(TEXT("plants do not repaint the soil"), TintSoil(Grass, EPocket::Vegetated, 1.0).Equals(Grass));
	const FLinearColor Mud = TintSoil(Grass, EPocket::Muddy, 1.0);
	const FLinearColor Clean = TintSoil(Grass, EPocket::Clean, 1.0);
	const FLinearColor Rock = TintSoil(Grass, EPocket::Rocky, 1.0);
	TestTrue(TEXT("mud is darker than the grass"), Mud.GetLuminance() < Grass.GetLuminance());
	TestTrue(TEXT("a clean pocket is not the mud pocket"), !Clean.Equals(Mud));
	TestTrue(TEXT("rock loses the grass green"), Rock.G < Grass.G && Rock.R > Grass.R);
	TestTrue(TEXT("alpha survives"), Mud.A == Grass.A);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMicroEcologyRejects, "Anastasis.MicroEcology.Rejects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMicroEcologyRejects::RunTest(const FString&)
{
	using namespace AnastasisMicroEcology;
	FPlan Plan;
	FString Error;
	FInputs Missing = AnastasisMicroEcologyTest::Channel();
	Missing.SampleHeight = nullptr;
	TestFalse(TEXT("no ground"), Build(Missing, FSettings(), Plan, Error));
	TestTrue(TEXT("error mentions ground"), Error.Contains(TEXT("ground")));
	FSettings Bad;
	Bad.BankMaxAboveUU = 1.0;
	Bad.BankMinAboveUU = 10.0;
	TestFalse(TEXT("bad band"), Build(AnastasisMicroEcologyTest::Channel(), Bad, Plan, Error));
	FSettings Cap;
	Cap.MaxInstances = 6;
	FPlan Full;
	TestTrue(TEXT("uncapped"), Build(AnastasisMicroEcologyTest::Channel(), FSettings(), Full, Error));
	TestTrue(TEXT("capped"), Build(AnastasisMicroEcologyTest::Channel(), Cap, Plan, Error));
	TestTrue(TEXT("cap truncates without emptying"), Plan.bTruncated && Plan.Instances.Num() > 0 && Plan.Instances.Num() < Full.Instances.Num());
	return true;
}
#endif
