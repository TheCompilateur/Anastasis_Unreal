#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisWorldView.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainForgeContract, "Anastasis.Terrain.Forge.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainForgeContract::RunTest(const FString&)
{
	const auto Crop = AnastasisWorldView::CaptureCanonicalWorld(12345);
	AnastasisTerrainSurface::FGeometry Raw;
	if (!TestTrue(TEXT("raw build"), AnastasisTerrainSurface::Build(Crop, Raw)))
	{
		return false;
	}
	TestEqual(TEXT("contrat tuile inchange 9216"), Raw.Vertices.Num(), 9216);

	AnastasisTerrainSurface::FGeometry Forged = Raw;
	AnastasisTerrainForge::FMesh Meta;
	if (!TestTrue(TEXT("forge apply"), AnastasisTerrainForge::Apply(Crop, Forged, Meta)))
	{
		return false;
	}

	const int32 ExpectedVerts = AnastasisTerrainForge::FineVerticesFor(96, 96, Meta.Subdiv);
	TestEqual(TEXT("tessellation"), Forged.Vertices.Num(), ExpectedVerts);
	TestTrue(TEXT("plus de faces que la grille tuile"), Forged.Triangles.Num() / 3 > 18050);
	TestEqual(TEXT("une couleur par sommet"), Forged.Colors.Num(), Forged.Vertices.Num());
	TestEqual(TEXT("nappe d'eau meme topologie XY"), Forged.WaterVertices.Num(), Forged.Vertices.Num());

	double WaterZError = 0.0;
	for (const FVector& W : Forged.WaterVertices)
	{
		WaterZError = FMath::Max(WaterZError, FMath::Abs(W.Z - AnastasisTerrainSurface::WaterPlaneZ));
	}
	TestTrue(TEXT("plan d'eau inchange"), WaterZError < 0.01);

	double RawLandMax = AnastasisTerrainSurface::WaterPlaneZ;
	for (int32 I = 0; I < Raw.Vertices.Num(); ++I)
	{
		if (Raw.Colors[I].A < 0.5f)
		{
			RawLandMax = FMath::Max(RawLandMax, static_cast<double>(Raw.Vertices[I].Z));
		}
	}
	TestTrue(TEXT("macro-relief plus haut que la nappe tuilee"), Meta.MaxZ > RawLandMax + 80.0);
	TestTrue(TEXT("bassin habitable identifie"), Meta.bBasinFound);
	TestTrue(TEXT("point haut identifie"), Meta.bLandmarkFound);
	TestTrue(TEXT("bassin et landmark distincts"),
		FVector2D(Meta.BasinX - Meta.LandmarkX, Meta.BasinY - Meta.LandmarkY).Size() > 200.0);

	AnastasisTerrainForge::ClearActive();
	AddInfo(FString::Printf(
		TEXT("TERRAIN_FORGE subdiv=%d verts=%d tris=%d z=[%.1f,%.1f] raw_land_max=%.1f basin=(%.0f,%.0f,%.0f) landmark=(%.0f,%.0f,%.0f)"),
		Meta.Subdiv, Forged.Vertices.Num(), Forged.Triangles.Num() / 3,
		Meta.MinZ, Meta.MaxZ, RawLandMax,
		Meta.BasinX, Meta.BasinY, Meta.BasinZ,
		Meta.LandmarkX, Meta.LandmarkY, Meta.LandmarkZ));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainForgeSample, "Anastasis.Terrain.Forge.SampleHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainForgeSample::RunTest(const FString&)
{
	const auto Crop = AnastasisWorldView::CaptureCanonicalWorld(12345);
	AnastasisTerrainSurface::FGeometry Geometry;
	if (!TestTrue(TEXT("build"), AnastasisTerrainSurface::Build(Crop, Geometry)))
	{
		return false;
	}
	AnastasisTerrainForge::FMesh Meta;
	if (!TestTrue(TEXT("forge"), AnastasisTerrainForge::Apply(Crop, Geometry, Meta)))
	{
		return false;
	}

	double MaxVertexError = 0.0;
	const int32 Step = FMath::Max(1, Meta.FineW / 16);
	for (int32 Y = 0; Y < Meta.FineH; Y += Step)
	{
		for (int32 X = 0; X < Meta.FineW; X += Step)
		{
			const FVector& P = Meta.Geometry.Vertices[Y * Meta.FineW + X];
			double Z = 0.0;
			if (!TestTrue(TEXT("sample sommet"), AnastasisTerrainForge::SampleHeight(Meta, P.X, P.Y, Z)))
			{
				AnastasisTerrainForge::ClearActive();
				return false;
			}
			MaxVertexError = FMath::Max(MaxVertexError, FMath::Abs(Z - P.Z));
		}
	}
	TestTrue(TEXT("sample = sommet forge"), MaxVertexError < 1.e-3);

	AnastasisTerrainForge::FMesh Again;
	AnastasisTerrainSurface::FGeometry Geometry2;
	TestTrue(TEXT("rebuild"), AnastasisTerrainSurface::Build(Crop, Geometry2));
	TestTrue(TEXT("forge 2"), AnastasisTerrainForge::Apply(Crop, Geometry2, Again));
	TestEqual(TEXT("deterministe count"), Again.Geometry.Vertices.Num(), Meta.Geometry.Vertices.Num());
	double MaxDelta = 0.0;
	for (int32 I = 0; I < Meta.Geometry.Vertices.Num(); ++I)
	{
		MaxDelta = FMath::Max(MaxDelta, static_cast<double>(FVector::Dist(Meta.Geometry.Vertices[I], Again.Geometry.Vertices[I])));
	}
	TestTrue(TEXT("deterministe geometrie"), MaxDelta < 1.e-6);

	AnastasisTerrainForge::ClearActive();
	AddInfo(FString::Printf(TEXT("TERRAIN_FORGE_SAMPLE vertex_error=%.9f determinism_delta=%.9f"), MaxVertexError, MaxDelta));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainForgeCarriesMorphology, "Anastasis.Terrain.Forge.CarriesMorphology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainForgeCarriesMorphology::RunTest(const FString&)
{
	// Apply REMPLACE la FGeometry. Tout canal par sommet qu'il oublie de recopier arrive
	// donc vide a la ProceduralMeshComponent, qui le complete a zero sans rien dire -- et
	// le materiau de sol lit alors partout Rock=0 Litter=0 Worked=0 Wetness=0. Plus de
	// roche, plus de litiere, plus de bande humide : le sol redevient une nappe plate,
	// sans erreur, sans journal, sans test rouge. C'est exactement le mode de panne que
	// GROUND_SURFACE_001 a corrige, et ce test est ce qui empeche de l'y reconduire.
	const auto Crop = AnastasisWorldView::CaptureCanonicalWorld(12345);
	AnastasisTerrainSurface::FGeometry Raw;
	if (!TestTrue(TEXT("raw build"), AnastasisTerrainSurface::Build(Crop, Raw))) return false;

	AnastasisTerrainSurface::FGeometry Forged = Raw;
	AnastasisTerrainForge::FMesh Meta;
	if (!TestTrue(TEXT("forge apply"), AnastasisTerrainForge::Apply(Crop, Forged, Meta))) return false;

	// On SORT si la taille est fausse, au lieu de continuer vers l'indexation.
	//
	// La premiere version de ce test se contentait d'un TestEqual puis bouclait sur
	// UV0[I] : le manquement qu'il devait detecter laisse justement ces tableaux VIDES,
	// donc le test lisait hors bornes et faisait tomber l'editeur. La suite passait de
	// 60 tests a 42 et rapportait FAIL=0 -- le garde-fou emportait dix-huit tests avec
	// lui et rendait un vert. Un test qui plante est pire que le bug qu'il surveille.
	if (Forged.UV0.Num() != Forged.Vertices.Num() || Forged.UV1.Num() != Forged.Vertices.Num())
	{
		AddError(FString::Printf(
			TEXT("Apply n'a pas transporte les canaux morphologiques : vertices=%d UV0=%d UV1=%d. ")
			TEXT("La ProceduralMeshComponent completerait a zero et le sol perdrait roche, litiere et humidite."),
			Forged.Vertices.Num(), Forged.UV0.Num(), Forged.UV1.Num()));
		AnastasisTerrainForge::ClearActive();
		return false;
	}

	// Bornes et partition de l'unite preservees par l'interpolation. Un bilineaire sur une
	// partition reste une partition : si ce n'etait plus vrai, le materiau melangerait des
	// poids qui ne somment plus a 1 et l'herbe -- qui est le RESTE -- deviendrait negative.
	int32 Rock = 0, Litter = 0, Worked = 0, Grass = 0, Wet = 0;
	double WorstSum = 0.0;
	for (int32 I = 0; I < Forged.Vertices.Num(); ++I)
	{
		const double R = Forged.UV0[I].X, L = Forged.UV0[I].Y;
		const double W = Forged.UV1[I].X, Wetness = Forged.UV1[I].Y;
		const double G = 1.0 - R - L - W;
		WorstSum = FMath::Max(WorstSum, FMath::Abs(R + L + W + G - 1.0));
		TestTrue(TEXT("poids et humidite bornes"),
			R >= -KINDA_SMALL_NUMBER && L >= -KINDA_SMALL_NUMBER && W >= -KINDA_SMALL_NUMBER
			&& G >= -KINDA_SMALL_NUMBER && G <= 1.0 + KINDA_SMALL_NUMBER
			&& Wetness >= -KINDA_SMALL_NUMBER && Wetness <= 1.0 + KINDA_SMALL_NUMBER);
		if (R > 0.5) ++Rock;
		if (L > 0.5) ++Litter;
		if (W > 0.5) ++Worked;
		if (G > 0.5) ++Grass;
		if (Wetness > 0.5) ++Wet;
	}

	// Non-vacuite : des tableaux de la bonne TAILLE mais remplis de zeros passeraient les
	// controles ci-dessus sans que le sol reponde a quoi que ce soit.
	TestTrue(TEXT("la roche survit a la tessellation"), Rock > 0);
	TestTrue(TEXT("la litiere survit a la tessellation"), Litter > 0);
	TestTrue(TEXT("la terre travaillee survit a la tessellation"), Worked > 0);
	TestTrue(TEXT("l'herbe survit a la tessellation"), Grass > 0);
	TestTrue(TEXT("l'humidite survit a la tessellation"), Wet > 0);

	AnastasisTerrainForge::ClearActive();
	AddInfo(FString::Printf(
		TEXT("TERRAIN_FORGE_MORPHOLOGY vertices=%d rock=%d litter=%d worked=%d grass=%d wet_gt_half=%d partition_error=%.9f"),
		Forged.Vertices.Num(), Rock, Litter, Worked, Grass, Wet, WorstSum));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainForgeChunkSeam, "Anastasis.Terrain.Forge.ChunkSeam",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainForgeChunkSeam::RunTest(const FString&)
{
	// Verite de reference : le monde entier d'un coup. Chaque sommet qui n'est pas sur le
	// bord du MONDE (X/Y = 0 ou 95) y est calcule avec de vrais voisins des deux cotes --
	// aucun bord de CHUNK n'existe ici, donc rien n'y masque l'artefact vise.
	const auto World = AnastasisWorldView::CaptureCanonicalWorld(12345);
	AnastasisTerrainSurface::FGeometry RefGeometry;
	if (!TestTrue(TEXT("ref build"), AnastasisTerrainSurface::Build(World, RefGeometry))) return false;
	AnastasisTerrainForge::FMesh RefMesh;
	TArray<double> RefLap;
	if (!TestTrue(TEXT("ref forge"), AnastasisTerrainForge::Apply(World, RefGeometry, RefMesh, nullptr, &RefLap))) return false;

	// Une emprise loin de tous les bords du monde -- les siens SONT des bords de chunk.
	const int32 OX = 24, OY = 24, W = 20, H = 20;
	const auto Crop = AnastasisWorldView::CropSnapshot(World, OX, OY, W, H);
	if (!TestEqual(TEXT("crop valide"), Crop.Tiles.Num(), W * H)) return false;

	AnastasisTerrainSurface::FGeometry GeoNoHalo;
	if (!TestTrue(TEXT("build sans halo"), AnastasisTerrainSurface::Build(Crop, GeoNoHalo))) return false;
	AnastasisTerrainForge::FMesh MeshNoHalo;
	TArray<double> LapNoHalo;
	if (!TestTrue(TEXT("forge sans halo"), AnastasisTerrainForge::Apply(Crop, GeoNoHalo, MeshNoHalo, nullptr, &LapNoHalo))) return false;

	const int32 M = AnastasisTerrainForge::HaloTiles;
	const auto HaloCrop = AnastasisWorldView::CropSnapshot(World, OX - M, OY - M, W + 2 * M, H + 2 * M);
	if (!TestEqual(TEXT("halo valide"), HaloCrop.Tiles.Num(), (W + 2 * M) * (H + 2 * M))) return false;

	AnastasisTerrainSurface::FGeometry GeoHalo;
	if (!TestTrue(TEXT("build avec halo"), AnastasisTerrainSurface::Build(Crop, GeoHalo))) return false;
	AnastasisTerrainForge::FMesh MeshHalo;
	TArray<double> LapHalo;
	if (!TestTrue(TEXT("forge avec halo"), AnastasisTerrainForge::Apply(Crop, GeoHalo, MeshHalo, &HaloCrop, &LapHalo))) return false;

	if (!TestEqual(TEXT("meme subdiv que la reference"), MeshNoHalo.Subdiv, RefMesh.Subdiv)) return false;
	const int32 Subdiv = MeshNoHalo.Subdiv;

	// Anneau exterieur du maillage fin de Crop. On y compare le LAPLACIEN, pas la hauteur
	// finale : bassin habitable et point haut cherchent leur candidat sur TOUTE l'emprise,
	// donc la hauteur finale diverge legitimement entre un petit crop et le monde entier
	// (mesure separement ci-dessous, a titre informatif). Le Laplacien ne depend que des
	// 4 voisins immediats -- c'est la seule quantite que HaloCrop peut faire correspondre
	// exactement a celle du monde entier, et c'est exactement ce que le bug visait.
	double MaxLapErrorNoHalo = 0.0;
	double MaxLapErrorHalo = 0.0;
	double MaxHeightErrorNoHalo = 0.0;
	double MaxHeightErrorHalo = 0.0;
	int32 Sampled = 0;
	for (int32 JY = 0; JY < MeshNoHalo.FineH; ++JY)
	{
		for (int32 IX = 0; IX < MeshNoHalo.FineW; ++IX)
		{
			const bool bBorder = IX == 0 || JY == 0 || IX == MeshNoHalo.FineW - 1 || JY == MeshNoHalo.FineH - 1;
			if (!bBorder)
			{
				continue;
			}
			const int32 I = JY * MeshNoHalo.FineW + IX;
			const int32 WorldIX = OX * Subdiv + IX;
			const int32 WorldIY = OY * Subdiv + JY;
			if (WorldIX < 0 || WorldIY < 0 || WorldIX >= RefMesh.FineW || WorldIY >= RefMesh.FineH)
			{
				continue; // Hors du monde entier : pas de verite a comparer ici.
			}
			const int32 RefI = WorldIY * RefMesh.FineW + WorldIX;
			MaxLapErrorNoHalo = FMath::Max(MaxLapErrorNoHalo, FMath::Abs(LapNoHalo[I] - RefLap[RefI]));
			MaxLapErrorHalo = FMath::Max(MaxLapErrorHalo, FMath::Abs(LapHalo[I] - RefLap[RefI]));

			const FVector& P = MeshNoHalo.Geometry.Vertices[I];
			double ZRef = 0.0, ZHalo = 0.0;
			if (AnastasisTerrainForge::SampleHeight(RefMesh, P.X, P.Y, ZRef)
				&& AnastasisTerrainForge::SampleHeight(MeshHalo, P.X, P.Y, ZHalo))
			{
				MaxHeightErrorNoHalo = FMath::Max(MaxHeightErrorNoHalo, FMath::Abs(P.Z - ZRef));
				MaxHeightErrorHalo = FMath::Max(MaxHeightErrorHalo, FMath::Abs(ZHalo - ZRef));
			}
			++Sampled;
		}
	}

	TestTrue(TEXT("anneau de bord echantillonne"), Sampled > 0);
	// Le halo ne doit jamais degrader l'accord du Laplacien avec le monde entier.
	TestTrue(TEXT("le halo ne fait jamais pire que le clamp"), MaxLapErrorHalo <= MaxLapErrorNoHalo + KINDA_SMALL_NUMBER);
	// Et il doit coller a cette verite : c'est le contrat de raccord de chunk vise par le fix.
	TestTrue(TEXT("le halo colle au Laplacien du monde entier"), MaxLapErrorHalo < 1.e-6);

	AnastasisTerrainForge::ClearActive();
	AddInfo(FString::Printf(
		TEXT("TERRAIN_FORGE_CHUNK_SEAM border_samples=%d lap_error_no_halo=%.9f lap_error_halo=%.9f ")
		TEXT("height_error_no_halo=%.3f height_error_halo=%.3f (hauteur = info seulement : bassin/point ")
		TEXT("haut cherchent sur toute l'emprise, leur placement differe legitimement du monde entier)"),
		Sampled, MaxLapErrorNoHalo, MaxLapErrorHalo, MaxHeightErrorNoHalo, MaxHeightErrorHalo));
	return true;
}

namespace
{
/** Lecture du relief REELLEMENT rendu : ce que le joueur voit, pas l'altitude de tuile. */
struct FRenderedReliefStats
{
	int32 Samples = 0;
	/** Moyenne de |Z[x-1] - 2 Z[x] + Z[x+1]| (x et y), uu. L'escalier est une rugosite fine. */
	double MeanSecondDiff = 0.0;
	/** Part des sommets de terre rendus au-dela de 60 degres : des parois, pas des pentes. */
	double SteepFrac = 0.0;
	double P99SlopeDeg = 0.0;
	double MaxSlopeDeg = 0.0;
	/** Sommets de terre plus hauts que leurs 8 voisins ET de plus de 50 uu que leur moyenne : des lames. */
	int32 Spikes = 0;
	/**
	 * |Z''| moyen sur les lignes de tuile / |Z''| moyen entre elles. Un bilineaire n'a de
	 * courbure QUE sur les lignes de tuile : le rapport y est grand. Une surface sans pli
	 * de grille tend vers 1.
	 */
	double CreaseRatio = 0.0;
};

FRenderedReliefStats MeasureRenderedRelief(const AnastasisTerrainForge::FMesh& Mesh)
{
	FRenderedReliefStats Out;
	const TArray<FVector>& V = Mesh.Geometry.Vertices;
	const int32 W = Mesh.FineW, H = Mesh.FineH;
	const double Spacing = AnastasisWorldView::TileWorldSize / static_cast<double>(Mesh.Subdiv);
	const double SeaZ = AnastasisTerrainSurface::WaterPlaneZ;
	TArray<double> Slopes;
	double SecondSum = 0.0;
	int32 Steep = 0;
	double OnLine = 0.0, OffLine = 0.0;
	int32 OnCount = 0, OffCount = 0;
	for (int32 Y = 1; Y < H - 1; ++Y)
	{
		for (int32 X = 1; X < W - 1; ++X)
		{
			const int32 I = Y * W + X;
			const double Z = V[I].Z, ZL = V[I - 1].Z, ZR = V[I + 1].Z, ZD = V[I - W].Z, ZU = V[I + W].Z;
			// Terre emergee seulement, voisins compris : la rive et le fond immerge ont leur
			// propre exageration et ne sont pas l'objet de cette mesure.
			if (Z <= SeaZ || ZL <= SeaZ || ZR <= SeaZ || ZD <= SeaZ || ZU <= SeaZ)
			{
				continue;
			}
			const double Dx = (ZR - ZL) / (2.0 * Spacing);
			const double Dy = (ZU - ZD) / (2.0 * Spacing);
			const double Deg = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(Dx * Dx + Dy * Dy)));
			Slopes.Add(Deg);
			Steep += Deg > 60.0 ? 1 : 0;
			const double D2X = FMath::Abs(ZL - 2.0 * Z + ZR);
			const double D2Y = FMath::Abs(ZD - 2.0 * Z + ZU);
			SecondSum += 0.5 * (D2X + D2Y);
			// Courbure le long de X : sur une ligne de tuile quand X tombe sur un noeud grossier.
			(X % Mesh.Subdiv == 0 ? OnLine : OffLine) += D2X;
			++(X % Mesh.Subdiv == 0 ? OnCount : OffCount);
			(Y % Mesh.Subdiv == 0 ? OnLine : OffLine) += D2Y;
			++(Y % Mesh.Subdiv == 0 ? OnCount : OffCount);

			double NeighbourSum = 0.0;
			bool bStrictMax = true;
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					if (DX == 0 && DY == 0) continue;
					const double ZN = V[I + DY * W + DX].Z;
					NeighbourSum += ZN;
					bStrictMax &= ZN < Z;
				}
			}
			Out.Spikes += (bStrictMax && Z - NeighbourSum / 8.0 > 50.0) ? 1 : 0;
		}
	}
	Out.CreaseRatio = (OnCount > 0 && OffCount > 0 && OffLine > 0.0)
		? (OnLine / OnCount) / (OffLine / OffCount)
		: 0.0;
	Out.Samples = Slopes.Num();
	if (Out.Samples == 0)
	{
		return Out;
	}
	Slopes.Sort();
	Out.MeanSecondDiff = SecondSum / Out.Samples;
	Out.SteepFrac = static_cast<double>(Steep) / Out.Samples;
	Out.P99SlopeDeg = Slopes[FMath::Min(Out.Samples - 1, static_cast<int32>(Out.Samples * 0.99))];
	Out.MaxSlopeDeg = Slopes.Last();
	return Out;
}

/** Forge Crop avec certaines CVars de la forge forcees, puis les restaure. Les autres gardent leur defaut. */
bool ForgeWith(std::initializer_list<TPair<const TCHAR*, int32>> Vars, const AnastasisWorldView::FWorldVisualSnapshot& Crop, AnastasisTerrainForge::FMesh& OutMesh)
{
	TArray<TPair<IConsoleVariable*, int32>> Previous;
	bool bOk = true;
	for (const TPair<const TCHAR*, int32>& Var : Vars)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Var.Key);
		if (!CVar)
		{
			bOk = false;
			break;
		}
		Previous.Emplace(CVar, CVar->GetInt());
		CVar->Set(Var.Value, ECVF_SetByCode);
	}
	AnastasisTerrainSurface::FGeometry Geometry;
	bOk = bOk && AnastasisTerrainSurface::Build(Crop, Geometry) && AnastasisTerrainForge::Apply(Crop, Geometry, OutMesh);
	for (const TPair<IConsoleVariable*, int32>& Var : Previous)
	{
		Var.Key->Set(Var.Value, ECVF_SetByCode);
	}
	return bOk;
}

const TCHAR* const TerracesVar = TEXT("anastasis.Terrain.Forge.Terraces");
const TCHAR* const EscarpmentsVar = TEXT("anastasis.Terrain.Forge.Escarpments");
const TCHAR* const BicubicVar = TEXT("anastasis.Terrain.Forge.Bicubic");
const TCHAR* const SharpenVar = TEXT("anastasis.Terrain.Forge.Sharpen");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainForgeNoStaircase, "Anastasis.Terrain.Forge.NoStaircase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainForgeNoStaircase::RunTest(const FString&)
{
	// TERRAIN_RELIEF_001, etape 1. Les terrasses quantifiaient l'altitude par paliers de
	// 0.016 sur des pentes jugees AVANT exageration, donc rendues jusqu'a ~58 degres : un
	// escalier. L'escarpement redressait ce qui etait deja rendu au-dela de 50 degres.
	// Les deux sont coupes par defaut ; ce test mesure la difference sur le maillage rendu,
	// contre la forge d'origine rebatie dans le meme processus.
	IConsoleVariable* TerraceVar = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Terrain.Forge.Terraces"));
	IConsoleVariable* EscarpVar = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Terrain.Forge.Escarpments"));
	if (!TestNotNull(TEXT("cvar terrasses"), TerraceVar) || !TestNotNull(TEXT("cvar escarpements"), EscarpVar))
	{
		return false;
	}
	TestEqual(TEXT("terrasses coupees par defaut"), TerraceVar->GetInt(), 0);
	TestEqual(TEXT("escarpements coupes par defaut"), EscarpVar->GetInt(), 0);

	const auto Crop = AnastasisWorldView::CaptureCanonicalWorld(12345);
	AnastasisTerrainForge::FMesh Legacy, Relief;
	// Bicubic/Sharpen epingles a leur valeur de l'etape 1 : ce test isole les terrasses et
	// l'escarpement, et ses chiffres doivent rester ceux que l'etape 1 a publies.
	if (!TestTrue(TEXT("forge d'origine"), ForgeWith({{BicubicVar, 0}, {SharpenVar, 1}, {TerracesVar, 1}, {EscarpmentsVar, 1}}, Crop, Legacy))
		|| !TestTrue(TEXT("forge sans escalier"), ForgeWith({{BicubicVar, 0}, {SharpenVar, 1}, {TerracesVar, 0}, {EscarpmentsVar, 0}}, Crop, Relief)))
	{
		AnastasisTerrainForge::ClearActive();
		return false;
	}
	const FRenderedReliefStats Before = MeasureRenderedRelief(Legacy);
	const FRenderedReliefStats After = MeasureRenderedRelief(Relief);
	TestTrue(TEXT("terre mesuree"), Before.Samples > 1000 && After.Samples > 1000);

	TestTrue(TEXT("le relief rendu est moins rugueux sans terrasses ni escarpements"),
		After.MeanSecondDiff < Before.MeanSecondDiff);
	TestTrue(TEXT("moins de parois au-dela de 60 degres"), After.SteepFrac <= Before.SteepFrac);
	TestTrue(TEXT("p99 de pente pas plus raide"), After.P99SlopeDeg <= Before.P99SlopeDeg + KINDA_SMALL_NUMBER);

	AnastasisTerrainForge::ClearActive();
	AddInfo(FString::Printf(
		TEXT("TERRAIN_RELIEF_STAIRCASE before: samples=%d second_diff_uu=%.2f steep60=%.4f p99_deg=%.1f max_deg=%.1f | ")
		TEXT("after: samples=%d second_diff_uu=%.2f steep60=%.4f p99_deg=%.1f max_deg=%.1f"),
		Before.Samples, Before.MeanSecondDiff, Before.SteepFrac, Before.P99SlopeDeg, Before.MaxSlopeDeg,
		After.Samples, After.MeanSecondDiff, After.SteepFrac, After.P99SlopeDeg, After.MaxSlopeDeg));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainForgeNoSpikes, "Anastasis.Terrain.Forge.NoSpikes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainForgeNoSpikes::RunTest(const FString&)
{
	// TERRAIN_RELIEF_001, etape 2. Deux causes, deux mesures :
	//  - le bilineaire plie le maillage sur chaque ligne de tuile     -> CreaseRatio
	//  - l'amplification du Laplacien leve des lames au bord des chenaux -> Spikes
	// Reference : la forge de l'etape 1 (bilineaire + amplification), meme processus.
	IConsoleVariable* Bicubic = IConsoleManager::Get().FindConsoleVariable(BicubicVar);
	IConsoleVariable* Sharpen = IConsoleManager::Get().FindConsoleVariable(SharpenVar);
	if (!TestNotNull(TEXT("cvar bicubique"), Bicubic) || !TestNotNull(TEXT("cvar affutage"), Sharpen))
	{
		return false;
	}
	TestEqual(TEXT("bicubique par defaut"), Bicubic->GetInt(), 1);
	TestEqual(TEXT("affutage coupe par defaut"), Sharpen->GetInt(), 0);

	const auto Crop = AnastasisWorldView::CaptureCanonicalWorld(12345);
	AnastasisTerrainForge::FMesh Legacy, Smooth;
	if (!TestTrue(TEXT("forge etape 1"), ForgeWith({{BicubicVar, 0}, {SharpenVar, 1}}, Crop, Legacy))
		|| !TestTrue(TEXT("forge etape 2"), ForgeWith({{BicubicVar, 1}, {SharpenVar, 0}}, Crop, Smooth)))
	{
		AnastasisTerrainForge::ClearActive();
		return false;
	}
	const FRenderedReliefStats Before = MeasureRenderedRelief(Legacy);
	const FRenderedReliefStats After = MeasureRenderedRelief(Smooth);
	TestTrue(TEXT("terre mesuree"), Before.Samples > 1000 && After.Samples > 1000);

	TestTrue(TEXT("moins de lames"), After.Spikes < Before.Spikes);
	TestTrue(TEXT("le pli de grille recule"), After.CreaseRatio < Before.CreaseRatio);
	TestTrue(TEXT("moins rugueux"), After.MeanSecondDiff < Before.MeanSecondDiff);

	AnastasisTerrainForge::ClearActive();
	AddInfo(FString::Printf(
		TEXT("TERRAIN_RELIEF_SPIKES before: spikes=%d crease=%.2f second_diff_uu=%.2f steep60=%.4f p99_deg=%.1f | ")
		TEXT("after: spikes=%d crease=%.2f second_diff_uu=%.2f steep60=%.4f p99_deg=%.1f"),
		Before.Spikes, Before.CreaseRatio, Before.MeanSecondDiff, Before.SteepFrac, Before.P99SlopeDeg,
		After.Spikes, After.CreaseRatio, After.MeanSecondDiff, After.SteepFrac, After.P99SlopeDeg));
	return true;
}

#endif
