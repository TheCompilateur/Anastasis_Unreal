#include "WorldView/AnastasisEcologicalDressing.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
AnastasisWorldView::FWorldVisualSnapshot FlatForestEdge()
{
    auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
    for (auto& T : S.Tiles)
    {
        T.Type = T.X >= 48 ? AnastasisWorld::ETileType::Forest : AnastasisWorld::ETileType::Grass;
        T.Alt = 0.5; T.Wetness = 0;
    }
    return S;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisEcologyDeterminism, "Anastasis.Ecology.DeterminismAndAnchoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisEcologyDeterminism::RunTest(const FString&)
{
    using namespace AnastasisEcologicalDressing;
    const auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
    const auto Before = S.Tiles;
    FAnastasisForestDressingSettings C;
    AnastasisEcologicalDressing::FPlan A,B;
    FString E;
    TestTrue(TEXT("canonical plan A"), Build(S,C,A,E));
    TestTrue(TEXT("canonical plan B"), Build(S,C,B,E));
    TestTrue(TEXT("reachable forest"), A.Instances.Num() > 0);
    TestEqual(TEXT("deterministic count"),A.Instances.Num(),B.Instances.Num());
    int32 Layers[3]={};
    for (int32 I=0; I<A.Instances.Num(); ++I)
    {
        const auto& P=A.Instances[I];
        if (B.Instances.IsValidIndex(I))
        {
            TestTrue(TEXT("exact placement"),P.Ground == B.Instances[I].Ground);
            TestEqual(TEXT("exact scale"),P.ScaleMultiplier,B.Instances[I].ScaleMultiplier);
            TestEqual(TEXT("exact layer"),static_cast<int32>(P.Layer),static_cast<int32>(B.Instances[I].Layer));
            TestEqual(TEXT("exact variant seed"),P.VisualSeed,B.Instances[I].VisualSeed);
        }
        double Z;
        TestTrue(TEXT("rendered ground exists"),AnastasisTerrainSurface::SampleHeight(S,P.Ground.X,P.Ground.Y,Z));
        TestTrue(TEXT("anchored exactly"),FMath::IsNearlyEqual(Z,P.Ground.Z,1.e-9));
        TestTrue(TEXT("above water"),Z > AnastasisTerrainSurface::WaterPlaneZ+C.WaterClearanceUU);
        TestTrue(TEXT("slope bound"),P.SlopeDegrees <= C.MaxSlopeDegrees);
        ++Layers[static_cast<uint8>(P.Layer)];
        for (int32 J=0; J<I; ++J)
            if (FVector::DistSquared2D(P.Ground,A.Instances[J].Ground) < FMath::Square(C.MinimumSpacing*100.0)-1.e-6)
                AddError(TEXT("minimum spacing violated"));
    }
    for (int32 I=0; I<S.Tiles.Num(); ++I)
        if (S.Tiles[I].Alt != Before[I].Alt || S.Tiles[I].Type != Before[I].Type
            || S.Tiles[I].Wetness != Before[I].Wetness || S.Tiles[I].Amount != Before[I].Amount)
            AddError(TEXT("snapshot mutated"));
    TestTrue(TEXT("three size layers reachable"), Layers[0]>0 && Layers[1]>0 && Layers[2]>0);
    AddInfo(FString::Printf(TEXT("seed=12345 instances=%d young=%d secondary=%d canopy=%d water=%d slope=%d spacing=%d"),
        A.Instances.Num(),Layers[0],Layers[1],Layers[2],A.RejectedWaterOrFootprint,A.RejectedSlope,A.RejectedSpacing));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisEcologyConditioning, "Anastasis.Ecology.EdgeAndConditioning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisEcologyConditioning::RunTest(const FString&)
{
    using namespace AnastasisEcologicalDressing;
    auto S=FlatForestEdge();
    FAnastasisForestDressingSettings C;
    AnastasisEcologicalDressing::FPlan A,Wet,Water,Steep,Disabled;
    FString E;
    TestTrue(TEXT("edge fixture"),Build(S,C,A,E));
    int32 Fringe=0, Interior=0, YoungFringe=0, CanopyInterior=0;
    for (const auto& P:A.Instances)
    {
        const double X=P.Ground.X/100.0;
        TestTrue(TEXT("no distant prairie scatter"),X > 45.0);
        if (X<48.0) { ++Fringe; YoungFringe += P.Layer==ELayer::Young; }
        if (X>=50.0 && X<53.0) { ++Interior; CanopyInterior += P.Layer==ELayer::Canopy; }
    }
    TestTrue(TEXT("graded presence beyond semantic forest"),Fringe>0);
    TestTrue(TEXT("interior denser than equal-width fringe"),Interior>Fringe);
    TestTrue(TEXT("young fringe and mature interior"),YoungFringe>0 && CanopyInterior>0);
    for (auto& T:S.Tiles) T.Wetness=1;
    TestTrue(TEXT("wet fixture"),Build(S,C,Wet,E));
    TestTrue(TEXT("wetness reduces population"),Wet.Instances.Num()<A.Instances.Num());
    for (auto& T:S.Tiles) T.Alt=0.20;
    TestTrue(TEXT("submerged fixture"),Build(S,C,Water,E));
    TestEqual(TEXT("zero submerged trees"),Water.Instances.Num(),0);
    for (auto& T:S.Tiles) { T.Alt=0.5+T.X*0.15; T.Wetness=0; }
    TestTrue(TEXT("steep fixture"),Build(S,C,Steep,E));
    TestEqual(TEXT("zero impossible-slope trees"),Steep.Instances.Num(),0);
    C.bEnabled=false;
    TestTrue(TEXT("disable accepted"),Build(S,C,Disabled,E));
    TestEqual(TEXT("disabled empty"),Disabled.Instances.Num(),0);
    AddInfo(FString::Printf(TEXT("fringe=%d interior_equal_width=%d dry=%d wet=%d submerged=%d steep=%d"),
        Fringe,Interior,A.Instances.Num(),Wet.Instances.Num(),Water.Instances.Num(),Steep.Instances.Num()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisEcologyBoundary, "Anastasis.Ecology.RejectInvalidInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisEcologyBoundary::RunTest(const FString&)
{
    using namespace AnastasisEcologicalDressing;
    auto S=FlatForestEdge();
    FAnastasisForestDressingSettings C;
    AnastasisEcologicalDressing::FPlan P; FString E;
    S.Tiles[17].Wetness=std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("NaN rejected"),Build(S,C,P,E));
    TestTrue(TEXT("precise tile path"),E.Contains(TEXT("Source.Tiles[17]")));
    TestEqual(TEXT("no partial placements"),P.Instances.Num(),0);
    S=FlatForestEdge(); C.MinimumSpacing=0;
    TestFalse(TEXT("invalid configuration rejected"),Build(S,C,P,E));
    C=FAnastasisForestDressingSettings{};
    S=AnastasisWorldView::CropSnapshot(S,0,0,32,32);
    TestFalse(TEXT("crop cannot invent missing ecological context"),Build(S,C,P,E));
    return true;
}
#endif
