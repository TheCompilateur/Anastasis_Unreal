#pragma once

#include "CoreMinimal.h"
#include "WorldView/AnastasisWorldView.h"
#include "AnastasisEcotoneDressing.generated.h"

/**
 * Ground-ecology layer (0.2 m to 3 m). Presentation only.
 *
 * This is not a second forest and not a prop scatter. It places traces on
 * transitions the simulation already knows: forest floor and fringe, shore,
 * and the foot of stone. Distances are simulation tiles unless named UU.
 */
USTRUCT(BlueprintType)
struct FAnastasisEcotoneDressingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ecotone")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Ecotone", meta = (ClampMin = "1", ClampMax = "4"))
	int32 CandidatesPerTile = 2;

	UPROPERTY(EditAnywhere, Category = "Ecotone", meta = (ClampMin = "1", ClampMax = "4"))
	float NeighborRadius = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Ecotone", meta = (ClampMin = "0", ClampMax = "1"))
	float UnderstoryDensity = 0.26f;

	UPROPERTY(EditAnywhere, Category = "Ecotone", meta = (ClampMin = "0", ClampMax = "1"))
	float ForestEdgeDensity = 0.44f;

	UPROPERTY(EditAnywhere, Category = "Ecotone", meta = (ClampMin = "0", ClampMax = "1"))
	float ShoreDensity = 0.58f;

	UPROPERTY(EditAnywhere, Category = "Ecotone", meta = (ClampMin = "0", ClampMax = "1"))
	float RockFootDensity = 0.40f;

	UPROPERTY(EditAnywhere, Category = "Ecotone", meta = (ClampMin = "0.25", ClampMax = "1.2"))
	float MinimumSpacing = 0.70f;

	UPROPERTY(EditAnywhere, Category = "Ecotone", meta = (ClampMin = "1", ClampMax = "60"))
	float MaxSlopeDegrees = 48.0f;

	UPROPERTY(EditAnywhere, Category = "Ecotone", meta = (ClampMin = "0", ClampMax = "30"))
	float WaterClearanceUU = 4.0f;

	UPROPERTY(EditAnywhere, Category = "Ecotone", meta = (ClampMin = "0", ClampMax = "1"))
	float CompanionChance = 0.70f;

	UPROPERTY(EditAnywhere, Category = "Ecotone")
	FVector2D ScaleEnvelope = FVector2D(0.88, 1.14);
};

namespace AnastasisEcotoneDressing
{
enum class EContext : uint8
{
	Understory = 0,
	ForestEdge = 1,
	Shore = 2,
	RockFoot = 3
};
inline constexpr int32 ContextCount = 4;

enum class EAsset : uint8
{
	Stump = 0,
	FallenLog = 1,
	ExposedRoots = 2,
	BranchPile = 3,
	Driftwood = 4,
	BushLow = 5,
	GrassTuft = 6,
	Reed = 7,
	ShoreTuft = 8,
	Sapling = 9,
	RockCluster = 10,
	BuriedBlock = 11
};
inline constexpr int32 AssetCount = 12;

struct FPlacement
{
	int32 SourceIndex = INDEX_NONE;
	uint32 VisualSeed = 0;
	FVector Ground = FVector::ZeroVector;
	double ScaleMultiplier = 1.0;
	double YawDegrees = 0.0;
	double SlopeDegrees = 0.0;
	EContext Context = EContext::ForestEdge;
	EAsset Asset = EAsset::Stump;
	bool bCompanion = false;
};

struct FPlan
{
	TArray<FPlacement> Instances;
	int32 RejectedWaterOrFootprint = 0;
	int32 RejectedSlope = 0;
	int32 RejectedSpacing = 0;
	int32 ContextCounts[ContextCount] = {};
	int32 AssetCounts[AssetCount] = {};
	int32 CompanionCount = 0;
};

const TCHAR* AssetName(EAsset Asset);
const TCHAR* ContextName(EContext Context);

/** Full canonical input. Read-only, no UObject, no mesh paths, no simulation RNG.
 * Invalid data returns a path in OutError and no partial result.
 */
bool Build(const AnastasisWorldView::FWorldVisualSnapshot& Source,
	const FAnastasisEcotoneDressingSettings& Settings, FPlan& Out, FString& OutError);
}
