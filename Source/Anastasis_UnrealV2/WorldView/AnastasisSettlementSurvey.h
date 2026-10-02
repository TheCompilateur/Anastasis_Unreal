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
}
