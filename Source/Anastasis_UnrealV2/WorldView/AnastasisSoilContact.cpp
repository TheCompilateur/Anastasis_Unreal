#include "WorldView/AnastasisSoilContact.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisPlaces.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Math/RandomStream.h"
#include "Anastasis_UnrealV2.h"

namespace AnastasisSoilContact
{
static TAutoConsoleVariable<int32> Enabled(TEXT("anastasis.Dressing.SoilContact"),1,
 TEXT("Bounded rock-soil-water pilot on seed12345 scale5. Decorative, no collision."));

FPlan Build(const FInputs& In)
{
 FPlan Out;
 if(!In.Ground || !In.Water || !In.Search.bIsValid) return Out;
 auto Height=[&](FVector2D P,double& Z){return In.Ground(P.X,P.Y,Z) && FMath::IsFinite(Z);};
 auto Wet=[&](FVector2D P){double G,W; return Height(P,G) && In.Water(P.X,P.Y,W) && FMath::IsFinite(W) && W-G>5;};
 double Best=-1.e30;
 for(double Y=In.Search.Min.Y;Y<=In.Search.Max.Y;Y+=400)
 for(double X=In.Search.Min.X;X<=In.Search.Max.X;X+=400)
 {
  FVector2D P(X,Y); double Z,E,W,N,S;
  if(!Height(P,Z)||Wet(P)||!Height(P+FVector2D(100,0),E)||!Height(P-FVector2D(100,0),W)
   ||!Height(P+FVector2D(0,100),N)||!Height(P-FVector2D(0,100),S)) continue;
  FVector2D D((E-W)/200,(N-S)/200); double Slope=D.Size();
  if(Slope<0.06 || Slope>0.65) continue;
  D.Normalize(); double High;
  if(!Height(P+D*1200,High)||High-Z<500) continue;
  double WaterDistance=1.e30;
  for(int32 A=0;A<12;++A) for(double R=400;R<=2800;R+=400)
  {
   const double T=A*2*PI/12; FVector2D Q=P+FVector2D(FMath::Cos(T),FMath::Sin(T))*R;
   // Deposits fan downhill toward actual water, not behind the cliff.
   if(FVector2D::DotProduct(Q-P,D)>400) continue;
   if(Wet(Q)) WaterDistance=FMath::Min(WaterDistance,R);
  }
  if(WaterDistance>2800) continue;
  const double Score=FMath::Min(High-Z,1800.0)-0.25*WaterDistance-0.02*FVector2D::Distance(P,FVector2D(35000,19500));
  if(Score>Best){Best=Score;Out.Anchor=FVector(X,Y,Z);Out.Uphill=D;Out.bValid=true;}
 }
 if(!Out.bValid) return Out;
 FRandomStream R(In.Seed+73003);
 const FVector2D A(Out.Anchor),U=Out.Uphill,Side(-U.Y,U.X);
 const double Strike=FMath::RadiansToDegrees(FMath::Atan2(Side.Y,Side.X));
 // Three discontinuous lobes share the same source face; coarse fragments remain uphill.
 const double Lobes[3]={-950,120,900};
 for(int32 Kind=0;Kind<3;++Kind)
 {
  const int32 Count=Kind==0?14:(Kind==1?38:110);
  for(int32 I=0;I<Count;++I)
  {
   const double L=Lobes[I%3]+R.FRandRange(-260,260);
   const double Down=Kind==0?R.FRandRange(-600,80):(Kind==1?R.FRandRange(0,650):R.FRandRange(350,1350));
   const FVector2D P=A+Side*L-U*Down;
   if(FVector2D::Distance(P,A)>1650) continue;
   double Z,Water;
   if(!Height(P,Z)) continue;
   const bool bWet=In.Water(P.X,P.Y,Water) && Water>Z;
   if(bWet && (Kind<2 || Water-Z>20)) continue;
   const double Diameter=Kind==0?R.FRandRange(170,340):(Kind==1?R.FRandRange(35,100):R.FRandRange(8,27));
   // Reject footprints with extreme ground variation; larger stones stay embedded.
   double Lo=Z,Hi=Z; bool bOK=true;
   for(int32 K=0;K<4;++K){double Q;const double T=K*PI/2;
    if(!Height(P+FVector2D(FMath::Cos(T),FMath::Sin(T))*Diameter*0.35,Q)){bOK=false;break;}
    Lo=FMath::Min(Lo,Q);Hi=FMath::Max(Hi,Q);}
   if(!bOK || Hi-Lo>Diameter*0.8) continue;
   FStone S;S.Foot=FVector(P,Lo);S.Diameter=Diameter;S.Kind=Kind;S.Variant=I%3;
   S.Yaw=Strike+R.FRandRange(Kind==0?-12:-70,Kind==0?12:70);S.Sink=Kind==0?0.42:(Kind==1?0.32:0.45);
   Out.Stones.Add(S);
  }
 }
 return Out;
}

void Apply(AActor& Owner,int32 Seed,double SpatialScale)
{
 TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> Components(&Owner);
 TMap<FName,UHierarchicalInstancedStaticMeshComponent*> Existing;
 for(auto* C:Components) if(C->GetName().StartsWith(TEXT("SoilContact_"))){C->ClearInstances();Existing.Add(C->GetFName(),C);}
 if(Enabled.GetValueOnGameThread()==0||Seed!=12345||FMath::Abs(SpatialScale-5.0)>0.01||GIsAutomationTesting) return;
 FInputs In;In.Seed=Seed;
 In.Ground=[](double X,double Y,double& Z){return AnastasisTerrainForge::SampleActive(X,Y,Z);};
 In.Water=[](double X,double Y,double& Z){return AnastasisTerrainForge::SampleActiveWater(X,Y,Z);};
 const FPlan Plan=Build(In);
 if(!Plan.bValid){UE_LOG(LogAnastasis_UnrealV2,Warning,TEXT("SOIL_CONTACT no_supported_site"));return;}
 int32 Counts[3]={};int32 Missing=0;
 for(int32 Kind=0;Kind<3;++Kind) for(int32 Variant=0;Variant<3;++Variant)
 {
  const auto Family=Kind==0?AnastasisPlaces::EFamily::RockSplit:(Kind==1?AnastasisPlaces::EFamily::RockBoulder:AnastasisPlaces::EFamily::RockLow);
  UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,*AnastasisPlaces::MeshPath(Family,Variant));
  if(!Mesh){++Missing;continue;}
  const FName Name(*FString::Printf(TEXT("SoilContact_%d_%d"),Kind,Variant));
  auto* C=Existing.FindRef(Name);
  if(!C){C=NewObject<UHierarchicalInstancedStaticMeshComponent>(&Owner,Name,RF_Transient);
   C->SetupAttachment(Owner.GetRootComponent());C->SetMobility(EComponentMobility::Movable);
   C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetGenerateOverlapEvents(false);C->SetCanEverAffectNavigation(false);
   Owner.AddInstanceComponent(C);C->RegisterComponent();}
  C->SetStaticMesh(Mesh);C->SetCastShadow(Kind<2);C->SetCullDistances(Kind==0?14000:3500,Kind==0?18000:6000);
  const FBox B=Mesh->GetBoundingBox();const double Width=2*FMath::Max(B.GetExtent().X,B.GetExtent().Y);
  TArray<FTransform> Batch;
  for(const FStone& S:Plan.Stones) if(S.Kind==Kind&&S.Variant==Variant)
  {
   FVector Scale(S.Diameter/FMath::Max(Width,1.0));
   if(Kind==0){Scale.X*=1.25;Scale.Y*=0.75;Scale.Z*=0.85;}
   FVector P=S.Foot;P.Z-=B.Min.Z*Scale.Z+S.Sink*B.GetSize().Z*Scale.Z;
   Batch.Add(FTransform(FRotator(0,S.Yaw,0),P,Scale));++Counts[Kind];
  }
  C->AddInstances(Batch,false,true,false);C->MarkRenderStateDirty();
 }
 UE_LOG(LogAnastasis_UnrealV2,Display,TEXT("SOIL_CONTACT anchor=(%.1f,%.1f,%.1f) uphill=(%.4f,%.4f) outcrop=%d fragments=%d gravel=%d missing=%d"),
  Plan.Anchor.X,Plan.Anchor.Y,Plan.Anchor.Z,Plan.Uphill.X,Plan.Uphill.Y,Counts[0],Counts[1],Counts[2],Missing);
}
}
