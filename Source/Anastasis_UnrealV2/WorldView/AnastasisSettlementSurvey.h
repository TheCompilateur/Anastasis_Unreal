#pragma once
#include "WorldView/AnastasisSettlementSite.h"
class UWorld;
namespace AnastasisWorld { struct FWorld; }
namespace AnastasisVillage { class FVillage; }
namespace AnastasisSettlementSurvey
{
/** Reads the actual world's ExperimentalTerrain sections; never the process-global Forge cache. */
bool Read(UWorld* World, uint32 Seed, const AnastasisWorld::FWorld& Sim,
    const AnastasisVillage::FVillage& Village, AnastasisSettlementSite::FInputs& Out, FString& Error);

/**
 * SITE_FROM_SIM_001 -- the site policy's inputs from the SIMULATION only: tile altitude, type,
 * wetness, resources and the village's own foot blocking. A pure function of (seed, world, village):
 * no rendered mesh and no render CVar (Drainage, HumanGeography, Forge, Scale) can move the opening
 * village. GEO_MEASURE_001 measured the rendered-mesh survey moving it by 0.5 to 0.7 km.
 *
 * Slopes are those of the simulation's own relief (canonical altitude, scale 5, 20 m tiles), not the
 * forged one. Calibrated on the canonical world (Anastasis.SettlementSite.FromSimulation sweep,
 * 2026-10-07): ReliefFactor 1.0 -> 1250 eligible sites, 1.5 -> 330, >= 2.0 -> none (the forge does
 * NOT exaggerate shores, a uniform factor does, so water becomes unreachable). The forge's
 * anastasis.Terrain.Forge.Exaggerate is never read. The rendered slope under the chosen site is
 * reported as an observation (`rendered_slope_deg`).
 */
inline constexpr double ReliefFactor = 1.0;
inline constexpr double CanonicalSpatialScale = 5.0;
void ReadSimulation(uint32 Seed, const AnastasisWorld::FWorld& Sim, const AnastasisVillage::FVillage& Village,
    AnastasisSettlementSite::FInputs& Out, double Relief = ReliefFactor);

/**
 * Copies what the rendered survey OBSERVED (water at centres, rendered slope, provenance) into a
 * simulation survey. Selection fields are not touched: the observation is reported, it never chooses.
 */
void MergeRenderObservation(const AnastasisSettlementSite::FInputs& Rendered, AnastasisSettlementSite::FInputs& Out);

/** `anastasis.Village.SiteSource` : true = the simulation chooses (default), false = the rendered relief. */
bool SiteFromSimulation();

/**
 * The inputs the opening site is chosen from -- one place for the host and its probes, so a probe
 * never re-chooses a different site than the village. From the simulation (merged with `Rendered`
 * when `bRenderedOk`), or `Rendered` itself under SiteSource 0.
 */
AnastasisSettlementSite::FInputs SiteInputs(const AnastasisSettlementSite::FInputs& Rendered, bool bRenderedOk,
    uint32 Seed, const AnastasisWorld::FWorld& Sim, const AnastasisVillage::FVillage& Village);
}
