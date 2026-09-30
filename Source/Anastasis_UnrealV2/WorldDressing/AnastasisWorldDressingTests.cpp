#include "WorldDressing/AnastasisWorldDressingPlacement.h"
#include "WorldDressing/AnastasisWorldDressingManager.h"
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
using namespace AnastasisWorldDressing;
FMap Fixture()
{
    FMap M; auto& S=M.Snapshot; S.W=8; S.H=8; S.SourceW=96; S.SourceH=96;
    for (int32 Y=0;Y<8;++Y) for (int32 X=0;X<8;++X)
    {
        AnastasisWorldView::FVisualTile T; T.X=X;T.Y=Y;T.Alt=0.5;
        T.Type=X==0?AnastasisWorld::ETileType::Water:AnastasisWorld::ETileType::Grass; S.Tiles.Add(T);
    }
    const double Extent=8*AnastasisWorldView::TileWorldSize;
    M.Triangles.Add({FVector(0,0,500),FVector(Extent,0,500),FVector(0,Extent,500)});
    M.Triangles.Add({FVector(Extent,0,500),FVector(Extent,Extent,500),FVector(0,Extent,500)});
    return M;
}
FAnastasisDressingRule Rule()
{
    FAnastasisDressingRule R; R.AssetId=TEXT("test"); R.Density=2;
    R.StaticMesh=TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
    return R;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldDressingDeterminism,"Anastasis.WorldDressing.DeterminismAndBounds",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldDressingDeterminism::RunTest(const FString&)
{
    auto M=Fixture(); FString E; TestTrue(TEXT("prepare"),M.Prepare(E));
    auto R=Rule(); R.ClusterRadius=120; TArray<FAnastasisDressingRule> Rules={R};
    FResult A,B,C; TestTrue(TEXT("build"),Build(M,Rules,42,20000,A,E));
    TestTrue(TEXT("repeat"),Build(M,Rules,42,20000,B,E));
    TestTrue(TEXT("different seed"),Build(M,Rules,43,20000,C,E));
    TestTrue(TEXT("reachable"),A.Placements.Num()>0);
    TestEqual(TEXT("same seed hash"),A.Hash,B.Hash);
    TestTrue(TEXT("changed seed changes placements"),A.Hash!=C.Hash);
    for (const auto& P:A.Placements)
    {
        TestTrue(TEXT("in bounds"),M.TileAt(P.Ground.X,P.Ground.Y)!=nullptr);
        TestTrue(TEXT("not in water"),M.IsDry(FVector2D(P.Ground.X,P.Ground.Y)));
        TestEqual(TEXT("rendered triangle height"),P.Ground.Z,500.0);
    }
    TestTrue(TEXT("limit"),Build(M,Rules,42,5,B,E));
    TestEqual(TEXT("cap enforced"),B.Placements.Num(),5);TestTrue(TEXT("cap reported"),B.bCapped);
    AddInfo(FString::Printf(TEXT("seed=42 instances=%d hash=%s"),A.Placements.Num(),*A.Hash));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldDressingFilters,"Anastasis.WorldDressing.FiltersAndInvalidInput",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldDressingFilters::RunTest(const FString&)
{
    auto M=Fixture(); FString E; TestTrue(TEXT("prepare"),M.Prepare(E)); auto R=Rule(); FResult A;
    auto Count=[&](const FAnastasisDressingRule& V) { TestTrue(TEXT("valid rule"),Build(M,{V},42,20000,A,E)); return A.Placements.Num(); };
    TestTrue(TEXT("baseline"),Count(R)>0);
    auto V=R;V.AltitudeMin=501;TestEqual(TEXT("altitude exclusion"),Count(V),0);
    V=R;V.SlopeMin=1;TestEqual(TEXT("slope exclusion"),Count(V),0);
    V=R;V.AllowedTerrainFamilies={EAnastasisDressingTerrain::Forest};TestEqual(TEXT("family exclusion"),Count(V),0);
    V=R;V.DistanceToWaterMin=9*AnastasisWorldView::TileWorldSize;TestEqual(TEXT("water distance exclusion"),Count(V),0);
    V=R;V.StaticMesh.Reset();TestEqual(TEXT("unassigned mesh supported"),Count(V),0);
    M.Buildings.Add(FBox2D(FVector2D(0,0),FVector2D(8*AnastasisWorldView::TileWorldSize,8*AnastasisWorldView::TileWorldSize)));
    TestEqual(TEXT("building exclusion"),Count(R),0);V=R;V.bAvoidBuildings=false;TestTrue(TEXT("building toggle"),Count(V)>0);
    M.Buildings.Reset();M.Roads.Add(FBox2D(FVector2D(0,0),FVector2D(8*AnastasisWorldView::TileWorldSize,8*AnastasisWorldView::TileWorldSize)));
    TestEqual(TEXT("road exclusion"),Count(R),0);V=R;V.bAvoidRoads=false;TestTrue(TEXT("road toggle"),Count(V)>0);
    V=R;V.Density=std::numeric_limits<float>::quiet_NaN();TestFalse(TEXT("NaN rejected"),Build(M,{V},42,20000,A,E));
    TestFalse(TEXT("duplicate ID rejected"),Build(M,{R,R},42,20000,A,E));
    M=Fixture();for (auto& T:M.Triangles) { T.A.Z=200;T.B.Z=200;T.C.Z=200; }
    TestTrue(TEXT("submerged geometry"),M.Prepare(E));V=R;V.bAvoidWater=false;
    TestEqual(TEXT("water forbidden even with margin disabled"),Count(V),0);
    M=Fixture(); M.Snapshot.Tiles[2].Alt=std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("invalid map rejected"),M.Prepare(E));TestTrue(TEXT("path in error"),E.Contains(TEXT("tile[2]")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldDressingSurface,"Anastasis.WorldDressing.RenderedSlopeAndAlignment",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldDressingSurface::RunTest(const FString&)
{
    auto M=Fixture(); FString E; auto R=Rule(); FResult A,B;
    // Semantic altitude stays flat. The actual surface rises 2cm per horizontal cm.
    for (auto& T:M.Triangles)
    {
        T.A.Z=500+2*T.A.X; T.B.Z=500+2*T.B.X; T.C.Z=500+2*T.C.X;
    }
    TestTrue(TEXT("steep rendered geometry"),M.Prepare(E));
    TestTrue(TEXT("steep plan"),Build(M,{R},42,20000,A,E));
    TestEqual(TEXT("max slope excludes actual steep triangles"),A.Placements.Num(),0);
    R.SlopeMax=80;R.bAlignToSurfaceNormal=true;
    TestTrue(TEXT("aligned plan"),Build(M,{R},42,20000,A,E));
    TestTrue(TEXT("aligned placements reachable"),!A.Placements.IsEmpty());
    const FVector ExpectedNormal=FVector(-2,0,1).GetSafeNormal();
    for (const auto& P:A.Placements)
    {
        TestTrue(TEXT("up follows actual face normal"),P.Transform.GetRotation().RotateVector(FVector::UpVector).Equals(ExpectedNormal,1.e-6));
        TestTrue(TEXT("height follows triangle not semantic altitude"),FMath::IsNearlyEqual(P.Ground.Z,500+2*P.Ground.X,1.e-6));
    }
    M.SourceTransform.SetTranslation(FVector(1200,-450,300));
    TestTrue(TEXT("translated source"),M.Prepare(E));
    TestTrue(TEXT("translated plan"),Build(M,{R},42,20000,B,E));
    TestEqual(TEXT("translation keeps count"),A.Placements.Num(),B.Placements.Num());
    TestTrue(TEXT("world placement hash changes with source translation"),A.Hash!=B.Hash);
    for (int32 I=0;I<A.Placements.Num() && I<B.Placements.Num();++I)
        TestTrue(TEXT("world location includes source translation"),(B.Placements[I].Transform.GetLocation()-A.Placements[I].Transform.GetLocation()).Equals(FVector(1200,-450,300)));
    TestTrue(TEXT("manager excluded from runtime cook"),GetDefault<AAnastasisWorldDressingManager>()->IsEditorOnly());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldDressingCurrentMap,"Anastasis.WorldDressing.CurrentSpatialScaleAndRaisedWater",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldDressingCurrentMap::RunTest(const FString&)
{
    FString E; auto M=Fixture(); M.Snapshot.SpatialScale=5;
    for (auto& T:M.Triangles) for (FVector* V:{&T.A,&T.B,&T.C}) { V->X*=5; V->Y*=5; }
    M.bUseRenderedWater=true;
    const double Step=AnastasisWorldView::TileWorldSize*5;
    // A raised river covers otherwise dry grass; fixed SeaLevel would miss this water entirely.
    const double X0=Step*3,X1=Step*5,Y0=0,Y1=Step*8;
    M.WaterTriangles={{{X0,Y0,700},{X1,Y0,700},{X0,Y1,700}},{{X1,Y0,700},{X1,Y1,700},{X0,Y1,700}}};
    TestTrue(TEXT("scale5 and raised river prepare"),M.Prepare(E));
    const auto* T=M.TileAt(Step*6.5,Step*4.5);
    TestTrue(TEXT("tile indices use spatial scale"),T && T->X==6 && T->Y==4);
    TestFalse(TEXT("raised river is wet on grass"),M.IsDry({Step*4,Step*4}));
    TestTrue(TEXT("neighboring bank is dry"),M.IsDry({Step*6,Step*4}));
    TestEqual(TEXT("distance includes rendered river"),M.WaterDistance({Step*5.5,Step*4}),Step*.5);
    FResult A,B; auto R=Rule();
    TestTrue(TEXT("build current map"),Build(M,{R},12345,20000,A,E));
    TestTrue(TEXT("repeat current map"),Build(M,{R},12345,20000,B,E));
    TestEqual(TEXT("current scale repeat hash"),A.Hash,B.Hash);
    TestTrue(TEXT("current map reachable"),A.Placements.Num()>5);
    for (const auto& P:A.Placements)
    {
        TestTrue(TEXT("no river roots"),P.Ground.X<X0 || P.Ground.X>X1);
        TestTrue(TEXT("physical map bounds"),P.Ground.X>=0 && P.Ground.X<8*Step && P.Ground.Y>=0 && P.Ground.Y<8*Step);
    }
    M.Snapshot.SpatialScale=std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("invalid scale rejected"),M.Prepare(E));
    return true;
}
#endif
