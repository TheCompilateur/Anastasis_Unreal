// mains-joueur-001 -- les mains du joueur (ecart n°57).
//
// Demande d'Alexandre (2026-10-09) : « on a donne son corps, mais pas la capacite de ses mains ». Le joueur
// vise un batiment ou une tuile et maintient le clic : le geste se fait sur place. Pas de but, pas de trajet,
// pas de decision de Nous : c'est la main. Chaque geste reprend la regle de l'habitant qui fait la meme
// chose -- la buche du bucheron (`ProgressWoodGather`), la pierre du porteur (`ProgressMaterialCourier`), la
// portion de la tuile (`TakeFromTile`, `DepleteTile`), le grenier (`CreditFood`, `FoodAvailable`), le
// chantier (`CreditSiteMaterials`, `WorkConstruction`, `CanBuildAt`), boire et manger (`SatisfyDrink`,
// `SatisfyEat`) -- pour que l'humain et l'IA partagent les memes verbes. La reference n'a pas de geste
// direct : le joueur n'y agit qu'en posant un but (ecart n°20).
//
// Le sac est celui du corps : `InventoryFood`, `InventoryWood`, et la pierre dans `MaterialCarry`
// (`MaterialResource == Stone`). Aucun champ nouveau, aucun changement du format de sauvegarde.

#include "Village/AnastasisVillage.h"

#include "Core/AnastasisSimMath.h"
#include "Life/AnastasisNeeds.h"
#include "Work/AnastasisBuild.h"
#include "Work/AnastasisWoodHarvest.h"
#include "World/AnastasisWorld.h"

namespace AnastasisVillage
{
	namespace
	{
		using AnastasisWorld::EResource;

		/** Le bois qu'une tuile cede encore, plancher forestier compris (meme regle que `Available` du bucheron). */
		int32 WoodLeft(const AnastasisWorld::FTile& Tile)
		{
			if (Tile.Resource != EResource::Wood) return 0;
			return AnastasisWoodHarvest::Take(Tile.Amount,
				AnastasisWoodHarvest::Floor(Tile.Type == AnastasisWorld::ETileType::Forest, 0, 0, false), Tile.Amount);
		}

	}

	const TCHAR* FVillage::PlayerHandLabel(EPlayerHand Hand)
	{
		switch (Hand)
		{
		case EPlayerHand::Drink: return TEXT("boire");
		case EPlayerHand::Gather: return TEXT("recolter");
		case EPlayerHand::Chop: return TEXT("couper du bois");
		case EPlayerHand::Quarry: return TEXT("tailler une pierre");
		case EPlayerHand::Deposit: return TEXT("deposer la nourriture");
		case EPlayerHand::Take: return TEXT("prendre une portion");
		case EPlayerHand::Supply: return TEXT("donner bois et pierre");
		case EPlayerHand::Build: return TEXT("batir");
		case EPlayerHand::Eat: return TEXT("manger");
		default: return TEXT("");
		}
	}

	int32 FVillage::PlayerStone() const
	{
		const FNpc* P = PlayerActor();
		return P && P->MaterialResource == EResource::Stone ? P->MaterialCarry : 0;
	}

	FVillage::EPlayerHand FVillage::PlayerHandOffer(const FString& BuildingId, int32 TileX, int32 TileY) const
	{
		const FNpc* P = PlayerActor();
		if (!P || !World || P->Inside.bActive) return EPlayerHand::None;
		if (!BuildingId.IsEmpty())
		{
			const FBuilding* B = FindBuilding(BuildingId);
			if (!B) return EPlayerHand::None;
			if (!B->IsCompleted())
			{
				// Un chantier : d'abord ce que le sac peut lui donner, ensuite la main.
				const AnastasisBuild::FSiteMaterials& M = B->Materials;
				const bool bWood = P->InventoryWood > 0 && M.NeedWood - M.ConsumedWood - M.StockWood > 0;
				const bool bStone = PlayerStone() > 0 && M.NeedStone - M.ConsumedStone - M.StockStone > 0;
				return B->bHasMaterials && (bWood || bStone) ? EPlayerHand::Supply : EPlayerHand::Build;
			}
			if (B->Type == WellType) return EPlayerHand::Drink;
			if (B->Type == GranaryType)
			{
				if (P->InventoryFood > 0 && B->FoodPhysical < GranaryFoodCap) return EPlayerHand::Deposit;
				return B->FoodAvailable() > 0 && P->InventoryFood < PlayerBagCap ? EPlayerHand::Take : EPlayerHand::None;
			}
			return EPlayerHand::None;
		}
		if (TileX < 0 || TileY < 0 || TileX >= World->W || TileY >= World->H) return EPlayerHand::None;
		// relay-mains-001 : une tuile ou se dresse un batiment (acheve ou chantier) n'offre rien a la main. Le batiment
		// la couvre (l'ecran y abat les arbres de sa parcelle) : la main vise le batiment, pas la tuile. Sans cette garde,
		// le bois d'une tuile sous une maison restait « a couper » sans qu'aucun regard puisse l'atteindre.
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (FMath::FloorToInt32(B.X) == TileX && FMath::FloorToInt32(B.Y) == TileY) return EPlayerHand::None;
		}
		const AnastasisWorld::FTile Tile = LiveTileAt(TileX, TileY);
		if (Tile.Resource == EResource::Food && Tile.Amount > 0) return EPlayerHand::Gather;
		if (WoodLeft(Tile) > 0) return EPlayerHand::Chop;
		if (Tile.Resource == EResource::Stone && Tile.Amount > 0) return EPlayerHand::Quarry;
		return EPlayerHand::None;
	}

	FVillage::FPlayerHandResult FVillage::PlayerHandDo(EPlayerHand Hand, const FString& BuildingId, int32 TileX, int32 TileY)
	{
		FPlayerHandResult R;
		FNpc* P = PlayerActor();
		if (!P || !World)
		{
			R.Refusal = TEXT("personne n'est incarne");
			return R;
		}
		if (Hand == EPlayerHand::Eat)
		{
			// Manger ce qu'on porte : la portion du sac, comme l'habitant qui a reserve « inventaire ».
			if (P->InventoryFood <= 0) { R.Refusal = TEXT("rien a manger dans le sac"); return R; }
			--P->InventoryFood;
			AnastasisNeeds::SatisfyEat(P->Needs, P->Inside.bActive, P->Inside.bActive && P->Inside.BuildingId == P->HomeId);
			++P->MealsTaken;
			SetActivity(*P, TEXT("mange"));
			R.bDone = true;
			R.Amount = 1;
			return R;
		}
		if (P->Inside.bActive) { R.Refusal = TEXT("dedans"); return R; }
		if (Hand != PlayerHandOffer(BuildingId, TileX, TileY)) { R.Refusal = TEXT("plus rien a faire ici"); return R; }

		// La portee : le seuil du batiment le plus proche, ou le centre de la tuile.
		if (!BuildingId.IsEmpty())
		{
			const FBuilding* B = FindBuilding(BuildingId);
			double Best = AnastasisMath::Dist(P->X, P->Y, B->X + 0.5, B->Y + 0.5);
			for (const FPoint& Door : B->AccessPoints) Best = FMath::Min(Best, AnastasisMath::Dist(P->X, P->Y, Door.X, Door.Y));
			if (Best > PlayerHandReach) { R.Refusal = TEXT("trop loin"); return R; }
		}
		else if (AnastasisMath::Dist(P->X, P->Y, TileX + 0.5, TileY + 0.5) > PlayerHandReach)
		{
			R.Refusal = TEXT("trop loin");
			return R;
		}

		// Un geste direct retire l'intention posee, comme conduire le corps a la main.
		if (bHasPlayerChoice)
		{
			bHasPlayerChoice = false;
			PlayerChoice = FPlayerGoalChoice();
			bPlayerChoiceDirty = true;
		}
		P->bHasTarget = false;
		P->AiThinkAt = Now;
		const int32 Index = TileY * World->W + TileX;
		switch (Hand)
		{
		case EPlayerHand::Drink:
			AnastasisNeeds::SatisfyDrink(P->Needs);
			++P->DrinksTaken;
			SetActivity(*P, TEXT("boit"));
			R.Amount = 1;
			break;
		case EPlayerHand::Gather:
			if (P->InventoryFood >= PlayerBagCap) { R.Refusal = TEXT("le sac est plein"); return R; }
			TakeFromTile(Index, 1);
			if (LiveTileAt(TileX, TileY).Amount <= 0) DepleteTile(Index);
			++P->InventoryFood;
			++P->GatheredFood;
			SetActivity(*P, TEXT("cueille"));
			R.Amount = 1;
			break;
		case EPlayerHand::Chop:
			if (P->InventoryWood >= PlayerBagCap) { R.Refusal = TEXT("le sac est plein"); return R; }
			TakeFromTile(Index, 1);
			// Le plancher forestier empeche d'epuiser une foret ; une tuile de bois isolee peut l'etre.
			if (LiveTileAt(TileX, TileY).Amount <= 0) DepleteTile(Index);
			++P->InventoryWood;
			++P->GatheredWood;
			SetActivity(*P, TEXT("bucher"));
			R.Amount = 1;
			break;
		case EPlayerHand::Quarry:
			if (P->MaterialResource != EResource::Stone && P->MaterialCarry > 0) { R.Refusal = TEXT("les mains sont prises"); return R; }
			if (P->MaterialCarry >= PlayerBagCap) { R.Refusal = TEXT("le sac est plein"); return R; }
			TakeFromTile(Index, 1);
			if (LiveTileAt(TileX, TileY).Amount <= 0) DepleteTile(Index);
			P->MaterialResource = EResource::Stone;
			++P->MaterialCarry;
			SetActivity(*P, TEXT("taille"));
			R.Amount = 1;
			break;
		case EPlayerHand::Deposit:
		{
			const int32 Accepted = CreditFood(BuildingId, P->InventoryFood);
			if (Accepted <= 0) { R.Refusal = TEXT("le grenier est plein"); return R; }
			P->InventoryFood -= Accepted;
			P->DeliveredFood += Accepted;
			SetActivity(*P, TEXT("depose"));
			R.Amount = Accepted;
			break;
		}
		case EPlayerHand::Take:
		{
			// Une portion libre du grenier (les repas reserves par les autres restent a eux).
			FBuilding* B = Buildings.FindById(BuildingId);
			if (!B || B->FoodAvailable() <= 0) { R.Refusal = TEXT("le grenier est vide"); return R; }
			if (P->InventoryFood >= PlayerBagCap) { R.Refusal = TEXT("le sac est plein"); return R; }
			--B->FoodPhysical;
			++P->InventoryFood;
			SetActivity(*P, TEXT("prend"));
			R.Amount = 1;
			break;
		}
		case EPlayerHand::Supply:
		{
			// Ce qui manque au devis, pas plus (comme le porteur, `ProgressMaterialCourier`) ; le stock du chantier
			// mesure ce qu'il a vraiment pris de chaque chose.
			const FBuilding* Site = FindBuilding(BuildingId);
			const AnastasisBuild::FSiteMaterials Before = Site->Materials;
			const int32 WoodMissing = FMath::Max(0, Before.NeedWood - Before.ConsumedWood - Before.StockWood);
			const int32 StoneMissing = FMath::Max(0, Before.NeedStone - Before.ConsumedStone - Before.StockStone);
			const int32 Credited = CreditSiteMaterials(BuildingId, FMath::Min(P->InventoryWood, WoodMissing),
				FMath::Min(PlayerStone(), StoneMissing));
			if (Credited <= 0) { R.Refusal = TEXT("le chantier n'en veut plus"); return R; }
			const AnastasisBuild::FSiteMaterials& After = FindBuilding(BuildingId)->Materials;
			P->InventoryWood -= After.StockWood - Before.StockWood;
			P->MaterialCarry -= After.StockStone - Before.StockStone;
			if (P->MaterialCarry <= 0) { P->MaterialCarry = 0; P->MaterialResource = EResource::None; }
			P->MaterialsDelivered += Credited;
			SetActivity(*P, TEXT("livre"));
			R.Amount = Credited;
			break;
		}
		case EPlayerHand::Build:
		{
			FBuilding* Site = Buildings.FindById(BuildingId);
			if (!Site) { R.Refusal = TEXT("plus de chantier"); return R; }
			if (!CanBuildAt(*Site, P->Id)) { R.Refusal = TEXT("ce chantier n'est pas le votre : demandez a la famille"); return R; }
			if (!WorkConstruction(*Site, *P)) { R.Refusal = TEXT("il manque du bois ou de la pierre"); return R; }
			SetActivity(*P, TEXT("chantier"));
			R.Amount = 1;
			break;
		}
		default:
			R.Refusal = TEXT("rien a faire");
			return R;
		}
		R.bDone = true;
		return R;
	}
}
