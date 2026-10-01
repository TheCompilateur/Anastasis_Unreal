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
		if (!FMath::IsFinite(Candidate.Ground.X) || !FMath::IsFinite(Candidate.Ground.Y) || !FMath::IsFinite(Candidate.Score))
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
	// One shell per 60 m stand. Fewer than six trunks is a tree, not a mass.
	constexpr double CellCm = 6000.0;
	constexpr int32 MinTrees = 6;
	constexpr int32 ShellCap = 240;
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
	for (const TPair<uint64, TArray<int32>>& Pair : Cells) Keys.Add(Pair.Key);
	Keys.Sort();
	for (uint64 Key : Keys)
	{
		const TArray<int32>& Ids = Cells[Key];
		if (Ids.Num() < MinTrees) continue;
		FVector2D Sum = FVector2D::ZeroVector;
		for (int32 Id : Ids) Sum += Candidates[Id].Ground;
		const FVector2D Center = Sum / static_cast<double>(Ids.Num());
		double MaxDistance = 0.0;
		for (int32 Id : Ids) MaxDistance = FMath::Max(MaxDistance, FVector2D::Distance(Center, Candidates[Id].Ground));
		FShell Shell;
		Shell.Center = Center;
		Shell.RadiusCm = FMath::Max(1200.0, MaxDistance * 1.35);
		Shell.Trees = Ids.Num();
		Shells.Add(Shell);
		if (Shells.Num() >= ShellCap) break;
	}
	Report.Heroes = Heroes.Num();
	Report.Shells = Shells.Num();
	return true;
}
}
