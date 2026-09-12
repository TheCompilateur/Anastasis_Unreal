#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWorld.h"

namespace AnastasisHydrology
{
	inline constexpr int32 ValleySeed = 4099;
	inline constexpr int32 WarpSeed = 5107;
	inline constexpr int32 BasinSeed = 6121;
	inline constexpr double WaterFlowAmtGate = 0.06;

	struct FResult
	{
		TArray<float> Filled;
		TArray<float> Accum;
		TArray<int32> FlowTo;
	};

	ANASTASISSIM_API FResult Apply(
		TArray<float>& Height,
		int32 W,
		int32 H,
		uint32 Seed,
		double SeaLevel,
		double ValleyDig,
		double BasinDig,
		double ChannelDig,
		double LakeDig,
		int32 MinWaterBody);

	ANASTASISSIM_API void StampWaterFlowFields(
		TArray<AnastasisWorld::FTile>& Tiles,
		int32 W,
		int32 H,
		const TArray<float>& Accum,
		const TArray<int32>& FlowTo,
		double SeaLevel,
		const TArray<float>& Height);
}
