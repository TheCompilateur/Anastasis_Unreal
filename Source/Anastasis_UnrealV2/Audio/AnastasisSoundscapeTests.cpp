#include "Audio/AnastasisSoundscapeSignal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace AnastasisSoundscape;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundscapeCausality, "Anastasis.Audio.Soundscape.Causality",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoundscapeCausality::RunTest(const FString&)
{
    FTrack Track;
    FObservation O;
    O.bVisible = true;
    O.Session = TEXT("site1");
    O.Swings = 4;
    O.LastSwing = 1;
    TestFalse(TEXT("initial observation is not a historical hammer strike"), Track.Observe(O, .1).bWork);
    TestFalse(TEXT("work intention alone makes no sound"), Track.Observe(O, .1).bWork);
    ++O.Swings; O.LastSwing = 2;
    TestTrue(TEXT("observed new swing sounds"), Track.Observe(O, .1).bWork);
    TestFalse(TEXT("same swing cannot repeat"), Track.Observe(O, .1).bWork);
    O.Position.X += 90;
    TestTrue(TEXT("visible stride sounds"), Track.Observe(O, .2).bStep);
    TestFalse(TEXT("stationary body cannot walk"), Track.Observe(O, .3).bStep);
    O.Position.X += 5000; ++O.Swings; O.LastSwing = 3;
    const FEvents Teleport = Track.Observe(O, .1);
    TestFalse(TEXT("teleport does not walk"), Teleport.bStep);
    TestFalse(TEXT("teleport consumes stale work"), Teleport.bWork);
    O.bVisible = false; ++O.Swings; O.LastSwing = 4;
    TestFalse(TEXT("inside worker is silent"), Track.Observe(O, .2).bWork);
    O.bVisible = true;
    TestFalse(TEXT("exit does not replay inside work"), Track.Observe(O, .2).bWork);
    O.Swings += 100; O.LastSwing = 100;
    TestTrue(TEXT("warp produces at most one event"), Track.Observe(O, .2).bWork);
    TestFalse(TEXT("warp has no backlog"), Track.Observe(O, .2).bWork);
    O.Session = TEXT("site2"); O.Swings = 200; O.LastSwing = 200;
    TestFalse(TEXT("new session cannot inherit swings"), Track.Observe(O, .2).bWork);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundscapeSignal, "Anastasis.Audio.Soundscape.Signal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSoundscapeSignal::RunTest(const FString&)
{
    for (const ESound Sound : {ESound::Step, ESound::Work, ESound::Water})
    {
        FSynth Synth(123);
        const auto PCM = Synth.Render(Sound, SampleRate / 4);
        int32 Peak = 0;
        for (const int16 Sample : PCM) Peak = FMath::Max(Peak, FMath::Abs(static_cast<int32>(Sample)));
        TestTrue(TEXT("nonzero, bounded PCM"), Peak > 100 && Peak < 27000);
        if (Sound != ESound::Water)
        {
            TestEqual(TEXT("impulse starts at zero"), PCM[0], static_cast<int16>(0));
            TestTrue(TEXT("impulse retires silently"), FMath::Abs(static_cast<int32>(PCM.Last())) <= 1);
        }
    }
    FSynth Whole(71), Blocks(71);
    const auto Reference = Whole.Render(ESound::Water, SampleRate);
    auto Split = Blocks.Render(ESound::Water, SampleRate / 2);
    Split.Append(Blocks.Render(ESound::Water, SampleRate / 2));
    TestTrue(TEXT("water block boundary is sample-identical to continuous synthesis"), Reference == Split);
    return true;
}
#endif
