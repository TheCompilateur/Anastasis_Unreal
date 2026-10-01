#include "WorldView/AnastasisUnderstory.h"
#include "WorldView/AnastasisWorldView.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisUnderstoryTestDetail
{
using namespace AnastasisUnderstory;
using AnastasisWorld::ETileType;

constexpr double Tile = AnastasisWorldView::TileWorldSize;
constexpr double Span = 2000.0;
constexpr double Water = 275.0;

/** Le monde canonique, types reecrits ; echelle 1 (tuile de 4 m). */
AnastasisWorldView::FWorldVisualSnapshot World(ETileType Type)
{
	auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
	for (auto& T : S.Tiles) { T.Type = Type; T.Wetness = 0.0; }
	return S;
}

/**
 * Relief en dents de scie de pente constante (periode 2000 uu) : une pente reguliere sur toute la
 * carte ferait sortir l'altitude relative de la bande visee, et l'altitude decide aussi.
 */
AnastasisUnderstory::FInputs Inputs(const AnastasisWorldView::FWorldVisualSnapshot& S, double Altitude, double SlopeDegrees = 0.0)
{
	AnastasisUnderstory::FInputs In;
	In.Source = &S;
	const double Rise = FMath::Tan(FMath::DegreesToRadians(SlopeDegrees));
	const double Base = Water + Altitude * Span;
	In.SampleHeight = [Base, Rise](double X, double, double& Z)
	{
		constexpr double Period = 2000.0;
		Z = Base + Rise * (0.5 * Period - FMath::Abs(FMath::Fmod(X, Period) - 0.5 * Period));
		return true;
	};
	In.WaterPlaneZ = Water;
	In.AltitudeSpanUU = Span;
	return In;
}

int32 Count(const AnastasisUnderstory::FPlan& P, AnastasisUnderstory::EKind K) { return P.Counts[static_cast<int32>(K)]; }
int32 Shrubs(const AnastasisUnderstory::FPlan& P) { return Count(P, AnastasisUnderstory::EKind::Lentisk) + Count(P, AnastasisUnderstory::EKind::KermesOak) + Count(P, AnastasisUnderstory::EKind::Broom); }

double MeanRockSize(const AnastasisUnderstory::FPlan& P)
{
	double Sum = 0.0; int32 N = 0;
	for (const FInstance& I : P.Instances) if (I.Kind == AnastasisUnderstory::EKind::Rock) { Sum += I.HeightM; ++N; }
	return N > 0 ? Sum / N : 0.0;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisUnderstoryRules, "Anastasis.Understory.SlopeAltitudeAndReserves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisUnderstoryRules::RunTest(const FString&)
{
	using namespace AnastasisUnderstoryTestDetail;
	const auto Grass = World(ETileType::Grass);
	FSettings C;
	AnastasisUnderstory::FPlan Flat, Again, Steep, Cliffy, High;
	FString E;
	if (!TestTrue(TEXT("flat meadow"), Build(Inputs(Grass, 0.36), C, Flat, E))) return false;
	TestTrue(TEXT("repeat"), Build(Inputs(Grass, 0.36), C, Again, E));
	TestEqual(TEXT("deterministic count"), Again.Instances.Num(), Flat.Instances.Num());
	for (int32 I = 0; I < Flat.Instances.Num() && I < Again.Instances.Num(); ++I)
	{
		if (!TestTrue(TEXT("identical instance"), Flat.Instances[I].Ground == Again.Instances[I].Ground
			&& Flat.Instances[I].Kind == Again.Instances[I].Kind && Flat.Instances[I].HeightM == Again.Instances[I].HeightM)) break;
	}
	TestTrue(TEXT("the meadow holds shrubs"), Shrubs(Flat) > 100);
	TestTrue(TEXT("and a few scattered rocks"), Count(Flat, AnastasisUnderstory::EKind::Rock) > 0);
	TestEqual(TEXT("no bramble without an edge or a river"), Count(Flat, AnastasisUnderstory::EKind::Bramble), 0);

	// Les rochers suivent la pente, et grossissent avec elle.
	TestTrue(TEXT("slope 30"), Build(Inputs(Grass, 0.36, 30.0), C, Steep, E));
	TestTrue(TEXT("rocks multiply with the slope"), Count(Steep, AnastasisUnderstory::EKind::Rock) > 5 * FMath::Max(1, Count(Flat, AnastasisUnderstory::EKind::Rock)));
	TestTrue(TEXT("slope 40"), Build(Inputs(Grass, 0.36, 40.0), C, Cliffy, E));
	TestTrue(TEXT("rocks grow on steep ground"), MeanRockSize(Cliffy) > 1.3 * MeanRockSize(Flat));
	for (const FInstance& I : Cliffy.Instances)
	{
		if (I.Kind != AnastasisUnderstory::EKind::Rock && !TestTrue(TEXT("no shrub beyond its slope"), I.SlopeDegrees <= C.MaxShrubSlopeDegrees)) break;
	}

	// Le maquis tient le bas ; en haut il se raréfie.
	TestTrue(TEXT("summit"), Build(Inputs(Grass, 0.95), C, High, E));
	TestTrue(TEXT("maquis thins out with altitude"), Shrubs(High) < 0.4 * Shrubs(Flat));

	// Reserves : champs, ruines, eau, bassin du village.
	for (const ETileType Reserved : {ETileType::Field, ETileType::Ruin, ETileType::Water})
	{
		const auto S = World(Reserved);
		AnastasisUnderstory::FPlan P;
		TestTrue(TEXT("reserved build"), Build(Inputs(S, 0.36), C, P, E));
		TestEqual(TEXT("nothing on fields, ruins or simulation water"), P.Instances.Num(), 0);
	}
	{
		AnastasisUnderstory::FInputs In = Inputs(Grass, 0.36);
		In.SampleWaterHeight = [](double, double, double& Z) { Z = 5000.0; return true; };
		AnastasisUnderstory::FPlan P;
		TestTrue(TEXT("flooded build"), Build(In, C, P, E));
		TestEqual(TEXT("nothing under the rendered water"), P.Instances.Num(), 0);
	}
	{
		AnastasisUnderstory::FInputs In = Inputs(Grass, 0.36);
		In.bHasBasin = true;
		In.Basin = FVector(48 * Tile, 48 * Tile, 0.0);
		AnastasisUnderstory::FPlan P;
		TestTrue(TEXT("basin build"), Build(In, C, P, E));
		for (const FInstance& I : P.Instances)
		{
			if (!TestTrue(TEXT("village basin kept clear"), FVector2D::Distance(FVector2D(I.Ground), FVector2D(In.Basin)) >= C.BasinClearRadiusTiles * Tile)) break;
		}
	}
	AddInfo(FString::Printf(TEXT("UNDERSTORY flat shrubs=%d rocks=%d | slope30 rocks=%d | slope40 rock_m=%.2f vs %.2f | summit shrubs=%d"),
		Shrubs(Flat), Count(Flat, AnastasisUnderstory::EKind::Rock), Count(Steep, AnastasisUnderstory::EKind::Rock), MeanRockSize(Cliffy), MeanRockSize(Flat), Shrubs(High)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisUnderstoryEdges, "Anastasis.Understory.EdgesRiversAndSpecies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisUnderstoryEdges::RunTest(const FString&)
{
	using namespace AnastasisUnderstoryTestDetail;
	const auto Grass = World(ETileType::Grass);
	FSettings C;
	FString E;

	// Ronces et maquis a la lisiere des couronnes, peu sous le couvert.
	AnastasisUnderstory::FInputs Wood = Inputs(Grass, 0.36);
	for (int32 I = 4; I < 15; ++I)
		for (int32 J = 4; J < 15; ++J)
			Wood.Canopy.Add(FVector((I + 0.5) * 2000.0, (J + 0.5) * 2000.0, 600.0));
	AnastasisUnderstory::FPlan W;
	if (!TestTrue(TEXT("wood build"), Build(Wood, C, W, E))) return false;
	const auto Relative = [&Wood](double X, double Y)
	{
		double Rel = TNumericLimits<double>::Max();
		for (const FVector& Crown : Wood.Canopy) Rel = FMath::Min(Rel, FVector2D::Distance(FVector2D(X, Y), FVector2D(Crown.X, Crown.Y)) / Crown.Z);
		return Rel;
	};
	int32 Ring = 0, Far = 0, UnderShrubs = 0, OpenShrubs = 0;
	for (const FInstance& I : W.Instances)
	{
		const double Rel = Relative(I.Ground.X, I.Ground.Y);
		const bool bShrub = I.Kind != AnastasisUnderstory::EKind::Bramble && I.Kind != AnastasisUnderstory::EKind::Rock;
		if (I.Kind == AnastasisUnderstory::EKind::Bramble) { Ring += Rel >= 1.0 && Rel <= 2.6 ? 1 : 0; Far += Rel > 4.0 ? 1 : 0; }
		if (bShrub && Rel < 0.75) ++UnderShrubs;
		if (bShrub && Rel > 4.0) ++OpenShrubs;
	}
	// Surfaces mesurees, en cellules : sous couvert (< 0.75 rayon) et ouvert (> 4 rayons).
	double UnderCells = 0.0, OpenCells = 0.0;
	for (int32 GY = 0; GY < 192; ++GY)
		for (int32 GX = 0; GX < 192; ++GX)
		{
			const double Rel = Relative((GX + 0.5) * 200.0, (GY + 0.5) * 200.0);
			// Un point de grille vaut 200 x 200 uu, en cellules du plan.
			const double Share = 200.0 * 200.0 / (C.CellUU * C.CellUU);
			UnderCells += Rel < 0.75 ? Share : 0.0;
			OpenCells += Rel > 4.0 ? Share : 0.0;
		}
	TestTrue(TEXT("brambles line the forest edge"), Ring > 50);
	TestTrue(TEXT("and stay there"), Far * 20 <= Ring);
	TestTrue(TEXT("the shade under the crowns thins the maquis"), UnderShrubs / UnderCells < 0.5 * OpenShrubs / OpenCells);

	// Ronces le long des rivieres rendues.
	AnastasisUnderstory::FInputs River = Inputs(Grass, 0.36);
	River.SampleRiparian = [](double X, double, double& R) { R = X < 20.0 * Tile ? 0.5 : 0.0; return true; };
	AnastasisUnderstory::FPlan Rv;
	TestTrue(TEXT("river build"), Build(River, C, Rv, E));
	int32 Bank = 0, Dry = 0;
	for (const FInstance& I : Rv.Instances)
		if (I.Kind == AnastasisUnderstory::EKind::Bramble) (I.Ground.X < 20.0 * Tile ? Bank : Dry) += 1;
	TestTrue(TEXT("brambles follow the river band"), Bank > 100 && Dry * 20 <= Bank);

	// Essences : lentisque au bas et au plat, kermes sur la pente rocheuse.
	AnastasisUnderstory::FPlan Low, Rocky;
	TestTrue(TEXT("low build"), Build(Inputs(Grass, 0.05), C, Low, E));
	const auto Stone = World(ETileType::Stone);
	TestTrue(TEXT("rocky build"), Build(Inputs(Stone, 0.4, 20.0), C, Rocky, E));
	TestTrue(TEXT("lentisk leads on low flat ground"), Count(Low, AnastasisUnderstory::EKind::Lentisk) > Count(Low, AnastasisUnderstory::EKind::KermesOak) + Count(Low, AnastasisUnderstory::EKind::Broom));
	TestTrue(TEXT("kermes oak leads on rocky slopes"), Count(Rocky, AnastasisUnderstory::EKind::KermesOak) > Count(Rocky, AnastasisUnderstory::EKind::Lentisk));

	// Entrees invalides : refus net, rien de partiel.
	AnastasisUnderstory::FInputs Broken = Inputs(Grass, 0.36);
	Broken.AltitudeSpanUU = std::numeric_limits<double>::quiet_NaN();
	AnastasisUnderstory::FPlan Bad;
	TestFalse(TEXT("NaN span rejected"), Build(Broken, C, Bad, E));
	TestEqual(TEXT("no partial plan"), Bad.Instances.Num(), 0);
	AnastasisUnderstory::FInputs NoSource;
	TestFalse(TEXT("missing source rejected"), Build(NoSource, C, Bad, E));
	AddInfo(FString::Printf(TEXT("UNDERSTORY_EDGES ring=%d far=%d under=%.3f open=%.3f bank=%d dry=%d low L/K/B=%d/%d/%d rocky K/L=%d/%d"),
		Ring, Far, UnderShrubs / UnderCells, OpenShrubs / OpenCells, Bank, Dry,
		Count(Low, AnastasisUnderstory::EKind::Lentisk), Count(Low, AnastasisUnderstory::EKind::KermesOak), Count(Low, AnastasisUnderstory::EKind::Broom),
		Count(Rocky, AnastasisUnderstory::EKind::KermesOak), Count(Rocky, AnastasisUnderstory::EKind::Lentisk)));
	return true;
}
#endif
