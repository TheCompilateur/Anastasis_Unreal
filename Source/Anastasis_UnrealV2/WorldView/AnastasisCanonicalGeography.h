#pragma once
#include "CoreMinimal.h"

/**
 * WATER_NETWORK_001 -- the canonical geography: the drained relief and its water network, computed
 * from the seed alone with a FIXED recipe (scale 5, Human_Geography on, forge and drainage at their
 * defaults). It is the same chain the embodiment runs (TerrainSurface -> TerrainForge -> Drainage
 * -> lake sheet + river ribbons), but it never reads a render CVar and never needs a UWorld.
 *
 * Why (decision of Alexandre, 2026-10-08: « l'eau doit respecter son reseau ») : the simulation's
 * water came from the JS hydrology (sea-level trenches, 27 water bodies, most without an outlet),
 * while the player saw the drainage network. GEO_MEASURE_001 measured 833 of 9 216 tile centres in
 * disagreement, 75 % of them produced by the drainage network. The simulation now takes its water
 * from this network (FAnastasisSimulation::ApplyWaterMask, ecart declared in AnastasisSim).
 *
 * Render CVars still change what is DRAWN (a debug A/B of anastasis.Terrain.Drainage 0 shows the old
 * water); they no longer change what the villagers drink from or walk around.
 */
namespace AnastasisCanonicalGeography
{
struct FGeography
{
	bool bValid = false;
	FString Error;
	uint32 Seed = 0;
	int32 W = 0, H = 0;
	/** 1 = water of the network at the tile centre (lake sheet or river ribbon at or above the ground). */
	TArray<uint8> Water;
	/** Drained ground at the tile centre (cm, canonical scale 5) and its slope (deg, 3x3 samples at +-0.4 tile,
	 *  as the settlement survey reads the rendered mesh). The opening site reads THIS relief (SITE_FROM_SIM_001). */
	TArray<float> GroundZ;
	TArray<float> Slope;
	int32 WaterTiles = 0;
	int32 Rivers = 0, Lakes = 0;
	double Seconds = 0.0;
};

/** Computes it (forge + drainage of the whole canonical world: ~2-3 s). */
FGeography Compute(uint32 Seed);

/** Cached per seed for the process (game thread). */
const FGeography& Get(uint32 Seed);
}
