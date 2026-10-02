// resource-targets-001 -- la prevision de survie et le risque spatial d'adultScores.
//
// Port de `survivalForecastBias` et `spatialRiskBiasMap` (npc.js l. 1336 et 1448), de leurs replis
// (`forecastEatTarget`, `forecastDrinkTarget` / `drinkTarget`, `forecastRestTarget`, `shelterRainAccess`)
// et des cibles de `spatialRiskTargetForGoal` (`gatherWoodTarget`, `recallOrSearch`, `recallResource`,
// `fieldWorkTarget` / `farmPos`, `constructionAccessPoint`, `maintenancePos` / `workCommutePos`,
// `householdAidTarget` / `stableBuildingAccess`). La partie pure vit dans `Ai/AnastasisSpatialRisk`.
//
// Ces fonctions ECRIVENT, comme dans la reference : chaque `buildingAccessPoint` filtre les seuils devenus
// bloques (`ensureBuildingAccessPoints`, paresseux) et pose la destination de l'habitant. C'est par la
// preparation de la decision que la reference retire au tick 32 le seuil (49.5, 57.5) du puits.
//
// Reduit : les croyances (`bestKnownWater`, `bestKnownBed`, ecart n°33), le choix par quartier
// (`pickDistrictAwareBuilding`, ecart n°3), le plan d'aide du foyer (ecart n°7), l'intention du jour
// (`intentExploreHint`, ecart n°24). Sans objet dans le catalogue porte (puits, maison, grenier) : postes
// d'extraction, scierie, elevage, taverne, batiments de commandement, gardes.

#include "Village/AnastasisVillage.h"

#include "Ai/AnastasisSpatialRisk.h"
#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Work/AnastasisGather.h"

namespace AnastasisVillage
{
	bool IsCivicGroupType(const FString& Type);

	namespace
	{
		TOptional<FVector2D> OptionalOf(bool bFound, const FPoint& P)
		{
			if (!bFound) return {};
			return FVector2D(P.X, P.Y);
		}

		/** `NPC_UNSTICK.accessLockRadius`. */
		constexpr double AccessLockRadius = 6.8;
	}

	TArray<TPair<FString, double>> FVillage::SurvivalForecastBiasFor(FNpc& Npc, bool bMealBlocked)
	{
		AnastasisSpatialRisk::FForecastInputs In;
		In.NpcX = Npc.X;
		In.NpcY = Npc.Y;
		In.Speed = Npc.Speed;
		In.Hunger = Npc.Needs.Hunger;
		In.Thirst = Npc.Needs.Thirst;
		In.Energy = Npc.Needs.Energy;
		In.InventoryFood = Npc.InventoryFood;
		In.bMealBlocked = bMealBlocked;
		// Dans l'ordre de la reference : manger, boire, dormir.
		FPoint P;
		In.EatTarget = OptionalOf(ForecastEatTarget(Npc, P), P);
		In.DrinkTarget = OptionalOf(ForecastDrinkTarget(Npc, P), P);
		In.RestTarget = OptionalOf(ForecastRestTarget(Npc, P), P);
		return AnastasisSpatialRisk::SurvivalForecastBias(In);
	}

	TArray<TPair<FString, double>> FVillage::SpatialRiskBiasMapFor(FNpc& Npc)
	{
		AnastasisSpatialRisk::FRiskInputs In;
		In.NpcX = Npc.X;
		In.NpcY = Npc.Y;
		In.Speed = Npc.Speed;
		In.Hunger = Npc.Needs.Hunger;
		In.Thirst = Npc.Needs.Thirst;
		In.Energy = Npc.Needs.Energy;
		// `secondsUntilNight(sim)` lit `civilHourOf(sim)` = `sim.dayFrac()`.
		In.DayFrac = AnastasisRhythm::DayFracOf(Now);
		In.Rain = TickWeather.Rain;
		// Les replis AVANT les cibles, dans l'ordre de la reference : dormir, manger, boire, s'abriter.
		FPoint P;
		In.RestSafe = OptionalOf(ForecastRestTarget(Npc, P), P);
		In.EatSafe = OptionalOf(ForecastEatTarget(Npc, P), P);
		In.DrinkSafe = OptionalOf(ForecastDrinkTarget(Npc, P), P);
		// `shelterRainAccess(sim, npc) || restSafe`.
		In.ShelterSafe = OptionalOf(ShelterRainAccess(Npc, P), P);
		if (!In.ShelterSafe.IsSet()) In.ShelterSafe = In.RestSafe;
		return AnastasisSpatialRisk::BiasMap(In, [this, &Npc](const FString& Goal)
		{
			FPoint Target;
			return OptionalOf(SpatialRiskTargetForGoal(Npc, Goal, Target), Target);
		});
	}

	bool FVillage::ForecastEatTarget(FNpc& Npc, FPoint& Out)
	{
		// `livingHome(npc)` = foyer, sinon abri ; `(npc.inventory.food | 0) > 0`.
		const FString& Living = Npc.LivingHomeId();
		if (Npc.InventoryFood > 0 && !Living.IsEmpty())
		{
			if (FBuilding* Home = Buildings.FindById(Living))
			{
				return BuildingAccessPoint(*Home, &Npc, Out);
			}
		}
		// `sim.marketAccessPoint?.(npc) || sim.marketPos?.()` : aucun marche bati, le site prevu.
		if (MarketAccessPoint(&Npc, Out)) return true;
		Out = PlannedMarketPos();
		return true;
	}

	bool FVillage::ForecastDrinkTarget(FNpc& Npc, FPoint& Out)
	{
		// `bestKnownWater(sim, npc)` : les croyances ne sont pas portees (ecart n°33).
		// `sim.drinkAccessPoint(npc)` : le puits le plus proche, sinon la berge a 14 cases (DrinkTarget),
		// sinon `accessPointNear(settlement)` ; puis `sim.accessPointNear(sim.settlement, npc)`.
		FString Source;
		if (DrinkTarget(Npc, Out, Source)) return true;
		return AccessPointNear(Settlement.X, Settlement.Y, &Npc, Out);
	}

	bool FVillage::ForecastRestTarget(FNpc& Npc, FPoint& Out)
	{
		// `bestKnownBed(sim, npc) || accessPointNear(settlement) || marketAccessPoint(npc)`.
		if (KnownBedOf(Npc, Out)) return true;
		if (AccessPointNear(Settlement.X, Settlement.Y, &Npc, Out)) return true;
		return MarketAccessPoint(&Npc, Out);
	}

	bool FVillage::KnownBedOf(const FNpc& Npc, FPoint& Out) const
	{
		// ecart n°33 : `seedHomeBedBelief` seme le lit du foyer (`npc.home`, pas l'abri) s'il est acheve ;
		// les autres lits vus ou entendus ne sont pas portes. Seul, il gagne : `{ home.x + 0.5, home.y + 0.5 }`.
		if (Npc.HomeId.IsEmpty()) return false;
		const FBuilding* Home = Buildings.FindById(Npc.HomeId);
		if (!Home || Home->Progress < 1.0) return false;
		Out = { Home->X + 0.5, Home->Y + 0.5 };
		return true;
	}

	bool FVillage::StableBuildingAccess(FNpc& Npc, FBuilding& Building, FPoint& Out)
	{
		// `isStableAccessTarget(sim, building, npc.target)` : a moins de 6,8 du centre, case de pied libre.
		if (Npc.bHasTarget && FMath::IsFinite(Npc.Target.X) && FMath::IsFinite(Npc.Target.Y)
			&& AnastasisMath::JsHypot(Npc.Target.X - (Building.X + 0.5), Npc.Target.Y - (Building.Y + 0.5)) <= AccessLockRadius
			&& !IsFootBlocked(Npc.Target.X, Npc.Target.Y))
		{
			Out = Npc.Target;
			return true;
		}
		return BuildingAccessPoint(Building, &Npc, Out);
	}

	bool FVillage::ShelterRainAccess(FNpc& Npc, FPoint& Out)
	{
		// `bestKnownBed` d'abord (ecart n°33).
		if (KnownBedOf(Npc, Out)) return true;
		const FString& Living = Npc.LivingHomeId();
		if (FBuilding* Home = Living.IsEmpty() ? nullptr : Buildings.FindById(Living))
		{
			if (Home->Progress >= 1.0) return StableBuildingAccess(Npc, *Home, Out);
		}
		// Taverne : aucune dans le catalogue porte.
		if (FBuilding* Post = Npc.WorkplaceId.IsEmpty() ? nullptr : Buildings.FindById(Npc.WorkplaceId))
		{
			if (Post->Progress >= 1.0) return BuildingAccessPoint(*Post, &Npc, Out);
		}
		for (FBuilding& B : Buildings.GetItemsMutable())
		{
			// `(b.progress ?? 1) >= 1 && (housing || socialiser || trade || civic)`, dans l'ordre de `sim.buildings`.
			if (B.Progress < 1.0) continue;
			if (HousingOfType(B.Type) > 0 || IsCivicGroupType(B.Type)) return BuildingAccessPoint(B, &Npc, Out);
		}
		FString Source;
		return SocialPos(Npc, Out, Source);
	}

	bool FVillage::SpatialRiskTargetForGoal(FNpc& Npc, const FString& Goal, FPoint& Out)
	{
		if (Goal == TEXT("gatherWood"))
		{
			// `gatherWoodTarget` : sans scierie, ni doctrine de lisiere (ecart n°33), `recallOrSearch(wood)`.
			return RecallOrSearch(Npc, TEXT("wood"), Out);
		}
		if (Goal == TEXT("gatherStone")) return RecallOrSearch(Npc, TEXT("stone"), Out);
		if (Goal == TEXT("gatherFood"))
		{
			// Pas de depot d'elevage dans le catalogue porte.
			return RecallOrSearch(Npc, TEXT("food"), Out);
		}
		if (Goal == TEXT("helpFarm"))
		{
			const int32 Plot = FindTendFieldFor(Npc);
			if (Plot != INDEX_NONE)
			{
				Out = FieldWorkTarget(Npc, LiveTile(Plot));
				return true;
			}
			return FarmPos(Npc, Out);
		}
		if (Goal == TEXT("build")) return ConstructionAccessPoint(Npc, Out);
		if (Goal == TEXT("maintain")) return MaintenancePos(Npc, Out);
		if (Goal == TEXT("aidHousehold")) return HouseholdAidTarget(Npc, Out);
		if (Goal == TEXT("explore"))
		{
			// `intentExploreHint(sim, npc) || npc.target || null` : sans intention du jour (ecart n°24),
			// la cible en cours.
			if (!Npc.bHasTarget) return false;
			Out = Npc.Target;
			return true;
		}
		return false;
	}

	bool FVillage::RecallOrSearch(FNpc& Npc, const FString& Resource, FPoint& Out)
	{
		// Doctrine de lisiere (`wantsColonizationClear`) : non portee (ecart n°33). Cour du poste
		// d'extraction (`extractionCourtTarget`) : aucun poste d'extraction dans le catalogue porte.
		if (RecallResource(Npc, Resource, Out)) return true;
		// `exploreTarget(sim, npc)` : tire dans `sim.rng`.
		Out = ExploreTargetFor(Npc).Point;
		return true;
	}

	bool FVillage::RecallResource(const FNpc& Npc, const FString& Resource, FPoint& Out) const
	{
		const FResourceSpot* Best = nullptr;
		double BestScore = -AnastasisNav::Infinity;
		const int32 Today = Day();
		for (const FResourceSpot& Spot : Npc.Spots)
		{
			if (Spot.Resource != Resource) continue;
			const int32 Age = Today - Spot.Day;
			// `dangerPenaltyAt` : pas de zone de danger connue, 0.
			const double Score = -AnastasisMath::Dist(Npc.X, Npc.Y, Spot.X, Spot.Y) - Age * 1.5
				- (Spot.bHearsay ? AnastasisGather::HearsayPenalty : 0.0) - 0.0;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = &Spot;
			}
		}
		if (!Best) return false;
		Out = { Best->X, Best->Y };
		return true;
	}

	bool FVillage::WorkCommutePos(FNpc& Npc, const FString& Goal, FPoint& Out)
	{
		if (Npc.JobId.IsEmpty() || Npc.JobId == AnastasisGather::JobSettler || Goal.IsEmpty()) return false;
		// `workplaceForNpc` = `assignWorkplace(npc)` : le poste acheve garde son habitant ; la reaffectation
		// d'un poste perdu n'est pas portee (deux metiers, ecart n°10).
		FBuilding* Workplace = Npc.WorkplaceId.IsEmpty() ? nullptr : Buildings.FindById(Npc.WorkplaceId);
		if (!Workplace || Workplace->Progress < 1.0) return false;
		// `maintain` : tout metier entretient son poste. `helpFarm` : seulement un poste `ruralProduction`
		// (aucun dans le catalogue porte).
		if (Goal == TEXT("maintain")) return BuildingAccessPoint(*Workplace, &Npc, Out);
		return false;
	}

	bool FVillage::MaintenancePos(FNpc& Npc, FPoint& Out)
	{
		if (WorkCommutePos(Npc, TEXT("maintain"), Out)) return true;
		// Pas de garde : pas de ronde de quartier.
		int32 Vacant = 0;
		int32 Homeless = 0;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Type == HouseType && B.Progress >= 1.0 && B.Owner.IsEmpty()) ++Vacant;
		}
		for (const FNpc& N : Actors.GetItems())
		{
			// `n.lifeStage !== "child" && !n.home` : aucun enfant dans ce portage.
			if (N.HomeId.IsEmpty()) ++Homeless;
		}
		const bool bVacancy = Vacant >= 1 && Homeless >= 1;
		// `completedBuildingEntries()` sans role `commandement` (aucun dans le catalogue porte).
		TArray<FBuilding*> Candidates;
		for (FBuilding& B : Buildings.GetItemsMutable())
		{
			if (B.Progress >= 1.0) Candidates.Add(&B);
		}
		if (bVacancy)
		{
			TArray<FBuilding*> VacantHomes = Candidates.FilterByPredicate(
				[](const FBuilding* B) { return B->Type == HouseType && B->Owner.IsEmpty(); });
			if (VacantHomes.Num() > 0) Candidates = MoveTemp(VacantHomes);
		}
		// `pickDailyBuilding(candidates, actor, "maintain")` = `pickDistrictAwareBuilding`. Sans quartiers
		// (ecart n°3), son repli : un hachage FNV du jour, de l'habitant, du but et du sel.
		FBuilding* Target = nullptr;
		if (Candidates.Num() > 0)
		{
			const FString GoalOf = Npc.Goal.IsEmpty() ? FString(TEXT("maintain")) : Npc.Goal;
			const FString Text = FString::Printf(TEXT("%s:%s:%s"), *Npc.Id, *GoalOf, TEXT("maintain"));
			// `let hash = (sim.day || 1) * 2166136261` puis `hash ^= c; hash = Math.imul(hash, 16777619) >>> 0`.
			double Hash = static_cast<double>(Day() != 0 ? Day() : 1) * 2166136261.0;
			for (const TCHAR C : Text)
			{
				const int32 Mixed = AnastasisJs::ToInt32(Hash) ^ static_cast<int32>(C);
				Hash = static_cast<double>(static_cast<uint32>(Mixed) * 16777619u);
			}
			Target = Candidates[static_cast<int32>(static_cast<uint32>(Hash) % static_cast<uint32>(Candidates.Num()))];
		}
		if (Target) return BuildingAccessPoint(*Target, &Npc, Out);
		return MarketAccessPoint(&Npc, Out);
	}

	bool FVillage::FarmPos(FNpc& Npc, FPoint& Out)
	{
		// `workCommutePos(helpFarm)`, poste `ruralProduction`, fermes achevees : aucune dans le catalogue
		// porte. Reste `marketAccessPoint(actor)`.
		return MarketAccessPoint(&Npc, Out);
	}

	bool FVillage::HouseholdAidTarget(FNpc& Npc, FPoint& Out)
	{
		// `householdAidPlan(npc)` : pas de plan d'aide du foyer dans ce portage (ecart n°7).
		// `livingHome(npc) ? stableBuildingAccess(sim, npc, livingHome(npc)) : sim.socialPos(npc)`.
		const FString& Living = Npc.LivingHomeId();
		if (FBuilding* Home = Living.IsEmpty() ? nullptr : Buildings.FindById(Living))
		{
			return StableBuildingAccess(Npc, *Home, Out);
		}
		FString Source;
		return SocialPos(Npc, Out, Source);
	}
}
