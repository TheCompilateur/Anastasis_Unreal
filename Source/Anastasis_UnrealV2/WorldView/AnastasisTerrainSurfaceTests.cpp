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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainSemantics, "Anastasis.Terrain.Semantics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainSemantics::RunTest(const FString&)
{
    const auto Full = AnastasisWorldView::CaptureCanonicalWorld(12345);
    const auto Crop = AnastasisWorldView::CropSnapshot(Full, 0, 0, 32, 32);
    AnastasisTerrainSurface::FGeometry G, Again;
    if (!TestTrue(TEXT("build"), AnastasisTerrainSurface::Build(Crop, G))) return false;

    TestEqual(TEXT("une couleur par sommet"), G.Colors.Num(), 1024);
    TestEqual(TEXT("une nappe d'eau par sommet"), G.WaterVertices.Num(), 1024);

    int32 WaterTiles = 0, LandTiles = 0;
    for (int32 I = 0; I < 1024; ++I)
    {
        const auto& T = Crop.Tiles[I];
        const FLinearColor& C = G.Colors[I];
        TestTrue(TEXT("couleur finie et bornee"),
            FMath::IsFinite(C.R) && FMath::IsFinite(C.G) && FMath::IsFinite(C.B)
            && C.R >= 0.f && C.R <= 1.f && C.G >= 0.f && C.G <= 1.f && C.B >= 0.f && C.B <= 1.f);
        TestEqual(TEXT("nappe d'eau plate au niveau de la mer"), G.WaterVertices[I].Z, AnastasisTerrainSurface::WaterPlaneZ);
        TestEqual(TEXT("nappe alignee sur le relief en XY"), FVector2D(G.WaterVertices[I]), FVector2D(G.Vertices[I]));
        if (T.Type == AnastasisWorld::ETileType::Water)
        {
            ++WaterTiles;
            TestEqual(TEXT("alpha eau = 1"), C.A, 1.0f);
            TestTrue(TEXT("l'eau est bleue dominante"), C.B > C.R && C.B > C.G);
        }
        else
        {
            ++LandTiles;
            TestEqual(TEXT("alpha terre = 0"), C.A, 0.0f);
            TestTrue(TEXT("la terre n'est pas bleue dominante"), C.B <= C.R || C.B <= C.G);
        }
    }
    TestTrue(TEXT("la tranche contient de l'eau"), WaterTiles > 0);
    TestTrue(TEXT("la tranche contient de la terre"), LandTiles > 0);

    // Provenance de la nappe : une cellule ne porte de l'eau que si une tuile source en porte.
    TestTrue(TEXT("nappe non vide"), G.WaterTriangles.Num() > 0);
    TestEqual(TEXT("triangles d'eau complets"), G.WaterTriangles.Num() % 3, 0);
    for (int32 I = 0; I < G.WaterTriangles.Num(); I += 6)
    {
        const int32 A = G.WaterTriangles[I];
        const int32 B = A + 1, C = A + 32, D = C + 1;
        const bool bAnyWater =
            Crop.Tiles[A].Type == AnastasisWorld::ETileType::Water ||
            Crop.Tiles[B].Type == AnastasisWorld::ETileType::Water ||
            Crop.Tiles[C].Type == AnastasisWorld::ETileType::Water ||
            Crop.Tiles[D].Type == AnastasisWorld::ETileType::Water;
        TestTrue(TEXT("cellule d'eau adossee a une tuile d'eau source"), bAnyWater);
    }

    // Rive : au moins une tuile de terre porte un Shore franc, sinon la bande cotiere ne veut rien dire.
    int32 ShoreTiles = 0;
    for (const auto& T : Crop.Tiles)
    {
        if (T.Type != AnastasisWorld::ETileType::Water && T.Shore > 0.5) ++ShoreTiles;
    }
    TestTrue(TEXT("bande cotiere presente dans la tranche"), ShoreTiles > 0);

    const auto Second = AnastasisWorldView::CropSnapshot(AnastasisWorldView::CaptureCanonicalWorld(12345), 0, 0, 32, 32);
    TestTrue(TEXT("reconstruction"), AnastasisTerrainSurface::Build(Second, Again));
    TestTrue(TEXT("classification semantique deterministe"),
        G.Colors == Again.Colors && G.WaterVertices == Again.WaterVertices && G.WaterTriangles == Again.WaterTriangles);

    AddInfo(FString::Printf(TEXT("TERRAIN_SEMANTICS water_tiles=%d land_tiles=%d shore_tiles=%d water_quads=%d"),
        WaterTiles, LandTiles, ShoreTiles, G.WaterTriangles.Num() / 6));
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
    if (Surface && Surface->GetProcMeshSection(0))
    {
        // Mode 1 reste la tranche scellee meme quand l'emprise incarnee est le monde entier.
        TestEqual(TEXT("mode 1 = 1024 sommets (tranche scellee)"), Surface->GetProcMeshSection(0)->ProcVertexBuffer.Num(), 1024);
        // Mode 2 : la meme surface, batie sur les 96x96 reellement incarnes.
        Var->Set(2,ECVF_SetByCode); Actor->Embody(12345,96,96);
        TestTrue(TEXT("surface monde visible"), Surface->IsVisible() && Surface->GetProcMeshSection(0) != nullptr);
        if (Surface->GetProcMeshSection(0))
        {
            TestEqual(TEXT("mode 2 = 9216 sommets"), Surface->GetProcMeshSection(0)->ProcVertexBuffer.Num(), 9216);
            TestEqual(TEXT("mode 2 = 18050 triangles"), Surface->GetProcMeshSection(0)->ProcIndexBuffer.Num()/3, 18050);
        }
        TArray<UHierarchicalInstancedStaticMeshComponent*> HiddenCheck; Actor->GetComponents(HiddenCheck);
        for (auto* Mesh : HiddenCheck) { if (Mesh->GetName().StartsWith(TEXT("Tiles_"))) TestFalse(TEXT("dalles DEBUG masquees en mode 2"), Mesh->IsVisible()); }
    }
    Var->Set(0,ECVF_SetByCode); Actor->Embody(12345,96,96);
    TestTrue(TEXT("surface disabled"),Surface && !Surface->IsVisible());
    // GetComponents(UHierarchicalInstancedStaticMeshComponent) also finds the
    // presentation-resolver dressing meshes (Tree/Ruin) added alongside the 7 legacy
    // per-ETileType meshes: same component class, orthogonal purpose. Filter by the
    // legacy naming convention ("Tiles_<TypeName>", see TerrainComponentName in
    // AnastasisWorldEmbodiment.cpp) so this test keeps checking exactly what it says.
    TArray<UHierarchicalInstancedStaticMeshComponent*> AllHism; Actor->GetComponents(AllHism);
    TArray<UHierarchicalInstancedStaticMeshComponent*> Legacy;
    for (auto* Mesh : AllHism) { if (Mesh->GetName().StartsWith(TEXT("Tiles_"))) Legacy.Add(Mesh); }
    TestEqual(TEXT("7 legacy classes"),Legacy.Num(),7);
    for(auto* Mesh:Legacy) TestTrue(TEXT("legacy restored"),Mesh->IsVisible());
    TestEqual(TEXT("9216 legacy instances"),Actor->GetInstanceCount(),9216);
    Actor->Destroy(); Var->Set(Previous,ECVF_SetByCode);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainWorldExtent, "Anastasis.Terrain.WorldExtent", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainWorldExtent::RunTest(const FString&)
{
    const auto Full = AnastasisWorldView::CaptureCanonicalWorld(12345);

    // 1. NON-REGRESSION. La tranche scellee doit sortir de Build exactement comme avant
    //    la generalisation : c'est ce qui rend WORLD_SLICE_006 encore prouvable.
    const auto Canonical = AnastasisWorldView::CropSnapshot(Full, 0, 0, 32, 32);
    AnastasisTerrainSurface::FGeometry Sealed;
    if (!TestTrue(TEXT("la tranche canonique se batit toujours"), AnastasisTerrainSurface::Build(Canonical, Sealed))) return false;
    TestEqual(TEXT("TERRAIN_CONTRACT vertices=1024 inchange"), Sealed.Vertices.Num(), 1024);
    TestEqual(TEXT("TERRAIN_CONTRACT triangles=1922 inchange"), Sealed.Triangles.Num() / 3, 1922);

    // 2. Le monde entier, d'un seul tenant.
    AnastasisTerrainSurface::FGeometry World;
    if (!TestTrue(TEXT("le monde 96x96 se batit"), AnastasisTerrainSurface::Build(Full, World))) return false;
    TestEqual(TEXT("9216 sommets"), World.Vertices.Num(), AnastasisTerrainSurface::VerticesFor(96, 96));
    TestEqual(TEXT("18050 triangles"), World.Triangles.Num() / 3, AnastasisTerrainSurface::TrianglesFor(96, 96));
    TestEqual(TEXT("une couleur par sommet"), World.Colors.Num(), 9216);
    TestEqual(TEXT("une nappe d'eau par sommet"), World.WaterVertices.Num(), 9216);

    // La tranche scellee est le coin (0,0) du monde : memes sommets, memes index source.
    // La couleur, elle, est volontairement relative a l'emprise (MinAlt/MaxAlt du crop),
    // donc elle n'est PAS comparee ici -- voir AddInfo.
    for (int32 I = 0; I < 1024; ++I)
    {
        const int32 WorldIndex = (I / 32) * 96 + I % 32;
        TestEqual(TEXT("sommet partage avec la tranche scellee"), World.Vertices[WorldIndex], Sealed.Vertices[I]);
        TestEqual(TEXT("index source partage"), World.SourceIndices[WorldIndex], Sealed.SourceIndices[I]);
    }

    // 3. Pas une seule fissure interne sur toute l'emprise : chaque arete interieure
    //    est partagee par exactement deux triangles, les aretes uniques sont le perimetre.
    TMap<uint64, int32> Edges;
    for (int32 I = 0; I < World.Triangles.Num(); I += 3)
    {
        const int32 A = World.Triangles[I], B = World.Triangles[I+1], C = World.Triangles[I+2];
        TestTrue(TEXT("face avant non degeneree"), FVector::CrossProduct(World.Vertices[C]-World.Vertices[A], World.Vertices[B]-World.Vertices[A]).Z > 0);
        const int32 V[3] = {A, B, C};
        for (int32 J = 0; J < 3; ++J) { const uint32 Lo = FMath::Min(V[J], V[(J+1)%3]), Hi = FMath::Max(V[J], V[(J+1)%3]); ++Edges.FindOrAdd((uint64(Lo)<<32)|Hi); }
    }
    int32 Boundary = 0;
    for (const auto& E : Edges) { TestTrue(TEXT("maillage manifold"), E.Value == 1 || E.Value == 2); if (E.Value == 1) ++Boundary; }
    TestEqual(TEXT("380 aretes de perimetre, aucune fissure interne"), Boundary, 2*(96-1) + 2*(96-1));

    // 4. Une emprise quelconque, ni canonique ni a l'origine : la generalisation n'est
    //    pas le cas canonique deguise. Les sommets doivent coincider avec ceux du monde.
    const auto Offset = AnastasisWorldView::CropSnapshot(Full, 33, 7, 16, 24);
    AnastasisTerrainSurface::FGeometry Off;
    if (!TestTrue(TEXT("emprise decalee 16x24 batie"), AnastasisTerrainSurface::Build(Offset, Off))) return false;
    TestEqual(TEXT("384 sommets"), Off.Vertices.Num(), AnastasisTerrainSurface::VerticesFor(16, 24));
    TestEqual(TEXT("690 triangles"), Off.Triangles.Num() / 3, AnastasisTerrainSurface::TrianglesFor(16, 24));
    for (int32 I = 0; I < Off.Vertices.Num(); ++I)
    {
        const int32 X = 33 + I % 16, Y = 7 + I / 16;
        TestEqual(TEXT("index source absolu"), Off.SourceIndices[I], Y * 96 + X);
        TestEqual(TEXT("sommet a la place monde de sa tuile"), Off.Vertices[I], AnastasisWorldView::TileToUnreal(X, Y, Full.Tiles[Y*96+X].Alt));
        TestEqual(TEXT("meme sommet que dans le monde entier"), Off.Vertices[I], World.Vertices[Y*96+X]);
    }

    // 5. Determinisme a l'echelle du monde.
    AnastasisTerrainSurface::FGeometry Again;
    TestTrue(TEXT("reconstruction"), AnastasisTerrainSurface::Build(AnastasisWorldView::CaptureCanonicalWorld(12345), Again));
    TestTrue(TEXT("geometrie et semantique deterministes"),
        World.Vertices == Again.Vertices && World.Triangles == Again.Triangles
        && World.Normals == Again.Normals && World.Colors == Again.Colors
        && World.WaterVertices == Again.WaterVertices && World.WaterTriangles == Again.WaterTriangles);

    // 6. Refus. L'emprise reste bornee par le monde, et une bande d'une tuile de large
    //    ne porte aucune cellule : ce n'est pas une surface.
    AnastasisTerrainSurface::FGeometry Rejected;
    auto Overflow = AnastasisWorldView::CropSnapshot(Full, 80, 0, 16, 16);
    Overflow.OriginX = 90;
    TestFalse(TEXT("refus d'une emprise qui deborde du monde"), AnastasisTerrainSurface::Build(Overflow, Rejected));
    TestFalse(TEXT("refus d'une emprise large d'une tuile"), AnastasisTerrainSurface::Build(AnastasisWorldView::CropSnapshot(Full, 0, 0, 1, 32), Rejected));
    TestEqual(TEXT("aucune geometrie partielle"), Rejected.Vertices.Num(), 0);

    // Un monde qui n'est pas le 96x96 canonique reste refuse : la surface est adossee
    // a la verite de simulation, pas a une taille arbitraire.
    auto AlienWorld = Full; AlienWorld.SourceW = 64;
    TestFalse(TEXT("refus d'un monde non canonique"), AnastasisTerrainSurface::Build(AlienWorld, Rejected));

    AddInfo(FString::Printf(TEXT("TERRAIN_EXTENT world=96x96 vertices=%d triangles=%d boundary_edges=%d sealed_crop=32x32/1024/1922 palette=crop_relative(MinAlt=%.6f MaxAlt=%.6f)"),
        World.Vertices.Num(), World.Triangles.Num()/3, Boundary, Full.MinAlt, Full.MaxAlt));
    return true;
}
#endif

