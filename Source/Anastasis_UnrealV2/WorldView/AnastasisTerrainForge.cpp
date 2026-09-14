#include "WorldView/AnastasisTerrainForge.h"

#include "HAL/IConsoleManager.h"
#include "Math/NumericLimits.h"
#include "World/AnastasisWorldNoise.h"

static TAutoConsoleVariable<int32> CVarForgeSubdiv(
	TEXT("anastasis.Terrain.Forge.Subdiv"),
	AnastasisTerrainForge::DefaultSubdiv,
	TEXT("Tessellation factor of TERRAIN_FORGE. 2-6. Applied on embodiment."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarForgeExaggerate(
	TEXT("anastasis.Terrain.Forge.Exaggerate"),
	3.6f,
	TEXT("Vertical exaggeration of land above sea. Water plane stays at SeaLevel."),
	ECVF_Default);

namespace
{
using AnastasisWorldView::FVisualTile;
using AnastasisWorldView::FWorldVisualSnapshot;
using AnastasisWorld::ETileType;

AnastasisTerrainForge::FMesh GActive;
bool GActiveValid = false;

double SmoothStep(double Edge0, double Edge1, double X)
{
	const double T = FMath::Clamp((X - Edge0) / FMath::Max(Edge1 - Edge0, 1.e-8), 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T);
}

const FVisualTile& CoarseTile(const FWorldVisualSnapshot& Crop, int32 X, int32 Y)
{
	return Crop.Tiles[Y * Crop.W + X];
}

void BilinearSample(
	const FWorldVisualSnapshot& Crop,
	double U, double V,
	double& Alt, double& Shore, double& Wetness,
	bool& bWater)
{
	const int32 W = Crop.W, H = Crop.H;
	const int32 X = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(U)), 0, W - 2);
	const int32 Y = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(V)), 0, H - 2);
	const double Fx = FMath::Clamp(U - static_cast<double>(X), 0.0, 1.0);
	const double Fy = FMath::Clamp(V - static_cast<double>(Y), 0.0, 1.0);
	const int32 A = Y * W + X, B = A + 1, C = A + W, D = C + 1;
	const auto& TA = Crop.Tiles[A];
	const auto& TB = Crop.Tiles[B];
	const auto& TC = Crop.Tiles[C];
	const auto& TD = Crop.Tiles[D];
	const double W00 = (1.0 - Fx) * (1.0 - Fy);
	const double W10 = Fx * (1.0 - Fy);
	const double W01 = (1.0 - Fx) * Fy;
	const double W11 = Fx * Fy;
	Alt = TA.Alt * W00 + TB.Alt * W10 + TC.Alt * W01 + TD.Alt * W11;
	Shore = TA.Shore * W00 + TB.Shore * W10 + TC.Shore * W01 + TD.Shore * W11;
	Wetness = TA.Wetness * W00 + TB.Wetness * W10 + TC.Wetness * W01 + TD.Wetness * W11;
	const int32 NX = FMath::Clamp(FMath::RoundToInt(U), 0, W - 1);
	const int32 NY = FMath::Clamp(FMath::RoundToInt(V), 0, H - 1);
	bWater = CoarseTile(Crop, NX, NY).Type == ETileType::Water;
}

ETileType NearestType(const FWorldVisualSnapshot& Crop, double U, double V)
{
	const int32 NX = FMath::Clamp(FMath::RoundToInt(U), 0, Crop.W - 1);
	const int32 NY = FMath::Clamp(FMath::RoundToInt(V), 0, Crop.H - 1);
	return CoarseTile(Crop, NX, NY).Type;
}

FLinearColor BilinearColor(const AnastasisTerrainSurface::FGeometry& Coarse, int32 CoarseW, int32 CoarseH, double U, double V)
{
	const int32 X = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(U)), 0, CoarseW - 2);
	const int32 Y = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(V)), 0, CoarseH - 2);
	const float Fx = static_cast<float>(FMath::Clamp(U - static_cast<double>(X), 0.0, 1.0));
	const float Fy = static_cast<float>(FMath::Clamp(V - static_cast<double>(Y), 0.0, 1.0));
	const int32 A = Y * CoarseW + X, B = A + 1, C = A + CoarseW, D = C + 1;
	const FLinearColor CA = Coarse.Colors[A];
	const FLinearColor CB = Coarse.Colors[B];
	const FLinearColor CC = Coarse.Colors[C];
	const FLinearColor CD = Coarse.Colors[D];
	return FMath::Lerp(FMath::Lerp(CA, CB, Fx), FMath::Lerp(CC, CD, Fx), Fy);
}

void RebuildNormals(AnastasisTerrainSurface::FGeometry& G)
{
	G.Normals.SetNum(G.Vertices.Num());
	for (FVector& N : G.Normals)
	{
		N = FVector::ZeroVector;
	}
	for (int32 I = 0; I < G.Triangles.Num(); I += 3)
	{
		const int32 A = G.Triangles[I], B = G.Triangles[I + 1], C = G.Triangles[I + 2];
		const FVector N = FVector::CrossProduct(G.Vertices[C] - G.Vertices[A], G.Vertices[B] - G.Vertices[A]);
		G.Normals[A] += N;
		G.Normals[B] += N;
		G.Normals[C] += N;
	}
	for (FVector& N : G.Normals)
	{
		N = N.GetSafeNormal();
	}
}
}

void AnastasisTerrainForge::SetActive(const FMesh& Mesh)
{
	GActive = Mesh;
	GActiveValid = Mesh.FineW > 1 && Mesh.FineH > 1 && Mesh.Geometry.Vertices.Num() == Mesh.FineW * Mesh.FineH;
}

void AnastasisTerrainForge::ClearActive()
{
	GActive = FMesh{};
	GActiveValid = false;
}

bool AnastasisTerrainForge::SampleActive(double WorldX, double WorldY, double& OutZ)
{
	if (!GActiveValid)
	{
		return false;
	}
	return SampleHeight(GActive, WorldX, WorldY, OutZ);
}

bool AnastasisTerrainForge::SampleHeight(const FMesh& Mesh, double WorldX, double WorldY, double& OutZ)
{
	OutZ = 0.0;
	if (Mesh.FineW < 2 || Mesh.FineH < 2 || Mesh.Subdiv < 1)
	{
		return false;
	}
	if (Mesh.Geometry.Vertices.Num() != Mesh.FineW * Mesh.FineH)
	{
		return false;
	}
	if (!FMath::IsFinite(WorldX) || !FMath::IsFinite(WorldY))
	{
		return false;
	}
	const double U = WorldX / AnastasisWorldView::TileWorldSize - 0.5 - static_cast<double>(Mesh.OriginX);
	const double V = WorldY / AnastasisWorldView::TileWorldSize - 0.5 - static_cast<double>(Mesh.OriginY);
	const double FineU = U * static_cast<double>(Mesh.Subdiv);
	const double FineV = V * static_cast<double>(Mesh.Subdiv);
	if (FineU < 0.0 || FineV < 0.0 || FineU > static_cast<double>(Mesh.FineW - 1) || FineV > static_cast<double>(Mesh.FineH - 1))
	{
		return false;
	}
	const int32 X = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(FineU)), 0, Mesh.FineW - 2);
	const int32 Y = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(FineV)), 0, Mesh.FineH - 2);
	const double Fx = FineU - static_cast<double>(X);
	const double Fy = FineV - static_cast<double>(Y);
	const int32 A = Y * Mesh.FineW + X, B = A + 1, C = A + Mesh.FineW, D = C + 1;
	const double ZA = Mesh.Geometry.Vertices[A].Z;
	const double ZB = Mesh.Geometry.Vertices[B].Z;
	const double ZC = Mesh.Geometry.Vertices[C].Z;
	const double ZD = Mesh.Geometry.Vertices[D].Z;
	OutZ = (Fx + Fy <= 1.0)
		? ZA + Fx * (ZB - ZA) + Fy * (ZC - ZA)
		: ZD + (1.0 - Fx) * (ZC - ZD) + (1.0 - Fy) * (ZB - ZD);
	return FMath::IsFinite(OutZ);
}

bool AnastasisTerrainForge::Apply(
	const FWorldVisualSnapshot& Crop,
	AnastasisTerrainSurface::FGeometry& InOut,
	FMesh& OutMeta)
{
	OutMeta = FMesh{};
	const int32 CoarseW = Crop.W;
	const int32 CoarseH = Crop.H;
	if (CoarseW < 2 || CoarseH < 2 || InOut.Vertices.Num() != CoarseW * CoarseH)
	{
		return false;
	}

	const int32 Subdiv = FMath::Clamp(CVarForgeSubdiv.GetValueOnGameThread(), 2, 6);
	const double Exaggerate = FMath::Clamp(static_cast<double>(CVarForgeExaggerate.GetValueOnGameThread()), 1.0, 8.0);
	const int32 FineW = (CoarseW - 1) * Subdiv + 1;
	const int32 FineH = (CoarseH - 1) * Subdiv + 1;
	const int32 FineN = FineW * FineH;
	const double Sea = AnastasisWorld::SeaLevel;
	const double SeaZ = AnastasisTerrainSurface::WaterPlaneZ;
	const double InvSub = 1.0 / static_cast<double>(Subdiv);
	const uint32 Seed = Crop.Seed;

	TArray<double> H;
	TArray<double> Shore;
	TArray<double> Wet;
	TArray<uint8> Water;
	TArray<uint8> Buildable;
	H.SetNumUninitialized(FineN);
	Shore.SetNumUninitialized(FineN);
	Wet.SetNumUninitialized(FineN);
	Water.SetNumUninitialized(FineN);
	Buildable.SetNumZeroed(FineN);

	for (int32 JY = 0; JY < FineH; ++JY)
	{
		for (int32 IX = 0; IX < FineW; ++IX)
		{
			const int32 I = JY * FineW + IX;
			const double U = static_cast<double>(IX) * InvSub;
			const double V = static_cast<double>(JY) * InvSub;
			double Alt = 0.0, Sh = 0.0, We = 0.0;
			bool bWater = false;
			BilinearSample(Crop, U, V, Alt, Sh, We, bWater);
			H[I] = Alt;
			Shore[I] = Sh;
			Wet[I] = We;
			Water[I] = bWater ? 1 : 0;
			const ETileType Type = NearestType(Crop, U, V);
			Buildable[I] = (!bWater && (Type == ETileType::Grass || Type == ETileType::Field || Type == ETileType::Scrub)) ? 1 : 0;
		}
	}

	auto At = [FineW, FineH, FineN](const TArray<double>& G, int32 X, int32 Y) -> double
	{
		X = FMath::Clamp(X, 0, FineW - 1);
		Y = FMath::Clamp(Y, 0, FineH - 1);
		return G[Y * FineW + X];
	};

	TArray<double> Slope;
	TArray<double> Lap;
	Slope.SetNumUninitialized(FineN);
	Lap.SetNumUninitialized(FineN);
	for (int32 JY = 0; JY < FineH; ++JY)
	{
		for (int32 IX = 0; IX < FineW; ++IX)
		{
			const int32 I = JY * FineW + IX;
			const double Dx = At(H, IX + 1, JY) - At(H, IX - 1, JY);
			const double Dy = At(H, IX, JY + 1) - At(H, IX, JY - 1);
			Slope[I] = FMath::Sqrt(Dx * Dx + Dy * Dy) * 0.5 * static_cast<double>(Subdiv);
			Lap[I] = 4.0 * H[I] - At(H, IX - 1, JY) - At(H, IX + 1, JY) - At(H, IX, JY - 1) - At(H, IX, JY + 1);
		}
	}

	// D8 accumulation on the fine height field — drainage for ravine carving, not hydrology color.
	TArray<int32> Order;
	Order.SetNumUninitialized(FineN);
	for (int32 I = 0; I < FineN; ++I)
	{
		Order[I] = I;
	}
	Order.Sort([&H](int32 A, int32 B) { return H[A] > H[B]; });
	TArray<float> Accum;
	Accum.Init(1.0f, FineN);
	const int32 OffX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
	const int32 OffY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
	for (int32 I : Order)
	{
		const int32 X = I % FineW;
		const int32 Y = I / FineW;
		int32 Best = INDEX_NONE;
		double BestH = H[I];
		for (int32 K = 0; K < 8; ++K)
		{
			const int32 NX = X + OffX[K];
			const int32 NY = Y + OffY[K];
			if (NX < 0 || NY < 0 || NX >= FineW || NY >= FineH)
			{
				continue;
			}
			const int32 NI = NY * FineW + NX;
			if (H[NI] < BestH)
			{
				BestH = H[NI];
				Best = NI;
			}
		}
		if (Best != INDEX_NONE)
		{
			Accum[Best] += Accum[I];
		}
	}

	TArray<double> Forged = H;
	double BestBasin = -1.0;
	int32 BasinI = INDEX_NONE;
	double BestLandmark = -1.0;
	int32 LandmarkI = INDEX_NONE;

	for (int32 JY = 0; JY < FineH; ++JY)
	{
		for (int32 IX = 0; IX < FineW; ++IX)
		{
			const int32 I = JY * FineW + IX;
			if (Water[I])
			{
				continue;
			}
			double Alt = H[I];
			const double S = Slope[I];
			const double L = Lap[I];
			const double Sh = Shore[I];
			const double We = Wet[I];
			const double Acc = static_cast<double>(Accum[I]);

			// Macro: ridges rise, bowls drop — existing masses, not new continents.
			Alt += FMath::Max(L, 0.0) * 1.35;
			Alt += FMath::Min(L, 0.0) * 0.95;

			// Ravines follow drainage. Keep them off the wet coastal shelf.
			const double Ravine = SmoothStep(8.0, 40.0, Acc) * (1.0 - SmoothStep(0.35, 0.75, Sh));
			Alt -= Ravine * 0.016 * (0.45 + S * 2.0);

			// Terraces on usable hillsides — agricultural benches, not a staircase everywhere.
			const double SlopeDeg = FMath::RadiansToDegrees(FMath::Atan(S * AnastasisWorldView::AltitudeScale / AnastasisWorldView::TileWorldSize));
			if (SlopeDeg > 6.0 && SlopeDeg < 24.0 && Sh < 0.5)
			{
				const double Step = 0.016;
				const double Rel = (Alt - Sea) / Step;
				const double Terr = Sea + FMath::Floor(Rel + 0.5) * Step;
				const double Blend = SmoothStep(6.0, 9.0, SlopeDeg) * (1.0 - SmoothStep(20.0, 24.0, SlopeDeg)) * 0.62;
				Alt = FMath::Lerp(Alt, Terr, Blend);
			}

			// Escarpments: steepen already-steep convex breaks.
			if (SlopeDeg > 18.0 && L > 0.0)
			{
				Alt += SmoothStep(18.0, 28.0, SlopeDeg) * 0.012 * L * 8.0;
			}

			// Meso roughness only on slopes, never as a uniform blanket.
			if (S > 0.004)
			{
				const double Wx = Crop.OriginX + static_cast<double>(IX) * InvSub;
				const double Wy = Crop.OriginY + static_cast<double>(JY) * InvSub;
				const double N1 = AnastasisWorldNoise::Fbm(Wx / 3.4, Wy / 3.4, static_cast<double>(Seed) + 701.0);
				const double N2 = AnastasisWorldNoise::Fbm(Wx / 1.6, Wy / 1.6, static_cast<double>(Seed) + 909.0);
				Alt += (N1 - 0.5) * 0.010 * SmoothStep(0.004, 0.02, S);
				Alt += (N2 - 0.5) * 0.004 * SmoothStep(0.01, 0.04, S);
			}

			Forged[I] = Alt;

			const double Habit = static_cast<double>(Buildable[I])
				* (1.0 - SmoothStep(0.008, 0.028, S))
				* SmoothStep(Sea + 0.008, Sea + 0.035, Alt)
				* (1.0 - SmoothStep(Sea + 0.14, Sea + 0.22, Alt))
				* FMath::Max(Sh, We * 0.65);
			if (Habit > BestBasin)
			{
				BestBasin = Habit;
				BasinI = I;
			}
			const double EdgeDist = static_cast<double>(FMath::Min(FMath::Min(IX, JY), FMath::Min(FineW - 1 - IX, FineH - 1 - JY)));
			if (Alt > BestLandmark && Sh < 0.25 && EdgeDist > static_cast<double>(Subdiv) * 12.0)
			{
				BestLandmark = Alt;
				LandmarkI = I;
			}
		}
	}

	// Habitable basin: flatten a compact neighbourhood of the best site, then terrace its rim.
	TArray<uint8> BasinMask;
	BasinMask.SetNumZeroed(FineN);
	if (BasinI != INDEX_NONE && BestBasin > 0.02)
	{
		TArray<int32> Queue;
		Queue.Add(BasinI);
		BasinMask[BasinI] = 1;
		double Sum = Forged[BasinI];
		int32 Count = 1;
		const int32 RadiusCells = FMath::Max(6, Subdiv * 3);
		for (int32 Q = 0; Q < Queue.Num(); ++Q)
		{
			const int32 I = Queue[Q];
			const int32 X = I % FineW;
			const int32 Y = I / FineW;
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 NX = X + OffX[K];
				const int32 NY = Y + OffY[K];
				if (NX < 0 || NY < 0 || NX >= FineW || NY >= FineH)
				{
					continue;
				}
				const int32 NI = NY * FineW + NX;
				if (BasinMask[NI] || Water[NI] || !Buildable[NI])
				{
					continue;
				}
				const int32 DX = (NX - (BasinI % FineW));
				const int32 DY = (NY - (BasinI / FineW));
				if (DX * DX + DY * DY > RadiusCells * RadiusCells)
				{
					continue;
				}
				if (Slope[NI] > 0.032)
				{
					continue;
				}
				BasinMask[NI] = 1;
				Queue.Add(NI);
				Sum += Forged[NI];
				++Count;
			}
		}
		const double Target = Sum / static_cast<double>(Count);
		for (int32 I = 0; I < FineN; ++I)
		{
			if (!BasinMask[I])
			{
				continue;
			}
			Forged[I] = FMath::Lerp(Forged[I], Target, 0.58);
		}
		// Secondary terraces around the basin — buildable shoulders, not a village yet.
		for (int32 I = 0; I < FineN; ++I)
		{
			if (BasinMask[I] || Water[I] || !Buildable[I])
			{
				continue;
			}
			const int32 X = I % FineW;
			const int32 Y = I / FineW;
			const int32 BX = BasinI % FineW;
			const int32 BY = BasinI / FineW;
			const double Dist = FMath::Sqrt(static_cast<double>((X - BX) * (X - BX) + (Y - BY) * (Y - BY)));
			if (Dist > RadiusCells && Dist < RadiusCells * 2.2 && Slope[I] < 0.04)
			{
				const double Step = 0.014;
				const double Terr = Sea + FMath::Floor((Forged[I] - Sea) / Step + 0.5) * Step;
				Forged[I] = FMath::Lerp(Forged[I], Terr, 0.45);
			}
		}
	}

	// Landmark high ground: keep the peak, sharpen its shoulders.
	if (LandmarkI != INDEX_NONE)
	{
		const int32 LX = LandmarkI % FineW;
		const int32 LY = LandmarkI / FineW;
		const int32 PeakR = FMath::Max(4, Subdiv * 2);
		for (int32 JY = FMath::Max(0, LY - PeakR); JY <= FMath::Min(FineH - 1, LY + PeakR); ++JY)
		{
			for (int32 IX = FMath::Max(0, LX - PeakR); IX <= FMath::Min(FineW - 1, LX + PeakR); ++IX)
			{
				const int32 I = JY * FineW + IX;
				if (Water[I])
				{
					continue;
				}
				const double Dist = FMath::Sqrt(static_cast<double>((IX - LX) * (IX - LX) + (JY - LY) * (JY - LY)));
				const double W = 1.0 - SmoothStep(0.0, static_cast<double>(PeakR), Dist);
				Forged[I] += W * 0.018 * FMath::Max(Lap[I], 0.0) * 4.0;
			}
		}
	}

	AnastasisTerrainSurface::FGeometry Result;
	Result.Vertices.Reserve(FineN);
	Result.Colors.Reserve(FineN);
	Result.SourceIndices.Reserve(FineN);
	Result.WaterVertices.Reserve(FineN);
	Result.Triangles.Reserve(2 * (FineW - 1) * (FineH - 1) * 3);
	Result.WaterTriangles.Reserve(2 * (FineW - 1) * (FineH - 1) * 3);

	double MinZ = TNumericLimits<double>::Max();
	double MaxZ = TNumericLimits<double>::Lowest();

	for (int32 JY = 0; JY < FineH; ++JY)
	{
		for (int32 IX = 0; IX < FineW; ++IX)
		{
			const int32 I = JY * FineW + IX;
			const double U = static_cast<double>(IX) * InvSub;
			const double V = static_cast<double>(JY) * InvSub;
			const double TileX = static_cast<double>(Crop.OriginX) + U;
			const double TileY = static_cast<double>(Crop.OriginY) + V;
			const double WorldX = (TileX + 0.5) * AnastasisWorldView::TileWorldSize;
			const double WorldY = (TileY + 0.5) * AnastasisWorldView::TileWorldSize;

			double Z;
			if (Water[I])
			{
				const double DepthExag = 1.7;
				Z = SeaZ + (H[I] - Sea) * AnastasisWorldView::AltitudeScale * DepthExag;
			}
			else
			{
				const double Land = Forged[I];
				const double Above = Land - Sea;
				const double ShoreBlend = SmoothStep(0.15, 0.85, Shore[I]);
				const double EdgeDist = static_cast<double>(FMath::Min(FMath::Min(IX, JY), FMath::Min(FineW - 1 - IX, FineH - 1 - JY)));
				const double Interior = SmoothStep(static_cast<double>(Subdiv) * 2.0, static_cast<double>(Subdiv) * 10.0, EdgeDist);
				const double LocalExag = FMath::Lerp(1.55, Exaggerate, Interior) * FMath::Lerp(1.0, 0.42, ShoreBlend);
				Z = SeaZ + Above * AnastasisWorldView::AltitudeScale * LocalExag;
			}

			Result.Vertices.Add(FVector(WorldX, WorldY, Z));
			Result.WaterVertices.Add(FVector(WorldX, WorldY, SeaZ));
			Result.Colors.Add(BilinearColor(InOut, CoarseW, CoarseH, U, V));
			const int32 CX = FMath::Clamp(FMath::RoundToInt(U), 0, CoarseW - 1);
			const int32 CY = FMath::Clamp(FMath::RoundToInt(V), 0, CoarseH - 1);
			Result.SourceIndices.Add(Crop.Tiles[CY * CoarseW + CX].SourceIndex);
			MinZ = FMath::Min(MinZ, Z);
			MaxZ = FMath::Max(MaxZ, Z);
		}
	}

	for (int32 JY = 0; JY < FineH - 1; ++JY)
	{
		for (int32 IX = 0; IX < FineW - 1; ++IX)
		{
			const int32 A = JY * FineW + IX, B = A + 1, C = A + FineW, D = C + 1;
			Result.Triangles.Append({A, C, B, B, C, D});
			if (Water[A] || Water[B] || Water[C] || Water[D])
			{
				Result.WaterTriangles.Append({A, C, B, B, C, D});
			}
		}
	}

	Result.WaterNormals.Init(FVector::UpVector, FineN);
	RebuildNormals(Result);

	OutMeta.Geometry = MoveTemp(Result);
	OutMeta.CoarseW = CoarseW;
	OutMeta.CoarseH = CoarseH;
	OutMeta.Subdiv = Subdiv;
	OutMeta.FineW = FineW;
	OutMeta.FineH = FineH;
	OutMeta.OriginX = Crop.OriginX;
	OutMeta.OriginY = Crop.OriginY;
	OutMeta.MinZ = MinZ;
	OutMeta.MaxZ = MaxZ;
	if (BasinI != INDEX_NONE)
	{
		OutMeta.bBasinFound = true;
		OutMeta.BasinX = OutMeta.Geometry.Vertices[BasinI].X;
		OutMeta.BasinY = OutMeta.Geometry.Vertices[BasinI].Y;
		OutMeta.BasinZ = OutMeta.Geometry.Vertices[BasinI].Z;
	}
	if (LandmarkI != INDEX_NONE)
	{
		OutMeta.bLandmarkFound = true;
		OutMeta.LandmarkX = OutMeta.Geometry.Vertices[LandmarkI].X;
		OutMeta.LandmarkY = OutMeta.Geometry.Vertices[LandmarkI].Y;
		OutMeta.LandmarkZ = OutMeta.Geometry.Vertices[LandmarkI].Z;
	}

	InOut = OutMeta.Geometry;
	SetActive(OutMeta);
	return true;
}
