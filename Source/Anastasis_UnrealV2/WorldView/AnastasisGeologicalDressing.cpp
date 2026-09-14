#include "WorldView/AnastasisGeologicalDressing.h"
#include "WorldView/AnastasisTerrainSurface.h"

namespace AnastasisGeologicalDressing
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

const FVisualTile* TileAt(const FWorldVisualSnapshot& S, int32 X, int32 Y)
{
	if (X < 0 || Y < 0 || X >= S.W || Y >= S.H)
	{
		return nullptr;
	}
	return &S.Tiles[Y * S.W + X];
}

bool Hostile(ETileType Type)
{
	return Type == ETileType::Water || Type == ETileType::Ruin;
}

bool SampleSlope(const FWorldVisualSnapshot& S, double WorldX, double WorldY,
	double& OutZ, double& OutSlopeDeg, double& OutAspectYaw)
{
	OutZ = 0.0;
	OutSlopeDeg = 0.0;
	OutAspectYaw = 0.0;
	if (!AnastasisTerrainSurface::SampleHeight(S, WorldX, WorldY, OutZ))
	{
		return false;
	}
	const double U = WorldX / TileWorldSize - 0.5;
	const double V = WorldY / TileWorldSize - 0.5;
	const int32 IX = FMath::Clamp(FMath::FloorToInt(U), 0, S.W - 2);
	const int32 IY = FMath::Clamp(FMath::FloorToInt(V), 0, S.H - 2);
	const int32 A = IY * S.W + IX;
	const int32 B = A + 1;
	const int32 C = A + S.W;
	const int32 D = C + 1;
	const bool First = (U - IX) + (V - IY) <= 1.0;
	const double DX = First ? S.Tiles[B].Alt - S.Tiles[A].Alt : S.Tiles[D].Alt - S.Tiles[C].Alt;
	const double DY = First ? S.Tiles[C].Alt - S.Tiles[A].Alt : S.Tiles[D].Alt - S.Tiles[B].Alt;
	const double GradLen = FMath::Sqrt(DX * DX + DY * DY);
	OutSlopeDeg = FMath::RadiansToDegrees(FMath::Atan(GradLen * AltitudeScale / TileWorldSize));
	// +X of the mesh points uphill (into the face). Yaw 0 is +X.
	OutAspectYaw = FMath::RadiansToDegrees(FMath::Atan2(DY, DX));
	return FMath::IsFinite(OutSlopeDeg) && FMath::IsFinite(OutAspectYaw);
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

EKind PickCliffKind(double Convexity, double SlopeDeg, double Roll)
{
	if (Convexity > 0.045 && Roll < 0.22)
	{
		return EKind::Cornice;
	}
	if (Roll < 0.30)
	{
		return EKind::VerticalWall;
	}
	if (Roll < 0.50)
	{
		return EKind::Stratum;
	}
	if (Roll < 0.66)
	{
		return SlopeDeg < 48.0 ? EKind::InclinedWall : EKind::Fractured;
	}
	if (Roll < 0.82)
	{
		return EKind::Fractured;
	}
	return EKind::Outcrop;
}

EKind PickSlopeKind(double Roll)
{
	if (Roll < 0.34)
	{
		return EKind::InclinedWall;
	}
	if (Roll < 0.72)
	{
		return EKind::Outcrop;
	}
	if (Roll < 0.86)
	{
		return EKind::Stratum;
	}
	return EKind::Transition;
}

EKind PickTalusKind(double Roll)
{
	if (Roll < 0.48)
	{
		return EKind::TalusCluster;
	}
	if (Roll < 0.78)
	{
		return EKind::DetachedBlock;
	}
	return EKind::Transition;
}

enum class EContext : uint8 { None, Cliff, Slope, Talus, Summit };

EContext Classify(
	const FWorldVisualSnapshot& S,
	const FVisualTile& T,
	double SlopeDeg,
	const TArray<uint8>& CliffMask,
	float CliffMin)
{
	if (SlopeDeg >= CliffMin)
	{
		return EContext::Cliff;
	}
	bool bFoot = false;
	bool bRim = false;
	bool bHigher = true;
	for (int32 DY = -1; DY <= 1; ++DY)
	{
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			if (DX == 0 && DY == 0)
			{
				continue;
			}
			const FVisualTile* N = TileAt(S, T.X + DX, T.Y + DY);
			if (!N)
			{
				continue;
			}
			if (N->Alt > T.Alt + 1.0e-6)
			{
				bHigher = false;
			}
			if (N->Alt + 0.08 < T.Alt)
			{
				bRim = true;
			}
			const int32 NI = N->Y * S.W + N->X;
			if (CliffMask[NI] != 0 && T.Alt <= N->Alt + 0.02)
			{
				bFoot = true;
			}
		}
	}
	if (bHigher && bRim && T.Alt > 0.42)
	{
		return EContext::Summit;
	}
	if (bFoot)
	{
		return EContext::Talus;
	}
	if (SlopeDeg >= 14.0)
	{
		return EContext::Slope;
	}
	return EContext::None;
}
}

const TCHAR* KindName(EKind Kind)
{
	switch (Kind)
	{
	case EKind::VerticalWall: return TEXT("VerticalWall");
	case EKind::InclinedWall: return TEXT("InclinedWall");
	case EKind::Stratum: return TEXT("Stratum");
	case EKind::Cornice: return TEXT("Cornice");
	case EKind::Outcrop: return TEXT("Outcrop");
	case EKind::Fractured: return TEXT("Fractured");
	case EKind::DetachedBlock: return TEXT("DetachedBlock");
	case EKind::TalusCluster: return TEXT("TalusCluster");
	case EKind::Transition: return TEXT("Transition");
	case EKind::Summit: return TEXT("Summit");
	default: return TEXT("Unknown");
	}
}

float DefaultScaleMin(EKind Kind)
{
	switch (Kind)
	{
	case EKind::VerticalWall: return 2.6f;
	case EKind::InclinedWall: return 2.1f;
	case EKind::Stratum: return 2.3f;
	case EKind::Cornice: return 1.7f;
	case EKind::Outcrop: return 1.15f;
	case EKind::Fractured: return 2.4f;
	case EKind::DetachedBlock: return 1.35f;
	case EKind::TalusCluster: return 0.85f;
	case EKind::Transition: return 0.90f;
	case EKind::Summit: return 2.2f;
	default: return 1.0f;
	}
}

float DefaultScaleMax(EKind Kind)
{
	switch (Kind)
	{
	case EKind::VerticalWall: return 4.6f;
	case EKind::InclinedWall: return 3.6f;
	case EKind::Stratum: return 4.0f;
	case EKind::Cornice: return 3.1f;
	case EKind::Outcrop: return 2.1f;
	case EKind::Fractured: return 4.2f;
	case EKind::DetachedBlock: return 2.5f;
	case EKind::TalusCluster: return 1.7f;
	case EKind::Transition: return 1.55f;
	case EKind::Summit: return 3.8f;
	default: return 1.0f;
	}
}

bool Build(const FWorldVisualSnapshot& S, const FAnastasisLithosDressingSettings& C, FPlan& Out, FString& Error)
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
		C.CliffMinDegrees, C.SlopeMinDegrees, C.CliffDensity, C.SlopeDensity, C.TalusDensity,
		C.SummitDensity, C.CliffSpacing, C.SlopeSpacing, C.TalusSpacing, C.SummitSpacing,
		C.WaterClearanceUU, C.EmbedUU
	};
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value))
		{
			Error = TEXT("Settings: non-finite value");
			return false;
		}
	}
	if (C.CliffMinDegrees < 12.0f || C.CliffMinDegrees > 70.0f
		|| C.SlopeMinDegrees < 6.0f || C.SlopeMinDegrees > 40.0f
		|| C.SlopeMinDegrees >= C.CliffMinDegrees
		|| C.CliffDensity < 0.0f || C.CliffDensity > 1.0f
		|| C.SlopeDensity < 0.0f || C.SlopeDensity > 1.0f
		|| C.TalusDensity < 0.0f || C.TalusDensity > 1.0f
		|| C.SummitDensity < 0.0f || C.SummitDensity > 1.0f
		|| C.CliffSpacing < 1.2f || C.CliffSpacing > 8.0f
		|| C.SlopeSpacing < 1.2f || C.SlopeSpacing > 8.0f
		|| C.TalusSpacing < 0.8f || C.TalusSpacing > 6.0f
		|| C.SummitSpacing < 3.0f || C.SummitSpacing > 16.0f
		|| C.WaterClearanceUU < 0.0f || C.WaterClearanceUU > 40.0f
		|| C.EmbedUU < 8.0f || C.EmbedUU > 80.0f)
	{
		Error = TEXT("Settings: value outside supported range");
		return false;
	}
	for (int32 I = 0; I < S.Tiles.Num(); ++I)
	{
		const auto& T = S.Tiles[I];
		if (T.X != I % S.W || T.Y != I / S.W || T.SourceIndex != I
			|| !FMath::IsFinite(T.Alt) || !FMath::IsFinite(T.Wetness) || T.Wetness < 0 || T.Wetness > 1
			|| static_cast<uint8>(T.Type) >= AnastasisWorld::TileTypeCount)
		{
			Error = FString::Printf(TEXT("Source.Tiles[%d]: invalid coordinates, altitude, wetness or type"), I);
			return false;
		}
	}
	if (!C.bEnabled)
	{
		return true;
	}

	const int32 N = S.Tiles.Num();
	TArray<double> Slope;
	TArray<double> Aspect;
	TArray<double> HeightZ;
	TArray<uint8> CliffMask;
	Slope.SetNumUninitialized(N);
	Aspect.SetNumUninitialized(N);
	HeightZ.SetNumUninitialized(N);
	CliffMask.SetNumZeroed(N);
	for (int32 I = 0; I < N; ++I)
	{
		const auto& T = S.Tiles[I];
		const double WorldX = (static_cast<double>(T.X) + 0.5) * TileWorldSize;
		const double WorldY = (static_cast<double>(T.Y) + 0.5) * TileWorldSize;
		double Z = 0.0, Sl = 0.0, Yaw = 0.0;
		if (!SampleSlope(S, WorldX, WorldY, Z, Sl, Yaw))
		{
			Slope[I] = 0.0;
			Aspect[I] = 0.0;
			HeightZ[I] = 0.0;
			continue;
		}
		Slope[I] = Sl;
		Aspect[I] = Yaw;
		HeightZ[I] = Z;
		if (Sl >= C.CliffMinDegrees && !Hostile(T.Type))
		{
			CliffMask[I] = 1;
		}
	}

	FPlan Result;
	TMap<FIntPoint, TArray<FVector2D>> Occupied;
	for (int32 I = 0; I < N; ++I)
	{
		const auto& T = S.Tiles[I];
		if (Hostile(T.Type))
		{
			++Result.RejectedWater;
			continue;
		}
		if (HeightZ[I] <= AnastasisTerrainSurface::WaterPlaneZ + C.WaterClearanceUU)
		{
			++Result.RejectedWater;
			continue;
		}

		double Convexity = 0.0;
		int32 Neighbors = 0;
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				if (DX == 0 && DY == 0)
				{
					continue;
				}
				if (const FVisualTile* Nb = TileAt(S, T.X + DX, T.Y + DY))
				{
					Convexity += T.Alt - Nb->Alt;
					++Neighbors;
				}
			}
		}
		if (Neighbors > 0)
		{
			Convexity /= Neighbors;
		}

		const EContext Ctx = Classify(S, T, Slope[I], CliffMask, C.CliffMinDegrees);
		if (Ctx == EContext::None || (Ctx == EContext::Slope && Slope[I] < C.SlopeMinDegrees))
		{
			++Result.RejectedFlat;
			continue;
		}

		const uint32 Seed = Hash(S.Seed, T.X, T.Y, 701);
		double Density = 0.0;
		double SpacingTiles = 3.0;
		switch (Ctx)
		{
		case EContext::Cliff:
			Density = C.CliffDensity;
			SpacingTiles = C.CliffSpacing;
			break;
		case EContext::Slope:
			Density = C.SlopeDensity;
			SpacingTiles = C.SlopeSpacing;
			break;
		case EContext::Talus:
			Density = C.TalusDensity;
			SpacingTiles = C.TalusSpacing;
			break;
		case EContext::Summit:
			Density = C.SummitDensity;
			SpacingTiles = C.SummitSpacing;
			break;
		default:
			break;
		}
		if (T.Type == ETileType::Stone)
		{
			Density = FMath::Min(1.0, Density * 1.28);
		}
		if (Unit(Hash(Seed, T.X, T.Y, 3)) >= Density)
		{
			continue;
		}

		const double Jx = (Unit(Hash(Seed, T.X, T.Y, 4)) - 0.5) * 0.42;
		const double Jy = (Unit(Hash(Seed, T.X, T.Y, 5)) - 0.5) * 0.42;
		const double WorldX = (static_cast<double>(T.X) + 0.5 + Jx) * TileWorldSize;
		const double WorldY = (static_cast<double>(T.Y) + 0.5 + Jy) * TileWorldSize;
		double Z = 0.0, Sl = 0.0, Yaw = 0.0;
		if (!SampleSlope(S, WorldX, WorldY, Z, Sl, Yaw)
			|| Z <= AnastasisTerrainSurface::WaterPlaneZ + C.WaterClearanceUU)
		{
			++Result.RejectedWater;
			continue;
		}

		const FVector2D XY(WorldX, WorldY);
		const double Spacing = SpacingTiles * TileWorldSize;
		if (OccupiedNear(Occupied, XY, Spacing))
		{
			++Result.RejectedSpacing;
			continue;
		}

		const double Roll = Unit(Hash(Seed, T.X, T.Y, 6));
		EKind Kind = EKind::Outcrop;
		switch (Ctx)
		{
		case EContext::Cliff:
			Kind = PickCliffKind(Convexity, Sl, Roll);
			++Result.CliffCount;
			break;
		case EContext::Slope:
			Kind = PickSlopeKind(Roll);
			++Result.SlopeCount;
			break;
		case EContext::Talus:
			Kind = PickTalusKind(Roll);
			++Result.TalusCount;
			break;
		case EContext::Summit:
			Kind = Roll < 0.72 ? EKind::Summit : EKind::Outcrop;
			++Result.SummitCount;
			break;
		default:
			break;
		}

		Occupied.FindOrAdd(FIntPoint(FMath::FloorToInt(XY.X / Spacing), FMath::FloorToInt(XY.Y / Spacing))).Add(XY);

		FPlacement P;
		P.SourceIndex = T.SourceIndex;
		P.VisualSeed = Seed;
		P.Ground = FVector(WorldX, WorldY, Z);
		const double Lo = DefaultScaleMin(Kind);
		const double Hi = DefaultScaleMax(Kind);
		P.ScaleMultiplier = FMath::Lerp(Lo, Hi, Unit(Hash(Seed, T.X, T.Y, 7)));
		P.SlopeDegrees = Sl;
		P.AspectYaw = Yaw;
		const double YawJitter = (Unit(Hash(Seed, T.X, T.Y, 8)) - 0.5)
			* (Kind == EKind::TalusCluster || Kind == EKind::DetachedBlock ? 48.0 : 12.0);
		P.AspectYaw += YawJitter;
		P.PitchDegrees = FMath::Clamp(Sl * 0.18, 2.0, Kind == EKind::InclinedWall ? 16.0 : 9.0);
		if (Kind == EKind::TalusCluster || Kind == EKind::Transition)
		{
			P.PitchDegrees = FMath::Clamp(Sl * 0.08, 0.0, 6.0);
		}
		P.EmbedUU = C.EmbedUU * (Kind == EKind::TalusCluster ? 0.45 : Kind == EKind::Summit ? 0.70 : 1.0);
		P.Kind = Kind;
		Result.Instances.Add(P);
	}

	Out = MoveTemp(Result);
	return true;
}
}
