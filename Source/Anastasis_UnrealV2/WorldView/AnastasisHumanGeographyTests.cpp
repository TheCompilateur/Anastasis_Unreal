#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WorldView/AnastasisWorldView.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisHumanGeography.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Village/AnastasisVillagePresentation.h"
#include "Sim/AnastasisSimulation.h"

namespace
{
using namespace AnastasisWorldView;
bool BuildHuman(bool Layer, AnastasisTerrainForge::FMesh& M)
{
    auto S=CaptureCanonicalWorld(ReferenceSeed); S.SpatialScale=5;S.bHumanGeography=Layer;
    AnastasisTerrainSurface::FGeometry G;
    return AnastasisTerrainSurface::Build(S,G) && AnastasisTerrainForge::Apply(S,G,M);
}
double TriangleSlope(const FVector& A,const FVector& B,const FVector& C)
{
    const FVector N=FVector::CrossProduct(B-A,C-A);
    return FMath::RadiansToDegrees(FMath::Atan2(FMath::Sqrt(N.X*N.X+N.Y*N.Y),FMath::Abs(N.Z)));
}
double LargestGentlePatch(const AnastasisTerrainForge::FMesh& M,double X0,double X1,double Y0,double Y1,double MaxSlope=10)
{
    const int32 W=M.FineW-1,H=M.FineH-1;
    TArray<uint8> Valid;Valid.SetNumZeroed(W*H);
    const auto& V=M.Geometry.Vertices;const auto& Water=M.Geometry.WaterVertices;
    const double Unit=TileWorldSize*M.SpatialScale;
    for(int32 Y=0;Y<H;++Y) for(int32 X=0;X<W;++X)
    {
        const int32 A=Y*M.FineW+X,B=A+1,C=A+M.FineW,D=C+1;
        const double PX=V[A].X/Unit,PY=V[A].Y/Unit;
        if(PX<X0 || PX>X1 || PY<Y0 || PY>Y1) continue;
        if(V[A].Z<Water[A].Z+50 || V[B].Z<Water[B].Z+50 || V[C].Z<Water[C].Z+50 || V[D].Z<Water[D].Z+50) continue;
        if(FMath::Max(TriangleSlope(V[A],V[C],V[B]),TriangleSlope(V[B],V[C],V[D]))<MaxSlope) Valid[Y*W+X]=1;
    }
    int32 Best=0;TArray<int32> Queue;
    for(int32 I=0;I<Valid.Num();++I) if(Valid[I])
    {
        Queue.Reset();Queue.Add(I);Valid[I]=0;
        for(int32 Q=0;Q<Queue.Num();++Q)
        {
            const int32 X=Queue[Q]%W,Y=Queue[Q]/W;
            const FIntPoint Ns[]={{X-1,Y},{X+1,Y},{X,Y-1},{X,Y+1}};
            for(const auto& N:Ns) if(N.X>=0 && N.X<W && N.Y>=0 && N.Y<H && Valid[N.Y*W+N.X])
            {Valid[N.Y*W+N.X]=0;Queue.Add(N.Y*W+N.X);}
        }
        Best=FMath::Max(Best,Queue.Num());
    }
    const double Step=TileWorldSize*M.SpatialScale/M.Subdiv/100;
    return Best*Step*Step;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHumanGeographyFieldTruth,
    "Anastasis.Terrain.HumanGeography.FieldTruth",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHumanGeographyFieldTruth::RunTest(const FString&)
{
    using namespace AnastasisWorldView;
    FWorldVisualSnapshot S;
    S.SourceW=4; S.SourceH=4; S.W=4; S.H=4;
    S.Tiles.SetNum(16);
    for(int32 Y=0;Y<4;++Y) for(int32 X=0;X<4;++X)
    {
        FVisualTile& T=S.Tiles[Y*4+X];
        T.X=X; T.Y=Y; T.SourceIndex=Y*4+X;
        T.Type=AnastasisWorld::ETileType::Grass;
    }
    const FLinearColor Alluvium=AnastasisHumanGeography::ValleyLandUseToneAt(S,0.5,0.5);
    TestTrue(TEXT("no fictitious parcels across non-field alluvium"),
        Alluvium.Equals(AnastasisHumanGeography::ValleyLandUseToneAt(S,3.5,3.5)));
    FVisualTile& Field=S.Tiles[1*4+2];
    Field.Type=AnastasisWorld::ETileType::Field;
    Field.Resource=AnastasisWorld::EResource::Food;
    Field.CropId=AnastasisWorld::ECropId::Grain;
    Field.Amount=30;
    const FLinearColor Grain=AnastasisHumanGeography::ValleyLandUseToneAt(S,2.5,1.5);
    TestFalse(TEXT("only a real field gets a worked-land tone"),Grain.Equals(Alluvium));
    TestTrue(TEXT("neighboring non-field remains alluvium"),
        AnastasisHumanGeography::ValleyLandUseToneAt(S,1.5,1.5).Equals(Alluvium));
    Field.Amount=1;
    TestTrue(TEXT("static worldgen snapshot does not pretend to show harvest"),
        Grain.Equals(AnastasisHumanGeography::ValleyLandUseToneAt(S,2.5,1.5)));
    Field.CropId=AnastasisWorld::ECropId::Fallow;
    TestFalse(TEXT("fallow is visually distinct from initial grain"),
        Grain.Equals(AnastasisHumanGeography::ValleyLandUseToneAt(S,2.5,1.5)));
    const FWorldVisualSnapshot Crop=CropSnapshot(S,2,1,2,2);
    TestTrue(TEXT("crop origin preserves source-tile coordinate authority"),
        AnastasisHumanGeography::ValleyLandUseToneAt(S,2.5,1.5).Equals(
            AnastasisHumanGeography::ValleyLandUseToneAt(Crop,2.5,1.5)));
    Field.Type=AnastasisWorld::ETileType::Grass;
    TestTrue(TEXT("without Field type, crop metadata cannot invent a parcel"),
        AnastasisHumanGeography::ValleyLandUseToneAt(S,2.5,1.5).Equals(Alluvium));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHumanGeographyScale,"Anastasis.Terrain.HumanGeography.ScaleAndSampler",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHumanGeographyScale::RunTest(const FString&)
{
    auto S=CaptureCanonicalWorld(ReferenceSeed);
    const auto Original=S;
    S.SpatialScale=5;S.bHumanGeography=true;
    const auto Crop=CropSnapshot(S,24,32,20,18);
    TestEqual(TEXT("crop carries scale"),Crop.SpatialScale,5.0);
    const auto Plan=BuildPlan(S);
    TestEqual(TEXT("1900m between outer sample centres"),(Plan.Locations.Last().X-Plan.Locations[0].X)/100,1900.0);
    TestEqual(TEXT("sea datum unchanged"),AltitudeToUnreal(AnastasisWorld::SeaLevel,5),275.0);
    AnastasisTerrainSurface::FGeometry G;
    AnastasisTerrainForge::FMesh M;
    if(!TestTrue(TEXT("scaled surface builds"),AnastasisTerrainSurface::Build(S,G)))return false;
    if(!TestTrue(TEXT("human layer builds"),AnastasisTerrainForge::Apply(S,G,M)))return false;
    TestEqual(TEXT("one context UV per fine vertex"),M.Geometry.UV2.Num(),M.Geometry.Vertices.Num());
    int32 StrongRoad=0, OffRoad=0;
    if (M.Geometry.UV2.Num()==M.Geometry.Vertices.Num())
    {
        const double Unit=TileWorldSize*S.SpatialScale;
        for(int32 I=0;I<M.Geometry.Vertices.Num();++I)
        {
            const double X=M.Geometry.Vertices[I].X/Unit,Y=M.Geometry.Vertices[I].Y/Unit;
            if(X>41.5 && X<42.5 && Y>32.5 && Y<33.5 && M.Geometry.UV2[I].X>0.9) ++StrongRoad;
            if(X<15 && Y<15 && M.Geometry.UV2[I].X>0.001) ++OffRoad;
        }
    }
    TestTrue(TEXT("authored pass exports a strong road photo weight"),StrongRoad>0);
    TestEqual(TEXT("road photo absent outside authored pass"),OffRoad,0);
    double Error=0;
    for(int32 I=0;I<G.Triangles.Num();I+=3*317)
    {
        const FVector P=(G.Vertices[G.Triangles[I]]+G.Vertices[G.Triangles[I+1]]+G.Vertices[G.Triangles[I+2]])/3;
        double Z=0;
        if(!TestTrue(TEXT("triangle-centroid query succeeds"),AnastasisTerrainForge::SampleHeight(M,P.X,P.Y,Z)))return false;
        Error=FMath::Max(Error,FMath::Abs(Z-P.Z));
    }
    TestTrue(TEXT("sample follows actual triangulated ground within 0.001cm"),Error<0.001);
    double Z=0;TestFalse(TEXT("outside field is rejected"),AnastasisTerrainForge::SampleHeight(M,1,1,Z));
    for(int32 I=0;I<S.Tiles.Num();++I)
    {
        if(S.Tiles[I].Alt!=Original.Tiles[I].Alt || S.Tiles[I].Type!=Original.Tiles[I].Type || S.Tiles[I].Amount!=Original.Tiles[I].Amount)
        {AddError(TEXT("semantic source changed"));break;}
    }
    AddInfo(FString::Printf(TEXT("HUMAN_SCALE extent_m=1900 sampler_error_cm=%.9f source_tiles=%d"),Error,S.Tiles.Num()));
    AnastasisTerrainForge::ClearActive();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHumanGeographyBasins,"Anastasis.Terrain.HumanGeography.BasinsAndConservation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHumanGeographyBasins::RunTest(const FString&)
{
    AnastasisTerrainForge::FMesh Before,After;
    if(!TestTrue(TEXT("original and V2 build at same scale"),BuildHuman(false,Before)&&BuildHuman(true,After)))return false;
    const double A0=LargestGentlePatch(Before,31,75,39,70),A1=LargestGentlePatch(After,31,75,39,70);
    const double B0=LargestGentlePatch(Before,23,44,15,34),B1=LargestGentlePatch(After,23,44,15,34);
    TestTrue(TEXT("A contains a contiguous >=6ha patch below 10 degrees"),A1>=60000);
    TestTrue(TEXT("B contains a contiguous >=1.8ha patch below 10 degrees"),B1>=18000);
    // The corrected relief already has connected <10-degree land. The old x3
    // requirement was tied to the discarded spiky source, not to the human brief.
    // Keep the absolute floor, forbid loss of that existing land, and require
    // large continuous <5-degree agricultural areas with an actual A/B gain.
    const double A5Before=LargestGentlePatch(Before,31,75,39,70,5);
    const double A5=LargestGentlePatch(After,31,75,39,70,5);
    const double B5Before=LargestGentlePatch(Before,23,44,15,34,5);
    const double B5=LargestGentlePatch(After,23,44,15,34,5);
    TestTrue(TEXT("retain or enlarge the corrected source's connected gentle land"),A1>=A0 && B1>=B0);
    TestTrue(TEXT("A has >=12ha and B >=4ha of continuous land below 5 degrees"),A5>=120000 && B5>=40000);
    TestTrue(TEXT("both agricultural patches enlarge the corrected baseline"),A5>A5Before && B5>B5Before);
    AddInfo(FString::Printf(TEXT("HUMAN_AGRICULTURE A5_m2=%.0f->%.0f B5_m2=%.0f->%.0f"),A5Before,A5,B5Before,B5));
    int32 Changed=0,Protected=0;double ProtectedError=0,XYError=0;
    for(int32 I=0;I<After.Geometry.Vertices.Num();++I)
    {
        const auto& A=After.Geometry.Vertices[I];const auto& B=Before.Geometry.Vertices[I];
        XYError=FMath::Max(XYError,FVector2D(A.X-B.X,A.Y-B.Y).Size());
        if(FMath::Abs(A.Z-B.Z)>0.001)++Changed;
        const double X=A.X/2000,Y=A.Y/2000;
        if((X>=30 && X<=70 && Y>=82 && Y<=94) || (X>=65 && X<=70 && Y>=11 && Y<=14))
        {++Protected;ProtectedError=FMath::Max(ProtectedError,FMath::Abs(A.Z-B.Z));}
    }
    TestTrue(TEXT("major protected massifs are identical"),Protected>100 && ProtectedError<0.001);
    TestEqual(TEXT("all XY sample locations preserved"),XYError,0.0);
    TestTrue(TEXT("at least half original field remains untouched"),Changed<After.Geometry.Vertices.Num()/2);
    AddInfo(FString::Printf(TEXT("HUMAN_BASINS A_m2=%.0f->%.0f B_m2=%.0f->%.0f changed_vertices=%d/%d protected_error_cm=%.6f"),A0,A1,B0,B1,Changed,After.Geometry.Vertices.Num(),ProtectedError));
    AnastasisTerrainForge::ClearActive();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHumanGeographyWater,"Anastasis.Terrain.HumanGeography.RiverAndOutlet",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHumanGeographyWater::RunTest(const FString&)
{
    AnastasisTerrainForge::FMesh M;
    if(!TestTrue(TEXT("V2 builds"),BuildHuman(true,M)))return false;
    const FVector2D Pts[]={{40,59},{46,57},{52,58},{58,61},{63,65},{69,70},{74,70},{80,69},{86,72},{91,75},{95.5,76}};
    double Previous=1.e20,MinDepth=1.e20;int32 Count=0;
    for(const auto& P:Pts)
    {
        double Z=0,Water=0;
        const double X=P.X*2000,Y=P.Y*2000;
        const bool Hit=AnastasisTerrainForge::SampleHeight(M,X,Y,Z)&&AnastasisTerrainForge::SampleActiveWater(X,Y,Water);
        TestTrue(TEXT("river and outlet covered by field"),Hit);
        if(!Hit)continue;
        TestTrue(TEXT("water profile never flows uphill"),Water<=Previous+0.001);
        TestTrue(TEXT("river bed under water at every checkpoint"),Z<Water-20);
        Previous=Water;MinDepth=FMath::Min(MinDepth,Water-Z);++Count;
    }
    TestTrue(TEXT("outlet reaches edge below lake datum"),Previous<275);
    AddInfo(FString::Printf(TEXT("HUMAN_HYDROLOGY checkpoints=%d min_depth_cm=%.3f outlet_water_cm=%.3f"),Count,MinDepth,Previous));
    AnastasisTerrainForge::ClearActive();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHumanGeographyCollision,"Anastasis.Terrain.HumanGeography.CollisionAndDressing",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHumanGeographyCollision::RunTest(const FString&)
{
    UWorld* World=nullptr;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::Editor){World=C.World();break;}
    if(!World){AddError(TEXT("no editor world"));return false;}
    FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
    auto* Actor=World->SpawnActor<AAnastasisWorldEmbodiment>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
    if(!Actor){AddError(TEXT("spawn failed"));return false;}
    Actor->Embody(12345,96,96);
    FAnastasisSimulation Sim; Sim.Reset(12345,96,96);
    TestEqual(TEXT("default scene uses approved physical scale"),Actor->GetSnapshot().SpatialScale,5.0);
    TArray<UProceduralMeshComponent*> Meshes;Actor->GetComponents(Meshes);
    if(Meshes.IsEmpty()){Actor->Destroy();AddError(TEXT("no procedural collision surface"));return false;}
    int32 Hits=0;double Error=0;
    for(const FVector2D P: {FVector2D(53,53),FVector2D(61,47),FVector2D(33,24),FVector2D(42,33),FVector2D(60,86)})
    {
        double Z=0;if(!AnastasisTerrainForge::SampleActive(P.X*2000,P.Y*2000,Z))continue;
        const FVector VillagePosition = FAnastasisVillagePresentation::SimToUnreal(Sim.GetWorld(), P.X, P.Y, World);
        TestTrue(TEXT("village adapter uses physical tile coordinates and actual terrain height"),
            VillagePosition.Equals(FVector(P.X*2000,P.Y*2000,Z),1.0));
        FHitResult Hit;const FVector Top(P.X*2000,P.Y*2000,Z+5000),Bottom(P.X*2000,P.Y*2000,Z-5000);
        if(Meshes[0]->LineTraceComponent(Hit,Top,Bottom,FCollisionQueryParams(SCENE_QUERY_STAT(HumanGeographyTest),true)))
        {++Hits;Error=FMath::Max(Error,FMath::Abs(Hit.ImpactPoint.Z-Z));}
    }
    TestEqual(TEXT("collision hits at all five physical sites"),Hits,5);
    TestTrue(TEXT("collision follows rendered terrain within 1cm"),Error<1.0);
    TArray<UHierarchicalInstancedStaticMeshComponent*> HISMs;Actor->GetComponents(HISMs);
    int32 HiddenColliders=0,Instances=0;double MaxOtherScale=0,MaxTreeHeightUU=0;
    for(auto* H:HISMs)
    {
        if(H->GetName().StartsWith(TEXT("Tiles_"))){if(H->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)++HiddenColliders;continue;}
        // Lieux composes (AnastasisPlaces) : leur echelle est une taille en metres sur des meshes
        // normalises a 1 m (un chene de 21 m = echelle 21), constante, sans lien avec SpatialScale.
        if(H->GetName().StartsWith(TEXT("Place_"))){Instances+=H->GetInstanceCount();continue;}
        Instances+=H->GetInstanceCount();
        for(int32 I=0;I<H->GetInstanceCount();++I)
        {
            FTransform T;H->GetInstanceTransform(I,T,true);
            if(H->GetName().StartsWith(TEXT("Dressing_Tree_")) && H->GetStaticMesh())
                MaxTreeHeightUU=FMath::Max(MaxTreeHeightUU,H->GetStaticMesh()->GetBoundingBox().GetSize().Z*T.GetScale3D().Z);
            else MaxOtherScale=FMath::Max(MaxOtherScale,T.GetScale3D().GetMax());
        }
    }
    TestEqual(TEXT("hidden debug slabs cannot collide with the new terrain"),HiddenColliders,0);
    TestTrue(TEXT("dressing remains present"),Instances>0);
    // Macro trees intentionally exceed the old <10 mesh-scale proxy. Guard physical
    // stature separately: multiplying them by world scale 5 would break this 30m ceiling.
    TestTrue(TEXT("non-tree objects were not enlarged with the terrain"),MaxOtherScale<10.0);
    TestTrue(TEXT("tree stature remains under 30m independently of world scale"),MaxTreeHeightUU>0 && MaxTreeHeightUU<=3000.0);
    AddInfo(FString::Printf(TEXT("HUMAN_COLLISION hits=%d max_error_cm=%.6f dressing=%d max_other_scale=%.3f max_tree_height_cm=%.1f"),Hits,Error,Instances,MaxOtherScale,MaxTreeHeightUU));
    Actor->Destroy();AnastasisTerrainForge::ClearActive();return true;
}
#endif
