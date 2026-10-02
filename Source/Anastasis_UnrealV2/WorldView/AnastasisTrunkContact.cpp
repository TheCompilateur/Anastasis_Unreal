#include "WorldView/AnastasisTrunkContact.h"

namespace AnastasisTrunkContact
{
namespace
{
uint32 Hash(uint32 Seed, int32 Index, uint32 Salt)
{
	uint32 H = Seed ^ 0x9E3779B9u;
	H = (H ^ static_cast<uint32>(Index)) * 0x85EBCA6Bu;
	H = (H ^ Salt) * 0xC2B2AE35u;
	return H ^ (H >> 15);
}

double Unit(uint32 H) { return static_cast<double>(H) / 4294967296.0; }
}

bool Build(const TArray<FVector>& Trunks, uint32 Seed, TArray<FPatch>& Out, FReport& Report, FString& Error)
{
	Out.Reset();
	Report = FReport{};
	Error.Reset();
	Report.Trunks = Trunks.Num();
	for (const FVector& Trunk : Trunks)
	{
		if (!FMath::IsFinite(Trunk.X) || !FMath::IsFinite(Trunk.Y) || !FMath::IsFinite(Trunk.Z))
		{
			Error = TEXT("TrunkContact: non-finite trunk");
			Out.Reset();
			Report = FReport{};
			return false;
		}
	}
	// A crown under 80 cm is not a trunk worth a skirt. Four candidates, about
	// two kept, sitting 40–140 cm out so the ring is the foot, not the drip line.
	constexpr int32 Candidates = 4;
	constexpr int32 Cap = 6000;
	TArray<FPatch> Built;
	Built.Reserve(FMath::Min(Trunks.Num() * 2, Cap));
	for (int32 TrunkIndex = 0; TrunkIndex < Trunks.Num(); ++TrunkIndex)
	{
		const FVector& Trunk = Trunks[TrunkIndex];
		if (Trunk.Z < 80.0) continue;
		for (int32 Candidate = 0; Candidate < Candidates; ++Candidate)
		{
			const uint32 H = Hash(Seed, TrunkIndex * Candidates + Candidate, 0xC07Au);
			if (Unit(H) > 0.55) continue;
			const double Angle = Unit(Hash(Seed, TrunkIndex, 0xC07Bu + Candidate)) * 2.0 * PI;
			const double Distance = 40.0 + Unit(Hash(Seed, TrunkIndex, 0xC07Cu + Candidate)) * 100.0;
			FPatch Patch;
			Patch.Position = FVector2D(Trunk.X, Trunk.Y) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
			Patch.YawDegrees = Unit(Hash(Seed, TrunkIndex, 0xC07Du + Candidate)) * 360.0;
			const bool bMoss = Unit(Hash(Seed, TrunkIndex, 0xC07Eu + Candidate)) > 0.45;
			Patch.Kind = bMoss ? 1 : 0;
			// Litter lies flat and wide. Moss stays a low pad, not a tuft of meadow.
			Patch.ScaleXY = bMoss ? 0.42 + Unit(H) * 0.18 : 0.62 + Unit(H) * 0.28;
			Patch.ScaleZ = bMoss ? 0.14 + Unit(H) * 0.08 : 0.08 + Unit(H) * 0.05;
			Patch.Trunk = TrunkIndex;
			Built.Add(Patch);
			if (Built.Num() >= Cap) break;
		}
		if (Built.Num() >= Cap) break;
	}
	for (const FPatch& Patch : Built)
	{
		Report.Litter += Patch.Kind == 0;
		Report.Moss += Patch.Kind == 1;
	}
	Report.Patches = Built.Num();
	Out = MoveTemp(Built);
	return true;
}
}
