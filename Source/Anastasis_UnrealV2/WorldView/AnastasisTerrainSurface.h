#pragma once
#include "WorldView/AnastasisWorldView.h"

// Presentation only. Vertices are source tile centers; outer half-tile strips
// are deliberately NOT extrapolated. No border halo or new altitude is needed.
namespace AnastasisTerrainSurface
{
struct FGeometry
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<int32> SourceIndices;
};
bool Build(const AnastasisWorldView::FWorldVisualSnapshot& Crop, FGeometry& Out);
}
