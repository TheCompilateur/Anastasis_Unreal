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

#endif
