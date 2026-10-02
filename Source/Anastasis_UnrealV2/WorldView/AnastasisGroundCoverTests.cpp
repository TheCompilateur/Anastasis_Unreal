#include "WorldView/AnastasisGroundCover.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
// Noms qualifies partout : en build unity, les using-directives de fichier d'AnastasisPlaces et
// d'AnastasisWorldView declarent aussi FPlan / FInputs (cf. 6c9242f).
namespace AnastasisGroundCoverTestFixture
{

/** 100 m x 100 m ouverts, sol plan incline de SlopeDegrees vers +X, nappe tres basse. */
AnastasisGroundCover::FInputs OpenPlane(double SlopeDegrees)
{
	AnastasisGroundCover::FInputs In;
	const double Tan = FMath::Tan(FMath::DegreesToRadians(SlopeDegrees));
	In.SampleHeight = [Tan](double X, double, double& Z) { Z = 1000.0 + X * Tan; return true; };
	In.SampleWaterHeight = [](double, double, double& W) { W = -5000.0; return true; };
	In.Mask = [](double, double) { return 1.0; };
	In.Bounds = FBox2D(FVector2D(0, 0), FVector2D(10000, 10000));
	In.Seed = 12345u;
	return In;
}

int32 Count(const AnastasisGroundCover::FPlan& P, AnastasisGroundCover::EFamily F) { return P.Counts[static_cast<int32>(F)]; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGroundCoverDeterminism, "Anastasis.GroundCover.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGroundCoverDeterminism::RunTest(const FString&)
{
	const AnastasisGroundCover::FInputs In = AnastasisGroundCoverTestFixture::OpenPlane(5.0);
	AnastasisGroundCover::FPlan A, B;
	FString Error;
	TestTrue(TEXT("first build"), AnastasisGroundCover::Build(In, AnastasisGroundCover::FSettings(), A, Error));
	TestTrue(TEXT("second build"), AnastasisGroundCover::Build(In, AnastasisGroundCover::FSettings(), B, Error));
	TestTrue(TEXT("instances placed"), A.Instances.Num() > 0);
	TestEqual(TEXT("same count"), A.Instances.Num(), B.Instances.Num());
	bool bSame = A.Instances.Num() == B.Instances.Num();
	for (int32 I = 0; bSame && I < A.Instances.Num(); ++I)
	{
		bSame = A.Instances[I].Ground.Equals(B.Instances[I].Ground, 0.0) && A.Instances[I].Family == B.Instances[I].Family
			&& A.Instances[I].Yaw == B.Instances[I].Yaw && A.Instances[I].Scale == B.Instances[I].Scale
			&& A.Instances[I].Thin == B.Instances[I].Thin && A.Instances[I].Thin >= 0.0 && A.Instances[I].Thin < 1.0;
	}
	// Build est parallele (une rangee de blocs par tache) : deux passes egales prouvent aussi que
	// la fusion ne depend pas de l'ordonnancement.
	TestTrue(TEXT("identical plans"), bSame);

	AnastasisGroundCover::FInputs Other = In;
	Other.Seed = 42u;
	AnastasisGroundCover::FPlan C;
	TestTrue(TEXT("other seed builds"), AnastasisGroundCover::Build(Other, AnastasisGroundCover::FSettings(), C, Error));
	TestTrue(TEXT("seed changes the plan"), C.Instances.Num() != A.Instances.Num()
		|| (C.Instances.Num() > 0 && !C.Instances[0].Ground.Equals(A.Instances[0].Ground, 1.0)));

	// Chaque instance repose sur le sol lu, dans l'emprise.
	bool bGrounded = true;
	for (const AnastasisGroundCover::FPlacement& P : A.Instances)
	{
		double Z;
		In.SampleHeight(P.Ground.X, P.Ground.Y, Z);
		bGrounded &= FMath::IsNearlyEqual(P.Ground.Z, Z, 1e-6) && In.Bounds.IsInside(FVector2D(P.Ground));
	}
	TestTrue(TEXT("instances rest on the sampled ground"), bGrounded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGroundCoverSlopeBands, "Anastasis.GroundCover.SlopeBands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGroundCoverSlopeBands::RunTest(const FString&)
{
	FString Error;
	AnastasisGroundCover::FPlan Flat, Mid, Steep;
	TestTrue(TEXT("flat"), AnastasisGroundCover::Build(AnastasisGroundCoverTestFixture::OpenPlane(4.0), AnastasisGroundCover::FSettings(), Flat, Error));
	TestTrue(TEXT("mid"), AnastasisGroundCover::Build(AnastasisGroundCoverTestFixture::OpenPlane(16.0), AnastasisGroundCover::FSettings(), Mid, Error));
	TestTrue(TEXT("steep"), AnastasisGroundCover::Build(AnastasisGroundCoverTestFixture::OpenPlane(26.0), AnastasisGroundCover::FSettings(), Steep, Error));
	AddInfo(FString::Printf(TEXT("flat tall=%d short=%d | 16deg tall=%d short=%d | 26deg all=%d"),
		AnastasisGroundCoverTestFixture::Count(Flat, AnastasisGroundCover::EFamily::MeadowTall), AnastasisGroundCoverTestFixture::Count(Flat, AnastasisGroundCover::EFamily::MeadowShort),
		AnastasisGroundCoverTestFixture::Count(Mid, AnastasisGroundCover::EFamily::MeadowTall), AnastasisGroundCoverTestFixture::Count(Mid, AnastasisGroundCover::EFamily::MeadowShort), Steep.Instances.Num()));
	// Plan V2 : 0-10 prairie haute, 10-20 prairie basse, 20-45 lande, au-dela falaise nue.
	TestTrue(TEXT("flat ground is mostly tall meadow"), AnastasisGroundCoverTestFixture::Count(Flat, AnastasisGroundCover::EFamily::MeadowTall) > 2 * AnastasisGroundCoverTestFixture::Count(Flat, AnastasisGroundCover::EFamily::MeadowShort));
	TestTrue(TEXT("flat ground still mixes heights"), AnastasisGroundCoverTestFixture::Count(Flat, AnastasisGroundCover::EFamily::MeadowShort) > 0);
	TestEqual(TEXT("16 deg holds no tall meadow"), AnastasisGroundCoverTestFixture::Count(Mid, AnastasisGroundCover::EFamily::MeadowTall), 0);
	TestTrue(TEXT("16 deg holds short meadow"), AnastasisGroundCoverTestFixture::Count(Mid, AnastasisGroundCover::EFamily::MeadowShort) > 0);
	TestTrue(TEXT("slope thins the cover"), Mid.Instances.Num() < Flat.Instances.Num());
	const int32 SteepLande = AnastasisGroundCoverTestFixture::Count(Steep, AnastasisGroundCover::EFamily::HeathTussock)
		+ AnastasisGroundCoverTestFixture::Count(Steep, AnastasisGroundCover::EFamily::Heather);
	TestTrue(TEXT("26 deg holds lande"), SteepLande > 0);
	TestEqual(TEXT("26 deg holds only lande"), SteepLande, Steep.Instances.Num());
	TestEqual(TEXT("16 deg holds no lande"), AnastasisGroundCoverTestFixture::Count(Mid, AnastasisGroundCover::EFamily::HeathTussock)
		+ AnastasisGroundCoverTestFixture::Count(Mid, AnastasisGroundCover::EFamily::Heather), 0);
	AnastasisGroundCover::FPlan Cliff;
	TestTrue(TEXT("cliff"), AnastasisGroundCover::Build(AnastasisGroundCoverTestFixture::OpenPlane(50.0), AnastasisGroundCover::FSettings(), Cliff, Error));
	TestEqual(TEXT("50 deg holds nothing"), Cliff.Instances.Num(), 0);
	TestTrue(TEXT("50 deg refused by slope"), Cliff.RejectedSlope > 0);
	TestEqual(TEXT("dry plane holds no sedge"), AnastasisGroundCoverTestFixture::Count(Flat, AnastasisGroundCover::EFamily::Sedge) + AnastasisGroundCoverTestFixture::Count(Mid, AnastasisGroundCover::EFamily::Sedge), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGroundCoverLande, "Anastasis.GroundCover.Lande",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGroundCoverLande::RunTest(const FString&)
{
	FString Error;
	const AnastasisGroundCover::EFamily Heath = AnastasisGroundCover::EFamily::HeathTussock;
	const AnastasisGroundCover::EFamily Heather = AnastasisGroundCover::EFamily::Heather;

	// La lande lit son propre habitat : une roche sans prairie possible porte une lande...
	AnastasisGroundCover::FInputs Rock = AnastasisGroundCoverTestFixture::OpenPlane(30.0);
	Rock.Mask = [](double, double) { return 0.0; };
	Rock.LandeMask = [](double, double) { return 1.0; };
	AnastasisGroundCover::FPlan OnRock;
	TestTrue(TEXT("rock builds"), AnastasisGroundCover::Build(Rock, AnastasisGroundCover::FSettings(), OnRock, Error));
	const int32 RockLande = AnastasisGroundCoverTestFixture::Count(OnRock, Heath) + AnastasisGroundCoverTestFixture::Count(OnRock, Heather);
	TestTrue(TEXT("rocky slope holds lande"), RockLande > 0 && RockLande == OnRock.Instances.Num());

	// ... et un versant hors habitat de lande n'en porte pas, meme ouvert a la prairie.
	AnastasisGroundCover::FInputs Closed = AnastasisGroundCoverTestFixture::OpenPlane(30.0);
	Closed.LandeMask = [](double, double) { return 0.0; };
	AnastasisGroundCover::FPlan None;
	TestTrue(TEXT("closed builds"), AnastasisGroundCover::Build(Closed, AnastasisGroundCover::FSettings(), None, Error));
	TestEqual(TEXT("no lande outside its habitat"), None.Instances.Num(), 0);

	// Plus maigre que la prairie, et de plus en plus en montant.
	AnastasisGroundCover::FPlan Meadow, Foot, High;
	TestTrue(TEXT("meadow"), AnastasisGroundCover::Build(AnastasisGroundCoverTestFixture::OpenPlane(4.0), AnastasisGroundCover::FSettings(), Meadow, Error));
	TestTrue(TEXT("foot"), AnastasisGroundCover::Build(AnastasisGroundCoverTestFixture::OpenPlane(24.0), AnastasisGroundCover::FSettings(), Foot, Error));
	TestTrue(TEXT("high"), AnastasisGroundCover::Build(AnastasisGroundCoverTestFixture::OpenPlane(40.0), AnastasisGroundCover::FSettings(), High, Error));
	AddInfo(FString::Printf(TEXT("meadow 4deg=%d lande 24deg=%d (heather=%d) 40deg=%d"), Meadow.Instances.Num(),
		Foot.Instances.Num(), AnastasisGroundCoverTestFixture::Count(Foot, Heather), High.Instances.Num()));
	// Plus maigre que la prairie, sans plus : v1 exigeait un facteur 2, mais a cette densite le
	// versant restait invisible (~5 % couvert, mesure). Densite relevee a 0,9 au pied en v2.
	TestTrue(TEXT("lande is sparser than meadow"), Foot.Instances.Num() < Meadow.Instances.Num());
	TestTrue(TEXT("lande thins as it climbs"), High.Instances.Num() < Foot.Instances.Num() && High.Instances.Num() > 0);

	// Callune en haut du versant, touffes d'eboulis en bas : versant de 24 deg sur 100 m, soit
	// ~45 m de denivele au-dessus d'un fond de vallee pose au pied (Z = 1000). Bande 8-28 m.
	AnastasisGroundCover::FInputs Valley = AnastasisGroundCoverTestFixture::OpenPlane(24.0);
	Valley.bHasValleyFloor = true;
	Valley.ValleyFloorZ = 1000.0;
	AnastasisGroundCover::FPlan Hill;
	TestTrue(TEXT("hill builds"), AnastasisGroundCover::Build(Valley, AnastasisGroundCover::FSettings(), Hill, Error));
	int32 HeatherLow = 0, HeatherHigh = 0, TussockLow = 0;
	for (const AnastasisGroundCover::FPlacement& P : Hill.Instances)
	{
		const bool bLow = P.Ground.Z - 1000.0 < 800.0, bHigh = P.Ground.Z - 1000.0 > 2800.0;
		HeatherLow += bLow && P.Family == Heather;
		HeatherHigh += bHigh && P.Family == Heather;
		TussockLow += bLow && P.Family == Heath;
	}
	AddInfo(FString::Printf(TEXT("heather low=%d high=%d tussock low=%d"), HeatherLow, HeatherHigh, TussockLow));
	TestEqual(TEXT("no heather within 8 m of the valley floor"), HeatherLow, 0);
	TestTrue(TEXT("heather holds the upper slope, tussocks the foot"), HeatherHigh > 0 && TussockLow > 0);
	AnastasisGroundCover::FInputs BadFloor = Valley;
	BadFloor.ValleyFloorZ = std::numeric_limits<double>::quiet_NaN();
	AnastasisGroundCover::FPlan Refused;
	TestFalse(TEXT("non-finite valley floor is refused"), AnastasisGroundCover::Build(BadFloor, AnastasisGroundCover::FSettings(), Refused, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGroundCoverSoilTint, "Anastasis.GroundCover.SoilTint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGroundCoverSoilTint::RunTest(const FString&)
{
	// Champ : une prairie a l'ouest (X < 5000), rien a l'est.
	FString Error;
	AnastasisGroundCover::FInputs Half = AnastasisGroundCoverTestFixture::OpenPlane(3.0);
	Half.Mask = [](double X, double) { return X < 5000.0 ? 1.0 : 0.0; };
	AnastasisGroundCover::FPlan Plan;
	TestTrue(TEXT("plan builds"), AnastasisGroundCover::Build(Half, AnastasisGroundCover::FSettings(), Plan, Error));
	AnastasisGroundCover::FCoverField Field;
	AnastasisGroundCover::BuildCoverField(Plan, Half.Bounds, 400.0, AnastasisGroundCover::FSettings().CellUU, Field);
	TestTrue(TEXT("field is valid"), Field.IsValid());
	const FVector4f West = Field.Sample(2000.0, 5000.0), East = Field.Sample(8500.0, 5000.0);
	AddInfo(FString::Printf(TEXT("cover west=(%.2f %.2f %.2f) east=(%.2f %.2f %.2f)"), West.X, West.Y, West.Z, East.X, East.Y, East.Z));
	TestTrue(TEXT("meadow covers the west"), West.X > 0.3f && West.Y < 0.01f && West.Z < 0.01f && West.W < 0.01f);
	TestTrue(TEXT("nothing covers the east"), East.X + East.Y + East.Z + East.W < 0.01f);
	const FVector4f Outside = Field.Sample(-5000.0, 5000.0);
	TestTrue(TEXT("outside the grid is bare"), Outside.X + Outside.Y + Outside.Z + Outside.W == 0.f);

	// Teinte : sol nu intact, alpha intact, prairie plus sombre et plus verte que la lande.
	const AnastasisGroundCover::FSoilTint Tint;
	const FLinearColor Grass(0.16f, 0.19f, 0.07f, 0.0f), Sand(0.30f, 0.25f, 0.15f, 1.0f);
	double Amount = -1.0;
	TestTrue(TEXT("bare soil unchanged"), AnastasisGroundCover::TintSoil(Grass, FVector4f(0, 0, 0, 0), Tint, &Amount).Equals(Grass) && Amount == 0.0);
	const FLinearColor UnderMeadow = AnastasisGroundCover::TintSoil(Grass, FVector4f(1, 0, 0, 0), Tint, &Amount);
	TestTrue(TEXT("full cover tints at full strength"), FMath::IsNearlyEqual(Amount, Tint.Strength, 1e-6));
	TestTrue(TEXT("alpha (underwater flag) kept"), UnderMeadow.A == Grass.A && AnastasisGroundCover::TintSoil(Sand, FVector4f(1, 0, 0, 0), Tint).A == Sand.A);
	TestTrue(TEXT("soil under meadow is darker"), UnderMeadow.GetLuminance() < Grass.GetLuminance());
	const FLinearColor SandMeadow = AnastasisGroundCover::TintSoil(Sand, FVector4f(1, 0, 0, 0), Tint);
	TestTrue(TEXT("sand under meadow turns greener"), SandMeadow.G / SandMeadow.R > Sand.G / Sand.R);
	const FLinearColor UnderLande = AnastasisGroundCover::TintSoil(Grass, FVector4f(0, 0, 1, 0), Tint);
	TestTrue(TEXT("lande soil is browner than meadow soil"), UnderLande.R / UnderLande.G > UnderMeadow.R / UnderMeadow.G);
	const FLinearColor UnderSedge = AnastasisGroundCover::TintSoil(Grass, FVector4f(0, 1, 0, 0), Tint);
	TestTrue(TEXT("sedge soil is the darkest"), UnderSedge.GetLuminance() < UnderMeadow.GetLuminance());
	const FLinearColor UnderWood = AnastasisGroundCover::TintSoil(Grass, FVector4f(0, 0, 0, 1), Tint);
	TestTrue(TEXT("forest litter is brown and dark"), UnderWood.R / UnderWood.G > UnderMeadow.R / UnderMeadow.G
		&& UnderWood.GetLuminance() < UnderMeadow.GetLuminance());
	double Thin = 0.0;
	AnastasisGroundCover::TintSoil(Grass, FVector4f(0.15f, 0, 0, 0), Tint, &Thin);
	TestTrue(TEXT("sparse cover tints less"), Thin > 0.0 && Thin < Amount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGroundCoverWaterAndWetness, "Anastasis.GroundCover.WaterAndWetness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGroundCoverWaterAndWetness::RunTest(const FString&)
{
	FString Error;

	AnastasisGroundCover::FInputs Drowned = AnastasisGroundCoverTestFixture::OpenPlane(2.0);
	Drowned.SampleWaterHeight = [](double, double, double& W) { W = 5000.0; return true; };
	AnastasisGroundCover::FPlan Under;
	TestTrue(TEXT("drowned builds"), AnastasisGroundCover::Build(Drowned, AnastasisGroundCover::FSettings(), Under, Error));
	TestEqual(TEXT("nothing under water"), Under.Instances.Num(), 0);
	TestTrue(TEXT("refused by water"), Under.RejectedWater > 0);

	// Riviere le long de X = 5000 : humidite de rive decroissante sur 15 m.
	AnastasisGroundCover::FInputs River = AnastasisGroundCoverTestFixture::OpenPlane(2.0);
	River.SampleWetness = [](double X, double, double& W) { W = FMath::Clamp(1.0 - FMath::Abs(X - 5000.0) / 1500.0, 0.0, 1.0); return true; };
	AnastasisGroundCover::FPlan Banks;
	TestTrue(TEXT("river builds"), AnastasisGroundCover::Build(River, AnastasisGroundCover::FSettings(), Banks, Error));
	int32 SedgeNear = 0, SedgeFar = 0, MeadowNear = 0;
	for (const AnastasisGroundCover::FPlacement& P : Banks.Instances)
	{
		const bool bNear = FMath::Abs(P.Ground.X - 5000.0) < 500.0;
		const bool bFar = FMath::Abs(P.Ground.X - 5000.0) > 2000.0;
		SedgeNear += bNear && P.Family == AnastasisGroundCover::EFamily::Sedge;
		SedgeFar += bFar && P.Family == AnastasisGroundCover::EFamily::Sedge;
		MeadowNear += bNear && P.Family != AnastasisGroundCover::EFamily::Sedge;
	}
	AddInfo(FString::Printf(TEXT("sedge near=%d far=%d meadow near=%d"), SedgeNear, SedgeFar, MeadowNear));
	TestTrue(TEXT("sedge lines the river"), SedgeNear > 0 && MeadowNear == 0);
	TestEqual(TEXT("no sedge on dry meadow"), SedgeFar, 0);

	// Sol a 20 uu au-dessus de la nappe : sature sans riviere.
	AnastasisGroundCover::FInputs Damp = AnastasisGroundCoverTestFixture::OpenPlane(0.0);
	Damp.SampleWaterHeight = [](double, double, double& W) { W = 980.0; return true; };
	AnastasisGroundCover::FPlan Soaked;
	TestTrue(TEXT("damp builds"), AnastasisGroundCover::Build(Damp, AnastasisGroundCover::FSettings(), Soaked, Error));
	TestTrue(TEXT("low ground over the water table is sedge"), Soaked.Instances.Num() > 0
		&& AnastasisGroundCoverTestFixture::Count(Soaked, AnastasisGroundCover::EFamily::Sedge) == Soaked.Instances.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGroundCoverCanopyAndClearing, "Anastasis.GroundCover.CanopyAndClearing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGroundCoverCanopyAndClearing::RunTest(const FString&)
{
	FString Error;
	const AnastasisGroundCover::FSettings Settings;

	AnastasisGroundCover::FInputs Wood = AnastasisGroundCoverTestFixture::OpenPlane(3.0);
	Wood.Canopy.Add(FVector(3000, 3000, 600));
	Wood.Canopy.Add(FVector(7000, 7000, 900));
	AnastasisGroundCover::FPlan Edge;
	TestTrue(TEXT("canopy builds"), AnastasisGroundCover::Build(Wood, Settings, Edge, Error));
	// Prairie hors des couronnes, sous-bois dessous (H5), rien au pied du tronc.
	bool bMeadowOutside = true, bUnderstoryInside = true, bTrunkClear = true;
	int32 UnderCount = 0;
	for (const AnastasisGroundCover::FPlacement& P : Edge.Instances)
	{
		const bool bUnder = P.Family == AnastasisGroundCover::EFamily::Fern || P.Family == AnastasisGroundCover::EFamily::HartsTongue
			|| P.Family == AnastasisGroundCover::EFamily::WoodHerb;
		UnderCount += bUnder ? 1 : 0;
		double Nearest = TNumericLimits<double>::Max();
		for (const FVector& C : Wood.Canopy)
		{
			Nearest = FMath::Min(Nearest, FVector2D::Distance(FVector2D(P.Ground), FVector2D(C.X, C.Y)) / C.Z);
		}
		bMeadowOutside &= bUnder || Nearest >= Settings.CanopyExclusion;
		bUnderstoryInside &= !bUnder || Nearest < Settings.CanopyExclusion;
		bTrunkClear &= Nearest >= Settings.TrunkClearance;
	}
	AddInfo(FString::Printf(TEXT("understory=%d fern=%d harts=%d herb=%d"), UnderCount, Edge.Counts[5], Edge.Counts[6], Edge.Counts[7]));
	TestTrue(TEXT("no meadow under a crown"), bMeadowOutside);
	TestTrue(TEXT("understory only under a crown"), bUnderstoryInside && UnderCount > 0 && UnderCount == Edge.Understory);
	TestTrue(TEXT("nothing at the trunk"), bTrunkClear && Edge.RejectedCanopy > 0);
	TestTrue(TEXT("dry understory is mostly fern and herbs"), Edge.Counts[5] > 0 && Edge.Counts[7] > 0 && Edge.Counts[6] < Edge.Counts[5]);
	// Sous-bois humide : la scolopendre prend le pas.
	AnastasisGroundCover::FInputs WetWood = Wood;
	WetWood.SampleWetness = [](double, double, double& W) { W = 0.9; return true; };
	AnastasisGroundCover::FPlan Damp;
	TestTrue(TEXT("damp wood builds"), AnastasisGroundCover::Build(WetWood, Settings, Damp, Error));
	TestTrue(TEXT("damp understory favours harts-tongue"), Damp.Counts[6] > Damp.Counts[5] && Damp.Counts[6] > Damp.Counts[7]);

	AnastasisGroundCover::FInputs Hamlet = AnastasisGroundCoverTestFixture::OpenPlane(3.0);
	Hamlet.Clearings.Add({FVector2D(5000, 5000), 2000.0, 0.3});
	AnastasisGroundCover::FPlan Open, Trodden;
	TestTrue(TEXT("open builds"), AnastasisGroundCover::Build(AnastasisGroundCoverTestFixture::OpenPlane(3.0), Settings, Open, Error));
	TestTrue(TEXT("clearing builds"), AnastasisGroundCover::Build(Hamlet, Settings, Trodden, Error));
	const auto Inside = [](const AnastasisGroundCover::FPlan& P, AnastasisGroundCover::EFamily* Only)
	{
		int32 N = 0;
		for (const AnastasisGroundCover::FPlacement& I : P.Instances)
		{
			if (FVector2D::Distance(FVector2D(I.Ground), FVector2D(5000, 5000)) < 1200.0 && (!Only || I.Family == *Only)) ++N;
		}
		return N;
	};
	AnastasisGroundCover::EFamily Tall = AnastasisGroundCover::EFamily::MeadowTall;
	AddInfo(FString::Printf(TEXT("inside clearing: open=%d trodden=%d tall=%d"), Inside(Open, nullptr), Inside(Trodden, nullptr), Inside(Trodden, &Tall)));
	TestTrue(TEXT("trampled ground is sparser"), Inside(Trodden, nullptr) * 2 < Inside(Open, nullptr));
	TestTrue(TEXT("trampled ground is mostly short"), Inside(Trodden, &Tall) * 2 < Inside(Trodden, nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGroundCoverPatchesAndMask, "Anastasis.GroundCover.PatchesAndMask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGroundCoverPatchesAndMask::RunTest(const FString&)
{
	FString Error;

	// Masque : moitie ouest ouverte, moitie est fermee.
	AnastasisGroundCover::FInputs Half = AnastasisGroundCoverTestFixture::OpenPlane(2.0);
	Half.Mask = [](double X, double) { return X < 5000.0 ? 1.0 : 0.0; };
	AnastasisGroundCover::FPlan Plan;
	TestTrue(TEXT("half mask builds"), AnastasisGroundCover::Build(Half, AnastasisGroundCover::FSettings(), Plan, Error));
	bool bInside = Plan.Instances.Num() > 0;
	for (const AnastasisGroundCover::FPlacement& P : Plan.Instances) bInside &= P.Ground.X < 5000.0;
	TestTrue(TEXT("nothing outside the open-ground mask"), bInside);

	// Pas un tapis : la densite par carre de 10 m varie franchement.
	AnastasisGroundCover::FPlan Open;
	TestTrue(TEXT("open builds"), AnastasisGroundCover::Build(AnastasisGroundCoverTestFixture::OpenPlane(2.0), AnastasisGroundCover::FSettings(), Open, Error));
	TArray<int32> Squares;
	Squares.SetNumZeroed(100);
	for (const AnastasisGroundCover::FPlacement& P : Open.Instances)
	{
		++Squares[FMath::Clamp(FMath::FloorToInt(P.Ground.Y / 1000.0), 0, 9) * 10 + FMath::Clamp(FMath::FloorToInt(P.Ground.X / 1000.0), 0, 9)];
	}
	Squares.Sort();
	AddInfo(FString::Printf(TEXT("instances per 10 m square: p10=%d p50=%d p90=%d"), Squares[10], Squares[50], Squares[90]));
	TestTrue(TEXT("patches: dense squares hold at least 1.5x the sparse ones"), 2 * Squares[90] >= 3 * FMath::Max(1, Squares[10]));

	// Entrees invalides : refus explicite, plan vide.
	AnastasisGroundCover::FSettings Bad;
	Bad.MaxSlopeDegrees = 5.0;
	AnastasisGroundCover::FPlan Refused;
	TestFalse(TEXT("tall band above max slope is refused"), AnastasisGroundCover::Build(AnastasisGroundCoverTestFixture::OpenPlane(2.0), Bad, Refused, Error));
	AnastasisGroundCover::FInputs NoMask = AnastasisGroundCoverTestFixture::OpenPlane(2.0);
	NoMask.Mask = nullptr;
	TestFalse(TEXT("missing mask is refused"), AnastasisGroundCover::Build(NoMask, AnastasisGroundCover::FSettings(), Refused, Error));
	AnastasisGroundCover::FInputs NaNCrown = AnastasisGroundCoverTestFixture::OpenPlane(2.0);
	NaNCrown.Canopy.Add(FVector(std::numeric_limits<double>::quiet_NaN(), 0, 100));
	TestFalse(TEXT("non-finite crown is refused"), AnastasisGroundCover::Build(NaNCrown, AnastasisGroundCover::FSettings(), Refused, Error));
	TestEqual(TEXT("refused plan is empty"), Refused.Instances.Num(), 0);

	// Garde-fou de cout : tronque, et le dit.
	AnastasisGroundCover::FSettings Capped;
	Capped.MaxInstances = 100;
	AnastasisGroundCover::FPlan Cut;
	TestTrue(TEXT("capped builds"), AnastasisGroundCover::Build(AnastasisGroundCoverTestFixture::OpenPlane(2.0), Capped, Cut, Error));
	TestTrue(TEXT("cap reported"), Cut.bTruncated && Cut.Instances.Num() == 100);
	return true;
}
#endif
