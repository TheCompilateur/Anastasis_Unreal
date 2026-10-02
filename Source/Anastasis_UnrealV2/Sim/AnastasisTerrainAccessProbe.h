#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnastasisTerrainAccessProbe.generated.h"

/** Bounded PIE diagnostic. No terrain/nav policy changes. */
UCLASS()
class UAnastasisTerrainAccessProbe : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="Anastasis|Debug", meta=(WorldContext="Context"))
 static FString Begin(const UObject* Context);
 UFUNCTION(BlueprintPure, Category="Anastasis|Debug", meta=(WorldContext="Context"))
 static FString Read(const UObject* Context);
};
