#pragma once
#include "WorldView/AnastasisTerrainSurface.h"

/** Authored, reversible presentation layer for the inspected seed 12345.
 * XY are source tile coordinates (centres at X/Y + 0.5), independent of physical scale.
 * Height arguments/results are metres in the unscaled corrected relief.
 * Does not mutate Alt, Type, resources, fertility, or simulation water flow.
 */
namespace AnastasisHumanGeography
{
struct FSample
{
    double Height = 0;
    double WaterHeight = 2.75;
    double ValleyWeight = 0;
    double RiverWeight = 0;
};
FSample Evaluate(double X, double Y, double OriginalHeight);
/** Authored river centrelines, upstream first: XY in the same tile coordinates as Evaluate,
 * Z = water height in metres of the unscaled relief. Read by HYDRO_NETWORK_001. */
TArray<TArray<FVector>> AuthoredRivers();
void Apply(const AnastasisWorldView::FWorldVisualSnapshot& Snapshot,
    AnastasisTerrainSurface::FGeometry& Geometry, int32 Width, int32 Height);
}
