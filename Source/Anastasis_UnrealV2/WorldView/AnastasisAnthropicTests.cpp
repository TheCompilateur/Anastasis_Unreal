#include "WorldView/AnastasisAnthropicMemory.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnthropicObservationTest, "Anastasis.Anthropic.ObservationBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnthropicObservationTest::RunTest(const FString&)
{
    
    AnastasisAnthropic::FMemory M;
    M.Observe(1.0, {{TEXT("a"), FVector2D(10, 10), true}});
    M.Observe(1.0, {{TEXT("a"), FVector2D(100, 10), true}});
    TestEqual(TEXT("duplicate time never adds traffic"), M.GetCells().Num(), 0);
    M.Observe(1.0 + 1.0/60.0, {{TEXT("a"), FVector2D(10, 10), true}});
    TestEqual(TEXT("standing still leaves no tread"), M.GetCells().Num(), 0);
    M.Observe(1.0 + 2.0/60.0, {{TEXT("a"), FVector2D(110, 10), true}});
    TestTrue(TEXT("actual adjacent displacement recorded"), M.GetCells().Num() > 0);
    TestEqual(TEXT("one traverse remains visually invisible"), M.StrengthAt(FVector2D(60, 10)), 0.0);
    const int32 Before = M.GetCells().Num();
    M.Observe(1.0 + 3.0/60.0, {{TEXT("a"), FVector2D(1000, 10), true}});
    TestEqual(TEXT("teleport rejected"), M.RejectedJumps, 1);
    TestEqual(TEXT("teleport creates no corridor"), M.GetCells().Num(), Before);
    M.Observe(2.0, {{TEXT("a"), FVector2D(1100, 10), true}});
    TestEqual(TEXT("unsampled time gap rejected"), M.RejectedGaps, 1);
    TestEqual(TEXT("accelerated gap creates no corridor"), M.GetCells().Num(), Before);
    M.Observe(0.5, {{TEXT("a"), FVector2D(10, 10), true}});
    TestEqual(TEXT("clock reset clears history"), M.GetCells().Num(), 0);
    M.Observe(0.5 + 1.0/60.0, {{TEXT("a"), FVector2D(110, 10), false}});
    TestEqual(TEXT("entering interior creates no trail"), M.GetCells().Num(), 0);
    M.Observe(0.5 + 2.0/60.0, {});
    M.Observe(0.5 + 3.0/60.0, {{TEXT("a"), FVector2D(210, 10), true}});
    TestEqual(TEXT("respawn cannot bridge missing actor"), M.GetCells().Num(), 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnthropicMemoryTest, "Anastasis.Anthropic.RepetitionRecoveryAndBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnthropicMemoryTest::RunTest(const FString&)
{
    
    AnastasisAnthropic::FMemory M;
    double T = 0.0;
    for (int32 I = 0; I < 21; ++I)
    {
        M.Observe(T, {{TEXT("a"), FVector2D(I % 2 == 0 ? 10 : 90, 50), true}});
        T += 1.0/60.0;
    }
    const FVector2D Center(50, 50);
    const double Used = M.StrengthAt(Center);
    TestTrue(TEXT("repetition becomes visible"), Used > 0.9);
    TestEqual(TEXT("distant forest unchanged"), M.StrengthAt(FVector2D(500, 500)), 0.0);
    M.Observe(T + 8.0 * 90.0, {});
    TestTrue(TEXT("abandonment recovers vegetation gradually"), M.StrengthAt(Center) < Used);
    M.Observe(T + 160.0 * 90.0, {});
    TestEqual(TEXT("old weak traces expire"), M.GetCells().Num(), 0);
    M.Reset();
    for (int32 I = 0; I < AnastasisAnthropic::FMemory::MaxCells + 8; ++I)
        M.Observe(I/60.0, {{TEXT("a"), FVector2D(I * 100.0, 50), true}});
    TestTrue(TEXT("memory bounded"), M.GetCells().Num() <= AnastasisAnthropic::FMemory::MaxCells);
    TestTrue(TEXT("overflow explicitly reported"), M.DroppedCells > 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnthropicDistanceTest, "Anastasis.Anthropic.DistanceNotFrames",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnthropicDistanceTest::RunTest(const FString&)
{
    
    AnastasisAnthropic::FMemory A, B;
    A.Observe(0, {{TEXT("a"), FVector2D(10, 50), true}});
    A.Observe(1.0/60.0, {{TEXT("a"), FVector2D(190, 50), true}});
    B.Observe(0, {{TEXT("a"), FVector2D(10, 50), true}});
    B.Observe(1.0/120.0, {{TEXT("a"), FVector2D(100, 50), true}});
    B.Observe(1.0/60.0, {{TEXT("a"), FVector2D(190, 50), true}});
    double DA = 0, DB = 0;
    for (const auto& P : A.GetCells()) DA += P.Value.Metres;
    for (const auto& P : B.GetCells()) DB += P.Value.Metres;
    TestTrue(TEXT("distance conserved across grid boundary"), FMath::Abs(DA - 1.8) < 1.e-9);
    TestTrue(TEXT("subdivision differs only by decay time"), FMath::Abs(DA - DB) < 0.0001);
    return true;
}
#endif
