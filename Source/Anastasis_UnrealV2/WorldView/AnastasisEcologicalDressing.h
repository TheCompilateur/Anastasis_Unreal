#pragma once

#include "CoreMinimal.h"
#include "WorldView/AnastasisWorldView.h"
#include "AnastasisEcologicalDressing.generated.h"

/** Presentation controls only. All distances are simulation tiles unless named UU. */
USTRUCT(BlueprintType)
struct FAnastasisForestDressingSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category="Forest") bool bEnabled = true;
    UPROPERTY(EditAnywhere, Category="Forest", meta=(ClampMin="1", ClampMax="4")) int32 CandidatesPerTile = 4;
    UPROPERTY(EditAnywhere, Category="Forest", meta=(ClampMin="1", ClampMax="5")) float EdgeRadius = 2.5f;
    UPROPERTY(EditAnywhere, Category="Forest", meta=(ClampMin="0", ClampMax="1")) float Density = 0.72f;
    UPROPERTY(EditAnywhere, Category="Forest", meta=(ClampMin="1", ClampMax="16")) float ClusterSpan = 4.0f;
    UPROPERTY(EditAnywhere, Category="Forest", meta=(ClampMin="0", ClampMax="0.8")) float ClearingThreshold = 0.28f;
    UPROPERTY(EditAnywhere, Category="Forest", meta=(ClampMin="1", ClampMax="60")) float MaxSlopeDegrees = 35.0f;
    UPROPERTY(EditAnywhere, Category="Forest", meta=(ClampMin="0", ClampMax="1")) float WetnessPenalty = 0.55f;
    UPROPERTY(EditAnywhere, Category="Forest", meta=(ClampMin="0", ClampMax="50")) float WaterClearanceUU = 12.0f;
    UPROPERTY(EditAnywhere, Category="Forest", meta=(ClampMin="0.1", ClampMax="0.9")) float MinimumSpacing = 0.55f;
    UPROPERTY(EditAnywhere, Category="Forest", meta=(ClampMin="0", ClampMax="0.25")) float RootRadius = 0.12f;
    UPROPERTY(EditAnywhere, Category="Forest") FVector2D YoungScale = FVector2D(0.25, 0.45);
    UPROPERTY(EditAnywhere, Category="Forest") FVector2D SecondaryScale = FVector2D(0.50, 0.78);
    UPROPERTY(EditAnywhere, Category="Forest") FVector2D CanopyScale = FVector2D(0.95, 1.20);

    /** Macro pass uses the rendered relief; the old tile grammar remains available for comparison. */
    UPROPERTY(EditAnywhere, Category="Forest|Macro") bool bMacroForest = true;
    UPROPERTY(EditAnywhere, Category="Forest|Macro", meta=(ClampMin="4", ClampMax="32")) float MassSpan = 14.0f;
    UPROPERTY(EditAnywhere, Category="Forest|Macro", meta=(ClampMin="300", ClampMax="1600")) float TrunkSpacingUU = 500.0f;
    UPROPERTY(EditAnywhere, Category="Forest|Macro", meta=(ClampMin="1", ClampMax="4")) float HeightMultiplier = 3.5f;
    UPROPERTY(EditAnywhere, Category="Forest|Macro", meta=(ClampMin="0", ClampMax="20")) float BasinClearRadius = 8.0f;
    UPROPERTY(EditAnywhere, Category="Forest|Macro", meta=(ClampMin="1", ClampMax="60")) float HillsideMaxSlope = 48.0f;
};

namespace AnastasisEcologicalDressing
{
/** Rendered trees only, after mesh resolution and water rejection. Centimetres. */
struct FGroundTree
{
    FVector2D Position;
    double Height = 0;
    uint32 Seed = 0;
};

/** Presentation mask: R = overlapping litter beds, G = root contact. No geometry/RNG edits.
 * Linear byte data, texel centres mapped over Bounds; order-independent max union.
 */
bool BuildGroundMask(const TArray<FGroundTree>& Trees, const FBox2D& Bounds,
    int32 Resolution, TArray<FColor>& Out);

enum class ELayer : uint8 { Young, Secondary, Canopy };

struct FPlacement
{
    int32 SourceIndex = INDEX_NONE;
    uint32 VisualSeed = 0;
    FVector Ground = FVector::ZeroVector;
    double ScaleMultiplier = 1.0;
    double SlopeDegrees = 0.0;
    ELayer Layer = ELayer::Young;
};

struct FPlan
{
    TArray<FPlacement> Instances;
    int32 RejectedWaterOrFootprint = 0;
    int32 RejectedSlope = 0;
    int32 RejectedSpacing = 0;
    int32 RejectedOpenGround = 0;
};

/** Read-only view of the terrain already built by the owner. No terrain generation here. */
struct FRenderedHabitat
{
    TFunction<bool(double, double, double&)> SampleHeight;
    TFunction<bool(double, double, double&)> SampleWaterHeight;
    /** Optional: rendered riparian wetness [0,1] at (X, Y) world (HYDRO_NETWORK_001). Thins the
     * canopy along the rivers actually rendered, not only near the simulation's water tiles. */
    TFunction<bool(double, double, double&)> SampleRiparian;
    FVector Basin = FVector::ZeroVector;
    bool bHasBasin = false;
};

/** Full canonical input preserves ecological context independently of the camera.
 * Macro mode additionally conditions placement on the supplied rendered footprint and basin.
 * Read-only, no UObject creation, no simulation RNG, no mesh paths.
 * Invalid data returns a path in OutError and no partial result.
 */
bool Build(const AnastasisWorldView::FWorldVisualSnapshot& Source,
    const FAnastasisForestDressingSettings& Settings, FPlan& Out, FString& OutError,
    const FRenderedHabitat* Rendered = nullptr);
}
