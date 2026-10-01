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
    /** 1 on the saddle centreline between the two valleys. The relief is unchanged. */
    double PassageWeight = 0;
};
FSample Evaluate(double X, double Y, double OriginalHeight);
void Apply(const AnastasisWorldView::FWorldVisualSnapshot& Snapshot,
    AnastasisTerrainSurface::FGeometry& Geometry, int32 Width, int32 Height);
}
