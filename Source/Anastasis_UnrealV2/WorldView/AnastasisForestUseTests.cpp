#include "WorldView/AnastasisForestUse.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
AnastasisWorld::FWorld ForestFixture()
{
    AnastasisWorld::FWorld W; W.W=16; W.H=16; W.Tiles.SetNum(256);
    for (int32 I=0; I<256; ++I) { auto& T=W.Tiles[I]; T.X=I%16; T.Y=I/16; T.Alt=0.5; }
    for (int32 I : {85, 170}) { auto& T=W.Tiles[I]; T.Type=AnastasisWorld::ETileType::Forest; T.Resource=AnastasisWorld::EResource::Wood; T.Amount=40; }
    return W;
}
UHierarchicalInstancedStaticMeshComponent* FixtureMesh()
{
    auto* M=NewObject<UHierarchicalInstancedStaticMeshComponent>();
    M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    return M;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestUseLaw,"Anastasis.ForestUse.LocalMonotonic", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FForestUseLaw::RunTest(const FString&)
{
    using namespace AnastasisForestUse;
    const auto W=ForestFixture(); auto Live=W.Tiles[85];
    TestEqual(TEXT("untouched absent delta"),Depletion(W.Tiles[85],nullptr),0.0);
    int32 Before=0;
    for (int32 Amount=40; Amount>=0; --Amount)
    {
        Live.Amount=Amount; const double F=Depletion(W.Tiles[85],&Live); int32 Hidden=0;
        for (uint32 Seed=0; Seed<128; ++Seed)
        { Hidden+=HideTree(F,Seed); if (HideTree(FMath::Max(0.0,F-0.025),Seed)) TestTrue(TEXT("no removed tree reappears during extraction"),HideTree(F,Seed)); }
        TestTrue(TEXT("monotonic coverage loss"),Hidden>=Before); Before=Hidden;
    }
    TestEqual(TEXT("depleted removes all fixture trees"),Before,128);
    Live.Amount=45; TestEqual(TEXT("no negative extraction"),Depletion(W.Tiles[85],&Live),0.0);
    Live.Resource=AnastasisWorld::EResource::Food; TestEqual(TEXT("conversion is not extraction"),Depletion(W.Tiles[85],&Live),0.0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestUseBinding,"Anastasis.ForestUse.InstancesAndReset", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FForestUseBinding::RunTest(const FString&)
{
    using namespace AnastasisForestUse;
    const auto W=ForestFixture(); auto* M=FixtureMesh(); FBinding B;
    TArray<FTransform> Original;
    for (int32 I=0; I<32; ++I)
    {
        Original.Add(FTransform(FRotator(0,17.0+I,0),FVector(I*100.0,0,0),FVector(2,3,4)));
        B.Add(M,M->AddInstance(Original.Last()),I<16?85:170,uint32(I));
    }
    const int32 Shell=M->AddInstance(FTransform(FVector(700,0,500)));
    B.AddShell(M,Shell,FVector2D(700,0),1600);
    TMap<int32,AnastasisWorld::FTile> Live; Live.Add(85,W.Tiles[85]); Live[85].Amount=0;
    B.Apply(W,Live,true); TestEqual(TEXT("only exploited tile removed"),B.Hidden,16);
    for (int32 I=16; I<32; ++I) { FTransform T; M->GetInstanceTransform(I,T); TestTrue(TEXT("control transform unchanged"),T.Equals(Original[I],0.001)); }
    FTransform S; M->GetInstanceTransform(Shell,S); TestTrue(TEXT("far shell cannot fill opening"),S.GetScale3D().IsNearlyZero());
    B.Apply(W,Live,true); TestEqual(TEXT("stable state no rewrites"),B.Changed,0); TestEqual(TEXT("zero-scale rotation does not lose binding"),B.Conflicts,0);
    B.Apply(W,Live,false); TestEqual(TEXT("off restores all"),B.Hidden,0);
    for (int32 I=0; I<32; ++I) { FTransform T; M->GetInstanceTransform(I,T); TestTrue(TEXT("exact pose restored"),T.Equals(Original[I],0.001)); }
    B.Apply(W,Live,true); Live.Reset(); B.Apply(W,Live,true); TestEqual(TEXT("world reset restores baseline"),B.Hidden,0);
    const FTransform External(FVector(999,999,999)); M->UpdateInstanceTransform(0,External,false);
    B.Apply(W,Live,true); TestEqual(TEXT("external edit relinquished"),B.Conflicts,1);
    FTransform T; M->GetInstanceTransform(0,T); TestTrue(TEXT("external edit preserved"),T.Equals(External));
    B.Reset(); M->ClearInstances(); TestEqual(TEXT("rebuild clears bindings"),B.Count(),0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FForestUseHarvest,"Anastasis.ForestUse.RealHarvestReadOnly", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FForestUseHarvest::RunTest(const FString&)
{
    using namespace AnastasisForestUse;
    auto W=ForestFixture(); AnastasisVillage::FVillage V; V.Bind(W);
    AnastasisNeeds::FNeeds N; N.Hunger=10; N.Thirst=5; N.Energy=90; N.Social=80; N.Leisure=80; N.Hygiene=80; N.Health=95; N.Morale=60;
    const FString Id=V.SpawnNpc(6.5,5.5,N); V.SetJob(Id,TEXT("woodcutter"));
    auto* M=FixtureMesh(); FBinding B;
    for (int32 I=0; I<128; ++I) B.Add(M,M->AddInstance(FTransform(FVector(I*10,0,0))),85,I);
    double Time=27;
    for (int32 I=0; I<3600 && V.FindNpc(Id)->InventoryWood<=9; ++I) { Time+=1.0/60; V.UpdateActors(Time,1.0/60); }
    const int32 Amount=V.LiveTileAt(5,5).Amount, Cargo=V.FindNpc(Id)->InventoryWood;
    B.Apply(W,V.GetLiveTiles(),true);
    TestTrue(TEXT("actual harvested delta opens canopy"),B.Hidden>0 && B.Hidden<128);
    TestEqual(TEXT("presentation does not extract"),V.LiveTileAt(5,5).Amount,Amount);
    TestEqual(TEXT("presentation does not credit cargo"),V.FindNpc(Id)->InventoryWood,Cargo);
    TestEqual(TEXT("harvest conserves wood"),Amount+Cargo,40);
    TestEqual(TEXT("remote control unchanged"),V.LiveTileAt(10,10).Amount,40);
    TestEqual(TEXT("generated baseline immutable"),W.Tiles[85].Amount,40);
    return true;
}
#endif
