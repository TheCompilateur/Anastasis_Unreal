#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Audio/AnastasisSoundscapeSignal.h"
#include "AnastasisSoundscapeSubsystem.generated.h"

class UAudioComponent;
class USoundWaveProcedural;
USTRUCT()
struct FAnastasisSoundVoice
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UAudioComponent> Component;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> Wave;
    double Expires = 0;
};

UCLASS()
class UAnastasisSoundscapeSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override;
    FString Snapshot() const;
private:
    FAnastasisSoundVoice MakeVoice(const FVector& Position, float Radius, float Gain);
    void Emit(AnastasisSoundscape::ESound Sound, const FVector& Position);
    void StopAll();
    void UpdateWater(const FVector& Listener, float DeltaTime);
    UPROPERTY() TArray<FAnastasisSoundVoice> Voices;
    UPROPERTY() FAnastasisSoundVoice Water;
    TMap<FString, AnastasisSoundscape::FTrack> Tracks;
    AnastasisSoundscape::FSynth WaterSynth{781};
    FVector WaterTarget = FVector::ZeroVector;
    double Clock = 0, PreviousSimTime = -1;
    float WaterScan = 0, WaterGain = 0;
    bool bWaterNearby = false;
    int32 Steps = 0, Work = 0, Dropped = 0;
    uint32 Serial = 0;
};

UCLASS()
class UAnastasisSoundscapeDebugLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Anastasis|Soundscape", meta=(WorldContext="WorldContextObject"))
    static FString GetSoundscapeStatus(const UObject* WorldContextObject);
};
