#include "WorldView/AnastasisSettlementSite.h"
#include "Misc/AutomationTest.h"

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
#endif
