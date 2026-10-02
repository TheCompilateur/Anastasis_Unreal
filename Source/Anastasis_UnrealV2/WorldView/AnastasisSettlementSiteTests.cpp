#include "WorldView/AnastasisSettlementSite.h"
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
#endif
