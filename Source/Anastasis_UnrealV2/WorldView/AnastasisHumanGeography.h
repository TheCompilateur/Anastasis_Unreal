#pragma once
#include "WorldView/AnastasisTerrainSurface.h"

/** Authored, reversible presentation layer for the inspected seed 12345.
 * Coordinates are metres in the ORIGINAL 95m field, independent of physical scale.
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
void Apply(const AnastasisWorldView::FWorldVisualSnapshot& Snapshot,
    AnastasisTerrainSurface::FGeometry& Geometry, int32 Width, int32 Height);
}
