#include "Misc/AutomationTest.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisTerrainHorizon.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisWorldView.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
/**
 * Le monde tel que le jeu l'incarne par defaut : echelle 5 (1.9 km), Human_Geography_V2.
 * C'est son bord, et pas celui du monde d'essai a l'echelle 1, que l'anneau doit raccorder.
 */
bool ForgeCanonical(AnastasisTerrainForge::FMesh& OutMesh)
{
	auto Crop = AnastasisWorldView::CaptureCanonicalWorld(12345);
	Crop.SpatialScale = 5.0;
	Crop.bHumanGeography = true;
	AnastasisTerrainSurface::FGeometry Geometry;
	const bool bOk = AnastasisTerrainSurface::Build(Crop, Geometry) && AnastasisTerrainForge::Apply(Crop, Geometry, OutMesh);
	AnastasisTerrainForge::ClearActive();
	return bOk;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainHorizonSeam, "Anastasis.Terrain.Horizon.Seam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainHorizonSeam::RunTest(const FString&)
{
	AnastasisTerrainForge::FMesh Forge;
	if (!TestTrue(TEXT("forge du monde canonique"), ForgeCanonical(Forge)))
	{
		return false;
	}
	AnastasisTerrainHorizon::FRing Ring;
	if (!TestTrue(TEXT("anneau bati"), AnastasisTerrainHorizon::Build(Forge, 12345, Ring)))
	{
		return false;
	}
	const auto& G = Ring.Geometry;
	const int32 P = Ring.Perimeter;
	const TArray<int32> Edge = AnastasisTerrainHorizon::PerimeterIndices(Forge.FineW, Forge.FineH);
	TestEqual(TEXT("pourtour = bord fin complet"), P, 2 * (Forge.FineW - 1) + 2 * (Forge.FineH - 1));
	// Paliers : un anneau garde ou divise par deux les colonnes de l'anneau interieur, jamais
	// plus d'un palier a la fois ; l'anneau 1 est au pas fin (bande de raccord).
	int32 ExpectedVertices = 0, ExpectedTriangles = 0;
	bool bLevelsValid = Ring.RingLevel.Num() == Ring.Rings && Ring.RingLevel[0] == 0 && Ring.RingLevel[1] == 0;
	for (int32 K = 0; K < Ring.Rings; ++K)
	{
		ExpectedVertices += P >> Ring.RingLevel[K];
		if (K > 0)
		{
			const int32 Delta = Ring.RingLevel[K] - Ring.RingLevel[K - 1];
			bLevelsValid &= Delta == 0 || Delta == 1;
			ExpectedTriangles += Delta == 0 ? 2 * (P >> Ring.RingLevel[K]) : 3 * (P >> Ring.RingLevel[K]);
		}
	}
	TestTrue(TEXT("paliers valides (0 au raccord, +1 au plus par anneau)"), bLevelsValid);
	TestTrue(TEXT("les anneaux lointains sont decimes"), Ring.RingLevel.Last() > 0);
	TestEqual(TEXT("un sommet par (anneau, colonne)"), G.Vertices.Num(), ExpectedVertices);
	TestEqual(TEXT("anneau ferme, paliers cousus"), G.Triangles.Num(), ExpectedTriangles * 3);
	int32 BadIndex = 0;
	for (const int32 Index : G.Triangles)
	{
		BadIndex += G.Vertices.IsValidIndex(Index) ? 0 : 1;
	}
	TestEqual(TEXT("indices valides"), BadIndex, 0);
	TestEqual(TEXT("couleur par sommet"), G.Colors.Num(), G.Vertices.Num());
	TestEqual(TEXT("UV0 par sommet"), G.UV0.Num(), G.Vertices.Num());
	TestEqual(TEXT("UV1 par sommet"), G.UV1.Num(), G.Vertices.Num());

	// Couture : l'anneau 0 EST le bord forge, bit a bit.
	double PosGap = 0.0, NormalGap = 0.0, WaterGap = 0.0;
	for (int32 I = 0; I < P; ++I)
	{
		PosGap = FMath::Max(PosGap, FVector::Dist(G.Vertices[I], Forge.Geometry.Vertices[Edge[I]]));
		NormalGap = FMath::Max(NormalGap, FVector::Dist(G.Normals[I], Forge.Geometry.Normals[Edge[I]]));
		WaterGap = FMath::Max(WaterGap, FVector::Dist(G.WaterVertices[I], Forge.Geometry.WaterVertices[Edge[I]]));
	}
	TestEqual(TEXT("couture : ecart de position nul"), PosGap, 0.0);
	TestEqual(TEXT("couture : ecart de normale nul"), NormalGap, 0.0);
	TestEqual(TEXT("couture : nappe d'eau identique au bord"), WaterGap, 0.0);
	TestEqual(TEXT("une nappe par sommet d'anneau"), G.WaterVertices.Num(), G.Vertices.Num());

	// L'eau qui sort de la carte continue dans l'anneau, puis s'acheve : aucun triangle d'eau
	// au-dela de RiverEndTiles (sinon ce ne serait plus une rivière mais une mer sans rive).
	const int32 EdgeWater = Ring.EdgeWater;
	int32 WetQuads = 0, WetBeyondRiver = 0;
	for (int32 T = 0; T < G.WaterTriangles.Num(); T += 3)
	{
		const int32 K = Ring.RingOf(G.WaterTriangles[T]);
		++WetQuads;
		WetBeyondRiver += Ring.Distances[K] > AnastasisTerrainHorizon::RiverEndTiles * AnastasisWorldView::TileWorldSize * Forge.SpatialScale ? 1 : 0;
	}
	if (EdgeWater > 0)
	{
		TestTrue(TEXT("l'eau du bord continue dans l'anneau"), WetQuads > 0);
	}
	TestEqual(TEXT("aucune eau au-dela de la fin de rivière"), WetBeyondRiver, 0);

	// Hors de la carte : aucun sommet d'anneau (hors anneau 0) ne recouvre le maillage forge.
	const double MinX = Forge.Geometry.Vertices[0].X, MaxX = Forge.Geometry.Vertices[Forge.FineW - 1].X;
	const double MinY = Forge.Geometry.Vertices[0].Y, MaxY = Forge.Geometry.Vertices[(Forge.FineH - 1) * Forge.FineW].Y;
	int32 Inside = 0, NonFinite = 0;
	for (int32 V = P; V < G.Vertices.Num(); ++V)
	{
		const FVector& X = G.Vertices[V];
		NonFinite += X.ContainsNaN() ? 1 : 0;
		Inside += (X.X > MinX && X.X < MaxX && X.Y > MinY && X.Y < MaxY) ? 1 : 0;
	}
	TestEqual(TEXT("aucun sommet non fini"), NonFinite, 0);
	TestEqual(TEXT("aucun sommet d'anneau sur la carte"), Inside, 0);

	bool bIncreasing = true;
	for (int32 K = 1; K < Ring.Distances.Num(); ++K)
	{
		bIncreasing &= Ring.Distances[K] > Ring.Distances[K - 1];
	}
	TestTrue(TEXT("anneaux strictement croissants"), bIncreasing);
	const double TileStep = AnastasisWorldView::TileWorldSize * Forge.SpatialScale;
	TestTrue(TEXT("premier anneau au pas fin de la forge"),
		FMath::IsNearlyEqual(Ring.Distances[1], TileStep / Forge.Subdiv, 1.e-6));

	// Faces vers le haut : meme sens de face que la forge, sinon l'anneau serait invisible
	// d'en haut (culling) et visible d'en dessous.
	int32 Down = 0;
	for (int32 T = 0; T < G.Triangles.Num(); T += 3)
	{
		const FVector& A = G.Vertices[G.Triangles[T]];
		const FVector& B = G.Vertices[G.Triangles[T + 1]];
		const FVector& C = G.Vertices[G.Triangles[T + 2]];
		Down += FVector::CrossProduct(C - A, B - A).Z <= 0.0 ? 1 : 0;
	}
	TestEqual(TEXT("aucune face tournee vers le bas"), Down, 0);

	AddInfo(FString::Printf(TEXT("HORIZON_SEAM perimeter=%d rings=%d vertices=%d triangles=%d pos_gap=%.3f normal_gap=%.6f water_gap=%.3f edge_water=%d wet_quads=%d outer_m=%.0f skirt_m=%.0f z=[%.0f,%.0f]"),
		P, Ring.Rings, G.Vertices.Num(), G.Triangles.Num() / 3, PosGap, NormalGap, WaterGap, EdgeWater, WetQuads,
		Ring.Distances[Ring.Rings - 2] / 100.0, Ring.Distances.Last() / 100.0, Ring.MinZ, Ring.MaxZ));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainHorizonClosed, "Anastasis.Terrain.Horizon.Closed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainHorizonClosed::RunTest(const FString&)
{
	AnastasisTerrainForge::FMesh Forge;
	AnastasisTerrainHorizon::FRing Ring;
	if (!TestTrue(TEXT("forge"), ForgeCanonical(Forge)) || !TestTrue(TEXT("anneau"), AnastasisTerrainHorizon::Build(Forge, 12345, Ring)))
	{
		return false;
	}

	// Le vide noir est ce que voit un rayon horizontal qui ne rencontre rien. L'horizon est
	// ferme depuis un oeil si, dans CHAQUE direction, une partie de l'anneau depasse la
	// hauteur de l'oeil. Pire cas : le sommet le plus haut de toute la carte.
	FVector Highest = Forge.Geometry.Vertices[0];
	for (const FVector& V : Forge.Geometry.Vertices)
	{
		if (V.Z > Highest.Z)
		{
			Highest = V;
		}
	}
	struct FEye { const TCHAR* Name; FVector At; };
	const FEye Eyes[] = {
		{TEXT("bassin"), FVector(Forge.BasinX, Forge.BasinY, Forge.BasinZ + 170.0)},
		{TEXT("point_haut"), FVector(Forge.LandmarkX, Forge.LandmarkY, Forge.LandmarkZ + 200.0)},
		{TEXT("sommet_max"), Highest + FVector(0.0, 0.0, 200.0)},
	};
	// Mesure EXACTE sur la surface, pas sur les sommets : les anneaux lointains sont decimes
	// (95 colonnes a 20 km, une tous les ~5 degres vus de la carte) et une direction peut
	// passer entre deux sommets. Pour chaque direction, on coupe chaque arete de triangle
	// par le demi-plan vertical de cette direction : le point de coupe est sur la surface.
	constexpr int32 Bins = 720;
	const auto& G = Ring.Geometry;
	for (const FEye& Eye : Eyes)
	{
		TArray<double> Best;
		Best.Init(-90.0, Bins);
		for (int32 T = 0; T < G.Triangles.Num(); T += 3)
		{
			if (G.Triangles[T] < Ring.Perimeter && G.Triangles[T + 1] < Ring.Perimeter && G.Triangles[T + 2] < Ring.Perimeter)
			{
				continue;
			}
			for (int32 E = 0; E < 3; ++E)
			{
				const FVector A = G.Vertices[G.Triangles[T + E]] - Eye.At;
				const FVector B = G.Vertices[G.Triangles[T + (E + 1) % 3]] - Eye.At;
				const double AzA = FMath::RadiansToDegrees(FMath::Atan2(A.Y, A.X));
				double Span = FMath::RadiansToDegrees(FMath::Atan2(B.Y, B.X)) - AzA;
				Span = FMath::UnwindDegrees(Span);
				const double Lo = FMath::Min(AzA, AzA + Span), Hi = FMath::Max(AzA, AzA + Span);
				for (int32 Step = FMath::CeilToInt32(Lo * 2.0); Step <= FMath::FloorToInt32(Hi * 2.0); ++Step)
				{
					const double Theta = FMath::DegreesToRadians(Step * 0.5);
					const FVector2D U(FMath::Cos(Theta), FMath::Sin(Theta));
					const double Den = U.X * (B.Y - A.Y) - U.Y * (B.X - A.X);
					if (FMath::Abs(Den) < 1.e-9)
					{
						continue;
					}
					const double Tp = FMath::Clamp(-(U.X * A.Y - U.Y * A.X) / Den, 0.0, 1.0);
					const FVector Q = A + (B - A) * Tp;
					const double Flat = FVector2D(Q.X, Q.Y).Size();
					if (FVector2D::DotProduct(U, FVector2D(Q.X, Q.Y)) <= 0.0 || Flat <= 0.0)
					{
						continue;
					}
					const int32 Bin = ((Step % Bins) + Bins) % Bins;
					Best[Bin] = FMath::Max(Best[Bin], FMath::RadiansToDegrees(FMath::Atan2(Q.Z, Flat)));
				}
			}
		}
		double Worst = 90.0;
		int32 WorstBin = 0;
		for (int32 B = 0; B < Bins; ++B)
		{
			if (Best[B] < Worst)
			{
				Worst = Best[B];
				WorstBin = B;
			}
		}
		TestTrue(FString::Printf(TEXT("horizon ferme depuis %s (pire direction %.1f deg : %.2f deg)"), Eye.Name, WorstBin * 0.5, Worst), Worst > 0.0);
		AddInfo(FString::Printf(TEXT("HORIZON_CLOSED eye=%s at=(%.0f,%.0f,%.0f) worst_azimuth=%.1f worst_elevation_deg=%.3f"),
			Eye.Name, Eye.At.X, Eye.At.Y, Eye.At.Z, WorstBin * 0.5, Worst));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainHorizonGentle, "Anastasis.Terrain.Horizon.Gentle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainHorizonGentle::RunTest(const FString&)
{
	AnastasisTerrainForge::FMesh Forge;
	AnastasisTerrainHorizon::FRing Ring;
	if (!TestTrue(TEXT("forge"), ForgeCanonical(Forge)) || !TestTrue(TEXT("anneau"), AnastasisTerrainHorizon::Build(Forge, 12345, Ring)))
	{
		return false;
	}
	// L'anneau ne doit pas reintroduire ce que TERRAIN_RELIEF_001 a retire : ni paroi au
	// raccord, ni lame sur la chaine. Pente de chaque triangle de l'anneau.
	//
	// Pres du raccord, l'anneau ne peut pas etre moins raide que le bord forge : un
	// triangle qui contient un segment de bord a 48 degres a au moins 48 degres, et une
	// berge en rampe garde sa pente tant que le lissage est plus court qu'elle. Trois regles :
	//  1. au-dela de SeamFineSteps, rien au-dessus de 45 degres ;
	//  2. en deca, un triangle au-dessus de 45 degres doit prolonger une berge DEJA raide du
	//     bord forge (segment de bord a plus de 40 degres, a 3 colonnes pres) ;
	//  3. et ne pas etre plus raide que le plus raide des triangles forges du bord.
	auto SlopeDeg = [](const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector N = FVector::CrossProduct(C - A, B - A).GetSafeNormal();
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(N.Z, -1.0, 1.0)));
	};
	const auto& G = Ring.Geometry;
	const int32 P = Ring.Perimeter;
	TArray<uint8> SteepEdge;
	SteepEdge.SetNumZeroed(P);
	for (int32 I = 0; I < P; ++I)
	{
		const FVector& A = G.Vertices[I];
		const FVector& B = G.Vertices[(I + 1) % P];
		const double Deg = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(B.Z - A.Z), FVector2D(B.X - A.X, B.Y - A.Y).Size()));
		if (Deg > 40.0)
		{
			for (int32 J = -3; J <= 3; ++J)
			{
				SteepEdge[(I + J + P) % P] = 1;
			}
		}
	}
	const auto& F = Forge.Geometry;
	double ForgeRimMaxDeg = 0.0;
	for (int32 Y = 0; Y < Forge.FineH - 1; ++Y)
	{
		for (int32 X = 0; X < Forge.FineW - 1; ++X)
		{
			if (X > 1 && Y > 1 && X < Forge.FineW - 3 && Y < Forge.FineH - 3)
			{
				continue;
			}
			const int32 A = Y * Forge.FineW + X, B = A + 1, C = A + Forge.FineW, D = C + 1;
			ForgeRimMaxDeg = FMath::Max(ForgeRimMaxDeg, SlopeDeg(F.Vertices[A], F.Vertices[C], F.Vertices[B]));
			ForgeRimMaxDeg = FMath::Max(ForgeRimMaxDeg, SlopeDeg(F.Vertices[B], F.Vertices[C], F.Vertices[D]));
		}
	}
	const double SeamDist = AnastasisTerrainHorizon::SeamFineSteps * AnastasisWorldView::TileWorldSize * Forge.SpatialScale / Forge.Subdiv;
	TArray<double> Slopes;
	int32 SteepBank = 0, SteepElsewhere = 0, SteepBeyond = 0;
	double RingMaxDeg = 0.0;
	for (int32 T = 0; T < G.Triangles.Num(); T += 3)
	{
		const double Deg = SlopeDeg(G.Vertices[G.Triangles[T]], G.Vertices[G.Triangles[T + 1]], G.Vertices[G.Triangles[T + 2]]);
		Slopes.Add(Deg);
		RingMaxDeg = FMath::Max(RingMaxDeg, Deg);
		if (Deg <= 45.0)
		{
			continue;
		}
		const int32 K = Ring.RingOf(G.Triangles[T]), I = Ring.VertexColumn[G.Triangles[T]];
		const bool bSeam = Ring.Distances[K + 1] <= SeamDist + 1.e-6;
		int32& Bucket = !bSeam ? SteepBeyond : (SteepEdge[I] ? SteepBank : SteepElsewhere);
		++Bucket;
		if (&Bucket != &SteepBank && Bucket <= 8)
		{
			AddInfo(FString::Printf(TEXT("HORIZON_STEEP ring=%d dist_m=%.0f col=%d deg=%.1f z=%.0f"),
				K, Ring.Distances[K] / 100.0, I, Deg, G.Vertices[G.Triangles[T]].Z));
		}
	}
	Slopes.Sort();
	const double P99 = Slopes[FMath::Min(Slopes.Num() - 1, static_cast<int32>(Slopes.Num() * 0.99))];
	TestEqual(FString::Printf(TEXT("au-dela de %.0f m du raccord, aucun triangle au-dela de 45 degres"), SeamDist / 100.0), SteepBeyond, 0);
	TestEqual(TEXT("pres du raccord, raide seulement dans le prolongement d'une berge raide du bord"), SteepElsewhere, 0);
	TestTrue(FString::Printf(TEXT("jamais plus raide que la forge au bord (%.1f vs %.1f deg)"), RingMaxDeg, ForgeRimMaxDeg),
		RingMaxDeg <= ForgeRimMaxDeg + 0.5);
	const int32 Steep45 = SteepBank + SteepElsewhere + SteepBeyond;

	// Determinisme : meme graine, meme anneau.
	AnastasisTerrainHorizon::FRing Again;
	AnastasisTerrainHorizon::Build(Forge, 12345, Again);
	TestTrue(TEXT("deterministe"), Again.Geometry.Vertices == G.Vertices);

	AddInfo(FString::Printf(TEXT("HORIZON_GENTLE triangles=%d p99_deg=%.2f max_deg=%.2f steep45=%d bank=%d elsewhere=%d beyond=%d forge_rim_max_deg=%.1f"),
		Slopes.Num(), P99, RingMaxDeg, Steep45, SteepBank, SteepElsewhere, SteepBeyond, ForgeRimMaxDeg));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainHorizonNoSlivers, "Anastasis.Terrain.Horizon.NoSlivers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainHorizonNoSlivers::RunTest(const FString&)
{
	AnastasisTerrainForge::FMesh Forge;
	AnastasisTerrainHorizon::FRing Ring;
	if (!TestTrue(TEXT("forge"), ForgeCanonical(Forge)) || !TestTrue(TEXT("anneau"), AnastasisTerrainHorizon::Build(Forge, 12345, Ring)))
	{
		return false;
	}
	// Premiere version de l'anneau : toutes les colonnes du bord jusqu'a 20 km, pas radial
	// +15 % par anneau -- des aiguilles de 15 m sur 300 m des 2 km, que l'ombrage a facettes
	// rendait en rayures partant de la carte (capture H4). Allongement d'un triangle =
	// plus long cote / hauteur sur ce cote ; 2 pour une demi-cellule carree. Jupe exclue :
	// c'est une bande de 140 km de profondeur que personne ne voit de pres.
	const auto& G = Ring.Geometry;
	const int32 SkirtStart = Ring.RingStart[Ring.Rings - 2];
	TArray<double> Aspect;
	for (int32 T = 0; T < G.Triangles.Num(); T += 3)
	{
		if (G.Triangles[T] >= SkirtStart)
		{
			continue;
		}
		const FVector A = G.Vertices[G.Triangles[T]], B = G.Vertices[G.Triangles[T + 1]], C = G.Vertices[G.Triangles[T + 2]];
		const double AB = FVector::Dist2D(A, B), BC = FVector::Dist2D(B, C), CA = FVector::Dist2D(C, A);
		const double Longest = FMath::Max3(AB, BC, CA);
		const double Area2 = FMath::Abs(FVector2D::CrossProduct(FVector2D(B - A), FVector2D(C - A)));
		Aspect.Add(Area2 > 0.0 ? Longest * Longest / Area2 : 1.e9);
	}
	Aspect.Sort();
	const double P50 = Aspect[Aspect.Num() / 2];
	const double P99 = Aspect[FMath::Min(Aspect.Num() - 1, static_cast<int32>(Aspect.Num() * 0.99))];
	TestTrue(FString::Printf(TEXT("allongement p99 <= 8 (%.1f)"), P99), P99 <= 8.0);
	AddInfo(FString::Printf(TEXT("HORIZON_SLIVERS triangles=%d aspect_p50=%.2f aspect_p99=%.2f aspect_max=%.2f levels_last=%d"),
		Aspect.Num(), P50, P99, Aspect.Last(), Ring.RingLevel.Last()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainHorizonPalette, "Anastasis.Terrain.Horizon.Palette",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainHorizonPalette::RunTest(const FString&)
{
	AnastasisTerrainForge::FMesh Forge;
	AnastasisTerrainHorizon::FRing Ring;
	if (!TestTrue(TEXT("forge"), ForgeCanonical(Forge)) || !TestTrue(TEXT("anneau"), AnastasisTerrainHorizon::Build(Forge, 12345, Ring)))
	{
		return false;
	}
	// HORIZON_BLEND_001 : la premiere teinte lointaine etait la moyenne de toute la terre,
	// sol de foret compris -- un taupe absent de la carte, lu comme un desert autour d'un pays
	// vert (captures realism H3/H4). L'anneau emprunte maintenant aux prairies.
	FLinearColor AllLand(0.f, 0.f, 0.f, 0.f);
	int32 LandCount = 0;
	for (int32 I = 0; I < Forge.Geometry.Vertices.Num(); ++I)
	{
		if (Forge.Geometry.Colors[I].A < 0.5f)
		{
			AllLand += Forge.Geometry.Colors[I];
			++LandCount;
		}
	}
	AllLand /= static_cast<float>(FMath::Max(1, LandCount));

	TestTrue(FString::Printf(TEXT("assez de prairie pour une palette (%d donneurs)"), Ring.PaletteDonors), Ring.PaletteDonors >= 64);
	auto Greenish = [](const FLinearColor& C) { return C.G > C.R && C.G > C.B; };
	TestTrue(TEXT("prairie verte : le vert domine"), Greenish(Ring.LushColor));
	TestTrue(TEXT("prairie seche : le vert domine encore"), Greenish(Ring.DryColor));

	// Au-dela du fondu, chaque sommet est un melange des deux palettes : sa couleur reste dans
	// leur boite, canal par canal.
	const auto& G = Ring.Geometry;
	const double BlendDist = AnastasisTerrainHorizon::BlendTiles * AnastasisWorldView::TileWorldSize * Forge.SpatialScale;
	const FLinearColor Lo(FMath::Min(Ring.LushColor.R, Ring.DryColor.R), FMath::Min(Ring.LushColor.G, Ring.DryColor.G), FMath::Min(Ring.LushColor.B, Ring.DryColor.B));
	const FLinearColor Hi(FMath::Max(Ring.LushColor.R, Ring.DryColor.R), FMath::Max(Ring.LushColor.G, Ring.DryColor.G), FMath::Max(Ring.LushColor.B, Ring.DryColor.B));
	constexpr float Eps = 1.e-4f;
	int32 Far = 0, Outside = 0;
	FLinearColor FarMean(0.f, 0.f, 0.f, 0.f);
	for (int32 V = Ring.Perimeter; V < G.Vertices.Num(); ++V)
	{
		if (Ring.Distances[Ring.RingOf(V)] < BlendDist)
		{
			continue;
		}
		const FLinearColor& C = G.Colors[V];
		++Far;
		FarMean += C;
		Outside += (C.R < Lo.R - Eps || C.R > Hi.R + Eps || C.G < Lo.G - Eps || C.G > Hi.G + Eps || C.B < Lo.B - Eps || C.B > Hi.B + Eps) ? 1 : 0;
	}
	FarMean /= static_cast<float>(FMath::Max(1, Far));
	TestTrue(TEXT("des sommets au-dela du fondu"), Far > 0);
	TestEqual(TEXT("aucune teinte lointaine hors des deux palettes"), Outside, 0);
	TestTrue(TEXT("la teinte lointaine moyenne est verte"), Greenish(FarMean));

	AddInfo(FString::Printf(TEXT("HORIZON_PALETTE donors=%d lush=(%.3f,%.3f,%.3f) dry=(%.3f,%.3f,%.3f) far_mean=(%.3f,%.3f,%.3f) old_all_land=(%.3f,%.3f,%.3f) far_vertices=%d"),
		Ring.PaletteDonors, Ring.LushColor.R, Ring.LushColor.G, Ring.LushColor.B, Ring.DryColor.R, Ring.DryColor.G, Ring.DryColor.B,
		FarMean.R, FarMean.G, FarMean.B, AllLand.R, AllLand.G, AllLand.B, Far));
	return true;
}

#endif
