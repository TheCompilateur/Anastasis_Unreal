#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WorldView/AnastasisAnthropicMemory.h"
#include "AnastasisAnthropicSubsystem.generated.h"
class UHierarchicalInstancedStaticMeshComponent;

/** Experimental, reversible grass response to sampled NPC displacement. No simulation writes. */
UCLASS()
class UAnastasisAnthropicSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual void Deinitialize() override;
    UFUNCTION(BlueprintPure, Category="Anastasis|Anthropic")
    FString GetReport() const;
    void ResetPresentation();
private:
    struct FGrass
    {
        TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;
        int32 Index = 0;
        int32 Count = 0;
        FTransform Original;
        FTransform Applied;
    };
    AnastasisAnthropic::FMemory Memory;
    TArray<FGrass> Grass;
    void Restore();
    void ApplyGrass();
    double LastTime = -1.0;
    double Refresh = 0.0;
    double ApplyMs = 0.0;
    uint32 Seed = 0;
    bool bActive = false;
    bool bDrawn = false;
    double AppliedTime = -1.0;
    FVector Focus = FVector::ZeroVector;
    double FocusStrength = 0.0;
    bool bTruncatedGrass = false;
    int32 Restored = 0;
    int32 RestoreErrors = 0;
};


UCLASS()
class UAnastasisAnthropicDebugLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Anastasis|Anthropic", meta=(WorldContext="WorldContextObject"))
    static FString GetStatus(const UObject* WorldContextObject);
};
