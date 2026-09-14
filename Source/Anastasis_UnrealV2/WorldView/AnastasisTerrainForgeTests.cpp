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

#endif
