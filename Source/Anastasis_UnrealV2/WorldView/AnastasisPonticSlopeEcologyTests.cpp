#include "WorldView/AnastasisEcologicalDressing.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisPonticSlopeEcology,
    "Anastasis.Ecology.PonticSlopeEcology",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisPonticSlopeEcology::RunTest(const FString&)
{
    using namespace AnastasisEcologicalDressing;
    IConsoleVariable* Switch = IConsoleManager::Get().FindConsoleVariable(
        TEXT("anastasis.Dressing.PonticSlopeEcology"));
    if (!TestNotNull(TEXT("live visual switch"), Switch)) return false;
    struct FRestoreSwitch
    {
        IConsoleVariable* Variable;
        int32 Original;
        ~FRestoreSwitch() { Variable->Set(Original, ECVF_SetByCode); }
    } Restore{Switch, Switch->GetInt()};

    auto Snapshot = AnastasisWorldView::CaptureCanonicalWorld(12345u);
    Snapshot.SpatialScale = 5.0;
    Snapshot.bHumanGeography = false;
    for (auto& Tile : Snapshot.Tiles)
    {
        Tile.Type = Tile.X >= 30 && Tile.X < 66 && Tile.Y >= 30 && Tile.Y < 66
            ? AnastasisWorld::ETileType::Forest : AnastasisWorld::ETileType::Ruin;
        Tile.Alt = 0.5;
        Tile.Wetness = 1.0;
    }
    FAnastasisForestDressingSettings Settings;
    Settings.Density = 0.40f;
    Settings.WetnessPenalty = 0.80f;
    AnastasisEcologicalDressing::FRenderedHabitat Habitat;
    Habitat.SampleWaterHeight = [](double, double, double& Z)
    {
        Z = -10000.0;
        return true;
    };

    const auto BuildPair = [&](TFunction<bool(double, double, double&)> Height,
        AnastasisEcologicalDressing::FPlan& Old, AnastasisEcologicalDressing::FPlan& New) -> bool
    {
        Habitat.SampleHeight = MoveTemp(Height);
        FString Error;
        Switch->Set(0, ECVF_SetByCode);
        if (!AnastasisEcologicalDressing::Build(Snapshot, Settings, Old, Error, &Habitat))
        {
            AddError(FString::Printf(TEXT("reference build: %s"), *Error));
            return false;
        }
        Switch->Set(1, ECVF_SetByCode);
        if (!AnastasisEcologicalDressing::Build(Snapshot, Settings, New, Error, &Habitat))
        {
            AddError(FString::Printf(TEXT("Pontic build: %s"), *Error));
            return false;
        }
        return true;
    };

    AnastasisEcologicalDressing::FPlan FlatOld, FlatNew;
    if (!BuildPair([](double, double, double& Z) { Z = 50000.0; return true; }, FlatOld, FlatNew)) return false;
    TestTrue(TEXT("wet flatland has trees to compare"), FlatOld.Instances.Num() > 0);
    TestEqual(TEXT("flat wetland count remains identical"), FlatNew.Instances.Num(), FlatOld.Instances.Num());
    for (int32 Index = 0; Index < FlatOld.Instances.Num() && Index < FlatNew.Instances.Num(); ++Index)
        TestTrue(TEXT("flat wetland positions remain identical"),
            FlatOld.Instances[Index].Ground == FlatNew.Instances[Index].Ground);

    AnastasisEcologicalDressing::FPlan SlopeOld, SlopeNew;
    if (!BuildPair([](double X, double, double& Z)
        {
            Z = 50000.0 + X * FMath::Tan(FMath::DegreesToRadians(20.0));
            return true;
        }, SlopeOld, SlopeNew)) return false;
    TestTrue(TEXT("wet 20-degree slope gains woodland"),
        SlopeNew.Instances.Num() > SlopeOld.Instances.Num());
    for (const AnastasisEcologicalDressing::FPlacement& Tree : SlopeNew.Instances)
    {
        TestTrue(TEXT("slope bounded by planting rule"), Tree.SlopeDegrees <= Settings.HillsideMaxSlope);
        TestTrue(TEXT("forest stays within forest habitat"),
            Snapshot.Tiles.IsValidIndex(Tree.SourceIndex)
            && Snapshot.Tiles[Tree.SourceIndex].Type == AnastasisWorld::ETileType::Forest);
    }
    AddInfo(FString::Printf(TEXT("PONTIC_SLOPE flat=%d/%d wet_slope=%d/%d"),
        FlatOld.Instances.Num(), FlatNew.Instances.Num(),
        SlopeOld.Instances.Num(), SlopeNew.Instances.Num()));
    return true;
}
#endif
