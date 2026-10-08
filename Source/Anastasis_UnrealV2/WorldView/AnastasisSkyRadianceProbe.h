#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnastasisSkyRadianceProbe.generated.h"

class USkyLightComponent;

/** Diagnostic readback only. This performs a separate emissive-only capture and does not read the live Sky Light texture. */
UCLASS()
class UAnastasisSkyRadianceProbe : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Anastasis|Debug|Lighting")
    static FString CaptureEmissiveSkyRadiance(USkyLightComponent* SkyLight);
};
