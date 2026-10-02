#include "Work/AnastasisCraftMiss.h"

#include "Work/AnastasisGather.h"

namespace AnastasisCraftMiss
{
	FString MissKindFor(const FString& CraftId)
	{
		// `MISSABLE`.
		if (CraftId == TEXT("chop") || CraftId == TEXT("quarry") || CraftId == TEXT("forge")
			|| CraftId == TEXT("workshop") || CraftId == TEXT("hammer") || CraftId == TEXT("saw"))
		{
			return TEXT("glance");
		}
		if (CraftId == TEXT("build") || CraftId == TEXT("maintain")) return TEXT("slip");
		if (CraftId == TEXT("farm") || CraftId == TEXT("tend")) return TEXT("whiff");
		return FString();
	}

	double FatigueMissMul(int32 SwingsDone, double Energy)
	{
		return 1.0 + AnastasisGather::CraftFatigueT(SwingsDone, Energy) * (1.55 - 1.0);
	}

	double MissChance(const FString& CraftId, double Skill, double Mastery, int32 SwingsDone, double Energy)
	{
		if (CraftId.IsEmpty() || MissKindFor(CraftId).IsEmpty()) return 0.0;
		// `Number(npc?.skill) || 1` : 0 et NaN valent 1.
		const double S = (Skill != 0.0 && !FMath::IsNaN(Skill)) ? Skill : 1.0;
		const double Soft = 1.0 + FMath::Max(0.0, S - 1.0) * 0.55;
		const double Hand = 1.0 + Mastery * MasteryGuard;
		return (BaseChance / (Soft * Hand)) * FatigueMissMul(SwingsDone, Energy);
	}

	bool CanRoll(const FString& CraftId, double Now, double CraftMissAt, double LastMissAt)
	{
		if (MissKindFor(CraftId).IsEmpty()) return false;
		if (Now - CraftMissAt < Cooldown) return false;
		if (Now - LastMissAt < Cooldown) return false;
		return true;
	}
}
