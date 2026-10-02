#include "Misc/AutomationTest.h"
#include "WorldView/AnastasisTectonics.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisTerrainHorizon.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisWorldView.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
AnastasisTectonics::FTectonicFrame TectTestFrame()
{
	// Eau qui sort a l'ouest : les basses terres sont a l'ouest, les chaines a l'est.
	return AnastasisTectonics::MakeFrame(FVector2D(-1.0, 0.0), 12345);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTectonicsDeterministic, "Anastasis.Terrain.Tectonics.Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTectonicsDeterministic::RunTest(const FString&)
{
	const auto Frame = TectTestFrame();
	const auto Other = AnastasisTectonics::MakeFrame(FVector2D(-1.0, 0.0), 777);
	int32 NonFinite = 0, Differs = 0, OutOfRange = 0, Samples = 0;
	double Min = 1.e9, Max = -1.e9;
	for (double Y = -60.0; Y <= 60.0; Y += 1.7)
	{
		for (double X = -60.0; X <= 60.0; X += 1.7)
		{
			const double A = AnastasisTectonics::HeightM(Frame, X, Y);
			const double B = AnastasisTectonics::HeightM(Frame, X, Y);
			NonFinite += FMath::IsFinite(A) ? 0 : 1;
			Differs += A == B ? 0 : 1;
			OutOfRange += (A < -10.0 || A > AnastasisTectonics::MaxHeightM) ? 1 : 0;
			Min = FMath::Min(Min, A);
			Max = FMath::Max(Max, A);
			++Samples;
		}
	}
	TestEqual(TEXT("aucune hauteur non finie"), NonFinite, 0);
	TestEqual(TEXT("meme point, meme hauteur"), Differs, 0);
	TestEqual(TEXT("hauteurs dans [0, MaxHeightM]"), OutOfRange, 0);
	int32 SeedDiffers = 0;
	for (double X = -40.0; X <= 40.0; X += 3.1)
	{
		SeedDiffers += AnastasisTectonics::HeightM(Frame, X, 11.0) != AnastasisTectonics::HeightM(Other, X, 11.0) ? 1 : 0;
	}
	TestTrue(TEXT("une autre graine, un autre continent"), SeedDiffers > 8);
	AddInfo(FString::Printf(TEXT("TECTONICS_RANGE samples=%d min_m=%.0f max_m=%.0f"), Samples, Min, Max));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTectonicsStructure, "Anastasis.Terrain.Tectonics.Structure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTectonicsStructure::RunTest(const FString&)
{
	using namespace AnastasisTectonics;
	const FTectonicFrame Frame = TectTestFrame();
	// Coordonnees tectoniques : u vers l'interieur (a l'est ici), v le long de la chaine.
	// Ici Down = (-1,0) donc u = x ; Strike = (0,-1) donc v = -y. Le gauchissement decale un peu.
	auto At = [&Frame](double U, double V) { return HeightM(Frame, U, -V); };

	// Echantillon de l'interieur et des basses terres a la meme distance de la carte.
	double Interior = 0.0, Lowland = 0.0;
	int32 N = 0;
	for (double V = -30.0; V <= 30.0; V += 2.0)
	{
		Interior += At(14.0, V);
		Lowland += At(-14.0, V);
		++N;
	}
	Interior /= N;
	Lowland /= N;
	TestTrue(FString::Printf(TEXT("l'interieur est bien plus haut que les basses terres a 14 km (%.0f contre %.0f m)"), Interior, Lowland),
		Interior > Lowland + 700.0);

	// La chaine principale : un sommet de plus de 2 000 m entre 8 et 22 km.
	// (Le gauchissement decale les axes de quelques km : on balaie large.)
	double CrestMax = 0.0;
	for (double V = -40.0; V <= 40.0; V += 0.8)
	{
		for (double U = 4.0; U <= 28.0; U += 0.4)
		{
			CrestMax = FMath::Max(CrestMax, At(U, V));
		}
	}
	TestTrue(FString::Printf(TEXT("la chaine principale depasse 2 000 m (%.0f)"), CrestMax), CrestMax > 2000.0);

	// La seconde chaine, au fond du plateau, est plus haute que la premiere.
	double BackMax = 0.0;
	for (double V = -40.0; V <= 40.0; V += 1.2)
	{
		for (double U = 26.0; U <= 56.0; U += 0.6)
		{
			BackMax = FMath::Max(BackMax, At(U, V));
		}
	}
	TestTrue(FString::Printf(TEXT("la seconde chaine (%.0f m) est plus haute que la premiere (%.0f m)"), BackMax, CrestMax), BackMax > CrestMax);

	// La faille decrochante : sur chaque coupe perpendiculaire a la chaine, la tranchee se
	// trouve quelque part entre 2 et 14 km ; elle doit y creuser plus de 60 m presque partout.
	double DeepestTrench = 0.0;
	int32 TrenchHits = 0, Cuts = 0;
	// La ceinture de plis : sa crete la plus haute, et le nombre de coupes qui en portent une.
	double TallestFold = 0.0;
	int32 FoldCuts = 0;
	for (double V = -30.0; V <= 30.0; V += 1.0)
	{
		double CutTrench = 0.0, CutFold = 0.0;
		for (double X = 2.0; X <= 14.0; X += 0.05)
		{
			FBreakdown B;
			Evaluate(Frame, X, -V, B);
			CutTrench = FMath::Min(CutTrench, B.Fault);
			CutFold = FMath::Max(CutFold, B.Folds);
		}
		DeepestTrench = FMath::Min(DeepestTrench, CutTrench);
		TrenchHits += CutTrench < -60.0 ? 1 : 0;
		TallestFold = FMath::Max(TallestFold, CutFold);
		FoldCuts += CutFold > 90.0 ? 1 : 0;
		++Cuts;
	}
	TestTrue(FString::Printf(TEXT("la faille decrochante creuse une tranchee de plus de 80 m (%.0f)"), DeepestTrench), DeepestTrench < -80.0);
	TestTrue(FString::Printf(TEXT("et la creuse sur la plus grande partie de son trace (%d/%d)"), TrenchHits, Cuts), TrenchHits * 10 > Cuts * 8);
	TestTrue(FString::Printf(TEXT("la ceinture de plis porte des crêtes de plus de 150 m (%.0f)"), TallestFold), TallestFold > 150.0);
	TestTrue(FString::Printf(TEXT("sur au moins la moitie des coupes (%d/%d)"), FoldCuts, Cuts), FoldCuts * 2 > Cuts);
	const int32 Ridges = FoldCuts;

	// Aucune direction ne s'ouvre sur le vide : a 60 km, partout au moins 150 m.
	double LowestFar = 1.e9;
	for (int32 Step = 0; Step < 72; ++Step)
	{
		const double A = FMath::DegreesToRadians(Step * 5.0);
		LowestFar = FMath::Min(LowestFar, HeightM(Frame, 60.0 * FMath::Cos(A), 60.0 * FMath::Sin(A)));
	}
	TestTrue(FString::Printf(TEXT("a 60 km, aucune direction sous 150 m (%.0f)"), LowestFar), LowestFar > 150.0);

	// Pas de vallee interdite : sous la carte (la ou habite le joueur), le plan de reference.
	TestTrue(TEXT("sous la carte, le sol de reference reste bas"), HeightM(Frame, 0.0, 0.0) < 120.0);
	AddInfo(FString::Printf(TEXT("TECTONICS_STRUCTURE interior14=%.0f lowland14=%.0f crest=%.0f backdrop=%.0f trench=%.0f ridges=%d lowest60=%.0f"),
		Interior, Lowland, CrestMax, BackMax, DeepestTrench, Ridges, LowestFar));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTectonicsFrame, "Anastasis.Terrain.Tectonics.Frame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTectonicsFrame::RunTest(const FString&)
{
	using namespace AnastasisTectonics;
	const FTectonicFrame West = MakeFrame(FVector2D(-3.0, 0.0), 12345);
	TestTrue(TEXT("sortie a l'ouest : Down vers l'ouest"), West.Down.X < -0.999 && FMath::Abs(West.Down.Y) < 1.e-6);
	TestTrue(TEXT("axe de chaine perpendiculaire"), FMath::Abs(FVector2D::DotProduct(West.Down, West.Strike())) < 1.e-9);
	TestTrue(TEXT("axe unitaire"), FMath::IsNearlyEqual(West.Strike().Size(), 1.0, 1.e-9));

	const FTectonicFrame None = MakeFrame(FVector2D::ZeroVector, 12345);
	const FTectonicFrame NoneAgain = MakeFrame(FVector2D::ZeroVector, 12345);
	const FTectonicFrame NoneOther = MakeFrame(FVector2D::ZeroVector, 999);
	TestTrue(TEXT("sans eau au bord : unitaire"), FMath::IsNearlyEqual(None.Down.Size(), 1.0, 1.e-9));
	TestTrue(TEXT("sans eau au bord : deterministe"), None.Down == NoneAgain.Down);
	TestTrue(TEXT("sans eau au bord : la graine decide"), !(None.Down == NoneOther.Down));

	// L'eau sort la ou le continent descend : l'interieur est a l'oppose, quel que soit le cote.
	const FTectonicFrame North = MakeFrame(FVector2D(0.0, 5.0), 12345);
	double Inland = 0.0, Coast = 0.0;
	for (double V = -20.0; V <= 20.0; V += 2.0)
	{
		Inland += HeightM(North, 14.0 * -North.Down.X + V * North.Strike().X, 14.0 * -North.Down.Y + V * North.Strike().Y);
		Coast += HeightM(North, 14.0 * North.Down.X + V * North.Strike().X, 14.0 * North.Down.Y + V * North.Strike().Y);
	}
	TestTrue(FString::Printf(TEXT("sortie au nord : l'interieur (sud) est plus haut (%.0f contre %.0f)"), Inland / 21.0, Coast / 21.0),
		Inland > Coast + 21.0 * 500.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTectonicsSurface, "Anastasis.Terrain.Tectonics.Surface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTectonicsSurface::RunTest(const FString&)
{
	using namespace AnastasisTectonics;
	const FTectonicFrame Frame = TectTestFrame();
	const FSurface Valley = SurfaceAt(Frame, 3.0, 2.0, 40.0, 6.0);
	TestTrue(TEXT("vallee : ni foret, ni roche, ni neige"), Valley.Forest == 0.0 && Valley.Rock == 0.0 && Valley.Snow == 0.0 && Valley.Alpine == 0.0);
	const FSurface Montane = SurfaceAt(Frame, 3.0, 2.0, 1000.0, 15.0);
	TestTrue(TEXT("versant montagnard : foret"), Montane.Forest > 0.9 && Montane.Snow == 0.0 && Montane.Rock < 0.1);
	const FSurface Summit = SurfaceAt(Frame, 3.0, 2.0, 3900.0, 28.0);
	TestTrue(TEXT("sommet : neige"), Summit.Snow > 0.9 && Summit.Forest == 0.0);
	const FSurface Cliff = SurfaceAt(Frame, 3.0, 2.0, 1200.0, 62.0);
	TestTrue(TEXT("paroi a 62 degres : roche"), Cliff.Rock > 0.85 && Cliff.Forest < 0.15);
	const FSurface SteepSummit = SurfaceAt(Frame, 3.0, 2.0, 3900.0, 70.0);
	TestTrue(TEXT("face quasi verticale au-dessus des neiges : peu de neige"), SteepSummit.Snow < Summit.Snow);
	// La limite des neiges monte avec l'altitude, jamais l'inverse.
	double PrevSnow = 0.0;
	bool bMonotone = true;
	for (double H = 1500.0; H <= 4400.0; H += 100.0)
	{
		const double Snow = SurfaceAt(Frame, 3.0, 2.0, H, 30.0).Snow;
		bMonotone &= Snow + 1.e-9 >= PrevSnow;
		PrevSnow = Snow;
	}
	TestTrue(TEXT("la neige ne diminue pas avec l'altitude"), bMonotone);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTectonicsHorizon, "Anastasis.Terrain.Tectonics.Horizon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTectonicsHorizon::RunTest(const FString&)
{
	auto Crop = AnastasisWorldView::CaptureCanonicalWorld(12345);
	Crop.SpatialScale = 5.0;
	Crop.bHumanGeography = true;
	AnastasisTerrainSurface::FGeometry Geometry;
	AnastasisTerrainForge::FMesh Forge;
	const bool bForged = AnastasisTerrainSurface::Build(Crop, Geometry) && AnastasisTerrainForge::Apply(Crop, Geometry, Forge);
	AnastasisTerrainForge::ClearActive();
	if (!TestTrue(TEXT("forge du monde canonique"), bForged))
	{
		return false;
	}
	AnastasisTerrainHorizon::FRing Ring;
	if (!TestTrue(TEXT("anneau"), AnastasisTerrainHorizon::Build(Forge, 12345, Ring)))
	{
		return false;
	}
	const auto& G = Ring.Geometry;
	TestTrue(FString::Printf(TEXT("l'anneau porte de vraies montagnes (%.0f m)"), Ring.MaxHeightM), Ring.MaxHeightM > 2400.0);
	TestTrue(FString::Printf(TEXT("neige sur les sommets (%d sommets)"), Ring.SnowVertices), Ring.SnowVertices > 50);
	TestTrue(FString::Printf(TEXT("la neige ne couvre pas toute la montagne (neige %d, roche %d, foret %d)"), Ring.SnowVertices, Ring.RockVertices, Ring.ForestVertices),
		Ring.SnowVertices < Ring.ForestVertices);
	TestTrue(FString::Printf(TEXT("foret montagnarde (%d sommets)"), Ring.ForestVertices), Ring.ForestVertices > 500);
	TestTrue(FString::Printf(TEXT("roche (%d sommets)"), Ring.RockVertices), Ring.RockVertices > 200);
	TestTrue(TEXT("l'anneau va jusqu'a 60 km"), Ring.Distances[Ring.Rings - 2] / 100.0 >= 59000.0);

	// Resolution angulaire : a 14 km du centre, une cellule fait moins de 0,02 de la distance.
	const double TileStep = AnastasisWorldView::TileWorldSize * Forge.SpatialScale;
	int32 Coarse = 0;
	for (int32 K = 2; K < Ring.Rings - 2; ++K)
	{
		const double Radial = Ring.Distances[K + 1] - Ring.Distances[K];
		const double AtCenter = Ring.Distances[K] + 0.5 * TileStep * 96.0;
		Coarse += Radial > 0.02 * AtCenter ? 1 : 0;
	}
	TestEqual(TEXT("aucun anneau plus epais que 2 % de sa distance au centre"), Coarse, 0);

	// Hauteur apparente de la chaine depuis le bassin : mesure sur les sommets de l'anneau.
	const FVector Eye(Forge.BasinX, Forge.BasinY, Forge.BasinZ + 170.0);
	double BestDeg = -90.0;
	for (int32 V = Ring.Perimeter; V < G.Vertices.Num(); ++V)
	{
		const FVector Rel = G.Vertices[V] - Eye;
		const double Flat = FVector2D(Rel.X, Rel.Y).Size();
		if (Flat > 1.e4 && Ring.Distances[Ring.RingOf(V)] < 6.0e6)
		{
			BestDeg = FMath::Max(BestDeg, FMath::RadiansToDegrees(FMath::Atan2(Rel.Z, Flat)));
		}
	}
	TestTrue(FString::Printf(TEXT("depuis le bassin, la chaine monte a plus de 5 degres dans le ciel (%.1f)"), BestDeg), BestDeg > 5.0);
	AddInfo(FString::Printf(TEXT("TECTONICS_HORIZON max_m=%.0f snow=%d rock=%d forest=%d best_elevation_deg=%.1f down=(%.2f,%.2f) vertices=%d triangles=%d rings=%d"),
		Ring.MaxHeightM, Ring.SnowVertices, Ring.RockVertices, Ring.ForestVertices, BestDeg, Ring.Tectonics.Down.X, Ring.Tectonics.Down.Y,
		G.Vertices.Num(), G.Triangles.Num() / 3, Ring.Rings));
	return true;
}

#endif
