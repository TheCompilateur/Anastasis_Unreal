#include "WorldView/AnastasisSettlementSite.h"
#include "WorldView/AnastasisSettlementSurvey.h"
#include "WorldView/AnastasisWorldView.h"
#include "Sim/AnastasisSimulation.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisSettlementSiteTests
{
AnastasisSettlementSite::FInputs Valley()
{
    AnastasisSettlementSite::FInputs In; In.W=32; In.H=24; In.TileMetres=20; In.Cells.SetNum(In.W*In.H);
    for(int32 I=0;I<In.Cells.Num();++I)
    {
        auto& C=In.Cells[I]; C.bSurveyed=C.bWalkable=C.bDry=C.bCenterAllowed=true; C.Slope=2;
        const int32 X=I%In.W;
        if(X==4) { C.bWater=true; C.bWalkable=C.bDry=C.bCenterAllowed=false; }
        if(X==24) { C.bWood=true; C.bWalkable=C.bCenterAllowed=false; }
        if(X==18) { C.bFood=true; C.bCenterAllowed=false; }
    }
    In.Cells[12*In.W+16].Slope=35; // legacy central choice is physically steep
    return In;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettlementGeographyTest,"Anastasis.SettlementSite.GeographicChoice",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSettlementGeographyTest::RunTest(const FString&)
{
    auto In=AnastasisSettlementSiteTests::Valley();
    const auto A=AnastasisSettlementSite::Choose(In), B=AnastasisSettlementSite::Choose(In);
    TestTrue(TEXT("candidate found"),A.Best.bEligible);
    TestFalse(TEXT("central steep site rejected"),A.Legacy.bEligible);
    TestTrue(TEXT("choice moves from arbitrary centre"),A.Best.Index!=A.Legacy.Index);
    TestEqual(TEXT("same terrain same choice"),A.Best.Index,B.Best.Index);
    TestTrue(TEXT("gentle site"),A.Best.Slope<=8.0);
    TestTrue(TEXT("connected expansion area"),A.Best.AreaM2>=3600.0);
    TestTrue(TEXT("water reached along graph"),A.Best.WaterM>=0 && A.Best.WaterM<=300);
    TestTrue(TEXT("food reached along graph"),A.Best.FoodM>=0 && A.Best.FoodM<=600);
    TestTrue(TEXT("wood reached along graph"),A.Best.WoodM>=0 && A.Best.WoodM<=600);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettlementBarrierTest,"Anastasis.SettlementSite.BarriersAndMissingEvidence",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSettlementBarrierTest::RunTest(const FString&)
{
    auto In=AnastasisSettlementSiteTests::Valley();
    // Water is nearby in Euclidean distance but across a complete impassable ridge.
    for(int32 Y=0;Y<In.H;++Y) In.Cells[Y*In.W+8].Slope=60;
    auto R=AnastasisSettlementSite::Choose(In);
    TestFalse(TEXT("no site bridges an impassable ridge for resources"),R.Best.bEligible);
    In=AnastasisSettlementSiteTests::Valley();
    for(auto& C:In.Cells) C.bSurveyed=false;
    R=AnastasisSettlementSite::Choose(In);
    TestFalse(TEXT("missing rendered ground never becomes PASS"),R.Best.bEligible);
    TestEqual(TEXT("no implicit centre fallback"),R.Best.Index,INDEX_NONE);
    In=AnastasisSettlementSiteTests::Valley();
    for(int32 I=0;I<In.Cells.Num();++I) In.Cells[I].bCenterAllowed=(I/In.W==1);
    TestFalse(TEXT("opening placement margin respected"),AnastasisSettlementSite::Choose(In).Best.bEligible);
    In.Cells.Reset();
    TestFalse(TEXT("malformed input rejected"),AnastasisSettlementSite::Choose(In).bValidInput);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettlementResourceTest,"Anastasis.SettlementSite.ResourcesDriveChoice",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSettlementResourceTest::RunTest(const FString&)
{
    auto In=AnastasisSettlementSiteTests::Valley();
    const auto A=AnastasisSettlementSite::Choose(In);
    // Mirror the entire territory: choice follows resources, not the fixed centre.
    auto Mirrored=In;
    for(int32 Y=0;Y<In.H;++Y) for(int32 X=0;X<In.W;++X)
        Mirrored.Cells[Y*In.W+X]=In.Cells[Y*In.W+In.W-1-X];
    const auto B=AnastasisSettlementSite::Choose(Mirrored);
    TestTrue(TEXT("both scenarios eligible"),A.Best.bEligible && B.Best.bEligible);
    TestTrue(TEXT("site follows changed geography"),A.Best.Index%In.W!=B.Best.Index%In.W);
    for(auto& C:In.Cells) C.bFood=false;
    TestFalse(TEXT("no invented farming resource"),AnastasisSettlementSite::Choose(In).Best.bEligible);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettlementConcordanceTest,"Anastasis.SettlementSite.WaterConcordance",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSettlementConcordanceTest::RunTest(const FString&)
{
    auto In=AnastasisSettlementSiteTests::Valley();
    const auto Before=AnastasisSettlementSite::Choose(In);
    auto Read=[&]()
    {
        TSharedPtr<FJsonObject> Root;
        const auto Reader=TJsonReaderFactory<>::Create(AnastasisSettlementSite::ToJson(Before,In));
        FJsonSerializer::Deserialize(Reader,Root);
        return Root;
    };
    auto Root=Read();
    TestTrue(TEXT("JSON parsed"),Root.IsValid());
    if(!Root) return false;
    auto Water=Root->GetObjectField(TEXT("water_concordance"));
    TestEqual(TEXT("unmeasured never agreement"),Water->GetStringField(TEXT("agreement")),FString(TEXT("UNKNOWN")));
    TestEqual(TEXT("unknown covers all cells"),Water->GetArrayField(TEXT("unknown")).Num(),In.Cells.Num());
    for(int32 I=0;I<4;++I) In.Cells[I].bWaterObserved=true;
    In.Cells[1].bSimWater=In.Cells[1].bRenderedWater=true;
    In.Cells[2].bSimWater=true; In.Cells[3].bRenderedWater=true;
    Root=Read(); Water=Root->GetObjectField(TEXT("water_concordance"));
    TestEqual(TEXT("mismatches explicit"),Water->GetStringField(TEXT("agreement")),FString(TEXT("MISMATCH")));
    TestEqual(TEXT("both directions counted"),Water->GetNumberField(TEXT("mismatch_cells")),2.0);
    TestEqual(TEXT("render-only position preserved"),Water->GetArrayField(TEXT("render_only"))[0]->AsNumber(),3.0);
    TestEqual(TEXT("semantic-only position preserved"),Water->GetArrayField(TEXT("simulation_only"))[0]->AsNumber(),2.0);
    const auto After=AnastasisSettlementSite::Choose(In);
    TestEqual(TEXT("diagnostic cannot change selected site"),Before.Best.Index,After.Best.Index);
    TestEqual(TEXT("diagnostic cannot change site score"),Before.Best.Score,After.Best.Score);
    In.Cells[2].bSimWater=false; In.Cells[3].bRenderedWater=false;
    Root=Read();
    TestEqual(TEXT("missing coverage is partial"),Root->GetObjectField(TEXT("water_concordance"))->GetStringField(TEXT("agreement")),FString(TEXT("PARTIAL")));
    for(auto& C:In.Cells) C.bWaterObserved=true;
    Root=Read();
    TestEqual(TEXT("complete matching centres only"),Root->GetObjectField(TEXT("water_concordance"))->GetStringField(TEXT("agreement")),FString(TEXT("AGREEMENT_AT_CENTRES")));
    return true;
}

// SITE_FROM_SIM_001 -- the opening site from the simulation's own tiles, on the canonical world.
// GEO_MEASURE_001 measured the rendered survey moving the village 0.5-0.7 km under render CVars.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSettlementFromSimulationTest,"Anastasis.SettlementSite.FromSimulation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSettlementFromSimulationTest::RunTest(const FString&)
{
    FAnastasisSimulation Sim;
    Sim.Reset(AnastasisWorldView::ReferenceSeed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
    auto ReadSim=[&]()
    {
        AnastasisSettlementSite::FInputs In;
        AnastasisSettlementSurvey::ReadSimulation(Sim.GetSeed(), Sim.GetWorld(), Sim.GetVillage(), In);
        return In;
    };
    const auto In=ReadSim();
    TestEqual(TEXT("selection source"),In.SelectionSource,FString(TEXT("simulation")));
    TestEqual(TEXT("every tile surveyed"),In.Cells.Num(),Sim.GetWorld().Tiles.Num());
    TestEqual(TEXT("canonical 20 m tile"),In.TileMetres,20.0);
    // Calibration sweep, reported (not asserted): what the relief factor does to the choice.
    for(const double Relief : {1.0, 1.5, 2.0, 2.5, 3.0, 3.6})
    {
        AnastasisSettlementSite::FInputs S;
        AnastasisSettlementSurvey::ReadSimulation(Sim.GetSeed(), Sim.GetWorld(), Sim.GetVillage(), S, Relief);
        TArray<double> Slopes;
        for(const auto& C : S.Cells) if(C.bDry) Slopes.Add(C.Slope);
        Slopes.Sort();
        const auto R=AnastasisSettlementSite::Choose(S);
        AddInfo(FString::Printf(TEXT("SITE_FROM_SIM_SWEEP relief=%.1f land_slope_p50=%.2f p90=%.2f eligible=%d site=(%d,%d) score=%.2f water_m=%.0f error=%s"),
            Relief, Slopes.IsEmpty() ? -1.0 : Slopes[Slopes.Num()/2], Slopes.IsEmpty() ? -1.0 : Slopes[Slopes.Num()*9/10],
            R.Eligible, R.Best.Index<0 ? -1 : R.Best.Index%S.W, R.Best.Index<0 ? -1 : R.Best.Index/S.W, R.Best.Score, R.Best.WaterM, *R.Error));
    }
    const auto A=AnastasisSettlementSite::Choose(In);
    if(!TestTrue(TEXT("an eligible site on the canonical world"),A.Best.bEligible)) { AddInfo(A.Error); return false; }
    const auto& T=Sim.GetWorld().Tiles[A.Best.Index];
    TestTrue(TEXT("site is dry land"),T.Type!=AnastasisWorld::ETileType::Water);
    TestTrue(TEXT("gentle site"),A.Best.Slope<=8.0);
    TestTrue(TEXT("water within reach"),A.Best.WaterM>=0 && A.Best.WaterM<=300);
    AddInfo(FString::Printf(TEXT("SITE_FROM_SIM site=(%d,%d) score=%.2f eligible=%d slope=%.2f water=%.0f food=%.0f wood=%.0f"),
        A.Best.Index%In.W, A.Best.Index/In.W, A.Best.Score, A.Eligible, A.Best.Slope, A.Best.WaterM, A.Best.FoodM, A.Best.WoodM));

    // The render CVars that moved the village in GEO_MEASURE_001 must be invisible here.
    struct FCVar { const TCHAR* Name; int32 Old; };
    TArray<FCVar> Changed;
    for(const TCHAR* Name : {TEXT("anastasis.Terrain.Drainage"), TEXT("anastasis.Terrain.HumanGeography"), TEXT("anastasis.Terrain.Forge")})
    {
        if(IConsoleVariable* Var=IConsoleManager::Get().FindConsoleVariable(Name)) { Changed.Add({Name,Var->GetInt()}); Var->Set(0,ECVF_SetByCode); }
    }
    const auto B=AnastasisSettlementSite::Choose(ReadSim());
    for(const FCVar& C : Changed) IConsoleManager::Get().FindConsoleVariable(C.Name)->Set(C.Old,ECVF_SetByCode);
    TestEqual(TEXT("render CVars cannot move the simulated site"),B.Best.Index,A.Best.Index);
    TestEqual(TEXT("... nor its score"),B.Best.Score,A.Best.Score);

    // The rendered observation is reported, it never chooses.
    AnastasisSettlementSite::FInputs Rendered=In;
    Rendered.SelectionSource=TEXT("rendered_relief");
    for(auto& C : Rendered.Cells) { C.bWaterObserved=true; C.bRenderedWater=true; C.RenderedSlope=45.0; C.Slope=45.0; C.bCenterAllowed=false; }
    auto Merged=In;
    AnastasisSettlementSurvey::MergeRenderObservation(Rendered,Merged);
    const auto M=AnastasisSettlementSite::Choose(Merged);
    TestEqual(TEXT("observation cannot move the site"),M.Best.Index,A.Best.Index);
    TestEqual(TEXT("observation copied"),Merged.Cells[A.Best.Index].RenderedSlope,45.0);
    TestEqual(TEXT("selection source kept"),Merged.SelectionSource,FString(TEXT("simulation")));

    // anastasis.Village.SiteSource 0 hands back the rendered inputs untouched (A/B and rollback).
    IConsoleVariable* Source=IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Village.SiteSource"));
    if(TestNotNull(TEXT("SiteSource CVar registered"),Source))
    {
        const int32 Old=Source->GetInt();
        Source->Set(0,ECVF_SetByCode);
        const auto Legacy=AnastasisSettlementSurvey::SiteInputs(Rendered,true,Sim.GetSeed(),Sim.GetWorld(),Sim.GetVillage());
        Source->Set(1,ECVF_SetByCode);
        const auto Current=AnastasisSettlementSurvey::SiteInputs(Rendered,true,Sim.GetSeed(),Sim.GetWorld(),Sim.GetVillage());
        Source->Set(Old,ECVF_SetByCode);
        TestEqual(TEXT("SiteSource 0 = rendered relief"),Legacy.SelectionSource,FString(TEXT("rendered_relief")));
        TestEqual(TEXT("SiteSource 1 = simulation"),Current.SelectionSource,FString(TEXT("simulation")));
        TestEqual(TEXT("SiteSource 1 chooses the simulated site"),AnastasisSettlementSite::Choose(Current).Best.Index,A.Best.Index);
    }
    return true;
}
#endif
