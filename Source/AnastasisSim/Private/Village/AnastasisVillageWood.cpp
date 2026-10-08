#include "Village/AnastasisVillage.h"
#include "Work/AnastasisWoodHarvest.h"
#include "World/AnastasisWorld.h"
#include "Life/AnastasisWorkShift.h"
#include "Core/AnastasisSimMath.h"

namespace AnastasisVillage
{
namespace
{
int32 Available(const AnastasisWorld::FTile& Tile)
{
    if (Tile.Resource != AnastasisWorld::EResource::Wood) return 0;
    // ecart n°30: FTile has no crown/clearing or colonization frontier authority.
    // JS defaults for absent crown/clearing; no invented frontier exemption.
    return AnastasisWoodHarvest::Take(Tile.Amount,
        AnastasisWoodHarvest::Floor(Tile.Type == AnastasisWorld::ETileType::Forest, 0, 0, false), Tile.Amount);
}
}

// ecart n°30: woodcutters only, and never the material courier, which keeps its own
// cargo and its own site credit; the two flows never share an NPC.
bool FVillage::IsWoodHarvester(const FNpc& Npc) const
{
    return Npc.JobId == TEXT("woodcutter") && Npc.Id != MaterialCourierId;
}

bool FVillage::WoodTarget(const FNpc& Npc, FPoint& Out, FString& Source) const
{
    if (AnastasisGather::ShouldHaulGatherLoad(Npc.InventoryWood + Npc.InventoryFood)) return false;
    double Best = -1.e30;
    bool Found = false;
    for (const FResourceSpot& Spot : Npc.Spots)
    {
        if (Spot.Resource != TEXT("wood")) continue;
        const int32 X = FMath::FloorToInt32(Spot.X), Y = FMath::FloorToInt32(Spot.Y);
        if (Available(LiveTileAt(X, Y)) <= 0) continue;
        // ecart n°30: recallResource subset; no frontier doctrine or danger map.
        const double Score = -FMath::Sqrt(FMath::Square(Npc.X-Spot.X)+FMath::Square(Npc.Y-Spot.Y))
            - (Day()-Spot.Day)*1.5 - (Spot.bHearsay ? AnastasisGather::HearsayPenalty : 0.0);
        if (Score > Best) { Best=Score; Out={Spot.X,Spot.Y}; Found=true; }
    }
    Source=Found ? TEXT("wood_spot") : TEXT("none");
    return Found;
}

// ecart n°43: bridge the existing woodcutter cargo to the existing construction
// stock. The site remains the authority for material consumption.
bool FVillage::WoodDeliveryTarget(FNpc& Npc, FPoint& Out, FString& Source)
{
    if (!IsWoodHarvester(Npc) || Npc.InventoryWood <= 0) return false;
    for (FBuilding& Site : Buildings.GetItemsMutable())
    {
        if (Site.IsCompleted() || !Site.bHasMaterials) continue;
        const int32 Missing = Site.Materials.NeedWood - Site.Materials.ConsumedWood - Site.Materials.StockWood;
        if (Missing <= 0 || !BuildingAccessPoint(Site, &Npc, Out)) continue;
        Npc.DestBuildingId = Site.Id;
        Source = TEXT("wood_site");
        return true;
    }
    return false;
}

bool FVillage::DeliverWoodToSite(FNpc& Npc)
{
    if (!IsWoodHarvester(Npc) || Npc.InventoryWood <= 0 || Npc.DestBuildingId.IsEmpty()) return false;
    FBuilding* Site = Buildings.FindById(Npc.DestBuildingId);
    if (!Site || Site->IsCompleted() || !Site->bHasMaterials) return false;
    const int32 Missing = Site->Materials.NeedWood - Site->Materials.ConsumedWood - Site->Materials.StockWood;
    if (Missing <= 0 || !Npc.bHasTarget ||
        AnastasisMath::Dist(Npc.X, Npc.Y, Npc.Target.X, Npc.Target.Y) > ArrivalDistance) return false;
    const int32 Credited = CreditSiteMaterials(Site->Id, FMath::Min(Npc.InventoryWood, Missing), 0);
    if (Credited <= 0) return false;
    Npc.InventoryWood -= Credited;
    Npc.MaterialsDelivered += Credited;
    ++Npc.Deliveries;
    Npc.Activity = TEXT("livre bois");
    return true;
}

double FVillage::WoodRowScore(const FNpc& Npc, double PhaseBias, const FWorkRowContext& Work, double Noise) const
{
    FPoint Target; FString Source;
    if (!WoodTarget(Npc, Target, Source)) return -1000.0; // ecart n°30: no haul destination yet.
    namespace G = AnastasisGather;
    const double Gap = FMath::Max(0.0, 70.0-40.0*G::PresumedNoise(Npc.Id,TEXT("wood")));
    // ecart n°30: resourceScore subset; construction/colony pressure and density brake deferred.
    const double Base = Gap*G::TraitAt(Npc.TraitIndex).Gather*1.35 + Gap*0.32 + 18.0 + G::JobPriority(Npc.JobId,TEXT("gatherWood")) + Noise;
    return Base*G::SurvivalWorkFactor(TEXT("gatherWood"),Work.WorkFactor,Work.bMealBlocked)
        + PhaseBias + G::TraitGoalBias(G::TraitAt(Npc.TraitIndex),TEXT("gatherWood"))
        + G::SkillGoalBias(Npc.SkillGather)
        + G::CompletionBias(TEXT("gatherWood"),Npc.InventoryWood+Npc.InventoryFood,0,SessionGoalOf(Npc),NeedsCritical(Npc.Needs))
        + EpisodeGoalBiasOf(Npc,TEXT("gatherWood")); // ecart n°47
}

int32 FVillage::ProgressWoodGather(FNpc& Npc)
{
    namespace W = AnastasisWoodHarvest;
    namespace G = AnastasisGather;
    if (!World || Npc.Inside.bActive || G::ShouldHaulGatherLoad(Npc.InventoryWood+Npc.InventoryFood)) return 2;
    int32 Index=INDEX_NONE;
    const int32 CX=FMath::FloorToInt32(Npc.X), CY=FMath::FloorToInt32(Npc.Y);
    // Only an adjacent, currently harvestable tile. Never harvest a remote session anchor.
    auto Accept=[&](int32 X,int32 Y) {
        if (X<0 || Y<0 || X>=World->W || Y>=World->H || FMath::Abs(X-CX)>1 || FMath::Abs(Y-CY)>1) return false;
        const int32 I=Y*World->W+X;
        if (Available(LiveTile(I))<=0) return false;
        Index=I; return true;
    };
    if (Npc.WorkSession.bActive && Npc.WorkSession.CraftId==TEXT("chop")) Accept(Npc.WorkSession.TileX,Npc.WorkSession.TileY);
    for (int32 DY=-1; DY<=1 && Index==INDEX_NONE; ++DY)
        for (int32 DX=-1; DX<=1 && Index==INDEX_NONE; ++DX) Accept(CX+DX,CY+DY);
    if (Index==INDEX_NONE) return 0;
    const auto Tile=LiveTile(Index);
    auto& S=Npc.WorkSession;
    if (!S.bActive || S.CraftId!=TEXT("chop") || S.TileX!=Tile.X || S.TileY!=Tile.Y)
    {
        // ecart n°30: tool switching, craft misses and techniques deferred, as for farm.
        S=FWorkSession(); S.bActive=true; S.CraftId=TEXT("chop"); S.TileX=Tile.X; S.TileY=Tile.Y;
        S.ArrivedAt=Now; S.NextSwingAt=Now+0.42;
        AnastasisWorkShift::NoteShiftArrival(Npc.WorkShift);
    }
    Npc.Target={Tile.X+0.5,Tile.Y+0.5}; Npc.bHasTarget=true; Npc.Activity=TEXT("bucher");
    if (Now<S.NextSwingAt) return 1;
    const int32 Taken=FMath::Min(Available(Tile),W::Yield(Npc.Skill));
    if (Taken<=0) return 0;
    TakeFromTile(Index,Taken);
    Npc.InventoryWood+=Taken; Npc.GatheredWood+=Taken;
    G::GainDomainSkill(Npc.Skill,Npc.SkillGather,G::GatherSkillGain);
    ++S.SwingsDone; S.LastSwingAt=Now; S.NextSwingAt=Now+W::Period(Npc.Skill,S.SwingsDone,Npc.Needs.Energy);
    if (LiveTile(Index).Amount<=0) DepleteTile(Index); // forest floor prevents forest exhaustion.
    // ecart n°30: retain cargo; never credit a depot/site without an actual delivery.
    return G::ShouldHaulGatherLoad(Npc.InventoryWood+Npc.InventoryFood) || Available(LiveTile(Index))<=0 ? 2 : 1;
}
}
