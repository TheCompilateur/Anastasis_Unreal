#include "Village/AnastasisVillage.h"

#include "Core/AnastasisStateArchive.h"
#include "Core/AnastasisStateDigest.h"

// STATE_ORACLE_001 -- l'empreinte d'ETAT du village, distincte de `Digest()`.
// SAVE_STATE_001 -- et, par le meme parcours, sa sauvegarde.
//
// `Digest()` est la projection de parite JS : son perimetre est fige pour que les vecteurs de la
// reference ne bougent pas, et il ne lit qu'une partie de l'etat (22 champs de FNpc sur ~110, 5 membres
// de FVillage sur 46, ni `sim.rng`, ni la meteo, ni le joueur). IRON_CRUSADE_001 a montre que deux
// villages d'empreinte egale peuvent avoir des futurs differents (une ecriture dans `Speed` ou `sim.rng`
// diverge apres 8 a 9 s simulees). `StateDigest()` lit TOUT l'etat qui decide du futur : c'est l'oracle
// des tests de determinisme et de non-ecriture. Il n'est pas un format de parite.
//
// Chaque `VisitState(FStateArchive&, T&)` ci-dessous decrit l'etat UNE fois (Core/AnastasisStateArchive.h) :
// le meme parcours hache (`StateDigest`, bits inchanges depuis STATE_ORACLE_001), ecrit la sauvegarde
// (`SaveState`) et la relit (`LoadState`). Un parcours ne lit l'etat qu'a travers `Ar` : en hachage il ne
// l'ecrit jamais, ce qui rend sur le `const_cast` des points d'entree const.
//
// Contrat tenu par tools/migration/check-state-fields.mjs : chaque champ d'une structure du registre
// tools/migration/state-fields.json est lu dans son `VisitState`, ou y est classe hors etat avec sa raison.
// Ajouter un champ sans le lire ici ni le classer fait echouer `finish`.

namespace AnastasisVillage
{
	namespace
	{
		using AnastasisArchive::FStateArchive;

		// Toutes les surcharges declarees d'abord : VisitStates / VisitOptionalState les trouvent par
		// recherche ordinaire (la plupart des types vivent hors de ce namespace, l'ADL n'y suffirait pas).
		void VisitState(FStateArchive& Ar, FPoint& V);
		void VisitState(FStateArchive& Ar, AnastasisNeeds::FNeeds& V);
		void VisitState(FStateArchive& Ar, AnastasisWorld::FTile& V);
		void VisitState(FStateArchive& Ar, AnastasisBuild::FSiteMaterials& V);
		void VisitState(FStateArchive& Ar, AnastasisPlanner::FStockSlot& V);
		void VisitState(FStateArchive& Ar, FBuilding& V);
		void VisitState(FStateArchive& Ar, FFoodSource& V);
		void VisitState(FStateArchive& Ar, FMealReservation& V);
		void VisitState(FStateArchive& Ar, FHungerAction& V);
		void VisitState(FStateArchive& Ar, FStockBelief& V);
		void VisitState(FStateArchive& Ar, FResourceSpot& V);
		void VisitState(FStateArchive& Ar, FLastTalk& V);
		void VisitState(FStateArchive& Ar, FTalkFatigue& V);
		void VisitState(FStateArchive& Ar, FWorkSession& V);
		void VisitState(FStateArchive& Ar, FInside& V);
		void VisitState(FStateArchive& Ar, AnastasisWorkShift::FWorkShift& V);
		void VisitState(FStateArchive& Ar, AnastasisNous::FDecision& V);
		void VisitState(FStateArchive& Ar, AnastasisNous::FFoodContext& V);
		void VisitState(FStateArchive& Ar, AnastasisGenome::FPhenotype& V);
		void VisitState(FStateArchive& Ar, AnastasisConditioning::FConditioning& V);
		void VisitState(FStateArchive& Ar, AnastasisLifestyle::FLifestyle& V);
		void VisitState(FStateArchive& Ar, FNpc::FPlaceEntry& V);
		void VisitState(FStateArchive& Ar, AnastasisBonds::FPersonRow& V);
		void VisitState(FStateArchive& Ar, AnastasisBonds::FTomEntry& V);
		void VisitState(FStateArchive& Ar, AnastasisBonds::FMoodlet& V);
		void VisitState(FStateArchive& Ar, FNpc& V);
		void VisitState(FStateArchive& Ar, FPlayerGoalChoice& V);
		void VisitState(FStateArchive& Ar, FPlayerRefusal& V);
		void VisitState(FStateArchive& Ar, FPlayerGoalOption& V);
		void VisitState(FStateArchive& Ar, FVillage::FDeath& V);
		void VisitState(FStateArchive& Ar, FVillage::FFamily& V);
		void VisitState(FStateArchive& Ar, FVillage::FHelpAnswer& V);
		void VisitState(FStateArchive& Ar, FVillage::FArrivalMember& V);
		void VisitState(FStateArchive& Ar, FVillage::FArrivalGroup& V);
		void VisitState(FStateArchive& Ar, FVillage::FWelcomeVote& V);
		void VisitState(FStateArchive& Ar, FVillage::FCouncil& V);
		void VisitState(FStateArchive& Ar, FVillage::FPlayerAsk& V);
		void VisitState(FStateArchive& Ar, AnastasisEpisodes::FEpisode& V);
		void VisitState(FStateArchive& Ar, AnastasisEpisodes::FChronicle& V);
		void VisitState(FStateArchive& Ar, AnastasisWeatherBehavior::FSimWeather& V);
		void VisitState(FStateArchive& Ar, AnastasisBudget::FDirector& V);
		void VisitState(FStateArchive& Ar, AnastasisNav::FNavGrid& V);
		void VisitState(FStateArchive& Ar, FGoalExplainEntry& V);
		void VisitState(FStateArchive& Ar, FGoalExplain& V);
		void VisitState(FStateArchive& Ar, FStreetDecision& V);
		void VisitState(FStateArchive& Ar, AnastasisNature::FNature& V);
		void VisitState(FStateArchive& Ar, AnastasisNavService::FNavJob& V);
		void VisitState(FStateArchive& Ar, AnastasisNavService::FNavCacheEntry& V);
		void VisitState(FStateArchive& Ar, AnastasisNavService::FNavService& V);
		void VisitState(FStateArchive& Ar, AnastasisPlanner::FUrgencySnapshot& V);
		void VisitState(FStateArchive& Ar, FBiographyEvent& V);
		void VisitState(FStateArchive& Ar, FBuildingBiography& V);

		template <typename T>
		void VisitStates(FStateArchive& Ar, TArray<T>& Items)
		{
			AnastasisArchive::VisitArray(Ar, Items, [](FStateArchive& A, T& Item) { VisitState(A, Item); });
		}

		template <typename T>
		void VisitOptionalState(FStateArchive& Ar, TOptional<T>& V)
		{
			AnastasisArchive::VisitOptional(Ar, V, [](FStateArchive& A, T& Item) { VisitState(A, Item); });
		}

		void VisitState(FStateArchive& Ar, FPoint& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("x")).Number(V.X);
			Ar.Key(TEXT("y")).Number(V.Y);
			Ar.EndObject();
		}

		void VisitOptionalNumber(FStateArchive& Ar, TOptional<double>& V)
		{
			AnastasisArchive::VisitOptional(Ar, V, [](FStateArchive& A, double& D) { A.Number(D); });
		}

		void VisitOptionalBool(FStateArchive& Ar, TOptional<bool>& V)
		{
			AnastasisArchive::VisitOptional(Ar, V, [](FStateArchive& A, bool& B) { A.Bool(B); });
		}

		void VisitStrings(FStateArchive& Ar, TArray<FString>& Items)
		{
			AnastasisArchive::VisitArray(Ar, Items, [](FStateArchive& A, FString& S) { A.String(S); });
		}

		/** Une paire `[a, b]` : un tableau de deux, comme le hacheur l'a toujours ecrit. */
		template <typename FVisit>
		void VisitPair(FStateArchive& Ar, FVisit&& Visit)
		{
			int32 Two = 2;
			Ar.BeginArray(Two);
			Ar.Expect(Two, 2, TEXT("paire"));
			if (Ar.Ok()) Visit(Ar);
			Ar.EndArray();
		}

		void VisitNamedNumbers(FStateArchive& Ar, TArray<TPair<FString, double>>& Items)
		{
			AnastasisArchive::VisitArray(Ar, Items, [](FStateArchive& A, TPair<FString, double>& P)
			{
				VisitPair(A, [&P](FStateArchive& B) { B.String(P.Key).Number(P.Value); });
			});
		}

		/**
		 * TMap a cle entiere, en paires `[cle, valeur]` par cle croissante. En lecture, la table est
		 * vide puis remplie dans cet ordre.
		 */
		template <typename TValue, typename FVisit>
		void VisitSortedIntMap(FStateArchive& Ar, TMap<int32, TValue>& Map, FVisit&& VisitValue)
		{
			TArray<int32> Keys;
			if (!Ar.IsLoading())
			{
				Map.GetKeys(Keys);
				Keys.Sort();
			}
			int32 Count = Keys.Num();
			Ar.BeginArray(Count);
			if (Ar.IsLoading())
			{
				if (!Ar.Ok()) return;
				Map.Reset();
			}
			for (int32 I = 0; I < Count && Ar.Ok(); ++I)
			{
				VisitPair(Ar, [&](FStateArchive& A)
				{
					int32 K = A.IsLoading() ? 0 : Keys[I];
					A.Number(K);
					if (!A.Ok()) return;
					TValue& Value = A.IsLoading() ? Map.Add(K) : Map[K];
					VisitValue(A, Value);
				});
			}
			Ar.EndArray();
		}

		/** Meme chose, cle chaine (ordre lexical de FString::operator<, celui de TArray::Sort). */
		template <typename TValue, typename FVisit>
		void VisitSortedStringMap(FStateArchive& Ar, TMap<FString, TValue>& Map, FVisit&& VisitValue)
		{
			TArray<FString> Keys;
			if (!Ar.IsLoading())
			{
				Map.GetKeys(Keys);
				Keys.Sort();
			}
			int32 Count = Keys.Num();
			Ar.BeginArray(Count);
			if (Ar.IsLoading())
			{
				if (!Ar.Ok()) return;
				Map.Reset();
			}
			for (int32 I = 0; I < Count && Ar.Ok(); ++I)
			{
				VisitPair(Ar, [&](FStateArchive& A)
				{
					FString K = A.IsLoading() ? FString() : Keys[I];
					A.String(K);
					if (!A.Ok()) return;
					TValue& Value = A.IsLoading() ? Map.Add(K) : Map[K];
					VisitValue(A, Value);
				});
			}
			Ar.EndArray();
		}

		void VisitState(FStateArchive& Ar, AnastasisNeeds::FNeeds& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("hunger")).Number(V.Hunger);
			Ar.Key(TEXT("energy")).Number(V.Energy);
			Ar.Key(TEXT("social")).Number(V.Social);
			Ar.Key(TEXT("leisure")).Number(V.Leisure);
			Ar.Key(TEXT("hygiene")).Number(V.Hygiene);
			Ar.Key(TEXT("thirst")).Number(V.Thirst);
			Ar.Key(TEXT("health")).Number(V.Health);
			Ar.Key(TEXT("morale")).Number(V.Morale);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisWorld::FTile& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("x")).Number(V.X);
			Ar.Key(TEXT("y")).Number(V.Y);
			Ar.Key(TEXT("type")).Number(V.Type);
			Ar.Key(TEXT("resource")).Number(V.Resource);
			Ar.Key(TEXT("amount")).Number(V.Amount);
			Ar.Key(TEXT("alt")).Number(V.Alt);
			Ar.Key(TEXT("shade")).Number(V.Shade);
			Ar.Key(TEXT("shore")).Number(V.Shore);
			Ar.Key(TEXT("wetness")).Number(V.Wetness);
			Ar.Key(TEXT("flowX")).Number(V.FlowX);
			Ar.Key(TEXT("flowZ")).Number(V.FlowZ);
			Ar.Key(TEXT("flowAmt")).Number(V.FlowAmt);
			Ar.Key(TEXT("cropId")).Number(V.CropId);
			Ar.Key(TEXT("fertility")).Number(V.Fertility);
			Ar.Key(TEXT("forestMargin")).Number(V.ForestMargin);
			Ar.Key(TEXT("hasForestMargin")).Bool(V.bHasForestMargin);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisBuild::FSiteMaterials& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("needWood")).Number(V.NeedWood);
			Ar.Key(TEXT("needStone")).Number(V.NeedStone);
			Ar.Key(TEXT("consumedWood")).Number(V.ConsumedWood);
			Ar.Key(TEXT("consumedStone")).Number(V.ConsumedStone);
			Ar.Key(TEXT("stockWood")).Number(V.StockWood);
			Ar.Key(TEXT("stockStone")).Number(V.StockStone);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisPlanner::FStockSlot& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("physical")).Number(V.Physical);
			Ar.Key(TEXT("reserved")).Number(V.Reserved);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FBuilding& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("id")).String(V.Id);
			Ar.Key(TEXT("type")).String(V.Type);
			Ar.Key(TEXT("x")).Number(V.X);
			Ar.Key(TEXT("y")).Number(V.Y);
			Ar.Key(TEXT("progress")).Number(V.Progress);
			Ar.Key(TEXT("createdDay")).Number(V.CreatedDay);
			Ar.Key(TEXT("owner")).String(V.Owner);
			Ar.Key(TEXT("housePhase")).Number(V.HousePhase);
			Ar.Key(TEXT("accessPoints"));
			VisitStates(Ar, V.AccessPoints);
			Ar.Key(TEXT("hasPlannerStock")).Bool(V.bHasPlannerStock);
			Ar.Key(TEXT("plannerStock"));
			AnastasisArchive::VisitArray(Ar, V.PlannerStock, [](FStateArchive& A, TPair<FString, AnastasisPlanner::FStockSlot>& Slot)
			{
				VisitPair(A, [&Slot](FStateArchive& B) { B.String(Slot.Key); VisitState(B, Slot.Value); });
			});
			Ar.Key(TEXT("foodPhysical")).Number(V.FoodPhysical);
			Ar.Key(TEXT("foodReserved")).Number(V.FoodReserved);
			Ar.Key(TEXT("laborToday")).Number(V.LaborToday);
			Ar.Key(TEXT("hasLaborToday")).Bool(V.bHasLaborToday);
			Ar.Key(TEXT("piecesPlaced")).Number(V.PiecesPlaced);
			Ar.Key(TEXT("hasMaterials")).Bool(V.bHasMaterials);
			Ar.Key(TEXT("materials"));
			VisitState(Ar, V.Materials);
			Ar.Key(TEXT("builderId")).String(V.BuilderId);
			// ecart n°52 : qui l'a ouvert de lui-meme, et pourquoi.
			Ar.Key(TEXT("openedById")).String(V.OpenedById);
			Ar.Key(TEXT("openCause")).String(V.OpenCause);
			Ar.Key(TEXT("workers"));
			AnastasisArchive::VisitArray(Ar, V.Workers, [](FStateArchive& A, TPair<FString, int32>& Worker)
			{
				VisitPair(A, [&Worker](FStateArchive& B) { B.String(Worker.Key).Number(Worker.Value); });
			});
			Ar.Key(TEXT("completedDay")).Number(V.CompletedDay);
			// ecart n°48 : la maison d'une famille.
			Ar.Key(TEXT("ownerFamilyId")).String(V.OwnerFamilyId);
			Ar.Key(TEXT("allowedBuilders"));
			VisitStrings(Ar, V.AllowedBuilders);
			Ar.Key(TEXT("askedIds"));
			VisitStrings(Ar, V.AskedIds);
			Ar.Key(TEXT("completedById")).String(V.CompletedById);
			Ar.Key(TEXT("vacantSinceDay")).Number(V.VacantSinceDay);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FFoodSource& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("tile")).Number(V.TileIndex);
			Ar.Key(TEXT("position"));
			VisitState(Ar, V.Position);
			Ar.Key(TEXT("initial")).Number(V.Initial);
			Ar.Key(TEXT("remaining")).Number(V.Remaining);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FMealReservation& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("id")).String(V.Id);
			Ar.Key(TEXT("npcId")).String(V.NpcId);
			Ar.Key(TEXT("buildingId")).String(V.BuildingId);
			Ar.Key(TEXT("source")).String(V.Source);
			Ar.Key(TEXT("amount")).Number(V.Amount);
			Ar.Key(TEXT("createdAt")).Number(V.CreatedAt);
			Ar.Key(TEXT("expiresAt")).Number(V.ExpiresAt);
			Ar.Key(TEXT("absoluteExpiresAt")).Number(V.AbsoluteExpiresAt);
			Ar.Key(TEXT("renewals")).Number(V.Renewals);
			Ar.Key(TEXT("lastProgressAt")).Number(V.LastProgressAt);
			Ar.Key(TEXT("lastDistance")).Number(V.LastDistance);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FHungerAction& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("state")).String(V.State);
			Ar.Key(TEXT("targetId")).String(V.TargetId);
			Ar.Key(TEXT("startedAt")).Number(V.StartedAt);
			Ar.Key(TEXT("lastFailure")).String(V.LastFailure);
			Ar.Key(TEXT("cooldownUntil")).Number(V.CooldownUntil);
			Ar.Key(TEXT("progress")).Number(V.Progress);
			Ar.Key(TEXT("reservationId")).String(V.ReservationId);
			Ar.Key(TEXT("sourceBuildingId")).String(V.SourceBuildingId);
			Ar.Key(TEXT("excludedType")).String(V.ExcludedType);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FStockBelief& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("key")).String(V.Key);
			Ar.Key(TEXT("resource")).String(V.Resource);
			Ar.Key(TEXT("buildingId")).String(V.BuildingId);
			Ar.Key(TEXT("kind")).String(V.Kind);
			Ar.Key(TEXT("x")).Number(V.X);
			Ar.Key(TEXT("y")).Number(V.Y);
			Ar.Key(TEXT("estimatedAmount")).Number(V.EstimatedAmount);
			Ar.Key(TEXT("confidence")).Number(V.Confidence);
			Ar.Key(TEXT("day")).Number(V.Day);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FResourceSpot& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("key")).String(V.Key);
			Ar.Key(TEXT("x")).Number(V.X);
			Ar.Key(TEXT("y")).Number(V.Y);
			Ar.Key(TEXT("resource")).String(V.Resource);
			Ar.Key(TEXT("amount")).Number(V.Amount);
			Ar.Key(TEXT("day")).Number(V.Day);
			Ar.Key(TEXT("hearsay")).Bool(V.bHearsay);
			Ar.Key(TEXT("sourceId")).String(V.SourceId);
			Ar.Key(TEXT("originalSourceId")).String(V.OriginalSourceId);
			Ar.Key(TEXT("hopCount")).Number(V.HopCount);
			Ar.Key(TEXT("receivedDay")).Number(V.ReceivedDay);
			Ar.Key(TEXT("receivedAt")).Number(V.ReceivedAt);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FLastTalk& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("valid")).Bool(V.bValid);
			Ar.Key(TEXT("withId")).String(V.WithId);
			Ar.Key(TEXT("at")).Number(V.At);
			Ar.Key(TEXT("refuse")).Bool(V.bRefuse);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FTalkFatigue& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("id")).String(V.Id);
			Ar.Key(TEXT("count")).Number(V.Count);
			Ar.Key(TEXT("at")).Number(V.At);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FWorkSession& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("active")).Bool(V.bActive);
			Ar.Key(TEXT("craftId")).String(V.CraftId);
			Ar.Key(TEXT("tileX")).Number(V.TileX);
			Ar.Key(TEXT("tileY")).Number(V.TileY);
			Ar.Key(TEXT("postIndex")).Number(V.PostIndex);
			Ar.Key(TEXT("arrivedAt")).Number(V.ArrivedAt);
			Ar.Key(TEXT("nextSwingAt")).Number(V.NextSwingAt);
			Ar.Key(TEXT("swingsDone")).Number(V.SwingsDone);
			Ar.Key(TEXT("lastSwingAt")).Number(V.LastSwingAt);
			Ar.Key(TEXT("buildingId")).String(V.BuildingId);
			Ar.Key(TEXT("actionAcc")).Number(V.ActionAcc);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FInside& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("active")).Bool(V.bActive);
			Ar.Key(TEXT("buildingId")).String(V.BuildingId);
			Ar.Key(TEXT("activity")).String(V.Activity);
			Ar.Key(TEXT("goal")).String(V.Goal);
			Ar.Key(TEXT("enteredAt")).Number(V.EnteredAt);
			Ar.Key(TEXT("until")).Number(V.Until);
			Ar.Key(TEXT("exitX")).Number(V.ExitX);
			Ar.Key(TEXT("exitY")).Number(V.ExitY);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisWorkShift::FWorkShift& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("state")).Number(V.State);
			Ar.Key(TEXT("goal")).String(V.Goal);
			Ar.Key(TEXT("startedAt")).Number(V.StartedAt);
			Ar.Key(TEXT("floorUntil")).Number(V.FloorUntil);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisNous::FDecision& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("type")).String(V.Type);
			Ar.Key(TEXT("targetId")).String(V.TargetId);
			Ar.Key(TEXT("score")).Number(V.Score);
			Ar.Key(TEXT("urgency")).Number(V.Urgency);
			Ar.Key(TEXT("reason")).String(V.Reason);
			Ar.Key(TEXT("createdAt")).Number(V.CreatedAt);
			Ar.Key(TEXT("expectedDuration")).Number(V.ExpectedDuration);
			Ar.Key(TEXT("raw")).Number(V.Raw);
			Ar.Key(TEXT("sourceBuildingId")).String(V.SourceBuildingId);
			Ar.Key(TEXT("travelSeconds")).Number(V.TravelSeconds);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisNous::FFoodContext& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("hunger")).Number(V.Hunger);
			Ar.Key(TEXT("inventoryFood")).Number(V.InventoryFood);
			Ar.Key(TEXT("believedFood")).Number(V.BelievedFood);
			Ar.Key(TEXT("bestSourceBuildingId")).String(V.BestSourceBuildingId);
			Ar.Key(TEXT("bestSourceDistance")).Number(V.BestSourceDistance);
			Ar.Key(TEXT("bestSourceEstimated")).Number(V.BestSourceEstimated);
			Ar.Key(TEXT("bestSourceConfidence")).Number(V.BestSourceConfidence);
			Ar.Key(TEXT("certainty")).Number(V.Certainty);
			Ar.Key(TEXT("dangerNear")).Bool(V.bDangerNear);
			Ar.Key(TEXT("gold")).Number(V.Gold);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisGenome::FPhenotype& V)
		{
			Ar.BeginObject();
			int32 NumLoci = AnastasisGenome::NumLoci;
			Ar.Key(TEXT("loci")).BeginArray(NumLoci);
			Ar.Expect(NumLoci, AnastasisGenome::NumLoci, TEXT("loci"));
			for (int32 I = 0; I < AnastasisGenome::NumLoci && Ar.Ok(); ++I) Ar.Number(V.Loci[I]);
			Ar.EndArray();
			Ar.Key(TEXT("hydrationLossMultiplier")).Number(V.HydrationLossMultiplier);
			Ar.Key(TEXT("heatDissipationEfficiency")).Number(V.HeatDissipationEfficiency);
			Ar.Key(TEXT("metabolicDemandMultiplier")).Number(V.MetabolicDemandMultiplier);
			Ar.Key(TEXT("metabolicPeakRecoveryMultiplier")).Number(V.MetabolicPeakRecoveryMultiplier);
			Ar.Key(TEXT("fatigueRecoveryMultiplier")).Number(V.FatigueRecoveryMultiplier);
			Ar.Key(TEXT("fatigueRecoveryStrainCost")).Number(V.FatigueRecoveryStrainCost);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisConditioning::FConditioning& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("version")).Number(V.Version);
			Ar.Key(TEXT("workConditioning")).Number(V.WorkConditioning);
			Ar.Key(TEXT("fatigueAdaptation")).Number(V.FatigueAdaptation);
			Ar.Key(TEXT("recoveryConditioning")).Number(V.RecoveryConditioning);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisLifestyle::FLifestyle& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("id")).String(V.Id);
			Ar.Key(TEXT("sinceDay")).Number(V.SinceDay);
			Ar.Key(TEXT("rhythmScore")).Number(V.RhythmScore);
			Ar.Key(TEXT("lastNotedDay")).Number(V.LastNotedDay);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FNpc::FPlaceEntry& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("buildingId")).String(V.BuildingId);
			Ar.Key(TEXT("type")).String(V.Type);
			Ar.Key(TEXT("score")).Number(V.Score);
			Ar.Key(TEXT("work")).Number(V.Work);
			Ar.Key(TEXT("social")).Number(V.Social);
			Ar.Key(TEXT("home")).Number(V.Home);
			Ar.Key(TEXT("talk")).Number(V.Talk);
			Ar.Key(TEXT("drink")).Number(V.Drink);
			Ar.Key(TEXT("activity")).Number(V.Activity);
			Ar.Key(TEXT("crisis")).Number(V.Crisis);
			Ar.Key(TEXT("decayDay")).Number(V.DecayDay);
			Ar.Key(TEXT("lastDay")).Number(V.LastDay);
			Ar.Key(TEXT("lifestyle"));
			VisitNamedNumbers(Ar, V.Lifestyle);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisBonds::FPersonRow& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("id")).String(V.Id);
			Ar.Key(TEXT("trust")).Number(V.Trust);
			Ar.Key(TEXT("tag")).Number(V.Tag);
			Ar.Key(TEXT("day")).Number(V.Day);
			Ar.Key(TEXT("meets")).Number(V.Meets);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisBonds::FTomEntry& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("id")).String(V.Id);
			Ar.Key(TEXT("estimatedGoal")).String(V.EstimatedGoal);
			Ar.Key(TEXT("attitude")).Number(V.Attitude);
			Ar.Key(TEXT("confidence")).Number(V.Confidence);
			Ar.Key(TEXT("day")).Number(V.Day);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisBonds::FMoodlet& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("id")).String(V.Id);
			Ar.Key(TEXT("at")).Number(V.At);
			Ar.Key(TEXT("until")).Number(V.Until);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FGoalExplainEntry& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("goal")).String(V.Goal);
			Ar.Key(TEXT("score")).Number(V.Score);
			Ar.Key(TEXT("cause")).String(V.Cause);
			Ar.Key(TEXT("causeKey")).String(V.CauseKey);
			Ar.Key(TEXT("causeValue")).Number(V.CauseValue);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FGoalExplain& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("at")).Number(V.At);
			Ar.Key(TEXT("goal")).String(V.Goal);
			Ar.Key(TEXT("top"));
			VisitStates(Ar, V.Top);
			Ar.Key(TEXT("line")).String(V.Line);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FStreetDecision& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("at")).Number(V.At);
			Ar.Key(TEXT("until")).Number(V.Until);
			Ar.Key(TEXT("goal")).String(V.Goal);
			Ar.Key(TEXT("from")).String(V.From);
			Ar.Key(TEXT("margin")).Number(V.Margin);
			Ar.Key(TEXT("changed")).Bool(V.bChanged);
			Ar.Key(TEXT("tight")).Bool(V.bTight);
			Ar.Key(TEXT("cause")).String(V.Cause);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisNature::FNature& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("corps")).Number(V.Corps);
			Ar.Key(TEXT("esprit")).Number(V.Esprit);
			Ar.Key(TEXT("coeur")).Number(V.Coeur);
			Ar.Key(TEXT("qualities"));
			VisitStrings(Ar, V.Qualities);
			Ar.Key(TEXT("flaws"));
			VisitStrings(Ar, V.Flaws);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisNavService::FNavJob& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("actorId")).String(V.ActorId);
			Ar.Key(TEXT("start"));
			VisitState(Ar, V.Start);
			Ar.Key(TEXT("destination"));
			VisitState(Ar, V.Destination);
			Ar.Key(TEXT("goalKey")).String(V.GoalKey);
			Ar.Key(TEXT("priority")).Number(V.Priority);
			Ar.Key(TEXT("allowBlockedTarget")).Bool(V.bAllowBlockedTarget);
			Ar.Key(TEXT("requestedAt")).Number(V.RequestedAt);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisNavService::FNavCacheEntry& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("path"));
			VisitStates(Ar, V.Path);
			Ar.Key(TEXT("navVersion")).Number(V.NavVersion);
			Ar.Key(TEXT("storedAt")).Number(V.StoredAt);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisNavService::FNavService& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("queue"));
			VisitStates(Ar, V.Queue);
			// Le cache garde l'ordre d'insertion (ses evictions suivent cet ordre) : on le lit tel quel.
			// En lecture, le cache est vide puis rempli dans l'ordre sauve (`Set` ajoute une cle neuve a la fin).
			{
				const TArray<FString> Keys = Ar.IsLoading() ? TArray<FString>() : V.Cache.GetKeys();
				int32 Count = Keys.Num();
				Ar.Key(TEXT("cache")).BeginArray(Count);
				if (Ar.IsLoading() && Ar.Ok()) V.Cache.Empty();
				for (int32 I = 0; I < Count && Ar.Ok(); ++I)
				{
					VisitPair(Ar, [&](FStateArchive& A)
					{
						FString Key = A.IsLoading() ? FString() : Keys[I];
						AnastasisNavService::FNavCacheEntry Entry = A.IsLoading() ? AnastasisNavService::FNavCacheEntry() : V.Cache.GetEntry(I);
						A.String(Key);
						VisitState(A, Entry);
						if (A.IsLoading() && A.Ok()) V.Cache.Set(Key, Entry);
					});
				}
				Ar.EndArray();
			}
			Ar.Key(TEXT("pendingByActor"));
			VisitSortedStringMap(Ar, V.PendingByActor, [](FStateArchive& A, int32& N) { A.Number(N); });
			Ar.Key(TEXT("calcThisTick")).Number(V.CalcThisTick);
			Ar.Key(TEXT("maxCalcs")).Number(V.MaxCalcs);
			Ar.Key(TEXT("cacheTtl")).Number(V.CacheTtl);
			Ar.Key(TEXT("lastSweepAt")).Number(V.LastSweepAt);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FNpc& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("id")).String(V.Id);
			Ar.Key(TEXT("x")).Number(V.X);
			Ar.Key(TEXT("y")).Number(V.Y);
			Ar.Key(TEXT("speed")).Number(V.Speed);
			Ar.Key(TEXT("needs"));
			VisitState(Ar, V.Needs);
			Ar.Key(TEXT("goal")).String(V.Goal);
			Ar.Key(TEXT("activity")).String(V.Activity);
			Ar.Key(TEXT("hasTarget")).Bool(V.bHasTarget);
			Ar.Key(TEXT("target"));
			VisitState(Ar, V.Target);
			Ar.Key(TEXT("workTimer")).Number(V.WorkTimer);
			Ar.Key(TEXT("aiThinkAt")).Number(V.AiThinkAt);
			Ar.Key(TEXT("failedActions")).Number(V.FailedActions);
			Ar.Key(TEXT("homeId")).String(V.HomeId);
			Ar.Key(TEXT("shelterId")).String(V.ShelterId);
			// ecart n°44 : l'identite.
			Ar.Key(TEXT("name")).String(V.Name);
			Ar.Key(TEXT("familyName")).String(V.FamilyName);
			Ar.Key(TEXT("gender")).String(V.Gender);
			Ar.Key(TEXT("age")).Number(V.Age);
			Ar.Key(TEXT("familyId")).String(V.FamilyId);
			Ar.Key(TEXT("kinRole")).String(V.KinRole);
			Ar.Key(TEXT("chronicle"));
			VisitState(Ar, V.Chronicle);
			Ar.Key(TEXT("inside"));
			VisitState(Ar, V.Inside);
			Ar.Key(TEXT("doorStuckAt")).Number(V.DoorStuckAt);
			Ar.Key(TEXT("doorApproachAt")).Number(V.DoorApproachAt);

			Ar.Key(TEXT("path"));
			VisitStates(Ar, V.Path);
			Ar.Key(TEXT("pathStep")).Number(V.PathStep);
			Ar.Key(TEXT("hasPathGoal")).Bool(V.bHasPathGoal);
			Ar.Key(TEXT("pathGoal"));
			VisitState(Ar, V.PathGoal);
			Ar.Key(TEXT("pathFailed")).Bool(V.bPathFailed);
			Ar.Key(TEXT("pathCooldown")).Number(V.PathCooldown);
			Ar.Key(TEXT("navTargetKey")).String(V.NavTargetKey);
			Ar.Key(TEXT("navVersion")).Number(V.NavVersion);
			Ar.Key(TEXT("destBuildingId")).String(V.DestBuildingId);
			Ar.Key(TEXT("stuckTimer")).Number(V.StuckTimer);
			Ar.Key(TEXT("stuckStage")).Number(V.StuckStage);
			// `actor.trafficTimer` : decide quand tombe le prochain passage (relay-settlement-001).
			Ar.Key(TEXT("trafficTimer")).Number(V.TrafficTimer);

			Ar.Key(TEXT("inventoryFood")).Number(V.InventoryFood);
			Ar.Key(TEXT("materialCarry")).Number(V.MaterialCarry);
			Ar.Key(TEXT("materialResource")).Number(V.MaterialResource);
			Ar.Key(TEXT("materialSourceIndex")).Number(V.MaterialSourceIndex);
			Ar.Key(TEXT("materialRetryAt")).Number(V.MaterialRetryAt);
			Ar.Key(TEXT("materialsDelivered")).Number(V.MaterialsDelivered);
			Ar.Key(TEXT("gatheredFood")).Number(V.GatheredFood);
			Ar.Key(TEXT("deliveredFood")).Number(V.DeliveredFood);
			Ar.Key(TEXT("foodSourceIndex")).Number(V.FoodSourceIndex);
			Ar.Key(TEXT("knownFoodSources"));
			VisitSortedIntMap(Ar, V.KnownFoodSources, [](FStateArchive& A, int32& N) { A.Number(N); });

			Ar.Key(TEXT("jobId")).String(V.JobId);
			Ar.Key(TEXT("workplaceId")).String(V.WorkplaceId);
			Ar.Key(TEXT("traitIndex")).Number(V.TraitIndex);
			Ar.Key(TEXT("skill")).Number(V.Skill);
			Ar.Key(TEXT("skillGather")).Number(V.SkillGather);
			Ar.Key(TEXT("skillTrade")).Number(V.SkillTrade);
			Ar.Key(TEXT("skillCraft")).Number(V.SkillCraft);
			Ar.Key(TEXT("buildBinding")).String(V.BuildBinding);
			Ar.Key(TEXT("piecesPlaced")).Number(V.PiecesPlaced);
			Ar.Key(TEXT("buildingsCompleted")).Number(V.BuildingsCompleted);
			Ar.Key(TEXT("spots"));
			VisitStates(Ar, V.Spots);
			Ar.Key(TEXT("scanX")).Number(V.ScanX);
			Ar.Key(TEXT("scanY")).Number(V.ScanY);
			Ar.Key(TEXT("workSession"));
			VisitState(Ar, V.WorkSession);
			Ar.Key(TEXT("craftMissAt")).Number(V.CraftMissAt);
			Ar.Key(TEXT("craftMissKind")).String(V.CraftMissKind);
			Ar.Key(TEXT("craftMissCraftId")).String(V.CraftMissCraftId);
			Ar.Key(TEXT("craftMissStampAt")).Number(V.CraftMissStampAt);
			Ar.Key(TEXT("deliveries")).Number(V.Deliveries);
			Ar.Key(TEXT("hungerAction"));
			VisitState(Ar, V.HungerAction);
			Ar.Key(TEXT("knownStocks"));
			VisitStates(Ar, V.KnownStocks);
			Ar.Key(TEXT("lastScan")).Number(V.LastScan);
			{
				// TSet : cellules par ordre croissant ; en lecture, l'ensemble est refait dans cet ordre.
				TArray<int32> Cells;
				if (!Ar.IsLoading())
				{
					Cells = V.KnownCells.Array();
					Cells.Sort();
				}
				Ar.Key(TEXT("knownCells"));
				AnastasisArchive::VisitArray(Ar, Cells, [](FStateArchive& A, int32& C) { A.Number(C); });
				if (Ar.IsLoading() && Ar.Ok())
				{
					V.KnownCells.Reset();
					for (const int32 C : Cells) V.KnownCells.Add(C);
				}
			}
			Ar.Key(TEXT("cellCount")).Number(V.CellCount);
			Ar.Key(TEXT("villagePhase")).String(V.VillagePhase);
			Ar.Key(TEXT("goalSince")).Number(V.GoalSince);
			Ar.Key(TEXT("phaseChangedAt"));
			VisitOptionalNumber(Ar, V.PhaseChangedAt);
			Ar.Key(TEXT("workShift"));
			VisitState(Ar, V.WorkShift);
			Ar.Key(TEXT("hasAlgoDecision")).Bool(V.bHasAlgoDecision);
			Ar.Key(TEXT("algoDecision"));
			VisitState(Ar, V.AlgoDecision);
			Ar.Key(TEXT("algoContext"));
			VisitState(Ar, V.AlgoContext);
			Ar.Key(TEXT("algoInertiaKeep")).Bool(V.bAlgoInertiaKeep);
			Ar.Key(TEXT("algoInertiaReason")).String(V.AlgoInertiaReason);
			Ar.Key(TEXT("algoMappedGoal")).String(V.AlgoMappedGoal);

			Ar.Key(TEXT("drinksTaken")).Number(V.DrinksTaken);
			Ar.Key(TEXT("restsTaken")).Number(V.RestsTaken);
			Ar.Key(TEXT("mealsTaken")).Number(V.MealsTaken);
			Ar.Key(TEXT("socialsTaken")).Number(V.SocialsTaken);
			Ar.Key(TEXT("relaxesTaken")).Number(V.RelaxesTaken);
			Ar.Key(TEXT("sheltersTaken")).Number(V.SheltersTaken);
			Ar.Key(TEXT("deedsHelped")).Number(V.DeedsHelped);
			Ar.Key(TEXT("shelterResumeGoal")).String(V.ShelterResumeGoal);
			Ar.Key(TEXT("shelterCooldownUntil")).Number(V.ShelterCooldownUntil);
			Ar.Key(TEXT("simBudgetAccum")).Number(V.SimBudgetAccum);
			Ar.Key(TEXT("hasSimBudgetAccum")).Bool(V.bHasSimBudgetAccum);

			Ar.Key(TEXT("phenotype"));
			VisitOptionalState(Ar, V.Phenotype);
			Ar.Key(TEXT("conditioning"));
			VisitOptionalState(Ar, V.Conditioning);
			Ar.Key(TEXT("lifestyle"));
			VisitOptionalState(Ar, V.Lifestyle);
			Ar.Key(TEXT("placeEntries"));
			VisitStates(Ar, V.PlaceEntries);
			Ar.Key(TEXT("favoriteBuildingId")).String(V.FavoriteBuildingId);

			Ar.Key(TEXT("relations"));
			VisitNamedNumbers(Ar, V.Relations);
			Ar.Key(TEXT("people"));
			VisitStates(Ar, V.People);
			Ar.Key(TEXT("tom"));
			VisitStates(Ar, V.Tom);
			Ar.Key(TEXT("moodlets"));
			VisitStates(Ar, V.Moodlets);
			Ar.Key(TEXT("lastTalk"));
			VisitState(Ar, V.LastTalk);
			Ar.Key(TEXT("talkFatigue"));
			VisitStates(Ar, V.TalkFatigue);
			Ar.Key(TEXT("talkWithId")).String(V.TalkWithId);
			Ar.Key(TEXT("talkUntil")).Number(V.TalkUntil);
			Ar.Key(TEXT("talkTurn")).Number(V.TalkTurn);
			Ar.Key(TEXT("talkMaxTurns")).Number(V.TalkMaxTurns);
			Ar.Key(TEXT("talkStarterId")).String(V.TalkStarterId);
			Ar.Key(TEXT("talkNextAt")).Number(V.TalkNextAt);
			Ar.Key(TEXT("talkChain")).Bool(V.bTalkChain);
			Ar.Key(TEXT("talkAnchor")).Bool(V.bTalkAnchor);
			Ar.Key(TEXT("socialSeekId")).String(V.SocialSeekId);
			Ar.Key(TEXT("talksWithCompanion")).Number(V.TalksWithCompanion);
			Ar.Key(TEXT("rumorsHeard")).Number(V.RumorsHeard);
			Ar.Key(TEXT("rumorsShared")).Number(V.RumorsShared);

			Ar.Key(TEXT("reputation")).Number(V.Reputation);
			Ar.Key(TEXT("presence")).Number(V.Presence);
			Ar.Key(TEXT("idleSeconds")).Number(V.IdleSeconds);

			// Champs arrives apres STATE_ORACLE_001 (nav-wiring-001, lifestyle-decision-001, premiere-pensee-001...),
			// ranges par state-fields-tidy-001.
			Ar.Key(TEXT("pathFailStreak")).Number(V.PathFailStreak);
			Ar.Key(TEXT("navRequestedAt")).Number(V.NavRequestedAt);
			Ar.Key(TEXT("awaitingPath")).Bool(V.bAwaitingPath);
			Ar.Key(TEXT("navPath"));
			VisitStates(Ar, V.NavPath);
			Ar.Key(TEXT("navPathIndex")).Number(V.NavPathIndex);
			Ar.Key(TEXT("doorQueueRole")).String(V.DoorQueueRole);
			Ar.Key(TEXT("doorQueueRank")).Number(V.DoorQueueRank);
			Ar.Key(TEXT("stuckTicks")).Number(V.StuckTicks);
			Ar.Key(TEXT("hasLastMoveDir")).Bool(V.bHasLastMoveDir);
			Ar.Key(TEXT("lastMoveDir"));
			VisitState(Ar, V.LastMoveDir);
			Ar.Key(TEXT("hasHesitation")).Bool(V.bHasHesitation);
			Ar.Key(TEXT("hesitationTimer")).Number(V.HesitationTimer);
			Ar.Key(TEXT("hesitationCooldown")).Number(V.HesitationCooldown);
			Ar.Key(TEXT("trafficTimer")).Number(V.TrafficTimer);
			Ar.Key(TEXT("inventoryWood")).Number(V.InventoryWood);
			Ar.Key(TEXT("gatheredWood")).Number(V.GatheredWood);
			Ar.Key(TEXT("activitySince")).Number(V.ActivitySince);
			Ar.Key(TEXT("goalExplain"));
			VisitOptionalState(Ar, V.GoalExplain);
			Ar.Key(TEXT("streetDecision"));
			VisitOptionalState(Ar, V.StreetDecision);
			Ar.Key(TEXT("hasHungerAction")).Bool(V.bHasHungerAction);
			Ar.Key(TEXT("nocturnalIntent"));
			VisitOptionalBool(Ar, V.NocturnalIntent);
			Ar.Key(TEXT("hasBuildBinding")).Bool(V.bHasBuildBinding);
			Ar.Key(TEXT("hasSocialSeekId")).Bool(V.bHasSocialSeekId);
			Ar.Key(TEXT("hasFailureStore")).Bool(V.bHasFailureStore);
			Ar.Key(TEXT("skillCare")).Number(V.SkillCare);
			Ar.Key(TEXT("nature"));
			VisitOptionalState(Ar, V.Nature);
			Ar.Key(TEXT("gold"));
			VisitOptionalNumber(Ar, V.Gold);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FPlayerGoalChoice& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("goal")).String(V.Goal);
			Ar.Key(TEXT("holds")).Number(V.Holds);
			Ar.Key(TEXT("yields")).Number(V.Yields);
			Ar.Key(TEXT("cedingFor")).String(V.CedingFor);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FPlayerRefusal& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("wanted")).String(V.Wanted);
			Ar.Key(TEXT("reason")).String(V.Reason);
			Ar.Key(TEXT("applied")).String(V.Applied);
			Ar.Key(TEXT("day")).Number(V.Day);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FPlayerGoalOption& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("goal")).String(V.Goal);
			Ar.Key(TEXT("score")).Number(V.Score);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FVillage::FDeath& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("npcId")).String(V.NpcId);
			Ar.Key(TEXT("cause")).String(V.Cause);
			Ar.Key(TEXT("day")).Number(V.Day);
			Ar.EndObject();
		}

		// ecart n°47 : un souvenir, et la memoire qui les tient.
		void VisitState(FStateArchive& Ar, AnastasisEpisodes::FEpisode& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("id")).String(V.Id);
			Ar.Key(TEXT("rootId")).String(V.RootId);
			Ar.Key(TEXT("kind")).String(V.Kind);
			Ar.Key(TEXT("day")).Number(V.Day);
			Ar.Key(TEXT("tone")).Number(V.Tone);
			Ar.Key(TEXT("weight")).Number(V.Weight);
			Ar.Key(TEXT("aboutId")).String(V.AboutId);
			Ar.Key(TEXT("aboutName")).String(V.AboutName);
			Ar.Key(TEXT("x")).Number(V.X);
			Ar.Key(TEXT("y")).Number(V.Y);
			Ar.Key(TEXT("detail")).Number(V.Detail);
			Ar.Key(TEXT("note")).String(V.Note);
			Ar.Key(TEXT("hops")).Number(V.Hops);
			Ar.Key(TEXT("firsthand")).Bool(V.bFirsthand);
			Ar.Key(TEXT("sourceId")).String(V.SourceId);
			Ar.Key(TEXT("sourceEpisodeId")).String(V.SourceEpisodeId);
			Ar.Key(TEXT("originalSourceId")).String(V.OriginalSourceId);
			Ar.Key(TEXT("confidence")).Number(V.Confidence);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisEpisodes::FChronicle& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("events"));
			VisitStates(Ar, V.Events);
			Ar.Key(TEXT("lived")).Number(V.Lived);
			Ar.Key(TEXT("told")).Number(V.Told);
			Ar.Key(TEXT("heard")).Number(V.Heard);
			Ar.Key(TEXT("nextId")).Number(V.NextId);
			Ar.EndObject();
		}

		// ecart n°48 : une demande d'aide et sa reponse.
		void VisitState(FStateArchive& Ar, FVillage::FHelpAnswer& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("day")).Number(V.Day);
			Ar.Key(TEXT("fromId")).String(V.FromId);
			Ar.Key(TEXT("toId")).String(V.ToId);
			Ar.Key(TEXT("siteId")).String(V.SiteId);
			Ar.Key(TEXT("accepted")).Bool(V.bAccepted);
			Ar.Key(TEXT("reason")).String(V.Reason);
			Ar.EndObject();
		}

		// ecart n°44 : un foyer pose par l'hote.
		void VisitState(FStateArchive& Ar, FVillage::FFamily& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("id")).String(V.Id);
			Ar.Key(TEXT("name")).String(V.Name);
			Ar.Key(TEXT("adults"));
			VisitStrings(Ar, V.Adults);
			Ar.Key(TEXT("dependents"));
			VisitStrings(Ar, V.Dependents);
			Ar.Key(TEXT("homeId")).String(V.HomeId);
			// ecart n°53 : un groupe d'arrivants en attente, ou reparti.
			Ar.Key(TEXT("guest")).Bool(V.bGuest);
			Ar.Key(TEXT("left")).Bool(V.bLeft);
			Ar.Key(TEXT("arrivedDay")).Number(V.ArrivedDay);
			Ar.Key(TEXT("cause")).String(V.Cause);
			Ar.Key(TEXT("origin")).String(V.Origin);
			Ar.EndObject();
		}

		// ecart n°53 : les arrivants que l'hote tient prets, et le conseil du soir.
		void VisitState(FStateArchive& Ar, FVillage::FArrivalMember& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("name")).String(V.Name);
			Ar.Key(TEXT("gender")).String(V.Gender);
			Ar.Key(TEXT("age")).Number(V.Age);
			Ar.Key(TEXT("kinRole")).String(V.KinRole);
			Ar.Key(TEXT("adult")).Bool(V.bAdult);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FVillage::FArrivalGroup& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("familyName")).String(V.FamilyName);
			Ar.Key(TEXT("members"));
			VisitStates(Ar, V.Members);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FVillage::FWelcomeVote& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("voterId")).String(V.VoterId);
			Ar.Key(TEXT("yes")).Bool(V.bYes);
			Ar.Key(TEXT("reason")).String(V.Reason);
			Ar.Key(TEXT("score")).Number(V.Score);
			Ar.Key(TEXT("terms")).String(V.Terms);
			Ar.Key(TEXT("weight")).Number(V.Weight);
			Ar.EndObject();
		}

		// ecart n°54 : une demande d'aide faite au joueur.
		void VisitState(FStateArchive& Ar, FVillage::FPlayerAsk& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("day")).Number(V.Day);
			Ar.Key(TEXT("fromId")).String(V.FromId);
			Ar.Key(TEXT("siteId")).String(V.SiteId);
			Ar.Key(TEXT("answered")).Bool(V.bAnswered);
			Ar.Key(TEXT("accepted")).Bool(V.bAccepted);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FVillage::FCouncil& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("day")).Number(V.Day);
			Ar.Key(TEXT("familyId")).String(V.FamilyId);
			Ar.Key(TEXT("cause")).String(V.Cause);
			Ar.Key(TEXT("votes"));
			VisitStates(Ar, V.Votes);
			Ar.Key(TEXT("accepted")).Bool(V.bAccepted);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisWeatherBehavior::FSimWeather& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("rain")).Number(V.Rain);
			Ar.Key(TEXT("snow")).Number(V.Snow);
			Ar.Key(TEXT("wind")).Number(V.Wind);
			Ar.Key(TEXT("cover")).Number(V.Cover);
			Ar.Key(TEXT("season")).Number(V.Season);
			Ar.Key(TEXT("clearing")).Number(V.Clearing);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisBudget::FDirector& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("pressure")).Number(V.Pressure);
			Ar.Key(TEXT("viewX")).Number(V.ViewX);
			Ar.Key(TEXT("viewY")).Number(V.ViewY);
			Ar.Key(TEXT("viewPinned")).Bool(V.bViewPinned);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, AnastasisNav::FNavGrid& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("w")).Number(V.W);
			Ar.Key(TEXT("h")).Number(V.H);
			// Grilles de la taille du monde : le hacheur n'en garde qu'une empreinte, la sauvegarde chaque octet.
			Ar.Key(TEXT("blocked")).Blob(V.Blocked);
			Ar.Key(TEXT("moveCost")).Blob(V.MoveCost);
			Ar.EndObject();
		}
		void VisitState(FStateArchive& Ar, AnastasisPlanner::FUrgencySnapshot& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("hydrationActive")).Bool(V.bHydrationActive);
			Ar.Key(TEXT("hydrationLevel")).Number(V.HydrationLevel);
			Ar.Key(TEXT("hydrationPlannedGap")).Number(V.HydrationPlannedGap);
			Ar.Key(TEXT("housingActive")).Bool(V.bHousingActive);
			Ar.Key(TEXT("housingLevel")).Number(V.HousingLevel);
			Ar.Key(TEXT("housingDeficit")).Number(V.HousingDeficit);
			Ar.Key(TEXT("housingVacancyActive")).Bool(V.bHousingVacancyActive);
			Ar.Key(TEXT("accessActive")).Bool(V.bAccessActive);
			Ar.Key(TEXT("accessLevel")).Number(V.AccessLevel);
			Ar.Key(TEXT("accessResource")).String(V.AccessResource);
			Ar.EndObject();
		}
	}

	namespace
	{
		void VisitState(FStateArchive& Ar, FBiographyEvent& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("day")).Number(V.Day);
			Ar.Key(TEXT("kind")).Number(V.Kind);
			Ar.Key(TEXT("detail")).String(V.Detail);
			Ar.EndObject();
		}

		void VisitState(FStateArchive& Ar, FBuildingBiography& V)
		{
			Ar.BeginObject();
			Ar.Key(TEXT("id")).String(V.Id);
			Ar.Key(TEXT("type")).String(V.Type);
			Ar.Key(TEXT("cellX")).Number(V.CellX);
			Ar.Key(TEXT("cellY")).Number(V.CellY);
			Ar.Key(TEXT("firstSeenDay")).Number(V.FirstSeenDay);
			Ar.Key(TEXT("completedDay")).Number(V.CompletedDay);
			Ar.Key(TEXT("foundedDay")).Number(V.FoundedDay);
			Ar.Key(TEXT("founder")).String(V.Founder);
			Ar.Key(TEXT("founderJob")).String(V.FounderJob);
			Ar.Key(TEXT("founderHousehold")).Number(V.FounderHousehold);
			Ar.Key(TEXT("formHousePhase")).Number(V.FormHousePhase);
			Ar.Key(TEXT("formFixed")).Bool(V.bFormFixed);
			Ar.Key(TEXT("owner")).String(V.Owner);
			Ar.Key(TEXT("occupants")).Number(V.Occupants);
			Ar.Key(TEXT("peakOccupants")).Number(V.PeakOccupants);
			Ar.Key(TEXT("crowdedDays")).Number(V.CrowdedDays);
			Ar.Key(TEXT("ownerChanges")).Number(V.OwnerChanges);
			Ar.Key(TEXT("vacancyEpisodes")).Number(V.VacancyEpisodes);
			Ar.Key(TEXT("wasOccupied")).Bool(V.bWasOccupied);
			Ar.Key(TEXT("lastObservedDay")).Number(V.LastObservedDay);
			Ar.Key(TEXT("events"));
			VisitStates(Ar, V.Events);
			Ar.EndObject();
		}
	}

	uint64 FVillage::StateDigest() const
	{
		AnastasisDigest::FStateWriter Out;
		FStateArchive Ar = FStateArchive::ForHash(Out);
		// Le hachage ne fait que lire (voir l'en-tete du fichier).
		const_cast<FVillage*>(this)->ArchiveState(Ar);
		return Out.Digest();
	}

	void FVillage::ArchiveState(FStateArchive& Ar)
	{
		Ar.BeginObject();
		Ar.Key(TEXT("playerPersonId")).String(PlayerPersonId);
		Ar.Key(TEXT("playerDrive"));
		VisitState(Ar, PlayerDrive);
		Ar.Key(TEXT("hasPlayerChoice")).Bool(bHasPlayerChoice);
		Ar.Key(TEXT("playerChoice"));
		VisitState(Ar, PlayerChoice);
		Ar.Key(TEXT("playerChoiceDirty")).Bool(bPlayerChoiceDirty);
		Ar.Key(TEXT("hasPlayerRefusal")).Bool(bHasPlayerRefusal);
		Ar.Key(TEXT("playerRefusal"));
		VisitState(Ar, PlayerRefusal);
		Ar.Key(TEXT("playerOptions"));
		VisitStates(Ar, PlayerOptions);

		Ar.Key(TEXT("liveTiles"));
		VisitSortedIntMap(Ar, LiveTiles, [](FStateArchive& A, AnastasisWorld::FTile& Tile) { VisitState(A, Tile); });
		Ar.Key(TEXT("regrownFood")).Number(RegrownFood);
		Ar.Key(TEXT("recentVillageEmits"));
		AnastasisArchive::VisitArray(Ar, RecentVillageEmits, [](FStateArchive& A, double& At) { A.Number(At); });
		{
			uint32 RngState = VillageRng.GetState();
			Ar.Key(TEXT("villageRng")).Number(RngState);
			if (Ar.IsLoading() && Ar.Ok()) VillageRng.SetState(RngState);
		}
		Ar.Key(TEXT("nav"));
		VisitState(Ar, Nav);
		Ar.Key(TEXT("terrainTravelCostEnabled")).Bool(bTerrainTravelCostEnabled);
		Ar.Key(TEXT("materialCourierId")).String(MaterialCourierId);
		// ecart n°40 (opening-in-sim-001) : le verrou de la maison d'ouverture et son issue sont de l'etat :
		// ils decident qui recevra la maison achevee.
		Ar.Key(TEXT("openingSiteId")).String(OpeningSiteId);
		Ar.Key(TEXT("openingHomeStatus")).Number(OpeningHome.Status);
		Ar.Key(TEXT("openingHomeSiteId")).String(OpeningHome.SiteId);
		Ar.Key(TEXT("openingHomeNpcId")).String(OpeningHome.NpcId);
		Ar.Key(TEXT("openingHomeTime")).Number(OpeningHome.Time);
		Ar.Key(TEXT("navVersion")).Number(NavVersion);
		// `sim.traffic` et les sentiers qu'il fixe (ecart n°42) : la decroissance de minuit et l'effort de
		// defrichage en dependent, l'A* lit le cout des sentiers.
		Ar.Key(TEXT("traffic"));
		AnastasisArchive::VisitArray(Ar, Traffic, [](FStateArchive& A, float& T) { A.Number(T); });
		Ar.Key(TEXT("roadEvolutionEnabled")).Bool(bRoadEvolutionEnabled);
		// ecart n°52 : la croissance du village (valmire-grows-001).
		Ar.Key(TEXT("growthEnabled")).Bool(bGrowthEnabled);
		Ar.Key(TEXT("growthSitesOpened")).Number(GrowthSitesOpened);
		Ar.Key(TEXT("lastBuildDecision")).String(LastBuildDecision);
		{
			// Un sentier : `[tuile, classe, jour de naissance, trafic a la naissance]`.
			TArray<int32> RoadKeys;
			if (!Ar.IsLoading())
			{
				Roads.GetKeys(RoadKeys);
				RoadKeys.Sort();
			}
			int32 Count = RoadKeys.Num();
			Ar.Key(TEXT("roads")).BeginArray(Count);
			if (Ar.IsLoading() && Ar.Ok()) Roads.Reset();
			for (int32 I = 0; I < Count && Ar.Ok(); ++I)
			{
				int32 Four = 4;
				Ar.BeginArray(Four);
				Ar.Expect(Four, 4, TEXT("sentier"));
				int32 Index = Ar.IsLoading() ? 0 : RoadKeys[I];
				Ar.Number(Index);
				AnastasisTraffic::FRoadTile Road = Ar.IsLoading() ? AnastasisTraffic::FRoadTile() : Roads[Index];
				Ar.Number(Road.Class);
				Ar.Number(Road.BuiltDay);
				Ar.Number(Road.TrafficAtBirth);
				if (Ar.IsLoading() && Ar.Ok()) Roads.Add(Index, Road);
				Ar.EndArray();
			}
			Ar.EndArray();
			Ar.Key(TEXT("roadEfforts"));
			VisitSortedIntMap(Ar, RoadEfforts, [](FStateArchive& A, double& E) { A.Number(E); });
		}
		// ecart n°46 : la biographie des batiments (save-history-001).
		Ar.Key(TEXT("biographyEnabled")).Bool(bBiographyEnabled);
		Ar.Key(TEXT("biographies"));
		VisitSortedStringMap(Ar, Biographies, [](FStateArchive& A, FBuildingBiography& Bio) { VisitState(A, Bio); });
		Ar.Key(TEXT("settlement"));
		VisitState(Ar, Settlement);
		Ar.Key(TEXT("marketDx"));
		VisitOptionalNumber(Ar, MarketDx);
		Ar.Key(TEXT("marketDy"));
		VisitOptionalNumber(Ar, MarketDy);
		Ar.Key(TEXT("hasColony")).Bool(bHasColony);
		Ar.Key(TEXT("settlementClearRadius"));
		VisitOptionalNumber(Ar, SettlementClearRadius);
		Ar.Key(TEXT("colonyTreasury"));
		VisitOptionalNumber(Ar, ColonyTreasury);
		// Le cache d'urgence est indexe par la seconde de jeu, pas par l'etat : deux villages egaux par
		// ailleurs peuvent decider autrement dans la meme seconde. C'est donc de l'etat.
		Ar.Key(TEXT("urgencyBucket")).String(UrgencyBucket);
		Ar.Key(TEXT("urgencyCache"));
		VisitOptionalState(Ar, UrgencyCache);
		Ar.Key(TEXT("nextBuildingId")).Number(NextBuildingId);
		Ar.Key(TEXT("deathLog"));
		VisitStates(Ar, DeathLog);
		Ar.Key(TEXT("families"));
		VisitStates(Ar, Families);
		Ar.Key(TEXT("helpLog"));
		VisitStates(Ar, HelpLog);
		Ar.Key(TEXT("arrivalPool"));
		VisitStates(Ar, ArrivalPool);
		Ar.Key(TEXT("nextArrivalGroup")).Number(NextArrivalGroup);
		Ar.Key(TEXT("councilLog"));
		VisitStates(Ar, CouncilLog);
		Ar.Key(TEXT("playerVoteFamilyId")).String(PlayerVoteFamilyId);
		Ar.Key(TEXT("playerVoteYes")).Bool(bPlayerVoteYes);
		Ar.Key(TEXT("playerAsks"));
		VisitStates(Ar, PlayerAsks);
		Ar.Key(TEXT("nextFamilyId")).Number(NextFamilyId);
		Ar.Key(TEXT("nextNpcId")).Number(NextNpcId);
		Ar.Key(TEXT("now")).Number(Now);
		Ar.Key(TEXT("weatherSeed")).Number(WeatherSeed);
		Ar.Key(TEXT("weatherSeeded")).Bool(bWeatherSeeded);
		Ar.Key(TEXT("forcedWeather")).Bool(bForcedWeather);
		Ar.Key(TEXT("forcedWeatherValue"));
		VisitState(Ar, ForcedWeather);
		Ar.Key(TEXT("tickWeather"));
		VisitState(Ar, TickWeather);
		Ar.Key(TEXT("tickDailyRain")).Number(TickDailyRain);
		Ar.Key(TEXT("budgetDirector"));
		VisitState(Ar, BudgetDirector);
		Ar.Key(TEXT("simulationView")).Bool(bSimulationView);
		Ar.Key(TEXT("mealReservations"));
		VisitStates(Ar, MealReservations);
		Ar.Key(TEXT("foodSources"));
		VisitStates(Ar, FoodSources);
		Ar.Key(TEXT("mealSeq")).Number(MealSeq);
		Ar.Key(TEXT("reservationSweepAt")).Number(ReservationSweepAt);
		Ar.Key(TEXT("soilWaterByTile"));
		VisitSortedIntMap(Ar, SoilWaterByTile, [](FStateArchive& A, double& W) { A.Number(W); });
		Ar.Key(TEXT("soilWaterEnabled")).Bool(bSoilWaterEnabled);
		Ar.Key(TEXT("fieldHandsEnabled")).Bool(bFieldHandsEnabled); // ecart n°59
		Ar.Key(TEXT("fieldHandsHired")).Number(FieldHandsHired);
		Ar.Key(TEXT("thirstFirstEnabled")).Bool(bThirstFirstEnabled); // ecart n°58
		Ar.Key(TEXT("navService"));
		VisitState(Ar, NavService);
		if (Ar.IsHashing())
		{
			// Trafic par tuile, une seconde fois en empreinte d'octets (STATE_ORACLE_001). Deja sauve plus haut,
			// nombre par nombre : la sauvegarde ne le repete pas.
			Ar.Key(TEXT("traffic")).Blob(Traffic);
		}
		Ar.Key(TEXT("buildings"));
		VisitStates(Ar, Buildings.GetItemsMutable());
		Ar.Key(TEXT("actors"));
		VisitStates(Ar, Actors.GetItemsMutable());
		Ar.EndObject();
	}

	void FVillage::AfterStateLoaded()
	{
		// Les champs classes `cache:` dans tools/migration/state-fields.json : ni haches ni sauves, ils se
		// refont depuis l'etat relu. `Grid` se reconstruit au debut de chaque boucle des habitants.
		NavSourceShared.Reset();
		NavAgents.Reset();
	}
}

void AnastasisArchive::VisitTile(FStateArchive& Ar, AnastasisWorld::FTile& Tile)
{
	AnastasisVillage::VisitState(Ar, Tile);
}
