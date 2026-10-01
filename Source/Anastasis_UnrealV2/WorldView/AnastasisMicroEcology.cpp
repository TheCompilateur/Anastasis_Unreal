#include "WorldView/AnastasisMicroEcology.h"

#include "Async/ParallelFor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "WorldView/AnastasisPlaces.h"

namespace AnastasisMicroEcology
{
namespace Detail
{
static uint32 MicroMix(uint32 Seed, int32 X, int32 Y, uint32 Salt)
{
	uint32 H = Seed ^ 0x9E3779B9u;
	H = (H ^ static_cast<uint32>(X)) * 0x85EBCA6Bu;
	H = (H ^ static_cast<uint32>(Y)) * 0xC2B2AE35u;
	H = (H ^ Salt) * 0x27D4EB2Fu;
	return H ^ (H >> 15);
}

static double MicroUnit(uint32 H) { return static_cast<double>(H) / 4294967296.0; }

static double MicroSmooth(double V)
{
	V = FMath::Clamp(V, 0.0, 1.0);
	return V * V * (3.0 - 2.0 * V);
}

static double MicroCluster(uint32 Seed, double X, double Y, uint32 Salt)
{
	const int32 IX = FMath::FloorToInt(X), IY = FMath::FloorToInt(Y);
	const double U = MicroSmooth(X - IX), V = MicroSmooth(Y - IY);
	return FMath::Lerp(
		FMath::Lerp(MicroUnit(MicroMix(Seed, IX, IY, Salt)), MicroUnit(MicroMix(Seed, IX + 1, IY, Salt)), U),
		FMath::Lerp(MicroUnit(MicroMix(Seed, IX, IY + 1, Salt)), MicroUnit(MicroMix(Seed, IX + 1, IY + 1, Salt)), U), V);
}

static bool FiniteSpan(double V) { return FMath::IsFinite(V) && V > 0.0; }

static bool ValidSettings(const FSettings& C)
{
	return FiniteSpan(C.BankCellUU) && FiniteSpan(C.MeadowCellUU) && FiniteSpan(C.ProbeUU)
		&& FiniteSpan(C.SoilCellUU) && FiniteSpan(C.BankPocketSpanUU) && FiniteSpan(C.MeadowPocketSpanUU)
		&& FiniteSpan(C.EdgePocketSpanUU) && FiniteSpan(C.UnderPocketSpanUU)
		&& C.BankMinAboveUU >= 0.0 && C.BankMaxAboveUU > C.BankMinAboveUU
		&& C.BankWetExtendUU >= C.BankMaxAboveUU && C.BankWetness >= 0.0 && C.BankWetness <= 1.0
		&& C.MeadowMinMask >= 0.0 && C.MeadowCanopyClear > 1.0
		&& C.MaxBankSlope > 0.0 && C.MaxVegSlope > 0.0 && C.MaxMeadowSlope > 0.0 && C.MaxForestSlope > 0.0
		&& C.MaxInstances > 0 && C.MaxInstances <= 200000;
}

struct FMicroCanopyIndex
{
	double Cell = 1.0;
	TMap<FIntPoint, TArray<int32>> Cells;
	const TArray<FVector>* Crowns = nullptr;

	void Init(const TArray<FVector>& In)
	{
		Crowns = &In;
		double MaxRadius = 100.0;
		for (const FVector& C : In) MaxRadius = FMath::Max(MaxRadius, C.Z);
		Cell = FMath::Max(100.0, MaxRadius * 2.6);
		for (int32 I = 0; I < In.Num(); ++I)
		{
			Cells.FindOrAdd(FIntPoint(FMath::FloorToInt(In[I].X / Cell), FMath::FloorToInt(In[I].Y / Cell))).Add(I);
		}
	}

	/** Distance a la couronne la plus proche, en rayons. Grand = a decouvert. */
	double Nearest(double X, double Y) const
	{
		if (!Crowns || Crowns->Num() == 0) return 1000.0;
		double Best = TNumericLimits<double>::Max();
		const int32 CX = FMath::FloorToInt(X / Cell), CY = FMath::FloorToInt(Y / Cell);
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				const TArray<int32>* Found = Cells.Find(FIntPoint(CX + DX, CY + DY));
				if (!Found) continue;
				for (const int32 I : *Found)
				{
					const FVector& C = (*Crowns)[I];
					if (C.Z <= 1.0) continue;
					Best = FMath::Min(Best, FVector2D::Distance(FVector2D(X, Y), FVector2D(C.X, C.Y)) / C.Z);
				}
			}
		}
		return Best;
	}
};

static bool InClearing(const FInputs& In, double X, double Y)
{
	const FVector2D P(X, Y);
	for (const FClearing& C : In.Clearings)
	{
		if (C.Radius > 0.0 && FVector2D::Distance(P, C.Center) <= C.Radius) return true;
	}
	return false;
}

static double WetnessAt(const FInputs& In, double X, double Y)
{
	if (!In.SampleWetness) return 0.0;
	double W = 0.0;
	if (!In.SampleWetness(X, Y, W) || !FMath::IsFinite(W)) return 0.0;
	return FMath::Clamp(W, 0.0, 1.0);
}

static bool SampleSlope(const FInputs& In, double X, double Y, double Probe, double& Z, double& Slope, FVector& Normal)
{
	auto Height = [&](double PX, double PY, double& Out)
	{
		return In.SampleHeight(PX, PY, Out) && FMath::IsFinite(Out);
	};
	double East = 0.0, West = 0.0, North = 0.0, South = 0.0;
	if (!Height(X, Y, Z) || !Height(X + Probe, Y, East) || !Height(X - Probe, Y, West)
		|| !Height(X, Y + Probe, North) || !Height(X, Y - Probe, South)) return false;
	const double DX = (East - West) / (2.0 * Probe);
	const double DY = (North - South) / (2.0 * Probe);
	Slope = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX * DX + DY * DY)));
	Normal = FVector(-DX, -DY, 1.0).GetSafeNormal();
	return Normal.IsNormalized() && FMath::IsFinite(Slope);
}

static bool IsBank(double Above, double Wetness, const FSettings& C)
{
	if (!(Above >= C.BankMinAboveUU)) return false;
	if (Above <= C.BankMaxAboveUU) return true;
	return Wetness >= C.BankWetness && Above <= C.BankWetExtendUU;
}

struct FStamp
{
	double X = 0.0;
	double Y = 0.0;
	EPocket Pocket = EPocket::None;
};

struct FRow
{
	TArray<FPlacement> Items;
	TArray<FStamp> Stamps;
	int32 Candidates = 0;
	int32 RejectedGround = 0;
	int32 RejectedWater = 0;
	int32 RejectedSlope = 0;
	int32 RejectedMask = 0;
	int32 RejectedClearing = 0;
	int32 RejectedDensity = 0;
};

static void Arm(FPlacement& P, uint32 Seed, int32 IX, int32 IY, ERole Role, EPocket Pocket, double ScaleMin, double ScaleMax, uint32 Salt)
{
	P.Role = Role;
	P.Pocket = Pocket;
	const uint32 H = MicroMix(Seed, IX, IY, Salt);
	P.Scale = ScaleMin + MicroUnit(H) * (ScaleMax - ScaleMin);
	P.Yaw = MicroUnit(MicroMix(Seed, IX, IY, Salt + 17u)) * 360.0;
}

static bool Consider(const FInputs& In, const FSettings& C, const FMicroCanopyIndex& Crowns, double X, double Y,
	int32 IX, int32 IY, bool bBankPass, FRow& Row)
{
	++Row.Candidates;
	if (!In.Bounds.IsInside(FVector2D(X, Y))) return false;
	if (InClearing(In, X, Y))
	{
		++Row.RejectedClearing;
		return false;
	}
	double Z = 0.0, Slope = 0.0;
	FVector Normal = FVector::UpVector;
	if (!SampleSlope(In, X, Y, C.ProbeUU, Z, Slope, Normal))
	{
		++Row.RejectedGround;
		return false;
	}
	double Above = 1.0e9;
	if (In.SampleWaterHeight)
	{
		double Water = 0.0;
		if (!In.SampleWaterHeight(X, Y, Water) || !FMath::IsFinite(Water))
		{
			++Row.RejectedWater;
			return false;
		}
		Above = Z - Water;
		if (Above < C.BankMinAboveUU)
		{
			++Row.RejectedWater;
			return false;
		}
	}
	const double Wetness = WetnessAt(In, X, Y);
	const bool bBank = In.SampleWaterHeight && IsBank(Above, Wetness, C);
	const double Crown = Crowns.Nearest(X, Y);
	const double Mask = In.Mask ? In.Mask(X, Y) : 1.0;
	if (!bBankPass && bBank) return false;

	FPlacement P;
	P.Ground = FVector(X, Y, Z);
	P.Normal = Normal;
	P.SlopeDegrees = Slope;
	P.AboveWater = Above;
	const double Gate = MicroUnit(MicroMix(In.Seed, IX, IY, bBankPass ? 11u : 29u));

	if (bBankPass)
	{
		if (!bBank) return false;
		if (Slope > C.MaxBankSlope + 12.0)
		{
			++Row.RejectedSlope;
			return false;
		}
		const EPocket Pocket = Slope > C.MaxBankSlope ? EPocket::Rocky : BankPocket(In.Seed, X, Y, C);
		Row.Stamps.Add(FStamp{X, Y, Pocket});
		const bool bPlants = Slope <= C.MaxVegSlope;
		switch (Pocket)
		{
		case EPocket::Clean:
			return false;
		case EPocket::Rocky:
			if (Gate > 0.62)
			{
				++Row.RejectedDensity;
				return false;
			}
			Arm(P, In.Seed, IX, IY, ERole::BankPebble, Pocket, 0.22, 0.52, 41u);
			break;
		case EPocket::Muddy:
			if (!bPlants || Gate > 0.10)
			{
				++Row.RejectedDensity;
				return false;
			}
			Arm(P, In.Seed, IX, IY, ERole::BankTuft, Pocket, 0.70, 1.15, 43u);
			break;
		case EPocket::Vegetated:
			if (!bPlants || Gate > 0.40)
			{
				++Row.RejectedDensity;
				return false;
			}
			Arm(P, In.Seed, IX, IY, Gate < 0.16 ? ERole::BankTuft : ERole::BankReed, Pocket,
				Gate < 0.16 ? 0.70 : 0.85, Gate < 0.16 ? 1.15 : 1.45, 47u);
			break;
		case EPocket::Drift:
			if (!bPlants || Gate > 0.22)
			{
				++Row.RejectedDensity;
				return false;
			}
			Arm(P, In.Seed, IX, IY, Gate < 0.08 ? ERole::BankBranch : ERole::BankDrift, Pocket,
				Gate < 0.08 ? 0.55 : 0.65, Gate < 0.08 ? 0.95 : 1.15, 53u);
			break;
		default:
			return false;
		}
		Row.Items.Add(P);
		return true;
	}

	if (Crown < C.MeadowCanopyClear)
	{
		return false;
	}
	if (!(Mask >= C.MeadowMinMask))
	{
		++Row.RejectedMask;
		return false;
	}
	if (Slope > C.MaxMeadowSlope)
	{
		++Row.RejectedSlope;
		return false;
	}
	const EPocket Pocket = MeadowPocket(In.Seed, X, Y, C);
	if (Pocket != EPocket::None) Row.Stamps.Add(FStamp{X, Y, Pocket});
	switch (Pocket)
	{
	case EPocket::MeadowStone:
		if (Gate > 0.70)
		{
			++Row.RejectedDensity;
			return false;
		}
		if (Gate < 0.12) Arm(P, In.Seed, IX, IY, ERole::MeadowBush, Pocket, 0.80, 1.35, 61u);
		else Arm(P, In.Seed, IX, IY, ERole::MeadowStone, Pocket, 0.30, 0.68, 61u);
		break;
	case EPocket::MeadowBare:
		if (Gate > 0.08)
		{
			++Row.RejectedDensity;
			return false;
		}
		Arm(P, In.Seed, IX, IY, ERole::MeadowStone, Pocket, 0.22, 0.42, 67u);
		break;
	case EPocket::MeadowDry:
		if (Gate > 0.04)
		{
			++Row.RejectedDensity;
			return false;
		}
		Arm(P, In.Seed, IX, IY, ERole::MeadowBush, Pocket, 0.75, 1.20, 71u);
		break;
	case EPocket::MeadowWet:
		return false;
	default:
		return false;
	}
	Row.Items.Add(P);
	return true;
}

static void ScanGrid(const FInputs& In, const FSettings& C, const FMicroCanopyIndex& Crowns, double Cell, bool bBankPass, FRow& Accum)
{
	const int32 MinX = FMath::FloorToInt(In.Bounds.Min.X / Cell);
	const int32 MaxX = FMath::FloorToInt((In.Bounds.Max.X - 0.01) / Cell);
	const int32 MinY = FMath::FloorToInt(In.Bounds.Min.Y / Cell);
	const int32 MaxY = FMath::FloorToInt((In.Bounds.Max.Y - 0.01) / Cell);
	if (MinX > MaxX || MinY > MaxY) return;
	const int32 RowsN = MaxY - MinY + 1;
	TArray<FRow> Rows;
	Rows.SetNum(RowsN);
	ParallelFor(RowsN, [&](int32 RowIndex)
	{
		const int32 IY = MinY + RowIndex;
		FRow& Row = Rows[RowIndex];
		for (int32 IX = MinX; IX <= MaxX; ++IX)
		{
			const double Jx = (MicroUnit(MicroMix(In.Seed, IX, IY, bBankPass ? 3u : 5u)) - 0.5) * Cell * 0.62;
			const double Jy = (MicroUnit(MicroMix(In.Seed, IX, IY, bBankPass ? 4u : 6u)) - 0.5) * Cell * 0.62;
			Consider(In, C, Crowns, (IX + 0.5) * Cell + Jx, (IY + 0.5) * Cell + Jy, IX, IY, bBankPass, Row);
		}
	});
	for (const FRow& Row : Rows)
	{
		Accum.Items.Append(Row.Items);
		Accum.Stamps.Append(Row.Stamps);
		Accum.Candidates += Row.Candidates;
		Accum.RejectedGround += Row.RejectedGround;
		Accum.RejectedWater += Row.RejectedWater;
		Accum.RejectedSlope += Row.RejectedSlope;
		Accum.RejectedMask += Row.RejectedMask;
		Accum.RejectedClearing += Row.RejectedClearing;
		Accum.RejectedDensity += Row.RejectedDensity;
	}
}

static uint64 CellKey(double X, double Y, double Cell)
{
	const uint32 HX = static_cast<uint32>(FMath::FloorToInt(X / Cell));
	const uint32 HY = static_cast<uint32>(FMath::FloorToInt(Y / Cell));
	return (static_cast<uint64>(HX) << 32) | static_cast<uint64>(HY);
}

static void AddForest(const FInputs& In, const FSettings& C, const FMicroCanopyIndex& Crowns, FRow& Row)
{
	TSet<uint64> Seen;
	auto Try = [&](double X, double Y, bool bEdge, double RingT) -> bool
	{
		if (!In.Bounds.IsInside(FVector2D(X, Y)) || InClearing(In, X, Y)) return false;
		const uint64 Key = CellKey(X, Y, 280.0);
		if (Seen.Contains(Key)) return false;
		double Z = 0.0, Slope = 0.0;
		FVector Normal = FVector::UpVector;
		if (!SampleSlope(In, X, Y, C.ProbeUU, Z, Slope, Normal)) return false;
		if (Slope > C.MaxForestSlope) return false;
		double Above = 1.0e9;
		if (In.SampleWaterHeight)
		{
			double Water = 0.0;
			if (!In.SampleWaterHeight(X, Y, Water) || !FMath::IsFinite(Water)) return false;
			Above = Z - Water;
			if (Above < C.BankMinAboveUU || IsBank(Above, WetnessAt(In, X, Y), C)) return false;
		}
		const double Crown = Crowns.Nearest(X, Y);
		if (bEdge)
		{
			if (Crown < 1.02 || Crown > 2.45) return false;
			if (MicroCluster(In.Seed ^ 0xE6u, X / C.EdgePocketSpanUU, Y / C.EdgePocketSpanUU, 81u) < 0.64) return false;
		}
		else
		{
			if (Crown < 0.22 || Crown > 0.92) return false;
			const double N = MicroCluster(In.Seed ^ 0x0Du, X / C.UnderPocketSpanUU, Y / C.UnderPocketSpanUU, 83u);
			if (N < 0.38 || N >= 0.74) return false;
		}
		Seen.Add(Key);
		FPlacement P;
		P.Ground = FVector(X, Y, Z);
		P.Normal = Normal;
		P.SlopeDegrees = Slope;
		P.AboveWater = Above;
		const int32 IX = FMath::FloorToInt(X), IY = FMath::FloorToInt(Y);
		const double Pick = MicroUnit(MicroMix(In.Seed, IX, IY, bEdge ? 91u : 93u));
		if (bEdge)
		{
			const bool bInner = RingT < 1.55;
			const bool bSapling = bInner ? Pick < 0.72 : Pick >= 0.82;
			if (bSapling) Arm(P, In.Seed, IX, IY, ERole::EdgeSapling, EPocket::None, 0.55, 1.00, 97u);
			else Arm(P, In.Seed, IX, IY, ERole::EdgeBush, EPocket::None, 0.80, 1.40, 97u);
		}
		else
		{
			const double N = MicroCluster(In.Seed ^ 0x0Du, X / C.UnderPocketSpanUU, Y / C.UnderPocketSpanUU, 83u);
			if (N < 0.62)
			{
				if (Pick < 0.28) Arm(P, In.Seed, IX, IY, ERole::UnderLog, EPocket::None, 0.85, 1.25, 101u);
				else if (Pick < 0.52) Arm(P, In.Seed, IX, IY, ERole::UnderStump, EPocket::None, 0.70, 1.05, 103u);
				else if (Pick < 0.78) Arm(P, In.Seed, IX, IY, ERole::UnderBranch, EPocket::None, 0.55, 0.95, 107u);
				else Arm(P, In.Seed, IX, IY, ERole::UnderRoots, EPocket::None, 0.70, 1.10, 109u);
			}
			else Arm(P, In.Seed, IX, IY, ERole::UnderSapling, EPocket::None, 0.50, 0.90, 113u);
		}
		Row.Items.Add(P);
		return true;
	};

	for (const FVector& Crown : In.Canopy)
	{
		if (Crown.Z < 80.0) continue;
		const double Spin = MicroUnit(MicroMix(In.Seed, FMath::FloorToInt(Crown.X), FMath::FloorToInt(Crown.Y), 3u));
		for (double T = 0.30; T <= 0.80; T += 0.25)
		{
			const double Rad = Crown.Z * T;
			const int32 Steps = FMath::Max(6, FMath::CeilToInt(2.0 * PI * Rad / 340.0));
			for (int32 I = 0; I < Steps; ++I)
			{
				const double Ang = (static_cast<double>(I) + Spin) / static_cast<double>(Steps) * 2.0 * PI;
				Try(Crown.X + FMath::Cos(Ang) * Rad, Crown.Y + FMath::Sin(Ang) * Rad, false, T);
			}
		}
		for (double T = 1.12; T <= 2.28; T += 0.36)
		{
			const double Rad = Crown.Z * T;
			const int32 Steps = FMath::Max(8, FMath::CeilToInt(2.0 * PI * Rad / 340.0));
			for (int32 I = 0; I < Steps; ++I)
			{
				const double Ang = (static_cast<double>(I) + Spin) / static_cast<double>(Steps) * 2.0 * PI;
				Try(Crown.X + FMath::Cos(Ang) * Rad, Crown.Y + FMath::Sin(Ang) * Rad, true, T);
			}
		}
	}
}

static bool IsShorePocket(EPocket Pocket)
{
	const uint8 Value = static_cast<uint8>(Pocket);
	return Value >= static_cast<uint8>(EPocket::Clean) && Value <= static_cast<uint8>(EPocket::Drift);
}

static void RasterSoil(const FInputs& In, const FSettings& C, const TArray<FStamp>& Shore, const TArray<FStamp>& Meadow, FPlan& Out)
{
	FSoilField& Field = Out.Soil;
	Field.Origin = In.Bounds.Min;
	Field.CellUU = C.SoilCellUU;
	Field.W = FMath::Max(1, FMath::CeilToInt((In.Bounds.Max.X - In.Bounds.Min.X) / C.SoilCellUU));
	Field.H = FMath::Max(1, FMath::CeilToInt((In.Bounds.Max.Y - In.Bounds.Min.Y) / C.SoilCellUU));
	Field.Cells.Init(static_cast<uint8>(EPocket::None), Field.W * Field.H);
	auto Write = [&](const FStamp& S, bool bShore)
	{
		const int32 SX = FMath::FloorToInt((S.X - Field.Origin.X) / Field.CellUU);
		const int32 SY = FMath::FloorToInt((S.Y - Field.Origin.Y) / Field.CellUU);
		if (SX < 0 || SY < 0 || SX >= Field.W || SY >= Field.H) return;
		uint8& Cell = Field.Cells[SY * Field.W + SX];
		if (bShore || Cell == static_cast<uint8>(EPocket::None) || !IsShorePocket(static_cast<EPocket>(Cell)))
		{
			Cell = static_cast<uint8>(S.Pocket);
		}
	};
	for (const FStamp& S : Shore) Write(S, true);
	for (const FStamp& S : Meadow) Write(S, false);
	for (int32 I = 0; I < PocketCount; ++I) Out.PocketCells[I] = 0;
	for (const uint8 Cell : Field.Cells) ++Out.PocketCells[Cell];
}

struct FLook
{
	AnastasisPlaces::EFamily Family;
	bool bStone = false;
	bool bShadow = false;
	int32 FadeStart = 2800;
	int32 FadeEnd = 4500;
};

static FLook LookFor(ERole Role)
{
	using AnastasisPlaces::EFamily;
	switch (Role)
	{
	case ERole::BankPebble: return {EFamily::RockLow, true, false, 2200, 4200};
	case ERole::BankReed: return {EFamily::Reed, false, false, 3200, 6200};
	case ERole::BankTuft: return {EFamily::ShoreTuft, false, false, 2500, 4800};
	case ERole::BankDrift: return {EFamily::Driftwood, false, false, 3000, 5600};
	case ERole::BankBranch: return {EFamily::BranchPile, false, false, 2500, 4800};
	case ERole::MeadowStone: return {EFamily::RockLow, true, false, 2200, 4200};
	case ERole::MeadowBush: return {EFamily::BushLow, false, true, 4000, 7200};
	case ERole::EdgeBush: return {EFamily::BushLow, false, true, 4000, 7500};
	case ERole::EdgeSapling: return {EFamily::Sapling, false, true, 4200, 7800};
	case ERole::UnderLog: return {EFamily::FallenLog, false, true, 3800, 7000};
	case ERole::UnderStump: return {EFamily::Stump, false, true, 3200, 6000};
	case ERole::UnderBranch: return {EFamily::BranchPile, false, false, 2500, 4800};
	case ERole::UnderRoots: return {EFamily::ExposedRoots, false, false, 2500, 4800};
	case ERole::UnderSapling: return {EFamily::Sapling, false, true, 4000, 7200};
	default: return {EFamily::RockLow, true, false, 2200, 4200};
	}
}
}

const TCHAR* RoleName(ERole Role)
{
	switch (Role)
	{
	case ERole::BankPebble: return TEXT("pebble");
	case ERole::BankReed: return TEXT("reed");
	case ERole::BankTuft: return TEXT("tuft");
	case ERole::BankDrift: return TEXT("drift");
	case ERole::BankBranch: return TEXT("branch");
	case ERole::MeadowStone: return TEXT("stone");
	case ERole::MeadowBush: return TEXT("bush");
	case ERole::EdgeBush: return TEXT("edge_bush");
	case ERole::EdgeSapling: return TEXT("edge_sapling");
	case ERole::UnderLog: return TEXT("log");
	case ERole::UnderStump: return TEXT("stump");
	case ERole::UnderBranch: return TEXT("under_branch");
	case ERole::UnderRoots: return TEXT("roots");
	case ERole::UnderSapling: return TEXT("under_sapling");
	default: return TEXT("?");
	}
}

const TCHAR* PocketName(EPocket Pocket)
{
	switch (Pocket)
	{
	case EPocket::None: return TEXT("none");
	case EPocket::Clean: return TEXT("clean");
	case EPocket::Rocky: return TEXT("rocky");
	case EPocket::Muddy: return TEXT("muddy");
	case EPocket::Vegetated: return TEXT("vegetated");
	case EPocket::Drift: return TEXT("drift");
	case EPocket::MeadowBare: return TEXT("bare");
	case EPocket::MeadowDry: return TEXT("dry");
	case EPocket::MeadowStone: return TEXT("stone");
	case EPocket::MeadowWet: return TEXT("wet");
	default: return TEXT("?");
	}
}

EPocket BankPocket(uint32 Seed, double X, double Y, const FSettings& Settings)
{
	const double Span = Settings.BankPocketSpanUU;
	const double Broad = Detail::MicroCluster(Seed, X / Span, Y / Span, 91u);
	const double Break = Detail::MicroCluster(Seed ^ 0x51u, X / (Span * 0.28), Y / (Span * 0.46), 92u);
	const double N = Broad * 0.58 + Break * 0.42;
	if (N < 0.30) return EPocket::Clean;
	if (N < 0.46) return EPocket::Rocky;
	if (N < 0.62) return EPocket::Muddy;
	if (N < 0.78) return EPocket::Vegetated;
	return EPocket::Drift;
}

EPocket MeadowPocket(uint32 Seed, double X, double Y, const FSettings& Settings)
{
	const double Span = Settings.MeadowPocketSpanUU;
	const double Broad = Detail::MicroCluster(Seed ^ 0xA5u, X / Span, Y / Span, 71u);
	const double Break = Detail::MicroCluster(Seed ^ 0x3Cu, X / (Span * 0.45), Y / (Span * 0.45), 73u);
	const double N = Broad * 0.65 + Break * 0.35;
	if (N < 0.58) return EPocket::None;
	if (N < 0.72) return EPocket::MeadowBare;
	if (N < 0.82) return EPocket::MeadowDry;
	if (N < 0.88) return EPocket::MeadowStone;
	return EPocket::MeadowWet;
}

EPocket FSoilField::Sample(double X, double Y) const
{
	if (!IsValid()) return EPocket::None;
	const int32 SX = FMath::FloorToInt((X - Origin.X) / CellUU);
	const int32 SY = FMath::FloorToInt((Y - Origin.Y) / CellUU);
	if (SX < 0 || SY < 0 || SX >= W || SY >= H) return EPocket::None;
	return static_cast<EPocket>(Cells[SY * W + SX]);
}

bool Build(const FInputs& In, const FSettings& Settings, FPlan& Out, FString& OutError)
{
	Out = FPlan();
	if (!Detail::ValidSettings(Settings))
	{
		OutError = TEXT("micro ecology: invalid settings");
		return false;
	}
	if (!In.SampleHeight)
	{
		OutError = TEXT("micro ecology: no ground sampler");
		return false;
	}
	if (!In.Mask)
	{
		OutError = TEXT("micro ecology: no open-ground mask");
		return false;
	}
	if (!In.Bounds.bIsValid || !FMath::IsFinite(In.Bounds.Min.X) || !FMath::IsFinite(In.Bounds.Max.Y)
		|| In.Bounds.Max.X <= In.Bounds.Min.X || In.Bounds.Max.Y <= In.Bounds.Min.Y)
	{
		OutError = TEXT("micro ecology: empty or non-finite bounds");
		return false;
	}
	for (const FVector& Crown : In.Canopy)
	{
		if (!FMath::IsFinite(Crown.X) || !FMath::IsFinite(Crown.Y) || !FMath::IsFinite(Crown.Z) || Crown.Z < 0.0)
		{
			OutError = TEXT("micro ecology: non-finite crown");
			return false;
		}
	}

	Detail::FMicroCanopyIndex Crowns;
	Crowns.Init(In.Canopy);
	Detail::FRow Bank, Meadow, Forest;
	Detail::ScanGrid(In, Settings, Crowns, Settings.BankCellUU, true, Bank);
	Detail::ScanGrid(In, Settings, Crowns, Settings.MeadowCellUU, false, Meadow);
	Detail::AddForest(In, Settings, Crowns, Forest);

	Out.Instances.Reserve(Bank.Items.Num() + Meadow.Items.Num() + Forest.Items.Num());
	Out.Instances.Append(Bank.Items);
	Out.Instances.Append(Meadow.Items);
	Out.Instances.Append(Forest.Items);
	Out.Candidates = Bank.Candidates + Meadow.Candidates;
	Out.RejectedGround = Bank.RejectedGround + Meadow.RejectedGround;
	Out.RejectedWater = Bank.RejectedWater + Meadow.RejectedWater;
	Out.RejectedSlope = Bank.RejectedSlope + Meadow.RejectedSlope;
	Out.RejectedMask = Bank.RejectedMask + Meadow.RejectedMask;
	Out.RejectedClearing = Bank.RejectedClearing + Meadow.RejectedClearing;
	Out.RejectedDensity = Bank.RejectedDensity + Meadow.RejectedDensity;

	if (Out.Instances.Num() > Settings.MaxInstances)
	{
		const double Keep = static_cast<double>(Settings.MaxInstances) / static_cast<double>(Out.Instances.Num());
		TArray<FPlacement> Kept;
		Kept.Reserve(Settings.MaxInstances);
		for (int32 I = 0; I < Out.Instances.Num(); ++I)
		{
			if (Detail::MicroUnit(Detail::MicroMix(In.Seed, I, 0, 77u)) < Keep) Kept.Add(Out.Instances[I]);
		}
		Out.Instances = MoveTemp(Kept);
		Out.bTruncated = true;
	}
	for (const FPlacement& P : Out.Instances)
	{
		++Out.Counts[static_cast<int32>(P.Role)];
	}
	Detail::RasterSoil(In, Settings, Bank.Stamps, Meadow.Stamps, Out);
	return true;
}

FLinearColor TintSoil(const FLinearColor& Base, EPocket Pocket, double Strength)
{
	FLinearColor Target = Base;
	double Amount = 0.0;
	switch (Pocket)
	{
	case EPocket::Clean:
		Target = FLinearColor(0.075f, 0.092f, 0.040f);
		Amount = 0.22;
		break;
	case EPocket::Rocky:
		Target = FLinearColor(0.105f, 0.101f, 0.096f);
		Amount = 0.36;
		break;
	case EPocket::Muddy:
		Target = FLinearColor(0.042f, 0.034f, 0.024f);
		Amount = 0.50;
		break;
	case EPocket::MeadowBare:
		Target = FLinearColor(0.098f, 0.072f, 0.042f);
		Amount = 0.32;
		break;
	case EPocket::MeadowDry:
		Target = FLinearColor(0.086f, 0.080f, 0.040f);
		Amount = 0.14;
		break;
	case EPocket::MeadowStone:
		Target = FLinearColor(0.090f, 0.082f, 0.064f);
		Amount = 0.18;
		break;
	case EPocket::MeadowWet:
		Target = FLinearColor(0.040f, 0.055f, 0.032f);
		Amount = 0.24;
		break;
	default:
		return Base;
	}
	FLinearColor Out = FMath::Lerp(Base, Target, static_cast<float>(FMath::Clamp(Amount * Strength, 0.0, 1.0)));
	Out.A = Base.A;
	return Out;
}

FEmbodyResult Embody(AActor& Owner, const FPlan& Plan, UMaterialInterface* ShapeMaterial,
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& InOutComponents)
{
	TMap<FName, UHierarchicalInstancedStaticMeshComponent*> Existing;
	for (UHierarchicalInstancedStaticMeshComponent* C : InOutComponents)
	{
		if (!IsValid(C)) continue;
		C->ClearInstances();
		Existing.Add(C->GetFName(), C);
	}
	InOutComponents.RemoveAll([](const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& C) { return !IsValid(C); });

	FEmbodyResult Result;
	UMaterialInstanceDynamic* Stone = nullptr;
	if (ShapeMaterial)
	{
		Stone = UMaterialInstanceDynamic::Create(ShapeMaterial, &Owner);
		Stone->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.105f, 0.101f, 0.096f, 1.0f));
	}
	TMap<FString, UStaticMesh*> Meshes;
	constexpr double ChunkUU = 16000.0;
	TMap<uint64, TArray<FTransform>> Batches;
	TMap<uint64, Detail::FLook> Looks;
	TMap<uint64, UStaticMesh*> BatchMesh;
	auto KeyOf = [](int32 Role, int32 Variant, int32 CX, int32 CY)
	{
		return (static_cast<uint64>(Role * 8 + Variant) << 32)
			| (static_cast<uint64>(CX & 0xFFFF) << 16)
			| static_cast<uint64>(CY & 0xFFFF);
	};
	for (const FPlacement& P : Plan.Instances)
	{
		const Detail::FLook Look = Detail::LookFor(P.Role);
		const int32 Variants = FMath::Max(1, AnastasisPlaces::VariantCount(Look.Family));
		const int32 Variant = FMath::Clamp(FMath::FloorToInt(Detail::MicroUnit(Detail::MicroMix(0xC0FFEEu,
			FMath::FloorToInt(P.Ground.X), FMath::FloorToInt(P.Ground.Y), 5u)) * Variants), 0, Variants - 1);
		const FString Path = AnastasisPlaces::MeshPath(Look.Family, Variant);
		UStaticMesh* Mesh = Meshes.FindRef(Path);
		if (!Mesh && !Meshes.Contains(Path))
		{
			Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
			Meshes.Add(Path, Mesh);
			if (!Mesh) ++Result.MissingMeshes;
		}
		if (!Mesh) continue;
		const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(P.Yaw));
		const int32 CX = FMath::FloorToInt(P.Ground.X / ChunkUU), CY = FMath::FloorToInt(P.Ground.Y / ChunkUU);
		const uint64 Key = KeyOf(static_cast<int32>(P.Role), Variant, CX, CY);
		Batches.FindOrAdd(Key).Add(FTransform(Rotation, P.Ground - FVector(0, 0, 6.0), FVector(P.Scale)));
		Looks.FindOrAdd(Key) = Look;
		BatchMesh.FindOrAdd(Key) = Mesh;
	}
	for (TPair<uint64, TArray<FTransform>>& Batch : Batches)
	{
		const int32 Packed = static_cast<int32>(Batch.Key >> 32);
		const int32 Role = Packed / 8, Variant = Packed % 8;
		const int32 CX = static_cast<int16>((Batch.Key >> 16) & 0xFFFF), CY = static_cast<int16>(Batch.Key & 0xFFFF);
		const Detail::FLook Look = Looks.FindRef(Batch.Key);
		const FName Name(*FString::Printf(TEXT("MicroEco_%s_v%d_%d_%d"), RoleName(static_cast<ERole>(Role)), Variant, CX, CY));
		UHierarchicalInstancedStaticMeshComponent* Made = Existing.FindRef(Name);
		if (!Made)
		{
			Made = NewObject<UHierarchicalInstancedStaticMeshComponent>(&Owner,
				MakeUniqueObjectName(&Owner, UHierarchicalInstancedStaticMeshComponent::StaticClass(), Name));
			Made->SetFlags(RF_Transient);
			Made->SetupAttachment(Owner.GetRootComponent());
			Made->SetMobility(EComponentMobility::Movable);
			Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Made->SetGenerateOverlapEvents(false);
			Made->SetCanEverAffectNavigation(false);
			Made->RegisterComponent();
			InOutComponents.Add(Made);
		}
		Made->SetStaticMesh(BatchMesh.FindRef(Batch.Key));
		Made->SetCastShadow(Look.bShadow);
		Made->SetCullDistances(Look.FadeStart, Look.FadeEnd);
		if (Look.bStone && Stone) Made->SetMaterial(0, Stone);
		Made->AddInstances(Batch.Value, false, false, false);
		Made->MarkRenderStateDirty();
		Result.Instances += Batch.Value.Num();
		++Result.Components;
	}
	return Result;
}

int32 ApplySoil(UProceduralMeshComponent* Surface, const FSoilField& Field, double Strength)
{
	if (!Surface || !Field.IsValid() || !(Strength > 0.0)) return 0;
	FProcMeshSection* Section = Surface->GetProcMeshSection(0);
	if (!Section) return 0;
	const int32 N = Section->ProcVertexBuffer.Num();
	TArray<FColor> Colors;
	TArray<FVector> Positions;
	Colors.SetNumUninitialized(N);
	Positions.SetNumUninitialized(N);
	int32 Tinted = 0;
	for (int32 I = 0; I < N; ++I)
	{
		const FProcMeshVertex& V = Section->ProcVertexBuffer[I];
		Colors[I] = V.Color;
		Positions[I] = V.Position;
		if (V.Color.A > 200) continue;
		const EPocket Pocket = Field.Sample(V.Position.X, V.Position.Y);
		if (Pocket == EPocket::None || Pocket == EPocket::Vegetated || Pocket == EPocket::Drift) continue;
		const FLinearColor Next = TintSoil(V.Color.ReinterpretAsLinear(), Pocket, Strength);
		Colors[I] = Next.ToFColor(false);
		++Tinted;
	}
	if (Tinted > 0)
	{
		Surface->UpdateMeshSection(0, Positions, TArray<FVector>(), TArray<FVector2D>(),
			TArray<FVector2D>(), TArray<FVector2D>(), TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>());
	}
	return Tinted;
}
}
