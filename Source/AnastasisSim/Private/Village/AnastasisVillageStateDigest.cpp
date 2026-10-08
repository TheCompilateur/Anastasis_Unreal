#include "Village/AnastasisVillage.h"

#include "Core/AnastasisStateDigest.h"

// STATE_ORACLE_001 -- l'empreinte d'ETAT du village, distincte de `Digest()`.
//
// `Digest()` est la projection de parite JS : son perimetre est fige pour que les vecteurs de la
// reference ne bougent pas, et il ne lit qu'une partie de l'etat (22 champs de FNpc sur ~110, 5 membres
// de FVillage sur 46, ni `sim.rng`, ni la meteo, ni le joueur). IRON_CRUSADE_001 a montre que deux
// villages d'empreinte egale peuvent avoir des futurs differents (une ecriture dans `Speed` ou `sim.rng`
// diverge apres 8 a 9 s simulees). `StateDigest()` lit TOUT l'etat qui decide du futur : c'est l'oracle
// des tests de determinisme et de non-ecriture. Il n'est pas un format de sauvegarde ni de parite.
//
// Contrat tenu par tools/migration/check-state-fields.mjs : chaque champ d'une structure du registre
// tools/migration/state-fields.json est lu dans son `HashState`, ou y est classe hors etat avec sa raison.
// Ajouter un champ sans le lire ici ni le classer fait echouer `finish`.

namespace AnastasisVillage
{
	namespace
	{
		using AnastasisDigest::FStateWriter;

		// Toutes les surcharges declarees d'abord : HashArray / HashOptionalState les trouvent par
		// recherche ordinaire (la plupart des types vivent hors de ce namespace, l'ADL n'y suffirait pas).
		void HashState(FStateWriter& Out, const FPoint& V);
		void HashState(FStateWriter& Out, const AnastasisNeeds::FNeeds& V);
		void HashState(FStateWriter& Out, const AnastasisWorld::FTile& V);
		void HashState(FStateWriter& Out, const AnastasisBuild::FSiteMaterials& V);
		void HashState(FStateWriter& Out, const AnastasisPlanner::FStockSlot& V);
		void HashState(FStateWriter& Out, const FBuilding& V);
		void HashState(FStateWriter& Out, const FFoodSource& V);
		void HashState(FStateWriter& Out, const FMealReservation& V);
		void HashState(FStateWriter& Out, const FHungerAction& V);
		void HashState(FStateWriter& Out, const FStockBelief& V);
		void HashState(FStateWriter& Out, const FResourceSpot& V);
		void HashState(FStateWriter& Out, const FLastTalk& V);
		void HashState(FStateWriter& Out, const FTalkFatigue& V);
		void HashState(FStateWriter& Out, const FWorkSession& V);
		void HashState(FStateWriter& Out, const FInside& V);
		void HashState(FStateWriter& Out, const AnastasisWorkShift::FWorkShift& V);
		void HashState(FStateWriter& Out, const AnastasisNous::FDecision& V);
		void HashState(FStateWriter& Out, const AnastasisNous::FFoodContext& V);
		void HashState(FStateWriter& Out, const AnastasisGenome::FPhenotype& V);
		void HashState(FStateWriter& Out, const AnastasisConditioning::FConditioning& V);
		void HashState(FStateWriter& Out, const AnastasisLifestyle::FLifestyle& V);
		void HashState(FStateWriter& Out, const FNpc::FPlaceEntry& V);
		void HashState(FStateWriter& Out, const AnastasisBonds::FPersonRow& V);
		void HashState(FStateWriter& Out, const AnastasisBonds::FTomEntry& V);
		void HashState(FStateWriter& Out, const AnastasisBonds::FMoodlet& V);
		void HashState(FStateWriter& Out, const FNpc& V);
		void HashState(FStateWriter& Out, const FPlayerGoalChoice& V);
		void HashState(FStateWriter& Out, const FPlayerRefusal& V);
		void HashState(FStateWriter& Out, const FPlayerGoalOption& V);
		void HashState(FStateWriter& Out, const FVillage::FDeath& V);
		void HashState(FStateWriter& Out, const AnastasisWeatherBehavior::FSimWeather& V);
		void HashState(FStateWriter& Out, const AnastasisBudget::FDirector& V);
		void HashState(FStateWriter& Out, const AnastasisNav::FNavGrid& V);
		void HashState(FStateWriter& Out, const FGoalExplainEntry& V);
		void HashState(FStateWriter& Out, const FGoalExplain& V);
		void HashState(FStateWriter& Out, const FStreetDecision& V);
		void HashState(FStateWriter& Out, const AnastasisNature::FNature& V);
		void HashState(FStateWriter& Out, const AnastasisNavService::FNavJob& V);
		void HashState(FStateWriter& Out, const AnastasisNavService::FNavCacheEntry& V);
		void HashState(FStateWriter& Out, const AnastasisNavService::FNavService& V);
		void HashState(FStateWriter& Out, const AnastasisPlanner::FUrgencySnapshot& V);

		template <typename T>
		void HashArray(FStateWriter& Out, const TArray<T>& Items)
		{
			Out.BeginArray(Items.Num());
			for (const T& Item : Items) HashState(Out, Item);
			Out.EndArray();
		}

		template <typename T>
		void HashOptionalState(FStateWriter& Out, const TOptional<T>& V)
		{
			if (V.IsSet()) HashState(Out, V.GetValue()); else Out.Null();
		}

		void HashState(FStateWriter& Out, const FPoint& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("x")).Number(V.X);
			Out.Key(TEXT("y")).Number(V.Y);
			Out.EndObject();
		}

		void HashOptional(FStateWriter& Out, const TOptional<double>& V)
		{
			if (V.IsSet()) Out.Number(V.GetValue()); else Out.Null();
		}

		void HashStrings(FStateWriter& Out, const TArray<FString>& Items)
		{
			Out.BeginArray(Items.Num());
			for (const FString& S : Items) Out.String(S);
			Out.EndArray();
		}

		void HashNamedNumbers(FStateWriter& Out, const TArray<TPair<FString, double>>& Items)
		{
			Out.BeginArray(Items.Num());
			for (const TPair<FString, double>& P : Items)
			{
				Out.BeginArray(2);
				Out.String(P.Key).Number(P.Value);
				Out.EndArray();
			}
			Out.EndArray();
		}

		void HashState(FStateWriter& Out, const AnastasisNeeds::FNeeds& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("hunger")).Number(V.Hunger);
			Out.Key(TEXT("energy")).Number(V.Energy);
			Out.Key(TEXT("social")).Number(V.Social);
			Out.Key(TEXT("leisure")).Number(V.Leisure);
			Out.Key(TEXT("hygiene")).Number(V.Hygiene);
			Out.Key(TEXT("thirst")).Number(V.Thirst);
			Out.Key(TEXT("health")).Number(V.Health);
			Out.Key(TEXT("morale")).Number(V.Morale);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisWorld::FTile& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("x")).Number(V.X);
			Out.Key(TEXT("y")).Number(V.Y);
			Out.Key(TEXT("type")).Number(static_cast<int32>(V.Type));
			Out.Key(TEXT("resource")).Number(static_cast<int32>(V.Resource));
			Out.Key(TEXT("amount")).Number(V.Amount);
			Out.Key(TEXT("alt")).Number(V.Alt);
			Out.Key(TEXT("shade")).Number(V.Shade);
			Out.Key(TEXT("shore")).Number(V.Shore);
			Out.Key(TEXT("wetness")).Number(V.Wetness);
			Out.Key(TEXT("flowX")).Number(V.FlowX);
			Out.Key(TEXT("flowZ")).Number(V.FlowZ);
			Out.Key(TEXT("flowAmt")).Number(V.FlowAmt);
			Out.Key(TEXT("cropId")).Number(static_cast<int32>(V.CropId));
			Out.Key(TEXT("fertility")).Number(V.Fertility);
			Out.Key(TEXT("forestMargin")).Number(V.ForestMargin);
			Out.Key(TEXT("hasForestMargin")).Bool(V.bHasForestMargin);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisBuild::FSiteMaterials& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("needWood")).Number(V.NeedWood);
			Out.Key(TEXT("needStone")).Number(V.NeedStone);
			Out.Key(TEXT("consumedWood")).Number(V.ConsumedWood);
			Out.Key(TEXT("consumedStone")).Number(V.ConsumedStone);
			Out.Key(TEXT("stockWood")).Number(V.StockWood);
			Out.Key(TEXT("stockStone")).Number(V.StockStone);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisPlanner::FStockSlot& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("physical")).Number(V.Physical);
			Out.Key(TEXT("reserved")).Number(V.Reserved);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FBuilding& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("id")).String(V.Id);
			Out.Key(TEXT("type")).String(V.Type);
			Out.Key(TEXT("x")).Number(V.X);
			Out.Key(TEXT("y")).Number(V.Y);
			Out.Key(TEXT("progress")).Number(V.Progress);
			Out.Key(TEXT("createdDay")).Number(V.CreatedDay);
			Out.Key(TEXT("owner")).String(V.Owner);
			Out.Key(TEXT("housePhase")).Number(V.HousePhase);
			Out.Key(TEXT("accessPoints"));
			HashArray(Out, V.AccessPoints);
			Out.Key(TEXT("hasPlannerStock")).Bool(V.bHasPlannerStock);
			Out.Key(TEXT("plannerStock")).BeginArray(V.PlannerStock.Num());
			for (const TPair<FString, AnastasisPlanner::FStockSlot>& Slot : V.PlannerStock)
			{
				Out.BeginArray(2);
				Out.String(Slot.Key);
				HashState(Out, Slot.Value);
				Out.EndArray();
			}
			Out.EndArray();
			Out.Key(TEXT("foodPhysical")).Number(V.FoodPhysical);
			Out.Key(TEXT("foodReserved")).Number(V.FoodReserved);
			Out.Key(TEXT("laborToday")).Number(V.LaborToday);
			Out.Key(TEXT("hasLaborToday")).Bool(V.bHasLaborToday);
			Out.Key(TEXT("piecesPlaced")).Number(V.PiecesPlaced);
			Out.Key(TEXT("hasMaterials")).Bool(V.bHasMaterials);
			Out.Key(TEXT("materials"));
			HashState(Out, V.Materials);
			Out.Key(TEXT("builderId")).String(V.BuilderId);
			Out.Key(TEXT("workers")).BeginArray(V.Workers.Num());
			for (const TPair<FString, int32>& Worker : V.Workers)
			{
				Out.BeginArray(2);
				Out.String(Worker.Key).Number(Worker.Value);
				Out.EndArray();
			}
			Out.EndArray();
			Out.Key(TEXT("completedDay")).Number(V.CompletedDay);
			Out.Key(TEXT("completedById")).String(V.CompletedById);
			Out.Key(TEXT("vacantSinceDay")).Number(V.VacantSinceDay);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FFoodSource& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("tile")).Number(V.TileIndex);
			Out.Key(TEXT("position"));
			HashState(Out, V.Position);
			Out.Key(TEXT("initial")).Number(V.Initial);
			Out.Key(TEXT("remaining")).Number(V.Remaining);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FMealReservation& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("id")).String(V.Id);
			Out.Key(TEXT("npcId")).String(V.NpcId);
			Out.Key(TEXT("buildingId")).String(V.BuildingId);
			Out.Key(TEXT("source")).String(V.Source);
			Out.Key(TEXT("amount")).Number(V.Amount);
			Out.Key(TEXT("createdAt")).Number(V.CreatedAt);
			Out.Key(TEXT("expiresAt")).Number(V.ExpiresAt);
			Out.Key(TEXT("absoluteExpiresAt")).Number(V.AbsoluteExpiresAt);
			Out.Key(TEXT("renewals")).Number(V.Renewals);
			Out.Key(TEXT("lastProgressAt")).Number(V.LastProgressAt);
			Out.Key(TEXT("lastDistance")).Number(V.LastDistance);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FHungerAction& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("state")).String(V.State);
			Out.Key(TEXT("targetId")).String(V.TargetId);
			Out.Key(TEXT("startedAt")).Number(V.StartedAt);
			Out.Key(TEXT("lastFailure")).String(V.LastFailure);
			Out.Key(TEXT("cooldownUntil")).Number(V.CooldownUntil);
			Out.Key(TEXT("progress")).Number(V.Progress);
			Out.Key(TEXT("reservationId")).String(V.ReservationId);
			Out.Key(TEXT("sourceBuildingId")).String(V.SourceBuildingId);
			Out.Key(TEXT("excludedType")).String(V.ExcludedType);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FStockBelief& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("key")).String(V.Key);
			Out.Key(TEXT("resource")).String(V.Resource);
			Out.Key(TEXT("buildingId")).String(V.BuildingId);
			Out.Key(TEXT("kind")).String(V.Kind);
			Out.Key(TEXT("x")).Number(V.X);
			Out.Key(TEXT("y")).Number(V.Y);
			Out.Key(TEXT("estimatedAmount")).Number(V.EstimatedAmount);
			Out.Key(TEXT("confidence")).Number(V.Confidence);
			Out.Key(TEXT("day")).Number(V.Day);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FResourceSpot& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("key")).String(V.Key);
			Out.Key(TEXT("x")).Number(V.X);
			Out.Key(TEXT("y")).Number(V.Y);
			Out.Key(TEXT("resource")).String(V.Resource);
			Out.Key(TEXT("amount")).Number(V.Amount);
			Out.Key(TEXT("day")).Number(V.Day);
			Out.Key(TEXT("hearsay")).Bool(V.bHearsay);
			Out.Key(TEXT("sourceId")).String(V.SourceId);
			Out.Key(TEXT("originalSourceId")).String(V.OriginalSourceId);
			Out.Key(TEXT("hopCount")).Number(V.HopCount);
			Out.Key(TEXT("receivedDay")).Number(V.ReceivedDay);
			Out.Key(TEXT("receivedAt")).Number(V.ReceivedAt);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FLastTalk& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("valid")).Bool(V.bValid);
			Out.Key(TEXT("withId")).String(V.WithId);
			Out.Key(TEXT("at")).Number(V.At);
			Out.Key(TEXT("refuse")).Bool(V.bRefuse);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FTalkFatigue& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("id")).String(V.Id);
			Out.Key(TEXT("count")).Number(V.Count);
			Out.Key(TEXT("at")).Number(V.At);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FWorkSession& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("active")).Bool(V.bActive);
			Out.Key(TEXT("craftId")).String(V.CraftId);
			Out.Key(TEXT("tileX")).Number(V.TileX);
			Out.Key(TEXT("tileY")).Number(V.TileY);
			Out.Key(TEXT("postIndex")).Number(V.PostIndex);
			Out.Key(TEXT("arrivedAt")).Number(V.ArrivedAt);
			Out.Key(TEXT("nextSwingAt")).Number(V.NextSwingAt);
			Out.Key(TEXT("swingsDone")).Number(V.SwingsDone);
			Out.Key(TEXT("lastSwingAt")).Number(V.LastSwingAt);
			Out.Key(TEXT("buildingId")).String(V.BuildingId);
			Out.Key(TEXT("actionAcc")).Number(V.ActionAcc);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FInside& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("active")).Bool(V.bActive);
			Out.Key(TEXT("buildingId")).String(V.BuildingId);
			Out.Key(TEXT("activity")).String(V.Activity);
			Out.Key(TEXT("goal")).String(V.Goal);
			Out.Key(TEXT("enteredAt")).Number(V.EnteredAt);
			Out.Key(TEXT("until")).Number(V.Until);
			Out.Key(TEXT("exitX")).Number(V.ExitX);
			Out.Key(TEXT("exitY")).Number(V.ExitY);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisWorkShift::FWorkShift& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("state")).Number(static_cast<int32>(V.State));
			Out.Key(TEXT("goal")).String(V.Goal);
			Out.Key(TEXT("startedAt")).Number(V.StartedAt);
			Out.Key(TEXT("floorUntil")).Number(V.FloorUntil);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisNous::FDecision& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("type")).String(V.Type);
			Out.Key(TEXT("targetId")).String(V.TargetId);
			Out.Key(TEXT("score")).Number(V.Score);
			Out.Key(TEXT("urgency")).Number(V.Urgency);
			Out.Key(TEXT("reason")).String(V.Reason);
			Out.Key(TEXT("createdAt")).Number(V.CreatedAt);
			Out.Key(TEXT("expectedDuration")).Number(V.ExpectedDuration);
			Out.Key(TEXT("raw")).Number(V.Raw);
			Out.Key(TEXT("sourceBuildingId")).String(V.SourceBuildingId);
			Out.Key(TEXT("travelSeconds")).Number(V.TravelSeconds);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisNous::FFoodContext& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("hunger")).Number(V.Hunger);
			Out.Key(TEXT("inventoryFood")).Number(V.InventoryFood);
			Out.Key(TEXT("believedFood")).Number(V.BelievedFood);
			Out.Key(TEXT("bestSourceBuildingId")).String(V.BestSourceBuildingId);
			Out.Key(TEXT("bestSourceDistance")).Number(V.BestSourceDistance);
			Out.Key(TEXT("bestSourceEstimated")).Number(V.BestSourceEstimated);
			Out.Key(TEXT("bestSourceConfidence")).Number(V.BestSourceConfidence);
			Out.Key(TEXT("certainty")).Number(V.Certainty);
			Out.Key(TEXT("dangerNear")).Bool(V.bDangerNear);
			Out.Key(TEXT("gold")).Number(V.Gold);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisGenome::FPhenotype& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("loci")).BeginArray(AnastasisGenome::NumLoci);
			for (int32 I = 0; I < AnastasisGenome::NumLoci; ++I) Out.Number(V.Loci[I]);
			Out.EndArray();
			Out.Key(TEXT("hydrationLossMultiplier")).Number(V.HydrationLossMultiplier);
			Out.Key(TEXT("heatDissipationEfficiency")).Number(V.HeatDissipationEfficiency);
			Out.Key(TEXT("metabolicDemandMultiplier")).Number(V.MetabolicDemandMultiplier);
			Out.Key(TEXT("metabolicPeakRecoveryMultiplier")).Number(V.MetabolicPeakRecoveryMultiplier);
			Out.Key(TEXT("fatigueRecoveryMultiplier")).Number(V.FatigueRecoveryMultiplier);
			Out.Key(TEXT("fatigueRecoveryStrainCost")).Number(V.FatigueRecoveryStrainCost);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisConditioning::FConditioning& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("version")).Number(V.Version);
			Out.Key(TEXT("workConditioning")).Number(V.WorkConditioning);
			Out.Key(TEXT("fatigueAdaptation")).Number(V.FatigueAdaptation);
			Out.Key(TEXT("recoveryConditioning")).Number(V.RecoveryConditioning);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisLifestyle::FLifestyle& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("id")).String(V.Id);
			Out.Key(TEXT("sinceDay")).Number(V.SinceDay);
			Out.Key(TEXT("rhythmScore")).Number(V.RhythmScore);
			Out.Key(TEXT("lastNotedDay")).Number(V.LastNotedDay);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FNpc::FPlaceEntry& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("buildingId")).String(V.BuildingId);
			Out.Key(TEXT("type")).String(V.Type);
			Out.Key(TEXT("score")).Number(V.Score);
			Out.Key(TEXT("work")).Number(V.Work);
			Out.Key(TEXT("social")).Number(V.Social);
			Out.Key(TEXT("home")).Number(V.Home);
			Out.Key(TEXT("talk")).Number(V.Talk);
			Out.Key(TEXT("drink")).Number(V.Drink);
			Out.Key(TEXT("activity")).Number(V.Activity);
			Out.Key(TEXT("crisis")).Number(V.Crisis);
			Out.Key(TEXT("decayDay")).Number(V.DecayDay);
			Out.Key(TEXT("lastDay")).Number(V.LastDay);
			Out.Key(TEXT("lifestyle"));
			HashNamedNumbers(Out, V.Lifestyle);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisBonds::FPersonRow& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("id")).String(V.Id);
			Out.Key(TEXT("trust")).Number(V.Trust);
			Out.Key(TEXT("tag")).Number(static_cast<int32>(V.Tag));
			Out.Key(TEXT("day")).Number(V.Day);
			Out.Key(TEXT("meets")).Number(V.Meets);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisBonds::FTomEntry& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("id")).String(V.Id);
			Out.Key(TEXT("estimatedGoal")).String(V.EstimatedGoal);
			Out.Key(TEXT("attitude")).Number(V.Attitude);
			Out.Key(TEXT("confidence")).Number(V.Confidence);
			Out.Key(TEXT("day")).Number(V.Day);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisBonds::FMoodlet& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("id")).String(V.Id);
			Out.Key(TEXT("at")).Number(V.At);
			Out.Key(TEXT("until")).Number(V.Until);
			Out.EndObject();
		}

		void HashOptionalBool(FStateWriter& Out, const TOptional<bool>& V)
		{
			if (V.IsSet()) Out.Bool(V.GetValue()); else Out.Null();
		}

		void HashState(FStateWriter& Out, const FGoalExplainEntry& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("goal")).String(V.Goal);
			Out.Key(TEXT("score")).Number(V.Score);
			Out.Key(TEXT("cause")).String(V.Cause);
			Out.Key(TEXT("causeKey")).String(V.CauseKey);
			Out.Key(TEXT("causeValue")).Number(V.CauseValue);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FGoalExplain& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("at")).Number(V.At);
			Out.Key(TEXT("goal")).String(V.Goal);
			Out.Key(TEXT("top"));
			HashArray(Out, V.Top);
			Out.Key(TEXT("line")).String(V.Line);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FStreetDecision& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("at")).Number(V.At);
			Out.Key(TEXT("until")).Number(V.Until);
			Out.Key(TEXT("goal")).String(V.Goal);
			Out.Key(TEXT("from")).String(V.From);
			Out.Key(TEXT("margin")).Number(V.Margin);
			Out.Key(TEXT("changed")).Bool(V.bChanged);
			Out.Key(TEXT("tight")).Bool(V.bTight);
			Out.Key(TEXT("cause")).String(V.Cause);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisNature::FNature& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("corps")).Number(V.Corps);
			Out.Key(TEXT("esprit")).Number(V.Esprit);
			Out.Key(TEXT("coeur")).Number(V.Coeur);
			Out.Key(TEXT("qualities"));
			HashStrings(Out, V.Qualities);
			Out.Key(TEXT("flaws"));
			HashStrings(Out, V.Flaws);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisNavService::FNavJob& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("actorId")).String(V.ActorId);
			Out.Key(TEXT("start"));
			HashState(Out, V.Start);
			Out.Key(TEXT("destination"));
			HashState(Out, V.Destination);
			Out.Key(TEXT("goalKey")).String(V.GoalKey);
			Out.Key(TEXT("priority")).Number(V.Priority);
			Out.Key(TEXT("allowBlockedTarget")).Bool(V.bAllowBlockedTarget);
			Out.Key(TEXT("requestedAt")).Number(V.RequestedAt);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisNavService::FNavCacheEntry& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("path"));
			HashArray(Out, V.Path);
			Out.Key(TEXT("navVersion")).Number(V.NavVersion);
			Out.Key(TEXT("storedAt")).Number(V.StoredAt);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisNavService::FNavService& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("queue"));
			HashArray(Out, V.Queue);
			// Le cache garde l'ordre d'insertion (ses evictions suivent cet ordre) : on le lit tel quel.
			const TArray<FString>& Keys = V.Cache.GetKeys();
			Out.Key(TEXT("cache")).BeginArray(Keys.Num());
			for (int32 I = 0; I < Keys.Num(); ++I)
			{
				Out.BeginArray(2);
				Out.String(Keys[I]);
				HashState(Out, V.Cache.GetEntry(I));
				Out.EndArray();
			}
			Out.EndArray();
			TArray<FString> Pending;
			V.PendingByActor.GetKeys(Pending);
			Pending.Sort();
			Out.Key(TEXT("pendingByActor")).BeginArray(Pending.Num());
			for (const FString& Id : Pending)
			{
				Out.BeginArray(2);
				Out.String(Id).Number(V.PendingByActor[Id]);
				Out.EndArray();
			}
			Out.EndArray();
			Out.Key(TEXT("calcThisTick")).Number(V.CalcThisTick);
			Out.Key(TEXT("maxCalcs")).Number(V.MaxCalcs);
			Out.Key(TEXT("cacheTtl")).Number(V.CacheTtl);
			Out.Key(TEXT("lastSweepAt")).Number(V.LastSweepAt);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FNpc& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("id")).String(V.Id);
			Out.Key(TEXT("x")).Number(V.X);
			Out.Key(TEXT("y")).Number(V.Y);
			Out.Key(TEXT("speed")).Number(V.Speed);
			Out.Key(TEXT("needs"));
			HashState(Out, V.Needs);
			Out.Key(TEXT("goal")).String(V.Goal);
			Out.Key(TEXT("activity")).String(V.Activity);
			Out.Key(TEXT("hasTarget")).Bool(V.bHasTarget);
			Out.Key(TEXT("target"));
			HashState(Out, V.Target);
			Out.Key(TEXT("workTimer")).Number(V.WorkTimer);
			Out.Key(TEXT("aiThinkAt")).Number(V.AiThinkAt);
			Out.Key(TEXT("failedActions")).Number(V.FailedActions);
			Out.Key(TEXT("homeId")).String(V.HomeId);
			Out.Key(TEXT("shelterId")).String(V.ShelterId);
			Out.Key(TEXT("inside"));
			HashState(Out, V.Inside);
			Out.Key(TEXT("doorStuckAt")).Number(V.DoorStuckAt);
			Out.Key(TEXT("doorApproachAt")).Number(V.DoorApproachAt);

			Out.Key(TEXT("path"));
			HashArray(Out, V.Path);
			Out.Key(TEXT("pathStep")).Number(V.PathStep);
			Out.Key(TEXT("hasPathGoal")).Bool(V.bHasPathGoal);
			Out.Key(TEXT("pathGoal"));
			HashState(Out, V.PathGoal);
			Out.Key(TEXT("pathFailed")).Bool(V.bPathFailed);
			Out.Key(TEXT("pathCooldown")).Number(V.PathCooldown);
			Out.Key(TEXT("navTargetKey")).String(V.NavTargetKey);
			Out.Key(TEXT("navVersion")).Number(V.NavVersion);
			Out.Key(TEXT("destBuildingId")).String(V.DestBuildingId);
			Out.Key(TEXT("stuckTimer")).Number(V.StuckTimer);
			Out.Key(TEXT("stuckStage")).Number(V.StuckStage);
			// `actor.trafficTimer` : decide quand tombe le prochain passage (relay-settlement-001).
			Out.Key(TEXT("trafficTimer")).Number(V.TrafficTimer);

			Out.Key(TEXT("inventoryFood")).Number(V.InventoryFood);
			Out.Key(TEXT("materialCarry")).Number(V.MaterialCarry);
			Out.Key(TEXT("materialResource")).Number(static_cast<int32>(V.MaterialResource));
			Out.Key(TEXT("materialSourceIndex")).Number(V.MaterialSourceIndex);
			Out.Key(TEXT("materialRetryAt")).Number(V.MaterialRetryAt);
			Out.Key(TEXT("materialsDelivered")).Number(V.MaterialsDelivered);
			Out.Key(TEXT("gatheredFood")).Number(V.GatheredFood);
			Out.Key(TEXT("deliveredFood")).Number(V.DeliveredFood);
			Out.Key(TEXT("foodSourceIndex")).Number(V.FoodSourceIndex);
			{
				TArray<int32> Keys;
				V.KnownFoodSources.GetKeys(Keys);
				Keys.Sort();
				Out.Key(TEXT("knownFoodSources")).BeginArray(Keys.Num());
				for (const int32 K : Keys)
				{
					Out.BeginArray(2);
					Out.Number(K).Number(V.KnownFoodSources[K]);
					Out.EndArray();
				}
				Out.EndArray();
			}

			Out.Key(TEXT("jobId")).String(V.JobId);
			Out.Key(TEXT("workplaceId")).String(V.WorkplaceId);
			Out.Key(TEXT("traitIndex")).Number(V.TraitIndex);
			Out.Key(TEXT("skill")).Number(V.Skill);
			Out.Key(TEXT("skillGather")).Number(V.SkillGather);
			Out.Key(TEXT("skillTrade")).Number(V.SkillTrade);
			Out.Key(TEXT("skillCraft")).Number(V.SkillCraft);
			Out.Key(TEXT("buildBinding")).String(V.BuildBinding);
			Out.Key(TEXT("piecesPlaced")).Number(V.PiecesPlaced);
			Out.Key(TEXT("buildingsCompleted")).Number(V.BuildingsCompleted);
			Out.Key(TEXT("spots"));
			HashArray(Out, V.Spots);
			Out.Key(TEXT("scanX")).Number(V.ScanX);
			Out.Key(TEXT("scanY")).Number(V.ScanY);
			Out.Key(TEXT("workSession"));
			HashState(Out, V.WorkSession);
			Out.Key(TEXT("craftMissAt")).Number(V.CraftMissAt);
			Out.Key(TEXT("craftMissKind")).String(V.CraftMissKind);
			Out.Key(TEXT("craftMissCraftId")).String(V.CraftMissCraftId);
			Out.Key(TEXT("craftMissStampAt")).Number(V.CraftMissStampAt);
			Out.Key(TEXT("deliveries")).Number(V.Deliveries);
			Out.Key(TEXT("hungerAction"));
			HashState(Out, V.HungerAction);
			Out.Key(TEXT("knownStocks"));
			HashArray(Out, V.KnownStocks);
			Out.Key(TEXT("lastScan")).Number(V.LastScan);
			{
				TArray<int32> Cells = V.KnownCells.Array();
				Cells.Sort();
				Out.Key(TEXT("knownCells")).BeginArray(Cells.Num());
				for (const int32 C : Cells) Out.Number(C);
				Out.EndArray();
			}
			Out.Key(TEXT("cellCount")).Number(V.CellCount);
			Out.Key(TEXT("villagePhase")).String(V.VillagePhase);
			Out.Key(TEXT("goalSince")).Number(V.GoalSince);
			Out.Key(TEXT("phaseChangedAt"));
			HashOptional(Out, V.PhaseChangedAt);
			Out.Key(TEXT("workShift"));
			HashState(Out, V.WorkShift);
			Out.Key(TEXT("hasAlgoDecision")).Bool(V.bHasAlgoDecision);
			Out.Key(TEXT("algoDecision"));
			HashState(Out, V.AlgoDecision);
			Out.Key(TEXT("algoContext"));
			HashState(Out, V.AlgoContext);
			Out.Key(TEXT("algoInertiaKeep")).Bool(V.bAlgoInertiaKeep);
			Out.Key(TEXT("algoInertiaReason")).String(V.AlgoInertiaReason);
			Out.Key(TEXT("algoMappedGoal")).String(V.AlgoMappedGoal);

			Out.Key(TEXT("drinksTaken")).Number(V.DrinksTaken);
			Out.Key(TEXT("restsTaken")).Number(V.RestsTaken);
			Out.Key(TEXT("mealsTaken")).Number(V.MealsTaken);
			Out.Key(TEXT("socialsTaken")).Number(V.SocialsTaken);
			Out.Key(TEXT("relaxesTaken")).Number(V.RelaxesTaken);
			Out.Key(TEXT("sheltersTaken")).Number(V.SheltersTaken);
			Out.Key(TEXT("deedsHelped")).Number(V.DeedsHelped);
			Out.Key(TEXT("shelterResumeGoal")).String(V.ShelterResumeGoal);
			Out.Key(TEXT("shelterCooldownUntil")).Number(V.ShelterCooldownUntil);
			Out.Key(TEXT("simBudgetAccum")).Number(V.SimBudgetAccum);
			Out.Key(TEXT("hasSimBudgetAccum")).Bool(V.bHasSimBudgetAccum);

			Out.Key(TEXT("phenotype"));
			HashOptionalState(Out, V.Phenotype);
			Out.Key(TEXT("conditioning"));
			HashOptionalState(Out, V.Conditioning);
			Out.Key(TEXT("lifestyle"));
			HashOptionalState(Out, V.Lifestyle);
			Out.Key(TEXT("placeEntries"));
			HashArray(Out, V.PlaceEntries);
			Out.Key(TEXT("favoriteBuildingId")).String(V.FavoriteBuildingId);

			Out.Key(TEXT("relations"));
			HashNamedNumbers(Out, V.Relations);
			Out.Key(TEXT("people"));
			HashArray(Out, V.People);
			Out.Key(TEXT("tom"));
			HashArray(Out, V.Tom);
			Out.Key(TEXT("moodlets"));
			HashArray(Out, V.Moodlets);
			Out.Key(TEXT("lastTalk"));
			HashState(Out, V.LastTalk);
			Out.Key(TEXT("talkFatigue"));
			HashArray(Out, V.TalkFatigue);
			Out.Key(TEXT("talkWithId")).String(V.TalkWithId);
			Out.Key(TEXT("talkUntil")).Number(V.TalkUntil);
			Out.Key(TEXT("talkTurn")).Number(V.TalkTurn);
			Out.Key(TEXT("talkMaxTurns")).Number(V.TalkMaxTurns);
			Out.Key(TEXT("talkStarterId")).String(V.TalkStarterId);
			Out.Key(TEXT("talkNextAt")).Number(V.TalkNextAt);
			Out.Key(TEXT("talkChain")).Bool(V.bTalkChain);
			Out.Key(TEXT("talkAnchor")).Bool(V.bTalkAnchor);
			Out.Key(TEXT("socialSeekId")).String(V.SocialSeekId);
			Out.Key(TEXT("talksWithCompanion")).Number(V.TalksWithCompanion);
			Out.Key(TEXT("rumorsHeard")).Number(V.RumorsHeard);
			Out.Key(TEXT("rumorsShared")).Number(V.RumorsShared);

			Out.Key(TEXT("reputation")).Number(V.Reputation);
			Out.Key(TEXT("presence")).Number(V.Presence);
			Out.Key(TEXT("idleSeconds")).Number(V.IdleSeconds);

			// Champs arrives apres STATE_ORACLE_001 (nav-wiring-001, lifestyle-decision-001, premiere-pensee-001...),
			// ranges par state-fields-tidy-001.
			Out.Key(TEXT("pathFailStreak")).Number(V.PathFailStreak);
			Out.Key(TEXT("navRequestedAt")).Number(V.NavRequestedAt);
			Out.Key(TEXT("awaitingPath")).Bool(V.bAwaitingPath);
			Out.Key(TEXT("navPath"));
			HashArray(Out, V.NavPath);
			Out.Key(TEXT("navPathIndex")).Number(V.NavPathIndex);
			Out.Key(TEXT("doorQueueRole")).String(V.DoorQueueRole);
			Out.Key(TEXT("doorQueueRank")).Number(V.DoorQueueRank);
			Out.Key(TEXT("stuckTicks")).Number(V.StuckTicks);
			Out.Key(TEXT("hasLastMoveDir")).Bool(V.bHasLastMoveDir);
			Out.Key(TEXT("lastMoveDir"));
			HashState(Out, V.LastMoveDir);
			Out.Key(TEXT("hasHesitation")).Bool(V.bHasHesitation);
			Out.Key(TEXT("hesitationTimer")).Number(V.HesitationTimer);
			Out.Key(TEXT("hesitationCooldown")).Number(V.HesitationCooldown);
			Out.Key(TEXT("trafficTimer")).Number(V.TrafficTimer);
			Out.Key(TEXT("inventoryWood")).Number(V.InventoryWood);
			Out.Key(TEXT("gatheredWood")).Number(V.GatheredWood);
			Out.Key(TEXT("activitySince")).Number(V.ActivitySince);
			Out.Key(TEXT("goalExplain"));
			HashOptionalState(Out, V.GoalExplain);
			Out.Key(TEXT("streetDecision"));
			HashOptionalState(Out, V.StreetDecision);
			Out.Key(TEXT("hasHungerAction")).Bool(V.bHasHungerAction);
			Out.Key(TEXT("nocturnalIntent"));
			HashOptionalBool(Out, V.NocturnalIntent);
			Out.Key(TEXT("hasBuildBinding")).Bool(V.bHasBuildBinding);
			Out.Key(TEXT("hasSocialSeekId")).Bool(V.bHasSocialSeekId);
			Out.Key(TEXT("hasFailureStore")).Bool(V.bHasFailureStore);
			Out.Key(TEXT("skillCare")).Number(V.SkillCare);
			Out.Key(TEXT("nature"));
			HashOptionalState(Out, V.Nature);
			Out.Key(TEXT("gold"));
			HashOptional(Out, V.Gold);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FPlayerGoalChoice& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("goal")).String(V.Goal);
			Out.Key(TEXT("holds")).Number(V.Holds);
			Out.Key(TEXT("yields")).Number(V.Yields);
			Out.Key(TEXT("cedingFor")).String(V.CedingFor);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FPlayerRefusal& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("wanted")).String(V.Wanted);
			Out.Key(TEXT("reason")).String(V.Reason);
			Out.Key(TEXT("applied")).String(V.Applied);
			Out.Key(TEXT("day")).Number(V.Day);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FPlayerGoalOption& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("goal")).String(V.Goal);
			Out.Key(TEXT("score")).Number(V.Score);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const FVillage::FDeath& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("npcId")).String(V.NpcId);
			Out.Key(TEXT("cause")).String(V.Cause);
			Out.Key(TEXT("day")).Number(V.Day);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisWeatherBehavior::FSimWeather& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("rain")).Number(V.Rain);
			Out.Key(TEXT("snow")).Number(V.Snow);
			Out.Key(TEXT("wind")).Number(V.Wind);
			Out.Key(TEXT("cover")).Number(V.Cover);
			Out.Key(TEXT("season")).Number(static_cast<int32>(V.Season));
			Out.Key(TEXT("clearing")).Number(V.Clearing);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisBudget::FDirector& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("pressure")).Number(V.Pressure);
			Out.Key(TEXT("viewX")).Number(V.ViewX);
			Out.Key(TEXT("viewY")).Number(V.ViewY);
			Out.Key(TEXT("viewPinned")).Bool(V.bViewPinned);
			Out.EndObject();
		}

		void HashState(FStateWriter& Out, const AnastasisNav::FNavGrid& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("w")).Number(V.W);
			Out.Key(TEXT("h")).Number(V.H);
			// Grilles de la taille du monde : une empreinte de chaque, pas un nombre par case.
			AnastasisDigest::FFnv1a64 Blocked;
			Blocked.Bytes(V.Blocked.GetData(), V.Blocked.Num());
			AnastasisDigest::FFnv1a64 Cost;
			Cost.Bytes(reinterpret_cast<const uint8*>(V.MoveCost.GetData()), V.MoveCost.Num() * static_cast<int32>(sizeof(float)));
			Out.Key(TEXT("blocked")).String(AnastasisDigest::ToHex(Blocked.Hash));
			Out.Key(TEXT("moveCost")).String(AnastasisDigest::ToHex(Cost.Hash));
			Out.EndObject();
		}
		void HashState(FStateWriter& Out, const AnastasisPlanner::FUrgencySnapshot& V)
		{
			Out.BeginObject();
			Out.Key(TEXT("hydrationActive")).Bool(V.bHydrationActive);
			Out.Key(TEXT("hydrationLevel")).Number(V.HydrationLevel);
			Out.Key(TEXT("hydrationPlannedGap")).Number(V.HydrationPlannedGap);
			Out.Key(TEXT("housingActive")).Bool(V.bHousingActive);
			Out.Key(TEXT("housingLevel")).Number(V.HousingLevel);
			Out.Key(TEXT("housingDeficit")).Number(V.HousingDeficit);
			Out.Key(TEXT("housingVacancyActive")).Bool(V.bHousingVacancyActive);
			Out.Key(TEXT("accessActive")).Bool(V.bAccessActive);
			Out.Key(TEXT("accessLevel")).Number(V.AccessLevel);
			Out.Key(TEXT("accessResource")).String(V.AccessResource);
			Out.EndObject();
		}
	}

	uint64 FVillage::StateDigest() const
	{
		FStateWriter Out;
		Out.BeginObject();
		Out.Key(TEXT("playerPersonId")).String(PlayerPersonId);
		Out.Key(TEXT("playerDrive"));
		HashState(Out, PlayerDrive);
		Out.Key(TEXT("hasPlayerChoice")).Bool(bHasPlayerChoice);
		Out.Key(TEXT("playerChoice"));
		HashState(Out, PlayerChoice);
		Out.Key(TEXT("playerChoiceDirty")).Bool(bPlayerChoiceDirty);
		Out.Key(TEXT("hasPlayerRefusal")).Bool(bHasPlayerRefusal);
		Out.Key(TEXT("playerRefusal"));
		HashState(Out, PlayerRefusal);
		Out.Key(TEXT("playerOptions"));
		HashArray(Out, PlayerOptions);

		{
			TArray<int32> Touched;
			LiveTiles.GetKeys(Touched);
			Touched.Sort();
			Out.Key(TEXT("liveTiles")).BeginArray(Touched.Num());
			for (const int32 Index : Touched)
			{
				Out.BeginArray(2);
				Out.Number(Index);
				HashState(Out, LiveTiles[Index]);
				Out.EndArray();
			}
			Out.EndArray();
		}
		Out.Key(TEXT("regrownFood")).Number(static_cast<double>(RegrownFood));
		Out.Key(TEXT("recentVillageEmits")).BeginArray(RecentVillageEmits.Num());
		for (const double At : RecentVillageEmits) Out.Number(At);
		Out.EndArray();
		Out.Key(TEXT("villageRng")).Number(VillageRng.GetState());
		Out.Key(TEXT("nav"));
		HashState(Out, Nav);
		Out.Key(TEXT("terrainTravelCostEnabled")).Bool(bTerrainTravelCostEnabled);
		Out.Key(TEXT("materialCourierId")).String(MaterialCourierId);
		Out.Key(TEXT("navVersion")).Number(NavVersion);
		// `sim.traffic` et les sentiers qu'il fixe (ecart n°42) : la decroissance de minuit et l'effort de
		// defrichage en dependent, l'A* lit le cout des sentiers.
		Out.Key(TEXT("traffic")).BeginArray(Traffic.Num());
		for (const float T : Traffic) Out.Number(static_cast<double>(T));
		Out.EndArray();
		Out.Key(TEXT("roadEvolutionEnabled")).Bool(bRoadEvolutionEnabled);
		{
			TArray<int32> RoadKeys;
			Roads.GetKeys(RoadKeys);
			RoadKeys.Sort();
			Out.Key(TEXT("roads")).BeginArray(RoadKeys.Num());
			for (const int32 Index : RoadKeys)
			{
				const AnastasisTraffic::FRoadTile& Road = Roads[Index];
				Out.BeginArray(4);
				Out.Number(Index);
				Out.Number(static_cast<int32>(Road.Class));
				Out.Number(Road.BuiltDay);
				Out.Number(Road.TrafficAtBirth);
				Out.EndArray();
			}
			Out.EndArray();
			TArray<int32> EffortKeys;
			RoadEfforts.GetKeys(EffortKeys);
			EffortKeys.Sort();
			Out.Key(TEXT("roadEfforts")).BeginArray(EffortKeys.Num());
			for (const int32 Index : EffortKeys)
			{
				Out.BeginArray(2);
				Out.Number(Index);
				Out.Number(RoadEfforts[Index]);
				Out.EndArray();
			}
			Out.EndArray();
		}
		Out.Key(TEXT("settlement"));
		HashState(Out, Settlement);
		Out.Key(TEXT("marketDx"));
		HashOptional(Out, MarketDx);
		Out.Key(TEXT("marketDy"));
		HashOptional(Out, MarketDy);
		Out.Key(TEXT("hasColony")).Bool(bHasColony);
		Out.Key(TEXT("settlementClearRadius"));
		HashOptional(Out, SettlementClearRadius);
		Out.Key(TEXT("colonyTreasury"));
		HashOptional(Out, ColonyTreasury);
		// Le cache d'urgence est indexe par la seconde de jeu, pas par l'etat : deux villages egaux par
		// ailleurs peuvent decider autrement dans la meme seconde. C'est donc de l'etat.
		Out.Key(TEXT("urgencyBucket")).String(UrgencyBucket);
		Out.Key(TEXT("urgencyCache"));
		HashOptionalState(Out, UrgencyCache);
		Out.Key(TEXT("nextBuildingId")).Number(NextBuildingId);
		Out.Key(TEXT("deathLog"));
		HashArray(Out, DeathLog);
		Out.Key(TEXT("nextNpcId")).Number(NextNpcId);
		Out.Key(TEXT("now")).Number(Now);
		Out.Key(TEXT("weatherSeed")).Number(WeatherSeed);
		Out.Key(TEXT("weatherSeeded")).Bool(bWeatherSeeded);
		Out.Key(TEXT("forcedWeather")).Bool(bForcedWeather);
		Out.Key(TEXT("forcedWeatherValue"));
		HashState(Out, ForcedWeather);
		Out.Key(TEXT("tickWeather"));
		HashState(Out, TickWeather);
		Out.Key(TEXT("tickDailyRain")).Number(TickDailyRain);
		Out.Key(TEXT("budgetDirector"));
		HashState(Out, BudgetDirector);
		Out.Key(TEXT("simulationView")).Bool(bSimulationView);
		Out.Key(TEXT("mealReservations"));
		HashArray(Out, MealReservations);
		Out.Key(TEXT("foodSources"));
		HashArray(Out, FoodSources);
		Out.Key(TEXT("mealSeq")).Number(MealSeq);
		Out.Key(TEXT("reservationSweepAt")).Number(ReservationSweepAt);
		{
			TArray<int32> Soil;
			SoilWaterByTile.GetKeys(Soil);
			Soil.Sort();
			Out.Key(TEXT("soilWaterByTile")).BeginArray(Soil.Num());
			for (const int32 Index : Soil)
			{
				Out.BeginArray(2);
				Out.Number(Index).Number(SoilWaterByTile[Index]);
				Out.EndArray();
			}
			Out.EndArray();
		}
		Out.Key(TEXT("soilWaterEnabled")).Bool(bSoilWaterEnabled);
		Out.Key(TEXT("navService"));
		HashState(Out, NavService);
		{
			// Trafic par tuile (taille du monde) : une empreinte d'octets, pas un nombre par case.
			AnastasisDigest::FFnv1a64 TrafficHash;
			TrafficHash.Bytes(reinterpret_cast<const uint8*>(Traffic.GetData()), Traffic.Num() * static_cast<int32>(sizeof(int32)));
			Out.Key(TEXT("traffic")).String(AnastasisDigest::ToHex(TrafficHash.Hash));
		}
		Out.Key(TEXT("buildings"));
		HashArray(Out, Buildings.GetItems());
		Out.Key(TEXT("actors"));
		HashArray(Out, Actors.GetItems());
		Out.EndObject();
		return Out.Digest();
	}
}
