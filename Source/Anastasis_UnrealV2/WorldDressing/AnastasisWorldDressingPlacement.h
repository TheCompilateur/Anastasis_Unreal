#pragma once
#include "CoreMinimal.h"
#include "WorldDressing/AnastasisWorldDressingProfile.h"
#include "WorldView/AnastasisWorldView.h"

namespace AnastasisWorldDressing
{
struct FTriangle { FVector A, B, C; };
/** Read-only copy of the currently rendered ground plus semantic snapshot and exclusions. */
struct FMap
{
    AnastasisWorldView::FWorldVisualSnapshot Snapshot;
    FTransform SourceTransform = FTransform::Identity;
    TArray<FTriangle> Triangles;
    // Section 1 of this exact source, not the global Forge cache or a fixed sea level.
    TArray<FTriangle> WaterTriangles;
    TMap<FIntPoint, TArray<int32>> WaterBuckets;
    bool bUseRenderedWater = false;
    double TileSize() const { return AnastasisWorldView::TileWorldSize * Snapshot.SpatialScale; }
    TMap<FIntPoint, TArray<int32>> TriangleBuckets;
    TArray<FBox2D> Water, Roads, Buildings;
    bool Prepare(FString& Error);
    bool Sample(double X, double Y, FVector& Ground, FVector& Normal) const;
    bool SampleWater(double X, double Y, FVector& Ground, FVector& Normal) const;
    const AnastasisWorldView::FVisualTile* TileAt(double X, double Y) const;
    double WaterDistance(FVector2D Point) const;
    bool IsDry(FVector2D Point) const;
};
struct FPlacement
{
    int32 RuleIndex = 0;
    FTransform Transform;
    FVector Ground;
};
struct FResult
{
    TArray<FPlacement> Placements;
    TArray<int32> Counts;
    int32 Rejected = 0;
    bool bCapped = false;
    FString Hash;
};
/** Pure placement planner: no actors, no world mutation, no collision scene queries. */
bool Build(const FMap& Map, const TArray<FAnastasisDressingRule>& Rules, int32 Seed,
    int32 MaxInstances, FResult& Out, FString& Error);
FString PlacementHash(const FResult& Result, const TArray<FAnastasisDressingRule>& Rules);
}
