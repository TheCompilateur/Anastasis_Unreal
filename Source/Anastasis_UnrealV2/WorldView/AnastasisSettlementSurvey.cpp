#include "WorldView/AnastasisSettlementSurvey.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisWorldView.h"
#include "Village/AnastasisVillage.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
