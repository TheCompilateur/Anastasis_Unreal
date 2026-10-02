#pragma once
#include "CoreMinimal.h"
#include "Core/AnastasisJsNumeric.h"
#include "Work/AnastasisGather.h"

// fee66ae: forestSustain.js and craftWork.js, scalar port without technique/boost.
namespace AnastasisWoodHarvest
{
inline int32 Floor(bool Forest, double Crown, double Clearing, bool Frontier)
{
    if (!Forest || Frontier) return 0;
    return Crown >= 0.55 && Clearing < 0.02 ? 10 : 4;
}
inline int32 Take(int32 Amount, int32 Minimum, int32 Wanted)
{
    return FMath::Min(FMath::Max(0, Wanted), FMath::Max(0, Amount - Minimum));
}
inline int32 Yield(double Skill) { return AnastasisJs::NumberOr(Skill, 0.7) >= 1.35 ? 4 : 3; }
inline double Period(double Skill, int32 Swings, double Energy)
{
    return FMath::Max(0.44, 0.58 - AnastasisJs::NumberOr(Skill, 0.7) * 0.08)
        * AnastasisGather::CraftFatiguePeriodMul(Swings, Energy);
}
}
