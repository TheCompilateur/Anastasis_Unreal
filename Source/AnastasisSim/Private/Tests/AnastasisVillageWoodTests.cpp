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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWoodSiteNoGhost,"Anastasis.Sim.Wood.SiteNoGhost",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWoodSiteNoGhost::RunTest(const FString&)
{
    using namespace AnastasisVillage; using namespace AnastasisWorld;
    FWorld W; W.W=32; W.H=24; W.Tiles.SetNum(W.W*W.H);
    for (int32 I=0; I<W.Tiles.Num(); ++I) { auto& T=W.Tiles[I]; T.X=I%W.W; T.Y=I/W.W; T.Type=ETileType::Grass; T.Alt=0.5; }
    auto& Stand=W.Tiles[10*W.W+10]; Stand.Type=ETileType::Forest; Stand.Resource=EResource::Wood; Stand.Amount=4;
    FVillage V; V.Bind(W);
    const FString SiteId=V.OpenSite(WellType,17,11,false);
    if (!TestFalse(TEXT("dry well site exists"),SiteId.IsEmpty())) return false;
    AnastasisNeeds::FNeeds N; N.Hunger=10; N.Thirst=5; N.Energy=90; N.Health=95; N.Morale=60;
    N.Social=80; N.Leisure=80; N.Hygiene=80;
    const FString Cutter=V.SpawnNpc(11.5,10.5,N); V.SetJob(Cutter,TEXT("woodcutter"));
    const FString Builder=V.SpawnNpc(16.5,12.5,N); V.SetJob(Builder,AnastasisBuild::JobBuilder);
    for (int32 I=1; I<=60*60; ++I) V.UpdateActors(27.0+I/60.0,1.0/60.0);
    const FBuilding* Site=V.FindBuilding(SiteId);
    TestEqual(TEXT("forest reserve untouched"),V.LiveTileAt(10,10).Amount,4);
    TestEqual(TEXT("no wood created in cargo"),V.FindNpc(Cutter)->InventoryWood,0);
    TestEqual(TEXT("no wood credited to site"),Site->Materials.StockWood,0);
    TestEqual(TEXT("no wood consumed by builder"),Site->Materials.ConsumedWood,0);
    TestTrue(TEXT("unfinished without wood"),!Site->IsCompleted());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWoodSiteWell,"Anastasis.Sim.Wood.AutonomousWellChain",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWoodSiteWell::RunTest(const FString&)
{
    using namespace AnastasisVillage; using namespace AnastasisWorld;
    FWorld W; W.W=36; W.H=28; W.Tiles.SetNum(W.W*W.H);
    for (int32 I=0; I<W.Tiles.Num(); ++I) { auto& T=W.Tiles[I]; T.X=I%W.W; T.Y=I/W.W; T.Type=ETileType::Grass; T.Alt=0.5; }
    auto& Stand=W.Tiles[10*W.W+10]; Stand.Type=ETileType::Forest; Stand.Resource=EResource::Wood; Stand.Amount=30;
    FVillage V; V.Bind(W);
    const FString ExistingWellId=V.AddBuilding(WellType,7,10);
    if (!TestFalse(TEXT("existing village water for workers"),ExistingWellId.IsEmpty())) return false;
    const FString SiteId=V.OpenSite(WellType,17,11,false);
    if (!TestFalse(TEXT("well site exists"),SiteId.IsEmpty())) return false;
    TestEqual(TEXT("stone is the explicit initial condition"),V.CreditSiteMaterials(SiteId,0,18),18);
    AnastasisNeeds::FNeeds N; N.Hunger=10; N.Thirst=5; N.Energy=90; N.Health=95; N.Morale=60;
    N.Social=80; N.Leisure=80; N.Hygiene=80;
    const FString CutterId=V.SpawnNpc(11.5,10.5,N); V.SetJob(CutterId,TEXT("woodcutter"));
    const FString BuilderId=V.SpawnNpc(16.5,12.5,N); V.SetJob(BuilderId,AnastasisBuild::JobBuilder);
    bool bHarvested=false, bDelivered=false, bAtSite=false, bConserved=true, bSafe=true;
    int32 DeliveryTicks=0, HarvestTicks=0, BuildTicks=0, DeliveryTargetTicks=0, DeliveryDestTicks=0, DeliveryArrivedTicks=0;
    double ClosestDelivery=1000.0, MaxDeliveryWorkTimer=0.0;
    FPoint LastDeliveryTarget{0.0,0.0};
    FString ArrivalSnapshot;
    double Time=27.0;
    for (int32 I=0; I<360*60; ++I)
    {
        Time+=1.0/60.0; V.UpdateActors(Time,1.0/60.0);
        const FNpc* Cutter=V.FindNpc(CutterId);
        const FBuilding* Site=V.FindBuilding(SiteId);
        if (!Cutter || !Site) return false;
        bHarvested |= Cutter->GatheredWood>0;
        DeliveryTicks+=Cutter->Goal==GoalDeliver;
        HarvestTicks+=Cutter->Goal==TEXT("gatherWood");
        BuildTicks+=Cutter->Goal==AnastasisBuild::GoalBuild;
        if (Cutter->Goal==GoalDeliver)
        {
            DeliveryDestTicks+=Cutter->DestBuildingId==SiteId;
            if (Cutter->bHasTarget)
            {
                ++DeliveryTargetTicks;
                LastDeliveryTarget=Cutter->Target;
                const double Distance=FMath::Sqrt(FMath::Square(Cutter->X-Cutter->Target.X)+FMath::Square(Cutter->Y-Cutter->Target.Y));
                ClosestDelivery=FMath::Min(ClosestDelivery,Distance);
                DeliveryArrivedTicks+=Distance<=ArrivalDistance;
                if (Distance<=ArrivalDistance && ArrivalSnapshot.IsEmpty())
                    ArrivalSnapshot=FString::Printf(TEXT("pos=(%.2f,%.2f) target=(%.2f,%.2f) timer=%.2f activity=%s job=%s wood=%d need=%d stock=%d"),
                        Cutter->X,Cutter->Y,Cutter->Target.X,Cutter->Target.Y,Cutter->WorkTimer,*Cutter->Activity,*Cutter->JobId,
                        Cutter->InventoryWood,Site->Materials.NeedWood,Site->Materials.StockWood);
            }
            MaxDeliveryWorkTimer=FMath::Max(MaxDeliveryWorkTimer,Cutter->WorkTimer);
        }
        if (Cutter->MaterialsDelivered>0)
        {
            bDelivered=true;
            bAtSite |= FMath::Sqrt(FMath::Square(Cutter->X-Cutter->Target.X)+FMath::Square(Cutter->Y-Cutter->Target.Y))<=ArrivalDistance;
        }
        bConserved &= V.LiveTileAt(10,10).Amount+Cutter->InventoryWood+Site->Materials.StockWood+Site->Materials.ConsumedWood==30;
        if (Cutter->Goal==GoalDeliver) bSafe &= !Cutter->bPathFailed;
        if (Site->IsCompleted()) break;
    }
    const FBuilding* Site=V.FindBuilding(SiteId);
    const FNpc* Cutter=V.FindNpc(CutterId);
    AddInfo(FString::Printf(TEXT("chain before verdict: goal=%s activity=%s pos=(%.2f,%.2f) gathered=%d held=%d delivered=%d siteWood=%d consumed=%d progress=%.3f thirst=%.1f ticks deliver=%d harvest=%d build=%d target=%d dest=%d arrived=%d closest=%.2f targetPos=(%.2f,%.2f)"),
        Cutter ? *Cutter->Goal : TEXT("missing"),Cutter ? *Cutter->Activity : TEXT("missing"),
        Cutter ? Cutter->X : -1.0,Cutter ? Cutter->Y : -1.0,
        Cutter ? Cutter->GatheredWood : -1,Cutter ? Cutter->InventoryWood : -1,
        Cutter ? Cutter->MaterialsDelivered : -1,Site ? Site->Materials.StockWood : -1,
        Site ? Site->Materials.ConsumedWood : -1,Site ? Site->Progress : -1.0,
        Cutter ? Cutter->Needs.Thirst : -1.0,DeliveryTicks,HarvestTicks,BuildTicks,
        DeliveryTargetTicks,DeliveryDestTicks,DeliveryArrivedTicks,ClosestDelivery,LastDeliveryTarget.X,LastDeliveryTarget.Y));
    AddInfo(FString::Printf(TEXT("delivery arrival=%s maxTimer=%.2f"),*ArrivalSnapshot,MaxDeliveryWorkTimer));
    TestTrue(TEXT("woodcutter autonomously harvested"),bHarvested);
    TestTrue(TEXT("woodcutter physically delivered"),bDelivered && bAtSite);
    TestTrue(TEXT("wood conserved tile cargo site consumed"),bConserved);
    TestTrue(TEXT("woodcutter navigation did not fail"),bSafe);
    TestTrue(TEXT("builder completed well from stock"),Site && Site->IsCompleted());
    if (!Site || !Site->IsCompleted() || !Cutter) return false;
    TestEqual(TEXT("well wood bill consumed"),Site->Materials.ConsumedWood,Site->Materials.NeedWood);
    TestEqual(TEXT("well stone bill consumed"),Site->Materials.ConsumedStone,Site->Materials.NeedStone);
    FNpc* ThirstyCutter=V.FindNpcMutable(CutterId);
    const int32 CutterDrinksBefore=ThirstyCutter->DrinksTaken;
    ThirstyCutter->Needs.Thirst=60;
    AnastasisNeeds::FNeeds Thirsty=N; Thirsty.Thirst=60;
    const FString UserId=V.SpawnNpc(20.5,11.5,Thirsty);
    bool bUsedNewWell=false, bCutterDrankAfterWork=false;
    for (int32 I=0; I<60*60; ++I)
    {
        Time+=1.0/60.0; V.UpdateActors(Time,1.0/60.0);
        const FNpc* User=V.FindNpc(UserId);
        Cutter=V.FindNpc(CutterId);
        if (!User || !Cutter) return false;
        bCutterDrankAfterWork |= Cutter->DrinksTaken>CutterDrinksBefore;
        bConserved &= V.LiveTileAt(10,10).Amount+Cutter->InventoryWood+Site->Materials.StockWood+Site->Materials.ConsumedWood==30;
        if (User->DrinksTaken>0)
        {
            bUsedNewWell=V.AtDrinkSpot(User->X,User->Y)
                && FMath::Sqrt(FMath::Square(User->X-(Site->X+0.5))+FMath::Square(User->Y-(Site->Y+0.5)))<4.0;
        }
        if (bUsedNewWell && bCutterDrankAfterWork) break;
    }
    TestTrue(TEXT("another autonomous inhabitant used the completed well"),bUsedNewWell);
    TestTrue(TEXT("woodcutter can satisfy thirst after work"),bCutterDrankAfterWork);
    TestTrue(TEXT("wood conserved after well use"),bConserved);
    Cutter=V.FindNpc(CutterId);
    AddInfo(FString::Printf(TEXT("wood=%d carried=%d delivered=%d consumed=%d well=%d at t=%.2f"),
        V.LiveTileAt(10,10).Amount,Cutter->InventoryWood,Cutter->MaterialsDelivered,Site->Materials.ConsumedWood,Site->IsCompleted(),Time));
    return true;
}
#endif
