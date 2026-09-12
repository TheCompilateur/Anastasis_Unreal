#include <limits>
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainContract, "Anastasis.Terrain.Contract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainContract::RunTest(const FString&)
{
    const auto Full = AnastasisWorldView::CaptureCanonicalWorld(12345);
    const auto Before = Full;
    const auto Crop = AnastasisWorldView::CropSnapshot(Full, 0, 0, 32, 32);
    AnastasisTerrainSurface::FGeometry G, Again;
    if (!TestTrue(TEXT("valid crop"), AnastasisTerrainSurface::Build(Crop, G))) return false;
    TestEqual(TEXT("1024 source tiles"), Crop.Tiles.Num(), 1024);
    TestEqual(TEXT("1024 shared vertices"), G.Vertices.Num(), 1024);
    TestEqual(TEXT("1922 triangles"), G.Triangles.Num(), 5766);
    double MaxError = 0;
    for (int32 I=0; I<1024; ++I)
    {
        const int32 Source = (I/32)*96+I%32;
        TestEqual(TEXT("canonical index"), G.SourceIndices[I], Source);
        const FVector Expected = AnastasisWorldView::TileToUnreal(I%32, I/32, Full.Tiles[Source].Alt);
        MaxError = FMath::Max(MaxError, FVector::Distance(G.Vertices[I], Expected));
        TestTrue(TEXT("finite normal facing up"), !G.Normals[I].ContainsNaN() && G.Normals[I].Z > 0);
    }
    TestTrue(TEXT("altitude and coordinate error <= .01 UU"), MaxError <= .01);
    TestTrue(TEXT("nonflat source crop"), Crop.MaxAlt > Crop.MinAlt);
    TMap<uint64,int32> Edges;
    for(int32 I=0; I<G.Triangles.Num(); I+=3)
    {
        const int32 A=G.Triangles[I], B=G.Triangles[I+1], C=G.Triangles[I+2];
        TestTrue(TEXT("nondegenerate upward front face"), FVector::CrossProduct(G.Vertices[C]-G.Vertices[A],G.Vertices[B]-G.Vertices[A]).Z > 0);
        const int32 V[3]={A,B,C};
        for(int32 J=0;J<3;++J){const uint32 Lo=FMath::Min(V[J],V[(J+1)%3]), Hi=FMath::Max(V[J],V[(J+1)%3]); ++Edges.FindOrAdd((uint64(Lo)<<32)|Hi);}
    }
    int32 Boundary=0;
    for(const auto& E:Edges)
    {
        TestTrue(TEXT("manifold edges"), E.Value==1 || E.Value==2);
        if(E.Value==1)
        {
            ++Boundary; const int32 A=int32(E.Key>>32), B=int32(E.Key & 0xffffffff);
            TestTrue(TEXT("single-use edge only at external boundary"), (A/32==0 && B/32==0)||(A/32==31 && B/32==31)||(A%32==0 && B%32==0)||(A%32==31 && B%32==31));
        }
    }
    TestEqual(TEXT("124 perimeter edges, no internal cracks"), Boundary, 124);
    const auto Second = AnastasisWorldView::CropSnapshot(AnastasisWorldView::CaptureCanonicalWorld(12345),0,0,32,32);
    TestTrue(TEXT("repeat build"), AnastasisTerrainSurface::Build(Second,Again));
    TestTrue(TEXT("deterministic geometry"), G.Vertices==Again.Vertices && G.Triangles==Again.Triangles && G.Normals==Again.Normals);
    for(int32 I=0;I<Full.Tiles.Num();++I)
    {
        const auto& A=Full.Tiles[I]; const auto& B=Before.Tiles[I];
        TestTrue(TEXT("source unchanged"), A.SourceIndex==B.SourceIndex && A.X==B.X && A.Y==B.Y && A.Type==B.Type && A.Resource==B.Resource && A.Amount==B.Amount && A.Alt==B.Alt && A.Shade==B.Shade && A.Shore==B.Shore && A.Wetness==B.Wetness && A.FlowX==B.FlowX && A.FlowZ==B.FlowZ && A.FlowAmt==B.FlowAmt && A.CropId==B.CropId && A.Fertility==B.Fertility && A.ForestMargin==B.ForestMargin && A.bHasForestMargin==B.bHasForestMargin);
    }
    auto Invalid=Crop; Invalid.Tiles[0].Alt=std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("reject nonfinite"), AnastasisTerrainSurface::Build(Invalid,Again));
    TestEqual(TEXT("no partial geometry"), Again.Vertices.Num(),0);
    Invalid=Crop; Invalid.Tiles[0].SourceIndex=999;
    TestFalse(TEXT("reject wrong index"), AnastasisTerrainSurface::Build(Invalid,Again));
    AddInfo(FString::Printf(TEXT("TERRAIN_CONTRACT source=96x96 crop=0,0,32,32 vertices=1024 triangles=1922 max_error=%.9f boundary_edges=%d"),MaxError,Boundary));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainFallback, "Anastasis.Terrain.Fallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainFallback::RunTest(const FString&)
{
    UWorld* World=nullptr;
    for(const auto& Context:GEngine->GetWorldContexts()) if(Context.WorldType==EWorldType::Editor){World=Context.World();break;}
    if(!World){AddError(TEXT("No Editor world"));return false;}
    auto* Var=IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Terrain.Surface"));
    const int32 Previous=Var->GetInt();
    FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
    auto* Actor=World->SpawnActor<AAnastasisWorldEmbodiment>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if(!Actor){AddError(TEXT("Spawn failed"));return false;}
    Var->Set(1,ECVF_SetByCode); Actor->Embody(12345,96,96);
    auto* Surface=Actor->FindComponentByClass<UProceduralMeshComponent>();
    TestTrue(TEXT("surface visible"),Surface && Surface->IsVisible() && Surface->GetProcMeshSection(0));
    Var->Set(0,ECVF_SetByCode); Actor->Embody(12345,96,96);
    TestTrue(TEXT("surface disabled"),Surface && !Surface->IsVisible());
    TArray<UHierarchicalInstancedStaticMeshComponent*> Legacy; Actor->GetComponents(Legacy);
    TestEqual(TEXT("7 legacy classes"),Legacy.Num(),7);
    for(auto* Mesh:Legacy) TestTrue(TEXT("legacy restored"),Mesh->IsVisible());
    TestEqual(TEXT("9216 legacy instances"),Actor->GetInstanceCount(),9216);
    Actor->Destroy(); Var->Set(Previous,ECVF_SetByCode);
    return true;
}
#endif

