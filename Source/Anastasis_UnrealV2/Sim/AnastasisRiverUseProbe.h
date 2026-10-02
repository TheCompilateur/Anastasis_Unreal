#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnastasisRiverUseProbe.generated.h"

/** Explicit PIE experiment only; no changes to the portable simulator or its decisions. */
UCLASS()
class UAnastasisRiverUseProbe : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Anastasis|Debug", meta=(WorldContext="WorldContextObject"))
    static FString Describe(const UObject* WorldContextObject);
    /** Requires an empty PIE village. Spawns one thirsty autonomous NPC; never sets its goal. */
    UFUNCTION(BlueprintCallable, Category="Anastasis|Debug", meta=(WorldContext="WorldContextObject"))
    static FString Begin(const UObject* WorldContextObject, double SimX, double SimY);
    UFUNCTION(BlueprintPure, Category="Anastasis|Debug", meta=(WorldContext="WorldContextObject"))
    static FString Read(const UObject* WorldContextObject, const FString& NpcId);
};
