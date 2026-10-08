#include "WorldView/AnastasisCanonicalGeography.h"

#include "WorldView/AnastasisDrainage.h"
#include "WorldView/AnastasisProjectedMesh.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisWorldView.h"
#include "HAL/PlatformTime.h"

namespace AnastasisCanonicalGeography
{
	/** The recipe. Changing one of these changes the simulated world: it is a decision, not a tweak. */
	constexpr double CanonicalSpatialScale = 5.0;
	constexpr bool bCanonicalHumanGeography = true;
	constexpr bool bCanonicalWaterLook = true;

	FGeography Compute(uint32 Seed)
	{
		const double Began = FPlatformTime::Seconds();
		FGeography Out;
		Out.Seed = Seed;
		AnastasisWorldView::FWorldVisualSnapshot Source = AnastasisWorldView::CaptureCanonicalWorld(Seed);
		Source.SpatialScale = CanonicalSpatialScale;
		Source.bHumanGeography = bCanonicalHumanGeography;
		Out.W = Source.W;
		Out.H = Source.H;
		// The embodiment's whole-world crop and its halo (TerrainSurface mode 2, see AAnastasisWorldEmbodiment).
		const auto Crop = AnastasisWorldView::CropSnapshot(Source, 0, 0, Source.W, Source.H);
		const auto Halo = AnastasisWorldView::CropSnapshot(Source, 0, 0, Source.W, Source.H);
		AnastasisTerrainSurface::FGeometry Geometry;
		if (!AnastasisTerrainSurface::Build(Crop, Geometry)) { Out.Error = TEXT("surface_failed"); return Out; }
		const AnastasisTerrainForge::FSettings Forge = AnastasisTerrainForge::FSettings::Canonical();
		AnastasisTerrainForge::FMesh Mesh;
		if (!AnastasisTerrainForge::Apply(Crop, Geometry, Mesh, Forge, &Halo)) { Out.Error = TEXT("forge_failed"); return Out; }
		AnastasisDrainage::FParams Params;
		Params.bWaterLook = bCanonicalWaterLook;
		Params.ForgeExaggeration = Forge.Exaggerate;
		if (Mesh.bBasinFound || Mesh.bHumanGeography) Params.Protected.Add(FVector2D(Mesh.BasinX, Mesh.BasinY));
		if (Mesh.bLandmarkFound) Params.Protected.Add(FVector2D(Mesh.LandmarkX, Mesh.LandmarkY));
		AnastasisDrainage::FNetwork Network;
		if (!AnastasisDrainage::Apply(Crop, Mesh, Network, Params)) { Out.Error = TEXT("drainage_failed"); return Out; }
		const AnastasisTerrainSurface::FGeometry& Drained = Mesh.Geometry;
		AnastasisDrainage::FWaterRibbons Ribbons;
		AnastasisDrainage::BuildRiverRibbons(Network, Ribbons);

		// What the embodiment draws: ground section 0, lake sheet section 1, ribbons section 2.
		const double Cell = AnastasisWorldView::TileWorldSize * CanonicalSpatialScale;
		AnastasisProjectedMesh::FProjectedMesh Ground, Water;
		Ground.Cell = Water.Cell = Cell;
		Ground.Add(Drained.Vertices, Drained.Triangles);
		const TArray<int32>& Sheet = (Params.bWaterLook && Network.GridW > 0) ? Network.LakeWaterTriangles : Drained.WaterTriangles;
		Water.Add(Drained.WaterVertices, Sheet);
		Water.Add(Ribbons.Vertices, Ribbons.Triangles);

		Out.Water.SetNumZeroed(Out.W * Out.H);
		Out.GroundZ.SetNumZeroed(Out.W * Out.H);
		Out.Slope.Init(90.0f, Out.W * Out.H);
		for (int32 I = 0; I < Out.W * Out.H; ++I)
		{
			const double X = (I % Out.W + 0.5) * Cell, Y = (I / Out.W + 0.5) * Cell;
			double GroundZ = 0.0, WaterZ = 0.0;
			// Same criterion as the settlement survey's concordance: water at or above the ground (1 cm).
			const bool bGround = Ground.Sample(X, Y, GroundZ);
			if (bGround && Water.Sample(X, Y, WaterZ) && WaterZ >= GroundZ - 1.0)
			{
				Out.Water[I] = 1;
				++Out.WaterTiles;
			}
			Out.GroundZ[I] = static_cast<float>(GroundZ);
			// Slope as AnastasisSettlementSurvey::Read measures it on the rendered mesh.
			double H[9] = {};
			bool bComplete = true;
			for (int32 V = -1; V <= 1; ++V) for (int32 U = -1; U <= 1; ++U)
			{
				if (!Ground.Sample(X + U * Cell * 0.4, Y + V * Cell * 0.4, H[(V + 1) * 3 + U + 1])) bComplete = false;
			}
			if (bComplete)
			{
				double DX = 0, DY = 0;
				for (int32 V = 0; V < 3; ++V) for (int32 U = 0; U < 2; ++U) DX = FMath::Max(DX, FMath::Abs(H[V * 3 + U + 1] - H[V * 3 + U]) / (Cell * 0.4));
				for (int32 V = 0; V < 2; ++V) for (int32 U = 0; U < 3; ++U) DY = FMath::Max(DY, FMath::Abs(H[(V + 1) * 3 + U] - H[V * 3 + U]) / (Cell * 0.4));
				Out.Slope[I] = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX * DX + DY * DY))));
			}
		}
		Out.Rivers = Network.Rivers.Num();
		Out.Lakes = Network.Lakes.Num();
		Out.bValid = true;
		Out.Seconds = FPlatformTime::Seconds() - Began;
		return Out;
	}

	const FGeography& Get(uint32 Seed)
	{
		static TMap<uint32, FGeography> Cache;
		if (const FGeography* Found = Cache.Find(Seed)) return *Found;
		return Cache.Add(Seed, Compute(Seed));
	}
}
