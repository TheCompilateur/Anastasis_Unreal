#include "Village/AnastasisPlanner.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisRng.h"
#include "Core/AnastasisSimMath.h"
#include "Work/AnastasisGather.h"

#include <limits>

namespace AnastasisPlanner
{
	namespace Catalog
	{
		struct FNamedValue
		{
			const TCHAR* Key;
			double Value;
		};
		struct FBuildingSpec
		{
			const TCHAR* Type;
			double Housing;
			double Security;
			int32 JobsCount;
			int32 CostBegin;
			int32 CostCount;
			int32 StorageBegin;
			int32 StorageCount;
			bool bHousing;
		};
		struct FDepotProfile
		{
			const TCHAR* Type;
			int32 AllowBegin;
			int32 AllowCount;
		};
		struct FConsumeOrder
		{
			const TCHAR* Resource;
			const TCHAR* Types;
		};
		struct FCharterTheme
		{
			const TCHAR* Id;
			int32 GoalBegin;
			int32 GoalCount;
			int32 BuildingBegin;
			int32 BuildingCount;
			int32 JobBegin;
			int32 JobCount;
		};
		struct FFocusFloor
		{
			const TCHAR* Focus;
			const TCHAR* Goal;
			double Points;
		};

#include "AnastasisPlannerCatalog.inl"
	}

	namespace C = Catalog;

	namespace
	{
		// --- Les lectures de JavaScript --------------------------------------------------------
		double Clamp(double V, double Lo, double Hi) { return FMath::Max(Lo, FMath::Min(Hi, V)); }
		double Round(double V) { return AnastasisJs::Round(V); }
		double Floor(double V) { return FMath::FloorToDouble(V); }
		double Ceil(double V) { return FMath::CeilToDouble(V); }
		/** `x | 0`. */
		int32 I32(double V) { return AnastasisJs::ToInt32(V); }
		/** `building.progress ?? 1`. */
		double P1(const FPlannerBuilding& B) { return B.Progress.IsSet() ? B.Progress.GetValue() : 1.0; }
		/** `building.progress >= 1` (undefined : faux). */
		bool ProgressGe1(const FPlannerBuilding& B) { return B.Progress.IsSet() && B.Progress.GetValue() >= 1.0; }
		/** `building.progress < 1` (undefined : faux). */
		bool ProgressLt1(const FPlannerBuilding& B) { return B.Progress.IsSet() && B.Progress.GetValue() < 1.0; }

		/** `FARM_PER_POP` : habitants par ferme visee. */
		constexpr double FarmPerPop = 10.0;

		const TCHAR* const PriorityTypes[] = { TEXT("food"), TEXT("housing"), TEXT("tools"), TEXT("labor"), TEXT("transport"), TEXT("security") };
		const TCHAR* const TrackedJobs[] = { TEXT("farmer"), TEXT("woodcutter"), TEXT("builder"), TEXT("blacksmith"), TEXT("quarryman"),
			TEXT("guard"), TEXT("artisan"), TEXT("fisherman"), TEXT("merchant") };
		const TCHAR* const BuildCandidates[] = { TEXT("farm"), TEXT("chickencoop"), TEXT("fishery"), TEXT("sheepfold"), TEXT("mill"),
			TEXT("bakery"), TEXT("piggery"), TEXT("stable"), TEXT("dairy"), TEXT("butcher"), TEXT("granary"), TEXT("sawmill"),
			TEXT("lumbercamp"), TEXT("quarry"), TEXT("house"), TEXT("dormitory"), TEXT("tavern"), TEXT("well"), TEXT("watchtower"),
			TEXT("wall"), TEXT("barracks"), TEXT("warehouse"), TEXT("workshop"), TEXT("forge"), TEXT("tannery"), TEXT("weaver"),
			TEXT("townhall"), TEXT("guildhall"), TEXT("dock"), TEXT("lodge"), TEXT("manor"), TEXT("chapel"), TEXT("temple") };
		/** `GOAL_BIAS_SOFT_KEY` : but -> cle de `effects.goalBias`. */
		const TCHAR* const SoftKeys[][2] = {
			{ TEXT("build"), TEXT("build") }, { TEXT("craft"), TEXT("craft") }, { TEXT("gatherFood"), TEXT("gatherFood") },
			{ TEXT("helpFarm"), TEXT("helpFarm") }, { TEXT("gatherWood"), TEXT("gatherWood") }, { TEXT("gatherStone"), TEXT("gatherStone") },
			{ TEXT("haulJob"), TEXT("haul") }, { TEXT("haul"), TEXT("haul") }, { TEXT("haulCart"), TEXT("haul") },
			{ TEXT("deliver"), TEXT("haul") }, { TEXT("sell"), TEXT("sell") }, { TEXT("buy"), TEXT("buy") },
			{ TEXT("fetchInput"), TEXT("fetchInput") }, { TEXT("maintain"), TEXT("maintain") }, { TEXT("explore"), TEXT("explore") } };

		const C::FBuildingSpec* SpecOf(const FString& Type)
		{
			for (const C::FBuildingSpec& S : C::Buildings)
			{
				if (Type == S.Type) return &S;
			}
			return nullptr;
		}

		double CatalogValue(const C::FNamedValue* Table, int32 Num, const FString& Key, double Default)
		{
			for (int32 I = 0; I < Num; ++I)
			{
				if (Key == Table[I].Key) return Table[I].Value;
			}
			return Default;
		}

		double JobTraitBiasGather(const FString& JobId)
		{
			return CatalogValue(C::JobTraitBiasGather, UE_ARRAY_COUNT(C::JobTraitBiasGather), JobId, 0.0);
		}

		double MarketCapOf(const FString& Resource, double Default)
		{
			return CatalogValue(C::MarketCap, UE_ARRAY_COUNT(C::MarketCap), Resource, Default);
		}

		double PresumedOf(const FString& Resource)
		{
			return CatalogValue(C::ReportPresumed, UE_ARRAY_COUNT(C::ReportPresumed), Resource, 40.0);
		}

		/** `market.stock[res] || 0`. */
		double Market(const FPlannerVillage& V, const TCHAR* Resource) { return V.MarketStock.Get(Resource); }
	}

	// --- FOrderedMap ------------------------------------------------------------------------------

	const double* FOrderedMap::Find(const FString& Key) const
	{
		for (const TPair<FString, double>& P : Items)
		{
			if (P.Key == Key) return &P.Value;
		}
		return nullptr;
	}

	double* FOrderedMap::FindMutable(const FString& Key)
	{
		for (TPair<FString, double>& P : Items)
		{
			if (P.Key == Key) return &P.Value;
		}
		return nullptr;
	}

	double FOrderedMap::Get(const FString& Key) const
	{
		const double* V = Find(Key);
		// `map[key] || 0` : NaN et 0 valent 0.
		return (V && !FMath::IsNaN(*V)) ? *V : 0.0;
	}

	void FOrderedMap::Set(const FString& Key, double Value)
	{
		if (double* V = FindMutable(Key))
		{
			*V = Value;
			return;
		}
		Items.Add(TPair<FString, double>(Key, Value));
	}

	void FOrderedMap::Add(const FString& Key, double Delta)
	{
		Set(Key, Get(Key) + Delta);
	}

	const FStockSlot* FPlannerBuilding::SlotOf(const FString& Resource) const
	{
		if (!bHasStock) return nullptr;
		for (const TPair<FString, FStockSlot>& P : Stock)
		{
			if (P.Key == Resource) return &P.Value;
		}
		return nullptr;
	}

	int32 FPlannerVillage::IndexOfActor(const FString& Id) const
	{
		for (int32 I = 0; I < Actors.Num(); ++I)
		{
			if (Actors[I].Id == Id) return I;
		}
		return INDEX_NONE;
	}

	const TArray<FString>& GoalBiasGoals()
	{
		static const TArray<FString> Goals = []()
		{
			TArray<FString> Out;
			for (const auto& Pair : SoftKeys) Out.Add(Pair[0]);
			return Out;
		}();
		return Goals;
	}

	// --- Simulation : les methodes lues ---------------------------------------------------------

	int32 CountPlannedBuildings(const FPlannerVillage& V, const FString& Type)
	{
		int32 N = 0;
		for (const FPlannerBuilding& B : V.Buildings) N += B.Type == Type ? 1 : 0;
		return N;
	}

	int32 CountBuildings(const FPlannerVillage& V, const FString& Type)
	{
		int32 N = 0;
		for (const FPlannerBuilding& B : V.Buildings) N += (B.Type == Type && ProgressGe1(B)) ? 1 : 0;
		return N;
	}

	int32 ActiveConstructionCount(const FPlannerVillage& V, const FString& Type)
	{
		// `if (building.progress >= 1) continue;` : un batiment sans `progress` est compte.
		int32 N = 0;
		for (const FPlannerBuilding& B : V.Buildings)
		{
			if (ProgressGe1(B)) continue;
			if (!Type.IsEmpty() && B.Type != Type) continue;
			N += 1;
		}
		return N;
	}

	int32 ConstructionOpenSlots(const FPlannerVillage& V)
	{
		int32 Cap = FMath::Max(1, I32(C::CollectiveMaxActiveSites));
		bool bFarmDone = false;
		for (const FPlannerBuilding& B : V.Buildings)
		{
			if (B.Type == TEXT("farm") && P1(B) >= 1.0) bFarmDone = true;
		}
		if (!bFarmDone) Cap = 1;
		if (ActiveConstructionCount(V, TEXT("farm")) > 0) Cap = 1;
		return FMath::Max(0, Cap - ActiveConstructionCount(V));
	}

	namespace
	{
		/** `completedBuildingEntries()` : `progress < 1` exclu, type connu du catalogue. */
		template <typename F>
		void ForCompleted(const FPlannerVillage& V, F&& Fn)
		{
			for (const FPlannerBuilding& B : V.Buildings)
			{
				if (ProgressLt1(B)) continue;
				const C::FBuildingSpec* Spec = SpecOf(B.Type);
				if (!Spec) continue;
				Fn(B, *Spec);
			}
		}

		/** `houseCapacity(building, data)`. */
		double HouseCapacity(const FPlannerBuilding& B, const C::FBuildingSpec* Spec)
		{
			if (B.Type != TEXT("house")) return Spec ? Spec->Housing : 0.0;
			// `clamp(Math.floor(building.housePhase || 1), 1, HOUSE_PHASES.length)`.
			const double Raw = (B.HousePhase.IsSet() && B.HousePhase.GetValue() != 0.0 && !FMath::IsNaN(B.HousePhase.GetValue()))
				? B.HousePhase.GetValue() : 1.0;
			const int32 Phase = static_cast<int32>(Clamp(Floor(Raw), 1.0, static_cast<double>(UE_ARRAY_COUNT(C::HousePhaseCapacity))));
			return C::HousePhaseCapacity[Phase - 1];
		}
	}

	double HousingCapacity(const FPlannerVillage& V)
	{
		double Sum = 0.0;
		ForCompleted(V, [&](const FPlannerBuilding& B, const C::FBuildingSpec& Spec) { Sum += HouseCapacity(B, &Spec); });
		return 5.0 + Sum;
	}

	double PendingHousingCapacity(const FPlannerVillage& V)
	{
		double Capacity = 0.0;
		for (const FPlannerBuilding& B : V.Buildings)
		{
			const C::FBuildingSpec* Spec = SpecOf(B.Type);
			if (Spec && Spec->bHousing && ProgressLt1(B)) Capacity += HouseCapacity(B, Spec);
		}
		return Capacity;
	}

	FOrderedMap MarketCaps(const FPlannerVillage& V)
	{
		FOrderedMap Caps;
		for (const C::FNamedValue& Cap : C::MarketCap) Caps.Set(Cap.Key, Cap.Value);
		ForCompleted(V, [&](const FPlannerBuilding&, const C::FBuildingSpec& Spec)
		{
			for (int32 I = 0; I < Spec.StorageCount; ++I)
			{
				const C::FNamedValue& S = C::BuildingStorage[Spec.StorageBegin + I];
				Caps.Set(S.Key, Caps.Get(S.Key) + S.Value);
			}
		});
		return Caps;
	}

	double TotalBuildingValueSecurity(const FPlannerVillage& V)
	{
		double Total = 0.0;
		ForCompleted(V, [&](const FPlannerBuilding&, const C::FBuildingSpec& Spec) { Total += Spec.Security; });
		return Total;
	}

	FOrderedMap BuildCost(const FPlannerVillage& V, const FString& Type)
	{
		const C::FBuildingSpec* Spec = SpecOf(Type);
		// `costMultiplier(type)`.
		const double Count = Type.IsEmpty() ? V.Buildings.Num() : CountBuildings(V, Type);
		const double Mult = FMath::Min(C::CostGrowthMaxMultiplier,
			1.0 + FMath::Max(0.0, Count - C::CostGrowthFreePerType) * C::CostGrowthPerExistingOfType);
		FOrderedMap Cost;
		if (Spec)
		{
			for (int32 I = 0; I < Spec->CostCount; ++I)
			{
				const C::FNamedValue& E = C::BuildingCosts[Spec->CostBegin + I];
				Cost.Set(E.Key, Ceil(E.Value * Mult));
			}
		}
		const int32 Wood = I32(Cost.Get(TEXT("wood")));
		const int32 PlankStock = I32(Market(V, TEXT("planks")));
		if (Wood >= I32(C::PlankBuildMinWoodCost) && CountBuildings(V, TEXT("sawmill")) > 0 && PlankStock > 0)
		{
			const int32 Share = static_cast<int32>(Floor(Wood * (C::PlankBuildWoodShare != 0.0 ? C::PlankBuildWoodShare : 0.4)));
			if (Share > 0)
			{
				Cost.Set(TEXT("wood"), Wood - Share);
				Cost.Set(TEXT("planks"), I32(Cost.Get(TEXT("planks"))) + Share);
			}
		}
		return Cost;
	}

	// --- Stock des batiments (stockLedger.js) -----------------------------------------------------

	namespace
	{
		/** `profileForBuilding(building)` : [debut, n] dans `ProfileAllow` ; faux sans profil. */
		bool ProfileFor(const FPlannerBuilding& B, int32& OutBegin, int32& OutCount)
		{
			if (B.Type.IsEmpty()) return false;
			if (P1(B) < 1.0)
			{
				OutBegin = C::SiteProfile.AllowBegin;
				OutCount = C::SiteProfile.AllowCount;
				return true;
			}
			for (const C::FDepotProfile& P : C::DepotProfiles)
			{
				if (B.Type == P.Type)
				{
					OutBegin = P.AllowBegin;
					OutCount = P.AllowCount;
					return true;
				}
			}
			return false;
		}
	}

	void EnsureBuildingStock(FPlannerBuilding& B)
	{
		int32 Begin = 0;
		int32 Count = 0;
		if (!ProfileFor(B, Begin, Count))
		{
			// `building.stock ??= {}`.
			B.bHasStock = true;
			return;
		}
		B.bHasStock = true;
		for (int32 I = 0; I < Count; ++I)
		{
			const FString Resource = C::ProfileAllow[Begin + I].Key;
			TPair<FString, FStockSlot>* Found = B.Stock.FindByPredicate([&](const TPair<FString, FStockSlot>& P) { return P.Key == Resource; });
			if (!Found)
			{
				B.Stock.Add(TPair<FString, FStockSlot>(Resource, FStockSlot()));
			}
			else
			{
				Found->Value.Physical = FMath::Max(0, Found->Value.Physical);
				Found->Value.Reserved = FMath::Max(0, FMath::Min(Found->Value.Physical, Found->Value.Reserved));
			}
		}
	}

	int32 PhysicalStock(const FPlannerBuilding& B, const FString& Resource)
	{
		const FStockSlot* S = B.SlotOf(Resource);
		return S ? S->Physical : 0;
	}

	int32 AvailableStock(const FPlannerBuilding& B, const FString& Resource)
	{
		const FStockSlot* S = B.SlotOf(Resource);
		return S ? FMath::Max(0, S->Physical - S->Reserved) : 0;
	}

	bool AcceptsResource(const FPlannerBuilding& B, const FString& Resource)
	{
		int32 Begin = 0;
		int32 Count = 0;
		if (!ProfileFor(B, Begin, Count)) return false;
		for (int32 I = 0; I < Count; ++I)
		{
			if (Resource == C::ProfileAllow[Begin + I].Key) return true;
		}
		return false;
	}

	namespace
	{
		/** `findDepotsForResource(sim, resource, { withAvailable })` : des indices de `Buildings`. */
		TArray<int32> FindDepotsForResource(const FPlannerVillage& V, const FString& Resource, bool bWithAvailable)
		{
			TArray<FString> Order;
			for (const C::FConsumeOrder& O : C::ConsumeOrder)
			{
				if (Resource == O.Resource) FString(O.Types).ParseIntoArray(Order, TEXT(","));
			}
			TArray<TPair<int32, int32>> Ranked;
			TArray<int32> Rest;
			for (int32 I = 0; I < V.Buildings.Num(); ++I)
			{
				const FPlannerBuilding& B = V.Buildings[I];
				if (P1(B) < 1.0) continue;
				if (!AcceptsResource(B, Resource)) continue;
				if (bWithAvailable && AvailableStock(B, Resource) <= 0) continue;
				const int32 Idx = Order.IndexOfByKey(B.Type);
				if (Idx >= 0) Ranked.Add(TPair<int32, int32>(I, Idx));
				else Rest.Add(I);
			}
			// `ranked.sort((a, b) => a.idx - b.idx)` : tri stable.
			Ranked.StableSort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B) { return A.Value < B.Value; });
			TArray<int32> Out;
			for (const TPair<int32, int32>& R : Ranked) Out.Add(R.Key);
			Out.Append(Rest);
			return Out;
		}

		double HaulAccessibleStock(const FPlannerVillage& V, const FString& Resource)
		{
			double Total = 0.0;
			for (const int32 I : FindDepotsForResource(V, Resource, true)) Total += AvailableStock(V.Buildings[I], Resource);
			return Total;
		}
	}

	bool SiteCanPlacePiece(FPlannerBuilding& B)
	{
		if (!B.bHasMaterialsNeeded) return true;
		EnsureBuildingStock(B);
		const int32 Placed = B.PiecesPlaced;
		const int32 Remaining = FMath::Max(1, C::ConstructionPieceTotal - Placed);
		const int32 N = 1;
		for (const TPair<FString, double>& E : B.MaterialsNeeded.Items)
		{
			const int32 TotalNeed = I32(E.Value);
			if (TotalNeed <= 0) continue;
			const int32 Already = I32(B.MaterialsConsumed.Get(E.Key));
			const int32 StillNeeded = FMath::Max(0, TotalNeed - Already);
			if (StillNeeded <= 0) continue;
			const int32 Share = FMath::Max(1, static_cast<int32>(Ceil(static_cast<double>(StillNeeded * N) / Remaining)));
			if (AvailableStock(B, E.Key) < Share) return false;
		}
		return true;
	}

	FVector2D MarketPos(const FPlannerVillage& V)
	{
		if (V.MarketPosCache.IsSet()) return V.MarketPosCache.GetValue();
		// `plannedMarketPos()` : `ensureMarketOffset(dx, dy)` autour du foyer.
		const double SX = V.bHasSettlement ? V.SettlementX : 0.0;
		const double SY = V.bHasSettlement ? V.SettlementY : 0.0;
		constexpr int32 MinCheb = 4;
		const int32 MinDist = MinCheb + 1;
		const bool bDx = V.MarketDx.IsSet() && FMath::IsFinite(V.MarketDx.GetValue());
		const bool bDy = V.MarketDy.IsSet() && FMath::IsFinite(V.MarketDy.GetValue());
		int32 OX = bDx ? static_cast<int32>(FMath::TruncToDouble(V.MarketDx.GetValue())) : MinDist;
		int32 OY = bDy ? static_cast<int32>(FMath::TruncToDouble(V.MarketDy.GetValue())) : -FMath::Max(2, MinDist / 2);
		if (OX == 0 && OY == 0)
		{
			OX = MinDist;
			OY = -FMath::Max(2, MinDist / 2);
		}
		while (FMath::Max(FMath::Abs(OX), FMath::Abs(OY)) <= MinCheb)
		{
			if (FMath::Abs(OX) >= FMath::Abs(OY)) OX += OX >= 0 ? 1 : -1;
			else OY += OY >= 0 ? 1 : -1;
		}
		return FVector2D(SX + OX, SY + OY);
	}

	// --- Rapport de stock (colonyStockReport.js) -------------------------------------------------

	namespace
	{
		struct FHubPull
		{
			int32 Missed = 0;
			TArray<FStockBlind> Blind;
			int32 IgnoredFar = 0;
		};

		struct FStockGap
		{
			FString Resource;
			double Delta = 0.0;
			bool bUnderestimates = false;
		};

		struct FBlindSpots
		{
			TArray<FStockGap> Gaps;
			int32 IgnoredFar = 0;
		};

		FStockReport EmptyReport(int32 Day)
		{
			FStockReport R;
			R.bPresent = true;
			R.Day = Day;
			for (const TCHAR* Res : C::ReportResources) R.Stock.Set(Res, PresumedOf(Res));
			R.LastRefreshDay = -1.0;
			return R;
		}

		/** `ensureColonyStockReport(sim)` : sans colonie, un rapport neuf (non garde). */
		FStockReport& EnsureStockReport(FPlannerVillage& V, FStockReport& Scratch)
		{
			if (!V.bHasColony)
			{
				Scratch = EmptyReport(I32(V.Day));
				return Scratch;
			}
			FStockReport& R = V.Colony.StockReport;
			if (!R.bPresent) R = EmptyReport(I32(V.Day));
			for (const TCHAR* Res : C::ReportResources)
			{
				const double* Value = R.Stock.Find(Res);
				if (!Value || !FMath::IsFinite(*Value)) R.Stock.Set(Res, PresumedOf(Res));
			}
			return R;
		}

		double WeightForDistance(double Dist)
		{
			if (Dist <= C::ReportNearRadius) return 1.0;
			if (Dist >= C::ReportFarRadius) return C::ReportFarWeight;
			const double T = (Dist - C::ReportNearRadius) / FMath::Max(1.0, C::ReportDistanceHalfLife);
			return Clamp(1.0 / (1.0 + T), C::ReportFarWeight, 1.0);
		}

		double TrueStock(const FPlannerVillage& V, const TCHAR* Resource)
		{
			return FMath::Max(0.0, Market(V, Resource));
		}

		double NextRng(FPlannerVillage& V)
		{
			// `typeof sim.rng === "function" ? sim.rng : fallbackRng` : la vue fournit toujours le flux.
			check(V.Rng);
			return V.Rng->Next();
		}

		void RefreshReport(FPlannerVillage& V, FStockReport& Report)
		{
			const int32 Day = I32(V.Day);
			if (Report.LastRefreshDay == Day) return;

			const FVector2D Hub = MarketPos(V);
			FOrderedMap Sampled;
			for (const TCHAR* Res : C::ReportResources) Sampled.Set(Res, 0.0);
			int32 CertifiedNear = 0;
			int32 IgnoredFar = 0;
			TArray<FStockBlind> Candidates;

			for (FPlannerBuilding& B : V.Buildings)
			{
				if (P1(B) < 1.0) continue;
				EnsureBuildingStock(B);
				if (!B.bHasStock) continue;
				const double BX = B.X + 0.5;
				const double BY = B.Y + 0.5;
				const double D = AnastasisMath::JsHypot(BX - Hub.X, BY - Hub.Y);
				const double W = WeightForDistance(D);
				if (W >= 0.95) CertifiedNear += 1;
				if (W <= C::ReportFarWeight + 0.02) IgnoredFar += 1;
				for (const TCHAR* Res : C::ReportResources)
				{
					const int32 Phys = PhysicalStock(B, Res);
					if (Phys <= 0) continue;
					Sampled.Set(Res, Sampled.Get(Res) + Phys * W);
					const double Missed = Phys * (1.0 - W);
					if (Missed >= 8.0)
					{
						FStockBlind Blind;
						Blind.Resource = Res;
						Blind.Missed = Round(Missed);
						Blind.BuildingType = B.Type;
						Blind.BuildingId = B.Id;
						Blind.Dist = Round(D);
						Candidates.Add(Blind);
					}
				}
			}

			// Rumeur : fausse nouvelle sur la ressource ou l'ecart est le plus grand.
			TOptional<FStockRumor> Rumor;
			if (Report.Rumor.IsSet() && Report.Rumor->UntilDay > Day) Rumor = Report.Rumor;
			if (!Rumor.IsSet() && NextRng(V) < C::ReportRumorChance)
			{
				FString BestRes = TEXT("wood");
				double BestGap = -1.0;
				for (const TCHAR* Res : C::ReportResources)
				{
					const double Gap = FMath::Abs(TrueStock(V, Res) - Sampled.Get(Res));
					if (Gap > BestGap)
					{
						BestGap = Gap;
						BestRes = Res;
					}
				}
				const double SkewAmp = C::ReportRumorSkewMin + NextRng(V) * (C::ReportRumorSkewMax - C::ReportRumorSkewMin);
				const double Sign = NextRng(V) < 0.5 ? -1.0 : 1.0;
				FStockRumor New;
				New.Resource = BestRes;
				New.Mul = 1.0 + Sign * SkewAmp;
				New.UntilDay = Day + 1 + Floor(NextRng(V) * 2.0);
				New.Cause = Sign < 0.0 ? TEXT("rumeur de penurie") : TEXT("rumeur d'abondance");
				Rumor = New;
			}

			FOrderedMap Next;
			for (const TCHAR* Res : C::ReportResources)
			{
				double Value = Sampled.Get(Res);
				if (Rumor.IsSet() && Rumor->Resource == Res) Value *= Rumor->Mul;
				if (V.Buildings.Num() == 0) Value = PresumedOf(Res);
				Next.Set(Res, Value);
			}

			// Doute : rapport vieux -> melange vers la presomption.
			const double Age = FMath::Max(0.0, static_cast<double>(Day - I32(Report.Day)));
			if (Age > 0.0)
			{
				const double Doubt = Clamp(Age / C::ReportDoubtAfterDays, 0.0, 0.55);
				for (const TCHAR* Res : C::ReportResources)
				{
					Next.Set(Res, Next.Get(Res) * (1.0 - Doubt) + PresumedOf(Res) * Doubt);
				}
			}

			// Clamp FINAL autour de la verite.
			for (const TCHAR* Res : C::ReportResources)
			{
				const double TrueAmt = TrueStock(V, Res);
				const double Lo = TrueAmt * C::ReportClampMin;
				const double Hi = FMath::Max(TrueAmt * C::ReportClampMax, TrueAmt + 8.0);
				Next.Set(Res, Round(Clamp(*Next.Find(Res), Lo, Hi) * 10.0) / 10.0);
			}

			Candidates.StableSort([](const FStockBlind& A, const FStockBlind& B) { return A.Missed > B.Missed; });
			Report.Stock = Next;
			Report.Day = Day;
			Report.LastRefreshDay = Day;
			Report.Rumor = Rumor;
			Report.CertifiedNear = CertifiedNear;
			Report.IgnoredFar = IgnoredFar;
			if (Candidates.Num() > 4) Candidates.SetNum(4);
			Report.Blind = Candidates;
		}

		FHubPull HubConsolidationPressure(FPlannerVillage& V)
		{
			FStockReport Scratch;
			FStockReport& Report = EnsureStockReport(V, Scratch);
			if (Report.LastRefreshDay != I32(V.Day)) RefreshReport(V, Report);
			FHubPull Pull;
			for (const FStockBlind& B : Report.Blind) Pull.Missed += I32(B.Missed);
			Pull.Blind = Report.Blind;
			Pull.IgnoredFar = I32(Report.IgnoredFar);
			return Pull;
		}

		FBlindSpots ColonyStockBlindSpots(FPlannerVillage& V)
		{
			FStockReport Scratch;
			const FStockReport& Report = EnsureStockReport(V, Scratch);
			FBlindSpots Out;
			for (const TCHAR* Res : C::ReportResources)
			{
				const double T = TrueStock(V, Res);
				const double R = Report.Stock.Get(Res);
				const double Delta = R - T;
				const double Rel = T > 0.0 ? FMath::Abs(Delta) / T : (FMath::Abs(Delta) > 8.0 ? 1.0 : 0.0);
				if (Rel < 0.18 && FMath::Abs(Delta) < 10.0) continue;
				FStockGap Gap;
				Gap.Resource = Res;
				Gap.Delta = Round(Delta);
				Gap.bUnderestimates = Delta < 0.0;
				Out.Gaps.Add(Gap);
			}
			Out.Gaps.StableSort([](const FStockGap& A, const FStockGap& B) { return FMath::Abs(A.Delta) > FMath::Abs(B.Delta); });
			Out.IgnoredFar = I32(Report.IgnoredFar);
			return Out;
		}
	}

	void RefreshColonyStockReport(FPlannerVillage& V)
	{
		FStockReport Scratch;
		RefreshReport(V, EnsureStockReport(V, Scratch));
	}

	FOrderedMap ReportedColonyStock(FPlannerVillage& V)
	{
		FStockReport Scratch;
		FStockReport& Report = EnsureStockReport(V, Scratch);
		if (Report.LastRefreshDay != I32(V.Day)) RefreshReport(V, Report);
		return EnsureStockReport(V, Scratch).Stock;
	}

	// --- Frein forestier (forestSustain.js, colonizationDoctrine.js) -------------------------------

	namespace
	{
		struct FBandRange
		{
			double Min = 0.0;
			double Max = 0.0;
		};

		FBandRange ColonizationBandRange(const FPlannerVillage& V)
		{
			const double ClearRadius = V.bHasSettlement && V.SettlementClearRadius.IsSet() ? V.SettlementClearRadius.GetValue() : 0.0;
			// `sim.settlement?.clearRadius || 8`.
			const double ClearR = (ClearRadius != 0.0 && !FMath::IsNaN(ClearRadius)) ? ClearRadius : 8.0;
			const double Bonus = FMath::Max(0, I32(FMath::IsFinite(V.Colony.DoctrineExpansionBonus) ? V.Colony.DoctrineExpansionBonus : 0.0));
			FBandRange R;
			R.Min = FMath::Max(C::ColonizationBandMin, ClearR * 0.55);
			const double Soft = ClearR + C::ColonizationExpansionFromClear + Bonus;
			R.Max = FMath::Max(R.Min + 4.0, FMath::Min(C::ColonizationExpansionHardCap, Soft));
			return R;
		}

		struct FForestBrake
		{
			double GatherWoodBias = 0.0;
			double WoodcutterNeedMul = 1.0;
			double JobBoostPenalty = 0.0;
		};

		FForestBrake ForestGatherBrake(const FPlannerVillage& V)
		{
			const double Dens = FrontierForestDensity(V);
			FForestBrake B;
			if (Dens >= C::ForestDensityOk) return B;
			if (Dens >= C::ForestDensityWarn)
			{
				B.GatherWoodBias = C::ForestGatherBiasWarn;
				B.WoodcutterNeedMul = C::ForestWoodcutterNeedMulWarn;
				B.JobBoostPenalty = C::ForestJobBoostPenaltyWarn;
				return B;
			}
			if (Dens >= C::ForestDensityScarce)
			{
				B.GatherWoodBias = C::ForestGatherBiasScarce;
				B.WoodcutterNeedMul = C::ForestWoodcutterNeedMulScarce;
				B.JobBoostPenalty = C::ForestJobBoostPenaltyScarce;
				return B;
			}
			B.GatherWoodBias = C::ForestGatherBiasCritical;
			B.WoodcutterNeedMul = C::ForestWoodcutterNeedMulCritical;
			B.JobBoostPenalty = C::ForestJobBoostPenaltyCritical;
			return B;
		}
	}

	double FrontierForestDensity(const FPlannerVillage& V)
	{
		const double CX = Floor(V.bHasSettlement ? V.SettlementX : 0.0);
		const double CY = Floor(V.bHasSettlement ? V.SettlementY : 0.0);
		const FBandRange Band = ColonizationBandRange(V);
		int32 Forest = 0;
		int32 Total = 0;
		for (double Y = CY - Band.Max; Y <= CY + Band.Max; Y += 1.0)
		{
			for (double X = CX - Band.Max; X <= CX + Band.Max; X += 1.0)
			{
				const double D = AnastasisMath::JsHypot(X - CX, Y - CY);
				if (D < Band.Min || D > Band.Max) continue;
				FPlannerTile Tile;
				if (!V.TileAt || !V.TileAt(static_cast<int32>(Floor(X)), static_cast<int32>(Floor(Y)), Tile)) continue;
				if (Tile.Type == TEXT("water")) continue;
				Total += 1;
				if ((Tile.Type == TEXT("forest") || Tile.Type == TEXT("scrub")) && Tile.Resource == TEXT("wood") && Tile.Amount > 0.0) Forest += 1;
			}
		}
		return Total > 0 ? static_cast<double>(Forest) / Total : 0.0;
	}

	// --- Charte (founderCharter.js) ----------------------------------------------------------------

	namespace
	{
		const C::FCharterTheme* ThemeOf(const FString& Id)
		{
			for (const C::FCharterTheme& T : C::CharterThemes)
			{
				if (Id == T.Id) return &T;
			}
			return nullptr;
		}

		/** `ensureFounderCharter(sim)` : expire la charte echue (et vide `_effects`). */
		const FCharterState* LiveCharter(FPlannerVillage& V)
		{
			if (!V.bHasColony) return nullptr;
			if (!V.Colony.Charter.IsSet()) return nullptr;
			if (!ThemeOf(V.Colony.Charter->ThemeId))
			{
				V.Colony.Charter.Reset();
				return nullptr;
			}
			if (I32(V.Colony.Charter->UntilDay) < I32(V.Day))
			{
				// `expireFounderCharter(sim, { silent: true })`.
				V.Colony.Charter.Reset();
				V.Colony.Priorities.Effects.Reset();
				return nullptr;
			}
			return &V.Colony.Charter.GetValue();
		}

		void ApplyFounderCharterToEffects(FPlannerVillage& V, FCollectiveEffects& E)
		{
			const FCharterState* Charter = LiveCharter(V);
			if (!Charter) return;
			const C::FCharterTheme* Theme = ThemeOf(Charter->ThemeId);
			if (!Theme) return;
			for (int32 I = 0; I < Theme->GoalCount; ++I)
			{
				const C::FNamedValue& G = C::CharterValues[Theme->GoalBegin + I];
				E.GoalBias.Set(G.Key, E.GoalBias.Get(G.Key) + FMath::Min(C::CharterGoalBiasCap, G.Value));
			}
			for (int32 I = 0; I < Theme->BuildingCount; ++I)
			{
				const C::FNamedValue& B = C::CharterValues[Theme->BuildingBegin + I];
				E.BuildingBoosts.Set(B.Key, E.BuildingBoosts.Get(B.Key) + FMath::Min(C::CharterBuildingBoostCap, B.Value));
			}
			for (int32 I = 0; I < Theme->JobCount; ++I)
			{
				const C::FNamedValue& J = C::CharterValues[Theme->JobBegin + I];
				E.JobBoosts.Set(J.Key, E.JobBoosts.Get(J.Key) + FMath::Min(C::CharterJobBoostCap, J.Value));
			}
			E.CharterThemeId = Theme->Id;
		}
	}

	// --- Planificateur (collectivePriorities.js) ----------------------------------------------------

	namespace
	{
		int32 Pop(const FPlannerVillage& V) { return FMath::Max(1, V.Actors.Num()); }

		int32 CountWorkers(const FPlannerVillage& V, const FString& JobId)
		{
			int32 N = 0;
			for (const FPlannerActor& A : V.Actors)
			{
				if (A.LifeStage == TEXT("child")) continue;
				N += A.JobId == JobId ? 1 : 0;
			}
			return N;
		}

		struct FFarmGap
		{
			int32 Completed = 0;
			int32 Seats = 0;
			int32 Filled = 0;
			int32 Gap = 0;
		};

		FFarmGap FarmStaffing(const FPlannerVillage& V)
		{
			FFarmGap G;
			const C::FBuildingSpec* Farm = SpecOf(TEXT("farm"));
			for (const FPlannerBuilding& B : V.Buildings)
			{
				if (B.Type != TEXT("farm") || ProgressLt1(B)) continue;
				G.Completed += 1;
				G.Seats += (Farm && Farm->JobsCount > 0) ? Farm->JobsCount : 1;
				// `sim.workersAtBuilding(b.id)` : les habitants dont le poste est ce batiment.
				for (const FPlannerActor& A : V.Actors) G.Filled += (!B.Id.IsEmpty() && A.WorkplaceId == B.Id) ? 1 : 0;
			}
			G.Gap = FMath::Max(0, G.Seats - G.Filled);
			return G;
		}

		double HousingDeficit(const FPlannerVillage& V)
		{
			const double PopN = V.Actors.Num();
			const double Cap = HousingCapacity(V) + PendingHousingCapacity(V);
			const double ByCapacity = FMath::Max(0.0, PopN - Cap);
			int32 Roofless = 0;
			for (const FPlannerActor& A : V.Actors)
			{
				if (A.bAlive && A.LifeStage != TEXT("child") && A.HomeId.IsEmpty() && A.ShelterId.IsEmpty()) Roofless += 1;
			}
			return FMath::Max(ByCapacity, static_cast<double>(Roofless));
		}

		struct FVacancy
		{
			int32 Vacant = 0;
			int32 Homeless = 0;
			bool bActive = false;
		};

		/** `housingVacancySnapshot(sim)` : ECRIT `vacantSinceDay` des maisons achevees. */
		FVacancy HousingVacancySnapshot(FPlannerVillage& V)
		{
			FVacancy Out;
			// `sim?.day || 1`.
			const double Day = (V.Day != 0.0 && !FMath::IsNaN(V.Day)) ? V.Day : 1.0;
			for (FPlannerBuilding& B : V.Buildings)
			{
				if (B.Type != TEXT("house")) continue;
				if (P1(B) < 1.0) continue;
				if (!B.Owner.IsEmpty())
				{
					// `stampHouseOccupied`.
					if (B.VacantSinceDay.IsSet()) B.VacantSinceDay.Reset();
					continue;
				}
				// `stampHouseVacant(b, day)`.
				if (!B.VacantSinceDay.IsSet() || !FMath::IsFinite(B.VacantSinceDay.GetValue()))
				{
					B.VacantSinceDay = FMath::IsFinite(Day) ? Day : 1.0;
				}
				Out.Vacant += 1;
			}
			for (const FPlannerActor& A : V.Actors)
			{
				if (A.LifeStage == TEXT("child")) continue;
				if (!A.HomeId.IsEmpty()) continue;
				Out.Homeless += 1;
			}
			Out.bActive = Out.Vacant >= 1 && Out.Homeless >= 1;
			return Out;
		}

		double FoodDaysLeft(FPlannerVillage& V)
		{
			const double Stock = ReportedColonyStock(V).Get(TEXT("food"));
			const double Daily = FMath::Max(1.0, Pop(V) * 0.9);
			return Stock / Daily;
		}

		/** `livePriorityLevels(sim)` : 0 sans colonie. */
		FOrderedMap LivePriorityLevels(const FPlannerVillage& V)
		{
			FOrderedMap L;
			for (const TCHAR* T : PriorityTypes) L.Set(T, V.bHasColony ? V.Colony.Priorities.Levels.Get(T) : 0.0);
			return L;
		}

		double LevelOf(const FOrderedMap& L, const TCHAR* Type) { return L.Get(Type); }

		const FJobNeed* JobOf(const TArray<FJobNeed>& Jobs, const TCHAR* JobId)
		{
			return Jobs.FindByPredicate([&](const FJobNeed& J) { return J.JobId == JobId; });
		}

		double ReadySiteServiceDebt(FPlannerVillage& V)
		{
			const int32 Day = I32(V.Day);
			double Worst = 0.0;
			for (FPlannerBuilding& B : V.Buildings)
			{
				if (P1(B) >= 1.0 || !B.bHasMaterialsNeeded) continue;
				if (!SiteCanPlacePiece(B)) continue;
				const double* Since = V.bHasColony ? V.Colony.Priorities.SiteStalledSinceDay.Find(B.Id) : nullptr;
				if (!Since || !FMath::IsFinite(*Since)) continue;
				const double Debt = Day - I32(*Since);
				if (Debt > Worst) Worst = Debt;
			}
			return Worst;
		}

		bool HasWoodIndustry(const FPlannerVillage& V)
		{
			for (const FPlannerBuilding& B : V.Buildings)
			{
				if (P1(B) < 1.0) continue;
				if (B.Type == TEXT("lumbercamp") || B.Type == TEXT("sawmill")) return true;
			}
			return false;
		}

		bool WoodBootstrapNeeded(FPlannerVillage& V)
		{
			if (HasWoodIndustry(V)) return false;
			double Missing = 0.0;
			for (const FPlannerBuilding& B : V.Buildings)
			{
				if (P1(B) >= 1.0 || !B.bHasMaterialsNeeded) continue;
				const int32 Need = I32(B.MaterialsNeeded.Get(TEXT("wood")));
				if (Need <= 0) continue;
				Missing += FMath::Max(0, Need - I32(B.MaterialsConsumed.Get(TEXT("wood"))) - PhysicalStock(B, TEXT("wood")));
			}
			if (Missing <= 0.0) return false;
			for (FPlannerBuilding& B : V.Buildings)
			{
				if (P1(B) >= 1.0 || !B.bHasMaterialsNeeded) continue;
				if (SiteCanPlacePiece(B)) return false;
			}
			double Mobilizable = HaulAccessibleStock(V, TEXT("wood"));
			for (const FPlannerActor& A : V.Actors) Mobilizable += A.InventoryWood;
			return Mobilizable < Missing;
		}

		/** `woodBootstrapDraft(sim)` : memoise par jour dans `_woodDraftDay` / `_woodDraft`. */
		const TArray<FString>* WoodBootstrapDraft(FPlannerVillage& V)
		{
			if (!WoodBootstrapNeeded(V)) return nullptr;
			FCollectivePriorities& State = V.Colony.Priorities;
			const int32 Day = I32(V.Day);
			if (State.WoodDraftDay.IsSet() && State.WoodDraftDay.GetValue() == Day && State.WoodDraft.IsSet()) return &State.WoodDraft.GetValue();
			struct FRanked
			{
				FString Id;
				double Fit = 0.0;
			};
			TArray<FRanked> Ranked;
			int32 Adults = 0;
			for (const FPlannerActor& A : V.Actors)
			{
				if (!A.bAlive || A.LifeStage == TEXT("child")) continue;
				Adults += 1;
				FRanked R;
				R.Id = A.Id;
				R.Fit = (A.JobId == TEXT("woodcutter") ? 2.0 : 0.0) + A.TraitGather * JobTraitBiasGather(A.JobId);
				Ranked.Add(R);
			}
			// `(b.fit - a.fit) || String(a.id).localeCompare(String(b.id))` : ordre ordinal ici (ecart n°32).
			Ranked.StableSort([](const FRanked& A, const FRanked& B)
			{
				if (A.Fit != B.Fit) return A.Fit > B.Fit;
				return A.Id.Compare(B.Id, ESearchCase::CaseSensitive) < 0;
			});
			const int32 Quota = FMath::Max(2, static_cast<int32>(Ceil(Adults / 4.0)));
			TArray<FString> Draft;
			for (int32 I = 0; I < Ranked.Num() && I < Quota; ++I) Draft.Add(Ranked[I].Id);
			State.WoodDraftDay = Day;
			State.WoodDraft = Draft;
			return &State.WoodDraft.GetValue();
		}

		bool CanCoverSpineSeed(const FPlannerVillage& V, const FString& Type)
		{
			if (Type.IsEmpty()) return false;
			const FOrderedMap Cost = BuildCost(V, Type);
			for (const TPair<FString, double>& E : Cost.Items)
			{
				const int32 Need = I32(E.Value);
				if (Need <= 0) continue;
				const int32 Target = FMath::Min(Need, FMath::Max(1, static_cast<int32>(Ceil(Need * 0.10))));
				// `sim.totalColonyPhysical` n'existe pas sur la simulation : `phys` vaut toujours 0.
				const int32 Stock = I32(V.MarketStock.Get(E.Key));
				const int32 TotalAvail = Stock + 0;
				if (TotalAvail < Target && Stock < 1) return false;
			}
			return true;
		}

		bool WantExtraFarmHardGate(const FPlannerVillage& V, int32 Farms, int32 FarmCap, double StockFood, int32 PopN)
		{
			if (Farms < 1 || Farms >= FarmCap) return false;
			if (ActiveConstructionCount(V, TEXT("farm")) > 0) return false;
			if (StockFood >= PopN * 1.35) return false;
			if (!CanCoverSpineSeed(V, TEXT("farm"))) return false;
			if (CountPlannedBuildings(V, TEXT("sawmill")) >= 1 && CountPlannedBuildings(V, TEXT("watchtower")) < 1)
			{
				return StockFood < PopN * 0.7;
			}
			return true;
		}

		bool ConstructionSlotsFull(const FPlannerVillage& V) { return ConstructionOpenSlots(V) <= 0; }

		bool AnyPlanned(const FPlannerVillage& V, std::initializer_list<const TCHAR*> Types)
		{
			for (const TCHAR* T : Types)
			{
				if (CountPlannedBuildings(V, T) >= 1) return true;
			}
			return false;
		}

		FString BestPendingByScore(FPlannerVillage& V, std::initializer_list<const TCHAR*> Types);

		bool EarlySpineSitesOpen(const FPlannerVillage& V)
		{
			for (const FPlannerBuilding& B : V.Buildings)
			{
				if (P1(B) >= 1.0) continue;
				if (B.Type == TEXT("farm") || B.Type == TEXT("lumbercamp") || B.Type == TEXT("sawmill") || B.Type == TEXT("granary")) return true;
			}
			return false;
		}

		bool ToolsVacuum(FPlannerVillage& V)
		{
			if (CountPlannedBuildings(V, TEXT("workshop")) >= 1 || CountPlannedBuildings(V, TEXT("forge")) >= 1) return false;
			if (EarlySpineSitesOpen(V)) return false;
			const double Known = ReportedColonyStock(V).Get(TEXT("tools"));
			const double Truth = Market(V, TEXT("tools"));
			const double Cap = C::CollectiveToolsVacuumStock;
			return Truth <= Cap && Known <= FMath::Max(Cap + 6.0, 8.0);
		}

		bool CompletedToolsAtelier(const FPlannerVillage& V)
		{
			for (const FPlannerBuilding& B : V.Buildings)
			{
				if (P1(B) < 1.0) continue;
				if (B.Type == TEXT("workshop") || B.Type == TEXT("forge")) return true;
			}
			return false;
		}

		bool CraftIdle(FPlannerVillage& V)
		{
			if (!CompletedToolsAtelier(V)) return false;
			const FOrderedMap Known = ReportedColonyStock(V);
			const double TruthTools = Market(V, TEXT("tools"));
			const double KnownTools = Known.Get(TEXT("tools"));
			const double Cap = C::CollectiveCraftIdleToolsStock;
			if (TruthTools > Cap && KnownTools > Cap) return false;
			const double Wood = Market(V, TEXT("wood")) + Known.Get(TEXT("wood"));
			return Wood >= 4.0;
		}

		FOrderedMap ScoreProjects(FPlannerVillage& V, const FOrderedMap& L, const TArray<FJobNeed>& J);
		FCollectiveEffects ComputeEffects(const FOrderedMap& Levels, const TArray<FJobNeed>& Jobs, const FOrderedMap& BuildingScores, FPlannerVillage& V);
	}

	int32 FarmStaffingGap(const FPlannerVillage& V) { return FarmStaffing(V).Gap; }

	TArray<FJobNeed> MeasureJobNeeds(FPlannerVillage& V)
	{
		const int32 PopN = Pop(V);
		const FOrderedMap Stock = ReportedColonyStock(V);
		const double Deficit = HousingDeficit(V);
		const double FoodDays = FoodDaysLeft(V);
		const bool bBuildPressure = Deficit > 0.0 || (Stock.Get(TEXT("wood")) < PopN * 2 && PopN > 12);
		const int32 ActiveSites = ActiveConstructionCount(V);
		const int32 Lumber = CountPlannedBuildings(V, TEXT("lumbercamp")) + CountPlannedBuildings(V, TEXT("sawmill"));
		const int32 Quarries = CountPlannedBuildings(V, TEXT("quarry"));
		const int32 Workshops = CountPlannedBuildings(V, TEXT("workshop"));
		const int32 Forges = CountPlannedBuildings(V, TEXT("forge"));
		const int32 Towers = CountPlannedBuildings(V, TEXT("watchtower")) + CountPlannedBuildings(V, TEXT("wall")) + CountPlannedBuildings(V, TEXT("barracks"));
		const double BuilderPer = C::CollectiveBuilderPerPop != 0.0 ? C::CollectiveBuilderPerPop : 16.0;
		const double SiteBonus = I32(C::CollectiveActiveSiteBuilderBonus) * ActiveSites;
		const int32 Fisheries = CountPlannedBuildings(V, TEXT("fishery"));

		FOrderedMap Needed;
		Needed.Set(TEXT("farmer"), FMath::Max(1.0, Ceil(PopN / 10.0) + (FoodDays < 8.0 ? 2.0 : 0.0)));
		Needed.Set(TEXT("woodcutter"), FMath::Max(1.0, Ceil(
			(Ceil(PopN / 14.0) + (Stock.Get(TEXT("wood")) < PopN * 2 ? 1.0 : 0.0) + (Lumber > 0 ? 1.0 : 0.0))
			* ForestGatherBrake(V).WoodcutterNeedMul)));
		Needed.Set(TEXT("builder"), FMath::Max(1.0, Ceil(PopN / BuilderPer)
			+ (Deficit > 0.0 ? FMath::Min(4.0, Ceil(Deficit / 4.0)) : 0.0)
			+ (bBuildPressure ? 1.0 : 0.0)
			+ SiteBonus
			+ (ActiveSites > 0 ? 1.0 : 0.0)));
		Needed.Set(TEXT("blacksmith"), (Forges > 0 || Workshops > 0 || PopN > 18)
			? FMath::Max(1.0, Ceil(PopN / 40.0) + (Stock.Get(TEXT("tools")) < PopN * 0.3 ? 1.0 : 0.0) + (Forges > 0 ? 1.0 : 0.0))
			: 0.0);
		Needed.Set(TEXT("quarryman"), FMath::Max(0.0, Ceil(PopN / 28.0)
			+ (Stock.Get(TEXT("stone")) < PopN * 1.4 ? 1.0 : 0.0)
			+ (Quarries > 0 ? 1.0 : 0.0)));
		Needed.Set(TEXT("guard"), (Towers > 0 || PopN > 28) ? FMath::Max(1.0, Ceil(PopN / 45.0) + (Towers > 0 ? 1.0 : 0.0)) : 0.0);
		Needed.Set(TEXT("artisan"), (Workshops > 0 || PopN > 16) ? FMath::Max(1.0, Ceil(PopN / 35.0) + (Workshops > 0 ? 1.0 : 0.0)) : 0.0);
		Needed.Set(TEXT("fisherman"), (Fisheries + CountPlannedBuildings(V, TEXT("dock")) > 0)
			? FMath::Max(1.0, Ceil(PopN / 50.0) + (Fisheries > 0 ? 1.0 : 0.0))
			: 0.0);
		Needed.Set(TEXT("merchant"), 0.0);

		TArray<FJobNeed> Out;
		for (const TCHAR* JobId : TrackedJobs)
		{
			const double Current = CountWorkers(V, JobId);
			const double NeedCount = Needed.Get(JobId);
			const double Gap = FMath::Max(0.0, NeedCount - Current);
			const double Surplus = FMath::Max(0.0, Current - FMath::Max(NeedCount, 1.0));
			double Need = 0.0;
			if (NeedCount <= 0.0) Need = 0.0;
			else if (Current == 0.0 && NeedCount > 0.0) Need = FMath::Min(100.0, 55.0 + NeedCount * 12.0);
			else if (Gap > 0.0) Need = FMath::Min(100.0, 30.0 + (Gap / NeedCount) * 70.0);
			else if (Surplus >= 3.0 && Current > NeedCount * 1.6) Need = FMath::Max(0.0, 12.0 - Surplus * 2.0);
			FJobNeed J;
			J.JobId = JobId;
			J.Current = Current;
			J.Needed = NeedCount;
			J.Need = Clamp(Need, 0.0, 100.0);
			J.Surplus = Surplus;
			Out.Add(J);
		}
		return Out;
	}

	FString ExploitSpinePending(FPlannerVillage& V)
	{
		if (ConstructionSlotsFull(V)) return FString();
		const int32 PopN = Pop(V);
		const int32 Farms = CountPlannedBuildings(V, TEXT("farm"));
		const int32 FarmCap = FMath::Max(2, static_cast<int32>(Ceil(PopN / FarmPerPop)));
		const double StockFood = Market(V, TEXT("food"));
		const double HousingCap = HousingCapacity(V) + PendingHousingCapacity(V);
		const bool bHousingCrisis = HousingCap <= PopN;

		const bool bFarmSiteOpen = ActiveConstructionCount(V, TEXT("farm")) > 0;
		if (!bFarmSiteOpen && Farms < 1)
		{
			if (!CanCoverSpineSeed(V, TEXT("farm")) && CountPlannedBuildings(V, TEXT("quarry")) < 1) return TEXT("quarry");
			return TEXT("farm");
		}
		if (!bFarmSiteOpen && WantExtraFarmHardGate(V, Farms, FarmCap, StockFood, PopN)) return TEXT("farm");

		const int32 Lumbercamps = CountPlannedBuildings(V, TEXT("lumbercamp"));
		const int32 Sawmills = CountPlannedBuildings(V, TEXT("sawmill"));
		const int32 Quarries = CountPlannedBuildings(V, TEXT("quarry"));
		const bool bStoneFirst = V.ScarceSeedWood > V.ScarceSeedStone;
		if (bStoneFirst)
		{
			if (Quarries < 1) return TEXT("quarry");
			if (Lumbercamps < 1) return TEXT("lumbercamp");
			if (Sawmills < 1) return TEXT("sawmill");
		}
		else
		{
			if (Lumbercamps < 1) return TEXT("lumbercamp");
			if (Sawmills < 1) return TEXT("sawmill");
		}
		if (bHousingCrisis) return FString();

		const int32 Warehouses = CountPlannedBuildings(V, TEXT("warehouse"));
		const int32 Workshops = CountPlannedBuildings(V, TEXT("workshop"));
		const int32 Forges = CountPlannedBuildings(V, TEXT("forge"));
		const int32 Granaries = CountPlannedBuildings(V, TEXT("granary"));
		const double Stone = Market(V, TEXT("stone"));
		const double Wood = Market(V, TEXT("wood"));
		const double Food = Market(V, TEXT("food"));

		if (!bStoneFirst && Quarries < 1 && Stone < PopN * 1.8) return TEXT("quarry");
		if (Warehouses < 1 && (Quarries >= 1 || Wood > PopN * 2.5 || Market(V, TEXT("planks")) > 12)) return TEXT("warehouse");
		if (Workshops < 1 && (Warehouses >= 1 || Quarries >= 1)) return TEXT("workshop");
		if (Forges < 1 && Workshops >= 1) return TEXT("forge");
		if (Granaries < 1 && Forges >= 1 && Farms >= 1 && Food > PopN * 4) return TEXT("granary");
		return FString();
	}

	namespace
	{
		FString BestPendingByScore(FPlannerVillage& V, std::initializer_list<const TCHAR*> Types)
		{
			TArray<FString> Missing;
			for (const TCHAR* T : Types)
			{
				if (CountPlannedBuildings(V, T) < 1) Missing.Add(T);
			}
			if (Missing.Num() == 0) return FString();
			// Scores BRUTS (`scoreBuildingProjects(sim, livePriorityLevels(sim), measureJobNeeds(sim))`).
			const FOrderedMap Levels = LivePriorityLevels(V);
			const TArray<FJobNeed> Jobs = MeasureJobNeeds(V);
			const FOrderedMap Scores = ScoreProjects(V, Levels, Jobs);
			FString Best;
			double BestScore = -std::numeric_limits<double>::infinity();
			for (const FString& T : Missing)
			{
				const double Score = Scores.Get(T);
				if (Score > BestScore)
				{
					BestScore = Score;
					Best = T;
				}
			}
			return BestScore >= C::CollectiveBuildingPickMin ? Best : FString();
		}
	}

	FString VillageAmenityPending(FPlannerVillage& V)
	{
		if (ConstructionSlotsFull(V)) return FString();
		if (CountPlannedBuildings(V, TEXT("forge")) < 1 && CountPlannedBuildings(V, TEXT("watchtower")) < 1) return FString();
		return BestPendingByScore(V, { TEXT("well"), TEXT("chickencoop"), TEXT("lodge") });
	}

	FString VillageCraftPending(FPlannerVillage& V)
	{
		if (ConstructionSlotsFull(V)) return FString();
		if (!AnyPlanned(V, { TEXT("well"), TEXT("chickencoop"), TEXT("lodge") })) return FString();
		return BestPendingByScore(V, { TEXT("bakery"), TEXT("tavern") });
	}

	FString CraftBootstrapPending(FPlannerVillage& V)
	{
		if (ConstructionSlotsFull(V)) return FString();
		const double ToolsLevel = V.bHasColony ? V.Colony.Priorities.Levels.Get(TEXT("tools")) : 0.0;
		const double LaborLevel = V.bHasColony ? V.Colony.Priorities.Levels.Get(TEXT("labor")) : 0.0;
		const FString Focus = (V.bHasColony && V.Colony.Priorities.DailyFocus.IsSet()) ? V.Colony.Priorities.DailyFocus->Id : FString();
		const double Band = C::CollectiveCraftBootstrapToolsBand;
		const bool bVacuum = ToolsVacuum(V);
		const bool bWant = Focus == TEXT("tools") || bVacuum || ToolsLevel >= Band || (LaborLevel >= Band && ToolsLevel >= C::CollectiveBandLow);
		if (!bWant) return FString();
		const int32 Workshops = CountPlannedBuildings(V, TEXT("workshop"));
		const int32 Forges = CountPlannedBuildings(V, TEXT("forge"));
		const int32 Sawmills = CountPlannedBuildings(V, TEXT("sawmill"));
		const int32 Lumbercamps = CountPlannedBuildings(V, TEXT("lumbercamp"));
		if (Forges >= 1) return FString();
		if (Sawmills < 1 && Lumbercamps >= 1) return TEXT("sawmill");
		if (Workshops < 1) return TEXT("workshop");
		if (Forges < 1) return TEXT("forge");
		return FString();
	}

	FString VillageHerdPending(FPlannerVillage& V)
	{
		if (ConstructionSlotsFull(V)) return FString();
		if (!AnyPlanned(V, { TEXT("bakery"), TEXT("tavern") })) return FString();
		const FString Best = BestPendingByScore(V, { TEXT("fishery"), TEXT("sheepfold") });
		if (Best == TEXT("fishery") && V.FindBuildSpot && !V.FindBuildSpot(TEXT("fishery")))
		{
			return CountPlannedBuildings(V, TEXT("sheepfold")) < 1 ? FString(TEXT("sheepfold")) : FString();
		}
		return Best;
	}

	namespace
	{
		FOrderedMap ScoreProjects(FPlannerVillage& V, const FOrderedMap& L, const TArray<FJobNeed>& J)
		{
			const int32 PopN = Pop(V);
			const FOrderedMap Stock = ReportedColonyStock(V);
			const double Deficit = HousingDeficit(V);
			const double FoodDays = FoodDaysLeft(V);
			const int32 Day = I32(V.Day);
			const double Morale = (V.bHasColony && V.Colony.Morale.IsSet()) ? V.Colony.Morale.GetValue() : 50.0;
			const FOrderedMap Caps = MarketCaps(V);
			auto Planned = [&V](const TCHAR* T) { return CountPlannedBuildings(V, T); };
			const int32 Farms = Planned(TEXT("farm"));
			const int32 Lumbercamps = Planned(TEXT("lumbercamp"));
			const int32 Sawmills = Planned(TEXT("sawmill"));
			const int32 Quarries = Planned(TEXT("quarry"));
			const int32 Warehouses = Planned(TEXT("warehouse"));
			const int32 Workshops = Planned(TEXT("workshop"));
			const int32 FarmCap = FMath::Max(2, static_cast<int32>(Ceil(PopN / FarmPerPop)));
			const int32 MinimumFarms = FMath::Max(1, static_cast<int32>(Ceil(PopN / 36.0)));
			const double HousingCap = HousingCapacity(V) + PendingHousingCapacity(V);
			const int32 HotPads = V.bHasColony ? V.Colony.DoctrineHotPads : 0;
			const double SecValue = TotalBuildingValueSecurity(V);
			const int32 WorkshopCap = FMath::Max(1, static_cast<int32>(Floor(PopN / (C::WorkshopPerPopulation != 0.0 ? C::WorkshopPerPopulation : 15.0))));
			const int32 SawmillCap = FMath::Max(1, static_cast<int32>(Ceil(PopN / 25.0)));
			const bool bExtractSpine = Farms >= 1 && Lumbercamps >= 1;
			const bool bProcessSpine = bExtractSpine && Sawmills >= 1;
			const bool bSecuredSpine = bProcessSpine && (Planned(TEXT("forge")) >= 1 || Planned(TEXT("watchtower")) >= 1);
			const double SFood = Stock.Get(TEXT("food"));
			const double SWood = Stock.Get(TEXT("wood"));
			const double SStone = Stock.Get(TEXT("stone"));
			const double SPlanks = Stock.Get(TEXT("planks"));
			const double STools = Stock.Get(TEXT("tools"));
			const double SLeather = Stock.Get(TEXT("leather"));
			const double SWool = Stock.Get(TEXT("wool"));
			const double LFood = LevelOf(L, TEXT("food"));
			const double LHousing = LevelOf(L, TEXT("housing"));
			const double LTools = LevelOf(L, TEXT("tools"));
			const double LSecurity = LevelOf(L, TEXT("security"));
			const double LTransport = LevelOf(L, TEXT("transport"));

			FOrderedMap Scores;
			auto SetScore = [&Scores](const TCHAR* Type, double Score) { Scores.Set(Type, FMath::Max(0.0, Round(Score))); };

			// —— Nourriture / rural ——
			double Farm = 0.0;
			if (Farms < 1) Farm += 95;
			if (Farms < MinimumFarms && Farms < FarmCap) Farm += 48;
			if (SFood < PopN * 3 && Farms < FarmCap) Farm += 62;
			Farm += LFood * 0.55;
			if (FoodDays < 8.0) Farm += 28;
			if (HotPads > 0 && Farms < FarmCap && Farms >= 1) Farm += 18;
			if (Farms >= FarmCap) Farm -= 50;
			if (ActiveConstructionCount(V, TEXT("farm")) > 0) Farm = 0.0;
			else if (Farms >= 1 && !CanCoverSpineSeed(V, TEXT("farm"))) Farm = FMath::Min(Farm, 10.0);
			{
				const FFarmGap Gap = FarmStaffing(V);
				if (Gap.Completed >= 1 && Gap.Gap > 0) Farm -= 55;
			}
			SetScore(TEXT("farm"), Farm);

			const bool bFoodHoldBuild = FoodDays < C::CollectiveFoodHoldDays || LFood >= C::CollectiveBandCritical;
			SetScore(TEXT("chickencoop"),
				Planned(TEXT("chickencoop")) < 1 && (Planned(TEXT("forge")) >= 1 || Planned(TEXT("watchtower")) >= 1)
					? 48.0
					: (!bFoodHoldBuild && (bSecuredSpine || (bProcessSpine && PopN > 16))
						&& Planned(TEXT("chickencoop")) < Ceil(PopN / 55.0)
						? (bSecuredSpine ? 34.0 : 28.0)
						: 0.0));
			SetScore(TEXT("fishery"),
				Farms >= 1 && Planned(TEXT("tavern")) >= 1 && Planned(TEXT("fishery")) < Ceil(PopN / 65.0)
					? (Planned(TEXT("fishery")) < 1 ? 46.0 : 34.0)
					: (Farms >= 1 && Planned(TEXT("lodge")) >= 1 && Planned(TEXT("mill")) >= 1
						&& Planned(TEXT("fishery")) < Ceil(PopN / 70.0) ? 22.0 : 0.0));
			SetScore(TEXT("sheepfold"),
				!bFoodHoldBuild && bSecuredSpine && Planned(TEXT("tavern")) >= 1 && Planned(TEXT("sheepfold")) < Ceil(PopN / 65.0)
					? (Planned(TEXT("sheepfold")) < 1 ? 44.0 : 30.0)
					: (!bFoodHoldBuild && bSecuredSpine && Planned(TEXT("lodge")) >= 1 && Planned(TEXT("mill")) >= 1
						&& SFood > PopN * 2
						&& Planned(TEXT("sheepfold")) < Ceil(PopN / 65.0) ? 30.0 : 0.0));
			{
				const int32 Mills = Planned(TEXT("mill"));
				const int32 Bakeries = Planned(TEXT("bakery"));
				const int32 Granaries = Planned(TEXT("granary"));
				double Mill = 0.0;
				if (bProcessSpine && Granaries >= 1 && SFood > PopN * 3.5 && Mills < Ceil(PopN / 55.0)
					&& (Planned(TEXT("lodge")) >= 1 || Day > 18))
				{
					Mill = Mills < 1 ? 34.0 : 22.0;
				}
				else if (bSecuredSpine && Farms >= 2 && SFood > PopN * 5 && Day > 14 && Mills < Ceil(PopN / 55.0))
				{
					Mill = Mills < 1 ? 28.0 : 18.0;
				}
				SetScore(TEXT("mill"), Mill);
				SetScore(TEXT("bakery"),
					bSecuredSpine && Bakeries < Ceil(PopN / 80.0)
						? (Bakeries < 1 ? (Mills >= 1 ? 44.0 : 30.0) : 28.0)
						: 0.0);
			}
			SetScore(TEXT("piggery"),
				!bFoodHoldBuild && Planned(TEXT("dock")) >= 1 && SFood > PopN * 2.4 && Planned(TEXT("piggery")) < Ceil(PopN / 80.0) ? 30.0 : 0.0);
			SetScore(TEXT("stable"),
				!bFoodHoldBuild && Planned(TEXT("butcher")) >= 1 && SFood > PopN * 2.6 && Planned(TEXT("stable")) < Ceil(PopN / 90.0) ? 28.0 : 0.0);
			SetScore(TEXT("dairy"),
				!bFoodHoldBuild && Planned(TEXT("stable")) >= 1 && Planned(TEXT("dairy")) < Ceil(PopN / 95.0) ? 26.0 : 0.0);
			SetScore(TEXT("butcher"),
				Planned(TEXT("piggery")) >= 1 && Planned(TEXT("butcher")) < Ceil(PopN / 90.0) ? 28.0 : 0.0);
			{
				double Granary = 0.0;
				const int32 Granaries = Planned(TEXT("granary"));
				if (bProcessSpine && Granaries < 1 && Farms >= 1) Granary += 36;
				const double FoodCap = Caps.Get(TEXT("food")) != 0.0 ? Caps.Get(TEXT("food")) : (MarketCapOf(TEXT("food"), 0.0) != 0.0 ? MarketCapOf(TEXT("food"), 0.0) : 200.0);
				if (bProcessSpine && Granaries < Ceil(PopN / 45.0)
					&& (SFood > FoodCap * 0.55
						|| (LFood >= C::CollectiveBandElevated && Farms >= 2)
						|| (SFood > PopN * 5 && Farms >= 1)))
				{
					Granary += 24;
				}
				SetScore(TEXT("granary"), Granary);
			}

			// —— Bois / pierre ——
			const FJobNeed* Woodcutter = JobOf(J, TEXT("woodcutter"));
			double Lumber = 0.0;
			if (Lumbercamps < 1 && Farms >= 1) Lumber += 72;
			if (Lumbercamps < 1) Lumber += 28;
			if (SWood < PopN * 1.8 && Lumbercamps < Ceil(PopN / 36.0)) Lumber += 42;
			if ((Woodcutter ? Woodcutter->Need : 0.0) >= C::CollectiveBandElevated) Lumber += 16;
			if (HotPads > 2 && Lumbercamps < Ceil(PopN / 40.0)) Lumber += 14;
			if (Lumbercamps >= Ceil(PopN / 28.0)) Lumber -= 40;
			SetScore(TEXT("lumbercamp"), Lumber);

			double Saw = 0.0;
			if (Lumbercamps >= 1 && Sawmills < 1) Saw += 68;
			if (Lumbercamps >= 1 && Sawmills < SawmillCap && SWood < PopN * 2.4) Saw += 36;
			if (LHousing >= C::CollectiveBandElevated && Sawmills < SawmillCap) Saw += 24;
			if (SPlanks < 10 && Lumbercamps >= 1) Saw += 18;
			{
				const FBlindSpots Spots = ColonyStockBlindSpots(V);
				const FStockGap* WoodBlind = Spots.Gaps.FindByPredicate([](const FStockGap& G)
				{
					return G.Resource == TEXT("wood") && G.bUnderestimates && FMath::Abs(G.Delta) >= 12.0;
				});
				if (WoodBlind && Sawmills < SawmillCap) Saw += 18;
			}
			if (Sawmills >= SawmillCap) Saw -= 45;
			if (Lumbercamps < 1) Saw = FMath::Min(Saw, 8.0);
			SetScore(TEXT("sawmill"), Saw);

			double Quarry = 0.0;
			if (bExtractSpine && Quarries < 1 && SStone < PopN * 2) Quarry += 52;
			if (bProcessSpine && SStone < PopN * 1.6 && Quarries < Ceil(PopN / 35.0)) Quarry += 34;
			if (LHousing >= C::CollectiveBandElevated && SStone < PopN * 2) Quarry += 12;
			if (Quarries >= Ceil(PopN / 30.0)) Quarry -= 40;
			SetScore(TEXT("quarry"), Quarry);

			// —— Logement ——
			const FJobNeed* Builder = JobOf(J, TEXT("builder"));
			double House = 0.0;
			if (HousingCap <= PopN) House += 78;
			House += Deficit * 6;
			House += LHousing * 0.75;
			if (SWood > 20) House += 12;
			if ((Builder ? Builder->Current : 0.0) > 0.0) House += 8;
			if (HotPads > 0 && HousingCap <= PopN + 2 && Farms >= 1) House += 36;
			{
				const FVacancy Vacancy = HousingVacancySnapshot(V);
				if (Vacancy.bActive)
				{
					const double Damp = Deficit >= 8.0
						? FMath::Min(14.0, 8.0 + Vacancy.Vacant * 2.0)
						: FMath::Min(44.0, 22.0 + Vacancy.Vacant * 8.0);
					House -= Damp;
				}
			}
			SetScore(TEXT("house"), House);

			double Dorm = 0.0;
			if (PopN > 28 && HousingCap < PopN + 6 && Planned(TEXT("dormitory")) < Ceil(PopN / 70.0)) Dorm += 34;
			if (Deficit >= 6.0) Dorm += 14;
			if (PopN < 28) Dorm -= 20;
			{
				const FVacancy Vacancy = HousingVacancySnapshot(V);
				if (Vacancy.bActive && Deficit < 6.0) Dorm -= FMath::Min(20.0, 10.0 + Vacancy.Vacant * 3.0);
			}
			SetScore(TEXT("dormitory"), Dorm);

			// —— Civic / services ——
			double Tavern = 0.0;
			if (bSecuredSpine && Planned(TEXT("lodge")) >= 1 && Planned(TEXT("bakery")) >= 1 && Planned(TEXT("tavern")) < 1) Tavern += 46;
			else if (bSecuredSpine && Planned(TEXT("lodge")) >= 1 && Day > 7 && Planned(TEXT("tavern")) < 1) Tavern += 22;
			if (bSecuredSpine && Morale < 52) Tavern += 10;
			if (Planned(TEXT("tavern")) >= 1) Tavern -= 40;
			if (LHousing >= C::CollectiveBandHigh) Tavern -= 55;
			if (LFood >= C::CollectiveBandHigh) Tavern -= 35;
			if (!bSecuredSpine) Tavern = 0.0;
			if (Planned(TEXT("bakery")) < 1) Tavern = FMath::Min(Tavern, 12.0);
			SetScore(TEXT("tavern"), Tavern);

			SetScore(TEXT("well"),
				Planned(TEXT("well")) < 1 && (Planned(TEXT("forge")) >= 1 || Planned(TEXT("watchtower")) >= 1)
					? 52.0
					: (bSecuredSpine && Planned(TEXT("well")) < FMath::Max(1.0, Ceil(PopN / 55.0))
						? 28.0
						: (bProcessSpine && Day > 7 && Planned(TEXT("well")) < FMath::Max(1.0, Ceil(PopN / 55.0)) ? 18.0 : 0.0)));

			// —— Securite ——
			double Tower = LSecurity * 0.55;
			const bool bIndustryStarted = Quarries >= 1 || Warehouses >= 1 || Workshops >= 1;
			if (bProcessSpine && bIndustryStarted && Day > 5 && Planned(TEXT("watchtower")) < 1) Tower += 36;
			else if (bProcessSpine && Day > 9 && Planned(TEXT("watchtower")) < 1) Tower += 16;
			else if (bExtractSpine && Day > 11 && Planned(TEXT("watchtower")) < 1) Tower += 10;
			SetScore(TEXT("watchtower"), Tower);

			SetScore(TEXT("wall"),
				Planned(TEXT("watchtower")) >= 1 && Planned(TEXT("wall")) < Ceil(PopN / 45.0)
					? (Planned(TEXT("wall")) < 1 ? 34.0 : 24.0) + LSecurity * 0.28
					: (bProcessSpine && PopN > 38 && Planned(TEXT("watchtower")) >= 1 && Planned(TEXT("wall")) < Ceil(PopN / 50.0)
						? 20.0 + LSecurity * 0.25
						: 0.0));
			if (bSecuredSpine && Planned(TEXT("lodge")) < 1)
			{
				const double WallCap = Planned(TEXT("wall")) < 1 && Planned(TEXT("watchtower")) >= 1 ? 22.0 : 8.0;
				Scores.Set(TEXT("wall"), FMath::Min(Scores.Get(TEXT("wall")), WallCap));
			}
			SetScore(TEXT("barracks"),
				Planned(TEXT("wall")) >= 1 && Planned(TEXT("barracks")) < 1
					? 28.0 + LSecurity * 0.25
					: (bProcessSpine && PopN > 46 && SecValue < Ceil(PopN / 4.0) && Planned(TEXT("barracks")) < Ceil(PopN / 70.0)
						? 24.0 + LSecurity * 0.3
						: 0.0));
			if (bSecuredSpine && Planned(TEXT("lodge")) < 1)
			{
				Scores.Set(TEXT("barracks"), FMath::Min(Scores.Get(TEXT("barracks")), 8.0));
			}

			// —— Stockage / industrie ——
			{
				double Warehouse = 0.0;
				if (bProcessSpine && Warehouses < 1) Warehouse += 40;
				if (bProcessSpine
					&& (SWood > (MarketCapOf(TEXT("wood"), 0.0) != 0.0 ? MarketCapOf(TEXT("wood"), 0.0) : 200.0) * 0.55
						|| SStone > (MarketCapOf(TEXT("stone"), 0.0) != 0.0 ? MarketCapOf(TEXT("stone"), 0.0) : 200.0) * 0.55
						|| SPlanks > 24)
					&& Warehouses < Ceil(PopN / 55.0)) Warehouse += 26;
				if (LTransport >= C::CollectiveBandElevated && Warehouses < Ceil(PopN / 50.0))
				{
					Warehouse += 18 + LTransport * 0.22;
				}
				if (!bProcessSpine) Warehouse = FMath::Min(Warehouse, 6.0);
				SetScore(TEXT("warehouse"), Warehouse);
			}
			{
				const int32 Markets = Planned(TEXT("market"));
				double MarketScore = 0.0;
				if (bProcessSpine && Markets < 1) MarketScore += 42;
				if (PopN >= 14 && Markets < 1) MarketScore += 20 + FMath::Min(30.0, (PopN - 14) * 0.6);
				if (Warehouses >= 1 && Markets < 1) MarketScore += 16;
				if (!bProcessSpine) MarketScore = FMath::Min(MarketScore, 6.0);
				SetScore(TEXT("market"), MarketScore);
			}

			const FJobNeed* Blacksmith = JobOf(J, TEXT("blacksmith"));
			double Workshop = 0.0;
			if (bProcessSpine && Workshops < 1) Workshop += 44;
			if (bProcessSpine && Workshops < WorkshopCap && SWood > 36) Workshop += 30;
			Workshop += LTools * 0.35;
			const bool bFocusTools = V.bHasColony && V.Colony.Priorities.DailyFocus.IsSet() && V.Colony.Priorities.DailyFocus->Id == TEXT("tools");
			const bool bBootstrapWant = LTools >= C::CollectiveCraftBootstrapToolsBand || bFocusTools;
			if (bBootstrapWant && Workshops < 1) Workshop += 72;
			if (!bProcessSpine && !bBootstrapWant) Workshop = FMath::Min(Workshop, 6.0);
			SetScore(TEXT("workshop"), Workshop);

			double Forge = LTools * 0.75;
			if (bProcessSpine && Workshops >= 1 && Planned(TEXT("forge")) < 1) Forge += 48;
			if (bProcessSpine && Workshops >= 1 && Planned(TEXT("forge")) < Ceil(PopN / 55.0) && SWood > 28) Forge += 22;
			if ((Blacksmith ? Blacksmith->Current : 0.0) == 0.0 && Workshops >= 1) Forge += 14;
			if (STools < PopN * 0.35 && Workshops >= 1) Forge += 16;
			if (bBootstrapWant && Workshops >= 1 && Planned(TEXT("forge")) < 1) Forge += 60;
			if (Planned(TEXT("forge")) >= 1 && STools > PopN * 0.5) Forge -= 30;
			if (!bProcessSpine && !(bBootstrapWant && Workshops >= 1)) Forge = FMath::Min(Forge, 8.0);
			SetScore(TEXT("forge"), Forge);

			SetScore(TEXT("tannery"),
				Planned(TEXT("stable")) >= 1 && Planned(TEXT("tannery")) < Ceil(PopN / 85.0)
					? ((SLeather > 4 || bProcessSpine) ? 42.0 : 28.0)
					: (bProcessSpine && SLeather > 8 && Planned(TEXT("tannery")) < Ceil(PopN / 90.0) ? 24.0 : 0.0));
			SetScore(TEXT("weaver"),
				Planned(TEXT("sheepfold")) >= 1 && Planned(TEXT("weaver")) < Ceil(PopN / 80.0)
					? ((SWool > 6 || bProcessSpine) ? 40.0 : 26.0)
					: (bProcessSpine && SWool > 10 && Planned(TEXT("weaver")) < Ceil(PopN / 85.0) ? 22.0 : 0.0));

			// —— Civic avance ——
			SetScore(TEXT("townhall"),
				bProcessSpine && Planned(TEXT("tavern")) >= 1 && Planned(TEXT("townhall")) < 1
					? 36.0
					: (bProcessSpine && PopN > 36 && Planned(TEXT("camp")) >= 1 && Planned(TEXT("townhall")) < 1 ? 24.0 : 0.0));
			SetScore(TEXT("guildhall"),
				Planned(TEXT("townhall")) >= 1 && Workshops >= 1 && Planned(TEXT("guildhall")) < 1
					? 30.0
					: (bProcessSpine && PopN > 48 && Workshops >= 1 && Planned(TEXT("guildhall")) < 1 ? 15.0 : 0.0));
			SetScore(TEXT("dock"),
				Planned(TEXT("fishery")) >= 1 && Planned(TEXT("tavern")) >= 1 && Planned(TEXT("dock")) < 1
					? 32.0
					: (bProcessSpine && PopN > 44 && Planned(TEXT("fishery")) >= 1 && Planned(TEXT("dock")) < 1 ? 16.0 : 0.0));
			SetScore(TEXT("lodge"),
				bSecuredSpine && Planned(TEXT("lodge")) < 1
					? (Planned(TEXT("well")) >= 1 ? 50.0 : Planned(TEXT("chickencoop")) >= 1 ? 24.0 : 18.0) + FMath::Min(14.0, LHousing * 0.1)
					: (bProcessSpine && PopN > 34 && Planned(TEXT("lodge")) < 1
						? 14.0 + FMath::Min(18.0, LHousing * 0.12)
						: 0.0));
			SetScore(TEXT("manor"),
				Planned(TEXT("temple")) >= 1 && Planned(TEXT("manor")) < 1
					? 24.0
					: (bProcessSpine && PopN > 52 && HousingCap < PopN + 4 && Planned(TEXT("manor")) < Ceil(PopN / 100.0) ? 14.0 : 0.0));
			SetScore(TEXT("chapel"),
				Planned(TEXT("townhall")) >= 1 && Planned(TEXT("chapel")) < 1
					? 28.0
					: (bProcessSpine && PopN > 54 && Morale < 60 && Planned(TEXT("chapel")) < 1 ? 16.0 : 0.0));
			SetScore(TEXT("temple"),
				Planned(TEXT("chapel")) >= 1 && Planned(TEXT("temple")) < 1
					? (Planned(TEXT("townhall")) >= 1 ? 34.0 : 26.0)
					: (bProcessSpine && PopN > 52 && Planned(TEXT("chapel")) >= 1 && Planned(TEXT("temple")) < 1 ? 18.0 : 0.0));

			for (const TCHAR* T : BuildCandidates)
			{
				if (!Scores.Has(T)) Scores.Set(T, 0.0);
			}
			return Scores;
		}

		FCollectiveEffects ComputeEffects(const FOrderedMap& Levels, const TArray<FJobNeed>& Jobs, const FOrderedMap& BuildingScores, FPlannerVillage& V)
		{
			FCollectiveEffects E;
			for (const TCHAR* R : { TEXT("wood"), TEXT("stone"), TEXT("planks"), TEXT("tools"), TEXT("food") }) E.Reserve.Set(R, 0.0);
			for (const TCHAR* G : { TEXT("build"), TEXT("craft"), TEXT("gatherFood"), TEXT("gatherWood"), TEXT("gatherStone"), TEXT("haul"),
				TEXT("maintain"), TEXT("explore"), TEXT("helpFarm"), TEXT("sell"), TEXT("buy"), TEXT("fetchInput") })
			{
				E.GoalBias.Set(G, 0.0);
			}

			const double H = Levels.Get(TEXT("housing"));
			const double F = Levels.Get(TEXT("food"));
			const double T = Levels.Get(TEXT("tools"));
			const double S = Levels.Get(TEXT("security"));
			const double Tr = Levels.Get(TEXT("transport"));
			const double Lab = Levels.Get(TEXT("labor"));
			const double ReserveWoodCap = CatalogValue(C::CollectiveReserveCap, UE_ARRAY_COUNT(C::CollectiveReserveCap), TEXT("wood"), 0.0);
			const double ReservePlanksCap = CatalogValue(C::CollectiveReserveCap, UE_ARRAY_COUNT(C::CollectiveReserveCap), TEXT("planks"), 0.0);
			const double ReserveToolsCap = CatalogValue(C::CollectiveReserveCap, UE_ARRAY_COUNT(C::CollectiveReserveCap), TEXT("tools"), 0.0);
			const double ReserveStoneCap = CatalogValue(C::CollectiveReserveCap, UE_ARRAY_COUNT(C::CollectiveReserveCap), TEXT("stone"), 0.0);

			if (H >= C::CollectiveBandLow)
			{
				E.JobBoosts.Add(TEXT("builder"), (H / 100.0) * C::CollectiveJobBoostMax);
				E.BuildingBoosts.Set(TEXT("house"), H * 0.9);
				E.BuildingBoosts.Set(TEXT("dormitory"), H * 0.5);
				E.GoalBias.Add(TEXT("build"), (H / 100.0) * C::CollectiveGoalBiasMax);
				E.Reserve.Add(TEXT("wood"), FMath::Min(ReserveWoodCap, Round(H * 0.55)));
				E.Reserve.Add(TEXT("planks"), FMath::Min(ReservePlanksCap, Round(H * 0.25)));
				if (H >= C::CollectiveBandHigh)
				{
					E.ImmigrationMul *= 0.55;
					E.bSuspendSecondary = true;
				}
			}
			if (F >= C::CollectiveBandLow)
			{
				E.JobBoosts.Add(TEXT("farmer"), (F / 100.0) * C::CollectiveJobBoostMax);
				E.JobBoosts.Add(TEXT("fisherman"), (F / 100.0) * 1.2);
				E.BuildingBoosts.Set(TEXT("farm"), F * 0.85);
				E.GoalBias.Add(TEXT("gatherFood"), (F / 100.0) * C::CollectiveGoalBiasMax);
				if (F >= C::CollectiveBandHigh) E.ImmigrationMul *= 0.5;
			}
			if (T >= C::CollectiveBandLow)
			{
				E.JobBoosts.Add(TEXT("blacksmith"), (T / 100.0) * C::CollectiveJobBoostMax);
				E.JobBoosts.Add(TEXT("artisan"), (T / 100.0) * 1.4);
				E.BuildingBoosts.Set(TEXT("forge"), T * 0.8);
				E.BuildingBoosts.Set(TEXT("workshop"), T * 0.45);
				E.GoalBias.Add(TEXT("craft"), (T / 100.0) * C::CollectiveGoalBiasMax);
				E.Reserve.Add(TEXT("tools"), FMath::Min(ReserveToolsCap, Round(T * 0.15)));
			}
			if (S >= C::CollectiveBandLow)
			{
				E.JobBoosts.Add(TEXT("guard"), (S / 100.0) * 1.8);
				E.BuildingBoosts.Set(TEXT("watchtower"), S * 0.7);
				E.BuildingBoosts.Set(TEXT("wall"), S * 0.5);
				E.BuildingBoosts.Set(TEXT("barracks"), S * 0.55);
				E.GoalBias.Add(TEXT("maintain"), (S / 100.0) * FMath::Min(C::CollectiveGoalBiasMax, 12.0));
				E.GoalBias.Add(TEXT("explore"), (S / 100.0) * 6.0);
				E.Reserve.Add(TEXT("stone"), FMath::Min(ReserveStoneCap, Round(S * 0.1)));
				if (S >= C::CollectiveBandElevated)
				{
					E.GoalBias.Add(TEXT("maintain"), 5.0);
					E.ImmigrationMul *= 0.92;
					E.Reserve.Add(TEXT("wood"), FMath::Min(ReserveWoodCap, Round(S * 0.06)));
				}
			}
			if (Tr >= C::CollectiveBandLow)
			{
				E.JobBoosts.Add(TEXT("builder"), (Tr / 100.0) * 0.55);
				E.JobBoosts.Add(TEXT("merchant"), (Tr / 100.0) * 0.35);
				E.JobBoosts.Add(TEXT("steward"), (Tr / 100.0) * 0.25);
				E.BuildingBoosts.Set(TEXT("warehouse"), FMath::Max(E.BuildingBoosts.Get(TEXT("warehouse")), Tr * 0.4));
				E.GoalBias.Add(TEXT("haul"), (Tr / 100.0) * FMath::Min(C::CollectiveGoalBiasMax, 14.0));
				E.Reserve.Add(TEXT("wood"), FMath::Min(ReserveWoodCap, Round(Tr * 0.2)));
				E.Reserve.Add(TEXT("stone"), FMath::Min(ReserveStoneCap, Round(Tr * 0.12)));
				if (Tr >= C::CollectiveBandElevated)
				{
					E.GoalBias.Add(TEXT("build"), 4.0);
					E.GoalBias.Add(TEXT("haul"), 4.0);
				}
			}
			{
				const FHubPull Pull = HubConsolidationPressure(V);
				if (Pull.Missed >= C::CollectiveHubPullMissed)
				{
					E.bHubPull = true;
					const double PullBias = Clamp(Pull.Missed * 0.55, 10.0, C::CollectiveGoalBiasMax);
					E.GoalBias.Add(TEXT("haul"), PullBias);
					E.GoalBias.Set(TEXT("sell"), -FMath::Min(18.0, 8.0 + Pull.Missed * 0.28));
					E.JobBoosts.Add(TEXT("merchant"), 0.35);
					E.JobBoosts.Add(TEXT("steward"), 0.25);
				}
			}
			for (const FJobNeed& Job : Jobs)
			{
				if (Job.Need >= C::CollectiveBandHigh)
				{
					E.JobBoosts.Set(Job.JobId, FMath::Max(E.JobBoosts.Get(Job.JobId), (Job.Need / 100.0) * C::CollectiveJobBoostMax));
				}
				if (Job.Surplus >= 4.0 && Job.Need < C::CollectiveBandLow)
				{
					E.JobBoosts.Set(Job.JobId, E.JobBoosts.Get(Job.JobId) - 0.55);
				}
			}
			if (H >= C::CollectiveCrisisSuspendAt || F >= C::CollectiveCrisisSuspendAt) E.bSuspendSecondary = true;
			if (H >= C::CollectiveBandHigh && BuildingScores.Get(TEXT("sawmill")) > 40.0)
			{
				E.BuildingBoosts.Set(TEXT("sawmill"), FMath::Max(E.BuildingBoosts.Get(TEXT("sawmill")), H * 0.4));
				E.JobBoosts.Add(TEXT("woodcutter"), 0.6);
				E.GoalBias.Add(TEXT("gatherWood"), 8.0);
			}
			{
				const double Wood = Market(V, TEXT("wood"));
				const double Actors = FMath::Max(1, V.Actors.Num());
				if (Wood < Actors * 2.2 || BuildingScores.Get(TEXT("lumbercamp")) >= 40.0)
				{
					E.JobBoosts.Add(TEXT("woodcutter"), 0.85);
					E.BuildingBoosts.Set(TEXT("lumbercamp"), FMath::Max(E.BuildingBoosts.Get(TEXT("lumbercamp")), 18.0));
					E.BuildingBoosts.Set(TEXT("sawmill"), FMath::Max(E.BuildingBoosts.Get(TEXT("sawmill")), 14.0));
					E.GoalBias.Add(TEXT("gatherWood"), 10.0);
				}
			}
			{
				const FString Pending = ExploitSpinePending(V);
				if (!Pending.IsEmpty())
				{
					E.GoalBias.Add(TEXT("build"), 16.0);
					E.JobBoosts.Add(TEXT("builder"), 0.95);
					E.BuildingBoosts.Set(Pending, FMath::Max(E.BuildingBoosts.Get(Pending), 30.0));
					if (Pending == TEXT("lumbercamp") || Pending == TEXT("sawmill"))
					{
						E.GoalBias.Add(TEXT("gatherWood"), 12.0);
						E.JobBoosts.Add(TEXT("woodcutter"), 0.75);
					}
					else if (Pending == TEXT("quarry"))
					{
						E.JobBoosts.Add(TEXT("quarryman"), 0.7);
						E.GoalBias.Add(TEXT("gatherWood"), 4.0);
					}
					else if (Pending == TEXT("farm"))
					{
						E.GoalBias.Add(TEXT("gatherFood"), 6.0);
						E.JobBoosts.Add(TEXT("farmer"), 0.45);
					}
					else
					{
						E.GoalBias.Add(TEXT("gatherWood"), 6.0);
						E.GoalBias.Add(TEXT("haul"), 4.0);
					}
				}
			}
			{
				const double Debt = ReadySiteServiceDebt(V);
				if (Debt > 0.0)
				{
					const double Quantum = C::CollectiveReadySiteDebtQuantum;
					const double Cap = C::CollectiveReadySiteBuildFloor;
					E.GoalFloor.Set(TEXT("build"), FMath::Max(E.GoalFloor.Get(TEXT("build")), FMath::Min(Cap, Debt * Quantum)));
					E.bReadySiteStalled = true;
					E.ReadySiteDebt = Debt;
				}
			}
			{
				const int32 Active = ActiveConstructionCount(V);
				if (Active > 0)
				{
					E.GoalBias.Add(TEXT("build"), 10.0 + Active * C::CollectiveActiveSiteBuildBias);
					E.GoalBias.Add(TEXT("haul"), 6.0 + Active * C::CollectiveActiveSiteHaulBias);
					E.JobBoosts.Add(TEXT("builder"), 0.7 + Active * 0.35);
				}
			}
			if (CountPlannedBuildings(V, TEXT("sawmill")) >= 1 && Market(V, TEXT("planks")) < 10.0)
			{
				E.GoalBias.Add(TEXT("craft"), 7.0);
				E.GoalBias.Add(TEXT("gatherWood"), 4.0);
				E.JobBoosts.Add(TEXT("woodcutter"), 0.4);
			}
			{
				const FString Amenity = VillageAmenityPending(V);
				const FString Craft = VillageCraftPending(V);
				const FString Herd = VillageHerdPending(V);
				const FString Pending = !Amenity.IsEmpty() ? Amenity : !Craft.IsEmpty() ? Craft : Herd;
				if (!Pending.IsEmpty())
				{
					E.GoalBias.Add(TEXT("build"), !Amenity.IsEmpty() ? 12.0 : !Craft.IsEmpty() ? 10.0 : 9.0);
					E.JobBoosts.Add(TEXT("builder"), !Amenity.IsEmpty() ? 0.7 : 0.55);
					E.BuildingBoosts.Set(Pending, FMath::Max(E.BuildingBoosts.Get(Pending), 24.0));
					if (Craft == TEXT("bakery"))
					{
						E.GoalBias.Add(TEXT("gatherFood"), 4.0);
						E.JobBoosts.Add(TEXT("farmer"), 0.25);
					}
					if (Herd == TEXT("fishery"))
					{
						E.JobBoosts.Add(TEXT("fisherman"), 0.45);
						E.GoalBias.Add(TEXT("gatherFood"), 3.0);
					}
					if (Herd == TEXT("sheepfold"))
					{
						E.JobBoosts.Add(TEXT("herder"), 0.45);
					}
				}
			}
			{
				const FString Bootstrap = CraftBootstrapPending(V);
				if (!Bootstrap.IsEmpty() && E.CraftBootstrap.IsEmpty())
				{
					E.CraftBootstrap = Bootstrap;
					E.GoalBias.Add(TEXT("build"), 14.0);
					E.GoalFloor.Set(TEXT("build"), FMath::Max(E.GoalFloor.Get(TEXT("build")), 24.0));
					E.BuildingBoosts.Set(Bootstrap, FMath::Max(E.BuildingBoosts.Get(Bootstrap), 48.0));
					E.JobBoosts.Add(TEXT("builder"), 0.55);
				}
			}

			if (F >= C::CollectiveBandCritical)
			{
				E.bFoodRush = true;
				E.GoalBias.Add(TEXT("gatherFood"), 18.0);
				E.GoalBias.Add(TEXT("helpFarm"), 14.0);
				E.GoalBias.Add(TEXT("haul"), 16.0);
				E.JobBoosts.Add(TEXT("farmer"), 0.55);
				E.JobBoosts.Add(TEXT("fisherman"), 0.35);
				const FFarmGap Farms = FarmStaffing(V);
				const bool bHasFoodInfra = Farms.Completed >= 1;
				if (bHasFoodInfra)
				{
					E.GoalBias.Set(TEXT("gatherWood"), FMath::Max(0.0, E.GoalBias.Get(TEXT("gatherWood")) - 14.0));
					E.GoalBias.Set(TEXT("build"), FMath::Max(0.0, E.GoalBias.Get(TEXT("build")) - 12.0));
					E.GoalBias.Set(TEXT("craft"), FMath::Max(0.0, E.GoalBias.Get(TEXT("craft")) - 8.0));
					E.GoalBias.Set(TEXT("explore"), FMath::Max(0.0, E.GoalBias.Get(TEXT("explore")) - 6.0));
				}
				if (Farms.Gap > 0)
				{
					E.BuildingBoosts.Set(TEXT("farm"), FMath::Min(E.BuildingBoosts.Get(TEXT("farm")), 8.0));
					E.GoalBias.Set(TEXT("build"), FMath::Max(0.0, E.GoalBias.Get(TEXT("build")) - 6.0));
				}
			}
			else if (F >= C::CollectiveBandHigh)
			{
				E.GoalBias.Add(TEXT("gatherFood"), 8.0);
				E.GoalBias.Add(TEXT("helpFarm"), 8.0);
				E.GoalBias.Add(TEXT("haul"), 6.0);
			}

			{
				const double Days = FoodDaysLeft(V);
				const bool bWinter = AnastasisGather::FieldSeasonFromDay(I32(V.Day)) == 3;
				const double StockFood = I32(Market(V, TEXT("food")));
				const double PopN = Pop(V);
				E.bFoodHold = E.bFoodRush || Days < C::CollectiveFoodHoldDays || (bWinter && Days < 10.0);
				if (E.bFoodHold)
				{
					E.ImmigrationMul = FMath::Min(E.ImmigrationMul, C::CollectiveImmigrationFloor + 0.08);
					for (const TCHAR* Herd : { TEXT("sheepfold"), TEXT("stable"), TEXT("piggery"), TEXT("chickencoop"), TEXT("dairy") })
					{
						if (E.BuildingBoosts.Get(Herd) > 4.0) E.BuildingBoosts.Set(Herd, 4.0);
					}
					if (bWinter && Days < 10.0)
					{
						E.GoalBias.Add(TEXT("gatherFood"), 6.0);
						E.GoalBias.Add(TEXT("helpFarm"), 4.0);
						E.GoalBias.Set(TEXT("gatherWood"), FMath::Max(0.0, E.GoalBias.Get(TEXT("gatherWood")) - 4.0));
					}
				}
				if (StockFood <= FMath::Max(4.0, Ceil(PopN * 0.35)) || Days < 3.0)
				{
					E.GoalFloor.Set(TEXT("gatherFood"), FMath::Max(E.GoalFloor.Get(TEXT("gatherFood")), 40.0));
					E.GoalFloor.Set(TEXT("helpFarm"), FMath::Max(E.GoalFloor.Get(TEXT("helpFarm")), 28.0));
					E.GoalBias.Add(TEXT("gatherFood"), 14.0);
					E.GoalBias.Add(TEXT("helpFarm"), 10.0);
					const int32 ActiveSites = ActiveConstructionCount(V);
					if (ActiveSites <= 0)
					{
						E.GoalBias.Set(TEXT("build"), FMath::Max(0.0, E.GoalBias.Get(TEXT("build")) - 10.0));
						E.GoalBias.Set(TEXT("haul"), FMath::Max(0.0, E.GoalBias.Get(TEXT("haul")) - 6.0));
					}
					else
					{
						E.GoalBias.Add(TEXT("haul"), 8.0);
					}
					E.JobBoosts.Add(TEXT("farmer"), 0.45);
					E.bFoodHold = true;
				}
			}

			E.ImmigrationMul = Clamp(E.ImmigrationMul, C::CollectiveImmigrationFloor, C::CollectiveImmigrationCeil);
			if (!E.bFoodHold && Lab < C::CollectiveBandLow && H < C::CollectiveBandElevated && F < C::CollectiveBandElevated)
			{
				E.ImmigrationMul = FMath::Min(C::CollectiveImmigrationCeil, E.ImmigrationMul * 1.05);
			}
			if (!E.bFoodHold
				&& CountPlannedBuildings(V, TEXT("watchtower")) >= 1
				&& CountPlannedBuildings(V, TEXT("lodge")) >= 1
				&& CountPlannedBuildings(V, TEXT("forge")) >= 1
				&& H < C::CollectiveBandElevated
				&& F < C::CollectiveBandElevated)
			{
				E.ImmigrationMul = FMath::Min(C::CollectiveImmigrationCeil, E.ImmigrationMul * 1.08);
				E.GoalBias.Add(TEXT("gatherWood"), 3.0);
				if (CountPlannedBuildings(V, TEXT("tavern")) >= 1)
				{
					E.ImmigrationMul = FMath::Min(C::CollectiveImmigrationCeil, E.ImmigrationMul * 1.06);
				}
			}
			{
				const FForestBrake Brake = ForestGatherBrake(V);
				if (Brake.GatherWoodBias != 0.0) E.GoalBias.Set(TEXT("gatherWood"), E.GoalBias.Get(TEXT("gatherWood")) + Brake.GatherWoodBias);
				if (Brake.JobBoostPenalty != 0.0) E.JobBoosts.Set(TEXT("woodcutter"), E.JobBoosts.Get(TEXT("woodcutter")) + Brake.JobBoostPenalty);
			}
			return E;
		}

		void ApplyDailyFocusToEffects(FCollectiveEffects& E, const TOptional<FDailyFocus>& Focus, FPlannerVillage& V)
		{
			E.DailyFocus = Focus.IsSet() ? Focus->Id : FString();
			if (!Focus.IsSet() || Focus->Id.IsEmpty()) return;
			bool bKnown = false;
			const double Scale = (C::CollectiveDailyFocusFloor != 0.0 ? C::CollectiveDailyFocusFloor : 32.0) / 32.0;
			for (const C::FFocusFloor& Row : C::DailyFocusFloor)
			{
				if (Focus->Id != Row.Focus) continue;
				bKnown = true;
				E.GoalFloor.Set(Row.Goal, FMath::Max(E.GoalFloor.Get(Row.Goal), Round(Row.Points * Scale)));
			}
			if (!bKnown) return;
			if (Focus->Id == TEXT("food"))
			{
				E.GoalBias.Add(TEXT("gatherFood"), 4.0);
				E.GoalBias.Add(TEXT("helpFarm"), 3.0);
				if (Focus->Forced == TEXT("foundingFood"))
				{
					E.GoalFloor.Set(TEXT("build"), FMath::Max(E.GoalFloor.Get(TEXT("build")), 26.0));
					E.GoalBias.Add(TEXT("build"), 6.0);
					E.BuildingBoosts.Set(TEXT("farm"), FMath::Max(E.BuildingBoosts.Get(TEXT("farm")), 42.0));
					E.BuildingBoosts.Set(TEXT("fishery"), FMath::Max(E.BuildingBoosts.Get(TEXT("fishery")), 28.0));
				}
			}
			else if (Focus->Id == TEXT("housing"))
			{
				E.GoalBias.Add(TEXT("build"), 4.0);
				E.GoalBias.Add(TEXT("gatherWood"), 2.0);
			}
			else if (Focus->Id == TEXT("tools"))
			{
				E.GoalBias.Add(TEXT("craft"), 4.0);
				E.JobBoosts.Add(TEXT("blacksmith"), 0.25);
				E.JobBoosts.Add(TEXT("artisan"), 0.2);
				const FString Pending = CraftBootstrapPending(V);
				if (!Pending.IsEmpty())
				{
					E.GoalFloor.Set(TEXT("build"), FMath::Max(E.GoalFloor.Get(TEXT("build")), 28.0));
					E.GoalBias.Add(TEXT("build"), 10.0);
					E.BuildingBoosts.Set(Pending, FMath::Max(E.BuildingBoosts.Get(Pending), 55.0));
					E.JobBoosts.Add(TEXT("builder"), 0.45);
					E.CraftBootstrap = Pending;
				}
				else if (CompletedToolsAtelier(V))
				{
					const double Floor40 = C::CollectiveCraftProofCraftFloor != 0.0 ? C::CollectiveCraftProofCraftFloor : 40.0;
					E.GoalFloor.Set(TEXT("craft"), FMath::Max(E.GoalFloor.Get(TEXT("craft")), Floor40));
					E.GoalBias.Add(TEXT("craft"), 10.0);
					E.JobBoosts.Add(TEXT("artisan"), 0.35);
					E.JobBoosts.Add(TEXT("blacksmith"), 0.35);
					E.bCraftProof = true;
				}
			}
			else if (Focus->Id == TEXT("stone"))
			{
				E.GoalBias.Add(TEXT("gatherStone"), 4.0);
				E.JobBoosts.Add(TEXT("quarryman"), 0.3);
			}
			if (CraftIdle(V) && Focus->Id == TEXT("tools") && E.CraftBootstrap.IsEmpty())
			{
				const double Floor40 = C::CollectiveCraftProofCraftFloor != 0.0 ? C::CollectiveCraftProofCraftFloor : 40.0;
				E.GoalFloor.Set(TEXT("craft"), FMath::Max(E.GoalFloor.Get(TEXT("craft")), Floor40));
				E.bCraftProof = true;
			}
		}
	}

	FOrderedMap ScoreBuildingProjects(FPlannerVillage& V)
	{
		const FOrderedMap Levels = LivePriorityLevels(V);
		const TArray<FJobNeed> Jobs = MeasureJobNeeds(V);
		return ScoreProjects(V, Levels, Jobs);
	}

	FOrderedMap BoostedBuildingScores(FPlannerVillage& V)
	{
		const FOrderedMap Levels = LivePriorityLevels(V);
		const TArray<FJobNeed> Jobs = MeasureJobNeeds(V);
		FOrderedMap Scores = ScoreProjects(V, Levels, Jobs);
		FCollectiveEffects E = ComputeEffects(Levels, Jobs, Scores, V);
		ApplyFounderCharterToEffects(V, E);
		for (const TPair<FString, double>& Boost : E.BuildingBoosts.Items)
		{
			Scores.Set(Boost.Key, FMath::Max(0.0, Round(Scores.Get(Boost.Key) + Boost.Value)));
		}
		return Scores;
	}

	double CollectiveBuildingNeedScore(FPlannerVillage& V)
	{
		if (ActiveConstructionCount(V) > 0) return 85.0;
		const FOrderedMap Scores = BoostedBuildingScores(V);
		double Total = 0.0;
		for (const TPair<FString, double>& S : Scores.Items)
		{
			if (S.Value >= C::CollectiveBuildingPickMin) Total += S.Value;
		}
		double Need = FMath::Min(220.0, Round(Total));
		if (!ExploitSpinePending(V).IsEmpty()
			|| !VillageAmenityPending(V).IsEmpty()
			|| !VillageCraftPending(V).IsEmpty()
			|| !VillageHerdPending(V).IsEmpty()
			|| !CraftBootstrapPending(V).IsEmpty())
		{
			Need = FMath::Max(Need, 72.0);
		}
		return Need;
	}

	const FCollectiveEffects& LiveEffects(FPlannerVillage& V)
	{
		FCollectivePriorities& State = V.Colony.Priorities;
		if (State.Effects.IsSet()) return State.Effects.GetValue();
		FOrderedMap Levels;
		for (const TCHAR* T : PriorityTypes) Levels.Set(T, State.Levels.Get(T));
		FCollectiveEffects E = ComputeEffects(Levels, State.Jobs, State.BuildingScores, V);
		ApplyFounderCharterToEffects(V, E);
		ApplyDailyFocusToEffects(E, State.DailyFocus, V);
		State.Effects = E;
		return State.Effects.GetValue();
	}

	double CollectiveGoalBias(FPlannerVillage& V, const FString& Goal)
	{
		if (!V.bHasColony || Goal.IsEmpty()) return 0.0;
		const FCollectiveEffects& E = LiveEffects(V);
		for (const auto& Pair : SoftKeys)
		{
			if (Goal == Pair[0]) return E.GoalBias.Get(Pair[1]);
		}
		return 0.0;
	}

	double CollectiveGoalFloor(FPlannerVillage& V, const FString& Goal)
	{
		if (!V.bHasColony || Goal.IsEmpty()) return 0.0;
		return LiveEffects(V).GoalFloor.Get(Goal);
	}

	bool IsFoodRush(FPlannerVillage& V)
	{
		if (!V.bHasColony) return false;
		return LiveEffects(V).bFoodRush;
	}

	bool IsWoodBootstrapDraftee(FPlannerVillage& V, const FString& ActorId)
	{
		if (ActorId.IsEmpty()) return false;
		const TArray<FString>* Draft = WoodBootstrapDraft(V);
		return Draft && Draft->Contains(ActorId);
	}

	const FUrgencySnapshot& CollectiveUrgencySnapshot(FPlannerVillage& V)
	{
		const FString Bucket = FString::Printf(TEXT("%d:%lld"), I32(V.Day),
			static_cast<long long>(Floor((V.Time != 0.0 && !FMath::IsNaN(V.Time)) ? V.Time : 0.0)));
		if (V.UrgencyCache.IsSet() && V.UrgencyBucket == Bucket) return V.UrgencyCache.GetValue();
		FUrgencySnapshot U;
		{
			// `collectiveHydrationStress(sim)`.
			const double PopN = V.Actors.Num();
			const double Wells = CountBuildings(V, TEXT("well"));
			const double PlannedWells = CountPlannedBuildings(V, TEXT("well"));
			const double Need = FMath::Max(1.0, Ceil(PopN / 34.0));
			const double CoverageGap = FMath::Max(0.0, Need - Wells);
			const double PlannedGap = FMath::Max(0.0, Need - PlannedWells);
			const double Level = FMath::Min(100.0, Clamp(PlannedGap / Need, 0.0, 1.0) * 72.0 + Clamp(CoverageGap / Need, 0.0, 1.0) * 22.0);
			U.bHydrationActive = Level >= 18.0;
			U.HydrationLevel = Round(Level);
			U.HydrationPlannedGap = PlannedGap;
		}
		{
			// `collectiveHousingSaturation(sim)`.
			const double PopN = V.Actors.Num();
			const double Built = HousingCapacity(V);
			const double Total = Built + PendingHousingCapacity(V);
			const double Deficit = FMath::Max(0.0, PopN - Total);
			const FVacancy Vacancy = HousingVacancySnapshot(V);
			const double DeficitLevel = Clamp(Deficit / FMath::Max(1.0, PopN * 0.16), 0.0, 1.0) * 72.0;
			const double VacancyLevel = Vacancy.bActive
				? Clamp((Vacancy.Vacant * 1.4 + Vacancy.Homeless) / FMath::Max(1.0, PopN * 0.22), 0.0, 1.0) * 28.0
				: 0.0;
			const double Level = FMath::Min(100.0, DeficitLevel + VacancyLevel);
			U.bHousingActive = Level >= 18.0;
			U.HousingLevel = Round(Level);
			U.HousingDeficit = Deficit;
			U.bHousingVacancyActive = Vacancy.bActive;
		}
		{
			// `collectiveAccessStress(sim)`.
			const FHubPull Pull = HubConsolidationPressure(V);
			const FBlindSpots Spots = ColonyStockBlindSpots(V);
			const FStockBlind* TopBlind = Pull.Blind.Num() > 0 ? &Pull.Blind[0] : nullptr;
			const FStockGap* TopGap = Spots.Gaps.FindByPredicate([](const FStockGap& G) { return G.bUnderestimates; });
			if (!TopGap && Spots.Gaps.Num() > 0) TopGap = &Spots.Gaps[0];
			const double Remote = FMath::Max(Pull.Missed, Spots.IgnoredFar);
			const double Level = FMath::Min(100.0,
				Clamp(Remote / 20.0, 0.0, 1.0) * 74.0
				+ Clamp(FMath::Abs(TopGap ? TopGap->Delta : 0.0) / 18.0, 0.0, 1.0) * ((TopGap && TopGap->bUnderestimates) ? 20.0 : 10.0));
			U.bAccessActive = Level >= 18.0;
			U.AccessLevel = Round(Level);
			U.AccessResource = (TopBlind && !TopBlind->Resource.IsEmpty()) ? TopBlind->Resource : (TopGap ? TopGap->Resource : FString());
		}
		V.UrgencyBucket = Bucket;
		V.UrgencyCache = U;
		return V.UrgencyCache.GetValue();
	}

	TMap<FString, double> CollectiveUrgencyBiasMap(FPlannerVillage& V, const FString& ActorId)
	{
		TMap<FString, double> Bias;
		const int32 Index = V.IndexOfActor(ActorId);
		if (Index == INDEX_NONE) return Bias;
		auto AddBias = [&Bias](const TCHAR* Goal, double Value)
		{
			if (!FMath::IsFinite(Value) || Value == 0.0) return;
			Bias.FindOrAdd(Goal) += Value;
		};
		const FUrgencySnapshot U = CollectiveUrgencySnapshot(V);
		if (U.bHydrationActive)
		{
			const double Level = U.HydrationLevel / 100.0;
			AddBias(TEXT("drink"), 14 * Level);
			AddBias(TEXT("build"), 16 * Level);
			AddBias(TEXT("deliver"), 7 * Level);
			AddBias(TEXT("fetchInput"), 6 * Level);
			AddBias(TEXT("haulJob"), 5 * Level);
			AddBias(TEXT("explore"), -13 * Level);
			AddBias(TEXT("gatherWood"), -5 * Level);
			AddBias(TEXT("gatherStone"), -4 * Level);
			if (VillageAmenityPending(V) == TEXT("well") || U.HydrationPlannedGap > 0.0)
			{
				AddBias(TEXT("build"), 8 * Level);
			}
		}
		if (U.bHousingActive)
		{
			const double Level = U.HousingLevel / 100.0;
			AddBias(TEXT("build"), 15 * Level);
			AddBias(TEXT("deliver"), 8 * Level);
			AddBias(TEXT("fetchInput"), 7 * Level);
			AddBias(TEXT("explore"), -10 * Level);
			AddBias(TEXT("craft"), -4 * Level);
			if (U.HousingDeficit > 0.0)
			{
				AddBias(TEXT("gatherWood"), 7 * Level);
				AddBias(TEXT("gatherStone"), 5 * Level);
			}
			if (V.Actors[Index].HomeId.IsEmpty() && U.bHousingVacancyActive)
			{
				AddBias(TEXT("buy"), 12 * Level);
				AddBias(TEXT("visitFamily"), -5 * Level);
			}
		}
		if (U.bAccessActive)
		{
			const double Level = U.AccessLevel / 100.0;
			AddBias(TEXT("haulJob"), 15 * Level);
			AddBias(TEXT("deliver"), 11 * Level);
			AddBias(TEXT("fetchInput"), 10 * Level);
			AddBias(TEXT("explore"), -11 * Level);
			AddBias(TEXT("sell"), -5 * Level);
			if (U.AccessResource == TEXT("food"))
			{
				const int32 StockFood = I32(Market(V, TEXT("food")));
				if (!IsFoodRush(V) && StockFood > 8) AddBias(TEXT("gatherFood"), -8 * Level);
			}
			if (U.AccessResource == TEXT("wood")) AddBias(TEXT("gatherWood"), -6 * Level);
			if (U.AccessResource == TEXT("stone")) AddBias(TEXT("gatherStone"), -6 * Level);
		}
		return Bias;
	}

	FPlannerDecision DecisionFor(FPlannerVillage& V, const FString& ActorId)
	{
		FPlannerDecision D;
		D.BuildingNeedScore = CollectiveBuildingNeedScore(V);
		for (const FString& Goal : GoalBiasGoals()) D.GoalBias.Add(Goal, CollectiveGoalBias(V, Goal));
		if (V.bHasColony)
		{
			for (const TPair<FString, double>& F : LiveEffects(V).GoalFloor.Items) D.GoalFloor.Add(F.Key, CollectiveGoalFloor(V, F.Key));
		}
		D.UrgencyBias = CollectiveUrgencyBiasMap(V, ActorId);
		D.bWoodBootstrapDraftee = IsWoodBootstrapDraftee(V, ActorId);
		D.bFoodRush = IsFoodRush(V);
		D.FarmStaffingGap = FarmStaffingGap(V);
		return D;
	}
}
