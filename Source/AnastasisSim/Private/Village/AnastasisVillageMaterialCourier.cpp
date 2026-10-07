#include "Village/AnastasisVillage.h"

#include "Core/AnastasisSimMath.h"
#include "World/AnastasisPathfinding.h"
#include "World/AnastasisWorld.h"

namespace AnastasisVillage
{
	using AnastasisMath::Dist;
	// Extension opt-in ecart n°18 : une seule charge physique, retiree de la tuile
	// au ramassage et creditee au site seulement a l'arrivee. Aucun stock n'est cree.
	bool FVillage::ProgressMaterialCourier(FNpc& Npc, double Dt)
	{
		using AnastasisWorld::EResource;
		if (!World || Now < Npc.MaterialRetryAt) return false;
		FBuilding* Site = nullptr;
		for (FBuilding& Candidate : Buildings.GetItemsMutable())
		{
			if (Candidate.Progress >= 1.0) continue;
			const auto& M = Candidate.Materials;
			const int32 WoodMissing = M.NeedWood - M.ConsumedWood - M.StockWood;
			const int32 StoneMissing = M.NeedStone - M.ConsumedStone - M.StockStone;
			if (Npc.MaterialCarry > 0 &&
				((Npc.MaterialResource == EResource::Wood && WoodMissing <= 0)
					|| (Npc.MaterialResource == EResource::Stone && StoneMissing <= 0))) continue;
			if (WoodMissing > 0 || StoneMissing > 0) { Site = &Candidate; break; }
		}
		if (!Site) return false; // La charge reste dans l'inventaire si le chantier disparait.
		FPoint Door{ Site->X + 0.5, Site->Y + 0.5 };
		if (!BuildingAccessPoint(*Site, &Npc, Door)) return false;
		const AnastasisPath::FWorldNavSource NavSource(Nav, *World);
		if (Npc.MaterialCarry > 0)
		{
			Npc.Goal = TEXT("deliverMaterials");
			Npc.Activity = TEXT("porte materiaux");
			Npc.DestBuildingId = Site->Id;
			Npc.Target = Door;
			Npc.bHasTarget = true;
			if (Dist(Npc.X, Npc.Y, Door.X, Door.Y) > ArrivalDistance)
			{
				MoveActor(Npc, Door, Dt);
				if (Npc.bPathFailed)
				{
					Npc.MaterialRetryAt = Now + 8.0;
					Npc.bHasTarget = false;
					ClearNavigation(Npc);
				}
				return true;
			}
			const int32 Wood = Npc.MaterialResource == EResource::Wood ? FMath::Min(Npc.MaterialCarry,
				Site->Materials.NeedWood - Site->Materials.ConsumedWood - Site->Materials.StockWood) : 0;
			const int32 Stone = Npc.MaterialResource == EResource::Stone ? FMath::Min(Npc.MaterialCarry,
				Site->Materials.NeedStone - Site->Materials.ConsumedStone - Site->Materials.StockStone) : 0;
			const int32 Credited = CreditSiteMaterials(Site->Id, Wood, Stone);
			Npc.MaterialCarry -= Credited;
			Npc.MaterialsDelivered += Credited;
			if (Npc.MaterialCarry == 0)
			{
				Npc.MaterialResource = EResource::None;
				Npc.MaterialSourceIndex = INDEX_NONE;
				Npc.bHasTarget = false;
				ClearNavigation(Npc);
			}
			Npc.Activity = TEXT("livre materiaux");
			return true;
		}

		const auto& M = Site->Materials;
		const int32 WoodMissing = M.NeedWood - M.ConsumedWood - M.StockWood;
		const int32 StoneMissing = M.NeedStone - M.ConsumedStone - M.StockStone;
		const EResource Wanted = WoodMissing > 0 ? EResource::Wood : EResource::Stone;
		const int32 Deficit = Wanted == EResource::Wood ? WoodMissing : StoneMissing;
		if (Deficit <= 0) return false;
		if (Npc.MaterialResource != Wanted)
		{
			Npc.MaterialResource = Wanted;
			Npc.MaterialSourceIndex = INDEX_NONE;
		}
		if (Npc.MaterialSourceIndex == INDEX_NONE)
		{
			// Selection a l'ouverture du trajet seulement : aucun balayage du monde par frame.
			const int32 CX = FMath::FloorToInt32(Npc.X), CY = FMath::FloorToInt32(Npc.Y);
			for (int32 R = 0; R <= 24 && Npc.MaterialSourceIndex == INDEX_NONE; ++R)
			for (int32 DY = -R; DY <= R && Npc.MaterialSourceIndex == INDEX_NONE; ++DY)
			for (int32 DX = -R; DX <= R; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
				const int32 X = CX + DX, Y = CY + DY;
				if (X < 0 || Y < 0 || X >= World->W || Y >= World->H) continue;
				const int32 Index = Y * World->W + X;
				const AnastasisWorld::FTile Tile = LiveTile(Index);
				if (Tile.Resource != Wanted || Tile.Amount <= 0 || IsFootBlocked(X + 0.5, Y + 0.5)) continue;
				TArray<FPoint> Path;
				if (AnastasisPath::FindPath(NavSource, { Npc.X, Npc.Y }, { X + 0.5, Y + 0.5 }, {}, Path))
				{
					Npc.MaterialSourceIndex = Index;
					break;
				}
			}
			if (Npc.MaterialSourceIndex == INDEX_NONE)
			{
				Npc.MaterialRetryAt = Now + 8.0;
				return false;
			}
		}
		const int32 Index = Npc.MaterialSourceIndex;
		const AnastasisWorld::FTile Tile = LiveTile(Index);
		if (Tile.Resource != Wanted || Tile.Amount <= 0)
		{
			Npc.MaterialSourceIndex = INDEX_NONE;
			return true;
		}
		const FPoint Source{ Tile.X + 0.5, Tile.Y + 0.5 };
		Npc.Goal = Wanted == EResource::Wood ? TEXT("gatherWood") : TEXT("gatherStone");
		Npc.Activity = Wanted == EResource::Wood ? TEXT("va au bois") : TEXT("va a la pierre");
		Npc.DestBuildingId.Reset();
		Npc.Target = Source;
		Npc.bHasTarget = true;
		if (Dist(Npc.X, Npc.Y, Source.X, Source.Y) > ArrivalDistance)
		{
			MoveActor(Npc, Source, Dt);
			if (Npc.bPathFailed)
			{
				Npc.MaterialSourceIndex = INDEX_NONE;
				Npc.MaterialRetryAt = Now + 8.0;
				Npc.bHasTarget = false;
				ClearNavigation(Npc);
			}
			return true;
		}
		const int32 Taken = FMath::Min3(4, Tile.Amount, Deficit);
		if (Taken <= 0) return true;
		TakeFromTile(Index, Taken);
		if (LiveTile(Index).Amount <= 0) DepleteTile(Index);
		Npc.MaterialCarry = Taken;
		Npc.Activity = Wanted == EResource::Wood ? TEXT("coupe bois") : TEXT("tire pierre");
		Npc.bHasTarget = false;
		ClearNavigation(Npc);
		return true;
	}
}
