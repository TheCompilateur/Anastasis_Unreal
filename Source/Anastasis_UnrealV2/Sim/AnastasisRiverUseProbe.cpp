#include "Sim/AnastasisRiverUseProbe.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "WorldView/AnastasisSettlementSurvey.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
UAnastasisSimulationSubsystem* HostFor(const UObject* Object)
{
    UWorld* W=GEngine ? GEngine->GetWorldFromContextObject(Object,EGetWorldErrorMode::ReturnNull) : nullptr;
    return W && W->WorldType==EWorldType::PIE ? W->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
}
FString Encode(const TSharedRef<FJsonObject>& O)
{
    FString Out; FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Out)); return Out;
}
FString RiverProbeError(const FString& Why)
{
    auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("error"),Why); return Encode(O);
}
}

FString UAnastasisRiverUseProbe::Describe(const UObject* Object)
{
    auto* H=HostFor(Object);
    if(!H || !H->GetSimulation().IsRunning()) return RiverProbeError(TEXT("not_running_PIE"));
    const auto& S=H->GetSimulation(); const auto& V=S.GetVillage(); const auto& W=S.GetWorld();
    AnastasisSettlementSite::FInputs In; FString Why;
    if(!AnastasisSettlementSurvey::Read(H->GetWorld(),S.GetSeed(),W,V,In,Why)) return RiverProbeError(Why);
    auto O=MakeShared<FJsonObject>();
    O->SetNumberField(TEXT("seed"),S.GetSeed()); O->SetNumberField(TEXT("width"),W.W); O->SetNumberField(TEXT("height"),W.H);
    O->SetNumberField(TEXT("tile_m"),In.TileMetres); O->SetStringField(TEXT("world"),H->GetWorld()->GetPathName());
    TSharedPtr<FJsonObject> Site;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(AnastasisSettlementSite::ToJson(AnastasisSettlementSite::Choose(In),In)),Site);
    if(!Site) return RiverProbeError(TEXT("invalid_site_report"));
    O->SetObjectField(TEXT("site"),Site);
    TArray<TSharedPtr<FJsonValue>> Cells;
    for(int32 I=0;I<W.Tiles.Num();++I)
    {
        auto C=MakeShared<FJsonObject>();
        C->SetBoolField(TEXT("walkable"),In.Cells[I].bWalkable);
        C->SetBoolField(TEXT("drinkable"),V.AtDrinkSpot(I%W.W+0.5,I/W.W+0.5));
        C->SetBoolField(TEXT("semantic_water"),W.Tiles[I].Type==AnastasisWorld::ETileType::Water);
        C->SetNumberField(TEXT("shore"),W.Tiles[I].Shore);
        Cells.Add(MakeShared<FJsonValueObject>(C));
    }
    O->SetArrayField(TEXT("cells"),Cells); return Encode(O);
}

FString UAnastasisRiverUseProbe::Begin(const UObject* Object,double X,double Y)
{
    auto* H=HostFor(Object);
    if(!H || !H->GetSimulation().IsRunning()) return RiverProbeError(TEXT("not_running_PIE"));
    auto& V=H->GetSimulation().GetVillage(); const auto& Nav=V.GetNavGrid();
    if(!V.GetActors().IsEmpty() || !V.GetBuildings().IsEmpty()) return RiverProbeError(TEXT("requires_empty_village"));
    if(!FMath::IsFinite(X) || !FMath::IsFinite(Y) || X<0 || Y<0 || X>=Nav.W || Y>=Nav.H || V.IsFootBlocked(X,Y))
        return RiverProbeError(TEXT("invalid_spawn"));
    if(V.AtDrinkSpot(X,Y)) return RiverProbeError(TEXT("spawn_already_drinkable"));
    AnastasisNeeds::FNeeds Needs;
    Needs.Thirst=80; Needs.Hunger=10; Needs.Energy=80; Needs.Social=70;
    Needs.Leisure=70; Needs.Hygiene=60; Needs.Health=90; Needs.Morale=55;
    const FString Id=V.SpawnNpc(X,Y,Needs,4.0);
    H->SyncVillagePresentation();
    return Read(Object,Id);
}

FString UAnastasisRiverUseProbe::Read(const UObject* Object,const FString& Id)
{
    auto* H=HostFor(Object);
    if(!H) return RiverProbeError(TEXT("not_PIE"));
    const auto& S=H->GetSimulation(); const auto& V=S.GetVillage(); const auto* N=V.FindNpc(Id);
    if(!N) return RiverProbeError(TEXT("npc_missing"));
    auto O=MakeShared<FJsonObject>();
    O->SetStringField(TEXT("npc"),Id); O->SetStringField(TEXT("world"),H->GetWorld()->GetPathName());
    O->SetNumberField(TEXT("time"),S.GetTime()); O->SetNumberField(TEXT("x"),N->X); O->SetNumberField(TEXT("y"),N->Y);
    O->SetStringField(TEXT("goal"),N->Goal); O->SetStringField(TEXT("activity"),N->Activity);
    O->SetStringField(TEXT("source"),N->LastDecision.TargetSource); O->SetNumberField(TEXT("decision_time"),N->LastDecision.Time);
    O->SetBoolField(TEXT("has_target"),N->bHasTarget);
    if(N->bHasTarget) { O->SetNumberField(TEXT("target_x"),N->Target.X); O->SetNumberField(TEXT("target_y"),N->Target.Y); }
    O->SetNumberField(TEXT("drinks"),N->DrinksTaken); O->SetNumberField(TEXT("thirst"),N->Needs.Thirst);
    O->SetBoolField(TEXT("at_drink_spot"),V.AtDrinkSpot(N->X,N->Y)); O->SetBoolField(TEXT("path_failed"),N->bPathFailed);
    O->SetBoolField(TEXT("foot_blocked"),V.IsFootBlocked(N->X,N->Y));
    O->SetNumberField(TEXT("path_step"),N->PathStep); O->SetNumberField(TEXT("stuck_stage"),N->StuckStage);
    O->SetNumberField(TEXT("npc_count"),V.GetActors().Num()); O->SetNumberField(TEXT("building_count"),V.GetBuildings().Num());
    O->SetBoolField(TEXT("autonomous"),!V.IsPlayer(*N));
    TArray<TSharedPtr<FJsonValue>> Path;
    for(const auto& P:N->Path) { auto Q=MakeShared<FJsonObject>(); Q->SetNumberField(TEXT("x"),P.X); Q->SetNumberField(TEXT("y"),P.Y); Path.Add(MakeShared<FJsonValueObject>(Q)); }
    O->SetArrayField(TEXT("planned_path"),Path); return Encode(O);
}
