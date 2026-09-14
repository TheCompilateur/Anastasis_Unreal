#include <limits>
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisPresentationResolver.h"
#include "WorldView/AnastasisWorldDebugVisual.h"
#include "World/AnastasisHydrology.h"
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
        G.Colors == Again.Colors && G.WaterVertices == Again.WaterVertices && G.WaterTriangles == Again.WaterTriangles
        && G.UV0 == Again.UV0 && G.UV1 == Again.UV1);

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
    auto* ForgeVar=IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Terrain.Forge"));
    const int32 PreviousForge = ForgeVar ? ForgeVar->GetInt() : 0;
    if (ForgeVar) ForgeVar->Set(0, ECVF_SetByCode);
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
    if (ForgeVar) ForgeVar->Set(PreviousForge, ECVF_SetByCode);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainSampleHeight, "Anastasis.Terrain.SampleHeight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainSampleHeight::RunTest(const FString&)
{
    const auto Full = AnastasisWorldView::CaptureCanonicalWorld(12345);
    const auto Crop = AnastasisWorldView::CropSnapshot(Full, 0, 0, 32, 32);
    AnastasisTerrainSurface::FGeometry G;
    if (!TestTrue(TEXT("build"), AnastasisTerrainSurface::Build(Crop, G))) return false;

    const double Tolerance = 1.e-6;
    double Z = 0.0;

    // 1. Sur un sommet, l'echantillon EST le sommet. Si ca ne tient pas, tout ce qu'on
    //    pose sur la carte est decale d'entree.
    double MaxVertexError = 0.0;
    for (int32 I = 0; I < G.Vertices.Num(); ++I)
    {
        if (!TestTrue(TEXT("echantillon valide sur un sommet"), AnastasisTerrainSurface::SampleHeight(Crop, G.Vertices[I].X, G.Vertices[I].Y, Z))) return false;
        MaxVertexError = FMath::Max(MaxVertexError, FMath::Abs(Z - G.Vertices[I].Z));
    }
    TestTrue(TEXT("erreur nulle sur les 1024 sommets"), MaxVertexError <= Tolerance);

    // 2. LE test qui compte : au centre de gravite d'un triangle, un plan vaut la moyenne
    //    de ses trois sommets. Si l'echantillon suit vraiment la face rendue, il rend
    //    exactement cette moyenne -- une bilineaire, elle, s'en ecarterait des que les
    //    quatre coins de la cellule ne sont pas coplanaires.
    double MaxFaceError = 0.0;
    int32 NonCoplanarCells = 0;
    for (int32 I = 0; I < G.Triangles.Num(); I += 3)
    {
        const FVector& VA = G.Vertices[G.Triangles[I]];
        const FVector& VB = G.Vertices[G.Triangles[I+1]];
        const FVector& VC = G.Vertices[G.Triangles[I+2]];
        const FVector Centroid = (VA + VB + VC) / 3.0;
        if (!TestTrue(TEXT("echantillon valide au centre d'une face"), AnastasisTerrainSurface::SampleHeight(Crop, Centroid.X, Centroid.Y, Z))) return false;
        MaxFaceError = FMath::Max(MaxFaceError, FMath::Abs(Z - Centroid.Z));
    }
    TestTrue(TEXT("l'echantillon suit le plan de chaque face rendue"), MaxFaceError <= Tolerance);

    // Le test precedent ne vaut que si la surface a du relief non coplanaire : sinon
    // n'importe quelle interpolation passerait. On le prouve au lieu de le supposer.
    for (int32 Y = 0; Y < 31; ++Y)
        for (int32 X = 0; X < 31; ++X)
        {
            const int32 A = Y*32+X;
            const double ZA = G.Vertices[A].Z, ZB = G.Vertices[A+1].Z, ZC = G.Vertices[A+32].Z, ZD = G.Vertices[A+33].Z;
            if (FMath::Abs((ZA + ZD) - (ZB + ZC)) > 1.e-3) ++NonCoplanarCells;
        }
    TestTrue(TEXT("la tranche contient des cellules non coplanaires"), NonCoplanarCells > 0);

    // 3. Continuite sur la diagonale B-C, la couture entre les deux triangles : les deux
    //    branches doivent rendre la meme hauteur, sinon la carte a une arete fantome.
    double MaxSeamError = 0.0;
    for (int32 Y = 0; Y < 31; ++Y)
        for (int32 X = 0; X < 31; ++X)
        {
            const int32 A = Y*32+X;
            const FVector Mid = (G.Vertices[A+1] + G.Vertices[A+32]) * 0.5;
            if (!TestTrue(TEXT("echantillon valide sur la diagonale"), AnastasisTerrainSurface::SampleHeight(Crop, Mid.X, Mid.Y, Z))) return false;
            MaxSeamError = FMath::Max(MaxSeamError, FMath::Abs(Z - Mid.Z));
        }
    TestTrue(TEXT("pas de discontinuite sur la diagonale"), MaxSeamError <= Tolerance);

    // 4. Refus hors emprise. Les sommets etant au CENTRE des tuiles, il n'y a pas de sol
    //    dans la demi-tuile exterieure : y poser quoi que ce soit serait le poser sur rien.
    TestFalse(TEXT("refus avant le premier sommet"), AnastasisTerrainSurface::SampleHeight(Crop, 10.0, 1600.0, Z));
    TestFalse(TEXT("refus au-dela du dernier sommet"), AnastasisTerrainSurface::SampleHeight(Crop, 3190.0, 1600.0, Z));
    TestFalse(TEXT("refus en Y hors emprise"), AnastasisTerrainSurface::SampleHeight(Crop, 1600.0, -50.0, Z));
    TestFalse(TEXT("refus sur une coordonnee non finie"), AnastasisTerrainSurface::SampleHeight(Crop, std::numeric_limits<double>::quiet_NaN(), 1600.0, Z));

    // 5. Le monde entier, et le decalage d'emprise : un point donne doit rendre la MEME
    //    hauteur qu'on l'echantillonne dans la tranche ou dans le monde.
    double MaxCropAgreement = 0.0;
    double ZWorld = 0.0;
    for (int32 I = 0; I < G.Vertices.Num(); ++I)
    {
        if (!TestTrue(TEXT("echantillon monde valide"), AnastasisTerrainSurface::SampleHeight(Full, G.Vertices[I].X, G.Vertices[I].Y, ZWorld))) return false;
        MaxCropAgreement = FMath::Max(MaxCropAgreement, FMath::Abs(ZWorld - G.Vertices[I].Z));
    }
    TestTrue(TEXT("tranche et monde s'accordent sur la meme hauteur"), MaxCropAgreement <= Tolerance);

    AddInfo(FString::Printf(TEXT("TERRAIN_SAMPLE vertex_error=%.9f face_error=%.9f seam_error=%.9f crop_world_error=%.9f noncoplanar_cells=%d/961"),
        MaxVertexError, MaxFaceError, MaxSeamError, MaxCropAgreement, NonCoplanarCells));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisDressingOnGround, "Anastasis.Terrain.DressingOnGround", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisDressingOnGround::RunTest(const FString&)
{
    UWorld* World = nullptr;
    for (const auto& Context : GEngine->GetWorldContexts()) if (Context.WorldType == EWorldType::Editor) { World = Context.World(); break; }
    if (!World) { AddError(TEXT("No Editor world")); return false; }
    auto* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Terrain.Surface"));
    const int32 Previous = Var->GetInt();
    auto* ForgeVar = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Terrain.Forge"));
    const int32 PreviousForge = ForgeVar ? ForgeVar->GetInt() : 0;
    if (ForgeVar) ForgeVar->Set(0, ECVF_SetByCode);
    FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
    auto* Actor = World->SpawnActor<AAnastasisWorldEmbodiment>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Actor) { AddError(TEXT("Spawn failed")); return false; }

    // This test locks legacy one-per-tile placement; Ecology has its own conditioning tests.
    Actor->ForestDressing.bEnabled = false;

    // Le dressing est reparti sur plusieurs HISM (un par archetype) ; les sept HISM de sol
    // portent le prefixe "Tiles_" et ne sont pas du dressing.
    auto DressingTransforms = [Actor]()
    {
        TArray<FTransform> Out;
        TArray<UHierarchicalInstancedStaticMeshComponent*> All; Actor->GetComponents(All);
        for (auto* Mesh : All)
        {
            if (Mesh->GetName().StartsWith(TEXT("Tiles_"))) continue;
            for (int32 I = 0; I < Mesh->GetInstanceCount(); ++I)
            {
                FTransform T;
                if (Mesh->GetInstanceTransform(I, T, true)) Out.Add(T);
            }
        }
        return Out;
    };

    const auto Full = AnastasisWorldView::CaptureCanonicalWorld(12345);
    const double Tolerance = 1.e-3;

    // --- Mode 2 : la surface couvre le monde. Chaque instance doit poser sa BASE sur la
    //     hauteur echantillonnee SOUS SA POSITION JITTEE, pas sur l'altitude de sa tuile.
    Var->Set(2, ECVF_SetByCode);
    Actor->Embody(12345, 96, 96);
    const TArray<FTransform> OnSurface = DressingTransforms();
    TestEqual(TEXT("le compteur suit les instances reellement posees"), OnSurface.Num(), Actor->GetDressingInstanceCount());
    TestTrue(TEXT("la carte porte du dressing"), OnSurface.Num() > 0);

    double MaxGroundError = 0.0, MaxTileAltError = 0.0;
    int32 OffTileAlt = 0;
    for (const FTransform& T : OnSurface)
    {
        const FVector L = T.GetLocation();
        const double Scale = T.GetScale3D().X;
        const double Lift = 0.5 * AnastasisPresentation::EngineBasicShapeSize * Scale;
        double GroundZ = 0.0;
        if (!TestTrue(TEXT("chaque instance a du sol sous elle"), AnastasisTerrainSurface::SampleHeight(Full, L.X, L.Y, GroundZ))) return false;
        MaxGroundError = FMath::Max(MaxGroundError, FMath::Abs(L.Z - Lift - GroundZ));

        // Et la preuve que le correctif sert a quelque chose : sur une pente, la hauteur
        // de la tuile n'est PAS la hauteur du sol sous l'instance jittee. Si les deux
        // coincidaient partout, ce test ne prouverait rien.
        const int32 TileX = FMath::Clamp(static_cast<int32>(L.X / AnastasisWorldView::TileWorldSize), 0, 95);
        const int32 TileY = FMath::Clamp(static_cast<int32>(L.Y / AnastasisWorldView::TileWorldSize), 0, 95);
        const double TileZ = Full.Tiles[TileY * 96 + TileX].Alt * AnastasisWorldView::AltitudeScale;
        const double Delta = FMath::Abs(GroundZ - TileZ);
        MaxTileAltError = FMath::Max(MaxTileAltError, Delta);
        if (Delta > 1.0) ++OffTileAlt;
    }
    TestTrue(TEXT("chaque base repose exactement sur le sol rendu"), MaxGroundError <= Tolerance);
    TestTrue(TEXT("le correctif deplace reellement des instances"), OffTileAlt > 0);

    // --- Mode 1 : la surface ne couvre que la tranche 32x32. Les tuiles hors emprise
    //     n'ont pas de sol : elles doivent etre REFUSEES, pas suspendues dans le vide.
    Var->Set(1, ECVF_SetByCode);
    Actor->Embody(12345, 96, 96);
    const TArray<FTransform> OnSlice = DressingTransforms();
    TestTrue(TEXT("mode 1 pose moins d'instances que mode 2"), OnSlice.Num() < OnSurface.Num());
    TestTrue(TEXT("mode 1 pose quand meme quelque chose"), OnSlice.Num() > 0);
    for (const FTransform& T : OnSlice)
    {
        const FVector L = T.GetLocation();
        TestTrue(TEXT("aucune instance hors de la tranche rendue"),
            L.X >= 0.0 && L.X <= 32.0 * AnastasisWorldView::TileWorldSize
            && L.Y >= 0.0 && L.Y <= 32.0 * AnastasisWorldView::TileWorldSize);
    }

    // --- Mode 0 : pas de surface. Le sol est le DESSUS de la dalle, pas son centre --
    //     sinon l'instance s'enfonce d'une demi-epaisseur dans la dalle.
    Var->Set(0, ECVF_SetByCode);
    Actor->Embody(12345, 96, 96);
    const TArray<FTransform> OnSlab = DressingTransforms();
    // Mode 0 pose STRICTEMENT plus que mode 2, et l'ecart n'est pas arbitraire : la
    // surface n'existe qu'entre le premier et le dernier CENTRE de tuile, donc une
    // instance jittee vers l'exterieur depuis une tuile de bordure tombe dans la
    // demi-tuile sans face rendue. La dalle, elle, couvre sa tuile entiere et l'accepte.
    // L'ecart doit valoir EXACTEMENT le nombre d'instances-dalle hors du domaine
    // echantillonne : ni une de plus (on refuserait a tort), ni une de moins.
    TestTrue(TEXT("mode 0 pose au moins autant que mode 2"), OnSlab.Num() >= OnSurface.Num());
    int32 OutsideSampledDomain = 0;
    for (const FTransform& T : OnSlab)
    {
        double Unused = 0.0;
        if (!AnastasisTerrainSurface::SampleHeight(Full, T.GetLocation().X, T.GetLocation().Y, Unused)) ++OutsideSampledDomain;
    }
    TestEqual(TEXT("l'ecart est exactement la bordure sans sol rendu"), OnSlab.Num() - OnSurface.Num(), OutsideSampledDomain);
    double MaxSlabError = 0.0;
    for (const FTransform& T : OnSlab)
    {
        const FVector L = T.GetLocation();
        const double Lift = 0.5 * AnastasisPresentation::EngineBasicShapeSize * T.GetScale3D().X;
        const int32 TileX = FMath::Clamp(static_cast<int32>(L.X / AnastasisWorldView::TileWorldSize), 0, 95);
        const int32 TileY = FMath::Clamp(static_cast<int32>(L.Y / AnastasisWorldView::TileWorldSize), 0, 95);
        const double SlabTop = Full.Tiles[TileY * 96 + TileX].Alt * AnastasisWorldView::AltitudeScale
            + AnastasisWorldDebugVisual::SlabTopOffsetZ;
        MaxSlabError = FMath::Max(MaxSlabError, FMath::Abs(L.Z - Lift - SlabTop));
    }
    TestTrue(TEXT("chaque base repose sur le dessus de sa dalle"), MaxSlabError <= Tolerance);

    AddInfo(FString::Printf(TEXT("DRESSING_ON_GROUND surface=%d slice=%d slab=%d refused_by_slice=%d ground_error=%.9f slab_error=%.9f moved_by_fix=%d max_tile_alt_delta=%.3f"),
        OnSurface.Num(), OnSlice.Num(), OnSlab.Num(), OnSurface.Num() - OnSlice.Num(),
        MaxGroundError, MaxSlabError, OffTileAlt, MaxTileAltError));
    AddInfo(FString::Printf(TEXT("DRESSING_BORDER slab_only=%d (instances de bordure sans face rendue sous elles)"), OutsideSampledDomain));

    Actor->Destroy(); Var->Set(Previous, ECVF_SetByCode);
    if (ForgeVar) ForgeVar->Set(PreviousForge, ECVF_SetByCode);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainMorphologyChannels, "Anastasis.Terrain.MorphologyChannels", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainMorphologyChannels::RunTest(const FString&)
{
    // Ce que le materiau de sol lit. Un canal muet ou deborde ne se voit pas a l'ecran
    // comme une erreur : il se voit comme un sol fade, ce qu'aucun test de couleur
    // n'attrape. On verifie donc a la fois la BORNE et la NON-VACUITE de chaque canal.
    const auto Full = AnastasisWorldView::CaptureCanonicalWorld(12345);
    AnastasisTerrainSurface::FGeometry G;
    if (!TestTrue(TEXT("build monde"), AnastasisTerrainSurface::Build(Full, G))) return false;

    TestEqual(TEXT("un UV0 par sommet"), G.UV0.Num(), G.Vertices.Num());
    TestEqual(TEXT("un UV1 par sommet"), G.UV1.Num(), G.Vertices.Num());

    int32 RockVerts = 0, LitterVerts = 0, WorkedVerts = 0, GrassVerts = 0, WetVerts = 0;
    double MaxWetness = 0.0;
    for (int32 I = 0; I < G.Vertices.Num(); ++I)
    {
        const double Rock = G.UV0[I].X, Litter = G.UV0[I].Y;
        const double Worked = G.UV1[I].X, Wetness = G.UV1[I].Y;
        const double Grass = 1.0 - Rock - Litter - Worked;

        // Partition de l'unite : sans cela l'herbe, qui est le RESTE, deviendrait
        // negative sur certaines tuiles et le materiau melangerait des poids qui ne
        // somment plus a 1 -- un sol qui s'assombrit ou sature sans raison lisible.
        TestTrue(TEXT("poids de famille dans [0,1]"),
            Rock >= 0.0 && Rock <= 1.0 && Litter >= 0.0 && Litter <= 1.0
            && Worked >= 0.0 && Worked <= 1.0 && Grass >= -KINDA_SMALL_NUMBER && Grass <= 1.0 + KINDA_SMALL_NUMBER);
        TestTrue(TEXT("humidite dans [0,1]"), Wetness >= 0.0 && Wetness <= 1.0);

        if (Rock > 0.5) ++RockVerts;
        if (Litter > 0.5) ++LitterVerts;
        if (Worked > 0.5) ++WorkedVerts;
        if (Grass > 0.5) ++GrassVerts;
        if (Wetness > 0.5) ++WetVerts;
        MaxWetness = FMath::Max(MaxWetness, Wetness);
    }

    // Non-vacuite : les quatre familles existent reellement dans le monde canonique.
    // Si l'une disparaissait, la grammaire de sol en revendiquerait une de trop.
    TestTrue(TEXT("la roche existe dans le monde"), RockVerts > 0);
    TestTrue(TEXT("la litiere existe dans le monde"), LitterVerts > 0);
    TestTrue(TEXT("la terre travaillee existe dans le monde"), WorkedVerts > 0);
    TestTrue(TEXT("l'herbe existe dans le monde"), GrassVerts > 0);
    TestTrue(TEXT("l'humidite est un champ, pas une constante"), WetVerts > 0 && MaxWetness > 0.9);

    // La correspondance canal <-> tuile source, verifiee sur la tuile, pas sur un total.
    for (int32 I = 0; I < G.Vertices.Num(); ++I)
    {
        const auto Mix = AnastasisTerrainSurface::SurfaceMixFor(Full.Tiles[I].Type);
        TestEqual(TEXT("UV0 = (Rock, Litter) de la tuile"), G.UV0[I], FVector2D(Mix.Rock, Mix.Litter));
        TestEqual(TEXT("UV1.x = Worked de la tuile"), G.UV1[I].X, Mix.Worked);
    }

    AddInfo(FString::Printf(
        TEXT("TERRAIN_MORPHOLOGY vertices=%d rock=%d litter=%d worked=%d grass=%d wet_gt_half=%d max_wetness=%.3f"),
        G.Vertices.Num(), RockVerts, LitterVerts, WorkedVerts, GrassVerts, WetVerts, MaxWetness));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainHydrologyGradient, "Anastasis.Terrain.HydrologyGradient", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainHydrologyGradient::RunTest(const FString&)
{
    using AnastasisWorld::ETileType;

    auto Luma = [](const FLinearColor& C)
    {
        return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B;
    };

    AnastasisWorldView::FVisualTile Dry;
    Dry.Type = ETileType::Grass;
    Dry.Alt = AnastasisWorld::SeaLevel + 0.02;
    Dry.Shore = 0.0;
    Dry.Wetness = 0.0;
    Dry.Shade = 0.0;
    Dry.FlowAmt = 0.0;

    AnastasisWorldView::FVisualTile Flood = Dry;
    Flood.Wetness = 0.8;

    AnastasisWorldView::FVisualTile Bank = Dry;
    Bank.Shore = 0.9;
    Bank.Wetness = 0.85;

    const double MinAlt = 0.0, MaxAlt = 1.0;
    const FLinearColor DryColor = AnastasisTerrainSurface::TileColor(Dry, MinAlt, MaxAlt);
    const FLinearColor FloodColor = AnastasisTerrainSurface::TileColor(Flood, MinAlt, MaxAlt);
    const FLinearColor BankColor = AnastasisTerrainSurface::TileColor(Bank, MinAlt, MaxAlt);

    TestTrue(TEXT("Wetness change la couleur sans Shore"), DryColor != FloodColor);
    TestTrue(TEXT("la crue assombrit le sol"), Luma(FloodColor) < Luma(DryColor));
    TestTrue(TEXT("Shore change la couleur a Wetness egale"), BankColor != FloodColor);
    TestTrue(TEXT("la berge n'est pas une plage seche"), BankColor.R < 0.45f);

    AnastasisWorldView::FVisualTile Still;
    Still.Type = ETileType::Water;
    Still.Shade = 0.6 + 0.4 * (-1.6);
    Still.FlowAmt = 0.0;
    AnastasisWorldView::FVisualTile Channel = Still;
    Channel.FlowAmt = 0.7;
    const FLinearColor StillColor = AnastasisTerrainSurface::TileColor(Still, MinAlt, MaxAlt);
    const FLinearColor ChannelColor = AnastasisTerrainSurface::TileColor(Channel, MinAlt, MaxAlt);
    TestTrue(TEXT("FlowAmt change la couleur d'eau a Shade egal"), StillColor != ChannelColor);
    TestTrue(TEXT("eau stagnante bleue dominante"), StillColor.B > StillColor.R && StillColor.B > StillColor.G);
    TestTrue(TEXT("eau courante bleue dominante"), ChannelColor.B > ChannelColor.R && ChannelColor.B > ChannelColor.G);
    TestEqual(TEXT("profondeur inversee"), AnastasisTerrainSurface::WaterDepthFromShade(Still.Shade), 0.4);

    const auto Full = AnastasisWorldView::CaptureCanonicalWorld(12345);
    const auto Before = Full;
    const auto Crop = AnastasisWorldView::CropSnapshot(Full, 0, 0, 32, 32);
    AnastasisTerrainSurface::FGeometry G;
    if (!TestTrue(TEXT("build crop"), AnastasisTerrainSurface::Build(Crop, G))) return false;
    TestEqual(TEXT("TERRAIN_CONTRACT vertices"), G.Vertices.Num(), 1024);
    TestEqual(TEXT("TERRAIN_CONTRACT triangles"), G.Triangles.Num() / 3, 1922);

    int32 InlandWet = 0, SaturatedShore = 0, CropFlowing = 0, StillWater = 0;
    for (int32 I = 0; I < Crop.Tiles.Num(); ++I)
    {
        const auto& T = Crop.Tiles[I];
        if (T.Type == ETileType::Water)
        {
            if (T.FlowAmt >= AnastasisHydrology::WaterFlowAmtGate) ++CropFlowing;
            else ++StillWater;
        }
        else
        {
            if (T.Wetness > 0.10 && T.Shore < 0.08) ++InlandWet;
            if (T.Shore > 0.5) ++SaturatedShore;
            TestEqual(TEXT("terre porte la couleur du sommet"), G.Colors[I], AnastasisTerrainSurface::TileColor(T, Crop.MinAlt, Crop.MaxAlt));
        }
    }
    TestTrue(TEXT("la tranche a une vase interieure (Wetness sans Shore)"), InlandWet > 0);
    TestTrue(TEXT("la tranche a une berge saturee"), SaturatedShore > 0);

    AnastasisTerrainSurface::FGeometry WorldGeom;
    if (!TestTrue(TEXT("build monde"), AnastasisTerrainSurface::Build(Full, WorldGeom))) return false;
    int32 WorldFlowing = 0;
    for (const auto& T : Full.Tiles)
    {
        if (T.Type == ETileType::Water && T.FlowAmt >= AnastasisHydrology::WaterFlowAmtGate) ++WorldFlowing;
    }
    TestTrue(TEXT("le monde a des tuiles d'ecoulement"), WorldFlowing > 0);

    for (int32 I = 0; I < Full.Tiles.Num(); ++I)
    {
        const auto& A = Full.Tiles[I];
        const auto& B = Before.Tiles[I];
        TestTrue(TEXT("aucune mutation de sim"),
            A.Type == B.Type && A.Alt == B.Alt && A.Shade == B.Shade && A.Shore == B.Shore
            && A.Wetness == B.Wetness && A.FlowAmt == B.FlowAmt && A.FlowX == B.FlowX && A.FlowZ == B.FlowZ);
    }

    auto Invalid = Crop;
    Invalid.Tiles[0].Wetness = std::numeric_limits<double>::quiet_NaN();
    AnastasisTerrainSurface::FGeometry Rejected;
    TestFalse(TEXT("refus Wetness non finie"), AnastasisTerrainSurface::Build(Invalid, Rejected));
    TestEqual(TEXT("aucune geometrie partielle"), Rejected.Vertices.Num(), 0);

    AddInfo(FString::Printf(TEXT("HYDROLOGY_GRADIENT inland_wet=%d saturated_shore=%d world_flowing=%d crop_flowing=%d still_water_crop=%d"),
        InlandWet, SaturatedShore, WorldFlowing, CropFlowing, StillWater));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisTerrainSlopeShade, "Anastasis.Terrain.SlopeShade", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisTerrainSlopeShade::RunTest(const FString&)
{
    using AnastasisWorld::ETileType;

    auto Luma = [](const FLinearColor& C)
    {
        return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B;
    };

    AnastasisWorldView::FVisualTile Flat;
    Flat.Type = ETileType::Grass;
    Flat.Alt = AnastasisWorld::SeaLevel + 0.05;
    Flat.Shade = 0.0;
    Flat.Shore = 0.0;
    Flat.Wetness = 0.0;

    AnastasisWorldView::FVisualTile Lit = Flat;
    Lit.Shade = 0.8;
    AnastasisWorldView::FVisualTile Shadowed = Flat;
    Shadowed.Shade = -0.8;

    const double MinAlt = 0.0, MaxAlt = 1.0;
    const FLinearColor FlatColor = AnastasisTerrainSurface::TileColor(Flat, MinAlt, MaxAlt);
    const FLinearColor LitColor = AnastasisTerrainSurface::TileColor(Lit, MinAlt, MaxAlt);
    const FLinearColor ShadowColor = AnastasisTerrainSurface::TileColor(Shadowed, MinAlt, MaxAlt);

    TestTrue(TEXT("Shade pente change la couleur a materiau egal"), LitColor != ShadowColor);
    TestTrue(TEXT("versant eclaire plus clair que versant oppose"), Luma(LitColor) > Luma(FlatColor));
    TestTrue(TEXT("versant oppose plus sombre que le plat"), Luma(ShadowColor) < Luma(FlatColor));
    TestTrue(TEXT("terre reste non bleue dominante"),
        (LitColor.B <= LitColor.R || LitColor.B <= LitColor.G)
        && (ShadowColor.B <= ShadowColor.R || ShadowColor.B <= ShadowColor.G));

    AnastasisWorldView::FVisualTile DeepWater;
    DeepWater.Type = ETileType::Water;
    DeepWater.Shade = 0.6 + 1.0 * (-1.6);
    DeepWater.FlowAmt = 0.0;
    AnastasisWorldView::FVisualTile ShallowWaterTile;
    ShallowWaterTile.Type = ETileType::Water;
    ShallowWaterTile.Shade = 0.6;
    ShallowWaterTile.FlowAmt = 0.0;
    const FLinearColor DeepColor = AnastasisTerrainSurface::TileColor(DeepWater, MinAlt, MaxAlt);
    const FLinearColor ShallowColor = AnastasisTerrainSurface::TileColor(ShallowWaterTile, MinAlt, MaxAlt);
    TestTrue(TEXT("sur l'eau Shade reste la profondeur"), Luma(DeepColor) < Luma(ShallowColor));
    TestTrue(TEXT("eau profonde bleue dominante"), DeepColor.B > DeepColor.R && DeepColor.B > DeepColor.G);

    const auto Full = AnastasisWorldView::CaptureCanonicalWorld(12345);
    const auto Crop = AnastasisWorldView::CropSnapshot(Full, 0, 0, 32, 32);
    AnastasisTerrainSurface::FGeometry G;
    if (!TestTrue(TEXT("build"), AnastasisTerrainSurface::Build(Crop, G))) return false;
    TestEqual(TEXT("TERRAIN_CONTRACT vertices"), G.Vertices.Num(), 1024);

    int32 LandShaded = 0;
    double MinLandShade = 1.0, MaxLandShade = -1.0;
    for (int32 I = 0; I < Crop.Tiles.Num(); ++I)
    {
        const auto& T = Crop.Tiles[I];
        TestEqual(TEXT("sommet = projection"), G.Colors[I], AnastasisTerrainSurface::TileColor(T, Crop.MinAlt, Crop.MaxAlt));
        if (T.Type == ETileType::Water) continue;
        MinLandShade = FMath::Min(MinLandShade, T.Shade);
        MaxLandShade = FMath::Max(MaxLandShade, T.Shade);
        if (FMath::Abs(T.Shade) > 0.05) ++LandShaded;
    }
    TestTrue(TEXT("la tranche a un relief de Shade sur la terre"), MaxLandShade > MinLandShade);
    TestTrue(TEXT("des tuiles terrestres portent une pente non nulle"), LandShaded > 0);

    AddInfo(FString::Printf(TEXT("SLOPE_SHADE land_shaded=%d shade_range=[%.6f, %.6f]"),
        LandShaded, MinLandShade, MaxLandShade));
    return true;
}

#endif
