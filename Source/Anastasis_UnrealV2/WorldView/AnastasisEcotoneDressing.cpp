#include "WorldView/AnastasisEcotoneDressing.h"
#include "WorldView/AnastasisTerrainSurface.h"

namespace AnastasisEcotoneDressing
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

bool IsOpen(ETileType Type)
{
	return Type == ETileType::Grass || Type == ETileType::Scrub || Type == ETileType::Field;
}

bool IsForestHost(ETileType Type)
{
	return Type == ETileType::Forest || Type == ETileType::Grass || Type == ETileType::Scrub;
}

bool IsLand(ETileType Type)
{
	return Type != ETileType::Water;
}

bool ValidScale(const FVector2D& V)
{
	return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && V.X > 0.0 && V.Y >= V.X && V.Y <= 3.0;
}

bool GroundAt(const FWorldVisualSnapshot& S, double X, double Y,
	const FAnastasisEcotoneDressingSettings& C, double& Z, double& Slope)
{
	const FVisualTile* Tile = FindTile(S,
		FMath::FloorToInt(X / TileWorldSize), FMath::FloorToInt(Y / TileWorldSize));
	if (!Tile || !IsLand(Tile->Type))
	{
		return false;
	}
	if (!AnastasisTerrainSurface::SampleHeight(S, X, Y, Z))
	{
		return false;
	}
	if (Z <= AnastasisTerrainSurface::WaterPlaneZ + C.WaterClearanceUU)
	{
		return false;
	}
	const double U = X / TileWorldSize - 0.5;
	const double V = Y / TileWorldSize - 0.5;
	const int32 IX = FMath::Min(FMath::FloorToInt(U), S.W - 2);
	const int32 IY = FMath::Min(FMath::FloorToInt(V), S.H - 2);
	if (IX < 0 || IY < 0)
	{
		Slope = 0.0;
		return true;
	}
	const int32 A = IY * S.W + IX;
	const int32 B = A + 1;
	const int32 CC = A + S.W;
	const int32 D = CC + 1;
	const bool First = (U - IX) + (V - IY) <= 1.0;
	const double DX = First ? S.Tiles[B].Alt - S.Tiles[A].Alt : S.Tiles[D].Alt - S.Tiles[CC].Alt;
	const double DY = First ? S.Tiles[CC].Alt - S.Tiles[A].Alt : S.Tiles[D].Alt - S.Tiles[B].Alt;
	Slope = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX * DX + DY * DY) * AltitudeScale / TileWorldSize));
	return true;
}

struct FNeighborhood
{
	double Forest = 0.0;
	double Open = 0.0;
	double Stone = 0.0;
	double Water = 0.0;
	double Weight = 0.0;
};

FNeighborhood Neighborhood(const FWorldVisualSnapshot& S, double X, double Y, float Radius)
{
	FNeighborhood N;
	const int32 CX = FMath::FloorToInt(X);
	const int32 CY = FMath::FloorToInt(Y);
	const int32 R = FMath::CeilToInt(Radius);
	for (int32 DY = -R; DY <= R; ++DY)
	{
		for (int32 DX = -R; DX <= R; ++DX)
		{
			const double Dist = FVector2D(X - (CX + DX + 0.5), Y - (CY + DY + 0.5)).Size();
			const double W = FMath::Max(0.0, 1.0 - Dist / Radius);
			if (W <= 0.0)
			{
				continue;
			}
			const FVisualTile* T = FindTile(S, CX + DX, CY + DY);
			if (!T)
			{
				continue;
			}
			N.Weight += W;
			if (T->Type == ETileType::Forest) N.Forest += W;
			if (IsOpen(T->Type)) N.Open += W;
			if (T->Type == ETileType::Stone) N.Stone += W;
			if (T->Type == ETileType::Water) N.Water += W;
		}
	}
	if (N.Weight > 0.0)
	{
		N.Forest /= N.Weight;
		N.Open /= N.Weight;
		N.Stone /= N.Weight;
		N.Water /= N.Weight;
	}
	return N;
}

EAsset PickAsset(EContext Ctx, uint32 Seed, int32 X, int32 Y, double Slope)
{
	const double U = Unit(Hash(Seed, X, Y, 41));
	const bool bSteep = Slope > 10.0;
	EAsset Assets[8];
	double Weights[8];
	int32 Count = 0;
	auto Add = [&](EAsset Asset, double Weight)
	{
		Assets[Count] = Asset;
		Weights[Count] = Weight;
		++Count;
	};
	switch (Ctx)
	{
	case EContext::Understory:
		Add(EAsset::Stump, 0.16);
		Add(EAsset::FallenLog, 0.18);
		Add(EAsset::ExposedRoots, bSteep ? 0.12 : 0.04);
		Add(EAsset::BranchPile, 0.14);
		Add(EAsset::BushLow, 0.16);
		Add(EAsset::GrassTuft, 0.12);
		Add(EAsset::Sapling, 0.08);
		break;
	case EContext::ForestEdge:
		Add(EAsset::BushLow, 0.22);
		Add(EAsset::GrassTuft, 0.16);
		Add(EAsset::Sapling, 0.16);
		Add(EAsset::Stump, 0.12);
		Add(EAsset::FallenLog, 0.12);
		Add(EAsset::BranchPile, 0.10);
		Add(EAsset::ExposedRoots, bSteep ? 0.12 : 0.04);
		break;
	case EContext::Shore:
		Add(EAsset::Reed, 0.34);
		Add(EAsset::ShoreTuft, 0.28);
		Add(EAsset::Driftwood, 0.18);
		Add(EAsset::RockCluster, 0.20);
		break;
	case EContext::RockFoot:
	default:
		Add(EAsset::BuriedBlock, 0.26);
		Add(EAsset::RockCluster, 0.34);
		Add(EAsset::GrassTuft, 0.22);
		Add(EAsset::ExposedRoots, 0.18);
		break;
	}
	double Total = 0.0;
	for (int32 I = 0; I < Count; ++I)
	{
		Total += Weights[I];
	}
	double Acc = 0.0;
	for (int32 I = 0; I < Count; ++I)
	{
		Acc += Weights[I] / Total;
		if (U < Acc)
		{
			return Assets[I];
		}
	}
	return Assets[Count - 1];
}

EAsset CompanionOf(EAsset Anchor, uint32 Seed, int32 X, int32 Y)
{
	const double U = Unit(Hash(Seed, X, Y, 77));
	switch (Anchor)
	{
	case EAsset::Stump:
		return U < 0.55 ? EAsset::Sapling : EAsset::GrassTuft;
	case EAsset::FallenLog:
		return U < 0.50 ? EAsset::BranchPile : EAsset::GrassTuft;
	case EAsset::BuriedBlock:
		return U < 0.55 ? EAsset::GrassTuft : EAsset::RockCluster;
	case EAsset::Driftwood:
		return U < 0.55 ? EAsset::ShoreTuft : EAsset::RockCluster;
	case EAsset::ExposedRoots:
		return EAsset::GrassTuft;
	case EAsset::BushLow:
		return EAsset::GrassTuft;
	default:
		return Anchor;
	}
}

bool WantsCompanion(EAsset Asset)
{
	switch (Asset)
	{
	case EAsset::Stump:
	case EAsset::FallenLog:
	case EAsset::BuriedBlock:
	case EAsset::Driftwood:
	case EAsset::ExposedRoots:
	case EAsset::BushLow:
		return true;
	default:
		return false;
	}
}

bool OccupiedNear(const TMap<FIntPoint, TArray<FVector2D>>& Occupied, const FVector2D& XY, double Spacing)
{
	const FIntPoint Cell(FMath::FloorToInt(XY.X / Spacing), FMath::FloorToInt(XY.Y / Spacing));
	for (int32 DY = -1; DY <= 1; ++DY)
	{
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			if (const TArray<FVector2D>* Neighbors = Occupied.Find(Cell + FIntPoint(DX, DY)))
			{
				for (const FVector2D& N : *Neighbors)
				{
					if (FVector2D::DistSquared(N, XY) < Spacing * Spacing)
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

void Occupy(TMap<FIntPoint, TArray<FVector2D>>& Occupied, const FVector2D& XY, double Spacing)
{
	const FIntPoint Cell(FMath::FloorToInt(XY.X / Spacing), FMath::FloorToInt(XY.Y / Spacing));
	Occupied.FindOrAdd(Cell).Add(XY);
}

bool FillPlacement(const FWorldVisualSnapshot& S, const FAnastasisEcotoneDressingSettings& C,
	const FVisualTile& T, uint32 Seed, EContext Ctx, EAsset Asset, bool bCompanion,
	double X, double Y, FPlacement& Out, FPlan& Result)
{
	double Z = 0.0;
	double Slope = 0.0;
	if (!GroundAt(S, X * TileWorldSize, Y * TileWorldSize, C, Z, Slope))
	{
		++Result.RejectedWaterOrFootprint;
		return false;
	}
	if (Slope > C.MaxSlopeDegrees)
	{
		++Result.RejectedSlope;
		return false;
	}
	Out.SourceIndex = T.SourceIndex;
	Out.VisualSeed = Seed;
	Out.Ground = FVector(X * TileWorldSize, Y * TileWorldSize, Z);
	Out.ScaleMultiplier = FMath::Lerp(C.ScaleEnvelope.X, C.ScaleEnvelope.Y, Unit(Hash(Seed, T.X, T.Y, 55)));
	Out.YawDegrees = Unit(Hash(Seed, T.X, T.Y, 56)) * 360.0;
	Out.SlopeDegrees = Slope;
	Out.Context = Ctx;
	Out.Asset = Asset;
	Out.bCompanion = bCompanion;
	return true;
}
}

const TCHAR* AssetName(EAsset Asset)
{
	static const TCHAR* Names[AssetCount] = {
		TEXT("Stump"), TEXT("FallenLog"), TEXT("ExposedRoots"), TEXT("BranchPile"),
		TEXT("Driftwood"), TEXT("BushLow"), TEXT("GrassTuft"), TEXT("Reed"),
		TEXT("ShoreTuft"), TEXT("Sapling"), TEXT("RockCluster"), TEXT("BuriedBlock")
	};
	const uint8 I = static_cast<uint8>(Asset);
	return I < AssetCount ? Names[I] : TEXT("Unknown");
}

const TCHAR* ContextName(EContext Context)
{
	static const TCHAR* Names[ContextCount] = {
		TEXT("understory"), TEXT("forest_edge"), TEXT("shore"), TEXT("rock_foot")
	};
	const uint8 I = static_cast<uint8>(Context);
	return I < ContextCount ? Names[I] : TEXT("unknown");
}

bool Build(const FWorldVisualSnapshot& S, const FAnastasisEcotoneDressingSettings& C, FPlan& Out, FString& Error)
{
	Out = FPlan{};
	Error.Reset();
	if (S.SourceW != ReferenceWidth || S.SourceH != ReferenceHeight || S.OriginX != 0 || S.OriginY != 0
		|| S.W != ReferenceWidth || S.H != ReferenceHeight || S.Tiles.Num() != ReferenceWidth * ReferenceHeight)
	{
		Error = TEXT("Source: expected full canonical 96x96 snapshot");
		return false;
	}
	const float Values[] = {
		C.NeighborRadius, C.UnderstoryDensity, C.ForestEdgeDensity, C.ShoreDensity, C.RockFootDensity,
		C.MinimumSpacing, C.MaxSlopeDegrees, C.WaterClearanceUU, C.CompanionChance
	};
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value))
		{
			Error = TEXT("Settings: non-finite value");
			return false;
		}
	}
	if (C.CandidatesPerTile < 1 || C.CandidatesPerTile > 4 || C.NeighborRadius < 1.f || C.NeighborRadius > 4.f
		|| C.UnderstoryDensity < 0.f || C.UnderstoryDensity > 1.f
		|| C.ForestEdgeDensity < 0.f || C.ForestEdgeDensity > 1.f
		|| C.ShoreDensity < 0.f || C.ShoreDensity > 1.f
		|| C.RockFootDensity < 0.f || C.RockFootDensity > 1.f
		|| C.MinimumSpacing < 0.25f || C.MinimumSpacing > 1.2f
		|| C.MaxSlopeDegrees < 1.f || C.MaxSlopeDegrees > 60.f
		|| C.WaterClearanceUU < 0.f || C.WaterClearanceUU > 30.f
		|| C.CompanionChance < 0.f || C.CompanionChance > 1.f
		|| !ValidScale(C.ScaleEnvelope))
	{
		Error = TEXT("Settings: value outside supported range");
		return false;
	}
	for (int32 I = 0; I < S.Tiles.Num(); ++I)
	{
		const FVisualTile& T = S.Tiles[I];
		if (T.X != I % S.W || T.Y != I / S.W || T.SourceIndex != I
			|| !FMath::IsFinite(T.Alt) || !FMath::IsFinite(T.Wetness) || T.Wetness < 0.0 || T.Wetness > 1.0
			|| !FMath::IsFinite(T.Shore) || static_cast<uint8>(T.Type) >= AnastasisWorld::TileTypeCount)
		{
			Error = FString::Printf(TEXT("Source.Tiles[%d]: invalid coordinates, altitude, wetness, shore or type"), I);
			return false;
		}
	}
	if (!C.bEnabled)
	{
		return true;
	}

	FPlan Result;
	const double Spacing = C.MinimumSpacing * TileWorldSize;
	TMap<FIntPoint, TArray<FVector2D>> Occupied;

	for (const FVisualTile& T : S.Tiles)
	{
		if (!IsLand(T.Type))
		{
			continue;
		}
		for (int32 Candidate = 0; Candidate < C.CandidatesPerTile; ++Candidate)
		{
			const uint32 Seed = Hash(S.Seed, T.X, T.Y, 300 + Candidate);
			const double X = T.X + Unit(Hash(Seed, T.X, T.Y, 1));
			const double Y = T.Y + Unit(Hash(Seed, T.X, T.Y, 2));
			const FNeighborhood N = Neighborhood(S, X, Y, C.NeighborRadius);

			const double UnderstoryScore = (T.Type == ETileType::Forest && N.Forest >= 0.62) ? N.Forest : 0.0;
			const double EdgeScore = IsForestHost(T.Type) ? (4.0 * N.Forest * N.Open) : 0.0;
			const double ShoreScore = FMath::Max(T.Shore, N.Water * (1.0 - N.Water) * 4.0);
			const double RockScore = (T.Type == ETileType::Stone)
				? (0.55 + 0.45 * N.Open)
				: (N.Stone * (IsOpen(T.Type) || T.Type == ETileType::Forest ? 1.0 : 0.0));

			double Scores[ContextCount] = { UnderstoryScore, EdgeScore, ShoreScore, RockScore };
			if (UnderstoryScore > 0.0 && EdgeScore > 0.0)
			{
				// Interior and fringe are mutually exclusive: a stand is one or the other.
				Scores[static_cast<uint8>(EContext::ForestEdge)] = 0.0;
			}

			double Densities[ContextCount] = {
				C.UnderstoryDensity, C.ForestEdgeDensity, C.ShoreDensity, C.RockFootDensity
			};

			double Total = 0.0;
			for (int32 I = 0; I < ContextCount; ++I)
			{
				if (Scores[I] < 0.12)
				{
					Scores[I] = 0.0;
				}
				Total += Scores[I];
			}
			if (Total <= 0.0)
			{
				continue;
			}

			const double Choice = Unit(Hash(Seed, T.X, T.Y, 3)) * Total;
			double Acc = 0.0;
			EContext Ctx = EContext::ForestEdge;
			double Score = 0.0;
			double Density = 0.0;
			for (int32 I = 0; I < ContextCount; ++I)
			{
				Acc += Scores[I];
				if (Choice <= Acc || I == ContextCount - 1)
				{
					Ctx = static_cast<EContext>(I);
					Score = Scores[I];
					Density = Densities[I];
					break;
				}
			}

			const double Probability = Density * FMath::Clamp(Score, 0.0, 1.0);
			if (Unit(Hash(Seed, T.X, T.Y, 4)) >= Probability)
			{
				continue;
			}

			FPlacement P;
			if (!FillPlacement(S, C, T, Seed, Ctx, EAsset::GrassTuft, false, X, Y, P, Result))
			{
				continue;
			}
			P.Asset = PickAsset(Ctx, Seed, T.X, T.Y, P.SlopeDegrees);

			const FVector2D XY(P.Ground.X, P.Ground.Y);
			if (OccupiedNear(Occupied, XY, Spacing))
			{
				++Result.RejectedSpacing;
				continue;
			}
			Occupy(Occupied, XY, Spacing);
			Result.Instances.Add(P);
			++Result.ContextCounts[static_cast<uint8>(P.Context)];
			++Result.AssetCounts[static_cast<uint8>(P.Asset)];

			if (!WantsCompanion(P.Asset) || Unit(Hash(Seed, T.X, T.Y, 8)) >= C.CompanionChance)
			{
				continue;
			}
			const double Angle = Unit(Hash(Seed, T.X, T.Y, 9)) * 2.0 * PI;
			const double DistTiles = 0.32 + 0.28 * Unit(Hash(Seed, T.X, T.Y, 10));
			const double CX = X + FMath::Cos(Angle) * DistTiles;
			const double CY = Y + FMath::Sin(Angle) * DistTiles;
			const uint32 CompSeed = Hash(Seed, T.X, T.Y, 400);
			FPlacement Comp;
			if (!FillPlacement(S, C, T, CompSeed, Ctx, CompanionOf(P.Asset, Seed, T.X, T.Y), true, CX, CY, Comp, Result))
			{
				continue;
			}
			const FVector2D CXY(Comp.Ground.X, Comp.Ground.Y);
			if (OccupiedNear(Occupied, CXY, Spacing * 0.45))
			{
				++Result.RejectedSpacing;
				continue;
			}
			Occupy(Occupied, CXY, Spacing);
			Comp.bCompanion = true;
			Result.Instances.Add(Comp);
			++Result.CompanionCount;
			++Result.ContextCounts[static_cast<uint8>(Comp.Context)];
			++Result.AssetCounts[static_cast<uint8>(Comp.Asset)];
		}
	}

	Out = MoveTemp(Result);
	return true;
}
}
