#pragma once

#include "CoreMinimal.h"

/**
 * Knobs SIM d'archetype — sous-ensemble de src/sim/worldArchetypes.js
 * consomme par generateWorld / hydrologie. Les blocs air/forest/wardrobe
 * (presentation) ne sont pas portes ici.
 */
namespace AnastasisWorldArchetype
{
	inline constexpr int32 Count = 5;

	enum class EId : uint8
	{
		Vale = 0,
		Delta = 1,
		Plateau = 2,
		Highland = 3,
		Deepwood = 4,
	};

	struct FKnobs
	{
		EId Id = EId::Vale;
		double SeaLowFrac = 0.10;
		double ValleyDig = 1.0;
		double BasinDig = 1.0;
		double ChannelDig = 1.0;
		double LakeDig = 1.0;
		int32 MinWaterBody = 8;
		double ForestT = 0.61;
		double FieldT = 0.35;
		double Highland = 0.61;
		double RockT = 0.45;
		double RuinT = 0.75;
		double RidgeBoost = 0.06;
		double PlateauFlat = 0.38;
		double MoistBias = 0.02;
		double RiparianBoost = 0.08;
		int32 ClearRadius = 9;
		int32 ExtentW = 96;
		int32 ExtentH = 96;
	};

	ANASTASISSIM_API EId PickId(uint32 Seed);
	ANASTASISSIM_API FKnobs Resolve(uint32 Seed);
	ANASTASISSIM_API const TCHAR* IdName(EId Id);
}
