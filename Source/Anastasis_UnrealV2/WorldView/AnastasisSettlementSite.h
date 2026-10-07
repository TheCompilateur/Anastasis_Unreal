#pragma once
#include "CoreMinimal.h"

/** Opening-site policy in the Unreal host. Does not change the portable simulator/navigation. */
namespace AnastasisSettlementSite
{
struct FCell
{
    bool bSurveyed = false;
    bool bWalkable = false;
    bool bDry = false;
    bool bCenterAllowed = false;
    bool bWater = false;
    // Read-only centre samples; never used by the selection policy.
    bool bWaterObserved = false;
    bool bSimWater = false;
    bool bRenderedWater = false;
    /** Rendered-mesh slope at the centre (degrees); < 0 = not observed. Observation only. */
    double RenderedSlope = -1.0;
    bool bWood = false;
    bool bFood = false;
    double Height = 0.0;
    double Slope = 90.0;
    double Fertility = 0.0;
};
struct FInputs
{
    int32 W = 0, H = 0;
    double TileMetres = 20.0;
    TArray<FCell> Cells;
    uint32 Seed = 0;
    FString SourceWorld, TerrainComponent;
    /** What the selection fields were read from: `simulation` (SITE_FROM_SIM_001) or `rendered_relief`. */
    FString SelectionSource = TEXT("rendered_relief");
};
struct FSettings
{
    double SiteSlope = 8.0;
    double ExpansionSlope = 12.0;
    double RouteSlope = 18.0;
    double WaterReach = 300.0;
    double FoodReach = 600.0;
    double WoodReach = 600.0;
    int32 AreaRadius = 3;
    int32 MinAreaCells = 9;
};
struct FCandidate
{
    int32 Index = INDEX_NONE;
    bool bEligible = false;
    double Score = 0.0;
    double Slope = 90.0;
    double AreaM2 = 0.0;
    double WaterM = -1.0, FoodM = -1.0, WoodM = -1.0;
    int32 WaterAccess = INDEX_NONE, FoodAccess = INDEX_NONE, WoodAccess = INDEX_NONE;
};
struct FReport
{
    bool bValidInput = false;
    FCandidate Best, Legacy;
    TArray<FCandidate> Top;
    int32 Surveyed = 0, Eligible = 0;
    FString Error;
};
FReport Choose(const FInputs& In, const FSettings& Settings = FSettings());
FString ToJson(const FReport& Report, const FInputs& In);
}
