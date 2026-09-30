#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldDressing/AnastasisWorldDressingProfile.h"
#include "AnastasisWorldDressingManager.generated.h"

class AAnastasisWorldEmbodiment;
class UHierarchicalInstancedStaticMeshComponent;

/** Optional editor preview consumer. Never auto-generates, saves a level, or spawns prop actors. */
UCLASS(BlueprintType, Blueprintable)
class ANASTASIS_UNREALV2_API AAnastasisWorldDressingManager : public AActor
{
    GENERATED_BODY()
public:
    AAnastasisWorldDressingManager();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World Dressing") TObjectPtr<UAnastasisWorldDressingProfile> Profile;
    /** Explicit source avoids guessing when a level contains several maps. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World Dressing") TObjectPtr<AAnastasisWorldEmbodiment> WorldSource;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World Dressing") int32 Seed = 12345;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World Dressing", meta=(ClampMin="1", ClampMax="100000")) int32 MaxInstances = 20000;
    /** Supplement automatic VillageBuilding / WorldDressingBuilding tag discovery. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World Dressing|Exclusions") TArray<TObjectPtr<AActor>> BuildingExclusions;
    /** Supplement semantic Road tiles / WorldDressingRoad tag discovery. Bounds projected onto XY. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World Dressing|Exclusions") TArray<TObjectPtr<AActor>> RoadExclusions;
    /** Padding also protects logical buildings that have no render mesh bounds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World Dressing|Exclusions", meta=(ClampMin="0")) float ExclusionPadding = 100;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="World Dressing|Report") FString PlacementHash;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="World Dressing|Report") FString PlacementReport = TEXT("EMPTY: GeneratePreview has not run");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="World Dressing|Report") int32 GeneratedInstanceCount = 0;
    UFUNCTION(CallInEditor, BlueprintCallable, Category="World Dressing") void GeneratePreview();
    UFUNCTION(CallInEditor, BlueprintCallable, Category="World Dressing") void ClearPreview();
    UFUNCTION(CallInEditor, BlueprintCallable, Category="World Dressing") void RebuildFromSeed();
    UFUNCTION(CallInEditor, BlueprintCallable, Category="World Dressing") void PrintPlacementReport();
    virtual bool IsEditorOnly() const override { return true; }
#if WITH_EDITOR
    virtual void OnConstruction(const FTransform& Transform) override;
#endif
private:
    UPROPERTY(Transient, DuplicateTransient) TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GeneratedComponents;
};
