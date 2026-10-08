#include "WorldView/AnastasisSettlementSurvey.h"
#include "WorldView/AnastasisProjectedMesh.h"
#include "WorldView/AnastasisCanonicalGeography.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisWorldView.h"
#include "Village/AnastasisVillage.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "ProceduralMeshComponent.h"

namespace AnastasisSettlementSurveyPrivate
{
// XY bins over actual triangles, supporting ground, clipped lake mesh and river ribbons.
// This avoids thousands of collision ray casts and cannot accidentally sample another PIE world.
void AddSection(AnastasisProjectedMesh::FProjectedMesh& Mesh, const FProcMeshSection* Section, const FTransform& Transform)
{
    if (!Section || !Section->bSectionVisible) return;
    TArray<FVector> Vertices; Vertices.Reserve(Section->ProcVertexBuffer.Num());
    for (const auto& P : Section->ProcVertexBuffer) Vertices.Add(P.Position);
    TArray<int32> Indices; Indices.Reserve(Section->ProcIndexBuffer.Num());
    for (const uint32 I : Section->ProcIndexBuffer) Indices.Add(static_cast<int32>(I));
    Mesh.Add(Vertices, Indices, Transform);
}
}

bool AnastasisSettlementSurvey::Read(UWorld* World, uint32 Seed, const AnastasisWorld::FWorld& Sim,
    const AnastasisVillage::FVillage& Village, AnastasisSettlementSite::FInputs& Out, FString& Error)
{
    Out = {}; Error.Reset();
    if (!World) { Error=TEXT("no_world"); return false; }
    UProceduralMeshComponent* Surface=nullptr;
    double Scale=0;
    for(TActorIterator<AAnastasisWorldEmbodiment> It(World);It;++It)
    {
        const auto& Snap=It->GetSnapshot();
        if(Snap.Seed!=Seed || Snap.SourceW!=Sim.W || Snap.SourceH!=Sim.H) continue;
        TArray<UProceduralMeshComponent*> Meshes; It->GetComponents(Meshes);
        for(auto* M:Meshes)
        {
            if(!M || M->GetFName()!=FName(TEXT("ExperimentalTerrain")) || !M->IsVisible() || !M->IsCollisionEnabled()) continue;
            const auto* Section=M->GetProcMeshSection(0);
            if(!Section || Section->ProcIndexBuffer.IsEmpty()) continue;
            if(Surface) { Error=TEXT("ambiguous_terrain"); return false; }
            // Existing village coordinates assume an untransformed terrain actor.
            if(!M->GetComponentTransform().Equals(FTransform::Identity,0.001)) { Error=TEXT("unsupported_terrain_transform"); return false; }
            Surface=M; Scale=Snap.SpatialScale;
        }
    }
    if(!Surface || !FMath::IsFinite(Scale) || Scale<=0) { Error=TEXT("terrain_not_ready"); return false; }
    const double Cell=AnastasisWorldView::TileWorldSize*Scale;
    AnastasisProjectedMesh::FProjectedMesh Ground, Water;
    Ground.Cell=Water.Cell=Cell;
    AnastasisSettlementSurveyPrivate::AddSection(Ground,Surface->GetProcMeshSection(0),Surface->GetComponentTransform());
    AnastasisSettlementSurveyPrivate::AddSection(Water,Surface->GetProcMeshSection(1),Surface->GetComponentTransform());
    AnastasisSettlementSurveyPrivate::AddSection(Water,Surface->GetProcMeshSection(2),Surface->GetComponentTransform());
    Out.Seed=Seed; Out.SourceWorld=World->GetPathName(); Out.TerrainComponent=Surface->GetPathName();
    const bool bWaterMeshAvailable=!Water.IsEmpty();
    Out.W=Sim.W; Out.H=Sim.H; Out.TileMetres=Cell/100.0; Out.Cells.SetNum(Sim.Tiles.Num());
    for(int32 I=0;I<Sim.Tiles.Num();++I)
    {
        const auto& T=Sim.Tiles[I]; auto& C=Out.Cells[I];
        const double X=(I%Sim.W+0.5)*Cell, Y=(I/Sim.W+0.5)*Cell;
        double H[9]={}; bool Complete=true; double Freeboard=1.e12;
        for(int32 V=-1;V<=1;++V) for(int32 U=-1;U<=1;++U)
        {
            const int32 J=(V+1)*3+U+1; const double PX=X+U*Cell*0.4, PY=Y+V*Cell*0.4;
            if(!Ground.Sample(PX,PY,H[J])) { Complete=false; continue; }
            double WZ=0;
            if(Water.Sample(PX,PY,WZ)) Freeboard=FMath::Min(Freeboard,H[J]-WZ);
        }
        C.bSurveyed=Complete; C.Height=H[4];
        C.bWalkable=!Village.IsFootBlocked(I%Sim.W+0.5,I/Sim.W+0.5);
        C.bDry=Complete && Freeboard>=10.0;
        double DX=0,DY=0;
        for(int32 V=0;V<3;++V) for(int32 U=0;U<2;++U) DX=FMath::Max(DX,FMath::Abs(H[V*3+U+1]-H[V*3+U])/(Cell*0.4));
        for(int32 V=0;V<2;++V) for(int32 U=0;U<3;++U) DY=FMath::Max(DY,FMath::Abs(H[(V+1)*3+U]-H[V*3+U])/(Cell*0.4));
        C.Slope=Complete ? FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX*DX+DY*DY))) : 90.0;
        C.RenderedSlope=Complete ? C.Slope : -1.0;
        C.Fertility=T.Fertility;
        C.bCenterAllowed=(T.Type==AnastasisWorld::ETileType::Grass || T.Type==AnastasisWorld::ETileType::Scrub)
            && T.Wetness<0.6 && Freeboard>=100.0;
        double WZ=0;
        C.bWater=T.Type==AnastasisWorld::ETileType::Water && Ground.Sample(X,Y,C.Height)
            && Water.Sample(X,Y,WZ) && WZ>=C.Height-1.0;
        // Observe all centres, including visual water on semantically dry tiles.
        // Missing ground / missing water mesh remains UNKNOWN, not agreement.
        double CentreGround=0;
        C.bSimWater=T.Type==AnastasisWorld::ETileType::Water;
        C.bWaterObserved=bWaterMeshAvailable && Ground.Sample(X,Y,CentreGround);
        C.bRenderedWater=C.bWaterObserved && Water.Sample(X,Y,WZ) && WZ>=CentreGround-1.0;
        C.bWood=T.Resource==AnastasisWorld::EResource::Wood && T.Amount>0;
        C.bFood=T.Type==AnastasisWorld::ETileType::Field && T.Resource==AnastasisWorld::EResource::Food && T.Amount>0;
    }
    return true;
}

void AnastasisSettlementSurvey::ReadSimulation(uint32 Seed, const AnastasisWorld::FWorld& Sim,
    const AnastasisVillage::FVillage& Village, AnastasisSettlementSite::FInputs& Out, double Relief,
    const AnastasisCanonicalGeography::FGeography* Canonical)
{
    using AnastasisWorld::ETileType;
    Out = {};
    Out.Seed=Seed; Out.W=Sim.W; Out.H=Sim.H;
    Out.SelectionSource=TEXT("simulation");
    Out.SourceWorld=TEXT("simulation"); Out.TerrainComponent=TEXT("simulation_tiles");
    const double Cell=AnastasisWorldView::TileWorldSize*CanonicalSpatialScale;
    Out.TileMetres=Cell/100.0;
    Out.Cells.SetNum(Sim.Tiles.Num());
    if (Sim.W<=0 || Sim.H<=0 || Sim.Tiles.Num()!=Sim.W*Sim.H) return;
    const bool bCanonical=Canonical && Canonical->bValid && Canonical->W==Sim.W && Canonical->H==Sim.H
        && Canonical->GroundZ.Num()==Sim.Tiles.Num() && Canonical->Slope.Num()==Sim.Tiles.Num();
    if (bCanonical) Out.TerrainComponent=TEXT("canonical_drained_relief");
    // Land above sea scaled by `Relief` (1 = the simulation's own relief, the default); the sea stays put.
    auto PolicyHeight=[&](int32 X,int32 Y)
    {
        const double Alt=Sim.Tiles[Y*Sim.W+X].Alt;
        const double Above=Alt-AnastasisWorld::SeaLevel;
        return AnastasisWorldView::AltitudeToUnreal(AnastasisWorld::SeaLevel+(Above>0.0 ? Above*Relief : Above), CanonicalSpatialScale);
    };
    auto IsWater=[&](int32 X,int32 Y)
    {
        return X>=0 && Y>=0 && X<Sim.W && Y<Sim.H && Sim.Tiles[Y*Sim.W+X].Type==ETileType::Water;
    };
    for(int32 I=0;I<Sim.Tiles.Num();++I)
    {
        const auto& T=Sim.Tiles[I]; auto& C=Out.Cells[I];
        const int32 X=I%Sim.W, Y=I/Sim.W;
        const bool bWater=T.Type==ETileType::Water;
        C.bSurveyed=true;
        C.Height=PolicyHeight(X,Y);
        C.bWalkable=!Village.IsFootBlocked(X+0.5,Y+0.5);
        C.bDry=!bWater;
        // Steepest neighbour step per axis, like the rendered survey's sample grid.
        double DX=0,DY=0;
        if (X>0) DX=FMath::Max(DX,FMath::Abs(C.Height-PolicyHeight(X-1,Y))/Cell);
        if (X+1<Sim.W) DX=FMath::Max(DX,FMath::Abs(PolicyHeight(X+1,Y)-C.Height)/Cell);
        if (Y>0) DY=FMath::Max(DY,FMath::Abs(C.Height-PolicyHeight(X,Y-1))/Cell);
        if (Y+1<Sim.H) DY=FMath::Max(DY,FMath::Abs(PolicyHeight(X,Y+1)-C.Height)/Cell);
        C.Slope=FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX*DX+DY*DY)));
        if (bCanonical) { C.Height=Canonical->GroundZ[I]; C.Slope=Canonical->Slope[I]; }
        C.Fertility=T.Fertility;
        // The rendered survey wanted 1 m of freeboard; on tiles, a centre is not a shore tile.
        const bool bShore=IsWater(X-1,Y)||IsWater(X+1,Y)||IsWater(X,Y-1)||IsWater(X,Y+1);
        C.bCenterAllowed=(T.Type==ETileType::Grass || T.Type==ETileType::Scrub) && T.Wetness<0.6 && !bWater && !bShore;
        C.bWater=bWater;
        C.bSimWater=bWater;
        C.bWood=T.Resource==AnastasisWorld::EResource::Wood && T.Amount>0;
        C.bFood=T.Type==ETileType::Field && T.Resource==AnastasisWorld::EResource::Food && T.Amount>0;
    }
}

void AnastasisSettlementSurvey::MergeRenderObservation(const AnastasisSettlementSite::FInputs& Rendered,
    AnastasisSettlementSite::FInputs& Out)
{
    if (Rendered.W!=Out.W || Rendered.H!=Out.H || Rendered.Cells.Num()!=Out.Cells.Num()) return;
    Out.SourceWorld=Rendered.SourceWorld; Out.TerrainComponent=Rendered.TerrainComponent;
    for(int32 I=0;I<Out.Cells.Num();++I)
    {
        const auto& R=Rendered.Cells[I]; auto& C=Out.Cells[I];
        C.bWaterObserved=R.bWaterObserved;
        C.bRenderedWater=R.bRenderedWater;
        C.RenderedSlope=R.RenderedSlope;
    }
}

bool AnastasisSettlementSurvey::SiteFromSimulation()
{
    const IConsoleVariable* Var=IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Village.SiteSource"));
    return !Var || Var->GetInt()!=0;
}

AnastasisSettlementSite::FInputs AnastasisSettlementSurvey::SiteInputs(const AnastasisSettlementSite::FInputs& Rendered,
    bool bRenderedOk, uint32 Seed, const AnastasisWorld::FWorld& Sim, const AnastasisVillage::FVillage& Village)
{
    if (!SiteFromSimulation()) return Rendered;
    AnastasisSettlementSite::FInputs In;
    // WATER_NETWORK_001 : the relief the player sees (canonical recipe, never a render CVar).
    const AnastasisCanonicalGeography::FGeography& Canonical=AnastasisCanonicalGeography::Get(Seed);
    ReadSimulation(Seed, Sim, Village, In, ReliefFactor, Canonical.bValid ? &Canonical : nullptr);
    if (bRenderedOk) MergeRenderObservation(Rendered, In);
    return In;
}
