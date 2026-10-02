#include "Ai/AnastasisGoalNoise.h"

#include "Core/AnastasisRng.h"

namespace AnastasisGoalNoise
{
	double GoalNoise(FAnastasisRng& Rng, double Amp, double Scale)
	{
		// `sim.rng() * amp * GOAL_AI.noiseScale` : gauche a droite.
		return Rng.Next() * Amp * Scale;
	}

	namespace
	{
		using C = ENoiseCondition;

		const FTableNoise Table[] = {
			{ TEXT("eatTogether"), 6.0, C::Always, 1114 },
			{ TEXT("relax"), 8.0, C::Always, 1116 },
			{ TEXT("drink"), 6.0, C::Always, 1118 },
			// `gatherWoodScoreForDecision` : resourceScore(wood) puis le bruit.
			{ TEXT("gatherWood"), 14.0, C::Always, 1072 },
			{ TEXT("gatherStone"), 16.0, C::Always, 1120 },
			{ TEXT("gatherFood"), 14.0, C::Always, 1121 },
			// `helpFarmScore`, apres `collectiveGoalBias(sim, "helpFarm")`.
			{ TEXT("helpFarm"), 10.0, C::HasFarm, 2559 },
			// `(buildScore(sim, npc) + goalNoise(sim, 14))` : buildScore d'abord (il peut
			// tirer, rarement, par findBuildSpot — hors de cette table).
			{ TEXT("build"), 14.0, C::Always, 1125 },
			{ TEXT("craft"), 8.0, C::HasCraftBuilding, 2780 },
			{ TEXT("maintain"), 7.0, C::Always, 2813 },
			{ TEXT("aidHousehold"), 4.0, C::Always, 1135 },
			{ TEXT("visitFamily"), 8.0, C::HasFamilyToVisit, 2965 },
			// Dans `exploreScore` (curiosite), avant nouveaute et frontiere.
			{ TEXT("explore"), 14.0, C::Always, 2355 },
			{ TEXT("socialize"), 8.0, C::Always, 1138 },
			{ TEXT("confront"), 6.0, C::Always, 1139 },
			{ TEXT("shelterRain"), 6.0, C::Always, 1140 },
			{ TEXT("closeWorkplace"), 5.0, C::Always, 1141 },
		};
	}

	TConstArrayView<FTableNoise> AdultTableNoises()
	{
		return TConstArrayView<FTableNoise>(Table, UE_ARRAY_COUNT(Table));
	}
}
