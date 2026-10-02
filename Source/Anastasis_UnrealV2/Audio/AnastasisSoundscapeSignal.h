#pragma once
#include "CoreMinimal.h"

// Presentation only: never consumes simulation RNG or changes village state.
namespace AnastasisSoundscape
{
constexpr int32 SampleRate = 24000;
enum class ESound : uint8 { Step, Work, Water };
struct FObservation
{
    FVector Position = FVector::ZeroVector;
    FString Session;
    int32 Swings = 0;
    double LastSwing = -1;
    bool bVisible = false;
};
struct FEvents { bool bStep = false; bool bWork = false; };
struct FTrack
{
    FObservation Previous;
    double Distance = 0, StepCooldown = 0, WorkCooldown = 0;
    bool bSeen = false;
    FEvents Observe(const FObservation& Current, double RealDt);
};
struct FSynth
{
    explicit FSynth(uint32 Seed) : Random(Seed) {}
    FRandomStream Random;
    double Time = 0;
    float Low = 0;
    TArray<int16> Render(ESound Sound, int32 Count);
};
}
