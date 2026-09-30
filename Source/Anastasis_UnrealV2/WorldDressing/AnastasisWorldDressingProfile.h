#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AnastasisWorldDressingProfile.generated.h"

class UStaticMesh;

UENUM(BlueprintType)
enum class EAnastasisDressingTerrain : uint8
{
    Grass, Water, Stone, Ruin, Forest, Scrub, Field, Road
};

USTRUCT(BlueprintType)
struct FAnastasisDressingRule
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement") FName AssetId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement") TSoftObjectPtr<UStaticMesh> StaticMesh;
    /** Empty means every land family. Water is always excluded in V0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement") TArray<EAnastasisDressingTerrain> AllowedTerrainFamilies;
    /** Rendered ground height, in source-local Unreal cm (not normalized simulation altitude). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement") float AltitudeMin = -100000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement") float AltitudeMax = 100000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement", meta=(ClampMin="0", ClampMax="89")) float SlopeMin = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement", meta=(ClampMin="0", ClampMax="89")) float SlopeMax = 35;
    /** Conservative XY distance to semantic water or rendered water triangles, cm; -1 max means unlimited. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement", meta=(ClampMin="0")) float DistanceToWaterMin = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement", meta=(ClampMin="-1")) float DistanceToWaterMax = -1;
    /** Expected candidate count per tile, before rejection. Hard cap 16 candidates/tile. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement", meta=(ClampMin="0", ClampMax="16")) float Density = 0.15f;
    /** 0 = uniform scatter; otherwise candidates form clumps around a seeded point per tile (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement", meta=(ClampMin="0", ClampMax="10000")) float ClusterRadius = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement", meta=(ClampMin="0.001")) float ScaleMin = 0.8f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement", meta=(ClampMin="0.001")) float ScaleMax = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement") bool bRandomYaw = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement") bool bAlignToSurfaceNormal = false;
    /** Adds a 25cm shoreline margin. Even when false, V0 never places roots in water. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement") bool bAvoidWater = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement") bool bAvoidRoads = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement") bool bAvoidBuildings = true;
    /** Root/support clearance radius, cm, scaled per instance. Not a canopy collision guarantee. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Placement", meta=(ClampMin="0", ClampMax="10000")) float ClearanceRadius = 15;
};

/** Create via Content Browser > Miscellaneous > Data Asset. No required final assets. */
UCLASS(BlueprintType)
class ANASTASIS_UNREALV2_API UAnastasisWorldDressingProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World Dressing") TArray<FAnastasisDressingRule> Rules;
};
