#include "WorldView/AnastasisEcologicalDressing.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisHumanGeography.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
AnastasisWorldView::FWorldVisualSnapshot FlatForestEdge()
{
    auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
    for (auto& T : S.Tiles)
    {
        T.Type = T.X >= 48 ? AnastasisWorld::ETileType::Forest : AnastasisWorld::ETileType::Grass;
        T.Alt = 0.5; T.Wetness = 0;
    }
    return S;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisEcologyDeterminism, "Anastasis.Ecology.DeterminismAndAnchoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisEcologyDeterminism::RunTest(const FString&)
{
    using namespace AnastasisEcologicalDressing;
    const auto S = AnastasisWorldView::CaptureCanonicalWorld(12345u);
    const auto Before = S.Tiles;
    FAnastasisForestDressingSettings C;
    AnastasisEcologicalDressing::FPlan A,B;
    FString E;
    TestTrue(TEXT("canonical plan A"), Build(S,C,A,E));
    TestTrue(TEXT("canonical plan B"), Build(S,C,B,E));
    TestTrue(TEXT("reachable forest"), A.Instances.Num() > 0);
    TestEqual(TEXT("deterministic count"),A.Instances.Num(),B.Instances.Num());
    int32 Layers[3]={};
    for (int32 I=0; I<A.Instances.Num(); ++I)
    {
        const auto& P=A.Instances[I];
        if (B.Instances.IsValidIndex(I))
        {
            TestTrue(TEXT("exact placement"),P.Ground == B.Instances[I].Ground);
            TestEqual(TEXT("exact scale"),P.ScaleMultiplier,B.Instances[I].ScaleMultiplier);
            TestEqual(TEXT("exact layer"),static_cast<int32>(P.Layer),static_cast<int32>(B.Instances[I].Layer));
            TestEqual(TEXT("exact variant seed"),P.VisualSeed,B.Instances[I].VisualSeed);
        }
        double Z;
        TestTrue(TEXT("rendered ground exists"),AnastasisTerrainSurface::SampleHeight(S,P.Ground.X,P.Ground.Y,Z));
        TestTrue(TEXT("anchored exactly"),FMath::IsNearlyEqual(Z,P.Ground.Z,1.e-9));
        TestTrue(TEXT("above water"),Z > AnastasisTerrainSurface::WaterPlaneZ+C.WaterClearanceUU);
        TestTrue(TEXT("slope bound"),P.SlopeDegrees <= C.MaxSlopeDegrees);
        ++Layers[static_cast<uint8>(P.Layer)];
        for (int32 J=0; J<I; ++J)
            if (FVector::DistSquared2D(P.Ground,A.Instances[J].Ground) < FMath::Square(C.MinimumSpacing*AnastasisWorldView::TileWorldSize)-1.e-6)
                AddError(TEXT("minimum spacing violated"));
    }
    for (int32 I=0; I<S.Tiles.Num(); ++I)
        if (S.Tiles[I].Alt != Before[I].Alt || S.Tiles[I].Type != Before[I].Type
            || S.Tiles[I].Wetness != Before[I].Wetness || S.Tiles[I].Amount != Before[I].Amount)
            AddError(TEXT("snapshot mutated"));
    TestTrue(TEXT("three size layers reachable"), Layers[0]>0 && Layers[1]>0 && Layers[2]>0);
    AddInfo(FString::Printf(TEXT("seed=12345 instances=%d young=%d secondary=%d canopy=%d water=%d slope=%d spacing=%d"),
        A.Instances.Num(),Layers[0],Layers[1],Layers[2],A.RejectedWaterOrFootprint,A.RejectedSlope,A.RejectedSpacing));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisEcologyConditioning, "Anastasis.Ecology.EdgeAndConditioning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisEcologyConditioning::RunTest(const FString&)
{
    using namespace AnastasisEcologicalDressing;
    auto S=FlatForestEdge();
    FAnastasisForestDressingSettings C;
    AnastasisEcologicalDressing::FPlan A,Wet,Water,Steep,Disabled;
    FString E;
    TestTrue(TEXT("edge fixture"),Build(S,C,A,E));
    int32 Fringe=0, Interior=0, YoungFringe=0, CanopyInterior=0;
    for (const auto& P:A.Instances)
    {
        const double X=P.Ground.X/AnastasisWorldView::TileWorldSize;
        TestTrue(TEXT("no distant prairie scatter"),X > 45.0);
        if (X<48.0) { ++Fringe; YoungFringe += P.Layer==ELayer::Young; }
        if (X>=50.0 && X<53.0) { ++Interior; CanopyInterior += P.Layer==ELayer::Canopy; }
    }
    TestTrue(TEXT("graded presence beyond semantic forest"),Fringe>0);
    TestTrue(TEXT("interior denser than equal-width fringe"),Interior>Fringe);
    TestTrue(TEXT("young fringe and mature interior"),YoungFringe>0 && CanopyInterior>0);
    for (auto& T:S.Tiles) T.Wetness=1;
    TestTrue(TEXT("wet fixture"),Build(S,C,Wet,E));
    TestTrue(TEXT("wetness reduces population"),Wet.Instances.Num()<A.Instances.Num());
    for (auto& T:S.Tiles) T.Alt=0.20;
    TestTrue(TEXT("submerged fixture"),Build(S,C,Water,E));
    TestEqual(TEXT("zero submerged trees"),Water.Instances.Num(),0);
    // Pente IMPOSSIBLE formulee en angle, pas en altitude par tuile : 0.15 par tuile valait
    // 56 degres a 1 m par tuile, mais 20 a 4 m -- des arbres y poussaient, legitimement.
    // Dix degres au-dessus de la limite reglee, quelle que soit l'echelle.
    const double Rise = FMath::Tan(FMath::DegreesToRadians(C.MaxSlopeDegrees + 10.0))
        * AnastasisWorldView::TileWorldSize / AnastasisWorldView::AltitudeScale;
    for (auto& T:S.Tiles) { T.Alt=0.5+T.X*Rise; T.Wetness=0; }
    TestTrue(TEXT("steep fixture"),Build(S,C,Steep,E));
    TestEqual(TEXT("zero impossible-slope trees"),Steep.Instances.Num(),0);
    C.bEnabled=false;
    TestTrue(TEXT("disable accepted"),Build(S,C,Disabled,E));
    TestEqual(TEXT("disabled empty"),Disabled.Instances.Num(),0);
    AddInfo(FString::Printf(TEXT("fringe=%d interior_equal_width=%d dry=%d wet=%d submerged=%d steep=%d"),
        Fringe,Interior,A.Instances.Num(),Wet.Instances.Num(),Water.Instances.Num(),Steep.Instances.Num()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisEcologyBoundary, "Anastasis.Ecology.RejectInvalidInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisEcologyBoundary::RunTest(const FString&)
{
    using namespace AnastasisEcologicalDressing;
    auto S=FlatForestEdge();
    FAnastasisForestDressingSettings C;
    AnastasisEcologicalDressing::FPlan P; FString E;
    S.Tiles[17].Wetness=std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("NaN rejected"),Build(S,C,P,E));
    TestTrue(TEXT("precise tile path"),E.Contains(TEXT("Source.Tiles[17]")));
    TestEqual(TEXT("no partial placements"),P.Instances.Num(),0);
    S=FlatForestEdge(); C.MinimumSpacing=0;
    TestFalse(TEXT("invalid configuration rejected"),Build(S,C,P,E));
    C=FAnastasisForestDressingSettings{};
    S=AnastasisWorldView::CropSnapshot(S,0,0,32,32);
    TestFalse(TEXT("crop cannot invent missing ecological context"),Build(S,C,P,E));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMacroForestHabitat, "Anastasis.Ecology.MacroForestRenderedHabitat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMacroForestHabitat::RunTest(const FString&)
{
    using namespace AnastasisEcologicalDressing;
    using AnastasisWorld::ETileType;
    auto S = FlatForestEdge();
    for (auto& T : S.Tiles) T.Type = ETileType::Stone;
    FAnastasisForestDressingSettings C;
    FRenderedHabitat H;
    H.bHasBasin = true;
    constexpr double TileUU=AnastasisWorldView::TileWorldSize;
    H.Basin = FVector(48*TileUU,48*TileUU,800);
    H.SampleHeight = [](double X,double Y,double& Z) { Z=800; return X>=TileUU*0.5 && Y>=TileUU*0.5 && X<=TileUU*95.5 && Y<=TileUU*95.5; };
    AnastasisEcologicalDressing::FPlan P, Repeat;
    FString E;
    TestTrue(TEXT("rendered upland rock habitat"),Build(S,C,P,E,&H));
    TestTrue(TEXT("mountain forest reachable beyond semantic forest"),P.Instances.Num()>100);
    TestTrue(TEXT("repeat build"),Build(S,C,Repeat,E,&H));
    TestEqual(TEXT("repeat count"),P.Instances.Num(),Repeat.Instances.Num());
    for (int32 I=0; I<P.Instances.Num(); ++I)
    {
        const auto& Tree = P.Instances[I];
        TestEqual(TEXT("uses rendered height rather than semantic altitude"),Tree.Ground.Z,800.0);
        TestTrue(TEXT("basin kept open"),FVector::DistSquared2D(Tree.Ground,H.Basin)>=FMath::Square(C.BasinClearRadius*TileUU));
        TestTrue(TEXT("macro contains no sapling filler"),Tree.Layer!=ELayer::Young);
        if (Repeat.Instances.IsValidIndex(I)) TestTrue(TEXT("stable placement"),Tree.Ground==Repeat.Instances[I].Ground);
        for (int32 J=0; J<I; ++J)
            if (FVector::DistSquared2D(Tree.Ground,P.Instances[J].Ground)<FMath::Square(C.TrunkSpacingUU)-1.e-6)
                AddError(TEXT("macro spacing violated"));
    }
    H.SampleWaterHeight=[](double,double,double& Z) { Z=900; return true; };
    TestTrue(TEXT("raised river sample"),Build(S,C,P,E,&H));
    TestEqual(TEXT("actual river level excludes submerged trees"),P.Instances.Num(),0);
    H.SampleWaterHeight=nullptr;
    H.SampleHeight = [](double,double,double& Z) { Z=200; return true; };
    TestTrue(TEXT("water sample"),Build(S,C,P,E,&H));
    TestEqual(TEXT("no submerged trunks despite dry semantic terrain"),P.Instances.Num(),0);
    H.SampleHeight = [](double X,double,double& Z) { Z=800+X*3.0; return true; };
    TestTrue(TEXT("cliff sample"),Build(S,C,P,E,&H));
    TestEqual(TEXT("rendered cliffs excluded despite flat semantic terrain"),P.Instances.Num(),0);
    H.SampleHeight = [](double,double,double& Z) { Z=400; return true; };
    for (auto& T : S.Tiles) T.Type=ETileType::Grass;
    TestTrue(TEXT("valley sample"),Build(S,C,P,E,&H));
    // FOREST_TERRAIN_P2 : la prairie basse reste sans FORET, mais plus vide -- quelques arbres
    // isoles et bosquets, clairsemes (au plus un pour vingt tuiles), jamais une masse.
    int32 LoneInPrairie=0;
    for (const auto& Tree : P.Instances) LoneInPrairie += Tree.bLone ? 1 : 0;
    TestEqual(TEXT("flat low prairie holds no forest mass, only lone trees"),LoneInPrairie,P.Instances.Num());
    TestTrue(TEXT("the prairie is no longer empty"),P.Instances.Num()>0);
    TestTrue(TEXT("lone trees stay sparse"),P.Instances.Num()<=S.Tiles.Num()/20);
    TestEqual(TEXT("the plan counts its lone trees"),P.LoneTrees,LoneInPrairie);
    H.SampleHeight = [](double,double,double& Z) { Z=1000; return true; };
    for (auto& T : S.Tiles) T.Type=ETileType::Field;
    TestTrue(TEXT("fields"),Build(S,C,P,E,&H));
    TestEqual(TEXT("fields reserved even at altitude"),P.Instances.Num(),0);
    for (auto& T : S.Tiles) T.Type=ETileType::Ruin;
    TestTrue(TEXT("ruins"),Build(S,C,P,E,&H));
    TestEqual(TEXT("existing ruin footprints reserved"),P.Instances.Num(),0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisMacroForestCanonical, "Anastasis.Ecology.MacroForestCanonicalRelief",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisMacroForestCanonical::RunTest(const FString&)
{
    using namespace AnastasisEcologicalDressing;
    for (const uint32 Seed : {12345u, 42u, 98765u})
    {
        auto S=AnastasisWorldView::CaptureCanonicalWorld(Seed);
        S.SpatialScale=5.0;
        S.bHumanGeography=Seed==AnastasisWorldView::ReferenceSeed;
        AnastasisTerrainSurface::FGeometry G;
        AnastasisTerrainForge::FMesh M;
        if (!TestTrue(TEXT("surface"),AnastasisTerrainSurface::Build(S,G))
            || !TestTrue(TEXT("rendered relief"),AnastasisTerrainForge::Apply(S,G,M))) return false;
        const auto BeforeVertices=G.Vertices;
        FRenderedHabitat H;
        H.SampleHeight=[&](double X,double Y,double& Z) { return AnastasisTerrainForge::SampleHeight(M,X,Y,Z); };
        H.SampleWaterHeight=[](double X,double Y,double& Z) { return AnastasisTerrainForge::SampleActiveWater(X,Y,Z); };
        H.Basin=FVector(M.BasinX,M.BasinY,M.BasinZ);
        H.bHasBasin=M.bBasinFound;
        FAnastasisForestDressingSettings C;
        AnastasisEcologicalDressing::FPlan P, B;
        FString E;
        TestTrue(TEXT("macro build"),Build(S,C,P,E,&H));
        TestTrue(TEXT("macro repeat"),Build(S,C,B,E,&H));
        TestEqual(TEXT("stable count"),P.Instances.Num(),B.Instances.Num());
        int32 Canopy=0, Stone=0;
        for (int32 I=0; I<P.Instances.Num(); ++I)
        {
            const auto& Tree=P.Instances[I];
            double Z=0;
            TestTrue(TEXT("rendered anchor exists"),H.SampleHeight(Tree.Ground.X,Tree.Ground.Y,Z));
            TestTrue(TEXT("exact rendered root"),FMath::IsNearlyEqual(Z,Tree.Ground.Z,1.e-8));
            TestTrue(TEXT("rendered slope bound"),Tree.SlopeDegrees<=C.HillsideMaxSlope);
            TestTrue(TEXT("above water"),Z>AnastasisTerrainSurface::WaterPlaneZ+C.WaterClearanceUU);
            double WaterZ=0;
            TestTrue(TEXT("water sample"),H.SampleWaterHeight(Tree.Ground.X,Tree.Ground.Y,WaterZ));
            TestTrue(TEXT("above actual water"),Z>WaterZ+C.WaterClearanceUU);
            if (H.bHasBasin) TestTrue(TEXT("basin reserved"),FVector::DistSquared2D(Tree.Ground,H.Basin)>=FMath::Square(C.BasinClearRadius*AnastasisWorldView::TileWorldSize*S.SpatialScale));
            if (S.bHumanGeography)
            {
                const auto Geo=AnastasisHumanGeography::Evaluate(Tree.Ground.X/(AnastasisWorldView::TileWorldSize*S.SpatialScale),
                    Tree.Ground.Y/(AnastasisWorldView::TileWorldSize*S.SpatialScale),0.0);
                // FOREST_TERRAIN_P2 : les vallees restent sans foret ; un arbre isole peut s'y tenir,
                // jamais sur la route du col ni dans le lit de la riviere.
                if (!Tree.bLone) TestTrue(TEXT("existing habitable valleys and pass remain free of forest"),Geo.ValleyWeight<0.8);
                TestTrue(TEXT("existing river corridor remains open"),Geo.RiverWeight<0.5);
                TestTrue(TEXT("no tree on the pass road"),Geo.RoadWeight<0.3);
            }
            if (B.Instances.IsValidIndex(I)) TestTrue(TEXT("identical tree"),Tree.Ground==B.Instances[I].Ground && Tree.ScaleMultiplier==B.Instances[I].ScaleMultiplier);
            Canopy += Tree.Layer==ELayer::Canopy;
            Stone += S.Tiles[Tree.SourceIndex].Type==AnastasisWorld::ETileType::Stone;
        }
        TestTrue(TEXT("macro forest present at 1.9km scale"),P.Instances.Num()>1000);
        TestTrue(TEXT("canopy dominates"),Canopy>P.Instances.Num()/2);
        TestTrue(TEXT("mountain habitat populated"),Stone>0);
        TestTrue(TEXT("terrain unchanged"),G.Vertices==BeforeVertices);
        AddInfo(FString::Printf(TEXT("MACRO_FOREST seed=%u trees=%d canopy=%d on_stone=%d open_rejected=%d slope_rejected=%d"),
            Seed,P.Instances.Num(),Canopy,Stone,P.RejectedOpenGround,P.RejectedSlope));
    }
    AnastasisTerrainForge::ClearActive();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisForestEdgesAndOpenings, "Anastasis.Ecology.ForestEdgesAndOpenings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisForestEdgesAndOpenings::RunTest(const FString&)
{
    // FOREST_TERRAIN_P2. Un bord foret / champ droit, a l'echelle reelle (5), en altitude : la
    // lisiere doit etre une bande qui s'eclaircit, pas une ligne pleine densite sur la grille ;
    // l'interieur doit varier en densite et s'ouvrir en clairieres.
    using namespace AnastasisEcologicalDressing;
    using AnastasisWorld::ETileType;
    auto S = FlatForestEdge();
    for (auto& T : S.Tiles) T.Type = T.X >= 48 ? ETileType::Forest : ETileType::Field;
    S.SpatialScale = 5.0;
    const double TileUU = AnastasisWorldView::TileWorldSize * S.SpatialScale;
    FAnastasisForestDressingSettings C;
    FRenderedHabitat H;
    H.SampleHeight = [](double,double,double& Z) { Z=5000; return true; };
    FPlan P;
    FString E;
    if (!TestTrue(TEXT("edge fixture builds"), Build(S,C,P,E,&H))) return false;

    int32 Columns[12] = {};
    int32 Interior = 0;
    TMap<FIntPoint, int32> Blocks3, Blocks2;
    for (const auto& Tree : P.Instances)
    {
        const double X = Tree.Ground.X / TileUU, Y = Tree.Ground.Y / TileUU;
        if (!TestTrue(TEXT("no tree on the field side"), X >= 48.0)) return false;
        const int32 Column = FMath::FloorToInt((X - 48.0) * 2.0);
        if (Column >= 0 && Column < 12) ++Columns[Column];
        if (X >= 55.0 && X < 93.0) ++Interior;
        if (X >= 54.0 && X < 93.0 && Y >= 3.0 && Y < 93.0)
            ++Blocks3.FindOrAdd(FIntPoint(FMath::FloorToInt((X - 54.0) / 3.0), FMath::FloorToInt((Y - 3.0) / 3.0)));
        if (X >= 54.0 && X < 94.0 && Y >= 2.0 && Y < 94.0)
            ++Blocks2.FindOrAdd(FIntPoint(FMath::FloorToInt((X - 54.0) / 2.0), FMath::FloorToInt((Y - 2.0) / 2.0)));
    }
    // Interieur ramene a la largeur d'une demi-tuile, comme les colonnes de lisiere.
    const double InteriorPerColumn = Interior / (38.0 * 2.0);
    TestTrue(TEXT("interior reached"), InteriorPerColumn > 0.0);
    TestTrue(TEXT("the first half-tile of the fringe is sparse, not a full-density cut"), Columns[0] < 0.4 * InteriorPerColumn);
    TestTrue(TEXT("the fringe fills in towards the interior"), Columns[0] < Columns[2] && Columns[1] < Columns[3]);

    // Densite variable : coefficient de variation des blocs de 3x3 tuiles.
    double Sum = 0.0, SumSq = 0.0;
    const int32 Count3 = 13 * 30;
    for (int32 BX = 0; BX < 13; ++BX)
        for (int32 BY = 0; BY < 30; ++BY)
        {
            const double N = Blocks3.FindRef(FIntPoint(BX, BY));
            Sum += N; SumSq += N * N;
        }
    const double Mean = Sum / Count3;
    const double CV = Mean > 0.0 ? FMath::Sqrt(FMath::Max(SumSq / Count3 - Mean * Mean, 0.0)) / Mean : 0.0;
    TestTrue(TEXT("forest density varies from stand to stand"), CV > 0.35);
    // Clairieres : des blocs de 2x2 tuiles vides au coeur du peuplement.
    int32 Empty = 0;
    for (int32 BX = 0; BX < 20; ++BX)
        for (int32 BY = 0; BY < 46; ++BY)
            Empty += Blocks2.Contains(FIntPoint(BX, BY)) ? 0 : 1;
    TestTrue(TEXT("glades open inside the forest"), Empty >= 10);
    AddInfo(FString::Printf(TEXT("FOREST_EDGE columns=[%d %d %d %d %d %d] interior_per_column=%.1f cv=%.2f empty_2x2=%d trees=%d lone=%d"),
        Columns[0], Columns[1], Columns[2], Columns[3], Columns[4], Columns[5], InteriorPerColumn, CV, Empty, P.Instances.Num(), P.LoneTrees));
    return true;
}
#endif
