#include "Village/AnastasisVillage.h"
#include "Core/AnastasisSimMath.h"
#include "World/AnastasisWorld.h"

namespace AnastasisVillage
{
	bool FVillage::ActivateFoodSource(int32 X, int32 Y)
	{
		if (!World || !Nav.IsInBounds(X, Y)) return false;
		const int32 Index = Y * World->W + X;
		if (FoodSources.ContainsByPredicate([&](const FFoodSource& S) { return S.TileIndex == Index; })) return true;
		const AnastasisWorld::FTile& T = World->Tiles[Index];
		if (T.Resource != AnastasisWorld::EResource::Food || T.Amount <= 0 || IsFootBlocked(X + 0.5, Y + 0.5)) return false;
		FoodSources.Add({Index, {X + 0.5, Y + 0.5}, T.Amount, T.Amount});
		for (FNpc& N : Actors.GetItemsMutable()) Perceive(N, true);
		return true;
	}

	bool FVillage::HasKnownFoodSource(const FNpc& N) const
	{
		for (const FFoodSource& S : FoodSources)
		{
			const int32* Amount = N.KnownFoodSources.Find(S.TileIndex);
			if (Amount && *Amount > 0) return true;
		}
		return false;
	}

	const FBuilding* FVillage::KnownFoodDepot(const FNpc& N) const
	{
		const FBuilding* Best = nullptr;
		double Distance = TNumericLimits<double>::Max();
		for (const FStockBelief& K : N.KnownStocks)
		{
			const FBuilding* B = Buildings.FindById(K.BuildingId);
			if (!B || B->Type != GranaryType || !B->IsCompleted() || K.EstimatedAmount >= GranaryFoodCap) continue;
			const double D = AnastasisMath::Dist(N.X, N.Y, B->X + 0.5, B->Y + 0.5);
			if (D < Distance) { Best = B; Distance = D; }
		}
		return Best;
	}

	bool FVillage::FoodSupplyTarget(FNpc& N, FPoint& Out, FString& Source)
	{
		const AnastasisPath::FWorldNavSource NavSource(Nav, *World);
		TArray<FPoint> Path;
		if (N.Goal == TEXT("deliver"))
		{
			const FBuilding* Depot = KnownFoodDepot(N);
			if (N.InventoryFood <= 0 || !Depot || !BuildingAccessPointById(Depot->Id, &N, Out)) return false;
			if (!AnastasisPath::FindPath(NavSource, {N.X, N.Y}, Out, {}, Path)) return false;
			N.DestBuildingId = Depot->Id;
			Source = TEXT("food_depot");
			return true;
		}
		N.FoodSourceIndex = INDEX_NONE;
		double Best = TNumericLimits<double>::Max();
		for (const FFoodSource& S : FoodSources)
		{
			const int32* Known = N.KnownFoodSources.Find(S.TileIndex);
			if (!Known || *Known <= 0 || N.InventoryFood > 0) continue;
			const double D = AnastasisMath::Dist(N.X, N.Y, S.Position.X, S.Position.Y);
			if (D >= Best || !AnastasisPath::FindPath(NavSource, {N.X, N.Y}, S.Position, {}, Path)) continue;
			Best = D;
			Out = S.Position;
			N.FoodSourceIndex = S.TileIndex;
		}
		Source = TEXT("food_patch");
		return N.FoodSourceIndex != INDEX_NONE;
	}

	bool FVillage::PerformFoodSupply(FNpc& N)
	{
		if (N.Goal == TEXT("gatherFood"))
		{
			FFoodSource* S = FoodSources.FindByPredicate([&](const FFoodSource& Item) { return Item.TileIndex == N.FoodSourceIndex; });
			if (!S || AnastasisMath::Dist(N.X, N.Y, S->Position.X, S->Position.Y) > ArrivalDistance || IsFootBlocked(S->Position.X, S->Position.Y)) return false;
			// Bounded hand gathering: reference gather() base yield 2, no skill progression.
			const int32 Take = FMath::Min(S->Remaining, FMath::Max(0, 2 - N.InventoryFood));
			S->Remaining -= Take;
			N.InventoryFood += Take;
			N.GatheredFood += Take;
			N.KnownFoodSources.Add(S->TileIndex, S->Remaining);
			N.Activity = S->Remaining > 0 ? TEXT("cueille") : TEXT("source epuisee");
			// End work commitment when carrying; Noûs can still choose an urgent meal.
			N.bHasAlgoDecision = false;
			N.bAlgoInertiaKeep = false;
			N.AiThinkAt = Now;
			return Take > 0;
		}
		FBuilding* Depot = Buildings.FindById(N.DestBuildingId);
		if (!Depot || Depot->Type != GranaryType || !Depot->IsCompleted() || !N.bHasTarget || !ReachedMoveTarget(N, N.Target)) return false;
		// Atomic transfer: the accepted amount alone leaves the bag (full depot keeps cargo).
		const int32 Moved = CreditFood(Depot->Id, FMath::Min(8, N.InventoryFood));
		N.InventoryFood -= Moved;
		N.DeliveredFood += Moved;
		N.Activity = Moved > 0 ? TEXT("depose") : TEXT("grenier plein");
		Perceive(N, true);
		N.bHasAlgoDecision = false;
		N.bAlgoInertiaKeep = false;
		N.AiThinkAt = Now;
		return Moved > 0;
	}
}
