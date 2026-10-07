#include "WorldView/AnastasisSettlementSurvey.h"
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
class FProjectedMesh
{
public:
    double Cell = 2000.0;
    TArray<FVector> V;
    TArray<FIntVector> T;
    TMap<FIntPoint,TArray<int32>> Bins;
    void Add(const FProcMeshSection* Section, const FTransform& Transform)
    {
        if (!Section || !Section->bSectionVisible) return;
        const int32 Offset = V.Num();
        for (const auto& P : Section->ProcVertexBuffer) V.Add(Transform.TransformPosition(P.Position));
        for (int32 I = 0; I + 2 < Section->ProcIndexBuffer.Num(); I += 3)
        {
            const int32 A = Offset+Section->ProcIndexBuffer[I], B = Offset+Section->ProcIndexBuffer[I+1], C = Offset+Section->ProcIndexBuffer[I+2];
            const int32 K = T.Add(FIntVector(A,B,C));
            const int32 X0 = FMath::FloorToInt32(FMath::Min3(V[A].X,V[B].X,V[C].X)/Cell);
            const int32 X1 = FMath::FloorToInt32(FMath::Max3(V[A].X,V[B].X,V[C].X)/Cell);
            const int32 Y0 = FMath::FloorToInt32(FMath::Min3(V[A].Y,V[B].Y,V[C].Y)/Cell);
            const int32 Y1 = FMath::FloorToInt32(FMath::Max3(V[A].Y,V[B].Y,V[C].Y)/Cell);
            for (int32 Y=Y0;Y<=Y1;++Y) for(int32 X=X0;X<=X1;++X) Bins.FindOrAdd(FIntPoint(X,Y)).Add(K);
        }
    }
    bool Sample(double X, double Y, double& Z) const
    {
        const auto* Bucket = Bins.Find(FIntPoint(FMath::FloorToInt32(X/Cell),FMath::FloorToInt32(Y/Cell)));
        if (!Bucket) return false;
        bool Found=false;
        for (int32 I : *Bucket)
        {
            const auto& Tri=T[I]; const auto& A=V[Tri.X]; const auto& B=V[Tri.Y]; const auto& C=V[Tri.Z];
            const double D=(B.Y-C.Y)*(A.X-C.X)+(C.X-B.X)*(A.Y-C.Y);
            if (FMath::Abs(D)<1.e-9) continue;
            const double U=((B.Y-C.Y)*(X-C.X)+(C.X-B.X)*(Y-C.Y))/D;
            const double W=((C.Y-A.Y)*(X-C.X)+(A.X-C.X)*(Y-C.Y))/D;
            const double Q=1.0-U-W;
            if (U < -1.e-7 || W < -1.e-7 || Q < -1.e-7) continue;
            const double H=U*A.Z+W*B.Z+Q*C.Z;
            if (!Found || H>Z) Z=H;
            Found=true;
        }
        return Found;
    }
};
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
    AnastasisSettlementSurveyPrivate::FProjectedMesh Ground, Water;
    Ground.Cell=Water.Cell=Cell;
    Ground.Add(Surface->GetProcMeshSection(0),Surface->GetComponentTransform());
    Water.Add(Surface->GetProcMeshSection(1),Surface->GetComponentTransform());
    Water.Add(Surface->GetProcMeshSection(2),Surface->GetComponentTransform());
    Out.Seed=Seed; Out.SourceWorld=World->GetPathName(); Out.TerrainComponent=Surface->GetPathName();
    const bool bWaterMeshAvailable=!Water.T.IsEmpty();
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
    const AnastasisVillage::FVillage& Village, AnastasisSettlementSite::FInputs& Out, double Relief)
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
    ReadSimulation(Seed, Sim, Village, In);
    if (bRenderedOk) MergeRenderObservation(Rendered, In);
    return In;
}
