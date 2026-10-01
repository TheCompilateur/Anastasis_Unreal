#include "WorldView/AnastasisHydrologyDressing.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "World/AnastasisHydrology.h"

namespace AnastasisHydrologyDressing
{
namespace
{
using namespace AnastasisWorldView;
using AnastasisWorld::ETileType;

uint32 Hash(uint32 Seed, int32 X, int32 Y, uint32 Salt)
{
	uint32 H = Seed ^ 0xA5A5A5A5u;
	H = (H ^ static_cast<uint32>(X)) * 0x85EBCA6Bu;
	H = (H ^ static_cast<uint32>(Y)) * 0xC2B2AE35u;
	H = (H ^ Salt) * 0x27D4EB2Fu;
	return H ^ (H >> 15);
}

double Unit(uint32 H)
{
	return static_cast<double>(H) / 4294967296.0;
}

const int32 Off4[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
const int32 Off8[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

const FLinearColor FlowTorrent(0.32f, 0.58f, 0.55f, 1.0f);
const FLinearColor FlowValley(0.14f, 0.40f, 0.44f, 1.0f);
const FLinearColor FlowInflow(0.16f, 0.34f, 0.28f, 1.0f);
const FLinearColor FoamWhite(0.84f, 0.88f, 0.86f, 1.0f);
const FLinearColor BankWet(0.145f, 0.132f, 0.098f, 0.0f);
const FLinearColor BankMud(0.210f, 0.188f, 0.140f, 0.0f);
const FLinearColor BankPebble(0.380f, 0.350f, 0.300f, 0.0f);
const FLinearColor RockDry(0.330f, 0.312f, 0.288f, 0.0f);
const FLinearColor RockWet(0.210f, 0.205f, 0.198f, 0.0f);
const FLinearColor ReedGreen(0.110f, 0.230f, 0.095f, 0.0f);
const FLinearColor DriftWood(0.180f, 0.130f, 0.090f, 0.0f);

const FVisualTile* TileAt(const FWorldVisualSnapshot& S, int32 X, int32 Y)
{
	return FindTile(S, X, Y);
}

bool InCrop(const FWorldVisualSnapshot* Crop, int32 X, int32 Y)
{
	if (!Crop)
	{
		return true;
	}
	return X >= Crop->OriginX && Y >= Crop->OriginY
		&& X < Crop->OriginX + Crop->W && Y < Crop->OriginY + Crop->H;
}

void FinishNormals(FMesh& M)
{
	M.Normals.SetNum(M.Vertices.Num());
	for (FVector& N : M.Normals)
	{
		N = FVector::ZeroVector;
	}
	for (int32 I = 0; I + 2 < M.Triangles.Num(); I += 3)
	{
		const int32 A = M.Triangles[I], B = M.Triangles[I + 1], C = M.Triangles[I + 2];
		if (!M.Vertices.IsValidIndex(A) || !M.Vertices.IsValidIndex(B) || !M.Vertices.IsValidIndex(C))
		{
			continue;
		}
		const FVector N = FVector::CrossProduct(M.Vertices[C] - M.Vertices[A], M.Vertices[B] - M.Vertices[A]);
		M.Normals[A] += N;
		M.Normals[B] += N;
		M.Normals[C] += N;
	}
	for (FVector& N : M.Normals)
	{
		N = N.GetSafeNormal();
		if (N.IsNearlyZero())
		{
			N = FVector::UpVector;
		}
	}
}

void AppendTri(FMesh& M, const FVector& A, const FVector& B, const FVector& C,
	const FLinearColor& Color, uint8 SnapA, uint8 SnapB, uint8 SnapC)
{
	const int32 Base = M.Vertices.Num();
	M.Vertices.Append({A, B, C});
	M.Colors.Append({Color, Color, Color});
	M.SnapToGround.Append({SnapA, SnapB, SnapC});
	M.Triangles.Append({Base, Base + 2, Base + 1});
}

void AppendQuad(FMesh& M,
	const FVector& BL, const FVector& BR, const FVector& TL, const FVector& TR,
	const FLinearColor& Color, uint8 SnapLand, uint8 SnapWater)
{
	const int32 Base = M.Vertices.Num();
	M.Vertices.Append({BL, BR, TL, TR});
	M.Colors.Append({Color, Color, Color, Color});
	M.SnapToGround.Append({SnapLand, SnapLand, SnapWater, SnapWater});
	// Same clockwise-from-above convention as TerrainSurface::Build.
	M.Triangles.Append({Base, Base + 2, Base + 1, Base + 1, Base + 2, Base + 3});
}

void AppendBox(FMesh& M, const FVector& Center, const FVector& Extent, const FRotator& Rot,
	const FLinearColor& Color, uint8 Snap)
{
	const FTransform T(Rot, Center);
	const FVector C[8] = {
		FVector(-1, -1, -1), FVector(1, -1, -1), FVector(1, 1, -1), FVector(-1, 1, -1),
		FVector(-1, -1, 1), FVector(1, -1, 1), FVector(1, 1, 1), FVector(-1, 1, 1)
	};
	FVector P[8];
	for (int32 I = 0; I < 8; ++I)
	{
		P[I] = T.TransformPosition(C[I] * Extent);
	}
	const int32 Faces[6][4] = {
		{0, 1, 4, 5}, {1, 2, 5, 6}, {2, 3, 6, 7}, {3, 0, 7, 4}, {4, 5, 7, 6}, {3, 2, 0, 1}
	};
	for (int32 F = 0; F < 6; ++F)
	{
		AppendQuad(M, P[Faces[F][0]], P[Faces[F][1]], P[Faces[F][2]], P[Faces[F][3]], Color, Snap, Snap);
	}
}

FLinearColor FlowColor(EArchetype Kind)
{
	switch (Kind)
	{
	case EArchetype::Torrent: return FlowTorrent;
	case EArchetype::Inflow: return FlowInflow;
	default: return FlowValley;
	}
}

void AccumulateSite(FSite& Site, EArchetype Kind, const FVector& P, double FlowAmt, double Relief, double Width, double Score, bool bCameraCandidate)
{
	if (Site.Archetype == EArchetype::None)
	{
		Site.Archetype = Kind;
	}
	Site.MeanFlowAmt += FlowAmt;
	Site.MeanBankRelief += Relief;
	Site.MeanWidth += Width;
	++Site.TileCount;
	if (bCameraCandidate && Score > Site.BestScore)
	{
		Site.BestScore = Score;
		Site.Centroid = P;
	}
}

void FinalizeSite(FSite& Site)
{
	if (Site.TileCount <= 0)
	{
		Site = FSite{};
		return;
	}
	const double N = static_cast<double>(Site.TileCount);
	Site.MeanFlowAmt /= N;
	Site.MeanBankRelief /= N;
	Site.MeanWidth /= N;
}
}

bool Build(
	const FWorldVisualSnapshot& Canonical,
	const FWorldVisualSnapshot* Crop,
	FPlan& Out,
	FString& OutError)
{
	Out = FPlan{};
	OutError.Reset();
	if (Canonical.SourceW != ReferenceWidth || Canonical.SourceH != ReferenceHeight
		|| Canonical.OriginX != 0 || Canonical.OriginY != 0
		|| Canonical.W != ReferenceWidth || Canonical.H != ReferenceHeight
		|| Canonical.Tiles.Num() != ReferenceWidth * ReferenceHeight)
	{
		OutError = TEXT("Source: expected full canonical 96x96 snapshot");
		return false;
	}
	if (Crop)
	{
		if (Crop->SourceW != ReferenceWidth || Crop->SourceH != ReferenceHeight
			|| Crop->W < 2 || Crop->H < 2
			|| Crop->OriginX < 0 || Crop->OriginY < 0
			|| Crop->OriginX + Crop->W > ReferenceWidth
			|| Crop->OriginY + Crop->H > ReferenceHeight
			|| Crop->Tiles.Num() != Crop->W * Crop->H)
		{
			OutError = TEXT("Crop: invalid rendered footprint");
			return false;
		}
	}
	for (int32 I = 0; I < Canonical.Tiles.Num(); ++I)
	{
		const auto& T = Canonical.Tiles[I];
		if (T.X != I % Canonical.W || T.Y != I / Canonical.W || T.SourceIndex != I
			|| !FMath::IsFinite(T.Alt) || !FMath::IsFinite(T.FlowAmt) || !FMath::IsFinite(T.Shore)
			|| !FMath::IsFinite(T.Wetness) || T.Wetness < 0.0 || T.Wetness > 1.0
			|| T.FlowAmt < 0.0 || T.FlowAmt > 1.0
			|| static_cast<uint8>(T.Type) >= AnastasisWorld::TileTypeCount)
		{
			OutError = FString::Printf(TEXT("Source.Tiles[%d]: invalid hydrology fields"), I);
			return false;
		}
	}

	const int32 W = Canonical.W;
	const int32 H = Canonical.H;
	const int32 N = W * H;
	const double Gate = AnastasisHydrology::WaterFlowAmtGate;
	constexpr int32 LakeMin = 8;
	constexpr double TorrentRelief = 0.040;
	constexpr int32 TorrentMaxWater3 = 4;

	TArray<uint8> IsWater;
	TArray<uint8> IsFlowing;
	IsWater.SetNumUninitialized(N);
	IsFlowing.SetNumUninitialized(N);
	for (int32 I = 0; I < N; ++I)
	{
		const auto& T = Canonical.Tiles[I];
		IsWater[I] = T.Type == ETileType::Water ? 1 : 0;
		IsFlowing[I] = (IsWater[I] && T.FlowAmt >= Gate) ? 1 : 0;
	}

	TArray<int32> StillComp;
	StillComp.Init(INDEX_NONE, N);
	TArray<int32> CompSize;
	int32 NextComp = 0;
	TArray<int32> Stack;
	for (int32 Start = 0; Start < N; ++Start)
	{
		if (!IsWater[Start] || IsFlowing[Start] || StillComp[Start] != INDEX_NONE)
		{
			continue;
		}
		int32 Size = 0;
		Stack.Reset();
		Stack.Add(Start);
		StillComp[Start] = NextComp;
		while (Stack.Num() > 0)
		{
			const int32 I = Stack.Pop();
			++Size;
			const int32 X = I % W;
			const int32 Y = I / W;
			for (int32 D = 0; D < 4; ++D)
			{
				const int32 NX = X + Off4[D][0];
				const int32 NY = Y + Off4[D][1];
				if (NX < 0 || NY < 0 || NX >= W || NY >= H)
				{
					continue;
				}
				const int32 NI = NY * W + NX;
				if (!IsWater[NI] || IsFlowing[NI] || StillComp[NI] != INDEX_NONE)
				{
					continue;
				}
				StillComp[NI] = NextComp;
				Stack.Add(NI);
			}
		}
		CompSize.Add(Size);
		++NextComp;
	}

	TArray<EArchetype> Class;
	Class.Init(EArchetype::None, N);
	TArray<double> BankRelief;
	TArray<double> Width01;
	BankRelief.SetNumZeroed(N);
	Width01.SetNumZeroed(N);

	for (int32 I = 0; I < N; ++I)
	{
		if (!IsWater[I])
		{
			continue;
		}
		const auto& T = Canonical.Tiles[I];
		int32 Water3 = 0;
		double MaxLandAlt = AnastasisWorld::SeaLevel;
		bool bTouchesLake = false;
		for (int32 D = 0; D < 8; ++D)
		{
			const int32 NX = T.X + Off8[D][0];
			const int32 NY = T.Y + Off8[D][1];
			const auto* Neigh = TileAt(Canonical, NX, NY);
			if (!Neigh)
			{
				continue;
			}
			if (Neigh->Type == ETileType::Water)
			{
				++Water3;
				const int32 NI = NY * W + NX;
				if (StillComp[NI] != INDEX_NONE && CompSize.IsValidIndex(StillComp[NI])
					&& CompSize[StillComp[NI]] >= LakeMin)
				{
					bTouchesLake = true;
				}
			}
			else
			{
				MaxLandAlt = FMath::Max(MaxLandAlt, Neigh->Alt);
			}
		}
		BankRelief[I] = FMath::Max(0.0, MaxLandAlt - AnastasisWorld::SeaLevel);
		Width01[I] = static_cast<double>(Water3) / 8.0;
		if (!IsFlowing[I])
		{
			Class[I] = EArchetype::Still;
			++Out.StillTiles;
			continue;
		}
		++Out.FlowingTiles;
		if (bTouchesLake)
		{
			Class[I] = EArchetype::Inflow;
			++Out.InflowTiles;
		}
		else if (Water3 <= TorrentMaxWater3 && BankRelief[I] >= TorrentRelief)
		{
			Class[I] = EArchetype::Torrent;
			++Out.TorrentTiles;
		}
		else
		{
			Class[I] = EArchetype::Valley;
			++Out.ValleyTiles;
		}
	}

	auto Score = [&](int32 I)
	{
		return BankRelief[I] * (1.15 - Width01[I]);
	};
	if (Out.InflowTiles == 0)
	{
		for (int32 I = 0; I < N; ++I)
		{
			if (Class[I] != EArchetype::Valley)
			{
				continue;
			}
			const auto& T = Canonical.Tiles[I];
			bool bTouchesStill = false;
			for (int32 D = 0; D < 4; ++D)
			{
				const auto* Neigh = TileAt(Canonical, T.X + Off4[D][0], T.Y + Off4[D][1]);
				if (Neigh && Neigh->Type == ETileType::Water && Neigh->FlowAmt < Gate)
				{
					bTouchesStill = true;
					break;
				}
			}
			if (!bTouchesStill)
			{
				continue;
			}
			Class[I] = EArchetype::Inflow;
			++Out.InflowTiles;
			--Out.ValleyTiles;
		}
	}
	if (Out.TorrentTiles == 0 && Out.ValleyTiles > 0)
	{
		TArray<int32> ValleyIdx;
		for (int32 I = 0; I < N; ++I)
		{
			if (Class[I] == EArchetype::Valley)
			{
				ValleyIdx.Add(I);
			}
		}
		ValleyIdx.Sort([&](int32 A, int32 B) { return Score(A) > Score(B); });
		const int32 Promote = FMath::Max(1, ValleyIdx.Num() / 5);
		for (int32 K = 0; K < Promote; ++K)
		{
			Class[ValleyIdx[K]] = EArchetype::Torrent;
			++Out.TorrentTiles;
			--Out.ValleyTiles;
		}
	}

	const double SeaZ = AnastasisTerrainSurface::WaterPlaneZ;
	const double Tile = TileWorldSize;

	for (int32 I = 0; I < N; ++I)
	{
		const auto& T = Canonical.Tiles[I];
		if (!InCrop(Crop, T.X, T.Y))
		{
			continue;
		}
		if (Class[I] == EArchetype::Torrent || Class[I] == EArchetype::Valley || Class[I] == EArchetype::Inflow)
		{
			FVector2D Flow(T.FlowX, T.FlowZ);
			if (Flow.SizeSquared() < 1.e-6)
			{
				Flow = FVector2D(1.0, 0.0);
			}
			Flow.Normalize();
			const FVector Tangent(Flow.X, Flow.Y, 0.0);
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Tangent).GetSafeNormal();
			const FVector Center = FVector(
				(static_cast<double>(T.X) + 0.5) * Tile,
				(static_cast<double>(T.Y) + 0.5) * Tile,
				SeaZ + (Class[I] == EArchetype::Torrent ? 8.0 : 6.0));
			const double HalfLen = Tile * 0.62;
			const double WidthScale = Class[I] == EArchetype::Torrent ? 0.30
				: Class[I] == EArchetype::Inflow ? 0.52 : 0.42;
			const double HalfWidth = Tile * (WidthScale + T.FlowAmt * 0.18 + Width01[I] * 0.10);
			const FVector A = Center - Tangent * HalfLen - Right * HalfWidth;
			const FVector B = Center - Tangent * HalfLen + Right * HalfWidth;
			const FVector C = Center + Tangent * HalfLen - Right * HalfWidth;
			const FVector D = Center + Tangent * HalfLen + Right * HalfWidth;
			AppendQuad(Out.Flow, A, B, C, D, FlowColor(Class[I]), 0, 0);

			const FVector WorldP = TileToUnreal(T.X, T.Y, AnastasisWorld::SeaLevel);
			const bool bCamera = T.X >= 10 && T.Y >= 10 && T.X < 86 && T.Y < 86;
			if (Class[I] == EArchetype::Torrent)
			{
				AccumulateSite(Out.Torrent, EArchetype::Torrent, WorldP, T.FlowAmt, BankRelief[I], Width01[I],
					BankRelief[I] * (1.2 - Width01[I]), bCamera);
			}
			else if (Class[I] == EArchetype::Valley)
			{
				AccumulateSite(Out.Valley, EArchetype::Valley, WorldP, T.FlowAmt, BankRelief[I], Width01[I],
					T.FlowAmt * Width01[I], bCamera);
			}
			else
			{
				AccumulateSite(Out.Inflow, EArchetype::Inflow, WorldP, T.FlowAmt, BankRelief[I], Width01[I],
					T.FlowAmt, bCamera);
			}
		}
	}

	auto EmitCascadeFoam = [&](const FVisualTile& WaterTile, const FVisualTile& LandTile)
	{
		const FVector Mid = FVector(
			(WaterTile.X * 0.65 + LandTile.X * 0.35 + 0.5) * Tile,
			(WaterTile.Y * 0.65 + LandTile.Y * 0.35 + 0.5) * Tile,
			SeaZ + 7.0);
		const FVector Along = FVector(LandTile.Y - WaterTile.Y, WaterTile.X - LandTile.X, 0.0).GetSafeNormal() * 22.0;
		const FVector Outward = FVector(WaterTile.X - LandTile.X, WaterTile.Y - LandTile.Y, 0.0).GetSafeNormal() * 12.0;
		AppendQuad(Out.Props,
			Mid - Along - Outward,
			Mid + Along - Outward,
			Mid - Along + Outward,
			Mid + Along + Outward,
			FoamWhite, 0, 0);
		++Out.FoamCount;
	};

	for (int32 I = 0; I < N; ++I)
	{
		const auto& Land = Canonical.Tiles[I];
		if (Land.Type == ETileType::Water || !InCrop(Crop, Land.X, Land.Y))
		{
			continue;
		}
		for (int32 D = 0; D < 4; ++D)
		{
			const auto* Water = TileAt(Canonical, Land.X + Off4[D][0], Land.Y + Off4[D][1]);
			if (!Water || Water->Type != ETileType::Water)
			{
				continue;
			}
			const int32 WI = Water->Y * W + Water->X;
			const EArchetype WaterKind = Class[WI];
			const auto* XM = TileAt(Canonical, Land.X - 1, Land.Y);
			const auto* XP = TileAt(Canonical, Land.X + 1, Land.Y);
			const auto* YM = TileAt(Canonical, Land.X, Land.Y - 1);
			const auto* YP = TileAt(Canonical, Land.X, Land.Y + 1);
			const double DX = (XP ? XP->Alt : Land.Alt) - (XM ? XM->Alt : Land.Alt);
			const double DY = (YP ? YP->Alt : Land.Alt) - (YM ? YM->Alt : Land.Alt);
			const double SlopeDeg = FMath::RadiansToDegrees(
				FMath::Atan(FMath::Sqrt(DX * DX + DY * DY) * AltitudeScale / Tile));
			const bool bSteep = SlopeDeg > 28.0;
			const FVector Inland(
				(Land.X + 0.5) * Tile,
				(Land.Y + 0.5) * Tile,
				Land.Alt * AltitudeScale + 2.0);
			const FVector Shore(
				(static_cast<double>(Land.X + Water->X) * 0.5 + 0.5) * Tile,
				(static_cast<double>(Land.Y + Water->Y) * 0.5 + 0.5) * Tile,
				SeaZ + 1.5);
			FVector Along(Off4[D][1], -Off4[D][0], 0.0);
			Along = Along.GetSafeNormal() * (Tile * (bSteep ? 0.28 : 0.46));
			const double InwardLen = Tile * (bSteep ? 0.12 : 0.42);
			const FVector Inward = (Inland - FVector(Shore.X, Shore.Y, Inland.Z)).GetSafeNormal() * InwardLen;
			const FVector LandA = FVector(Shore.X, Shore.Y, Inland.Z) + Inward - Along;
			const FVector LandB = FVector(Shore.X, Shore.Y, Inland.Z) + Inward + Along;
			const FVector WetA = Shore - Along;
			const FVector WetB = Shore + Along;
			FLinearColor BankColor = BankWet;
			if (WaterKind == EArchetype::Torrent)
			{
				BankColor = FMath::Lerp(BankWet, BankPebble, 0.55f);
			}
			else if (WaterKind == EArchetype::Inflow || WaterKind == EArchetype::Still)
			{
				BankColor = FMath::Lerp(BankWet, BankMud, 0.70f);
			}
			AppendQuad(Out.Bank, LandA, LandB, WetA, WetB, BankColor, 1, 0);

			if (WaterKind == EArchetype::Torrent && BankRelief[WI] >= 0.12
				&& (Land.Alt - AnastasisWorld::SeaLevel) >= 0.10
				&& Unit(Hash(Canonical.Seed, Land.X, Land.Y, 17)) < 0.22)
			{
				EmitCascadeFoam( *Water, Land);
			}

			const double Draw = Unit(Hash(Canonical.Seed, Land.X, Land.Y, 31 + D));
			const bool bTorrent = WaterKind == EArchetype::Torrent;
			const bool bValley = WaterKind == EArchetype::Valley;
			const bool bInflow = WaterKind == EArchetype::Inflow;
			const bool bStill = WaterKind == EArchetype::Still;
			const double RockP = bSteep ? (bTorrent ? 0.22 : 0.0) : (bTorrent ? 0.42 : bValley ? 0.22 : bInflow ? 0.18 : 0.08);
			const double ReedP = bSteep ? 0.0 : (bTorrent ? 0.05 : bValley ? 0.28 : bInflow ? 0.45 : 0.18);
			if (Draw < RockP)
			{
				const double U = Unit(Hash(Canonical.Seed, Land.X, Land.Y, 41 + D));
				const double V = Unit(Hash(Canonical.Seed, Land.X, Land.Y, 43 + D));
				const bool bSubmerged = bTorrent && (bSteep || U > 0.45);
				const FVector RockAt = bSubmerged
					? FVector(Shore.X + (U - 0.5) * 18.0, Shore.Y + (V - 0.5) * 18.0, SeaZ + 4.0 + U * 8.0)
					: FVector(LandA.X * (1.0 - U) + WetA.X * U, LandA.Y * (1.0 - V) + WetA.Y * V, Inland.Z + 6.0);
				const FVector Extent(
					6.0 + U * 10.0,
					5.0 + V * 8.0,
					bTorrent ? (7.0 + U * 11.0) : (4.5 + U * 6.0));
				const FRotator Rot(0.0f, static_cast<float>(U * 180.0), static_cast<float>((V - 0.5) * 18.0));
				AppendBox(Out.Props, RockAt, Extent, Rot, bSubmerged ? RockWet : RockDry, bSubmerged ? 0 : 1);
				++Out.RockCount;
			}
			if (Draw > 1.0 - ReedP && Land.Wetness >= 0.35 && !bTorrent)
			{
				const double U = Unit(Hash(Canonical.Seed, Land.X, Land.Y, 71 + D));
				const FVector ReedAt = FVector(
					WetA.X * 0.35 + LandA.X * 0.65 + (U - 0.5) * 12.0,
					WetA.Y * 0.35 + LandA.Y * 0.65,
					Inland.Z + 2.0);
				const double Hgt = 18.0 + U * 22.0;
				const FVector BladeA = ReedAt + FVector(-1.6, 0.0, 0.0);
				const FVector BladeB = ReedAt + FVector(1.6, 0.0, 0.0);
				const FVector BladeC = ReedAt + FVector(-1.2, 0.0, Hgt);
				const FVector BladeD = ReedAt + FVector(1.2, 0.0, Hgt);
				AppendQuad(Out.Props, BladeA, BladeB, BladeC, BladeD, ReedGreen, 1, 1);
				const FVector CrossA = ReedAt + FVector(0.0, -1.6, 0.0);
				const FVector CrossB = ReedAt + FVector(0.0, 1.6, 0.0);
				const FVector CrossC = ReedAt + FVector(0.0, -1.2, Hgt);
				const FVector CrossD = ReedAt + FVector(0.0, 1.2, Hgt);
				AppendQuad(Out.Props, CrossA, CrossB, CrossC, CrossD, ReedGreen, 1, 1);
				++Out.ReedCount;
			}
			if ((bInflow || bValley) && Land.Wetness > 0.55
				&& Unit(Hash(Canonical.Seed, Land.X, Land.Y, 91 + D)) < 0.08)
			{
				const FVector DriftAt(Shore.X, Shore.Y, SeaZ + 3.0);
				AppendBox(Out.Props, DriftAt, FVector(14.0, 3.2, 2.4),
					FRotator(0.0f, static_cast<float>(Land.X * 17 % 180), 4.0f), DriftWood, 0);
			}
		}
	}

	FinalizeSite(Out.Torrent);
	FinalizeSite(Out.Valley);
	FinalizeSite(Out.Inflow);
	FinishNormals(Out.Flow);
	FinishNormals(Out.Bank);
	FinishNormals(Out.Props);
	return true;
}
}
