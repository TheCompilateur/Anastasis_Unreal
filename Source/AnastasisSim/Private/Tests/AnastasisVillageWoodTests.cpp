#include "Misc/AutomationTest.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"
#include "Work/AnastasisWoodHarvest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWoodHarvestBounds,"Anastasis.Sim.Wood.Bounds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWoodHarvestBounds::RunTest(const FString&)
{
    namespace W=AnastasisWoodHarvest;
    TestEqual(TEXT("stand reserve"),W::Floor(true,0,0,false),4);
    TestEqual(TEXT("deep reserve"),W::Floor(true,0.55,0,false),10);
    TestEqual(TEXT("frontier exemption"),W::Floor(true,0.55,0,true),0);
    TestEqual(TEXT("scrub reserve"),W::Floor(false,0,0,false),0);
    TestEqual(TEXT("clamp at reserve"),W::Take(5,4,3),1);
    TestEqual(TEXT("empty"),W::Take(0,4,3),0);
    TestEqual(TEXT("negative request"),W::Take(20,4,-3),0);
    TestEqual(TEXT("base yield"),W::Yield(1),3);
    TestEqual(TEXT("skill threshold"),W::Yield(1.35),4);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWoodHarvestCircuit,"Anastasis.Sim.Wood.LocalConservation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWoodHarvestCircuit::RunTest(const FString&)
{
    using namespace AnastasisVillage;
    using namespace AnastasisWorld;
    FWorld World; World.W=32; World.H=24; World.Tiles.SetNum(World.W*World.H);
    for(int Y=0;Y<World.H;++Y) for(int X=0;X<World.W;++X) {
        auto& T=World.Tiles[Y*World.W+X]; T.X=X; T.Y=Y; T.Type=ETileType::Grass; T.Alt=0.5;
    }
    auto& Source=World.Tiles[10*World.W+10]; Source.Type=ETileType::Forest; Source.Resource=EResource::Wood; Source.Amount=30;
    auto& Control=World.Tiles[20*World.W+28]; Control.Type=ETileType::Forest; Control.Resource=EResource::Wood; Control.Amount=30;
    FVillage V; V.Bind(World);
    AnastasisNeeds::FNeeds N; N.Hunger=10; N.Thirst=5; N.Energy=90; N.Social=80; N.Leisure=80; N.Hygiene=80; N.Health=95; N.Morale=60;
    const FString Id=V.SpawnNpc(11.5,10.5,N);
    TestTrue(TEXT("existing SetJob accepts woodcutter"),V.SetJob(Id,TEXT("woodcutter")));
    bool Cut=false;
    for(int Tick=1;Tick<=3600;++Tick) {
        V.UpdateActors(27.0+Tick/60.0,1.0/60.0);
        const FNpc* P=V.FindNpc(Id); if(!P) return false;
        const int32 Left=V.LiveTileAt(10,10).Amount;
        if(Left+P->InventoryWood!=30) { AddError(TEXT("wood conservation broken")); return false; }
        if(V.LiveTileAt(28,20).Amount!=30) { AddError(TEXT("remote control changed")); return false; }
        if(Left<4) { AddError(TEXT("forest reserve crossed")); return false; }
        Cut |= P->InventoryWood>0;
    }
    const FNpc* P=V.FindNpc(Id);
    TestTrue(TEXT("autonomous decision and real local removal"),Cut);
    TestTrue(TEXT("finite haul stops extraction"),P->InventoryWood>9 && P->InventoryWood<=13);
    TestEqual(TEXT("confirmed deeds match retained wood"),P->GatheredWood,P->InventoryWood);
    TestEqual(TEXT("generated source remains immutable"),Source.Amount,30);
    TestEqual(TEXT("food inventory unchanged"),P->InventoryFood,0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWoodHarvestFloor,"Anastasis.Sim.Wood.ExhaustedStand",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWoodHarvestFloor::RunTest(const FString&)
{
    using namespace AnastasisVillage; using namespace AnastasisWorld;
    FWorld W; W.W=16; W.H=16; W.Tiles.SetNum(256);
    for(int I=0;I<256;++I) { auto& T=W.Tiles[I]; T.X=I%16; T.Y=I/16; T.Type=ETileType::Grass; }
    auto& T=W.Tiles[8*16+8]; T.Type=ETileType::Forest; T.Resource=EResource::Wood; T.Amount=5;
    FVillage V; V.Bind(W); AnastasisNeeds::FNeeds N; N.Hunger=10; N.Thirst=5; N.Energy=90; N.Social=80; N.Leisure=80; N.Hygiene=80; N.Health=95; N.Morale=60;
    const FString Id=V.SpawnNpc(9.5,8.5,N); V.SetJob(Id,TEXT("woodcutter"));
    for(int I=1;I<=1800;++I) V.UpdateActors(27.0+I/60.0,1.0/60.0);
    TestEqual(TEXT("one removable unit"),V.FindNpc(Id)->InventoryWood,1);
    TestEqual(TEXT("stand stays at reserve"),V.LiveTileAt(8,8).Amount,4);
    TestEqual(TEXT("forest is not turned into a field"),(int32)V.LiveTileAt(8,8).Type,(int32)ETileType::Forest);
    return true;
}
#endif
