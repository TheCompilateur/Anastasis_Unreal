#include "Sim/AnastasisTerrainAccessProbe.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "WorldView/AnastasisSettlementSurvey.h"
#include "World/AnastasisPathfinding.h"
#include "Village/AnastasisVillagerVisual.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
using namespace AnastasisVillage;
UAnastasisSimulationSubsystem* AccessHost(const UObject* C)
{
 auto* W=GEngine ? GEngine->GetWorldFromContextObject(C,EGetWorldErrorMode::ReturnNull) : nullptr;
 return W && W->WorldType==EWorldType::PIE ? W->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
}
FString EncodeAccess(const TSharedRef<FJsonObject>& O)
{
 FString S; FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S)); return S;
}
FString AccessError(const FString& E)
{
 auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("error"),E); return EncodeAccess(O);
}
TSharedPtr<FJsonValue> Point(double X,double Y)
{
 auto O=MakeShared<FJsonObject>(); O->SetNumberField(TEXT("x"),X); O->SetNumberField(TEXT("y"),Y);
 return MakeShared<FJsonValueObject>(O);
}
TSharedPtr<FJsonValue> Route(const TCHAR* Name,const FPoint& Start,const FPoint& End,const AnastasisPath::FWorldNavSource& Nav)
{
 auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("name"),Name);
 O->SetField(TEXT("start"),Point(Start.X,Start.Y)); O->SetField(TEXT("end"),Point(End.X,End.Y));
 TArray<FPoint> P; const bool Found=AnastasisPath::FindPath(Nav,Start,End,{},P);
 O->SetBoolField(TEXT("found"),Found); O->SetStringField(TEXT("scope"),TEXT("planned_semantic_path_default_search_budget"));
 TArray<TSharedPtr<FJsonValue>> Points;
 if(Found) { Points.Add(Point(Start.X,Start.Y)); for(const auto& Q:P) Points.Add(Point(Q.X,Q.Y)); Points.Add(Point(End.X,End.Y)); }
 O->SetArrayField(TEXT("points"),Points); return MakeShared<FJsonValueObject>(O);
}
}
FString UAnastasisTerrainAccessProbe::Begin(const UObject* C)
{
 auto* H=AccessHost(C); if(!H || !H->GetSimulation().IsRunning()) return AccessError(TEXT("not_running_PIE"));
 auto& S=H->GetSimulation(); auto& V=S.GetVillage(); const auto& W=S.GetWorld();
 if(!V.GetActors().IsEmpty() || !V.GetBuildings().IsEmpty()) return AccessError(TEXT("requires_empty_village"));
 AnastasisSettlementSite::FInputs In; FString Why;
 if(!AnastasisSettlementSurvey::Read(H->GetWorld(),S.GetSeed(),W,V,In,Why)) return AccessError(Why);
 const auto Site=AnastasisSettlementSite::Choose(In);
 if(!Site.Best.bEligible) return AccessError(TEXT("no_eligible_site"));
 const FString Granary=H->SeedFirstFarmer(1,Site.Best.Index%W.W,Site.Best.Index/W.W);
 if(Granary.IsEmpty() || V.GetActors().Num()!=1) return AccessError(TEXT("farmer_fixture_failed"));
 const FString Npc=V.GetActors()[0].Id;
 const FIntPoint Field=H->GetFarmerField();
 const FPoint Food={Field.X+0.5,Field.Y+0.5};
 const auto* Depot=V.FindBuilding(Granary);
 if(!Depot || Depot->AccessPoints.IsEmpty()) return AccessError(TEXT("granary_door_missing"));
 const FPoint DepotDoor=Depot->AccessPoints[0];
 FString House; FPoint Home={};
 // Deterministic fixture near the real generated field. No slope filtering: this is an audit.
 for(int32 R=3;R<=8 && House.IsEmpty();++R) for(int32 DY=-R;DY<=R && House.IsEmpty();++DY) for(int32 DX=-R;DX<=R && House.IsEmpty();++DX)
 {
  if(FMath::Max(FMath::Abs(DX),FMath::Abs(DY))!=R) continue;
  const int32 X=Field.X+DX,Y=Field.Y+DY;
  if(X<2||Y<2||X>=W.W-2||Y>=W.H-2||V.LiveTileAt(X,Y).Resource!=AnastasisWorld::EResource::None||V.IsFootBlocked(X+.5,Y+.5)) continue;
  const FString Id=V.AddBuilding(HouseType,X,Y);
  if(Id.IsEmpty()) continue;
  const AnastasisPath::FWorldNavSource Nav(V.GetNavGrid(),W);
  for(const auto& Door:V.FindBuilding(Id)->AccessPoints)
  {
   TArray<FPoint> P;
   if(AnastasisPath::FindPath(Nav,Door,Food,{},P)) { House=Id; Home=Door; break; }
  }
  if(House.IsEmpty()) V.RemoveBuilding(Id);
 }
 if(House.IsEmpty() || !V.AssignHome(Npc,House)) return AccessError(TEXT("home_fixture_failed"));
 const AnastasisPath::FWorldNavSource Nav(V.GetNavGrid(),W);
 TArray<int32> Drink;
 for(int32 I=0;I<W.Tiles.Num();++I)
  if(!V.IsFootBlocked(I%W.W+.5,I/W.W+.5) && V.AtDrinkSpot(I%W.W+.5,I/W.W+.5)) Drink.Add(I);
 Drink.StableSort([&](int32 A,int32 B){return FMath::Square(A%W.W+.5-Home.X)+FMath::Square(A/W.W+.5-Home.Y)<FMath::Square(B%W.W+.5-Home.X)+FMath::Square(B/W.W+.5-Home.Y);});
 auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("world"),H->GetWorld()->GetPathName());
 O->SetNumberField(TEXT("seed"),S.GetSeed()); O->SetNumberField(TEXT("tile_m"),In.TileMetres);
 O->SetStringField(TEXT("npc"),Npc); O->SetStringField(TEXT("house"),House); O->SetStringField(TEXT("granary"),Granary);
 TArray<TSharedPtr<FJsonValue>> Routes;
 bool WaterFound=false;
 for(int32 I:Drink)
 {
  const FPoint End={I%W.W+.5,I/W.W+.5}; TArray<FPoint> P;
  if(AnastasisPath::FindPath(Nav,Home,End,{},P)) { Routes.Add(Route(TEXT("home_water"),Home,End,Nav)); WaterFound=true; break; }
 }
 if(!WaterFound) { auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("name"),TEXT("home_water")); R->SetBoolField(TEXT("found"),false); Routes.Add(MakeShared<FJsonValueObject>(R)); }
 Routes.Add(Route(TEXT("home_field"),Home,Food,Nav));
 Routes.Add(Route(TEXT("field_granary"),Food,DepotDoor,Nav));
 O->SetArrayField(TEXT("routes"),Routes); H->SyncVillagePresentation(); return EncodeAccess(O);
}
FString UAnastasisTerrainAccessProbe::Read(const UObject* C)
{
 auto* H=AccessHost(C); if(!H) return AccessError(TEXT("not_PIE"));
 const auto& S=H->GetSimulation(); const auto& V=S.GetVillage();
 auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("world"),H->GetWorld()->GetPathName()); O->SetNumberField(TEXT("time"),S.GetTime());
 TArray<TSharedPtr<FJsonValue>> Actors;
 for(const auto& N:V.GetActors())
 {
  auto A=MakeShared<FJsonObject>(); A->SetStringField(TEXT("id"),N.Id); A->SetField(TEXT("position"),Point(N.X,N.Y));
  A->SetStringField(TEXT("goal"),N.Goal); A->SetBoolField(TEXT("inside"),N.Inside.bActive);
  A->SetNumberField(TEXT("gathered"),N.GatheredFood); A->SetNumberField(TEXT("delivered"),N.DeliveredFood); A->SetNumberField(TEXT("drinks"),N.DrinksTaken);
  A->SetBoolField(TEXT("path_failed"),N.bPathFailed); A->SetBoolField(TEXT("blocked"),V.IsFootBlocked(N.X,N.Y));
  if(const auto* Visual=H->GetVillagePresentation().FindVillager(N.Id))
  {
   const auto P=Visual->GetActorLocation(); A->SetField(TEXT("visual_xy_cm"),Point(P.X,P.Y)); A->SetNumberField(TEXT("visual_z_cm"),P.Z);
   A->SetBoolField(TEXT("visual_hidden"),Visual->IsHidden());
  }
  Actors.Add(MakeShared<FJsonValueObject>(A));
 }
 O->SetArrayField(TEXT("actors"),Actors); return EncodeAccess(O);
}
