#include "Audio/AnastasisSoundscapeSignal.h"
namespace AnastasisSoundscape
{
FEvents FTrack::Observe(const FObservation& Current, double RealDt)
{
    FEvents Events;
    StepCooldown = FMath::Max(0.0, StepCooldown - RealDt);
    WorkCooldown = FMath::Max(0.0, WorkCooldown - RealDt);
    const double Moved = FVector::Dist2D(Current.Position, Previous.Position);
    const bool bContinuous = bSeen && Current.bVisible && Previous.bVisible
        && RealDt > 0 && RealDt <= 0.25 && Moved <= 800 * RealDt + 10;
    if (bContinuous)
    {
        Distance += Moved;
        if (Distance >= 85 && StepCooldown <= 0)
        {
            Events.bStep = true;
            Distance = FMath::Fmod(Distance, 85.0);
            StepCooldown = 0.24;
        }
        Events.bWork = !Current.Session.IsEmpty() && Current.Session == Previous.Session
            && Current.Swings > Previous.Swings && Current.LastSwing > Previous.LastSwing
            && WorkCooldown <= 0;
        if (Events.bWork) WorkCooldown = 0.18;
    }
    else Distance = 0;
    // Always consume observations: no backlog after warp, hiding or unmute.
    Previous = Current;
    bSeen = true;
    return Events;
}

TArray<int16> FSynth::Render(ESound Sound, int32 Count)
{
    TArray<int16> Samples;
    Samples.SetNumUninitialized(Count);
    for (int32 I = 0; I < Count; ++I)
    {
        const float Noise = Random.FRandRange(-1.f, 1.f);
        Low += 0.045f * (Noise - Low);
        double Value;
        if (Sound == ESound::Water)
        {
            const double Swell = 0.8 + 0.12 * FMath::Sin(Time * 1.31) + 0.08 * FMath::Sin(Time * 0.47);
            Value = (0.07 * Noise + 0.40 * Low) * Swell;
        }
        else
        {
            const bool bWork = Sound == ESound::Work;
            const double Envelope = FMath::Min(Time / 0.003, 1.0) * FMath::Exp(-Time * (bWork ? 24 : 30));
            const double Frequency = bWork ? 510 : 92;
            const double Body = FMath::Sin(2 * PI * Frequency * Time)
                + 0.3 * FMath::Sin(2 * PI * Frequency * 1.73 * Time);
            Value = Envelope * ((bWork ? 0.25 : 0.19) * Body + 0.12 * Noise + 0.3 * Low);
            Value *= FMath::Clamp((0.25 - Time) / 0.03, 0.0, 1.0);
        }
        Samples[I] = static_cast<int16>(FMath::Clamp(Value, -0.8, 0.8) * 32767);
        Time += 1.0 / SampleRate;
    }
    return Samples;
}
}
