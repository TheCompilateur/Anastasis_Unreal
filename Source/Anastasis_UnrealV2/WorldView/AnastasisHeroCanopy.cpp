#include "WorldView/AnastasisHeroCanopy.h"

namespace AnastasisHeroCanopy
{
namespace
{
bool IsHeroSpecies(uint8 Species)
{
	return Species >= 1 && Species <= 4;
}
}

bool Build(const TArray<FCandidate>& Candidates, TArray<FHero>& Heroes, TArray<FShell>& Shells, FReport& Report, FString& Error)
{
	Heroes.Reset();
	Shells.Reset();
	Report = FReport{};
	Error.Reset();
	Report.Candidates = Candidates.Num();
	for (const FCandidate& Candidate : Candidates)
	{
		if (!FMath::IsFinite(Candidate.Ground.X) || !FMath::IsFinite(Candidate.Ground.Y) || !FMath::IsFinite(Candidate.Score)
			|| !FMath::IsFinite(Candidate.GroundZ) || !FMath::IsFinite(Candidate.HeightCm) || !FMath::IsFinite(Candidate.Dryness))
		{
			Error = TEXT("HeroCanopy: non-finite candidate");
			Report = FReport{};
			return false;
		}
	}
	TArray<int32> Order;
	Order.Reserve(Candidates.Num());
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (IsHeroSpecies(Candidates[Index].Species) && Candidates[Index].Score >= 6.0)
		{
			Order.Add(Index);
		}
	}
	Order.Sort([&Candidates](int32 A, int32 B)
	{
		if (Candidates[A].Score != Candidates[B].Score) return Candidates[A].Score > Candidates[B].Score;
		return A < B;
	});
	// Eight specimens, far enough apart that a valley does not become a hero grove.
	constexpr double SpacingCm = 2200.0;
	constexpr int32 HeroCap = 8;
	for (int32 Index : Order)
	{
		bool bClear = true;
		for (const FHero& Chosen : Heroes)
		{
			if (FVector2D::Distance(Candidates[Index].Ground, Candidates[Chosen.Index].Ground) < SpacingCm)
			{
				bClear = false;
				break;
			}
		}
		if (!bClear) continue;
		FHero Hero;
		Hero.Index = Index;
		Hero.Species = Candidates[Index].Species;
		Heroes.Add(Hero);
		if (Heroes.Num() >= HeroCap) break;
	}
	// One shell per 30 m stand. Under four trunks (about 45 a hectare) it is a few
	// trees, not a mass. The shell fills crowns that are already there: a footprint
	// wider than the trunks, or a top above them, reads as a green cloud over the
	// forest instead of the forest itself.
	constexpr double CellCm = 3000.0;
	constexpr int32 MinTrees = 4;
	constexpr int32 ShellCap = 600;
	constexpr double CrownMarginCm = 250.0;
	constexpr double MinRadiusCm = 600.0;
	constexpr double MaxRadiusCm = 1800.0;
	constexpr double CrownBase = 0.4;
	constexpr double TopBelowMedian = 0.92;
	TMap<uint64, TArray<int32>> Cells;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const int32 CX = FMath::FloorToInt(Candidates[Index].Ground.X / CellCm);
		const int32 CY = FMath::FloorToInt(Candidates[Index].Ground.Y / CellCm);
		const uint64 Key = (static_cast<uint64>(static_cast<uint32>(CX)) << 32) | static_cast<uint32>(CY);
		Cells.FindOrAdd(Key).Add(Index);
	}
	TArray<uint64> Keys;
	Keys.Reserve(Cells.Num());
	for (const TPair<uint64, TArray<int32>>& Pair : Cells)
	{
		if (Pair.Value.Num() >= MinTrees) Keys.Add(Pair.Key);
	}
	Report.Stands = Keys.Num();
	// Past the cap the densest stands keep their shell, wherever they are on the map.
	Keys.Sort([&Cells](uint64 A, uint64 B)
	{
		const int32 NA = Cells[A].Num(), NB = Cells[B].Num();
		if (NA != NB) return NA > NB;
		return A < B;
	});
	for (uint64 Key : Keys)
	{
		const TArray<int32>& Ids = Cells[Key];
		TArray<double> Heights;
		FVector2D Sum = FVector2D::ZeroVector;
		double GroundSum = 0.0;
		double DrySum = 0.0;
		for (int32 Id : Ids)
		{
			Sum += Candidates[Id].Ground;
			GroundSum += Candidates[Id].GroundZ;
			DrySum += Candidates[Id].Dryness;
			if (Candidates[Id].HeightCm > 0.0) Heights.Add(Candidates[Id].HeightCm);
		}
		if (Heights.Num() < MinTrees) continue;
		const FVector2D Center = Sum / static_cast<double>(Ids.Num());
		// Half the trunks of a disc of radius R lie within R / sqrt(2). The median, not the
		// farthest trunk: one stray tree at the cell's corner does not stretch the shell.
		TArray<double> Distances;
		Distances.Reserve(Ids.Num());
		for (int32 Id : Ids) Distances.Add(FVector2D::Distance(Center, Candidates[Id].Ground));
		Distances.Sort();
		const double HalfRadius = Distances[Distances.Num() / 2];
		Heights.Sort();
		const double Median = Heights[Heights.Num() / 2];
		FShell Shell;
		Shell.Center = Center;
		Shell.RadiusCm = FMath::Clamp(HalfRadius * UE_SQRT_2 + CrownMarginCm, MinRadiusCm, MaxRadiusCm);
		Shell.GroundZ = GroundSum / static_cast<double>(Ids.Num());
		Shell.TopCm = Median * TopBelowMedian;
		Shell.BaseCm = Median * CrownBase;
		Shell.Dryness = FMath::Clamp(DrySum / static_cast<double>(Ids.Num()), 0.0, 1.0);
		Shell.Trees = Ids.Num();
		Shells.Add(Shell);
		if (Shells.Num() >= ShellCap) break;
	}
	Report.Heroes = Heroes.Num();
	Report.Shells = Shells.Num();
	return true;
}
}
