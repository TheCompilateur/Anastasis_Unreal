#include "Village/AnastasisVillage.h"
#include "Work/AnastasisCraftMiss.h"

#include "Ai/AnastasisSpatialRisk.h"
#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"
#include "Core/AnastasisStateDigest.h"
#include "World/AnastasisWorld.h"
#include "World/AnastasisWeather.h"
#include "Work/AnastasisFields.h"

namespace AnastasisVillage
{
	using AnastasisMath::Clamp;
	using AnastasisMath::Dist;
	using AnastasisMath::JsHypot;

	namespace
	{
		/** `ACCESS_BUDGET` de navGrid.js, pour les types que ce portage sait poser. */
		int32 AccessBudget(const FString& Type)
		{
			if (Type == WellType) return 4;
			if (Type == HouseType) return 2;
			return 3; // `ACCESS_BUDGET.default`
		}

		/** Types connus du catalogue porte (`BUILDINGS[type]` existe). */
		bool IsKnownType(const FString& Type)
		{
			return Type == WellType || Type == HouseType || Type == GranaryType;
		}

		/** `DOMESTIC_GOALS` restreint aux buts portes : rest, eat, relax. */
		bool IsDomesticGoal(const FString& Goal)
		{
			return Goal == GoalRest || Goal == GoalEat || Goal == GoalRelax;
		}

		/** Activite d'attente et d'interieur (`waitingActivity`). */
		const TCHAR* DomesticActivity(const FString& Goal, bool bNight)
		{
			if (Goal == GoalEat) return TEXT("mange");
			if (Goal == GoalRelax) return TEXT("relaxe");
			return bNight ? TEXT("dort") : TEXT("repose");
		}

		/** `WORKISH` de bridge.js. */
		bool IsWorkish(const FString& Goal)
		{
			static const TCHAR* const Goals[] = {
				TEXT("gatherWood"), TEXT("gatherStone"), TEXT("gatherFood"), TEXT("build"), TEXT("craft"),
				TEXT("maintain"), TEXT("deliver"), TEXT("sell"), TEXT("buy"), TEXT("helpFarm"),
				TEXT("haulJob"), TEXT("fetchInput"), TEXT("apprentice"),
			};
			for (const TCHAR* G : Goals)
			{
				if (Goal == G) return true;
			}
			return false;
		}

		/** Ordre de la table adulte (adultScores, npc.js:1112-1141). */
		const TCHAR* const AdultTable[] = {
			TEXT("eat"), TEXT("eatTogether"), TEXT("rest"), TEXT("relax"), TEXT("relieve"), TEXT("drink"),
			TEXT("gatherWood"), TEXT("gatherStone"), TEXT("gatherFood"), TEXT("helpFarm"), TEXT("sell"),
			TEXT("buy"), TEXT("build"), TEXT("craft"), TEXT("maintain"), TEXT("deliver"), TEXT("fetchInput"),
			TEXT("haulJob"), TEXT("aidHousehold"), TEXT("visitFamily"), TEXT("explore"), TEXT("socialize"),
			TEXT("confront"), TEXT("shelterRain"), TEXT("closeWorkplace"),
		};

		bool IsPortedGoal(const FString& Goal)
		{
			return Goal == GoalEat || Goal == GoalRest || Goal == GoalDrink || Goal == TEXT("gatherFood") || Goal == TEXT("deliver")
				|| Goal == TEXT("socialize") || Goal == TEXT("relax") || Goal == GoalShelterRain;
		}

		FString TargetKey(const FPoint& P)
		{
			return FString::Printf(
				TEXT("%d,%d"),
				static_cast<int32>(AnastasisJs::Floor(P.X)),
				static_cast<int32>(AnastasisJs::Floor(P.Y)));
		}

		int32 FloorInt(double V) { return static_cast<int32>(AnastasisJs::Floor(V)); }

		/** `distToBuilding` — distance au CENTRE du batiment. */
		double DistToBuilding(const FNpc& Npc, const FBuilding& B)
		{
			return JsHypot(Npc.X - (B.X + 0.5), Npc.Y - (B.Y + 0.5));
		}

		/** `restActivity` : « dort » la nuit (pas de gardes dans ce portage), « repose » sinon. */
		const TCHAR* RestActivity(bool bNight)
		{
			return bNight ? TEXT("dort") : TEXT("repose");
		}
	}

	double AiThinkStagger(const FString& Id)
	{
		// h ^= charCodeAt ; h = Math.imul(h, 16777619) — memes bits en uint32.
		uint32 H = 2166136261u;
		for (const TCHAR C : Id)
		{
			H ^= static_cast<uint32>(C);
			H = AnastasisJs::Imul(H, 16777619u);
		}
		return static_cast<double>(H % 1000u) / 1000.0 * ThinkEvery;
	}

	bool NeedsCritical(const AnastasisNeeds::FNeeds& N)
	{
		// Une seule formule : `needsCritical` est porte et prouve dans le module des besoins
		// (`Anastasis.Sim.Parite.BesoinsFacteurs`, 18 cas de part et d'autre des seuils).
		return AnastasisNeeds::AreNeedsCritical(N);
	}

	int32 HousingOfType(const FString& Type)
	{
		// sim/batiments/catalog.js : house `housing: 3`. Le puits n'abrite personne.
		if (Type == HouseType) return 3;
		return 0;
	}

	bool IsFoodGroupType(const FString& Type)
	{
		// catalog.js : granary `group: "food"`. Le puits est civique, la maison un logement.
		return Type == GranaryType;
	}

	bool IsCivicGroupType(const FString& Type)
	{
		// catalog.js : well `group: "civic"` (la taverne aussi, absente ici).
		return Type == WellType;
	}

	double ComputeMealTtlSeconds(double TravelSeconds, double Base)
	{
		if (!FMath::IsFinite(TravelSeconds) || TravelSeconds < 0.0)
		{
			return FMath::Max(5.0, Base);
		}
		const double WithMargin = TravelSeconds * 1.5 + 20.0;
		return FMath::Max(5.0, FMath::Min(Meal::TtlMaxSeconds, FMath::Max(Base, WithMargin)));
	}

	const TArray<FString>& UnportedGoals()
	{
		// Table adulte de npc.js (adultScores), moins `eat`, `rest`, `drink`, `socialize`, `relax`.
		static const TArray<FString> Goals = {
			TEXT("eatTogether"), TEXT("relieve"),
			TEXT("gatherWood"), TEXT("gatherStone"), TEXT("helpFarm"),
			TEXT("sell"), TEXT("buy"), TEXT("build"), TEXT("craft"), TEXT("maintain"),
			TEXT("fetchInput"), TEXT("haulJob"), TEXT("aidHousehold"), TEXT("visitFamily"), TEXT("explore"),
			TEXT("confront"), TEXT("shelterRain"), TEXT("closeWorkplace"),
		};
		return Goals;
	}

	// --- Monde et batiments ---------------------------------------------------

	void FVillage::Bind(const AnastasisWorld::FWorld& InWorld)
	{
		World = &InWorld;
		Nav = AnastasisNav::FNavGrid();
		AnastasisNav::InitFromWorld(Nav, InWorld);
		AnastasisNav::RebuildMoveCosts(Nav, InWorld);
		bTerrainTravelCostEnabled = false;
		MaterialCourierId.Reset();
		NavVersion = 0;
		Settlement = { InWorld.W * 0.5, InWorld.H * 0.5 };
		NextBuildingId = 0;
		NextNpcId = 0;
		Now = 0.0;
		FoodSources.Reset();
		MealReservations.Reset();
		LiveTiles.Reset();
		SoilWaterByTile.Reset();
		bSoilWaterEnabled = false;
		RegrownFood = 0;
		MealSeq = 0;
		ReservationSweepAt = 0.0;
		Buildings = TAnastasisEntityTable<FBuilding>();
		Actors = TAnastasisEntityTable<FNpc>();
		PlayerPersonId.Reset();
		ResetPlayerHand();
	}

	bool FVillage::IsFootBlocked(double InX, double InY) const
	{
		if (!World)
		{
			return true;
		}
		return AnastasisNav::FootBlockedAt(Nav, *World, FloorInt(InX), FloorInt(InY));
	}

	bool FVillage::IsBlocked(double InX, double InY) const
	{
		return AnastasisNav::BlockedAt(Nav, FloorInt(InX), FloorInt(InY));
	}

	bool FVillage::IsFreeCell(int32 TX, int32 TY) const
	{
		if (!World || TX < 0 || TY < 0 || TX >= Nav.W || TY >= Nav.H) return false;
		return !AnastasisNav::FootBlockedAt(Nav, *World, TX, TY);
	}

	FString FVillage::AddBuilding(const FString& Type, int32 TileX, int32 TileY, double Progress, int32 Day)
	{
		if (!World || !Nav.IsInBounds(TileX, TileY))
		{
			return FString();
		}
		const int32 Index = TileY * Nav.W + TileX;
		// Garde du portage : la reference fait verifier le site par son planificateur
		// (non porte). Poser un batiment sur l'eau ou sur un autre batiment n'a pas
		// de sens, on refuse plutot que de corrompre la grille.
		if (AnastasisNav::FootBlockedAt(Nav, *World, TileX, TileY))
		{
			return FString();
		}

		FBuilding Building;
		Building.Id = FString::Printf(TEXT("building-%d"), NextBuildingId++);
		Building.Type = Type;
		Building.X = TileX;
		Building.Y = TileY;
		Building.Progress = Progress;
		Building.CreatedDay = Day;
		Building.HousePhase = 1;
		// `addBuilding` : `if (type === "house") { if (!building.owner) building.vacantSinceDay = this.day }`.
		StampHouseVacant(Building, Day);

		Nav.Blocked[Index] = 1;
		Nav.MoveCost[Index] = std::numeric_limits<float>::infinity();
		// `bumpNavVersion` + `clearNavCache` : les chemins qui traversaient la case sont perimes.
		++NavVersion;

		Building.AccessPoints = ComputeBuildingAccessPoints(Building);
		const FString Id = Building.Id;
		Buildings.Add(MoveTemp(Building));
		return Id;
	}

	bool FVillage::RestoreForHarness(const TArray<FBuilding>& InBuildings, const TArray<FNpc>& InActors,
		const TArray<FMealReservation>& InMeals, int32 InMealSeq, int32 InNextBuildingId, int32 InNextNpcId, FString& OutError)
	{
		if (!World)
		{
			OutError = TEXT("village non lie a un monde");
			return false;
		}
		if (Buildings.Num() > 0 || Actors.Num() > 0)
		{
			OutError = TEXT("village deja peuple : la reprise part d'un village vide (Bind)");
			return false;
		}
		for (const FBuilding& Building : InBuildings)
		{
			const int32 TX = FloorInt(Building.X);
			const int32 TY = FloorInt(Building.Y);
			if (!Nav.IsInBounds(TX, TY))
			{
				OutError = FString::Printf(TEXT("%s hors de la carte (%d, %d)"), *Building.Id, TX, TY);
				return false;
			}
			// `deserialize` : `buildingIndex[index] = building ; blocked[index] = 1`,
			// puis `rebuildMoveCosts` — une case de batiment ne se traverse pas.
			const int32 Index = TY * Nav.W + TX;
			Nav.Blocked[Index] = 1;
			Nav.MoveCost[Index] = std::numeric_limits<float>::infinity();
			++NavVersion;
			Buildings.Add(Building);
		}
		for (const FNpc& Npc : InActors)
		{
			Actors.Add(Npc);
		}
		MealReservations = InMeals;
		MealSeq = InMealSeq;
		NextBuildingId = InNextBuildingId;
		NextNpcId = InNextNpcId;
		return true;
	}

	bool FVillage::RemoveBuilding(const FString& Id)
	{
		const int32 Index = Buildings.IndexOfId(Id);
		if (Index == INDEX_NONE || !World)
		{
			return false;
		}
		const FBuilding& Building = Buildings[Index];
		const int32 TX = FloorInt(Building.X);
		const int32 TY = FloorInt(Building.Y);
		Buildings.RemoveAt(Index);

		if (Nav.IsInBounds(TX, TY))
		{
			const int32 Tile = TY * Nav.W + TX;
			Nav.Blocked[Tile] = World->Tiles[Tile].Type == AnastasisWorld::ETileType::Water ? 1 : 0;
			AnastasisNav::RebuildMoveCosts(Nav, *World);
		}
		++NavVersion;

		// Reservations sur ce grenier : la reference les lache au prochain renouvellement
		// ou balayage (« source_gone ») ; ici tout de suite. Le stock est parti avec lui.
		MealReservations.RemoveAll([&](const FMealReservation& R) { return R.BuildingId == Id; });

		// Aucune reference morte : dedans, foyer, abri, cible, chemin.
		for (FNpc& Npc : Actors.GetItemsMutable())
		{
			bool bLostPlace = false;
			if (Npc.Inside.bActive && Npc.Inside.BuildingId == Id)
			{
				// Sortie par le seuil d'entree ; la case du batiment est libre desormais,
				// et exitBuilding ne retrouverait plus le batiment de toute facon.
				Npc.X = Npc.Inside.ExitX;
				Npc.Y = Npc.Inside.ExitY;
				Npc.Inside = FInside();
				bLostPlace = true;
			}
			if (Npc.HomeId == Id)
			{
				Npc.HomeId.Reset();
			}
			if (Npc.ShelterId == Id)
			{
				Npc.ShelterId.Reset();
			}
			if (Npc.DestBuildingId == Id)
			{
				bLostPlace = true;
			}
			if (Npc.WorkplaceId == Id)
			{
				// Plus de poste : plus de depot ou livrer, plus de recolte pour lui (ecart n°10).
				// Le sac reste plein ; le metier reste, sans lieu.
				Npc.WorkplaceId.Reset();
				ClearWorkSession(Npc);
				if (Npc.Goal == GoalGatherFood || Npc.Goal == GoalDeliver) bLostPlace = true;
			}
			if (bLostPlace)
			{
				ClearNavigation(Npc);
				Npc.bHasTarget = false;
				Npc.DestBuildingId.Reset();
				Npc.Goal = GoalObserver;
				Npc.WorkTimer = 0.0;
				Npc.DoorStuckAt = 0.0;
				Npc.DoorApproachAt = 0.0;
				SetActivity(Npc, TEXT("attend"));
			}
		}
		return true;
	}

	int32 FVillage::CountBuildings(const FString& Type) const
	{
		int32 Count = 0;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Type == Type && B.Progress >= 1.0) ++Count;
		}
		return Count;
	}

	TArray<FPoint> FVillage::ComputeBuildingAccessPoints(const FBuilding& Building) const
	{
		struct FCandidate
		{
			int32 CX;
			int32 CY;
			double Rank;
		};
		TArray<FCandidate> Candidates;
		const int32 BX = FloorInt(Building.X);
		const int32 BY = FloorInt(Building.Y);
		const int32 Budget = AccessBudget(Building.Type);

		auto PushCell = [&](int32 CX, int32 CY, double Rank)
		{
			if (!IsFreeCell(CX, CY)) return;
			for (const FCandidate& C : Candidates)
			{
				if (C.CX == CX && C.CY == CY) return;
			}
			const bool bRoad = World->Tiles[CY * Nav.W + CX].Type == AnastasisWorld::ETileType::Road;
			Candidates.Add({ CX, CY, Rank + (bRoad ? -0.35 : 0.0) });
		};

		// `urbanIntentAccessCandidates` : non porte (ecart n°3), liste vide.

		// `towardSettlementDir`, sans facade d'intention urbaine.
		const double SX = Settlement.X - Building.X;
		const double SY = Settlement.Y - Building.Y;
		int32 PDX = 0;
		int32 PDY = 0;
		if (FMath::Abs(SX) >= FMath::Abs(SY))
		{
			PDX = SX >= 0.0 ? 1 : -1;
		}
		else
		{
			PDY = SY >= 0.0 ? 1 : -1;
		}

		struct FStep { int32 DX; int32 DY; double Rank; };
		TArray<FStep> Cardinals = {
			{ PDX, PDY, 0.0 },
			{ -PDX, -PDY, 0.4 },
			{ PDY, -PDX, 0.55 },
			{ -PDY, PDX, 0.7 },
		};
		for (const FStep& Step : { FStep{ 1, 0, 0.8 }, FStep{ -1, 0, 0.85 }, FStep{ 0, 1, 0.9 }, FStep{ 0, -1, 0.95 } })
		{
			if (!Cardinals.ContainsByPredicate([&](const FStep& C) { return C.DX == Step.DX && C.DY == Step.DY; }))
			{
				Cardinals.Add(Step);
			}
		}
		for (const FStep& C : Cardinals)
		{
			PushCell(BX + C.DX, BY + C.DY, C.Rank);
		}

		const bool bWantDiagonals = Budget >= 4;
		if (bWantDiagonals)
		{
			const int32 Diagonals[4][2] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
			for (const auto& D : Diagonals)
			{
				PushCell(BX + D[0], BY + D[1], 1.2);
			}
		}

		// `candidates.sort((a, b) => a.rank - b.rank)` : tri STABLE depuis ES2019.
		Candidates.StableSort([](const FCandidate& A, const FCandidate& B) { return A.Rank < B.Rank; });

		TArray<FPoint> Out;
		for (int32 I = 0; I < Candidates.Num() && I < Budget; ++I)
		{
			Out.Add({ Candidates[I].CX + 0.5, Candidates[I].CY + 0.5 });
		}
		return Out;
	}

	const TArray<FPoint>& FVillage::EnsureBuildingAccessPoints(FBuilding& Building)
	{
		if (Building.AccessPoints.Num() > 0)
		{
			// Filtrer les seuils devenus bloques (construction voisine).
			TArray<FPoint> Live;
			for (const FPoint& P : Building.AccessPoints)
			{
				if (IsFreeCell(FloorInt(P.X), FloorInt(P.Y))) Live.Add(P);
			}
			if (Live.Num() > 0)
			{
				Building.AccessPoints = MoveTemp(Live);
				return Building.AccessPoints;
			}
		}
		Building.AccessPoints = ComputeBuildingAccessPoints(Building);
		return Building.AccessPoints;
	}

	TMap<FIntPoint, double> FVillage::LocalOccupancy(int32 CX, int32 CY, int32 Radius, const FNpc* Ignored) const
	{
		TMap<FIntPoint, double> Counts;
		auto Bump = [&](int32 X, int32 Y, double Weight)
		{
			if (FMath::Abs(X - CX) > Radius || FMath::Abs(Y - CY) > Radius) return;
			Counts.FindOrAdd(FIntPoint(X, Y)) += Weight;
		};
		for (const FNpc& Actor : Actors.GetItems())
		{
			if (&Actor == Ignored) continue;
			if (Actor.Inside.bActive) continue;
			Bump(FloorInt(Actor.X), FloorInt(Actor.Y), 1.0);
			// La case visee est reservee aussi : sinon trois PNJ choisissent le meme
			// seuil tant qu'aucun n'y est encore arrive.
			if (!Actor.bHasTarget) continue;
			if (Dist(Actor.X, Actor.Y, Actor.Target.X, Actor.Target.Y) <= ArrivalDistance) continue;
			Bump(FloorInt(Actor.Target.X), FloorInt(Actor.Target.Y), 0.85);
		}
		return Counts;
	}

	bool FVillage::PickBuildingAccessPoint(FBuilding& Building, const FNpc* Actor, FPoint& Out, const FPoint* Exclude)
	{
		const TArray<FPoint> Points = EnsureBuildingAccessPoints(Building);
		if (Points.Num() == 0) return false;
		const FPoint Origin = Actor ? FPoint{ Actor->X, Actor->Y } : Settlement;
		const TMap<FIntPoint, double> Occupancy =
			LocalOccupancy(FloorInt(Building.X), FloorInt(Building.Y), 5, Actor);

		bool bFound = false;
		double BestScore = AnastasisNav::Infinity;
		for (const FPoint& Point : Points)
		{
			const int32 TX = FloorInt(Point.X);
			const int32 TY = FloorInt(Point.Y);
			if (!IsFreeCell(TX, TY)) continue;
			if (Exclude && FloorInt(Exclude->X) == TX && FloorInt(Exclude->Y) == TY) continue;
			const double* Crowd = Occupancy.Find(FIntPoint(TX, TY));
			const double Score = Dist(Origin.X, Origin.Y, Point.X, Point.Y) + (Crowd ? *Crowd : 0.0) * 3.2;
			if (Score < BestScore)
			{
				BestScore = Score;
				Out = Point;
				bFound = true;
			}
		}
		return bFound;
	}

	bool FVillage::BuildingAccessPoint(FBuilding& Building, FNpc* Actor, FPoint& Out, const FPoint* Exclude)
	{
		if (PickBuildingAccessPoint(Building, Actor, Out, Exclude))
		{
			if (Actor) Actor->DestBuildingId = Building.Id;
			return true;
		}
		// Batiment emmure : la reference retombe sur la case libre la plus proche.
		if (AccessPointNear(Building.X + 0.5, Building.Y + 0.5, Actor, Out))
		{
			if (Actor) Actor->DestBuildingId = Building.Id;
			return true;
		}
		return false;
	}

	bool FVillage::BuildingAccessPointById(const FString& BuildingId, FNpc* Actor, FPoint& Out)
	{
		FBuilding* Building = Buildings.FindById(BuildingId);
		return Building && BuildingAccessPoint(*Building, Actor, Out);
	}

	bool FVillage::NearestFreePoint(double InX, double InY, int32 MaxRadius, FPoint& Out) const
	{
		const int32 BaseX = FloorInt(InX);
		const int32 BaseY = FloorInt(InY);
		if (!IsFootBlocked(BaseX + 0.5, BaseY + 0.5))
		{
			Out = { BaseX + 0.5, BaseY + 0.5 };
			return true;
		}
		for (int32 Radius = 1; Radius <= MaxRadius; ++Radius)
		{
			for (int32 DY = -Radius; DY <= Radius; ++DY)
			{
				for (int32 DX = -Radius; DX <= Radius; ++DX)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
					const double CX = BaseX + DX + 0.5;
					const double CY = BaseY + DY + 0.5;
					if (CX < 1.0 || CY < 1.0 || CX > Nav.W - 2 || CY > Nav.H - 2) continue;
					if (!IsFootBlocked(CX, CY))
					{
						Out = { CX, CY };
						return true;
					}
				}
			}
		}
		return false;
	}

	namespace Market
	{
		// `CAMP_MARKET_MIN_CHEBYSHEV` (simulation.js l. 781).
		constexpr int32 CampMarketMinChebyshev = 4;

		// `ensureMarketOffset(dx, dy)` (simulation.js l. 783) : le site reserve est strictement au-dela
		// de l'espacement camp / marche, en distance de Chebyshev.
		void EnsureMarketOffset(const TOptional<double>& Dx, const TOptional<double>& Dy, int32& OutX, int32& OutY)
		{
			const int32 MinDist = CampMarketMinChebyshev + 1;
			const int32 FallbackY = -FMath::Max(2, MinDist / 2);
			// `Math.trunc` d'un fini ; `Number.isFinite` faux = repli.
			int32 Ox = (Dx.IsSet() && FMath::IsFinite(*Dx)) ? static_cast<int32>(FMath::TruncToDouble(*Dx)) : MinDist;
			int32 Oy = (Dy.IsSet() && FMath::IsFinite(*Dy)) ? static_cast<int32>(FMath::TruncToDouble(*Dy)) : FallbackY;
			if (Ox == 0 && Oy == 0)
			{
				Ox = MinDist;
				Oy = FallbackY;
			}
			while (FMath::Max(FMath::Abs(Ox), FMath::Abs(Oy)) <= CampMarketMinChebyshev)
			{
				if (FMath::Abs(Ox) >= FMath::Abs(Oy)) Ox += Ox >= 0 ? 1 : -1;
				else Oy += Oy >= 0 ? 1 : -1;
			}
			OutX = Ox;
			OutY = Oy;
		}
	}

	FPoint FVillage::PlannedMarketPos() const
	{
		int32 Dx = 0;
		int32 Dy = 0;
		Market::EnsureMarketOffset(MarketDx, MarketDy, Dx, Dy);
		return { Settlement.X + Dx, Settlement.Y + Dy };
	}

	bool FVillage::MarketAccessPoint(const FNpc* Actor, FPoint& Out) const
	{
		// Aucun type `market` dans ce village : `accessPointNear(plannedMarketPos(), actor)`.
		const FPoint Planned = PlannedMarketPos();
		return AccessPointNear(Planned.X, Planned.Y, Actor, Out);
	}

	bool FVillage::AccessPointNear(double PosX, double PosY, const FNpc* Actor, FPoint& Out) const
	{
		const FPoint Origin = Actor ? FPoint{ Actor->X, Actor->Y } : Settlement;
		const int32 BaseX = FloorInt(PosX);
		const int32 BaseY = FloorInt(PosY);
		const TMap<FIntPoint, double> Occupancy = LocalOccupancy(BaseX, BaseY, 8, Actor);
		const FString JitterId = Actor ? Actor->Id : FString(TEXT("world"));

		// `accessJitter` : hash texte de l'acteur puis de la case, `>>> 0` a chaque etape.
		auto AccessJitter = [&](double CX, double CY)
		{
			double Hash = 0.0;
			for (const TCHAR C : JitterId)
			{
				Hash = AnastasisJs::ToUint32(Hash * 31.0 + static_cast<double>(C));
			}
			Hash = AnastasisJs::ToUint32(Hash + AnastasisJs::Floor(CX) * 73856093.0 + AnastasisJs::Floor(CY) * 19349663.0);
			return static_cast<double>(static_cast<uint32>(Hash) % 1000u) / 1000.0;
		};

		bool bFound = false;
		double BestScore = AnastasisNav::Infinity;
		for (int32 Radius = 1; Radius <= 12; ++Radius)
		{
			for (int32 DY = -Radius; DY <= Radius; ++DY)
			{
				for (int32 DX = -Radius; DX <= Radius; ++DX)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
					const double CX = BaseX + DX + 0.5;
					const double CY = BaseY + DY + 0.5;
					if (CX < 1.0 || CY < 1.0 || CX > Nav.W - 2 || CY > Nav.H - 2) continue;
					if (IsFootBlocked(CX, CY)) continue;
					const double* Crowd = Occupancy.Find(FIntPoint(FloorInt(CX), FloorInt(CY)));
					const double Score = Dist(Origin.X, Origin.Y, CX, CY)
						+ JsHypot(DX, DY) * 0.35
						+ (Crowd ? *Crowd : 0.0) * 3.2
						+ AccessJitter(CX, CY) * 2.4;
					if (Score < BestScore)
					{
						BestScore = Score;
						Out = { CX, CY };
						bFound = true;
					}
				}
			}
			if (bFound) return true;
		}
		// Ultime repli : case libre la plus proche, JAMAIS le centre bloque.
		if (NearestFreePoint(BaseX + 0.5, BaseY + 0.5, 28, Out)) return true;
		if (!IsFootBlocked(Origin.X, Origin.Y))
		{
			Out = Origin;
			return true;
		}
		return NearestFreePoint(Settlement.X, Settlement.Y, 16, Out);
	}

	bool FVillage::AtDrinkSpot(double InX, double InY) const
	{
		if (!World) return false;
		const int32 TX = FloorInt(InX);
		const int32 TY = FloorInt(InY);
		if (Nav.IsInBounds(TX, TY))
		{
			const AnastasisWorld::FTile& Tile = World->Tiles[TY * Nav.W + TX];
			if (Tile.Type == AnastasisWorld::ETileType::Water || Tile.Shore >= ShoreReach) return true;
		}
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Type != WellType || B.Progress < 1.0) continue;
			const double DX = B.X + 0.5 - InX;
			const double DY = B.Y + 0.5 - InY;
			if (DX * DX + DY * DY <= WellReach * WellReach) return true;
		}
		return false;
	}

	const FBuilding* FVillage::NearestWell(double FromX, double FromY) const
	{
		const FBuilding* Best = nullptr;
		double BestDist = AnastasisNav::Infinity;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Type != WellType || B.Progress < 1.0) continue;
			const double DX = (B.X + 0.5) - FromX;
			const double DY = (B.Y + 0.5) - FromY;
			const double D = DX * DX + DY * DY;
			if (D < BestDist)
			{
				BestDist = D;
				Best = &B;
			}
		}
		return Best;
	}

	bool FVillage::DrinkTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource)
	{
		// `rhythmTarget` (couche qui l'emporte dans resolveTarget) : s'il existe un
		// puits acheve, c'est lui, le plus proche, quelle que soit la base.
		if (const FBuilding* Well = NearestWell(Npc.X, Npc.Y))
		{
			if (BuildingAccessPointById(Well->Id, &Npc, OutTarget))
			{
				OutSource = TEXT("well");
				return true;
			}
		}

		// Sans puits : `drinkAccessPoint`, balayage des berges et de l'eau a 14 cases.
		// (`bestKnownWater`, la croyance, n'est pas portee : il faut la perception.)
		const int32 OX = FloorInt(Npc.X);
		const int32 OY = FloorInt(Npc.Y);
		constexpr int32 Radius = 14;
		bool bFound = false;
		double BestScore = AnastasisNav::Infinity;
		for (int32 Y = OY - Radius; Y <= OY + Radius; ++Y)
		{
			for (int32 X = OX - Radius; X <= OX + Radius; ++X)
			{
				if (!Nav.IsInBounds(X, Y)) continue;
				const AnastasisWorld::FTile& Tile = World->Tiles[Y * Nav.W + X];
				const bool bWater = Tile.Type == AnastasisWorld::ETileType::Water;
				if (!bWater && Tile.Shore < 0.3) continue;
				const double DX = X + 0.5 - Npc.X;
				const double DY = Y + 0.5 - Npc.Y;
				const double Score = DX * DX + DY * DY + (bWater ? 2.5 : 0.0);
				if (Score >= BestScore) continue;
				FPoint Spot = { X + 0.5, Y + 0.5 };
				if (bWater && !AccessPointNear(X + 0.5, Y + 0.5, &Npc, Spot)) continue;
				BestScore = Score;
				OutTarget = Spot;
				bFound = true;
			}
		}
		if (bFound)
		{
			OutSource = TEXT("shore");
			return true;
		}
		OutSource = TEXT("none");
		return false;
	}

	// --- Foyer (life/domestic.js) ----------------------------------------------

	int32 FVillage::ShelterCapacity(const FBuilding& Building) const
	{
		if (Building.Type == HouseType)
		{
			// `HOUSE_PHASES[phase - 1].capacity` ; seule la phase 1 existe dans ce portage.
			return HousePhaseOneCapacity + FMath::Clamp(Building.HousePhase, 1, 6) - 1;
		}
		return HousingOfType(Building.Type);
	}

	int32 FVillage::CountShelterOccupants(const FString& BuildingId) const
	{
		int32 N = 0;
		for (const FNpc& Actor : Actors.GetItems())
		{
			if (Actor.HomeId == BuildingId || Actor.ShelterId == BuildingId) ++N;
		}
		return N;
	}

	TArray<FString> FVillage::InsideOf(const FString& BuildingId) const
	{
		TArray<FString> Ids;
		for (const FNpc& Actor : Actors.GetItems())
		{
			if (Actor.Inside.bActive && Actor.Inside.BuildingId == BuildingId) Ids.Add(Actor.Id);
		}
		return Ids;
	}

	double FVillage::SleepQualityOf(const FNpc& Npc)
	{
		return AnastasisNeeds::SleepQuality(
			Npc.Inside.bActive ? Npc.Inside.BuildingId : FString(), Npc.HomeId, Npc.ShelterId);
	}

	bool FVillage::IsEnterableHousing(const FBuilding& Building) const
	{
		if (Building.Progress < 1.0) return false;
		return HousingOfType(Building.Type) > 0;
	}

	const FBuilding* FVillage::NearLivingHome(const FNpc& Npc) const
	{
		const FString& Living = Npc.LivingHomeId();
		if (Living.IsEmpty()) return nullptr;
		const FBuilding* Home = Buildings.FindById(Living);
		if (!Home) return nullptr;
		return DistToBuilding(Npc, *Home) <= AnastasisNeeds::Domestic::HomeEnterRadius ? Home : nullptr;
	}

	const FBuilding* FVillage::NearestHousing(const FNpc& Npc) const
	{
		const FBuilding* Best = nullptr;
		double BestDist = AnastasisNav::Infinity;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Progress < 1.0 || HousingOfType(B.Type) <= 0) continue;
			if (B.Type == HouseType && !B.Owner.IsEmpty() && B.Owner != Npc.Id) continue;
			const double DX = (B.X + 0.5) - Npc.X;
			const double DY = (B.Y + 0.5) - Npc.Y;
			const double D = DX * DX + DY * DY;
			const double Bias = 2.0; // dortoir 0, le reste 2 ; pas de dortoir dans ce portage
			if (D + Bias < BestDist)
			{
				BestDist = D + Bias;
				Best = &B;
			}
		}
		return Best;
	}

	const FBuilding* FVillage::FindOpenShelter(const FNpc& Npc) const
	{
		const FBuilding* Best = nullptr;
		double BestScore = AnastasisNav::Infinity;
		const FString& Living = Npc.LivingHomeId();
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (!IsEnterableHousing(B)) continue;
			// Maison d'un autre foyer : pas d'intrusion (les maisons du joueur restent cedables).
			if (B.Type == HouseType && !B.Owner.IsEmpty() && B.Owner != Npc.Id && B.Owner != TEXT("player")) continue;
			const int32 Occupants = CountShelterOccupants(B.Id);
			const int32 Cap = ShelterCapacity(B);
			if (Occupants >= Cap && Living != B.Id) continue;
			const double D = DistToBuilding(Npc, B);
			const double Bias = B.Type == HouseType ? 4.0 : 12.0; // dortoir 0
			const double Score = D + Bias + Occupants * 0.35;
			if (Score < BestScore)
			{
				BestScore = Score;
				Best = &B;
			}
		}
		return Best;
	}

	bool FVillage::AssignHome(const FString& NpcId, const FString& HouseId)
	{
		FBuilding* House = Buildings.FindById(HouseId);
		FNpc* Npc = Actors.FindById(NpcId);
		if (!House || !Npc || House->Type != HouseType || !House->IsCompleted())
		{
			return false;
		}
		if (!House->Owner.IsEmpty() && House->Owner != NpcId)
		{
			return false;
		}
		House->Owner = NpcId;
		StampHouseOccupied(*House);
		Npc->HomeId = HouseId;
		// `owner.morale = clamp(owner.morale + 12, 0, 100)` — sans repli sur 50.
		Npc->Needs.Morale = Clamp(Npc->Needs.Morale + 12.0, 0.0, 100.0);
		return true;
	}

	int32 FVillage::AssignSheltersDaily()
	{
		int32 Assigned = 0;
		// La liste des sans-toit est prise AVANT la liberation des abris, comme la reference.
		TArray<FString> Homeless;
		for (const FNpc& Npc : Actors.GetItems())
		{
			if (Npc.HomeId.IsEmpty() && Npc.ShelterId.IsEmpty()) Homeless.Add(Npc.Id);
		}
		for (FNpc& Npc : Actors.GetItemsMutable())
		{
			if (!Npc.HomeId.IsEmpty() && !Npc.ShelterId.IsEmpty()) Npc.ShelterId.Reset();
			if (!Npc.ShelterId.IsEmpty())
			{
				const FBuilding* Shelter = Buildings.FindById(Npc.ShelterId);
				if (!Shelter || !IsEnterableHousing(*Shelter)) Npc.ShelterId.Reset();
			}
		}
		for (const FString& Id : Homeless)
		{
			FNpc* Npc = Actors.FindById(Id);
			if (!Npc) continue;
			const FBuilding* Bed = FindOpenShelter(*Npc);
			if (!Bed) break;
			Npc->ShelterId = Bed->Id;
			const double Morale = Npc->Needs.Morale != 0.0 ? Npc->Needs.Morale : 50.0; // `npc.morale || 50`
			Npc->Needs.Morale = Clamp(Morale + 3.0, 0.0, 100.0);
			++Assigned;
		}
		return Assigned;
	}

	bool FVillage::RestTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource)
	{
		// Base (`bestKnownBed || home access || accessPointNear(settlement)`) : toujours
		// recouverte par la couche rythme ci-dessous, qui ne rend jamais rien de vide.
		bool bHave = false;

		// Couche rythme (`rhythmTarget`, branche rest).
		if (!Npc.HomeId.IsEmpty() && BuildingAccessPointById(Npc.HomeId, &Npc, OutTarget))
		{
			bHave = true;
			OutSource = TEXT("home");
		}
		else if (!Npc.ShelterId.IsEmpty() && BuildingAccessPointById(Npc.ShelterId, &Npc, OutTarget))
		{
			bHave = true;
			OutSource = TEXT("shelter");
		}
		else if (const FBuilding* Housing = NearestHousing(Npc))
		{
			if (BuildingAccessPointById(Housing->Id, &Npc, OutTarget))
			{
				bHave = true;
				OutSource = TEXT("housing");
			}
		}
		if (!bHave && AccessPointNear(Settlement.X, Settlement.Y, &Npc, OutTarget))
		{
			bHave = true;
			OutSource = TEXT("settlement");
		}

		// Couche domestique (`domesticTarget`) : le foyer, sinon un abri ouvert.
		const FString Living = Npc.LivingHomeId();
		FPoint Domestic;
		if (!Living.IsEmpty())
		{
			if (BuildingAccessPointById(Living, &Npc, Domestic))
			{
				OutTarget = Domestic;
				bHave = true;
				OutSource = Living == Npc.HomeId ? TEXT("home") : TEXT("shelter");
			}
		}
		else if (const FBuilding* Open = FindOpenShelter(Npc))
		{
			if (BuildingAccessPointById(Open->Id, &Npc, Domestic))
			{
				OutTarget = Domestic;
				bHave = true;
				OutSource = TEXT("open-shelter");
			}
		}
		if (!bHave)
		{
			OutSource = TEXT("none");
		}
		return bHave;
	}

	// --- Interieur -------------------------------------------------------------

	const FBuilding* FVillage::BuildingNearActor(const FNpc& Npc, double Radius) const
	{
		const FBuilding* Best = nullptr;
		double BestScore = Radius;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Progress < 1.0) continue;
			const double TargetDistance = Npc.bHasTarget
				? JsHypot(Npc.Target.X - (B.X + 0.5), Npc.Target.Y - (B.Y + 0.5))
				: AnastasisNav::Infinity;
			const double ActorDistance = JsHypot(Npc.X - (B.X + 0.5), Npc.Y - (B.Y + 0.5));
			const double Score = FMath::Min(ActorDistance, TargetDistance * 0.85);
			if (Score < BestScore)
			{
				Best = &B;
				BestScore = Score;
			}
		}
		return Best;
	}

	namespace PlaceUse
	{
		// `WORK_LABOR_KINDS` (simulation.js l. 498) : activites ET buts qui comptent comme travail.
		bool IsWorkKind(const FString& Kind)
		{
			static const TCHAR* const Kinds[] = {
				TEXT("travaille"), TEXT("fabrique"), TEXT("forge"), TEXT("tanne"), TEXT("tisse"), TEXT("scie"),
				TEXT("repare"), TEXT("chantier"), TEXT("bucher"), TEXT("recolte"), TEXT("taille"), TEXT("peche"),
				TEXT("mene"), TEXT("build"), TEXT("craft"), TEXT("maintain"), TEXT("helpFarm"), TEXT("gatherWood"),
				TEXT("gatherFood"), TEXT("gatherStone"), TEXT("apprentice") };
			for (const TCHAR* K : Kinds) if (Kind.Equals(K, ESearchCase::CaseSensitive)) return true;
			return false;
		}
		bool IsOneOf(const FString& Kind, std::initializer_list<const TCHAR*> List)
		{
			for (const TCHAR* K : List) if (Kind.Equals(K, ESearchCase::CaseSensitive)) return true;
			return false;
		}
		// `BUILDINGS[type].jobs.length` : dans le catalogue de la reference, seule la maison n'a pas
		// de metier (`catalog.js`).
		bool TypeHasJobs(const FString& Type) { return !Type.IsEmpty() && Type != HouseType; }
	}

	void FVillage::NotePlaceUse(FNpc& Npc, const FString& Kind, double Amount)
	{
		const bool bWork = PlaceUse::IsWorkKind(Kind);
		// `actor.workPresence` n'est pas porte (ecart n°26) : dedans, sinon le plus proche (1,9).
		FBuilding* Building = Npc.Inside.bActive ? Buildings.FindById(Npc.Inside.BuildingId) : nullptr;
		if (!Building)
		{
			const FBuilding* Near = BuildingNearActor(Npc, 1.9);
			Building = Near ? Buildings.FindById(Near->Id) : nullptr;
		}
		FBuilding* Workplace = Npc.WorkplaceId.IsEmpty() ? nullptr : Buildings.FindById(Npc.WorkplaceId);
		// Recolte / soins hors poste : le depot metier compte quand meme.
		if (!Building && bWork && Workplace && Workplace->Progress >= 1.0) Building = Workplace;
		if (!Building) return;

		const double DayNow = static_cast<double>(Day());
		FNpc::FPlaceEntry* Entry = Npc.PlaceEntries.FindByPredicate([Building](const FNpc::FPlaceEntry& E) { return E.BuildingId == Building->Id; });
		if (!Entry)
		{
			Entry = &Npc.PlaceEntries.AddDefaulted_GetRef();
			Entry->BuildingId = Building->Id;
			Entry->Type = Building->Type;
			Entry->DecayDay = DayNow;
			Entry->LastDay = DayNow;
		}
		const double ElapsedDays = FMath::Max(0.0, DayNow - Entry->DecayDay);
		if (ElapsedDays > 0.0)
		{
			const double Decay = AnastasisJs::Pow(0.76, FMath::Min(5.0, ElapsedDays));
			Entry->Score *= Decay;
			Entry->Work *= Decay;
			Entry->Social *= Decay;
			Entry->Home *= Decay;
			Entry->Talk *= Decay;
			Entry->Drink *= Decay;
			Entry->Activity *= Decay;
			Entry->Crisis *= Decay;
			Entry->DecayDay = DayNow;
		}
		const double Gain = FMath::Max(0.05, Amount);
		Entry->Score = FMath::Min(80.0, Entry->Score + Gain);
		Entry->Activity = FMath::Min(36.0, Entry->Activity + Gain);
		Entry->LastDay = DayNow;
		Entry->Type = Building->Type;
		if (bWork) Entry->Work += Gain;
		if (PlaceUse::IsOneOf(Kind, { TEXT("socialise"), TEXT("socialize"), TEXT("visitFamily"), TEXT("play"), TEXT("relax"), TEXT("relaxe") })) Entry->Social += Gain;
		if (PlaceUse::IsOneOf(Kind, { TEXT("socialise"), TEXT("socialize"), TEXT("talk"), TEXT("visitFamily"), TEXT("play") })) Entry->Talk += Gain;
		if (PlaceUse::IsOneOf(Kind, { TEXT("drink"), TEXT("boit") })) Entry->Drink += Gain;
		if (PlaceUse::IsOneOf(Kind, { TEXT("repose"), TEXT("rest"), TEXT("dort"), TEXT("relaxe"), TEXT("relax") }) || Npc.HomeId == Building->Id) Entry->Home += Gain;
		// `(actor.morale || 100) < 18` : un moral nul compte pour 100.
		const double Morale = Npc.Needs.Morale != 0.0 ? Npc.Needs.Morale : 100.0;
		if (Npc.Needs.Hunger > 88.0 || Morale < 18.0) Entry->Crisis = FMath::Min(24.0, Entry->Crisis + Gain * 0.6);
		// `lifestyleNotePlaceUse` : seulement pour un habitant qui a un mode de vie. Sans lui, la
		// reference en tirerait un dans le flux de secours ; ceux du C++ restent sans (ecart n°8).
		if (Npc.Lifestyle.IsSet())
		{
			AnastasisLifestyle::FLifestyleSubject Subject;
			Subject.Goal = Npc.Goal;
			Subject.JobId = Npc.JobId;
			Subject.HomeId = Npc.HomeId;
			Subject.Skill = Npc.Skill;
			Subject.Energy = Npc.Needs.Energy;
			Subject.bHasTarget = Npc.bHasTarget;
			Subject.FavoriteBuildingId = Npc.FavoriteBuildingId;
			AnastasisLifestyle::FPlaceUseEntry Use;
			Use.BuildingId = Entry->BuildingId;
			Use.Work = Entry->Work;
			Use.Social = Entry->Social;
			Use.Home = Entry->Home;
			Use.Lifestyle = Entry->Lifestyle;
			AnastasisLifestyle::LifestyleNotePlaceUse(Npc.Lifestyle, Subject, Kind, Use, Gain);
			Entry->Work = Use.Work;
			Entry->Social = Use.Social;
			Entry->Home = Use.Home;
			Entry->Lifestyle = MoveTemp(Use.Lifestyle);
		}

		// Le travail du jour alimente la production quotidienne du poste.
		if (bWork)
		{
			FBuilding* Post = (Workplace && Workplace->Progress >= 1.0) ? Workplace
				: (PlaceUse::TypeHasJobs(Building->Type) ? Building : nullptr);
			if (Post && Post->Progress >= 1.0 && PlaceUse::TypeHasJobs(Post->Type))
			{
				Post->LaborToday += Gain;
				Post->bHasLaborToday = true;
			}
		}

		// Le favori : le meilleur score parmi les lieux vus depuis 18 jours, le premier a egalite.
		const FNpc::FPlaceEntry* Favorite = nullptr;
		for (const FNpc::FPlaceEntry& Remembered : Npc.PlaceEntries)
		{
			const double Last = Remembered.LastDay != 0.0 ? Remembered.LastDay : DayNow;
			if (DayNow - Last > 18.0) continue;
			if (!Favorite || Remembered.Score > Favorite->Score) Favorite = &Remembered;
		}
		Npc.FavoriteBuildingId = (Favorite && !Favorite->BuildingId.IsEmpty()) ? Favorite->BuildingId : Building->Id;
	}

	const FBuilding* FVillage::BuildingForIndoorAction(const FNpc& Npc, const FString& Goal) const
	{
		// Foyer d'abord : dormir, manger se font DANS la maison ou l'abri.
		const FString& Living = Npc.LivingHomeId();
		if (!Living.IsEmpty())
		{
			const FBuilding* Home = Buildings.FindById(Living);
			if (Home && Home->Progress >= 1.0
				&& DistToBuilding(Npc, *Home) <= AnastasisNeeds::Domestic::HomeEnterRadius)
			{
				return Home;
			}
		}
		// (Poste de travail : pas de metier dans ce portage.)
		const FBuilding* Building = BuildingNearActor(Npc, IndoorBuildingRadius);
		if (!Building || Building->Progress < 1.0) return nullptr;
		if (!IsKnownType(Building->Type)) return nullptr;
		const bool bAtOwn = !Living.IsEmpty() && Living == Building->Id;
		// AUCUN controle de proprietaire ni de capacite : c'est la reference.
		if (Goal == GoalEat)
		{
			// `afford(atOwn || data.group === "food" || data.group === "trade" || data.housing)`.
			return bAtOwn || IsFoodGroupType(Building->Type) || HousingOfType(Building->Type) > 0 ? Building : nullptr;
		}
		// `relax` : afford(atOwn || socialiser || trade || housing) — ni lieu de rencontre ni
		// commerce dans ce portage. `rest` / `relieve` : afford(atOwn || housing).
		return bAtOwn || HousingOfType(Building->Type) > 0 ? Building : nullptr;
	}

	bool FVillage::EnterBuilding(FNpc& Npc, const FBuilding& Building, const FString& InActivity, double Duration)
	{
		if (Building.Progress < 1.0 || Npc.Inside.bActive) return false;
		FPoint Entrance = { Npc.X, Npc.Y };
		if (IsBlocked(Npc.X, Npc.Y))
		{
			FPoint Door;
			if (BuildingAccessPointById(Building.Id, &Npc, Door)) Entrance = Door;
		}
		Npc.X = Entrance.X;
		Npc.Y = Entrance.Y;
		Npc.Inside.bActive = true;
		Npc.Inside.BuildingId = Building.Id;
		Npc.Inside.Activity = InActivity;
		Npc.Inside.Goal = Npc.Goal;
		Npc.Inside.EnteredAt = Now;
		Npc.Inside.Until = Now + FMath::Max(0.45, Duration);
		Npc.Inside.ExitX = Entrance.X;
		Npc.Inside.ExitY = Entrance.Y;
		Npc.bHasTarget = false;
		ClearNavigation(Npc);
		// `enterBuilding` : l'entree compte au lieu (`notePlaceUse(actor, activity, 0.8)`).
		NotePlaceUse(Npc, InActivity, 0.8);
		return true;
	}

	bool FVillage::ExitBuilding(FNpc& Npc)
	{
		if (!Npc.Inside.bActive) return false;
		const FString BuildingId = Npc.Inside.BuildingId;
		const double ExitX = Npc.Inside.ExitX;
		const double ExitY = Npc.Inside.ExitY;
		Npc.Inside = FInside();
		if (FMath::IsFinite(ExitX) && FMath::IsFinite(ExitY) && !IsBlocked(ExitX, ExitY))
		{
			Npc.X = ExitX;
			Npc.Y = ExitY;
		}
		else
		{
			FPoint Exit;
			if (BuildingAccessPointById(BuildingId, &Npc, Exit))
			{
				Npc.X = Exit.X;
				Npc.Y = Exit.Y;
			}
		}
		Npc.Path.Reset();
		Npc.PathStep = 0;
		Npc.bHasPathGoal = false;
		return true;
	}

	const FBuilding* FVillage::ShelterBuildingNearActor(const FNpc& Npc) const
	{
		// `buildingForIndoorAction(actor, "shelterRain")` : pas un but domestique, donc pas de
		// foyer d'abord. Son poste a 3,4 tuiles (`workplaceAcceptsIndoorGoal` : toujours vrai
		// pour shelterRain), sinon le batiment a portee, s'il abrite.
		if (!Npc.WorkplaceId.IsEmpty())
		{
			const FBuilding* Post = Buildings.FindById(Npc.WorkplaceId);
			if (Post && Post->Progress >= 1.0 && DistToBuilding(Npc, *Post) <= 3.4) return Post;
		}
		const FBuilding* Building = BuildingNearActor(Npc, IndoorBuildingRadius);
		if (!Building || Building->Progress < 1.0 || !IsKnownType(Building->Type)) return nullptr;
		const FString& Living = Npc.LivingHomeId();
		const bool bAtOwn = !Living.IsEmpty() && Living == Building->Id;
		// `afford(atOwn || housing || socialiser || trade || civic || fabriquer)`.
		return bAtOwn || HousingOfType(Building->Type) > 0 || IsCivicGroupType(Building->Type) ? Building : nullptr;
	}

	bool FVillage::ShelterRainTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource)
	{
		// `shelterRainAccess` : lit connu (non porte : le foyer en tient lieu), foyer, taverne
		// (absente), poste, premier batiment couvert, sinon la place.
		const FString& Living = Npc.LivingHomeId();
		if (!Living.IsEmpty())
		{
			const FBuilding* Home = Buildings.FindById(Living);
			if (Home && Home->Progress >= 1.0 && BuildingAccessPointById(Living, &Npc, OutTarget))
			{
				Npc.DestBuildingId = Living;
				OutSource = TEXT("home");
				return true;
			}
		}
		if (!Npc.WorkplaceId.IsEmpty())
		{
			const FBuilding* Post = Buildings.FindById(Npc.WorkplaceId);
			if (Post && Post->Progress >= 1.0 && BuildingAccessPointById(Npc.WorkplaceId, &Npc, OutTarget))
			{
				Npc.DestBuildingId = Npc.WorkplaceId;
				OutSource = TEXT("workplace");
				return true;
			}
		}
		for (const FBuilding& B : Buildings.GetItems())
		{
			// `(b.progress ?? 1) >= 1 && (housing || socialiser || trade || civic)`, dans l'ordre de `sim.buildings`.
			if (B.Progress < 1.0) continue;
			if (HousingOfType(B.Type) > 0 || IsCivicGroupType(B.Type))
			{
				const FString Id = B.Id;
				if (BuildingAccessPointById(Id, &Npc, OutTarget))
				{
					Npc.DestBuildingId = Id;
					OutSource = TEXT("roofed");
					return true;
				}
			}
		}
		return SocialPos(Npc, OutTarget, OutSource);
	}

	bool FVillage::PerformShelterRain(FNpc& Npc)
	{
		// Encore dehors : garder la cible abri (l'entree passe par TryEnterIndoorAction) et ne
		// PAS reussir au seuil, sinon l'acte serait valide sans toit.
		if (!Npc.Inside.bActive)
		{
			if (!Npc.bHasTarget)
			{
				FPoint Access;
				FString Source;
				if (ShelterRainTarget(Npc, Access, Source))
				{
					Npc.bHasTarget = true;
					Npc.Target = Access;
				}
			}
			return false;
		}
		Npc.Needs.Energy = Clamp(Npc.Needs.Energy + AnastasisWeatherBehavior::Shelter::EnergyRecover, 0.0, 100.0);
		Npc.Needs.Morale = Clamp(Npc.Needs.Morale + 2.0, 0.0, 100.0);
		++Npc.SheltersTaken;
		const FString Resume = Npc.ShelterResumeGoal;
		Npc.ShelterResumeGoal.Reset();
		Npc.ShelterCooldownUntil = Now + AnastasisWeatherBehavior::Shelter::CooldownSeconds;
		// Reprendre le travail expose ; sinon `craft` dans la reference, non porte : observer.
		Npc.Goal = !Resume.IsEmpty() && Resume != GoalShelterRain && AnastasisWeatherBehavior::IsRainExposedGoal(Resume)
			&& IsPortedGoalFor(Npc, Resume)
			? Resume : FString(GoalObserver);
		Npc.GoalSince = Now;
		return true;
	}

	bool FVillage::TryEnterIndoorAction(FNpc& Npc)
	{
		if (Npc.Inside.bActive) return false;
		if (Npc.Goal == GoalShelterRain)
		{
			// `indoorDuration(shelterRain)` = shelterRainDuration ; `waitingActivity` = « abrite ».
			const FBuilding* Shelter = ShelterBuildingNearActor(Npc);
			return Shelter && EnterBuilding(Npc, *Shelter, TEXT("abrite"),
				AnastasisWeatherBehavior::ShelterRainDuration(TickWeather.Rain));
		}
		if (!IsDomesticGoal(Npc.Goal)) return false;
		// Supply scenario: no hunger reduction by sitting in an empty granary.
		// Preserve the reference-only granary/house fixtures when supply is inactive.
		if (FoodSources.Num() > 0 && Npc.Goal == GoalEat && !FindMealReservation(Npc.Id)) return false;
		const FBuilding* Building = NearLivingHome(Npc);
		if (!Building) Building = BuildingForIndoorAction(Npc, Npc.Goal);
		if (!Building)
		{
			// Au seuil du foyer meme si son centre est un peu loin.
			const FString Living = Npc.LivingHomeId();
			const FBuilding* Home = Living.IsEmpty() ? nullptr : Buildings.FindById(Living);
			if (Home && Home->Progress >= 1.0)
			{
				FPoint Access = Npc.Target;
				const bool bAccess = Npc.bHasTarget || BuildingAccessPointById(Living, &Npc, Access);
				if (bAccess && Dist(Npc.X, Npc.Y, Access.X, Access.Y) <= DoorAccessRadius) Building = Home;
			}
		}
		if (!Building) return false;
		// Poste de service : rester un peu plus longtemps (on « tient » le lieu).
		const double AtPost = !Npc.WorkplaceId.IsEmpty() && Npc.WorkplaceId == Building->Id ? 1.25 : 1.0;
		if (Npc.Goal == GoalEat)
		{
			// `indoorNeedDuration(eat)` = eatDuration ; `waitingActivity(eat)` = « mange ».
			return EnterBuilding(Npc, *Building, TEXT("mange"), AnastasisNeeds::Constants::EatDuration * AtPost);
		}
		if (Npc.Goal == GoalRelax)
		{
			// `indoorNeedDuration(relax)` = relaxDuration.
			return EnterBuilding(Npc, *Building, TEXT("relaxe"), AnastasisNeeds::Constants::RelaxDuration * AtPost);
		}
		// `indoorNeedDuration(rest)` : la nuit se decide UNE fois, a l'entree.
		const bool bNight = IsNight();
		const double Duration = bNight ? AnastasisNeeds::Constants::SleepDuration : AnastasisNeeds::Constants::NapDuration;
		return EnterBuilding(Npc, *Building, RestActivity(bNight), Duration * AtPost);
	}

	void FVillage::UpdateInside(FNpc& Npc, double Dt)
	{
		SetActivity(Npc, Npc.Inside.Activity);
		// `notePlaceUse(npc, npc.activity, dt * 0.5)` a chaque tick dedans, puis, l'acte fait et
		// AVANT la sortie, `notePlaceUse(npc, inside.goal || npc.goal, 1.2)`.
		NotePlaceUse(Npc, Npc.Activity, Dt * 0.5);
		if (Now < Npc.Inside.Until) return;
		const FString InsideGoal = !Npc.Inside.Goal.IsEmpty() ? Npc.Inside.Goal : Npc.Goal;
		const bool bWorked = Perform(Npc);
		if (bWorked) NotePlaceUse(Npc, InsideGoal, 1.2);
		ExitBuilding(Npc);
		if (!bWorked)
		{
			SetActivity(Npc, TEXT("attend"));
			if (++Npc.FailedActions >= 3) RedirectAfterFailure(Npc);
		}
		else
		{
			Npc.FailedActions = 0;
		}
		// La cible a ete effacee a l'entree : la prochaine pensee redecidera.
		Npc.bHasTarget = false;
	}

	// --- Habitants -------------------------------------------------------------

	FString FVillage::SpawnNpc(double InX, double InY, const AnastasisNeeds::FNeeds& Needs, double Speed)
	{
		FNpc Npc;
		Npc.Id = FString::Printf(TEXT("npc-%d"), NextNpcId++);
		Npc.X = InX;
		Npc.Y = InY;
		Npc.Speed = Speed;
		Npc.Needs = Needs;
		// createNpc : competences fournies a 1, teintees par le trait (ecart n°10).
		const AnastasisGather::FTrait& Trait = AnastasisGather::TraitAt(Npc.TraitIndex);
		Npc.SkillGather = AnastasisGather::TintedGatherSkill(Trait);
		Npc.SkillTrade = AnastasisGather::TintedTradeSkill(Trait);
		Npc.SkillCraft = AnastasisBuild::TintedCraftSkill(Trait);
		const FString Id = Npc.Id;
		Actors.Add(MoveTemp(Npc));
		// `spawnNpc` : `perceive(this, npc, true)`.
		if (World)
		{
			PerceiveNow(Id);
		}
		return Id;
	}

	bool FVillage::RemoveNpc(const FString& Id)
	{
		if (!Actors.FindById(Id))
		{
			return false;
		}
		// Sa reservation rend la portion au stock (`expireMealReservations`, branche
		// « habitant disparu » de la reference, appliquee tout de suite).
		if (FNpc* Leaving = Actors.FindById(Id))
		{
			ReleaseMeal(*Leaving, TEXT("npc_removed"));
		}
		// Un proprietaire qui disparait libere sa maison : la reference le fait a la
		// mort (mortality.js) ; sans cela la maison resterait close pour toujours.
		for (FBuilding& B : Buildings.GetItemsMutable())
		{
			if (B.Owner == Id)
			{
				B.Owner.Reset();
				// `releaseHome(npc, day)` -> `stampHouseVacant(npc.home, day)` (mortality.js).
				StampHouseVacant(B, Day());
			}
		}
		// `playerActor()` ne rend plus personne : on ne reste pas maitre d'un absent.
		if (Id == PlayerPersonId)
		{
			PlayerPersonId.Reset();
			ResetPlayerHand();
		}
		return Actors.RemoveById(Id);
	}

	FString FVillage::CauseOfDeath(const FNpc& Npc)
	{
		// ecart n°28 : seule la branche `(npc.health ?? 100) <= 0` de causeOfDeath. Le libelle « de faim »
		// ignore `starvingDays > 0` (compteur non porte) : la faim haute suffit.
		if (Npc.Needs.Health > 0.0)
		{
			return FString();
		}
		if (Npc.Needs.Thirst >= AnastasisNeeds::Constants::ParchedAt) return TEXT("de soif");
		if (Npc.Needs.Hunger >= MortalityFamineHunger) return TEXT("de faim");
		if (Npc.Needs.Energy <= AnastasisNeeds::Constants::ExhaustedAt + 8.0) return TEXT("d'epuisement");
		return TEXT("de faiblesse");
	}

	int32 FVillage::UpdateMortalityDaily()
	{
		// Parcours a l'envers comme la reference : on retire en cours de route.
		TArray<FString> Ids;
		for (const FNpc& Npc : Actors.GetItems())
		{
			Ids.Add(Npc.Id);
		}
		const int32 Today = Day();
		int32 Died = 0;
		for (int32 I = Ids.Num() - 1; I >= 0; --I)
		{
			const FNpc* Npc = Actors.FindById(Ids[I]);
			if (!Npc)
			{
				continue;
			}
			const FString Cause = CauseOfDeath(*Npc);
			if (Cause.IsEmpty())
			{
				continue;
			}
			const FString Id = Ids[I];
			RemoveNpc(Id);
			// `forgetTheDead` : plus de relation avec un disparu. La lignee et les memoires restent.
			for (FNpc& Other : Actors.GetItemsMutable())
			{
				Other.Relations.RemoveAll([&Id](const TPair<FString, double>& Row) { return Row.Key == Id; });
			}
			FDeath Death;
			Death.NpcId = Id;
			Death.Cause = Cause;
			Death.Day = Today;
			DeathLog.Add(MoveTemp(Death));
			++Died;
		}
		return Died;
	}

	bool FVillage::AssignWorkplace(const FString& NpcId, const FString& JobId, const FString& BuildingId)
	{
		FNpc* Npc = Actors.FindById(NpcId);
		const FBuilding* B = Buildings.FindById(BuildingId);
		if (!Npc || !B || !B->IsCompleted() || B->Type != GranaryType || JobId != AnastasisGather::JobFarmer)
		{
			return false;
		}
		Npc->JobId = JobId;
		Npc->WorkplaceId = BuildingId;
		return true;
	}

	bool FVillage::IsGranaryWorker(const FNpc& Npc) const
	{
		if (Npc.JobId != AnastasisGather::JobFarmer || Npc.WorkplaceId.IsEmpty()) return false;
		const FBuilding* B = Buildings.FindById(Npc.WorkplaceId);
		return B && B->IsCompleted() && B->Type == GranaryType;
	}

	int32 FVillage::MarketFood() const
	{
		// `rebuildMarketAggregate` : somme des `stock.food.physical` de tous les batiments.
		int32 Total = 0;
		for (const FBuilding& B : Buildings.GetItems())
		{
			Total += B.FoodPhysical;
		}
		return Total;
	}

	bool FVillage::IsPortedGoalFor(const FNpc& Npc, const FString& Goal) const
	{
		if (IsPortedGoal(Goal)) return true;
		if (Goal == TEXT("gatherWood")) return IsWoodHarvester(Npc); // ecart n°30
		if (Goal == AnastasisBuild::GoalBuild) return true;
		// `helpFarm` (help-farm-001) : soigner une parcelle, pour tout adulte.
		if (Goal == GoalHelpFarm) return true;
		return (Goal == GoalGatherFood || Goal == GoalDeliver) && IsGranaryWorker(Npc);
	}

	void FVillage::UpdateActors(double Time, double Dt)
	{
		if (!World)
		{
			return;
		}
		Now = Time;
		// La meteo ne change pas dans un tick : lue une fois, pour tous les habitants.
		TickWeather = CurrentWeather();
		TickDailyRain = DailyRain();
		TArray<FNpc>& Items = Actors.GetItemsMutable();
		// `rebuildActorSpatialIndex` : une fois par tick, avant la boucle (positions du debut de tick).
		TArray<FVector2D> Points;
		Points.Reserve(Items.Num());
		for (const FNpc& Npc : Items) Points.Add(FVector2D(Npc.X, Npc.Y));
		Grid.Rebuild(Points);
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			UpdateNpc(Items[Index], Dt);
		}
	}

	bool FVillage::IsNight() const
	{
		return AnastasisRhythm::IsNightPhase(Now);
	}

	AnastasisWeatherBehavior::FSimWeather FVillage::CurrentWeather() const
	{
		// `readSimWeather` : `sim.forceWeather` d'abord (ses champs, le reste a 0 / ete), puis
		// `weatherAt(sim.seed >>> 0, sim.day || 1, null, dayFrac)`.
		if (bForcedWeather) return ForcedWeather;
		// Sans graine de simulation (pas d'hote) : ciel d'ete sec, tous les termes meteo a 0.
		if (!bWeatherSeeded) return AnastasisWeatherBehavior::FSimWeather();
		return AnastasisWeatherBehavior::ReadSimWeather(WeatherSeed, Day(), Now);
	}

	double FVillage::DailyRain() const
	{
		// `movementSpeedFactor` : `weatherAt(sim.seed, 1 + sim.time / DAY_LENGTH)`, SANS heure —
		// la base journaliere, pas le front de l'heure (le forcage, lui, vaut pour les deux).
		if (bForcedWeather) return ForcedWeather.Rain;
		if (!bWeatherSeeded) return 0.0;
		return AnastasisWeather::WeatherAt(WeatherSeed, 1.0 + Now / AnastasisRhythm::DayLength).Rain;
	}

	AnastasisRhythm::FPhaseSubject FVillage::PhaseSubjectOf(const FNpc& Npc) const
	{
		AnastasisRhythm::FPhaseSubject S;
		S.bHasHomeOrShelter = !Npc.LivingHomeId().IsEmpty();
		S.Energy = Npc.Needs.Energy;
		S.Hunger = Npc.Needs.Hunger;
		return S;
	}

	void FVillage::UpdateNpc(FNpc& Npc, double Dt)
	{
		// `consumeNpcSimulationCadence(sim, npc, dt, { critical: needsCritical(npc) })`, premiere
		// instruction d'`updateNpc` : hors de la bande near, l'habitant accumule le temps et ne
		// tourne qu'a l'intervalle de sa bande, avec le temps accumule. Sans vue : ecart n°5.
		if (bSimulationView)
		{
			AnastasisBudget::FNpcView View;
			View.X = Npc.X;
			View.Y = Npc.Y;
			View.bInside = Npc.Inside.bActive;
			const AnastasisBudget::FCadenceStep Step =
				AnastasisBudget::ConsumeCadence(BudgetDirector, View, Dt, Npc.SimBudgetAccum, NeedsCritical(Npc.Needs));
			// La reference n'ecrit `npc._simBudgetAccum` que hors de near (et hors critique).
			if (Step.Band != AnastasisBudget::EBand::Near)
			{
				Npc.bHasSimBudgetAccum = true;
			}
			if (!Step.bRun)
			{
				return;
			}
			Dt = Step.Dt;
		}

		// `lifestyleDailyUpdate(sim, npc)`, juste apres la cadence et avant `tickNeeds` : une fois par
		// jour au plus, le score de regularite suit le but du moment. L'`ensureLifestyle` de tete
		// d'`updateNpc` ne tire que pour un habitant SANS mode de vie ; ceux du C++ n'en ont pas et
		// n'en recoivent pas (ecart n°8) : ils ne passent pas ici.
		if (Npc.Lifestyle.IsSet())
		{
			AnastasisLifestyle::FLifestyleSubject Subject;
			Subject.Goal = Npc.Goal;
			Subject.JobId = Npc.JobId;
			Subject.HomeId = Npc.HomeId;
			Subject.Skill = Npc.Skill;
			Subject.Energy = Npc.Needs.Energy;
			Subject.bHasTarget = Npc.bHasTarget;
			AnastasisLifestyle::LifestyleDailyUpdate(Npc.Lifestyle, Subject, &VillageRng,
				static_cast<double>(Day()), AnastasisRhythm::DayFracOf(Now));
		}

		// Les cinq facteurs de l'habitant, lus en tete de `tickNeeds`, AVANT la branche.
		// Sans phenotype ni conditionnement (habitant cree par le C++), facteurs 1 : ecart n°8.
		const AnastasisNeeds::FNeedFactors Factors = AnastasisNeeds::NeedFactorsFor(
			Npc.Phenotype.IsSet() ? &Npc.Phenotype.GetValue() : nullptr,
			Npc.Conditioning.IsSet() ? &Npc.Conditioning.GetValue() : nullptr);
		if (Npc.Inside.bActive && Npc.Inside.Goal == GoalRest)
		{
			AnastasisNeeds::TickNeedsRestInside(Npc.Needs, Dt, IsNight(), SleepQualityOf(Npc), Factors);
		}
		else if (Npc.Inside.bActive && Npc.Inside.Goal == GoalEat)
		{
			AnastasisNeeds::TickNeedsEatInside(Npc.Needs, Dt, Factors);
		}
		else if ((Npc.Inside.bActive && Npc.Inside.Goal == GoalSocialize) || Npc.Goal == GoalSocialize)
		{
			// `insideGoal === "socialize" || npc.goal === "socialize"` : dehors, gain x 0,45.
			AnastasisNeeds::TickNeedsSocialize(Npc.Needs, Dt, Npc.Inside.bActive, Factors);
		}
		else if ((Npc.Inside.bActive && Npc.Inside.Goal == GoalRelax) || Npc.Goal == GoalRelax)
		{
			AnastasisNeeds::TickNeedsRelax(Npc.Needs, Dt, Factors);
		}
		else
		{
			const bool bDrinking = Npc.Goal == GoalDrink && !Npc.Inside.bActive && AtDrinkSpot(Npc.X, Npc.Y);
			// `WORK_GOALS.has(npc.goal) && !npc.inside` pour le fermier (recolter, livrer) ;
			// l'extension food-supply garde sa regle (collecte seule).
			const bool bWorking = IsGranaryWorker(Npc)
				? (Npc.Goal == GoalGatherFood || Npc.Goal == GoalDeliver) && !Npc.Inside.bActive
				: Npc.Goal == TEXT("gatherFood") || (Npc.Goal == TEXT("gatherWood") && IsWoodHarvester(Npc) && !Npc.Inside.bActive);
			AnastasisNeeds::TickNeeds(Npc.Needs, Dt, bDrinking, bWorking, Factors);
		}
		// La fin de `tickNeeds`, dans son ordre : `tickMoodlets` (l'humeur d'une amitie neuve monte
		// le moral, plafonnee), puis `tickConditioning`, dont la porte de surmenage lit les metres
		// APRES la branche et les moodlets. Avant needs-wiring-001, les moodlets suivaient la pluie :
		// ils n'ecrivent que le moral, la pluie l'energie et la sante, l'echange ne change aucun bit.
		AnastasisBonds::TickMoodlets(Npc.Moodlets, Npc.Needs.Morale, Dt, Now);
		if (Npc.Conditioning.IsSet())
		{
			// `working: WORK_GOALS.has(npc.goal)` (dedans compris), `resting: npc.goal === "rest"`.
			AnastasisNeeds::TickNeedsConditioning(Npc.Conditioning.GetValue(), Npc.Needs, Dt,
				AnastasisRhythm::IsWorkGoal(Npc.Goal), Npc.Goal == GoalRest);
		}
		// `applyRainExposure`, juste apres `tickNeeds` : dehors sous l'orage, l'energie fond.
		AnastasisWeatherBehavior::ApplyRainExposure(TickWeather.Rain, Npc.Inside.bActive, Npc.Goal, Dt,
			Npc.Needs.Energy, Npc.Needs.Health);

		// `holdTalkAct` : en conversation, l'habitant est fige (ni pensee, ni marche).
		if (HoldTalk(Npc))
		{
			// `holdTalkAct` : la conversation compte au lieu, dedans comme dehors.
			NotePlaceUse(Npc, Npc.Activity, Dt * 0.35);
			return;
		}
		if (Npc.bTalkAnchor)
		{
			// Fin de session : l'ancre du regard n'est pas une destination (voir ecart n°16).
			Npc.bTalkAnchor = false;
			Npc.bHasTarget = false;
			ClearNavigation(Npc);
		}

		// `playerControlled` (player-minimal-001, ecart n°20) : l'habitant incarne pense sans Nous et marche a la main.
		if (IsPlayer(Npc))
		{
			UpdatePlayer(Npc, Dt);
			return;
		}

		// Dedans : ni pensee, ni marche, seulement le temps qui passe.
		if (Npc.Inside.bActive)
		{
			// `updateInside` : sous l'auvent, recuperation lente (`energyDrainPerSec * 0.55 * dt`).
			if (Npc.Goal == GoalShelterRain && Dt > 0.0)
			{
				Npc.Needs.Energy = Clamp(Npc.Needs.Energy
					+ AnastasisWeatherBehavior::Shelter::EnergyDrainPerSec * AnastasisWeatherBehavior::Shelter::InsideRecoverFactor * Dt,
					0.0, 100.0);
			}
			UpdateInside(Npc, Dt);
			return;
		}
		// Extension de portage materiel (ecart n°18), inactive dans le harnais JS.
		// La faim/soif/fatigue critiques gardent la decision ordinaire prioritaire.
		if (Npc.Id == MaterialCourierId && !NeedsCritical(Npc.Needs) && ProgressMaterialCourier(Npc, Dt)) return;

		// `syncVillagePhase` : la bascule de phase force une pensee. La phase est PERSONNELLE
		// (`villagePhaseFor`, reconsider-001) : le mode de vie decale l'horloge ; sans mode de vie
		// (habitant C++, ecart n°8), c'est la phase du village, comme avant.
		const FString PhaseNow = AnastasisRhythm::PhaseId(PersonalPhaseOf(Npc));
		const bool bPhaseFlip = !Npc.VillagePhase.IsEmpty() && Npc.VillagePhase != PhaseNow;
		Npc.VillagePhase = PhaseNow;
		if (bPhaseFlip)
		{
			Npc.PhaseChangedAt = Now;
		}

		const bool bCritical = NeedsCritical(Npc.Needs);
		// Noûs actif : `tickAlgorithmicNpc` a chaque mise a jour d'habitant, comme la reference.
		TickAlgorithmicNpc();
		if (Npc.AiThinkAt < 0.0)
		{
			const double Base = AnastasisNous::DecisionIntervalSeconds(bCritical);
			Npc.AiThinkAt = Now + AnastasisNous::DeterministicAlgoStagger(Npc.Id, Base);
		}
		if (bPhaseFlip)
		{
			Npc.AiThinkAt = Now;
		}
		if (Now >= Npc.AiThinkAt)
		{
			const double ThinkDt = AnastasisNous::DecisionIntervalSeconds(bCritical);
			Npc.AiThinkAt = Now + ThinkDt;
			Perceive(Npc, false);
			ComputeAlgorithmicDecision(Npc);
			// `if (!npc.target || sim.rng() < chance) chooseGoal(sim, npc);` (reconsider-001) :
			// UN tirage du flux partage, seulement avec une cible, AVANT la decision.
			if (!Npc.bHasTarget || VillageRng.Next() < ReconsiderChanceNow(Npc.Id, ThinkDt))
			{
				ChooseGoal(Npc);
			}
		}

		if (Npc.FailedActions >= 3)
		{
			RedirectAfterFailure(Npc);
		}

		Act(Npc, Dt);
	}

	namespace PlannerWiring
	{
		// `worldArchetypes.js` : `scarceSeed` par archetype (jours de graine rare). Seuls bois et pierre
		// sont lus par le planificateur (`exploitSpinePending`, `stoneFirst`).
		void ScarceSeedOf(AnastasisWorldArchetype::EId Id, double& OutWood, double& OutStone)
		{
			OutWood = 0.0;
			OutStone = 0.0;
			const FString Name = AnastasisWorldArchetype::IdName(Id);
			if (Name == TEXT("delta")) { OutWood = 2.0; OutStone = 3.0; }
			else if (Name == TEXT("plateau")) { OutWood = 2.0; }
			else if (Name == TEXT("highland")) { OutWood = 3.0; }
			else if (Name == TEXT("deepwood")) { OutStone = 3.0; }
		}

		const TCHAR* ResourceName(AnastasisWorld::EResource Resource)
		{
			switch (Resource)
			{
			case AnastasisWorld::EResource::Stone: return TEXT("stone");
			case AnastasisWorld::EResource::Wood: return TEXT("wood");
			case AnastasisWorld::EResource::Food: return TEXT("food");
			default: return TEXT("");
			}
		}

		void SetSlot(TArray<TPair<FString, AnastasisPlanner::FStockSlot>>& Stock, const TCHAR* Resource, int32 Physical, int32 Reserved, bool bAddIfMissing)
		{
			for (TPair<FString, AnastasisPlanner::FStockSlot>& Slot : Stock)
			{
				if (Slot.Key == Resource)
				{
					Slot.Value.Physical = Physical;
					Slot.Value.Reserved = Reserved;
					return;
				}
			}
			if (!bAddIfMissing) return;
			AnastasisPlanner::FStockSlot New;
			New.Physical = Physical;
			New.Reserved = Reserved;
			Stock.Emplace(Resource, New);
		}

		const AnastasisPlanner::FStockSlot* FindSlot(const TArray<TPair<FString, AnastasisPlanner::FStockSlot>>& Stock, const TCHAR* Resource)
		{
			for (const TPair<FString, AnastasisPlanner::FStockSlot>& Slot : Stock)
			{
				if (Slot.Key == Resource) return &Slot.Value;
			}
			return nullptr;
		}
	}

	void FVillage::RestoreColonyForHarness(const AnastasisPlanner::FColonyState& InColony, const AnastasisPlanner::FOrderedMap& InMarketStock,
		const TOptional<double>& InSettlementClearRadius, const TOptional<double>& InTreasury)
	{
		ColonyTreasury = InTreasury;
		bHasColony = true;
		Colony = InColony;
		MarketStock = InMarketStock;
		SettlementClearRadius = InSettlementClearRadius;
		UrgencyBucket.Reset();
		UrgencyCache.Reset();
	}

	AnastasisPlanner::FPlannerVillage FVillage::BuildPlannerView()
	{
		AnastasisPlanner::FPlannerVillage View;
		View.Day = static_cast<double>(Day());
		View.Time = Now;
		View.W = World ? World->W : 0;
		View.H = World ? World->H : 0;
		View.bHasSettlement = true;
		View.SettlementX = Settlement.X;
		View.SettlementY = Settlement.Y;
		View.SettlementClearRadius = SettlementClearRadius;
		View.MarketDx = MarketDx;
		View.MarketDy = MarketDy;
		// `sim._marketPos` : pose a l'achevement d'un marche ; ce village n'en batit pas.
		for (const FBuilding& B : Buildings.GetItems())
		{
			AnastasisPlanner::FPlannerBuilding& Out = View.Buildings.AddDefaulted_GetRef();
			Out.Id = B.Id;
			Out.Type = B.Type;
			Out.Progress = B.Progress;
			Out.X = B.X;
			Out.Y = B.Y;
			Out.Owner = B.Owner;
			if (B.VacantSinceDay >= 0) Out.VacantSinceDay = static_cast<double>(B.VacantSinceDay);
			// `createdDay || day` : un 0 se lit comme une absence, des deux cotes.
			Out.CreatedDay = static_cast<double>(B.CreatedDay);
			if (B.Type == HouseType) Out.HousePhase = static_cast<double>(B.HousePhase);
			if (B.bHasMaterials)
			{
				Out.bHasMaterialsNeeded = true;
				if (B.Materials.NeedWood > 0) Out.MaterialsNeeded.Set(TEXT("wood"), B.Materials.NeedWood);
				if (B.Materials.NeedStone > 0) Out.MaterialsNeeded.Set(TEXT("stone"), B.Materials.NeedStone);
				if (B.Materials.ConsumedWood > 0) Out.MaterialsConsumed.Set(TEXT("wood"), B.Materials.ConsumedWood);
				if (B.Materials.ConsumedStone > 0) Out.MaterialsConsumed.Set(TEXT("stone"), B.Materials.ConsumedStone);
			}
			Out.PiecesPlaced = B.PiecesPlaced;
			// Le stock : les cases de la reference, avec la nourriture et le stock de chantier tenus par le C++.
			Out.bHasStock = B.bHasPlannerStock;
			Out.Stock = B.PlannerStock;
			if (B.Type == GranaryType || PlannerWiring::FindSlot(Out.Stock, TEXT("food")))
			{
				PlannerWiring::SetSlot(Out.Stock, TEXT("food"), B.FoodPhysical, B.FoodReserved, Out.bHasStock);
			}
			if (B.bHasMaterials && Out.bHasStock)
			{
				PlannerWiring::SetSlot(Out.Stock, TEXT("wood"), B.Materials.StockWood, 0, false);
				PlannerWiring::SetSlot(Out.Stock, TEXT("stone"), B.Materials.StockStone, 0, false);
			}
		}
		for (const FNpc& N : Actors.GetItems())
		{
			AnastasisPlanner::FPlannerActor& Out = View.Actors.AddDefaulted_GetRef();
			Out.Id = N.Id;
			// Adultes seulement, vivants (ecart n°8).
			Out.LifeStage = FString();
			Out.JobId = N.JobId;
			Out.bAlive = true;
			Out.HomeId = N.HomeId;
			Out.ShelterId = N.ShelterId;
			Out.WorkplaceId = N.WorkplaceId;
			Out.TraitGather = AnastasisGather::TraitAt(N.TraitIndex).Gather;
			// Le bois coupe par le bucheron (ecart n°30) reste dans son sac : aucun chemin ne le
			// livre, il n est donc pas mobilisable pour le planificateur.
			Out.InventoryWood = 0;
		}
		View.bHasColony = bHasColony;
		View.Colony = Colony;
		View.MarketStock = MarketStock;
		if (World) PlannerWiring::ScarceSeedOf(World->Archetype.Id, View.ScarceSeedWood, View.ScarceSeedStone);
		View.TileAt = [this](int32 X, int32 Y, AnastasisPlanner::FPlannerTile& Out)
		{
			if (!World || X < 0 || Y < 0 || X >= World->W || Y >= World->H) return false;
			const AnastasisWorld::FTile Tile = LiveTileAt(X, Y);
			const TCHAR* TypeName = AnastasisWorld::TileTypeName(Tile.Type);
			Out.Type = TypeName ? TypeName : TEXT("");
			Out.Resource = PlannerWiring::ResourceName(Tile.Resource);
			Out.Amount = static_cast<double>(Tile.Amount);
			return true;
		};
		// `findBuildSpot` : la frontiere urbaine n'est pas portee (ecart n°27) : non fournie.
		View.Rng = &VillageRng;
		View.UrgencyBucket = UrgencyBucket;
		View.UrgencyCache = UrgencyCache;
		return View;
	}

	void FVillage::WritePlannerView(const AnastasisPlanner::FPlannerVillage& View)
	{
		// L'ordre de la fiche du planificateur : vacance, stock des batiments, rapport (et flux, tenu
		// en place : `View.Rng` est le flux du village), charte, caches.
		TArray<FBuilding>& Items = Buildings.GetItemsMutable();
		for (int32 I = 0; I < Items.Num() && I < View.Buildings.Num(); ++I)
		{
			FBuilding& B = Items[I];
			const AnastasisPlanner::FPlannerBuilding& V = View.Buildings[I];
			B.VacantSinceDay = V.VacantSinceDay.IsSet() ? static_cast<int32>(V.VacantSinceDay.GetValue()) : -1;
			B.bHasPlannerStock = V.bHasStock;
			B.PlannerStock = V.Stock;
			if (const AnastasisPlanner::FStockSlot* Food = PlannerWiring::FindSlot(V.Stock, TEXT("food")))
			{
				B.FoodPhysical = Food->Physical;
				B.FoodReserved = Food->Reserved;
			}
			if (B.bHasMaterials)
			{
				if (const AnastasisPlanner::FStockSlot* Wood = PlannerWiring::FindSlot(V.Stock, TEXT("wood"))) B.Materials.StockWood = Wood->Physical;
				if (const AnastasisPlanner::FStockSlot* Stone = PlannerWiring::FindSlot(V.Stock, TEXT("stone"))) B.Materials.StockStone = Stone->Physical;
			}
		}
		Colony = View.Colony;
		UrgencyBucket = View.UrgencyBucket;
		UrgencyCache = View.UrgencyCache;
	}

	FCollectiveDecision FVillage::CollectiveDecisionOf(const FNpc& Npc)
	{
		if (CollectiveDecisionOverride) return CollectiveDecisionOverride(Npc);
		// Un village cree par le C++ n'a pas de colonie : la reference, sans `sim.colony`, rend des biais
		// et des planchers nuls ; son urgence et sa corvee de bois, elles, ne sont pas consultees ici (ecart n°27).
		if (!bHasColony) return FCollectiveDecision();
		// Le cache des effets n'est vide, dans la reference, que par la passe quotidienne
		// (`updateCollectivePrioritiesDaily`, non portee, masquee dans le harnais) et l'expiration de la charte :
		// il n'est donc pas vide au changement de jour ici (ecart n°27).
		AnastasisPlanner::FPlannerVillage View = BuildPlannerView();
		const AnastasisPlanner::FPlannerDecision D = AnastasisPlanner::DecisionFor(View, Npc.Id);
		FCollectiveDecision Out;
		// `buildScore` (npc.js l. 2733) : ses lectures du planificateur, dans son ordre.
		Out.bHasColony = true;
		{
			const double Need = D.BuildingNeedScore;
			const int32 HotPads = View.Colony.DoctrineHotPads;
			const bool bSpine = !AnastasisPlanner::ExploitSpinePending(View).IsEmpty();
			const bool bAmenity = !AnastasisPlanner::VillageAmenityPending(View).IsEmpty();
			const bool bCraft = !AnastasisPlanner::VillageCraftPending(View).IsEmpty();
			const bool bHerd = !AnastasisPlanner::VillageHerdPending(View).IsEmpty();
			const bool bBootstrap = !AnastasisPlanner::CraftBootstrapPending(View).IsEmpty();
			const bool bAnyPending = bSpine || bAmenity || bCraft || bHerd || bBootstrap;
			if (Need <= 0.0 && HotPads <= 0 && !bAnyPending)
			{
				Out.bBuildIdle = true;
			}
			else
			{
				namespace B = AnastasisBuild;
				const bool bActiveSite = AnastasisPlanner::ActiveConstructionCount(View) > 0;
				const int32 Treasury = ColonyTreasury.IsSet() ? AnastasisJs::ToInt32(ColonyTreasury.GetValue()) : 0;
				// `(npc.inventory?.wood | 0) + (sim.market?.stock?.wood | 0)` : personne ne porte de bois ici.
				const int32 Wood = AnastasisJs::ToInt32(View.MarketStock.Get(TEXT("wood")));
				const bool bFoundationOk = bBootstrap && (Treasury >= B::FoundationWageGold || Wood >= B::FoundationInKindWood);
				// `isEssentialBuildType(sim, sim.chooseBuildingType())` : `pickCollectiveBuilding` n'est pas
				// porte ; lu faux. Ne pese qu'entre 1 et 14 pieces d'or, sans chantier ni fondation (ecart n°27).
				const bool bEssentialOk = false;
				const bool bCanPay = Treasury >= B::BuildWage || bFoundationOk || bEssentialOk || Treasury <= 0;
				const double Liquidity = bActiveSite ? 1.0 : bCanPay ? 1.0 : Treasury > 0 ? 0.35 : 0.15;
				const double NeedFloor = FMath::Max(Need, FMath::Max(HotPads > 0 ? 28.0 : 0.0, bAnyPending ? 48.0 : 0.0));
				Out.BuildNeedTerm = NeedFloor * Liquidity;
			}
		}
		WritePlannerView(View);
		Out.GoalBias = D.GoalBias;
		Out.GoalFloor = D.GoalFloor;
		Out.UrgencyBias = D.UrgencyBias;
		Out.bWoodBootstrapDraftee = D.bWoodBootstrapDraftee;
		Out.bFoodRush = D.bFoodRush;
		Out.FarmStaffingGap = D.FarmStaffingGap;
		Out.BuildingNeedScore = D.BuildingNeedScore;
		return Out;
	}

	void FVillage::ApplyCollectivePass(const FNpc& Npc, const FCollectiveDecision& Collective,
		TArray<TPair<FString, double>>& Rows, FDecisionTrace& Trace) const
	{
		const bool bCritical = NeedsCritical(Npc.Needs);
		for (TPair<FString, double>& Row : Rows)
		{
			const double Before = Row.Value;
			// `addScore(score, "score.collective_urgency", ..., collectiveUrgency[score.goal] || 0)`.
			Row.Value += Collective.UrgencyOf(Row.Key);
			// La pression morale sur les buts sociaux (`(pressure.socialMul - 1) * 18`) vient ici dans la
			// reference, entre l'urgence et le plancher. Le C++ l'ajoute plus tot, dans `SocialRowScore` :
			// meme valeur, autre ordre d'addition, donc un arrondi qui peut differer d'un ULP quand
			// l'urgence de `socialize` / `relax` n'est pas nulle.
			// Plancher collectif : un PLANCHER, pas un appoint ; un besoin vital reel le recoit en appoint.
			double Floor = Collective.FloorOf(Row.Key);
			if (Row.Key == TEXT("gatherWood") && Collective.bWoodBootstrapDraftee)
			{
				// `COLLECTIVE_PRIORITIES.bootstrapWoodFloor ?? 165`.
				Floor = FMath::Max(Floor, 165.0);
			}
			if (Floor > 0.0)
			{
				Row.Value = bCritical ? Row.Value + Floor : FMath::Max(Row.Value, Floor);
			}
			if (Row.Value != Before) Trace.CollectiveDelta.Add(Row.Key, Row.Value - Before);
		}
		// « Rush famine : sac food -> depot avant autre but de travail. »
		if (Collective.bFoodRush && Npc.InventoryFood > 0)
		{
			for (TPair<FString, double>& Row : Rows)
			{
				const double Before = Row.Value;
				if (Row.Key == GoalDeliver) Row.Value += 28.0;
				else if (Row.Key == TEXT("sell")) Row.Value += 16.0;
				else if (Row.Key == GoalGatherFood) Row.Value += 6.0;
				else if (Row.Key == TEXT("gatherWood") || Row.Key == TEXT("gatherStone") || Row.Key == AnastasisBuild::GoalBuild
					|| Row.Key == TEXT("craft") || Row.Key == TEXT("explore"))
				{
					Row.Value -= 10.0;
				}
				if (Row.Value != Before) Trace.CollectiveDelta.FindOrAdd(Row.Key) += Row.Value - Before;
			}
		}
	}

	void FVillage::ChooseGoal(FNpc& Npc)
	{
		// `phaseBias` / `phaseWorkFactor` lisent `villagePhaseFor(sim, npc)` : la phase PERSONNELLE
		// (reconsider-001). Sans mode de vie, la phase du village.
		const AnastasisRhythm::EPhase Phase = PersonalPhaseOf(Npc);
		const AnastasisRhythm::FPhaseSubject Subject = PhaseSubjectOf(Npc);
		const int32 Wells = CountBuildings(WellType);

		FDecisionTrace Trace;
		Trace.Time = Now;
		Trace.Phase = AnastasisRhythm::PhaseId(Phase);
		Trace.NeedScores = AnastasisNeeds::NeedGoalScores(Npc.Needs, Wells, /*CompletedTaverns=*/0);

		// Fermier au grenier (ecart n°10) : ses lignes de travail sont calculees.
		const bool bWorker = IsGranaryWorker(Npc);
		// Un chantier ouvert : la ligne `build` de chacun lit le meme facteur de travail.
		const bool bSite = ActiveSites().Num() > 0;
		// Le facteur de travail sert a toute ligne de travail calculee : celles du fermier, `build` d'un
		// chantier ouvert, et `helpFarm` de chacun (help-farm-001).
		FWorkRowContext Work;
		{
			// s = believedStock : sans souvenir de marche, la presomption bruitee.
			Work.Believed = AnastasisGather::BelievedFoodPresumed(Npc.Id);
			Work.bMealBlocked = AnastasisGather::MealPathBlocked(Npc.Needs.Hunger, Npc.InventoryFood, Work.Believed);
			// `pressure.effectiveWork * phaseWorkFactor * elderWorkFactor * natureWorkFactor` (lifestyle-decision-001) :
			// la phase personnelle avec metier et mode de vie, adulte (1), la nature lue (moyenne sans elle).
			AnastasisNature::FNature Average;
			const double NatureWorkFactor = AnastasisNature::NatureWorkFactor(Npc.Nature.IsSet() ? Npc.Nature.GetValue() : Average);
			const FString LifestyleId = Npc.Lifestyle.IsSet() ? Npc.Lifestyle->Id : FString();
			Work.WorkFactor = AnastasisGather::MoralEffectiveWork(Npc.Needs, MarketFood(), Day())
				* AnastasisRhythm::PhaseWorkFactor(Phase, Npc.JobId, LifestyleId) * 1.0 * NatureWorkFactor;
			if (bWorker || bSite) Trace.WorkFactor = Work.WorkFactor;
		}
		// Ce que le planificateur collectif dit pour cet habitant : lu une fois, pour les lignes et la passe finale.
		const FCollectiveDecision Collective = CollectiveDecisionOf(Npc);
		// --- resource-targets-001 : `survivalForecastBias` puis `spatialRiskBiasMap` (adultScores, npc.js
		// l. 1103-1104), apres la lecture du planificateur, AVANT `failureTargetBiasMap`. Leurs replis et leurs cibles ecrivent (seuils filtres,
		// destination posee), dans l'ordre de la reference ; `recallOrSearch` tire s'il ne connait aucun
		// gisement. Les deux cartes s'ajoutent aux lignes apres la meteo (plus bas).
		const TArray<TPair<FString, double>> ForecastBias = SurvivalForecastBiasFor(Npc, Work.bMealBlocked);
		const TArray<TPair<FString, double>> SpatialRiskBias = SpatialRiskBiasMapFor(Npc);
		for (const TPair<FString, double>& Entry : ForecastBias) Trace.ForecastBias.Add(Entry.Key, Entry.Value);
		for (const TPair<FString, double>& Entry : SpatialRiskBias) Trace.SpatialRiskBias.Add(Entry.Key, Entry.Value);
		// --- fin resource-targets-001

		// Preparation d'adultScores (perception-explore-001) : `failureTargetBiasMap` evalue
		// `failureCauseForGoal(sim, npc, "explore")` = `intentExploreHint(sim, npc) ||
		// exploreTarget(sim, npc)`. Sans intention du jour portee (ecart n°24), c'est
		// `exploreTarget` : 2 a 8 tirages dans le releve, AVANT les bruits de la table. La
		// cible ne sert qu'au biais d'echec de la ligne `explore`, non portee (ecart n°1) : seuls ses
		// tirages comptent ici. `spatialRiskBiasMap` est calcule plus haut (resource-targets-001).
		Trace.ExploreDraws = ExploreTargetFor(Npc).Draws;

		// adultScores : les 25 lignes, dans l'ordre de la reference, puis
		// `rhythmBias` (= phaseBias). Les buts non portes valent 42 avant rythme.
		// Chaque ligne tire son `goalNoise` a sa place dans la table (`Ai/AnastasisGoalNoise.h`),
		// et le bruit entre dans la somme la ou la reference l'ecrit.
		const TConstArrayView<AnastasisGoalNoise::FTableNoise> Noises = AnastasisGoalNoise::AdultTableNoises();
		auto DrawNoise = [this, &Npc, &Noises, &Trace](const FString& G) -> double
		{
			for (const AnastasisGoalNoise::FTableNoise& N : Noises)
			{
				if (G != N.Goal) continue;
				if (!NoiseConditionHolds(Npc, N.Condition)) return 0.0;
				Trace.NoiseDraws += 1;
				const double Value = AnastasisGoalNoise::GoalNoise(VillageRng, N.Amp);
				Trace.RowNoise.Add(G, Value);
				return Value;
			}
			return 0.0;
		};
		TArray<TPair<FString, double>> Rows;
		for (const TCHAR* Goal : AdultTable)
		{
			const FString G = Goal;
			const double Noise = DrawNoise(G);
			// Un but non porte vaut le plancher (ecart n°1) : son bruit est TIRE (la reference le
			// tire, et l'ordre du flux en depend) mais pas ajoute, le plancher n'etant pas un score
			// de la reference. Ajoute a 42, il ferait gagner `observer` au hasard.
			double Score = UnportedGoalsFloor;
			if (G == GoalEat) Score = Trace.NeedScores.Eat + EatJobPriorityBias;
			else if (G == GoalRest) Score = Trace.NeedScores.Rest + RestJobPriorityBias;
			// `needs.drink + (sim.countBuildings?.("well") > 0 ? 6 : 0) + goalNoise(sim, 6)`.
			else if (G == GoalDrink) Score = Trace.NeedScores.Drink + (Wells > 0 ? 6.0 : 0.0) + Noise;
			// `{ goal: "shelterRain", score: shelterRainScore(sim, npc) + goalNoise(sim, 6) }`.
			else if (G == GoalShelterRain) Score = AnastasisWeatherBehavior::ShelterRainScore(
				TickWeather.Rain, Npc.Inside.bActive, Npc.Goal, Npc.JobId, Now, Npc.ShelterCooldownUntil) + Noise;
			else if (G == GoalSocialize || G == GoalRelax)
			{
				const double Need = G == GoalSocialize ? Trace.NeedScores.Socialize : Trace.NeedScores.Relax;
				Rows.Add(TPair<FString, double>(G, SocialRowScore(Npc, G, Need, RhythmStatusLifestyle(Npc, Phase, Subject, G), Noise)));
				continue;
			}
			else if (G == TEXT("gatherWood") && IsWoodHarvester(Npc))
			{
				Rows.Add(TPair<FString, double>(G, WoodRowScore(Npc, AnastasisRhythm::PhaseBias(Phase, Subject, G), Work, Noise)));
				continue;
			}
			else if ((bSite || Collective.bHasColony) && G == AnastasisBuild::GoalBuild)
			{
				Rows.Add(TPair<FString, double>(G, BuildRowScore(Npc, RhythmStatusLifestyle(Npc, Phase, Subject, G), Work, Noise, Collective)));
				continue;
			}
			else if (G == GoalHelpFarm)
			{
				Rows.Add(TPair<FString, double>(G, HelpFarmRowScore(Npc, RhythmStatusLifestyle(Npc, Phase, Subject, G), Work, Noise, Collective)));
				continue;
			}
			else if (bWorker && (G == GoalGatherFood || G == GoalDeliver))
			{
				const double Row = WorkRowScore(Npc, G, RhythmStatusLifestyle(Npc, Phase, Subject, G), Work, Noise);
				(G == GoalGatherFood ? Trace.GatherRowTable : Trace.DeliverRowTable) = Row;
				Rows.Add(TPair<FString, double>(G, Row));
				continue;
			}
			// Bounded food-supply extension: known finite source + known depot.
			// Existing Noûs urgency and phase biases still arbitrate needs.
			// (Le bruit de la ligne est tire quand meme : la reference le tire.)
			if (FoodSources.Num() > 0 && G == TEXT("gatherFood"))
				Score = Npc.InventoryFood == 0 && HasKnownFoodSource(Npc) && KnownFoodDepot(Npc) ? 85.0 : -1000.0;
			if (FoodSources.Num() > 0 && G == TEXT("deliver"))
				Score = Npc.InventoryFood > 0 && KnownFoodDepot(Npc) ? 100.0 + 5.0 * Npc.InventoryFood : -1000.0;
			Score += RhythmStatusLifestyle(Npc, Phase, Subject, G);
			Rows.Add(TPair<FString, double>(G, Score));
		}

		// `addScore(score, "score.weather", ..., weatherGoalBias(sim, npc, score.goal))` : sur CHAQUE
		// ligne, dans la chaine des biais de chooseGoal, donc avant commitGoalChoice et Noûs.
		Trace.WeatherRain = TickWeather.Rain;
		for (TPair<FString, double>& Row : Rows)
		{
			Row.Value += AnastasisWeatherBehavior::WeatherGoalBias(TickWeather, Npc.JobId, Row.Key);
			// `score.nature` (lifestyle-decision-001), juste apres la meteo et le trait dans la chaine.
			Row.Value += NatureGoalBiasOf(Npc, Row.Key);
		}
		// --- resource-targets-001 : `score.survival_forecast` puis `score.spatial_risk`, `map[score.goal] || 0`,
		// a leur place dans la chaine (apres la meteo et les biais non portes, avant l'urgence collective).
		// Comme la meteo et la passe collective, sur chaque ligne, plancher compris.
		for (TPair<FString, double>& Row : Rows)
		{
			Trace.RowsBeforeRisk.Add(Row.Key, Row.Value);
			Row.Value += AnastasisSpatialRisk::BiasOf(ForecastBias, Row.Key);
			Row.Value += AnastasisSpatialRisk::BiasOf(SpatialRiskBias, Row.Key);
			Trace.RowsAfterRisk.Add(Row.Key, Row.Value);
		}
		// --- fin resource-targets-001
		// Ce que la table ecrit en passant (premiere-pensee-001) : `failureTargetBiasMap` cree
		// `mind.failures` (`ensureFailures`, puis sa case `negative`), et `phaseBias` pose `nocturnalIntent`
		// a chaque ligne : la nuit, hors garde, `isNocturnalWanderer` ; de jour, faux ; garde de nuit, inchange.
		Npc.bHasFailureStore = true;
		if (Phase != AnastasisRhythm::EPhase::Night) Npc.NocturnalIntent = false;
		else if (!Subject.bGuard) Npc.NocturnalIntent = Subject.bNocturnalWanderer;
		// La fin de la chaine d'`adultScores` : urgence collective, plancher collectif, rush famine.
		ApplyCollectivePass(Npc, Collective, Rows, Trace);

		// commitGoalChoice : `applyGoalStickiness` (reconsider-001) — le but en cours garde sa ligne, pour
		// tout habitant, joueur compris. Nature sans qualite ni defaut (ecart n°10) : `natureStickBonus` = 0.
		{
			const AnastasisGather::FTrait& T = AnastasisGather::TraitAt(Npc.TraitIndex);
			AnastasisReconsider::FStickSubject Stick;
			Stick.CurrentGoal = Npc.Goal;
			Stick.bCritical = NeedsCritical(Npc.Needs);
			Stick.bCurrentRelievesCritical = AnastasisReconsider::IsCriticalReliefGoal(Npc.Needs, Npc.InventoryFood, Npc.Goal);
			Stick.PhaseChangedAt = Npc.PhaseChangedAt;
			Stick.TraitBuild = T.Build;
			Stick.TraitTrade = T.Trade;
			Stick.TraitGather = T.Gather;
			Stick.TraitExplore = T.Explore;
			Stick.bWorkSession = Npc.WorkSession.bActive;
			Stick.InventoryLoad = Npc.InventoryFood + Npc.InventoryWood;

			if (Npc.Nature.IsSet()) Stick.NatureStick = AnastasisNature::NatureStickBonus(Npc.Nature.GetValue());
			for (TPair<FString, double>& Row : Rows)
			{
				const double Bonus = AnastasisReconsider::GoalStickinessBonus(Stick, Now, Row.Key);
				Row.Value += Bonus;
				if (Bonus != 0.0) Trace.Stickiness = Bonus;
			}
		}
		// Noûs biaise la table avant le tri. Pas pour l'habitant incarne : `algoOn = !playerControlled`
		// (npc.js), Noûs ne pense pas pour lui.
		const bool bPlayer = IsPlayer(Npc);
		if (!bPlayer)
		{
			ApplyAlgorithmicScoreBias(Npc, Rows);
		}
		// `applyGoalEligibility` : `deliver` n'est candidat qu'avec une charge.
		if (Npc.InventoryFood <= 0)
		{
			Rows.RemoveAll([](const TPair<FString, double>& Row) { return Row.Key == GoalDeliver; });
		}

		Trace.FloorScore = -AnastasisNav::Infinity;
		for (const TPair<FString, double>& Row : Rows)
		{
			if (Row.Key == GoalEat) Trace.EatRowScore = Row.Value;
			else if (Row.Key == GoalRest) Trace.RestRowScore = Row.Value;
			else if (Row.Key == GoalDrink) Trace.DrinkRowScore = Row.Value;
			else if (Row.Key == GoalSocialize) Trace.SocializeRowScore = Row.Value;
			else if (Row.Key == GoalRelax) Trace.RelaxRowScore = Row.Value;
			else if (bWorker && Row.Key == GoalGatherFood) Trace.GatherRowScore = Row.Value;
			else if ((bSite || Collective.bHasColony) && Row.Key == AnastasisBuild::GoalBuild) Trace.BuildRowScore = Row.Value;
			else if (bWorker && Row.Key == GoalDeliver) Trace.DeliverRowScore = Row.Value;
			else if (Row.Key == GoalShelterRain) Trace.ShelterRowScore = Row.Value;
			else if (Row.Value > Trace.FloorScore)
			{
				Trace.FloorScore = Row.Value;
				Trace.FloorGoal = Row.Key;
			}
		}

		// `scores.sort((a, b) => b.score - a.score)` : tri STABLE.
		Rows.StableSort([](const TPair<FString, double>& A, const TPair<FString, double>& B) { return A.Value > B.Value; });
		Trace.TableWinner = Rows[0].Key;
		if (Npc.bHasAlgoDecision)
		{
			Trace.NousType = Npc.AlgoDecision.Type;
			Trace.NousScore = Npc.AlgoDecision.Score;
			Trace.NousUrgency = Npc.AlgoDecision.Urgency;
		}
		FString Next = bPlayer ? Rows[0].Key : ApplyAlgorithmicCommitGate(Npc, Rows[0].Key, Npc.Goal, Trace.CommitGate);
		// Ce que la table donnait avant les verrous : s'ils changent `Next`, le joueur est verrouille.
		const FString Gated = Next;
		// LOT C — VERROU DE QUART (commitGoalChoice, reconsider-001) : un but engage sous plancher
		// ne bascule pas vers un but hors famille travail ; seul le palier critique (deja teste
		// dans `shiftShields`) force la sortie. Apres la porte Noûs, avant l'orage.
		if (!AnastasisWorkShift::IsShiftWorkFamily(Next)
			&& AnastasisWorkShift::IsShiftWorkFamily(Npc.Goal)
			&& AnastasisWorkShift::ShiftShields(Npc.WorkShift, Npc.Goal, Now, NeedsCritical(Npc.Needs)))
		{
			Next = Npc.Goal;
			Trace.bShiftLock = true;
		}
		// Finish a physical delivery before resuming gathering; urgent needs retain priority.
		// (Extension food-supply seulement : le fermier du grenier suit la reference.)
		if (!bWorker && FoodSources.Num() > 0 && Npc.InventoryFood > 0 && KnownFoodDepot(Npc) && !NeedsCritical(Npc.Needs))
			Next = TEXT("deliver");
		if (!bWorker && FoodSources.Num() > 0 && Next == TEXT("gatherFood") && (!HasKnownFoodSource(Npc) || Npc.InventoryFood > 0 || !KnownFoodDepot(Npc)))
			Next = GoalObserver;
		// « Orage : quitter le travail outdoor pour un toit (sauf cargo / urgence) » — commitGoalChoice.
		// `criticalReliefGoals` n'est consulte que hors besoin critique : il y est vide, la garde
		// `needsCritical` suffit. `fetchInput` / `haulJob` ne sont pas portes (jamais gagnants ici).
		{
			const FString& Previous = Npc.Goal;
			if (Next != GoalShelterRain && Next != GoalDeliver
				&& !NeedsCritical(Npc.Needs)
				&& AnastasisWeatherBehavior::ShouldSeekRainShelter(TickWeather.Rain, Npc.Inside.bActive, Npc.Goal, Npc.JobId, Now, Npc.ShelterCooldownUntil)
				&& (AnastasisWeatherBehavior::IsRainExposedGoal(Next) || AnastasisWeatherBehavior::IsRainExposedGoal(Previous)))
			{
				const FString Resume = AnastasisWeatherBehavior::IsRainExposedGoal(Previous) ? Previous
					: AnastasisWeatherBehavior::IsRainExposedGoal(Next) ? Next : FString();
				if (!Resume.IsEmpty()) Npc.ShelterResumeGoal = Resume;
				Next = GoalShelterRain;
				Trace.bStormGate = true;
			}
		}
		// `decideGoal(sim, npc, scores, ctx)` (player-goals-001, ecart n°20) : le seam unique, APRES le tri,
		// l'eligibilite et les verrous. Pour tout autre habitant, la fonction identite.
		if (bPlayer)
		{
			PlayerOptions.Reset();
			for (const TPair<FString, double>& Row : Rows)
			{
				if (PlayerOptions.Num() >= PlayerDecision::OptionsKept) break;
				if (IsPlayerTableGoal(Npc, Row.Key)) PlayerOptions.Add({ Row.Key, Row.Value });
			}
			Next = DecideAsPlayer(Npc, Rows, Next, Next != Gated);
			if (Next == GoalIdle)
			{
				CommitPlayerIdle(Npc);
				Trace.Winner = GoalIdle;
				Trace.CommitGate = TEXT("player");
				Npc.LastDecision = MoveTemp(Trace);
				return;
			}
			Trace.CommitGate = TEXT("player");
		}
		CommitGoal(Npc, Next, Trace, &Rows);
		// Le but humain passe la table mais ne trouve rien a viser (pas de puits atteignable, pas de
		// logement) : `AssignTarget` retombe sur `observer`, qui n'est pas un but humain. L'humain attend,
		// et le refus le dit -- impossible ici et maintenant.
		if (bPlayer && Npc.Goal == GoalObserver)
		{
			CedePlayerGoal(PlayerDecision::RefusalNotInTable);
			CommitPlayerIdle(Npc);
			Npc.LastDecision.Winner = GoalIdle;
		}
	}

	void FVillage::CommitGoal(FNpc& Npc, const FString& Next, FDecisionTrace& Trace, const TArray<TPair<FString, double>>* Scores)
	{
		const FString Previous = Npc.Goal;
		// Un but non porte qui gagne ne fait rien : `observer`.
		const FString NewGoal = IsPortedGoalFor(Npc, Next) ? Next : FString(GoalObserver);
		Npc.DestBuildingId.Reset();
		Npc.Goal = NewGoal;
		// `captureGoalExplain` juste apres `npc.goal = nextGoal` (le collant se lit sur le nouveau but),
		// `stampStreetDecision` apres `onAlgorithmicGoalCommitted` (premiere-pensee-001).
		if (Scores) CaptureGoalExplain(Npc, *Scores);
		OnAlgorithmicGoalCommitted(Npc, Previous, NewGoal);
		if (Scores) StampStreetDecision(Npc, *Scores, Previous);
		if (NewGoal != Previous)
		{
			Npc.GoalSince = Now;
			Npc.DoorStuckAt = 0.0;
			// La reference ne remet pas `workTimer` a zero au changement de but : le reste d'une attente compte
			// pour le but suivant. Remis a zero ici (ecart n°36).
			Npc.WorkTimer = 0.0;
		}
		// `if (npc.goal !== "build") npc.buildBinding = null` : la cle existe des le premier commit.
		Npc.bHasBuildBinding = true;
		if (Npc.Goal != AnastasisBuild::GoalBuild) Npc.BuildBinding.Reset();
		AssignTarget(Npc, Trace);
		// `noteShiftGoalCommit(sim, npc)` (reconsider-001) : le point de decision ouvre, continue ou
		// ferme le quart. Apres la cible : `opensExtractionShift` lit `npc.target`.
		{
			const FBuilding* Workplace = Npc.WorkplaceId.IsEmpty() ? nullptr : Buildings.FindById(Npc.WorkplaceId);
			const bool bEntry = AnastasisWorkShift::IsShiftEntryGoal(Npc.Goal)
				|| (Workplace && AnastasisWorkShift::OpensExtractionShift(Npc.Goal, Workplace->Type, Workplace->Progress,
					Workplace->X, Workplace->Y, Npc.bHasTarget, Npc.Target.X, Npc.Target.Y));
			AnastasisWorkShift::NoteShiftGoalCommit(Npc.WorkShift, Npc.Goal, Now, NeedsCritical(Npc.Needs), bEntry);
		}
		Trace.Winner = Npc.Goal;
		Trace.BuildingId = Npc.DestBuildingId;
		Npc.LastDecision = MoveTemp(Trace);
	}

	namespace
	{
		/** `SHORT_GOAL` d'explainGoal.js, puis `goalLabel(goal).split(" ")[0]`. */
		FString ShortGoal(const FString& Goal)
		{
			static const TPair<const TCHAR*, const TCHAR*> Short[] = {
				{ TEXT("eat"), TEXT("manger") }, { TEXT("eatTogether"), TEXT("table") }, { TEXT("rest"), TEXT("dormir") },
				{ TEXT("drink"), TEXT("boire") }, { TEXT("relax"), TEXT("souffler") }, { TEXT("relieve"), TEXT("besoins") },
				{ TEXT("gatherWood"), TEXT("bois") }, { TEXT("gatherStone"), TEXT("pierre") }, { TEXT("gatherFood"), TEXT("nourriture") },
				{ TEXT("sell"), TEXT("vendre") }, { TEXT("buy"), TEXT("acheter") }, { TEXT("build"), TEXT("batir") },
				{ TEXT("craft"), TEXT("fabriquer") }, { TEXT("maintain"), TEXT("entretenir") }, { TEXT("deliver"), TEXT("livrer") },
				{ TEXT("haulJob"), TEXT("transporter") }, { TEXT("explore"), TEXT("explorer") }, { TEXT("socialize"), TEXT("parler") },
				{ TEXT("visitFamily"), TEXT("famille") }, { TEXT("confront"), TEXT("affronter") }, { TEXT("play"), TEXT("jouer") },
				{ TEXT("study"), TEXT("etudier") }, { TEXT("apprentice"), TEXT("apprendre") }, { TEXT("helpFarm"), TEXT("champs") },
			};
			if (Goal.IsEmpty()) return TEXT("?");
			for (const TPair<const TCHAR*, const TCHAR*>& S : Short)
			{
				if (Goal == S.Key) return S.Value;
			}
			// `GOAL_LABELS` (ai/goalLabels.js), premier mot ; sinon l'identifiant.
			static const TPair<const TCHAR*, const TCHAR*> Labels[] = {
				{ TEXT("observer"), TEXT("observer") }, { TEXT("patrol"), TEXT("faire") }, { TEXT("haulCart"), TEXT("conduire") },
				{ TEXT("shelterRain"), TEXT("s'abriter") }, { TEXT("fetchInput"), TEXT("chercher") }, { TEXT("closeWorkplace"), TEXT("fermer") },
			};
			for (const TPair<const TCHAR*, const TCHAR*>& L : Labels)
			{
				if (Goal == L.Key) return L.Value;
			}
			return Goal;
		}

		/** `skillGoalBias` : le domaine du but (`GOAL_DOMAIN`, `domainForGoal`), `(skills[domain] || 1) - 1) * 8`. */
		double SkillBiasFor(const FNpc& Npc, const FString& Goal)
		{
			double V = 0.0;
			if (Goal == TEXT("craft") || Goal == TEXT("maintain") || Goal == TEXT("build") || Goal == TEXT("apprentice")) V = Npc.SkillCraft;
			else if (Goal == TEXT("helpFarm") || Goal.StartsWith(TEXT("gather"), ESearchCase::CaseSensitive)) V = Npc.SkillGather;
			else if (Goal == TEXT("sell") || Goal == TEXT("buy") || Goal == TEXT("deliver") || Goal == TEXT("haulJob")) V = Npc.SkillTrade;
			else if (Goal == TEXT("socialize") || Goal == TEXT("visitFamily") || Goal == TEXT("play") || Goal == TEXT("study")) V = Npc.SkillCare;
			else return 0.0;
			if (V == 0.0 || FMath::IsNaN(V)) V = 1.0;
			return (V - 1.0) * 8.0;
		}

		double RoundTenth(double V) { return AnastasisJs::Round(V * 10.0) / 10.0; }
	}

	double FVillage::StatusBias(const FNpc& Npc, const FString& Goal)
	{
		// `(npc.gold || 0)` : sans or lu, celui d'un habitant tout juste cree (10 a 29) : ni misere ni aisance.
		const double Gold = Npc.Gold.IsSet() ? (FMath::IsNaN(Npc.Gold.GetValue()) ? 0.0 : Npc.Gold.GetValue()) : 10.0;
		const double Morale = Npc.Needs.Morale != 0.0 && !FMath::IsNaN(Npc.Needs.Morale) ? Npc.Needs.Morale : 100.0;
		const bool bMisery = Morale < 18.0 || Npc.Needs.Hunger > 90.0 || (Gold <= 2.0 && Npc.HomeId.IsEmpty());
		const bool bComfortable = !bMisery && (Gold >= 95.0 || Npc.Reputation >= 76.0);
		if (bMisery)
		{
			if (Goal == TEXT("sell") || Goal == TEXT("deliver")) return -12.0;
			if (Goal == GoalEat || Goal == TEXT("eatTogether") || Goal == GoalRest || Goal == GoalDrink) return 14.0;
			if (Goal == GoalSocialize || Goal == GoalRelax) return 8.0;
		}
		if (bComfortable)
		{
			if (Goal == TEXT("sell") && Npc.JobId == TEXT("merchant")) return 5.0;
			if (Goal == TEXT("buy")) return 4.0;
			if (Goal == GoalSocialize || Goal == TEXT("visitFamily") || Goal == TEXT("eatTogether") || Goal == GoalRelax) return 6.0;
			if (Goal.StartsWith(TEXT("gather"), ESearchCase::CaseSensitive)) return -3.0;
		}
		return 0.0;
	}

	double FVillage::NatureGoalBiasOf(const FNpc& Npc, const FString& Goal)
	{
		return Npc.Nature.IsSet() ? AnastasisNature::NatureGoalBias(Npc.Nature.GetValue(), Goal) : 0.0;
	}

	double FVillage::RhythmStatusLifestyle(const FNpc& Npc, AnastasisRhythm::EPhase Phase, const AnastasisRhythm::FPhaseSubject& Subject, const FString& Goal) const
	{
		double Score = AnastasisRhythm::PhaseBias(Phase, Subject, Goal) + StatusBias(Npc, Goal);
		// `lifestyleBias(sim, npc, goal)` a l'heure du VILLAGE (`sim.dayFrac()`). Sans mode de vie, la reference
		// en tirerait un dans `sim.rng` : ceux du C++ restent sans, et le penchant vaut 0 (ecart n°8).
		if (Npc.Lifestyle.IsSet())
		{
			TOptional<AnastasisLifestyle::FLifestyle> Lifestyle = Npc.Lifestyle;
			AnastasisLifestyle::FLifestyleSubject S;
			S.JobId = Npc.JobId;
			S.HomeId = Npc.HomeId;
			S.Skill = Npc.Skill;
			S.Energy = Npc.Needs.Energy;
			S.Goal = Npc.Goal;
			S.bHasTarget = Npc.bHasTarget;
			S.FavoriteBuildingId = Npc.FavoriteBuildingId;
			Score += AnastasisLifestyle::LifestyleBias(Lifestyle, S, nullptr, AnastasisRhythm::DayFracOf(Now), Goal);
		}
		return Score;
	}

	void FVillage::CaptureGoalExplain(FNpc& Npc, const TArray<TPair<FString, double>>& Scores)
	{
		// `EXPLAIN_GOAL.minCause` = 4.
		constexpr double MinCause = 4.0;
		FGoalExplain Explain;
		Explain.At = Now;
		if (Scores.Num() == 0)
		{
			Npc.GoalExplain = Explain;
			return;
		}
		Explain.Goal = Scores[0].Key;
		const AnastasisNeeds::FNeedGoalScores Needs = AnastasisNeeds::NeedGoalScores(Npc.Needs, CountBuildings(WellType), /*CompletedTaverns=*/0);
		const AnastasisGather::FTrait& Trait = AnastasisGather::TraitAt(Npc.TraitIndex);
		const bool bCritical = NeedsCritical(Npc.Needs);
		// `goalStickinessBonus(sim, npc, goal)` lu sur le but deja commis, avant `clearWorkSession`.
		AnastasisReconsider::FStickSubject Stick;
		Stick.CurrentGoal = Npc.Goal;
		Stick.bCritical = bCritical;
		Stick.bCurrentRelievesCritical = AnastasisReconsider::IsCriticalReliefGoal(Npc.Needs, Npc.InventoryFood, Npc.Goal);
		Stick.PhaseChangedAt = Npc.PhaseChangedAt;
		Stick.TraitBuild = Trait.Build;
		Stick.TraitTrade = Trait.Trade;
		Stick.TraitGather = Trait.Gather;
		Stick.TraitExplore = Trait.Explore;
		Stick.bWorkSession = Npc.WorkSession.bActive;
		Stick.InventoryLoad = Npc.InventoryFood;

		if (Npc.Nature.IsSet()) Stick.NatureStick = AnastasisNature::NatureStickBonus(Npc.Nature.GetValue());

		struct FPart { const TCHAR* Key; const TCHAR* Label; double Value; };
		for (int32 I = 0; I < Scores.Num() && I < 3; ++I)
		{
			const FString& Goal = Scores[I].Key;
			// `goalCauseParts` : dans son ordre. Les biais non portes valent 0 et ne passent pas le seuil
			// (micro-plan, foyer, age, ambition, teinte du jour, nature, memoire sociale, echec, souvenir,
			// mandats : ecarts n°1, n°10, n°24, n°27).
			TArray<FPart> Parts;
			auto Push = [&Parts](const TCHAR* Key, const TCHAR* Label, double Value)
			{
				const double V = FMath::IsNaN(Value) ? 0.0 : Value;
				if (V >= MinCause) Parts.Add({ Key, Label, V });
			};
			if (Goal == GoalEat || Goal == TEXT("eatTogether")) Push(TEXT("need_eat"), TEXT("faim"), Needs.Eat);
			if (Goal == TEXT("eatTogether")) Push(TEXT("household"), TEXT("foyer"), 10.0);
			if (Goal == GoalRest) Push(TEXT("need_rest"), TEXT("fatigue"), Needs.Rest);
			if (Goal == GoalDrink) Push(TEXT("need_drink"), TEXT("soif"), Needs.Drink);
			if (Goal == GoalRelax) Push(TEXT("need_relax"), TEXT("ennui"), Needs.Relax);
			if (Goal == TEXT("relieve")) Push(TEXT("need_hygiene"), TEXT("proprete"), Needs.Relieve);
			if (Goal == GoalSocialize || Goal == TEXT("visitFamily") || Goal == TEXT("play")) Push(TEXT("need_social"), TEXT("solitude"), Needs.Socialize);
			const double Weather = AnastasisWeatherBehavior::WeatherGoalBias(TickWeather, Npc.JobId, Goal);
			if (Weather >= MinCause) Push(TEXT("weather"), TEXT("meteo"), Weather);
			else if (Weather <= -MinCause) Push(TEXT("weather"), TEXT("meteo"), FMath::Abs(Weather));
			Push(TEXT("trait"), TEXT("temperament"), AnastasisGather::TraitGoalBias(Trait, Goal));
			const double Nature = NatureGoalBiasOf(Npc, Goal);
			if (Nature >= MinCause || Nature <= -MinCause) Push(TEXT("nature"), TEXT("caractere"), FMath::Abs(Nature));
			const double Skill = SkillBiasFor(Npc, Goal);
			if (Skill >= MinCause) Push(TEXT("skill"), TEXT("competence"), Skill);
			Push(TEXT("stick"), TEXT("habitude"), Goal == Npc.Goal ? AnastasisReconsider::GoalStickinessBonus(Stick, Now, Goal) : 0.0);
			Push(TEXT("job"), TEXT("metier"), AnastasisGather::JobPriority(Npc.JobId, Goal));
			if (bCritical && (Goal == GoalEat || Goal == TEXT("eatTogether") || Goal == GoalRest || Goal == GoalDrink))
			{
				const bool bEat = Goal == GoalEat || Goal == TEXT("eatTogether");
				const double Survival = bEat ? Needs.Eat : Goal == GoalRest ? Needs.Rest : Needs.Drink;
				Push(TEXT("survival"), bEat ? TEXT("faim") : Goal == GoalRest ? TEXT("fatigue") : TEXT("soif"), Survival + 8.0);
			}
			// `parts.sort((a, b) => b.value - a.value)` (stable), puis le premier strictement plus grand.
			Parts.StableSort([](const FPart& A, const FPart& B) { return A.Value > B.Value; });
			FPart Best = Parts.Num() > 0 ? Parts[0] : FPart{ TEXT("work"), TEXT("travail"), 0.0 };
			for (const FPart& Part : Parts)
			{
				if (Part.Value > Best.Value) Best = Part;
			}
			FGoalExplainEntry& Entry = Explain.Top.AddDefaulted_GetRef();
			Entry.Goal = Goal;
			Entry.Score = RoundTenth(Scores[I].Value);
			Entry.Cause = Best.Label;
			Entry.CauseKey = Best.Key;
			Entry.CauseValue = RoundTenth(Best.Value);
		}
		// `formatExplainLine(top)`.
		const FGoalExplainEntry& Head = Explain.Top[0];
		if (Explain.Top.Num() < 2 || Head.Score >= Explain.Top[1].Score + 35.0)
		{
			Explain.Line = FString(TEXT("parce que ")) + Head.Cause;
		}
		else
		{
			TArray<FString> Bits;
			for (const FGoalExplainEntry& E : Explain.Top) Bits.Add(ShortGoal(E.Goal) + TEXT(": ") + E.Cause);
			Explain.Line = FString::Join(Bits, TEXT(" \u00B7 "));
		}
		Npc.GoalExplain = MoveTemp(Explain);
	}

	void FVillage::StampStreetDecision(FNpc& Npc, const TArray<TPair<FString, double>>& Scores, const FString& PreviousGoal)
	{
		if (Scores.Num() == 0) return;
		// `STREET_DECISION` : holdChange 4,5 ; holdTight 3,2 ; holdCritical 6,5 ; holdSoft 3 ; tightMargin 18.
		const double Margin = Scores[0].Value - (Scores.Num() > 1 ? Scores[1].Value : 0.0);
		const FString& Goal = Scores[0].Key;
		const bool bChanged = Goal != PreviousGoal;
		const bool bTight = !bChanged && Margin >= 0.0 && Margin < 18.0;
		if (!bChanged && !bTight) return;
		double Hold = bChanged ? 4.5 : 3.2;
		const bool bSoft = Goal == GoalObserver || Goal == TEXT("explore") || Goal == GoalSocialize || Goal == GoalRelax
			|| Goal == TEXT("play") || Goal == TEXT("study");
		if (NeedsCritical(Npc.Needs)) Hold = 6.5;
		else if (bSoft) Hold = FMath::Min(Hold, 3.0);
		FStreetDecision D;
		D.At = Now;
		D.Until = Now + Hold;
		D.Goal = Goal;
		D.From = PreviousGoal;
		D.Margin = RoundTenth(Margin);
		D.bChanged = bChanged;
		D.bTight = bTight;
		D.Cause = (Npc.GoalExplain.IsSet() && Npc.GoalExplain->Top.Num() > 0) ? Npc.GoalExplain->Top[0].Cause : FString();
		Npc.StreetDecision = D;
	}

	bool FVillage::AssignTarget(FNpc& Npc, FDecisionTrace& Trace)
	{
		// `resolveTarget` -> `resolveNpcDestination` (destination.js), en fin d'`assignTarget` pour tout but :
		// hors `socialize` / `visitFamily` (et `play`), `npc.socialSeekId = null` (premiere-pensee-001).
		Npc.bHasSocialSeekId = true;
		if (Npc.Goal != GoalSocialize && Npc.Goal != TEXT("visitFamily") && Npc.Goal != TEXT("play")) Npc.SocialSeekId.Reset();
		FPoint Target;
		FString Source;
		bool bFound = false;
		if (Npc.Goal == GoalDrink) bFound = DrinkTarget(Npc, Target, Source);
		else if (Npc.Goal == GoalRest) bFound = RestTarget(Npc, Target, Source);
		else if (Npc.Goal == GoalEat) bFound = EatTarget(Npc, Target, Source);
		else if (Npc.Goal == GoalSocialize) bFound = SocializeTarget(Npc, Target, Source);
		else if (Npc.Goal == GoalRelax) bFound = RelaxTarget(Npc, Target, Source);
		else if (Npc.Goal == TEXT("gatherWood") && IsWoodHarvester(Npc)) bFound = WoodTarget(Npc, Target, Source);
		else if (IsGranaryWorker(Npc) && Npc.Goal == GoalGatherFood) bFound = GatherTarget(Npc, Target, Source);
		else if (IsGranaryWorker(Npc) && Npc.Goal == GoalDeliver) bFound = DeliverTarget(Npc, Target, Source);
		else if (Npc.Goal == TEXT("gatherFood") || Npc.Goal == TEXT("deliver")) bFound = FoodSupplyTarget(Npc, Target, Source);
		else if (Npc.Goal == GoalShelterRain) bFound = ShelterRainTarget(Npc, Target, Source);
		else if (Npc.Goal == GoalHelpFarm) bFound = HelpFarmTarget(Npc, Target, Source);
		else if (Npc.Goal == AnastasisBuild::GoalBuild)
		{
			// `bindBuildSite` puis `constructionAccessPoint`, sinon `marketAccessPoint` : sans chantier,
			// le batisseur va au site du marche (planner-wiring-001).
			bFound = ConstructionAccessPoint(Npc, Target);
			Source = TEXT("site");
			if (!bFound && bHasColony)
			{
				bFound = MarketAccessPoint(&Npc, Target);
				Source = TEXT("market");
			}
		}
		else
		{
			Npc.bHasTarget = false;
			return false;
		}
		Trace.TargetSource = Source;
		if (!bFound)
		{
			// La reference trouve toujours une cible (repli au camp) ; ici, sans eau ni
			// logement atteignable, l'habitant vaque.
			if (Npc.Goal == GoalEat)
			{
				ReleaseMeal(Npc, TEXT("no_target"));
			}
			Npc.Goal = GoalObserver;
			Npc.bHasTarget = false;
			return false;
		}
		Npc.bHasTarget = true;
		Npc.Target = Target;
		Npc.DoorStuckAt = 0.0;
		Npc.DoorApproachAt = 0.0;
		return true;
	}

	void FVillage::RedirectAfterFailure(FNpc& Npc)
	{
		// `failureCauseForGoal(sim, npc, failedGoal)` : seul `explore` tire (exploreTarget).
		// Le C++ ne porte pas le but `explore` (il donne `observer`) : la branche est la pour
		// la fidelite, elle ne tire jamais aujourd'hui.
		if (Npc.Goal == TEXT("explore"))
		{
			ExploreTargetFor(Npc);
		}
		ClearNavigation(Npc);
		Npc.bHasTarget = false;
		Npc.DestBuildingId.Reset();
		Npc.Goal = GoalObserver;
		Npc.WorkTimer = 0.0;
		Npc.FailedActions = 0;
		Npc.StuckStage = 0;
		Npc.DoorStuckAt = 0.0;
		Npc.DoorApproachAt = 0.0;
		SetActivity(Npc, TEXT("attend"));
	}

	void FVillage::RedirectDomesticDoorFailure(FNpc& Npc)
	{
		// Porte inaccessible : le besoin se fait dehors, puis `explore` (non porte : observer).
		Npc.DoorStuckAt = 0.0;
		Npc.DoorApproachAt = 0.0;
		ClearNavigation(Npc);
		Npc.PathCooldown = 0.6;
		if (Npc.Goal == GoalRest || Npc.Goal == GoalDrink || Npc.Goal == GoalEat || Npc.Goal == GoalRelax)
		{
			if (Perform(Npc))
			{
				Npc.FailedActions = 0;
				Npc.Goal = GoalObserver;
				Npc.bHasTarget = false;
				Npc.DestBuildingId.Reset();
				return;
			}
		}
		RedirectAfterFailure(Npc);
	}

	bool FVillage::ReachedMoveTarget(const FNpc& Npc, const FPoint& Target) const
	{
		if (!FMath::IsFinite(Target.X) || !FMath::IsFinite(Target.Y)) return true;
		if (Dist(Npc.X, Npc.Y, Target.X, Target.Y) <= ArrivalDistance) return true;
		if (!IsFootBlocked(Target.X, Target.Y) || IsFootBlocked(Npc.X, Npc.Y)) return false;
		const int32 AX = FloorInt(Npc.X);
		const int32 AY = FloorInt(Npc.Y);
		const int32 TX = FloorInt(Target.X);
		const int32 TY = FloorInt(Target.Y);
		return FMath::Max(FMath::Abs(AX - TX), FMath::Abs(AY - TY)) <= 1;
	}

	namespace
	{
		/** `travelActivity(goal)` (npc.js l. 3821) : le mot d'un habitant en route, selon son but. */
		const TCHAR* JsTravelActivity(const FString& Goal)
		{
			if (Goal == TEXT("deliver") || Goal == TEXT("sell") || Goal == TEXT("buy") || Goal == TEXT("fetchInput")
				|| Goal == TEXT("haulJob") || Goal == TEXT("haulCart")) return TEXT("livre");
			if (Goal == TEXT("socialize") || Goal == TEXT("visitFamily") || Goal == TEXT("aidHousehold") || Goal == TEXT("play")) return TEXT("socialise");
			if (Goal == TEXT("relax")) return TEXT("relaxe");
			if (Goal == TEXT("relieve")) return TEXT("besoins");
			if (Goal == TEXT("drink")) return TEXT("boit");
			if (Goal == TEXT("gatherWood") || Goal == TEXT("gatherStone") || Goal == TEXT("gatherFood") || Goal == TEXT("explore")
				|| Goal == TEXT("helpFarm")) return TEXT("cherche");
			if (Goal == TEXT("build")) return TEXT("chantier");
			if (Goal == TEXT("craft") || Goal == TEXT("maintain")) return TEXT("travaille");
			if (Goal == TEXT("apprentice")) return TEXT("apprend");
			if (Goal == TEXT("study")) return TEXT("etudie");
			if (Goal == TEXT("rest")) return TEXT("repose");
			if (Goal == TEXT("eat") || Goal == TEXT("eatTogether")) return TEXT("mange");
			// Rites orthodoxes, `confront`, `shelterRain`, `closeWorkplace` et le reste.
			return TEXT("marche");
		}
	}

	void FVillage::SetActivity(FNpc& Npc, const FString& Activity)
	{
		// `maybeStampRestTrace` (repose, relaxe, dort) n'est pas porte (ecart n°34).
		if (Npc.Activity == Activity) return;
		Npc.Activity = Activity;
		Npc.ActivitySince = Now;
	}

	void FVillage::Act(FNpc& Npc, double Dt)
	{
		if (Npc.bHasTarget && !ReachedMoveTarget(Npc, Npc.Target))
		{
			// `if (!CRAFT_GOALS.has(npc.goal)) clearWorkSession(npc)`.
			if (Npc.Goal != GoalGatherFood && Npc.Goal != AnastasisBuild::GoalBuild) ClearWorkSession(Npc);
			// `setActivity(sim, npc, travelActivity(npc.goal))` (planner-wiring-001 : le C++ ecrivait « marche » pour tous).
			SetActivity(Npc, JsTravelActivity(Npc.Goal));
			MoveActor(Npc, Npc.Target, Dt);
			return;
		}

		// `tryEnterIndoorAction` a l'arrivee, pour tout but : ici le seul non domestique qui entre
		// est `shelterRain` (son poste, son foyer, un logement, le puits).
		if (Npc.Goal == GoalShelterRain && TryEnterIndoorAction(Npc))
		{
			Npc.DoorStuckAt = 0.0;
			Npc.DoorApproachAt = 0.0;
			return;
		}

		if (IsDomesticGoal(Npc.Goal))
		{
			if (TryEnterIndoorAction(Npc))
			{
				Npc.DoorStuckAt = 0.0;
				Npc.DoorApproachAt = 0.0;
				return;
			}
			const FString Living = Npc.LivingHomeId();
			if (!Living.IsEmpty())
			{
				// Foyer : on n'agit pas dehors. On insiste a la porte, puis on se debloque.
				FPoint Access = Npc.Target;
				const bool bAccess = Npc.bHasTarget || BuildingAccessPointById(Living, &Npc, Access);
				if (bAccess)
				{
					Npc.bHasTarget = true;
					Npc.Target = Access;
				}
				SetActivity(Npc, DomesticActivity(Npc.Goal, IsNight()));
				const bool bAtDoor = bAccess && Dist(Npc.X, Npc.Y, Access.X, Access.Y) <= DoorAccessRadius;
				if (!bAccess || !bAtDoor)
				{
					Npc.DoorApproachAt += Dt;
					if (!bAtDoor && bAccess) MoveActor(Npc, Access, Dt);
					if (Npc.DoorApproachAt >= DoorApproachSeconds) RedirectDomesticDoorFailure(Npc);
					return;
				}
				Npc.DoorApproachAt = 0.0;
				Npc.DoorStuckAt += Dt;
				if (Npc.DoorStuckAt >= DoorWaitSeconds) RedirectDomesticDoorFailure(Npc);
				return;
			}
			// Sans toit ni abri : pas de porte a forcer, on se repose dehors.
		}
		Npc.DoorStuckAt = 0.0;
		Npc.DoorApproachAt = 0.0;

		// `observer` passe par le geste generique comme dans la reference : il note le lieu, attend,
		// puis `perform` echoue (aucun cas pour lui) et l'echec compte (act-gate-001).
		if (!IsPortedGoalFor(Npc, Npc.Goal) && Npc.Goal != GoalObserver)
		{
			// But non porte : il n'accomplit rien. La reference ferait `perform`.
			SetActivity(Npc, TEXT("attend"));
			Npc.WorkTimer = 0.0;
			return;
		}

		if (Npc.Goal == TEXT("gatherWood"))
		{
			if (ProgressWoodGather(Npc) == 1) { Npc.FailedActions = 0; return; }
			ClearWorkSession(Npc);
			Npc.Goal = GoalObserver; Npc.bHasTarget = false; ClearNavigation(Npc);
			return;
		}

		// Sessions multi-coups hors du gate workTimer (collecte du fermier).
		if (Npc.Goal == GoalGatherFood && IsGranaryWorker(Npc))
		{
			const int32 Craft = ProgressCraftGather(Npc);
			if (Craft != 0)
			{
				// `craft === "working"` : le geste compte au lieu (activite du profil `farm`).
				if (Craft == 1) NotePlaceUse(Npc, TEXT("recolte"), Dt * 0.35);
				Npc.FailedActions = 0;
				return;
			}
			SetActivity(Npc, TEXT("attend"));
			if (++Npc.FailedActions >= 3) RedirectAfterFailure(Npc);
			return;
		}
		// `helpFarm` : profil `tend`, session de soins de parcelle (help-farm-001). `workAtWorkplaceYard`, qui
		// ancrerait la session au poste et noterait un second geste, n'est pas porte (ecart n°26).
		if (Npc.Goal == GoalHelpFarm)
		{
			const int32 Craft = ProgressTendWork(Npc);
			if (Craft != 0)
			{
				if (Craft == 1) NotePlaceUse(Npc, TEXT("recolte"), Dt * 0.35);
				Npc.FailedActions = 0;
				return;
			}
			SetActivity(Npc, TEXT("attend"));
			if (++Npc.FailedActions >= 3) RedirectAfterFailure(Npc);
			return;
		}
		if (Npc.Goal == AnastasisBuild::GoalBuild)
		{
			const int32 Craft = ProgressBuildWork(Npc, Dt);
			if (Craft != 0)
			{
				if (Craft == 1) NotePlaceUse(Npc, TEXT("chantier"), Dt * 0.35);
				Npc.FailedActions = 0;
				if (Craft == 2)
				{
					// Un travail de chantier fini (achevement, ou chantier a sec) relache la cible :
					// la reference repense a chaque pensee, ce portage seulement sans cible (ecart n°2).
					Npc.bHasTarget = false;
					ClearNavigation(Npc);
				}
				return;
			}
			SetActivity(Npc, TEXT("attend"));
			if (++Npc.FailedActions >= 3) RedirectAfterFailure(Npc);
			return;
		}
		if (Npc.WorkSession.bActive) ClearWorkSession(Npc);
		// `notePlaceUse(npc, waitingActivity(goal), dt * 0.35)`, avant le chronometre du geste.
		NotePlaceUse(Npc, JsWaitingActivity(Npc), Dt * 0.35);

		// Livrer a son propre depot : DEHORS, au seuil (`DEPOT_OUTSIDE`), jamais d'entree.
		Npc.WorkTimer += Dt;
		if (Npc.WorkTimer < (Npc.Goal == TEXT("gatherFood") ? 3.0 : 1.0))
		{
			// `waitingActivity` : « boit », « mange », « livre » ; pour rest, `restActivity`.
			// L'extension food-supply garde ses mots (« cueille », « depose »).
			const bool bWorker = IsGranaryWorker(Npc);
			SetActivity(Npc, Npc.Goal == GoalDrink ? TEXT("boit")
				: Npc.Goal == GoalEat ? TEXT("mange")
				: Npc.Goal == GoalDeliver ? (bWorker ? TEXT("livre") : TEXT("depose"))
				: Npc.Goal == GoalGatherFood ? TEXT("cueille")
				: Npc.Goal == GoalSocialize ? TEXT("discute")
				: Npc.Goal == GoalRelax ? TEXT("relaxe")
				: Npc.Goal == GoalShelterRain ? TEXT("abrite")
				: Npc.Goal == GoalObserver ? TEXT("attend")
				: RestActivity(IsNight()));
			return;
		}
		Npc.WorkTimer = 0.0;
		const bool bHadTarget = Npc.bHasTarget;
		const FPoint TargetBefore = Npc.Target;
		const FString GoalBefore = Npc.Goal;
		const bool bWorked = Perform(Npc);
		if (bWorked) NotePlaceUse(Npc, GoalBefore, 1.0);
		if (!bWorked)
		{
			SetActivity(Npc, TEXT("attend"));
			// `if (npc.failedActions >= 3) redirectAfterFailure(sim, npc)` (act-gate-001 : la porte
			// generique le fait, comme `updateInside`).
			if (++Npc.FailedActions >= 3) RedirectAfterFailure(Npc);
		}
		else
		{
			Npc.FailedActions = 0;
		}
		// `if (npc.target === target) npc.target = null;` — force une nouvelle decision, sauf si
		// l'acte (ou le renvoi apres echec) vient de poser une autre cible.
		const bool bSameTarget = Npc.bHasTarget == bHadTarget
			&& (!bHadTarget || (Npc.Target.X == TargetBefore.X && Npc.Target.Y == TargetBefore.Y));
		if (bSameTarget)
		{
			Npc.bHasTarget = false;
			ClearNavigation(Npc);
		}
	}

	FString FVillage::JsWaitingActivity(const FNpc& Npc) const
	{
		// `waitingActivity(sim, npc, goal)` (npc.js l. 3843), pour les buts que ce village connait.
		// Le mot sert de `kind` a `notePlaceUse` : c'est celui de la reference, meme quand l'affichage
		// du C++ en garde un autre (« discute », « cueille », « depose »).
		const FString& Goal = Npc.Goal;
		if (Goal == GoalDeliver || Goal == TEXT("sell") || Goal == TEXT("buy") || Goal == TEXT("fetchInput")) return TEXT("livre");
		if (Goal == GoalSocialize || Goal == TEXT("visitFamily") || Goal == TEXT("aidHousehold")) return TEXT("socialise");
		if (Goal == TEXT("confront")) return TEXT("attend");
		if (Goal == GoalShelterRain) return TEXT("abrite");
		if (Goal == TEXT("closeWorkplace")) return TEXT("range");
		if (Goal == GoalRelax) return TEXT("relaxe");
		if (Goal == TEXT("relieve")) return TEXT("besoins");
		if (Goal == GoalDrink) return TEXT("boit");
		if (Goal == AnastasisBuild::GoalBuild) return TEXT("chantier");
		if (Goal == GoalGatherFood) return TEXT("recolte");
		if (Goal == GoalRest) return RestActivity(IsNight());
		if (Goal == GoalEat || Goal == TEXT("eatTogether")) return TEXT("mange");
		return TEXT("attend");
	}

	bool FVillage::Perform(FNpc& Npc)
	{
		// Extension food-supply : tout habitant qui n'est pas le fermier du grenier.
		if ((Npc.Goal == GoalGatherFood || Npc.Goal == GoalDeliver) && !IsGranaryWorker(Npc)) return PerformFoodSupply(Npc);
		if (Npc.Goal == GoalDrink)
		{
			// `case "drink"` : setActivity("boit"), satisfyDrink, markDrink (gestuelle, non portee).
			SetActivity(Npc, TEXT("boit"));
			AnastasisNeeds::SatisfyDrink(Npc.Needs);
			++Npc.DrinksTaken;
			return true;
		}
		if (Npc.Goal == GoalEat)
		{
			// `case "eat"` : setActivity("mange"), eat().
			SetActivity(Npc, TEXT("mange"));
			return Eat(Npc);
		}
		if (Npc.Goal == GoalSocialize)
		{
			// `case "socialize"` : socialize(). Compagnon le mieux place a portee (ecart n°16),
			// sinon la branche ambiante : `satisfySocial(npc, NEEDS.socialAmbient)` puis moral +1.
			SetActivity(Npc, TEXT("discute"));
			++Npc.SocialsTaken;
			const double MaxDistance = Npc.Inside.bActive ? AnastasisBonds::MaxIndoor : AnastasisBonds::MaxOutdoor;
			if (FNpc* Other = PickSocialCompanion(Npc, MaxDistance))
			{
				SocializeWithCompanion(Npc, *Other);
				return true;
			}
			AnastasisNeeds::SatisfySocial(Npc.Needs, AnastasisNeeds::Constants::SocialAmbient, Npc.Inside.bActive);
			Npc.Needs.Morale = Clamp(Npc.Needs.Morale + 1.0, 0.0, 100.0);
			return true;
		}
		if (Npc.Goal == GoalRelax)
		{
			// `case "relax"` : setActivity("relaxe"), satisfyRelax (scene de foyer : non portee).
			SetActivity(Npc, TEXT("relaxe"));
			AnastasisNeeds::SatisfyRelax(Npc.Needs, Npc.Inside.bActive);
			++Npc.RelaxesTaken;
			return true;
		}
		if (Npc.Goal == GoalShelterRain)
		{
			// `case "shelterRain"` : setActivity("abrite"), performShelterRain().
			SetActivity(Npc, TEXT("abrite"));
			return PerformShelterRain(Npc);
		}
		if (Npc.Goal == GoalDeliver)
		{
			// `case "deliver"` : setActivity("livre"), deliver().
			SetActivity(Npc, TEXT("livre"));
			return Deliver(Npc);
		}
		if (Npc.Goal == GoalRest)
		{
			// `case "rest"` : setActivity(restActivity), satisfyRest.
			const bool bNight = IsNight();
			const FString& Living = Npc.LivingHomeId();
			const bool bAtHome = !Living.IsEmpty() && Npc.Inside.bActive && Npc.Inside.BuildingId == Living;
			SetActivity(Npc, RestActivity(bNight));
			AnastasisNeeds::SatisfyRest(Npc.Needs, bNight, SleepQualityOf(Npc), Npc.Inside.bActive, bAtHome);
			++Npc.RestsTaken;
			return true;
		}
		return false;
	}

	// --- Deplacement -----------------------------------------------------------

	void FVillage::ClearNavigation(FNpc& Npc)
	{
		Npc.Path.Reset();
		Npc.PathStep = 0;
		Npc.bHasPathGoal = false;
		Npc.bPathFailed = false;
		Npc.PathCooldown = 0.0;
		Npc.NavTargetKey.Reset();
		Npc.NavVersion = -1;
		Npc.StuckTimer = 0.0;
	}

	FPoint FVillage::NextWaypoint(FNpc& Npc, const FPoint& Target, double Dt)
	{
		Npc.PathCooldown = FMath::Max(0.0, Npc.PathCooldown - Dt);
		const FString GoalKey = TargetKey(Target);
		const bool bHasStep = Npc.Path.IsValidIndex(Npc.PathStep);
		const bool bStepBlocked = bHasStep && IsFootBlocked(Npc.Path[Npc.PathStep].X, Npc.Path[Npc.PathStep].Y);
		const bool bNeedsPath =
			Npc.Path.Num() == 0
			|| Npc.PathStep >= Npc.Path.Num()
			|| !Npc.bHasPathGoal
			|| Dist(Npc.PathGoal.X, Npc.PathGoal.Y, Target.X, Target.Y) > 1.2
			|| Npc.NavVersion != NavVersion
			|| Npc.NavTargetKey != GoalKey
			|| bStepBlocked;

		if (bNeedsPath && Npc.PathCooldown <= 0.0)
		{
			AnastasisPath::FOptions Options;
			Options.bAllowBlockedTarget = IsFootBlocked(Target.X, Target.Y);
			const AnastasisPath::FWorldNavSource Source(Nav, *World);
			TArray<FPoint> NewPath;
			const bool bOk = AnastasisPath::FindPath(Source, { Npc.X, Npc.Y }, Target, Options, NewPath);
			Npc.NavTargetKey = GoalKey;
			Npc.NavVersion = NavVersion;
			Npc.bHasPathGoal = true;
			Npc.PathGoal = Target;
			Npc.PathStep = 0;
			if (bOk)
			{
				Npc.Path = MoveTemp(NewPath);
				Npc.bPathFailed = false;
			}
			else
			{
				// Pas de chemin : on ne relance pas l'A* a chaque tick.
				Npc.Path.Reset();
				Npc.bPathFailed = true;
				Npc.PathCooldown = FMath::Max(0.12, Dt * 2.0);
			}
		}

		const FPoint Here = { Npc.X, Npc.Y };
		if (!Npc.Path.IsValidIndex(Npc.PathStep))
		{
			// Chemin en echec : la reference contourne localement (`steerAroundBlock`,
			// non porte). On reste sur place ; le compteur de blocage prend la suite.
			if (Npc.bPathFailed) return Here;
			if (IsFootBlocked(Target.X, Target.Y) && !IsFootBlocked(Npc.X, Npc.Y)) return Here;
			return Target;
		}
		const FPoint Point = Npc.Path[Npc.PathStep];
		if (IsFootBlocked(Point.X, Point.Y) && !IsFootBlocked(Npc.X, Npc.Y))
		{
			Npc.PathStep = Npc.Path.Num();
			return Here;
		}
		if (Dist(Npc.X, Npc.Y, Point.X, Point.Y) < 0.5)
		{
			++Npc.PathStep;
			const FPoint Next = Npc.Path.IsValidIndex(Npc.PathStep) ? Npc.Path[Npc.PathStep] : Target;
			if (IsFootBlocked(Next.X, Next.Y) && !IsFootBlocked(Npc.X, Npc.Y))
			{
				Npc.PathStep = Npc.Path.Num();
				return Here;
			}
			return Next;
		}
		return Point;
	}

	void FVillage::MoveActor(FNpc& Npc, const FPoint& Target, double Dt)
	{
		const double BeforeX = Npc.X;
		const double BeforeY = Npc.Y;
		FPoint Waypoint = NextWaypoint(Npc, Target, Dt);

		// Pas de marche de la reference : budget de distance, enchaine jusqu'a 8 noeuds.
		// `speed * movementSpeedFactor(...)` : seul le bloc pluie de ce facteur est porte (ecarts n°4, n°17).
		double Budget = Npc.Speed * AnastasisWeatherBehavior::RainSpeedFactor(TickDailyRain, Npc.Goal, Npc.JobId) * FMath::Max(0.0, Dt);
		for (int32 Guard = 0; Guard < 8 && Budget > 1e-4; ++Guard)
		{
			if (Guard > 0)
			{
				Waypoint = NextWaypoint(Npc, Target, 0.0);
			}
			const double DX = Waypoint.X - Npc.X;
			const double DY = Waypoint.Y - Npc.Y;
			const double Len = JsHypot(DX, DY);
			if (Len < 1e-4) break;
			const double DirX = DX / Len;
			const double DirY = DY / Len;
			// ecart n°29 : l'A* paie deja le cout de la case visee ; en mode jeu,
			// chaque segment depense le meme multiplicateur en temps de marche.
			// Le mode reference conserve exactement son ancien calcul.
			const double TravelCost = bTerrainTravelCostEnabled
				? AnastasisNav::MoveCostAt(Nav, FloorInt(Waypoint.X), FloorInt(Waypoint.Y)) : 1.0;
			if (!FMath::IsFinite(TravelCost) || TravelCost <= 0.0) break;
			const double Step = FMath::Min(Len, Budget / TravelCost);
			const double NextX = Npc.X + DirX * Step;
			const double NextY = Npc.Y + DirY * Step;
			// Exception unique : sortir d'une case deja bloquee.
			const bool bCellStuck = IsFootBlocked(Npc.X, Npc.Y);
			const double X0 = Npc.X;
			const double Y0 = Npc.Y;
			if (bCellStuck || !IsFootBlocked(NextX, Npc.Y))
			{
				Npc.X = Clamp(NextX, 1.0, Nav.W - 2);
			}
			if (bCellStuck || !IsFootBlocked(Npc.X, NextY))
			{
				Npc.Y = Clamp(NextY, 1.0, Nav.H - 2);
			}
			const double MovedSeg = JsHypot(Npc.X - X0, Npc.Y - Y0);
			if (MovedSeg < 1e-5) break;
			Budget -= MovedSeg * TravelCost;
			if (JsHypot(Npc.X - Waypoint.X, Npc.Y - Waypoint.Y) >= 0.45) break;
		}

		const double Moved = JsHypot(Npc.X - BeforeX, Npc.Y - BeforeY);
		if (Dist(Npc.X, Npc.Y, Target.X, Target.Y) > 1.2 && Moved < 0.05)
		{
			Npc.StuckTimer += Dt;
			if (Npc.StuckTimer > 0.75)
			{
				ResolveStuckActor(Npc, Target);
			}
		}
		else
		{
			Npc.StuckTimer = 0.0;
			if (Moved >= 0.05) Npc.StuckStage = 0;
		}
	}

	void FVillage::ResolveStuckActor(FNpc& Npc, const FPoint& Target)
	{
		// Escalade reduite (ecart n°4) : 1) recalcul, 2) autre seuil du meme batiment, 3) abandon.
		Npc.StuckTimer = 0.0;
		++Npc.StuckStage;
		if (Npc.StuckStage == 1)
		{
			ClearNavigation(Npc);
			return;
		}
		if (Npc.StuckStage == 2 && !Npc.DestBuildingId.IsEmpty())
		{
			if (FBuilding* Building = Buildings.FindById(Npc.DestBuildingId))
			{
				FPoint Other;
				if (PickBuildingAccessPoint(*Building, &Npc, Other, &Target))
				{
					ClearNavigation(Npc);
					Npc.Target = Other;
					return;
				}
			}
		}
		// `_navAbandon` -> redirectDomesticDoorFailure (foyer) ou redirectAfterFailure.
		++Npc.FailedActions;
		if (IsDomesticGoal(Npc.Goal) && !Npc.LivingHomeId().IsEmpty())
		{
			RedirectDomesticDoorFailure(Npc);
		}
		else
		{
			RedirectAfterFailure(Npc);
		}
	}

	// --- Observation ----------------------------------------------------------

	TArray<FString> FVillage::UsersOf(const FString& BuildingId) const
	{
		TArray<FString> Users;
		for (const FNpc& Npc : Actors.GetItems())
		{
			const bool bHeading = IsPortedGoalFor(Npc, Npc.Goal) && Npc.DestBuildingId == BuildingId;
			const bool bInside = Npc.Inside.bActive && Npc.Inside.BuildingId == BuildingId;
			if (bHeading || bInside)
			{
				Users.Add(Npc.Id);
			}
		}
		return Users;
	}

	uint64 FVillage::Digest() const
	{
		AnastasisDigest::FStateWriter Writer;
		Writer.BeginObject();
		Writer.Key(TEXT("buildings")).BeginArray(Buildings.Num());
		for (const FBuilding& B : Buildings.GetItems())
		{
			Writer.BeginObject();
			Writer.Key(TEXT("id")).String(B.Id);
			Writer.Key(TEXT("type")).String(B.Type);
			Writer.Key(TEXT("x")).Number(B.X);
			Writer.Key(TEXT("y")).Number(B.Y);
			Writer.Key(TEXT("progress")).Number(B.Progress);
			Writer.Key(TEXT("owner"));
			if (B.Owner.IsEmpty()) Writer.Null(); else Writer.String(B.Owner);
			Writer.Key(TEXT("housePhase")).Number(B.HousePhase);
			Writer.Key(TEXT("piecesPlaced")).Number(B.PiecesPlaced);
			if (B.bHasMaterials)
			{
				Writer.Key(TEXT("site")).BeginObject();
				Writer.Key(TEXT("needWood")).Number(B.Materials.NeedWood);
				Writer.Key(TEXT("needStone")).Number(B.Materials.NeedStone);
				Writer.Key(TEXT("consumedWood")).Number(B.Materials.ConsumedWood);
				Writer.Key(TEXT("consumedStone")).Number(B.Materials.ConsumedStone);
				Writer.Key(TEXT("stockWood")).Number(B.Materials.StockWood);
				Writer.Key(TEXT("stockStone")).Number(B.Materials.StockStone);
				Writer.EndObject();
			}
			Writer.Key(TEXT("food")).BeginObject();
			Writer.Key(TEXT("physical")).Number(B.FoodPhysical);
			Writer.Key(TEXT("reserved")).Number(B.FoodReserved);
			Writer.EndObject();
			Writer.Key(TEXT("accessPoints")).BeginArray(B.AccessPoints.Num());
			for (const FPoint& P : B.AccessPoints)
			{
				Writer.BeginObject();
				Writer.Key(TEXT("x")).Number(P.X);
				Writer.Key(TEXT("y")).Number(P.Y);
				Writer.EndObject();
			}
			Writer.EndArray();
			Writer.EndObject();
		}
		Writer.EndArray();

		Writer.Key(TEXT("foodSources")).BeginArray(FoodSources.Num());
		for (const FFoodSource& S : FoodSources)
		{
			Writer.BeginObject();
			Writer.Key(TEXT("tile")).Number(S.TileIndex);
			Writer.Key(TEXT("initial")).Number(S.Initial);
			Writer.Key(TEXT("remaining")).Number(S.Remaining);
			Writer.EndObject();
		}
		Writer.EndArray();
		Writer.Key(TEXT("actors")).BeginArray(Actors.Num());
		for (const FNpc& N : Actors.GetItems())
		{
			Writer.BeginObject();
			Writer.Key(TEXT("id")).String(N.Id);
			Writer.Key(TEXT("x")).Number(N.X);
			Writer.Key(TEXT("y")).Number(N.Y);
			Writer.Key(TEXT("goal")).String(N.Goal);
			Writer.Key(TEXT("target"));
			if (N.bHasTarget)
			{
				Writer.BeginObject();
				Writer.Key(TEXT("x")).Number(N.Target.X);
				Writer.Key(TEXT("y")).Number(N.Target.Y);
				Writer.EndObject();
			}
			else
			{
				Writer.Null();
			}
			Writer.Key(TEXT("homeId"));
			if (N.HomeId.IsEmpty()) Writer.Null(); else Writer.String(N.HomeId);
			Writer.Key(TEXT("shelterId"));
			if (N.ShelterId.IsEmpty()) Writer.Null(); else Writer.String(N.ShelterId);
			Writer.Key(TEXT("inside"));
			if (N.Inside.bActive)
			{
				Writer.BeginObject();
				Writer.Key(TEXT("buildingId")).String(N.Inside.BuildingId);
				Writer.Key(TEXT("goal")).String(N.Inside.Goal);
				Writer.Key(TEXT("until")).Number(N.Inside.Until);
				Writer.EndObject();
			}
			else
			{
				Writer.Null();
			}
			Writer.Key(TEXT("hunger")).Number(N.Needs.Hunger);
			Writer.Key(TEXT("energy")).Number(N.Needs.Energy);
			Writer.Key(TEXT("social")).Number(N.Needs.Social);
			Writer.Key(TEXT("leisure")).Number(N.Needs.Leisure);
			Writer.Key(TEXT("hygiene")).Number(N.Needs.Hygiene);
			Writer.Key(TEXT("thirst")).Number(N.Needs.Thirst);
			Writer.Key(TEXT("health")).Number(N.Needs.Health);
			Writer.Key(TEXT("morale")).Number(N.Needs.Morale);
			Writer.Key(TEXT("workTimer")).Number(N.WorkTimer);
			Writer.Key(TEXT("inventoryFood")).Number(N.InventoryFood);
			// ecart n°30: preserve legacy digest for actors carrying no wood.
			if (N.InventoryWood != 0 || N.GatheredWood != 0)
			{
				Writer.Key(TEXT("inventoryWood")).Number(N.InventoryWood);
				Writer.Key(TEXT("gatheredWood")).Number(N.GatheredWood);
			}
			Writer.Key(TEXT("foodSource")).Number(N.FoodSourceIndex);
			Writer.Key(TEXT("knownFood")).BeginArray(FoodSources.Num());
			for (const FFoodSource& S : FoodSources)
			{
				const int32* Known = N.KnownFoodSources.Find(S.TileIndex);
				if (Known) Writer.Number(*Known); else Writer.Null();
			}
			Writer.EndArray();
			Writer.Key(TEXT("hungerAction")).String(N.HungerAction.State);
			Writer.Key(TEXT("jobId")).String(N.JobId);
			Writer.Key(TEXT("workplaceId"));
			if (N.WorkplaceId.IsEmpty()) Writer.Null(); else Writer.String(N.WorkplaceId);
			Writer.Key(TEXT("skill")).Number(N.Skill);
			Writer.Key(TEXT("skills")).BeginObject();
			Writer.Key(TEXT("gather")).Number(N.SkillGather);
			Writer.Key(TEXT("trade")).Number(N.SkillTrade);
			Writer.EndObject();
			Writer.Key(TEXT("workSession"));
			if (N.WorkSession.bActive)
			{
				Writer.BeginObject();
				Writer.Key(TEXT("craftId")).String(N.WorkSession.CraftId);
				Writer.Key(TEXT("tileX")).Number(N.WorkSession.TileX);
				Writer.Key(TEXT("tileY")).Number(N.WorkSession.TileY);
				Writer.Key(TEXT("postIndex")).Number(N.WorkSession.PostIndex);
				Writer.Key(TEXT("nextSwingAt")).Number(N.WorkSession.NextSwingAt);
				Writer.Key(TEXT("swingsDone")).Number(N.WorkSession.SwingsDone);
				Writer.EndObject();
			}
			else
			{
				Writer.Null();
			}
			Writer.Key(TEXT("spots")).BeginArray(N.Spots.Num());
			for (const FResourceSpot& Spot : N.Spots)
			{
				Writer.BeginObject();
				Writer.Key(TEXT("key")).String(Spot.Key);
				Writer.Key(TEXT("resource")).String(Spot.Resource);
				Writer.Key(TEXT("amount")).Number(Spot.Amount);
				Writer.Key(TEXT("day")).Number(Spot.Day);
				Writer.EndObject();
			}
			Writer.EndArray();
			Writer.EndObject();
		}
		Writer.EndArray();

		TArray<int32> Touched;
		LiveTiles.GetKeys(Touched);
		Touched.Sort();
		Writer.Key(TEXT("liveTiles")).BeginArray(Touched.Num());
		for (const int32 Index : Touched)
		{
			const AnastasisWorld::FTile& T = LiveTiles[Index];
			Writer.BeginObject();
			Writer.Key(TEXT("index")).Number(Index);
			Writer.Key(TEXT("type")).Number(static_cast<int32>(T.Type));
			Writer.Key(TEXT("resource")).Number(static_cast<int32>(T.Resource));
			Writer.Key(TEXT("amount")).Number(T.Amount);
			Writer.Key(TEXT("cropId")).Number(static_cast<int32>(T.CropId));
			Writer.EndObject();
		}
		Writer.EndArray();

		if (bSoilWaterEnabled)
		{
			TArray<int32> SoilKeys;
			SoilWaterByTile.GetKeys(SoilKeys);
			SoilKeys.Sort();
			Writer.Key(TEXT("soilWater")).BeginArray(SoilKeys.Num());
			for (int32 Index : SoilKeys)
			{
				Writer.BeginObject();
				Writer.Key(TEXT("index")).Number(Index);
				Writer.Key(TEXT("stored")).Number(SoilWaterByTile.FindChecked(Index));
				Writer.EndObject();
			}
			Writer.EndArray();
		}
		Writer.Key(TEXT("mealReservations")).BeginArray(MealReservations.Num());
		for (const FMealReservation& R : MealReservations)
		{
			Writer.BeginObject();
			Writer.Key(TEXT("id")).String(R.Id);
			Writer.Key(TEXT("npcId")).String(R.NpcId);
			Writer.Key(TEXT("buildingId"));
			if (R.BuildingId.IsEmpty()) Writer.Null(); else Writer.String(R.BuildingId);
			Writer.Key(TEXT("expiresAt")).Number(R.ExpiresAt);
			Writer.EndObject();
		}
		Writer.EndArray();
		Writer.EndObject();
		return Writer.Digest();
	}

	// --- Stock (sim/transport/stockLedger.js, nourriture seulement) -------------

	int32 FVillage::CreditFood(const FString& BuildingId, int32 Amount)
	{
		FBuilding* B = Buildings.FindById(BuildingId);
		// `creditStock` : entier, positif, batiment qui accepte la nourriture, dans la capacite.
		Amount = FMath::Max(0, Amount);
		if (!B || Amount <= 0 || B->Type != GranaryType || !B->IsCompleted()) return 0;
		const int32 Room = FMath::Max(0, GranaryFoodCap - B->FoodPhysical);
		const int32 Added = FMath::Min(Room, Amount);
		B->FoodPhysical += Added;
		return Added;
	}

	const FMealReservation* FVillage::FindMealReservation(const FString& NpcId) const
	{
		return MealReservations.FindByPredicate([&](const FMealReservation& R) { return R.NpcId == NpcId; });
	}

	// --- Perception (ai/memory.js, branche nourriture) ---------------------------

	int32 FVillage::Day() const
	{
		// `day = 1 + floor(time / DAY_LENGTH)`, comme l'hote.
		return 1 + static_cast<int32>(AnastasisJs::Floor(Now / AnastasisRhythm::DayLength));
	}

	void StampHouseVacant(FBuilding& Building, const int32 Day)
	{
		if (Building.Type != HouseType)
		{
			return;
		}
		if (!Building.Owner.IsEmpty())
		{
			Building.VacantSinceDay = -1;
			return;
		}
		if (Building.VacantSinceDay < 0)
		{
			Building.VacantSinceDay = Day;
		}
	}

	void StampHouseOccupied(FBuilding& Building)
	{
		Building.VacantSinceDay = -1;
	}

	int32 VacantAgeDays(const FBuilding& Building, const int32 Day)
	{
		if (!Building.Owner.IsEmpty() || Building.Type != HouseType)
		{
			return 0;
		}
		// `createdDay || day` : 0 est faux en JS.
		const int32 Since = Building.VacantSinceDay >= 0
			? Building.VacantSinceDay
			: (Building.CreatedDay != 0 ? Building.CreatedDay : Day);
		return FMath::Max(0, Day - Since);
	}

	int32 VacantAgeBand(const FBuilding& Building, const int32 Day)
	{
		const int32 Days = VacantAgeDays(Building, Day);
		if (Days >= 45) return 3;
		if (Days >= 18) return 2;
		if (Days >= 6) return 1;
		return 0;
	}

	void FVillage::PerceiveNow(const FString& NpcId)
	{
		if (FNpc* Npc = Actors.FindById(NpcId))
		{
			Perceive(*Npc, true);
		}
	}

	AnastasisRhythm::EPhase FVillage::PersonalPhaseOf(const FNpc& Npc) const
	{
		return AnastasisReconsider::PersonalPhase(AnastasisRhythm::DayFracOf(Now), Npc.Lifestyle);
	}

	double FVillage::ReconsiderChanceNow(const FString& NpcId, double ThinkDt) const
	{
		const FNpc* Npc = Actors.FindById(NpcId);
		return Npc ? ReconsiderChanceAt(*Npc, Now, ThinkDt) : 0.0;
	}

	double FVillage::ReconsiderChanceAt(const FNpc& Npc, double At, double ThinkDt) const
	{
		const bool bCritical = NeedsCritical(Npc.Needs);
		const double Base = AnastasisReconsider::NeedsReconsiderChance(bCritical, Npc.Goal, ThinkDt);
		const AnastasisRhythm::EPhase Personal = AnastasisReconsider::PersonalPhase(AnastasisRhythm::DayFracOf(At), Npc.Lifestyle);
		const double Reconsider = AnastasisReconsider::PhaseReconsiderChance(Personal, ThinkDt, Base);
		AnastasisReconsider::FCommitSubject Subject;
		Subject.bCritical = bCritical;
		Subject.bHasTarget = Npc.bHasTarget;
		Subject.Goal = Npc.Goal;
		Subject.PhaseChangedAt = Npc.PhaseChangedAt;
		Subject.GoalSince = Npc.GoalSince;
		Subject.bShiftShields = AnastasisWorkShift::ShiftShields(Npc.WorkShift, Npc.Goal, At, bCritical);
		return AnastasisReconsider::CommittedReconsiderChance(Subject, At, Reconsider);
	}

	void FVillage::ChooseGoalNow(const FString& NpcId)
	{
		if (FNpc* Npc = Actors.FindById(NpcId))
		{
			ChooseGoal(*Npc);
		}
	}

	AnastasisExplore::FExploreWorld FVillage::ExploreWorld() const
	{
		AnastasisExplore::FExploreWorld Out;
		Out.W = World ? World->W : 0;
		Out.H = World ? World->H : 0;
		Out.IsBlocked = [this](double X, double Y) { return IsBlocked(X, Y); };
		Out.IsFootBlocked = [this](double X, double Y) { return IsFootBlocked(X, Y); };
		Out.Settlement = Settlement;
		return Out;
	}

	AnastasisExplore::FExploreResult FVillage::ExploreTargetFor(const FNpc& Npc)
	{
		return AnastasisExplore::ExploreTarget(ExploreWorld(), VillageRng, Npc.X, Npc.Y, Npc.KnownCells);
	}

	bool FVillage::NoiseConditionHolds(const FNpc& Npc, AnastasisGoalNoise::ENoiseCondition Condition) const
	{
		using C = AnastasisGoalNoise::ENoiseCondition;
		auto AnyCompleted = [this](std::initializer_list<const TCHAR*> Types)
		{
			for (const TCHAR* Type : Types)
			{
				if (CountBuildings(Type) > 0) return true;
			}
			return false;
		};
		switch (Condition)
		{
		case C::Always:
			return true;
		case C::HasFarm:
			// `countBuildings("farm") > 0 || countBuildingsWith(d => d.function === "nourrir") > 0`
			// : les types `nourrir` du catalogue (sim/batiments/catalog.js).
			return AnyCompleted({ TEXT("farm"), TEXT("fishery"), TEXT("mill"), TEXT("sheepfold"), TEXT("stable"),
				TEXT("piggery"), TEXT("chickencoop"), TEXT("bakery"), TEXT("dairy"), TEXT("butcher") });
		case C::HasCraftBuilding:
			// `countBuildingsWith(d => d.function === "fabriquer" || d.produces?.tools) > 0`.
			return AnyCompleted({ TEXT("lodge"), TEXT("workshop"), TEXT("forge"), TEXT("tannery"), TEXT("weaver") });
		case C::HasFamilyToVisit:
			// `if (!npc.home || !npc.familyId) return 0;` : la famille n'est pas portee
			// (ecart n°8), aucun habitant C++ n'a de `familyId`.
			return false;
		default:
			return false;
		}
	}

	void FVillage::Perceive(FNpc& Npc, bool bForce)
	{
		if (!bForce && Now - Npc.LastScan < PerceptionScanInterval) return;
		Npc.LastScan = Now;
		// `markCell(sim, mind, Math.floor(npc.x), Math.floor(npc.y))` (perception-explore-001).
		if (World)
		{
			AnastasisExplore::MarkCell(Npc.KnownCells, Npc.CellCount, World->W, World->H, FloorInt(Npc.X), FloorInt(Npc.Y));
		}
		for (const FFoodSource& S : FoodSources)
		{
			if (Dist(Npc.X, Npc.Y, S.Position.X, S.Position.Y) <= PerceptionRadius)
				Npc.KnownFoodSources.Add(S.TileIndex, S.Remaining);
		}
		const double CX = AnastasisJs::Floor(Npc.X);
		const double CY = AnastasisJs::Floor(Npc.Y);
		// `perceiveBeliefs` : un depot qui accepte la nourriture est une reserve VISIBLE.
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Progress < 1.0 || B.Type != GranaryType) continue;
			const double BX = B.X + 0.5;
			const double BY = B.Y + 0.5;
			if (FMath::Abs(BX - CX) > PerceptionRadius + 1.0 || FMath::Abs(BY - CY) > PerceptionRadius + 1.0) continue;
			if (Dist(Npc.X, Npc.Y, BX, BY) > PerceptionRadius + 0.75) continue;
			// `noteStockBelief` -> `rememberBeliefEntry`.
			const FString Key = FString::Printf(TEXT("stock:food:%s"), *B.Id);
			FStockBelief* Known = Npc.KnownStocks.FindByPredicate([&](const FStockBelief& E) { return E.Key == Key; });
			if (!Known)
			{
				FStockBelief Entry;
				Entry.Key = Key;
				Entry.Resource = TEXT("food");
				Entry.BuildingId = B.Id;
				Entry.Kind = B.Type;
				Entry.X = BX;
				Entry.Y = BY;
				Entry.EstimatedAmount = FMath::Max(0.0, static_cast<double>(B.FoodPhysical));
				Entry.Confidence = 0.9;
				Entry.Day = Day();
				Npc.KnownStocks.Add(Entry);
			}
			else
			{
				Known->X = BX;
				Known->Y = BY;
				Known->Kind = B.Type;
				Known->BuildingId = B.Id;
				Known->EstimatedAmount = FMath::Max(0.0, static_cast<double>(B.FoodPhysical));
				Known->Confidence = FMath::Max(Known->Confidence, 0.9);
				Known->Day = Day();
			}
		}
		// Puis les gisements sous ses yeux, s'il a assez bouge.
		ScanTiles(Npc, FloorInt(Npc.X), FloorInt(Npc.Y), bForce);
	}

	AnastasisNous::FFoodContext FVillage::PerceiveFoodContext(const FNpc& Npc) const
	{
		AnastasisNous::FFoodContext C;
		C.Hunger = Npc.Needs.Hunger;
		C.InventoryFood = Npc.InventoryFood;
		C.BelievedFood = 0.0; // `believedStock` lit la croyance de marche : pas de marche porte.
		C.Gold = 0;
		C.bDangerNear = false;

		// `listKnownFoodSources` : croyances seulement, jamais le stock vrai.
		struct FKnown
		{
			const FStockBelief* Entry;
			double Distance;
			int32 AgeDays;
			double Rank;
		};
		TArray<FKnown> Known;
		const int32 Today = Day();
		for (const FStockBelief& E : Npc.KnownStocks)
		{
			if (E.Resource != TEXT("food") || E.BuildingId.IsEmpty()) continue;
			const int32 Age = FMath::Max(0, Today - E.Day);
			const double D = JsHypot(E.X - Npc.X, E.Y - Npc.Y);
			const double Est = FMath::Max(0.0, E.EstimatedAmount);
			const double Rank = AnastasisNous::Clamp01(E.Confidence) * 100.0 + Est * 0.05 - D * 0.4 - Age * 2.0;
			Known.Add({ &E, D, Age, Rank });
		}
		Known.StableSort([](const FKnown& A, const FKnown& B) { return A.Rank > B.Rank; });

		const FMealReservation* Reservation = FindMealReservation(Npc.Id);
		C.BestSourceDistance = NAN;
		if (Known.Num() > 0)
		{
			const FKnown& Best = Known[0];
			C.BestSourceBuildingId = Best.Entry->BuildingId;
			C.BestSourceDistance = Best.Distance;
			C.BestSourceEstimated = FMath::Max(0.0, Best.Entry->EstimatedAmount);
			C.BestSourceConfidence = AnastasisNous::Clamp01(Best.Entry->Confidence);
		}
		if (C.InventoryFood > 0) C.Certainty = 1.0;
		else if (Reservation) C.Certainty = 0.95;
		else if (Known.Num() > 0) C.Certainty = AnastasisNous::Clamp01(C.BestSourceConfidence * (1.0 - FMath::Min(0.5, Known[0].AgeDays * 0.08)));
		else C.Certainty = 0.08; // ni croyance de marche, ni gisement : non portes
		return C;
	}

	// --- Reservations (ai/algorithmic/mealReservation.js) ------------------------

	FMealReservation* FVillage::GetMealReservation(const FNpc& Npc)
	{
		return MealReservations.FindByPredicate([&](const FMealReservation& R) { return R.NpcId == Npc.Id; });
	}

	bool FVillage::ReserveMeal(FNpc& Npc, const FString& BuildingId, double TravelSeconds, double TtlSeconds, FString& OutReason)
	{
		if (FMealReservation* Existing = GetMealReservation(Npc))
		{
			if (Existing->ExpiresAt > Now)
			{
				if (BuildingId.IsEmpty() || Existing->BuildingId == BuildingId || Existing->Source == TEXT("inventory"))
				{
					OutReason = TEXT("already_reserved");
					return true;
				}
			}
			ReleaseMeal(Npc, TEXT("stale_replace"));
		}

		if (Npc.InventoryFood > 0)
		{
			const double Ttl = ComputeMealTtlSeconds(0.0, 20.0);
			FMealReservation R;
			R.Id = FString::Printf(TEXT("meal_%d"), ++MealSeq);
			R.NpcId = Npc.Id;
			R.Source = TEXT("inventory");
			R.CreatedAt = Now;
			R.ExpiresAt = Now + Ttl;
			R.AbsoluteExpiresAt = Now + Meal::AbsoluteMaxSeconds;
			R.LastProgressAt = Now;
			MealReservations.Add(R);
			OutReason = TEXT("inventory");
			return true;
		}

		FBuilding* Source = BuildingId.IsEmpty() ? nullptr : Buildings.FindById(BuildingId);
		if (!Source)
		{
			OutReason = TEXT("no_known_source");
			return false;
		}
		// `reserveStock(source, "food", 1)` : seulement sur le disponible.
		if (Source->FoodAvailable() < 1)
		{
			OutReason = TEXT("unavailable");
			return false;
		}
		Source->FoodReserved += 1;

		const double Ttl = FMath::IsNaN(TtlSeconds) ? ComputeMealTtlSeconds(TravelSeconds) : FMath::Max(5.0, TtlSeconds);
		FPoint Access;
		const FString SourceId = Source->Id;
		const double Dist0 = BuildingAccessPointById(SourceId, &Npc, Access) ? JsHypot(Npc.X - Access.X, Npc.Y - Access.Y) : 0.0;

		FMealReservation R;
		R.Id = FString::Printf(TEXT("meal_%d"), ++MealSeq);
		R.NpcId = Npc.Id;
		R.BuildingId = SourceId;
		R.Source = TEXT("colony");
		R.Amount = 1;
		R.CreatedAt = Now;
		R.ExpiresAt = Now + Ttl;
		R.AbsoluteExpiresAt = Now + Meal::AbsoluteMaxSeconds;
		R.LastProgressAt = Now;
		R.LastDistance = Dist0;
		MealReservations.Add(R);
		OutReason = TEXT("colony_reserved");
		return true;
	}

	bool FVillage::ReleaseMeal(FNpc& Npc, const FString& /*Reason*/)
	{
		const int32 Index = MealReservations.IndexOfByPredicate([&](const FMealReservation& R) { return R.NpcId == Npc.Id; });
		if (Index == INDEX_NONE) return false;
		const FMealReservation& R = MealReservations[Index];
		if (R.Source == TEXT("colony") && !R.BuildingId.IsEmpty())
		{
			if (FBuilding* B = Buildings.FindById(R.BuildingId))
			{
				// `releaseStock` : jamais plus que le reserve.
				B->FoodReserved -= FMath::Min(B->FoodReserved, R.Amount);
			}
		}
		MealReservations.RemoveAt(Index);
		return true;
	}

	bool FVillage::ConfirmMeal(FNpc& Npc, FString& OutReason, FString& OutSourceId)
	{
		FMealReservation* R = GetMealReservation(Npc);
		if (!R)
		{
			OutReason = TEXT("no_reservation");
			return false;
		}
		if (R->Source == TEXT("inventory"))
		{
			ReleaseMeal(Npc, TEXT("confirmed_inventory"));
			OutReason = TEXT("inventory");
			OutSourceId = TEXT("inventory");
			return true;
		}
		FBuilding* B = Buildings.FindById(R->BuildingId);
		if (!B)
		{
			ReleaseMeal(Npc, TEXT("source_gone"));
			OutReason = TEXT("source_gone");
			return false;
		}
		// `takeReserved` : debite physique ET reserve ensemble.
		const int32 Taken = FMath::Min3(B->FoodReserved, B->FoodPhysical, R->Amount);
		if (Taken <= 0)
		{
			ReleaseMeal(Npc, TEXT("take_failed"));
			OutReason = TEXT("take_failed");
			return false;
		}
		B->FoodReserved -= Taken;
		B->FoodPhysical -= Taken;
		Npc.InventoryFood += Taken;
		OutSourceId = R->BuildingId;
		OutReason = TEXT("colony_taken");
		MealReservations.RemoveAll([&](const FMealReservation& E) { return E.NpcId == Npc.Id; });
		return true;
	}

	FString FVillage::MaybeRenewMealReservation(FNpc& Npc)
	{
		FMealReservation* R = GetMealReservation(Npc);
		if (!R) return TEXT("none");
		if (R->Source != TEXT("colony") || R->BuildingId.IsEmpty()) return TEXT("inventory");
		if (R->AbsoluteExpiresAt <= Now)
		{
			ReleaseMeal(Npc, TEXT("absolute_timeout"));
			return TEXT("absolute_timeout");
		}
		if (!Buildings.FindById(R->BuildingId))
		{
			ReleaseMeal(Npc, TEXT("source_gone"));
			return TEXT("source_gone");
		}
		FPoint Access;
		const FString BuildingId = R->BuildingId;
		const bool bAccess = BuildingAccessPointById(BuildingId, &Npc, Access);
		R = GetMealReservation(Npc); // BuildingAccessPoint ne touche pas le registre, mais restons prudents
		if (!R) return TEXT("none");
		const double Distance = bAccess ? JsHypot(Npc.X - Access.X, Npc.Y - Access.Y) : R->LastDistance;
		const double Last = FMath::IsFinite(R->LastDistance) ? R->LastDistance : Distance;
		const bool bProgressed = Distance < Last - 0.15;
		if (bProgressed)
		{
			R->LastDistance = Distance;
			R->LastProgressAt = Now;
			if (R->ExpiresAt - Now < 25.0 && R->Renewals < Meal::MaxRenewals)
			{
				++R->Renewals;
				R->ExpiresAt = FMath::Min(R->AbsoluteExpiresAt, Now + Meal::RenewalSeconds);
				return TEXT("progress");
			}
			return TEXT("progress_no_need");
		}
		const double Since = R->LastProgressAt != 0.0 ? R->LastProgressAt : R->CreatedAt;
		if (Now - Since > Meal::ProgressStallSeconds) return TEXT("stalled");
		return TEXT("no_progress");
	}

	int32 FVillage::ExpireMealReservations()
	{
		int32 Expired = 0;
		TArray<FString> Ids;
		for (const FMealReservation& R : MealReservations) Ids.Add(R.Id);
		for (const FString& Id : Ids)
		{
			FMealReservation* R = MealReservations.FindByPredicate([&](const FMealReservation& E) { return E.Id == Id; });
			if (!R) continue;
			const double HardAt = R->AbsoluteExpiresAt != 0.0 ? R->AbsoluteExpiresAt : R->ExpiresAt;
			const bool bHard = HardAt <= Now;
			const bool bSoft = R->ExpiresAt <= Now;
			if (!bHard && !bSoft) continue;
			if (FNpc* Npc = Actors.FindById(R->NpcId))
			{
				if (!bHard && bSoft && MaybeRenewMealReservation(*Npc) == TEXT("progress"))
				{
					continue;
				}
				ReleaseMeal(*Npc, bHard ? TEXT("absolute_timeout") : TEXT("expired"));
			}
			else
			{
				if (R->Source == TEXT("colony"))
				{
					if (FBuilding* B = Buildings.FindById(R->BuildingId))
					{
						B->FoodReserved -= FMath::Min(B->FoodReserved, R->Amount);
					}
				}
				MealReservations.RemoveAll([&](const FMealReservation& E) { return E.Id == Id; });
			}
			++Expired;
		}
		return Expired;
	}

	bool FVillage::MealSourceAccessPoint(FNpc& Npc, FPoint& Out, bool& bOutBuilding)
	{
		bOutBuilding = false;
		const FMealReservation* R = GetMealReservation(Npc);
		if (!R) return false;
		if (R->Source == TEXT("inventory"))
		{
			Out = { Npc.X, Npc.Y };
			return true;
		}
		const FString BuildingId = R->BuildingId;
		const FBuilding* B = Buildings.FindById(BuildingId);
		if (!B) return false;
		bOutBuilding = true;
		if (BuildingAccessPointById(BuildingId, &Npc, Out)) return true;
		Out = { B->X + 0.5, B->Y + 0.5 };
		return true;
	}

	bool FVillage::IsNpcAtMealSource(FNpc& Npc)
	{
		const FMealReservation* R = GetMealReservation(Npc);
		if (!R) return false;
		if (R->Source == TEXT("inventory")) return Npc.InventoryFood > 0;
		if (Npc.Inside.bActive && Npc.Inside.BuildingId == R->BuildingId) return true;
		FPoint Access;
		bool bBuilding = false;
		if (!MealSourceAccessPoint(Npc, Access, bBuilding)) return false;
		return JsHypot(Npc.X - Access.X, Npc.Y - Access.Y) <= Meal::InteractionRadius;
	}

	// --- Action faim (ai/algorithmic/hungerAction.js) ----------------------------

	void FVillage::FailHungerAction(FNpc& Npc, const FString& Reason, const FString& ExcludedType)
	{
		Npc.bHasHungerAction = true;
		ReleaseMeal(Npc, Reason);
		FHungerAction& A = Npc.HungerAction;
		A.State = TEXT("failed");
		A.LastFailure = Reason;
		A.CooldownUntil = Now + AnastasisNous::FailureCooldownSeconds;
		A.ExcludedType = ExcludedType;
		A.Progress = 0.0;
	}

	void FVillage::CancelHungerAction(FNpc& Npc, const FString& Reason)
	{
		Npc.bHasHungerAction = true;
		FHungerAction& A = Npc.HungerAction;
		if (A.State != TEXT("completed") && A.State != TEXT("failed") && A.State != TEXT("cancelled"))
		{
			ReleaseMeal(Npc, Reason);
		}
		A.State = TEXT("cancelled");
		A.LastFailure = Reason;
		A.CooldownUntil = Now + AnastasisNous::FailureCooldownSeconds * 0.5;
		A.ExcludedType = TEXT("seek_food");
	}

	int32 FVillage::RunHungerActionStep(FNpc& Npc, bool bAtFoodAccess, const FString& SourceBuildingId)
	{
		Npc.bHasHungerAction = true;
		FHungerAction& A = Npc.HungerAction;
		if (A.CooldownUntil > Now && A.State == TEXT("failed"))
		{
			return 0; // "cooldown"
		}

		if (A.State == TEXT("pending") || A.State == TEXT("completed") || A.State == TEXT("cancelled") || A.State == TEXT("failed"))
		{
			const bool bWasFailed = A.State == TEXT("failed");
			A.State = TEXT("reserving");
			A.StartedAt = Now;
			A.Progress = 0.0;
			if (!bWasFailed) A.LastFailure.Reset();
			A.SourceBuildingId = !SourceBuildingId.IsEmpty() ? SourceBuildingId : A.SourceBuildingId;
		}

		if (A.State == TEXT("reserving"))
		{
			const FString BuildingId = !SourceBuildingId.IsEmpty() ? SourceBuildingId : A.SourceBuildingId;
			FString Reason;
			const bool bOk = ReserveMeal(Npc, BuildingId, NAN, ComputeMealTtlSeconds(NAN), Reason);
			if (!bOk && Npc.InventoryFood <= 0)
			{
				FailHungerAction(Npc, Reason.IsEmpty() ? TEXT("reserve_failed") : Reason, TEXT("seek_food"));
				return 0;
			}
			const FMealReservation* R = GetMealReservation(Npc);
			A.State = TEXT("navigating");
			A.TargetId = R ? (!R->BuildingId.IsEmpty() ? R->BuildingId : R->Source) : FString(TEXT("inventory"));
			A.ReservationId = R ? R->Id : FString();
			A.SourceBuildingId = R ? R->BuildingId : FString();
			A.Progress = 0.1;
		}

		if (A.State == TEXT("navigating"))
		{
			MaybeRenewMealReservation(Npc);
			const FMealReservation* R = GetMealReservation(Npc);
			if (!R && Npc.InventoryFood <= 0)
			{
				FailHungerAction(Npc, TEXT("reservation_lost"), TEXT("seek_food"));
				return 0;
			}
			const bool bArrived = bAtFoodAccess || IsNpcAtMealSource(Npc);
			R = GetMealReservation(Npc);
			if (Npc.InventoryFood > 0 && R && R->Source == TEXT("inventory"))
			{
				A.State = TEXT("interacting");
			}
			else if (bArrived)
			{
				A.State = TEXT("interacting");
				A.Progress = 0.85;
			}
			else
			{
				A.Progress = FMath::Min(0.8, (A.Progress != 0.0 ? A.Progress : 0.1) + 0.04);
				return -1; // en route
			}
		}

		if (A.State == TEXT("interacting") || A.State == TEXT("consuming"))
		{
			// Garde physique : pas de consommation a distance.
			const FMealReservation* R = GetMealReservation(Npc);
			if (R && R->Source == TEXT("colony"))
			{
				const FString ReservedBuilding = R->BuildingId;
				if (!IsNpcAtMealSource(Npc) && !bAtFoodAccess)
				{
					A.State = TEXT("navigating");
					return -1;
				}
				if (!A.SourceBuildingId.IsEmpty() && !ReservedBuilding.IsEmpty() && A.SourceBuildingId != ReservedBuilding)
				{
					FailHungerAction(Npc, TEXT("source_mismatch"), TEXT("seek_food"));
					return 0;
				}
			}

			A.State = TEXT("consuming");
			if (Npc.InventoryFood <= 0)
			{
				FString Reason;
				FString SourceId;
				if (!ConfirmMeal(Npc, Reason, SourceId))
				{
					FailHungerAction(Npc, Reason.IsEmpty() ? TEXT("confirm_failed") : Reason, TEXT("seek_food"));
					return 0;
				}
			}
			else if (GetMealReservation(Npc))
			{
				ReleaseMeal(Npc, TEXT("consume_inventory"));
			}

			if (Npc.InventoryFood <= 0)
			{
				FailHungerAction(Npc, TEXT("empty_after_confirm"), TEXT("seek_food"));
				return 0;
			}

			Npc.InventoryFood -= 1;
			const FString& Living = Npc.LivingHomeId();
			const bool bAtHome = !Living.IsEmpty() && Npc.Inside.bActive && Npc.Inside.BuildingId == Living;
			AnastasisNeeds::SatisfyEat(Npc.Needs, Npc.Inside.bActive, bAtHome);
			// `recordAteFood` : memoire d'episode, non portee.

			A.State = TEXT("completed");
			A.Progress = 1.0;
			A.LastFailure.Reset();
			A.ExcludedType.Reset();
			A.ReservationId.Reset();
			return 1;
		}
		return 0; // "noop"
	}

	int32 FVillage::TryAlgorithmicEat(FNpc& Npc)
	{
		const bool bArrived = IsNpcAtMealSource(Npc);
		const FString Source = !Npc.HungerAction.SourceBuildingId.IsEmpty()
			? Npc.HungerAction.SourceBuildingId
			: (Npc.bHasAlgoDecision ? AnastasisNous::DecisionSourceBuildingId(Npc.AlgoDecision) : FString());
		return RunHungerActionStep(Npc, bArrived, Source);
	}

	bool FVillage::Eat(FNpc& Npc)
	{
		// Noûs : reservation -> arrivee physique -> confirmation -> consommation.
		IsNpcAtMealSource(Npc); // `atAccess`, calcule une premiere fois par la reference
		const int32 Result = TryAlgorithmicEat(Npc);
		if (Result == 1)
		{
			++Npc.MealsTaken;
			return true;
		}
		if (Result == 0)
		{
			// `begForFood` et `noteGoalFailure` : non portes. La pensee est avancee.
			Npc.AiThinkAt = Now;
			return false;
		}
		// `null` : encore en route — ne pas consommer a distance.
		SetActivity(Npc, TEXT("marche"));
		return true;
	}

	// --- Pont Noûs (ai/algorithmic/runtime.js, bridge.js) ------------------------

	void FVillage::ComputeAlgorithmicDecision(FNpc& Npc)
	{
		// `cooldownExcludeTypes` -> `ensureHungerAction(npc)` : la cle existe des la premiere decision.
		// Le paquet `_algoDebug` (decision, candidats, contexte) n'est pas tenu : ecart n°35.
		Npc.bHasHungerAction = true;
		TArray<FString> Exclude;
		const FHungerAction& A = Npc.HungerAction;
		if (A.CooldownUntil > Now && !A.ExcludedType.IsEmpty())
		{
			Exclude.Add(A.ExcludedType);
		}
		const AnastasisNous::FFoodContext Context = PerceiveFoodContext(Npc);
		AnastasisNous::FHungerSubject Subject;
		Subject.Energy = Npc.Needs.Energy;
		Subject.Speed = Npc.Speed;
		Subject.HomeId = Npc.HomeId;
		const AnastasisNous::FScored Scored = AnastasisNous::ScoreHungerCandidates(Context, Subject, Now, Exclude);

		bool bHasChosen = Scored.bHasBest;
		AnastasisNous::FDecision Chosen = Scored.Best;
		bool bKeep = false;
		FString Reason = TEXT("disabled");
		if (Npc.bHasAlgoDecision && bHasChosen)
		{
			const AnastasisNous::FDecision Current = Npc.AlgoDecision;
			const double Since = Current.CreatedAt != 0.0 ? Current.CreatedAt : Npc.GoalSince;
			const double Elapsed = Now - Since;
			const double CooldownLeft = FMath::Max(0.0, A.CooldownUntil - Now);
			if (Exclude.Contains(Current.Type) && Chosen.Type != Current.Type)
			{
				bKeep = false;
				Reason = TEXT("cooldown_forced_switch");
			}
			else
			{
				bKeep = AnastasisNous::EvaluateInertia(&Current, &Chosen, Elapsed, CooldownLeft, 0.0, Reason);
				if (bKeep && !Exclude.Contains(Current.Type))
				{
					Chosen = Current;
				}
			}
		}
		Npc.bHasAlgoDecision = bHasChosen;
		Npc.AlgoDecision = Chosen;
		Npc.AlgoContext = Context;
		Npc.bAlgoInertiaKeep = bKeep;
		Npc.AlgoInertiaReason = Reason;
		Npc.AlgoMappedGoal = bHasChosen ? AnastasisNous::DecisionToNpcGoal(Chosen) : FString();
	}

	void FVillage::ApplyAlgorithmicScoreBias(FNpc& Npc, TArray<TPair<FString, double>>& Rows)
	{
		if (!Npc.bHasAlgoDecision)
		{
			ComputeAlgorithmicDecision(Npc);
		}
		if (!Npc.bHasAlgoDecision) return;

		auto Bump = [&](const TCHAR* Goal, double Delta)
		{
			if (Delta == 0.0) return;
			for (TPair<FString, double>& Row : Rows)
			{
				if (Row.Key == Goal)
				{
					Row.Value += Delta;
					return;
				}
			}
		};

		const AnastasisNous::FDecision& D = Npc.AlgoDecision;
		const double Urgency = D.Urgency;
		const double Strength = D.Score * AnastasisNous::BridgeScoreScale + Urgency * AnastasisNous::BridgeUrgencyExtra;

		const FHungerAction& A = Npc.HungerAction;
		const bool bCooling = A.CooldownUntil > Now && A.State == TEXT("failed");
		if (bCooling)
		{
			Bump(TEXT("eat"), -AnastasisNous::BridgeFailureEatPenalty);
			Bump(TEXT("eatTogether"), -AnastasisNous::BridgeFailureEatPenalty * 0.7);
			Bump(TEXT("gatherFood"), AnastasisNous::BridgeFailureGatherBoost);
			Bump(TEXT("buy"), AnastasisNous::BridgeFailureGatherBoost * 0.5);
		}

		if (D.Type == TEXT("eat") || D.Type == TEXT("seek_food"))
		{
			const bool bReserved = FindMealReservation(Npc.Id) != nullptr;
			const bool bHasInv = Npc.AlgoContext.InventoryFood > 0 || Npc.InventoryFood > 0;
			const bool bKnownBuilding = !D.SourceBuildingId.IsEmpty() || !Npc.AlgoContext.BestSourceBuildingId.IsEmpty();
			if (bHasInv || bReserved || bKnownBuilding)
			{
				Bump(TEXT("eat"), Strength);
				Bump(TEXT("eatTogether"), Strength * 0.45);
			}
			else
			{
				Bump(TEXT("gatherFood"), Strength * 0.85 + AnastasisNous::BridgeSeekGatherBoost);
				Bump(TEXT("buy"), Strength * 0.55);
				Bump(TEXT("eat"), -AnastasisNous::BridgeWorkSuppressEat);
			}
			if (Urgency >= 0.55)
			{
				for (TPair<FString, double>& Row : Rows)
				{
					if (IsWorkish(Row.Key) && Row.Key != TEXT("gatherFood") && Row.Key != TEXT("buy"))
					{
						const double Delta = -Urgency * AnastasisNous::BridgeWorkSuppressEat;
						if (Delta != 0.0) Row.Value += Delta;
					}
				}
			}
		}
		else if (D.Type == TEXT("buy_food"))
		{
			Bump(TEXT("buy"), Strength);
			Bump(TEXT("eat"), Strength * 0.35);
		}
		else if (D.Type == TEXT("sleep"))
		{
			Bump(TEXT("rest"), Strength);
		}
		else if (D.Type == TEXT("work"))
		{
			if (IsWorkish(Npc.Goal))
			{
				Bump(*Npc.Goal, AnastasisNous::BridgeInertiaKeepBonus * (1.0 - Urgency));
			}
			if (Urgency < 0.45)
			{
				Bump(TEXT("eat"), -AnastasisNous::BridgeWorkSuppressEat * 0.6);
			}
		}

		if (Npc.bAlgoInertiaKeep && !Npc.Goal.IsEmpty())
		{
			// `observer` n'est pas une ligne : rien a pousser, comme un but absent de la table.
			Bump(*Npc.Goal, AnastasisNous::BridgeInertiaKeepBonus);
		}
		else if (!Npc.AlgoMappedGoal.IsEmpty() && AnastasisNous::IsUrgentInterrupt(Urgency))
		{
			Bump(*Npc.AlgoMappedGoal, Strength * 0.5);
		}
	}

	FString FVillage::ApplyAlgorithmicCommitGate(FNpc& Npc, const FString& Next, const FString& Previous, FString& OutGate)
	{
		if (!Npc.bHasAlgoDecision) return Next;
		const AnastasisNous::FDecision& D = Npc.AlgoDecision;
		const double Urgency = D.Urgency;
		const FString& Mapped = Npc.AlgoMappedGoal;

		if (Npc.bAlgoInertiaKeep && !Previous.IsEmpty() && Previous == Next)
		{
			return Next;
		}
		if (Npc.bAlgoInertiaKeep && !Previous.IsEmpty() && !AnastasisNous::IsUrgentInterrupt(Urgency))
		{
			if (Previous == TEXT("eat") || Previous == TEXT("eatTogether") || IsWorkish(Previous))
			{
				const bool bCooling = Npc.HungerAction.CooldownUntil > Now;
				if (!(Previous.StartsWith(TEXT("eat")) && bCooling))
				{
					OutGate = FString::Printf(TEXT("%s->%s (%s)"), *Next, *Previous, *Npc.AlgoInertiaReason);
					return Previous;
				}
			}
		}
		if (D.Type == TEXT("flee")) return Next;
		if (Mapped.IsEmpty() || !AnastasisNous::IsUrgentInterrupt(Urgency)) return Next;
		if (Mapped == Next) return Next;
		if ((D.Type == TEXT("eat") || D.Type == TEXT("seek_food")) && Mapped == TEXT("eat"))
		{
			if (Npc.Needs.Hunger >= AnastasisNeeds::Constants::HungerCritical || Urgency >= AnastasisNous::UrgencyInterruptAt)
			{
				OutGate = FString::Printf(TEXT("%s->eat (urgency_hunger)"), *Next);
				return TEXT("eat");
			}
		}
		if (D.Type == TEXT("sleep") && Mapped == TEXT("rest") && Urgency >= 0.85)
		{
			OutGate = FString::Printf(TEXT("%s->rest (urgency_fatigue)"), *Next);
			return TEXT("rest");
		}
		return Next;
	}

	void FVillage::OnAlgorithmicGoalCommitted(FNpc& Npc, const FString& Previous, const FString& Next)
	{
		const bool bEatGoals = Next == TEXT("eat") || Next == TEXT("eatTogether");
		const bool bWasEat = Previous == TEXT("eat") || Previous == TEXT("eatTogether");
		if (bEatGoals)
		{
			// ecart n°39 : le fournisseur humain choisit le but, pas la source du repas. Il n'a pas de decision
			// Noûs dont lire les metadonnees ; la reservation lit donc ses propres croyances,
			// comme le contexte alimentaire fourni au decideur autonome.
			const bool bPlayer = IsPlayer(Npc);
			const AnastasisNous::FDecision* D = !bPlayer && Npc.bHasAlgoDecision ? &Npc.AlgoDecision : nullptr;
			const FString SourceBuildingId = bPlayer ? PerceiveFoodContext(Npc).BestSourceBuildingId
				: D ? AnastasisNous::DecisionSourceBuildingId(*D) : FString();
			// `metadata.travelSeconds ?? expectedDuration ?? null` : le trajet du candidat
			// (seek_food : distance / vitesse ; eat : 0), sinon sa duree attendue.
			double Travel = NAN;
			if (D)
			{
				const bool bHasTravelMeta = D->Type == TEXT("eat") || D->Type == TEXT("seek_food");
				Travel = bHasTravelMeta && !FMath::IsNaN(D->TravelSeconds) ? D->TravelSeconds : D->ExpectedDuration;
			}
			FString Reason;
			const bool bOk = Npc.InventoryFood > 0
				? ReserveMeal(Npc, FString(), 0.0, NAN, Reason)
				: ReserveMeal(Npc, SourceBuildingId, Travel, ComputeMealTtlSeconds(Travel), Reason);
			FHungerAction& A = Npc.HungerAction;
			if (bOk)
			{
				const FMealReservation* R = GetMealReservation(Npc);
				A.State = TEXT("navigating");
				A.TargetId = R ? (!R->BuildingId.IsEmpty() ? R->BuildingId : R->Source) : FString(TEXT("inventory"));
				A.StartedAt = Now;
				A.Progress = 0.05;
				A.ReservationId = R ? R->Id : FString();
				A.SourceBuildingId = R ? R->BuildingId : FString();
				A.LastFailure.Reset();
			}
			else
			{
				FailHungerAction(Npc, Reason.IsEmpty() ? TEXT("reserve_failed") : Reason, TEXT("seek_food"));
				A.StartedAt = Now;
				A.Progress = 0.0;
				A.ReservationId.Reset();
				A.SourceBuildingId = SourceBuildingId;
			}
		}
		else if (bWasEat)
		{
			ReleaseMeal(Npc, TEXT("goal_changed"));
			CancelHungerAction(Npc, TEXT("goal_changed"));
		}
	}

	void FVillage::TickAlgorithmicNpc()
	{
		// Renouvellement pour les habitants en trajet de repas.
		for (FNpc& Npc : Actors.GetItemsMutable())
		{
			if (Npc.Goal == GoalEat)
			{
				MaybeRenewMealReservation(Npc);
			}
		}
		if (Now - ReservationSweepAt >= AnastasisNous::ReservationSweepSeconds)
		{
			ReservationSweepAt = Now;
			ExpireMealReservations();
		}
	}

	// --- Cible du repas --------------------------------------------------------------

	const FBuilding* FVillage::MealPlace(const FNpc& Npc) const
	{
		if (!Npc.HomeId.IsEmpty())
		{
			if (const FBuilding* Home = Buildings.FindById(Npc.HomeId)) return Home;
		}
		if (!Npc.ShelterId.IsEmpty())
		{
			if (const FBuilding* Shelter = Buildings.FindById(Npc.ShelterId)) return Shelter;
		}
		// Taverne, marche, groupe « food » ou « trade » : le grenier seul existe ici.
		const FBuilding* Best = nullptr;
		double BestDist = AnastasisNav::Infinity;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Progress < 1.0 || !IsFoodGroupType(B.Type)) continue;
			if (!Best) Best = &B;
			const double DX = (B.X + 0.5) - Npc.X;
			const double DY = (B.Y + 0.5) - Npc.Y;
			const double D = DX * DX + DY * DY;
			if (D < BestDist)
			{
				BestDist = D;
				Best = &B;
			}
		}
		return Best;
	}

	bool FVillage::EatTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource)
	{
		bool bHave = false;
		// Base (`assignTarget`, branche eat) : la source reservee.
		FPoint Meal;
		bool bBuilding = false;
		if (MealSourceAccessPoint(Npc, Meal, bBuilding) && bBuilding)
		{
			OutTarget = Meal;
			OutSource = TEXT("reserved");
			bHave = true;
		}
		else if (Npc.InventoryFood > 0 && !Npc.HomeId.IsEmpty() && BuildingAccessPointById(Npc.HomeId, &Npc, OutTarget))
		{
			OutSource = TEXT("home-food");
			bHave = true;
		}
		// `marketAccessPoint` : sans marche, `accessPointNear(plannedMarketPos)` ; le site
		// du marche n'est pas porte : le camp.
		else if (AccessPointNear(Settlement.X, Settlement.Y, &Npc, OutTarget))
		{
			OutSource = TEXT("market-site");
			bHave = true;
		}

		// Couche rythme (`rhythmTarget`, branche eat) : `mealPlace`.
		if (const FBuilding* Place = MealPlace(Npc))
		{
			FPoint Access;
			if (BuildingAccessPointById(Place->Id, &Npc, Access))
			{
				OutTarget = Access;
				OutSource = TEXT("meal-place");
				bHave = true;
			}
		}

		// Couche domestique : le foyer, sinon un abri ouvert — AVANT le grenier.
		const FString Living = Npc.LivingHomeId();
		FPoint Domestic;
		if (!Living.IsEmpty())
		{
			if (BuildingAccessPointById(Living, &Npc, Domestic))
			{
				OutTarget = Domestic;
				OutSource = Living == Npc.HomeId ? TEXT("home") : TEXT("shelter");
				bHave = true;
			}
		}
		else if (const FBuilding* Open = FindOpenShelter(Npc))
		{
			if (BuildingAccessPointById(Open->Id, &Npc, Domestic))
			{
				OutTarget = Domestic;
				OutSource = TEXT("open-shelter");
				bHave = true;
			}
		}
		if (!bHave)
		{
			OutSource = TEXT("none");
		}
		return bHave;
	}

	// --- Recolte et livraison (npc.js, craftWork.js, fieldWorkPosts.js, memory.js) ---

	namespace
	{
		int32 TileIndexOf(const AnastasisWorld::FWorld* World, int32 X, int32 Y)
		{
			if (!World || X < 0 || Y < 0 || X >= World->W || Y >= World->H) return INDEX_NONE;
			return Y * World->W + X;
		}

		const TCHAR* ResourceName(AnastasisWorld::EResource Resource)
		{
			switch (Resource)
			{
			case AnastasisWorld::EResource::Stone: return TEXT("stone");
			case AnastasisWorld::EResource::Wood: return TEXT("wood");
			case AnastasisWorld::EResource::Food: return TEXT("food");
			default: return TEXT("");
			}
		}

		/** `trimMemory` : au-dela de 26 gisements, on lache un souvenir de la ressource la plus connue. */
		void TrimSpots(TArray<FResourceSpot>& Spots)
		{
			if (Spots.Num() <= AnastasisGather::SpotCapacity) return;
			// `counts` : un objet JS, cles dans l'ordre de premiere apparition.
			TArray<TPair<FString, int32>> Counts;
			for (const FResourceSpot& Spot : Spots)
			{
				TPair<FString, int32>* Entry = Counts.FindByPredicate([&](const TPair<FString, int32>& C) { return C.Key == Spot.Resource; });
				if (Entry) ++Entry->Value;
				else Counts.Add(TPair<FString, int32>(Spot.Resource, 1));
			}
			int32 Dominant = 0;
			for (int32 I = 1; I < Counts.Num(); ++I)
			{
				if (Counts[I].Value > Counts[Dominant].Value) Dominant = I;
			}
			int32 Worst = 0;
			double WorstRank = AnastasisNav::Infinity;
			for (int32 I = 0; I < Spots.Num(); ++I)
			{
				if (Spots[I].Resource != Counts[Dominant].Key) continue;
				// On lache les on-dit en premier.
				const double Rank = Spots[I].Day - (Spots[I].bHearsay ? 2 : 0);
				if (Rank < WorstRank)
				{
					WorstRank = Rank;
					Worst = I;
				}
			}
			Spots.RemoveAt(Worst);
		}
	}

	double FVillage::WorkRowScore(const FNpc& Npc, const FString& Goal, double PhaseBias, const FWorkRowContext& Work, double Noise) const
	{
		namespace G = AnastasisGather;
		const G::FTrait& Trait = G::TraitAt(Npc.TraitIndex);
		// `inventoryLoad` : la nourriture est la seule charge portee ; le grenier la prend (depotResourceOf).
		const int32 Load = Npc.InventoryFood;
		const int32 DepotLoad = Npc.InventoryFood;
		const FString SessionGoal = SessionGoalOf(Npc);
		double Score = 0.0;
		double DomainSkill = 1.0;
		if (Goal == GoalGatherFood)
		{
			// `(resourceScore + npc.hunger * 0.15 + goalNoise(sim, 14)) * wf("gatherFood")` : le bruit est tire par ChooseGoal, a sa place dans la table.
			const double Base = G::ResourceScoreFood(Actors.Num(), Work.Believed, Npc.JobId, Trait.Gather, Npc.InventoryFood, Npc.Needs.Hunger)
				+ Npc.Needs.Hunger * 0.15 + Noise;
			Score = Base * G::SurvivalWorkFactor(Goal, Work.WorkFactor, Work.bMealBlocked);
			DomainSkill = Npc.SkillGather;
		}
		else
		{
			// `deliveryScore(sim, npc, s) * wf("deliver")`.
			Score = G::DeliveryScoreDepot(Npc.JobId, DepotLoad) * G::SurvivalWorkFactor(Goal, Work.WorkFactor, Work.bMealBlocked);
			DomainSkill = Npc.SkillTrade;
		}
		// Les biais, dans l'ordre d'adultScores. Nuls ici et omis : statut, mode de vie,
		// age, district, foyer, episodes, scenes, humeur, plans, meteo, nature (moyenne),
		// memoire sociale et d'echec, ordres, prevision, risque, urgence collective.
		Score += PhaseBias;
		Score += G::GranaryWorkplaceGoalBias(Npc.JobId, Goal, Npc.InventoryFood);
		Score += G::CompletionBias(Goal, Load, DepotLoad, SessionGoal, NeedsCritical(Npc.Needs));
		Score += G::TraitGoalBias(Trait, Goal);
		Score += G::SkillGoalBias(DomainSkill);
		return Score;
	}

	void FVillage::ClearWorkSession(FNpc& Npc)
	{
		Npc.WorkSession = FWorkSession();
	}

	void FVillage::ScanTiles(FNpc& Npc, int32 CX, int32 CY, bool bForce)
	{
		if (!World) return;
		// Le balayage des tuiles ne vaut que si l'habitant a bouge.
		const double Moved = FMath::Abs(Npc.X - Npc.ScanX) + FMath::Abs(Npc.Y - Npc.ScanY);
		if (!bForce && Moved < AnastasisGather::ScanMove) return;
		Npc.ScanX = Npc.X;
		Npc.ScanY = Npc.Y;

		const int32 R = static_cast<int32>(PerceptionRadius);
		const int32 MinX = FMath::Max(0, CX - R);
		const int32 MaxX = FMath::Min(World->W - 1, CX + R);
		const int32 MinY = FMath::Max(0, CY - R);
		const int32 MaxY = FMath::Min(World->H - 1, CY + R);
		const int32 Today = Day();
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				const AnastasisWorld::FTile Tile = LiveTile(Y * World->W + X);
				if (Tile.Resource == AnastasisWorld::EResource::None || Tile.Amount <= 0) continue;
				// `rememberSpot` : deja en tete -> mis a jour ; sinon ajoute, puis `trimMemory`.
				const FString Key = FString::Printf(TEXT("%d,%d"), X, Y);
				if (FResourceSpot* Known = Npc.Spots.FindByPredicate([&](const FResourceSpot& S) { return S.Key == Key; }))
				{
					Known->Amount = Tile.Amount;
					Known->Day = Today;
					Known->bHearsay = false;
					continue;
				}
				FResourceSpot Spot;
				Spot.Key = Key;
				Spot.X = X + 0.5;
				Spot.Y = Y + 0.5;
				Spot.Resource = ResourceName(Tile.Resource);
				Spot.Amount = Tile.Amount;
				Spot.Day = Today;
				Npc.Spots.Add(MoveTemp(Spot));
				TrimSpots(Npc.Spots);
			}
		}
		// La berge (croyance d'eau) n'est pas portee : la soif lit la berge du monde.
		// `forgetEmptied` : ce qu'il voit epuise, il l'efface.
		Npc.Spots.RemoveAll([&](const FResourceSpot& Spot)
		{
			const int32 X = FloorInt(Spot.X);
			const int32 Y = FloorInt(Spot.Y);
			if (X < MinX || X > MaxX || Y < MinY || Y > MaxY) return false;
			const AnastasisWorld::FTile Tile = LiveTile(Y * World->W + X);
			return Spot.Resource != ResourceName(Tile.Resource) || Tile.Amount <= 0;
		});
	}

	bool FVillage::GatherTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource)
	{
		// `recallOrSearch(sim, npc, "food")` : la doctrine de lisiere ne vise que le bois ;
		// le grenier n'est pas un poste d'extraction (pas de cour) -> `recallResource`.
		const FResourceSpot* Best = nullptr;
		double BestScore = -AnastasisNav::Infinity;
		const int32 Today = Day();
		for (const FResourceSpot& Spot : Npc.Spots)
		{
			if (Spot.Resource != TEXT("food")) continue;
			const int32 Age = Today - Spot.Day;
			// `dangerPenaltyAt` : pas de zone de danger connue, 0.
			const double Score = -Dist(Npc.X, Npc.Y, Spot.X, Spot.Y) - Age * 1.5
				- (Spot.bHearsay ? AnastasisGather::HearsayPenalty : 0.0) - 0.0;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = &Spot;
			}
		}
		if (!Best)
		{
			// `exploreTarget` tire `sim.rng` : non porte (ecart n°11).
			OutSource = TEXT("none");
			return false;
		}
		OutTarget = { Best->X, Best->Y };
		OutSource = TEXT("spot");
		return true;
	}

	bool FVillage::DeliverTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource)
	{
		// Sac du depot perso : retourner au poste (pas le marche).
		FBuilding* Workplace = Npc.WorkplaceId.IsEmpty() ? nullptr : Buildings.FindById(Npc.WorkplaceId);
		if (Workplace && Npc.InventoryFood > 0 && Workplace->Progress >= 1.0 && BuildingAccessPoint(*Workplace, &Npc, OutTarget))
		{
			OutSource = TEXT("workplace");
			return true;
		}
		// `sim.deliveryPos(npc)` : marche, entrepot, camp — non portes (ecart n°12).
		OutSource = TEXT("none");
		return false;
	}

	AnastasisWorld::FTile FVillage::LiveTile(int32 Index) const
	{
		AnastasisWorld::FTile Tile = LiveTiles.Contains(Index) ? LiveTiles[Index] : World->Tiles[Index];
		// Une tuile ouverte par l'extension food-supply : son registre fini fait foi.
		if (const FFoodSource* Source = FoodSources.FindByPredicate([&](const FFoodSource& S) { return S.TileIndex == Index; }))
		{
			Tile.Amount = Source->Remaining;
		}
		return Tile;
	}

	AnastasisWorld::FTile FVillage::LiveTileAt(int32 TileX, int32 TileY) const
	{
		const int32 Index = TileIndexOf(World, TileX, TileY);
		return Index == INDEX_NONE ? AnastasisWorld::FTile() : LiveTile(Index);
	}

	void FVillage::TakeFromTile(int32 Index, int32 Taken)
	{
		if (FFoodSource* Source = FoodSources.FindByPredicate([&](const FFoodSource& S) { return S.TileIndex == Index; }))
		{
			Source->Remaining -= Taken;
		}
		AnastasisWorld::FTile& Live = LiveTiles.FindOrAdd(Index, World->Tiles[Index]);
		Live.Amount -= Taken;
	}

	int32 FVillage::ResourceTileNear(const FNpc& Npc, AnastasisWorld::EResource Resource) const
	{
		const int32 CX = FloorInt(Npc.X);
		const int32 CY = FloorInt(Npc.Y);
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				const int32 Index = TileIndexOf(World, CX + DX, CY + DY);
				if (Index == INDEX_NONE) continue;
				const AnastasisWorld::FTile Tile = LiveTile(Index);
				if (Tile.Resource != Resource || !(Tile.Amount > 0)) continue;
				return Index;
			}
		}
		return INDEX_NONE;
	}

	void FVillage::EnsureCraftSession(FNpc& Npc, int32 TileX, int32 TileY, const TCHAR* CraftId)
	{
		const FWorkSession& Existing = Npc.WorkSession;
		if (Existing.bActive && Existing.CraftId == CraftId && Existing.TileX == TileX && Existing.TileY == TileY)
		{
			return;
		}
		// `craftToolSwitchSeconds(from, craftId)` : changer d'outil entre `farm` et `tend` (help-farm-001).
		const FString From = (Existing.bActive && Existing.CraftId != CraftId) ? Existing.CraftId : FString();
		const double SwitchUntil = Now + AnastasisBuild::CraftToolSwitchSeconds(From, CraftId);
		// `craftArriveSeconds(craftId)`.
		const double Arrive = FCString::Strcmp(CraftId, TEXT("tend")) == 0 ? AnastasisGather::TendArriveSeconds : AnastasisGather::FarmArriveSeconds;
		FWorkSession Session;
		Session.bActive = true;
		Session.CraftId = CraftId;
		Session.TileX = TileX;
		Session.TileY = TileY;
		Session.ArrivedAt = Now;
		Session.NextSwingAt = SwitchUntil + Arrive;
		Npc.WorkSession = Session;
		// `noteShiftArrival(sim, npc)` : arrivee physique au travail, COMMUTING -> ON_SHIFT.
		AnastasisWorkShift::NoteShiftArrival(Npc.WorkShift);
	}

	uint32 FVillage::ClaimedFieldPosts(const FNpc& Npc, int32 TileX, int32 TileY) const
	{
		return ClaimedFieldPostsExcept(&Npc, TileX, TileY);
	}

	// --- Soins de parcelle (helpFarm, help-farm-001) ------------------------------------
	// npc.js `helpFarmScore`, `progressTendWork`, `assignTarget` ; simulation.js `findTendFieldNear`.

	int32 FVillage::FindTendFieldNear(double OriginX, double OriginY, const FNpc* Actor) const
	{
		if (!World) return INDEX_NONE;
		// `Simulation.FIELD_TEND_RADIUS` = 8.
		constexpr int32 Radius = 8;
		const int32 OX = FloorInt(OriginX);
		const int32 OY = FloorInt(OriginY);
		int32 Best = INDEX_NONE;
		double BestScore = AnastasisNav::Infinity;
		for (int32 DY = -Radius; DY <= Radius; ++DY)
		{
			for (int32 DX = -Radius; DX <= Radius; ++DX)
			{
				const int32 Index = TileIndexOf(World, OX + DX, OY + DY);
				if (Index == INDEX_NONE) continue;
				const AnastasisWorld::FTile Tile = LiveTile(Index);
				if (Tile.Type != AnastasisWorld::ETileType::Field) continue;
				const int32 Stock = Tile.Resource == AnastasisWorld::EResource::Food ? Tile.Amount : 0;
				if (Stock >= AnastasisFields::FieldFoodCap) continue;
				const int32 Need = AnastasisFields::FieldFoodCap - Stock;
				const int32 Dist2 = DX * DX + DY * DY;
				// `countFieldWorkers(sim, tile, actor)` : les postes tenus, l'habitant lui-meme exclu.
				const int32 Workers = FMath::CountBits(ClaimedFieldPostsExcept(Actor, Tile.X, Tile.Y));
				// « Penalite douce : etale les fermiers sur plusieurs parcelles. »
				const double Score = Dist2 - Need * 0.35 + Workers * 2.6;
				if (Score < BestScore)
				{
					Best = Index;
					BestScore = Score;
				}
			}
		}
		return Best;
	}

	int32 FVillage::FindTendFieldFor(const FNpc& Npc) const
	{
		// `origin.inventory ? origin : null` : depuis l'habitant, il s'exclut ; depuis son poste, personne.
		const int32 Near = FindTendFieldNear(Npc.X, Npc.Y, &Npc);
		if (Near != INDEX_NONE) return Near;
		const FBuilding* Workplace = Npc.WorkplaceId.IsEmpty() ? nullptr : Buildings.FindById(Npc.WorkplaceId);
		if (Workplace && Workplace->Progress >= 1.0) return FindTendFieldNear(Workplace->X, Workplace->Y, nullptr);
		return INDEX_NONE;
	}

	double FVillage::HelpFarmRowScore(const FNpc& Npc, double PhaseBias, const FWorkRowContext& Work, double Noise,
		const FCollectiveDecision& Collective) const
	{
		namespace G = AnastasisGather;
		double Line = 0.0;
		// `helpFarmScore` : sans ferme ni batiment `nourrir`, 0 (et la reference ne tire pas son bruit).
		if (NoiseConditionHolds(Npc, AnastasisGoalNoise::ENoiseCondition::HasFarm))
		{
			const bool bFoodJob = Npc.JobId == TEXT("farmer") || Npc.JobId == TEXT("herder")
				|| Npc.JobId == TEXT("fisherman") || Npc.JobId == TEXT("baker");
			double Score = bFoodJob ? 12.0 : 3.0;
			Score += FMath::Max(0.0, 55.0 - Work.Believed) * 0.1;
			// `jobPriority + planBias` : les ambitions ne sont pas portees (ecart n°24), `planBias` = 0.
			Score += G::JobPriority(Npc.JobId, GoalHelpFarm);
			Score += Collective.BiasOf(GoalHelpFarm);
			Score += Noise;
			// « Postes ferme vides : attirer des bras meme hors metier farmer. »
			if (Collective.FarmStaffingGap > 0)
			{
				Score += bFoodJob ? 10.0 + Collective.FarmStaffingGap * 6.0 : 8.0 + Collective.FarmStaffingGap * 4.0;
			}
			// « Parcelle faible proche : soigner avant de recolter a vide. »
			if (bFoodJob)
			{
				const int32 Plot = FindTendFieldFor(Npc);
				if (Plot != INDEX_NONE)
				{
					const AnastasisWorld::FTile Tile = LiveTile(Plot);
					const int32 Stock = Tile.Resource == AnastasisWorld::EResource::Food ? Tile.Amount : 0;
					if (Stock < AnastasisFields::FieldFoodCap * 0.4) Score += Collective.bFoodRush ? 36.0 : 22.0;
					else Score -= 4.0;
				}
			}
			Line = FMath::Max(0.0, Score);
		}
		double Score = Line * G::SurvivalWorkFactor(GoalHelpFarm, Work.WorkFactor, Work.bMealBlocked);
		// La chaine des biais d'`adultScores`, dans son ordre. `workplaceGoalBias` : +14 pour un poste
		// depot de vivres (le grenier) ; les autres termes nuls ici sont omis, comme pour les lignes du fermier.
		Score += PhaseBias;
		if (IsGranaryWorker(Npc)) Score += G::GranaryWorkplaceGoalBias(Npc.JobId, GoalHelpFarm, Npc.InventoryFood);
		Score += G::CompletionBias(GoalHelpFarm, Npc.InventoryFood, IsGranaryWorker(Npc) ? Npc.InventoryFood : 0, SessionGoalOf(Npc), NeedsCritical(Npc.Needs));
		Score += G::TraitGoalBias(G::TraitAt(Npc.TraitIndex), GoalHelpFarm);
		// `skillGoalBias` : domaine `gather` pour `helpFarm` (skills.js).
		Score += G::SkillGoalBias(Npc.SkillGather);
		return Score;
	}

	bool FVillage::HelpFarmTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource)
	{
		// « Aller a la parcelle faible, pas seulement au batiment ferme (sinon on rate le plot). »
		const int32 Plot = FindTendFieldFor(Npc);
		if (Plot != INDEX_NONE)
		{
			const AnastasisWorld::FTile Tile = LiveTile(Plot);
			OutTarget = FieldWorkTarget(Npc, Tile);
			OutSource = TEXT("field");
			return true;
		}
		// `sim.farmPos(npc)` : sans parcelle ni ferme, le seuil du marche prevu. Non branche ici
		// (`marketAccessPoint` arrive avec build-decision-001) : l'habitant vaque (ecart n°1).
		return false;
	}

	int32 FVillage::ProgressTendWork(FNpc& Npc)
	{
		namespace G = AnastasisGather;
		// La session en cours garde sa parcelle tant qu'elle n'est pas pleine.
		int32 Index = INDEX_NONE;
		if (Npc.WorkSession.bActive && Npc.WorkSession.CraftId == TEXT("tend"))
		{
			const int32 SessionIndex = TileIndexOf(World, Npc.WorkSession.TileX, Npc.WorkSession.TileY);
			if (SessionIndex != INDEX_NONE)
			{
				const AnastasisWorld::FTile SessionTile = LiveTile(SessionIndex);
				const int32 Stock = SessionTile.Resource == AnastasisWorld::EResource::Food ? SessionTile.Amount : 0;
				if (SessionTile.Type == AnastasisWorld::ETileType::Field && Stock < AnastasisFields::FieldFoodCap) Index = SessionIndex;
			}
		}
		if (Index == INDEX_NONE) Index = FindTendFieldFor(Npc);
		if (Index == INDEX_NONE)
		{
			ClearWorkSession(Npc);
			return 0;
		}
		const AnastasisWorld::FTile Tile = LiveTile(Index);
		EnsureCraftSession(Npc, Tile.X, Tile.Y, TEXT("tend"));
		Npc.Target = FieldWorkTarget(Npc, Tile);
		Npc.bHasTarget = true;
		// « Marche d'abord : findTendFieldNear peut etre a plusieurs cases (pas adjacent comme gather). »
		if (Dist(Npc.X, Npc.Y, Npc.Target.X, Npc.Target.Y) > 0.85)
		{
			SetActivity(Npc, TEXT("cherche"));
			return 1;
		}
		SetActivity(Npc, TEXT("recolte"));
		if (Now < Npc.WorkSession.NextSwingAt) return 1;

		// `rollCraftMiss(sim, npc, "tend")` tire `sim.rng` : pas de rate (ecart n°11), a brancher sur
		// la fonction generique de chat-on-haul-001.

		// `npc.lifeStage === "teen" ? 3 : 2` : pas d'adolescents dans ce village (ecart n°8).
		constexpr int32 TendAmount = 2;
		AnastasisWorld::FTile Grown = Tile;
		const bool bGrown = AnastasisFields::RegrowFieldTile(Grown, G::FieldSeasonTendAmount(TendAmount, Day()), Day());
		if (bGrown) LiveTiles.Add(Index, Grown);
		// `markCraftSwing` : un coup de plus, et le suivant apres `swingPeriodFor(npc, "tend")`.
		Npc.WorkSession.SwingsDone += 1;
		Npc.WorkSession.LastSwingAt = Now;
		Npc.WorkSession.NextSwingAt = Now + G::SwingPeriodTend(Npc.Skill, Npc.WorkSession.SwingsDone, Npc.Needs.Energy);
		if (bGrown)
		{
			Npc.DeedsHelped += TendAmount;
			Npc.Needs.Morale = Clamp(Npc.Needs.Morale + 0.4, 0.0, 100.0);
			G::GainDomainSkill(Npc.Skill, Npc.SkillGather, 0.003);
		}
		else
		{
			Npc.DeedsHelped += 1;
			G::GainDomainSkill(Npc.Skill, Npc.SkillGather, 0.001);
		}
		const AnastasisWorld::FTile After = LiveTile(Index);
		const int32 Stock = After.Resource == AnastasisWorld::EResource::Food ? After.Amount : 0;
		if (Stock >= AnastasisFields::FieldFoodCap || Npc.WorkSession.SwingsDone >= G::TendSwingsPerPlot)
		{
			ClearWorkSession(Npc);
			return 2;
		}
		return 1;
	}

	uint32 FVillage::ClaimedFieldPostsExcept(const FNpc* Ignored, int32 TileX, int32 TileY) const
	{
		namespace G = AnastasisGather;
		uint32 Claimed = 0;
		for (const FNpc& Other : Actors.GetItems())
		{
			if (&Other == Ignored || Other.Inside.bActive) continue;
			bool bHas = false;
			double PX = 0.0;
			double PY = 0.0;
			const FWorkSession& S = Other.WorkSession;
			if (S.bActive && (S.CraftId == TEXT("tend") || S.CraftId == TEXT("farm")) && S.TileX == TileX && S.TileY == TileY)
			{
				if (S.PostIndex >= 0)
				{
					Claimed |= 1u << (S.PostIndex % G::FieldPostCount);
					continue;
				}
				PX = Other.X;
				PY = Other.Y;
				bHas = true;
			}
			if (Other.bHasTarget && FMath::IsFinite(Other.Target.X) && FMath::IsFinite(Other.Target.Y)
				&& FloorInt(Other.Target.X) == TileX && FloorInt(Other.Target.Y) == TileY)
			{
				PX = Other.Target.X;
				PY = Other.Target.Y;
				bHas = true;
			}
			if (!bHas) continue;
			const int32 Index = G::NearestFieldPostIndex(TileX, TileY, PX, PY);
			double PostX = 0.0;
			double PostY = 0.0;
			G::FieldPostWorld(TileX, TileY, Index, PostX, PostY);
			if (JsHypot(PX - PostX, PY - PostY) < 0.38)
			{
				Claimed |= 1u << Index;
			}
		}
		return Claimed;
	}

	FPoint FVillage::FieldWorkTarget(FNpc& Npc, const AnastasisWorld::FTile& Tile)
	{
		namespace G = AnastasisGather;
		FWorkSession& S = Npc.WorkSession;
		const bool bSameTile = S.bActive && S.TileX == Tile.X && S.TileY == Tile.Y;
		FPoint Post;
		if (bSameTile && S.PostIndex >= 0)
		{
			G::FieldPostWorld(Tile.X, Tile.Y, S.PostIndex, Post.X, Post.Y);
			return Post;
		}
		const int32 Preferred = G::PreferredFieldPostIndex(Npc.Id, Tile.X, Tile.Y);
		const int32 Index = G::FieldWorkPostIndex(Preferred, ClaimedFieldPosts(Npc, Tile.X, Tile.Y));
		if (bSameTile)
		{
			S.PostIndex = Index;
		}
		G::FieldPostWorld(Tile.X, Tile.Y, Index, Post.X, Post.Y);
		return Post;
	}

	void FVillage::DepleteTile(int32 Index)
	{
		using AnastasisWorld::ETileType;
		// La branche foret (souches) ne sert qu'au bois : la nourriture pousse aux champs.
		AnastasisWorld::FTile& Tile = LiveTiles.FindOrAdd(Index, World->Tiles[Index]);
		const ETileType Before = Tile.Type;
		Tile.Resource = AnastasisWorld::EResource::None;
		Tile.Amount = 0;
		if (Tile.Type == ETileType::Stone || Tile.Type == ETileType::Ruin || Tile.Type == ETileType::Scrub)
		{
			Tile.Type = ETileType::Grass;
		}
		else
		{
			Tile.Type = ETileType::Field;
			// `ensureFieldFallow`.
			Tile.CropId = AnastasisWorld::ECropId::Fallow;
		}
		// `syncTileMoveCost` : un changement de type change le cout de marche de la case.
		if (Tile.Type != Before && !Nav.Blocked[Index])
		{
			Nav.MoveCost[Index] = AnastasisJs::StoreF32(AnastasisNav::TerrainMoveCostOf(&Tile, nullptr));
		}
	}

	int32 FVillage::ProgressCraftGather(FNpc& Npc)
	{
		namespace G = AnastasisGather;
		using AnastasisWorld::EResource;
		int32 Index = INDEX_NONE;
		if (Npc.WorkSession.bActive && Npc.WorkSession.CraftId == TEXT("farm"))
		{
			const int32 SessionIndex = TileIndexOf(World, Npc.WorkSession.TileX, Npc.WorkSession.TileY);
			if (SessionIndex != INDEX_NONE)
			{
				const AnastasisWorld::FTile SessionTile = LiveTile(SessionIndex);
				if (SessionTile.Resource == EResource::Food && SessionTile.Amount > 0) Index = SessionIndex;
			}
		}
		if (Index == INDEX_NONE) Index = ResourceTileNear(Npc, EResource::Food);
		if (Index == INDEX_NONE || LiveTile(Index).Amount <= 0)
		{
			// `noteGoalFailure(sim, npc, goal, "empty")` : memoire d'echec non portee.
			ClearWorkSession(Npc);
			return 0;
		}
		const AnastasisWorld::FTile Tile = LiveTile(Index);
		EnsureCraftSession(Npc, Tile.X, Tile.Y);
		// Champs : un poste stable dans la tuile, pas tous au centre.
		Npc.Target = Tile.Type == AnastasisWorld::ETileType::Field
			? FieldWorkTarget(Npc, Tile)
			: FPoint{ Tile.X + 0.5, Tile.Y + 0.5 };
		Npc.bHasTarget = true;
		SetActivity(Npc, TEXT("recolte"));
		if (Now < Npc.WorkSession.NextSwingAt) return 1;

		// Micro-echec rare : geste sans rendement, reprise un peu plus lente (chat-on-haul-001).
		if (RollCraftMiss(Npc, TEXT("farm")))
		{
			ApplyCraftMissRecovery(Npc, G::SwingPeriodFarm(Npc.Skill, Npc.WorkSession.SwingsDone, Npc.Needs.Energy));
			return 1;
		}
		int32 Taken = G::YieldPerSwingFarm(Npc.Skill);
		if (Tile.Type == AnastasisWorld::ETileType::Field)
		{
			Taken = G::FieldSeasonGatherAmount(Taken, Day());
		}
		Taken = FMath::Min(Tile.Amount, Taken);
		if (Taken <= 0)
		{
			ClearWorkSession(Npc);
			return 0;
		}
		TakeFromTile(Index, Taken);
		Npc.InventoryFood += Taken;
		Npc.GatheredFood += Taken;
		// `gainSkill(npc, 0.008)` : domaine du but, la cueillette.
		G::GainDomainSkill(Npc.Skill, Npc.SkillGather, G::GatherSkillGain);
		Npc.WorkSession.SwingsDone += 1;
		Npc.WorkSession.LastSwingAt = Now;
		Npc.WorkSession.NextSwingAt = Now + G::SwingPeriodFarm(Npc.Skill, Npc.WorkSession.SwingsDone, Npc.Needs.Energy);

		if (LiveTile(Index).Amount <= 0)
		{
			DepleteTile(Index);
		}
		if (G::ShouldHaulGatherLoad(Npc.InventoryFood))
		{
			BeginHaulToDepot(Npc);
			return 2;
		}
		const AnastasisWorld::FTile After = LiveTile(Index);
		if (After.Resource == EResource::None || After.Amount <= 0)
		{
			ClearWorkSession(Npc);
			return 2;
		}
		return 1;
	}

	void FVillage::BeginHaulToDepot(FNpc& Npc)
	{
		ClearWorkSession(Npc);
		// `depotResourceOf(grenier)` = food : le sac va au poste.
		FBuilding* Workplace = Npc.WorkplaceId.IsEmpty() ? nullptr : Buildings.FindById(Npc.WorkplaceId);
		if (Workplace && Workplace->Type == GranaryType && Npc.InventoryFood > 0)
		{
			FPoint Access;
			Npc.Goal = GoalDeliver;
			Npc.bHasTarget = BuildingAccessPoint(*Workplace, &Npc, Access);
			Npc.Target = Access;
			ClearNavigation(Npc);
			return;
		}
		// Sans depot : livrer au hub ou vendre au marche — non portes (ecart n°12).
		Npc.Goal = GoalObserver;
		Npc.bHasTarget = false;
		Npc.DestBuildingId.Reset();
		ClearNavigation(Npc);
	}

	bool FVillage::Deliver(FNpc& Npc)
	{
		// `atOwnWorkplace(npc, 3.2)` : poste acheve, centre a 3,2 tuiles au plus.
		FBuilding* Workplace = Npc.WorkplaceId.IsEmpty() ? nullptr : Buildings.FindById(Npc.WorkplaceId);
		const bool bAtOwn = Workplace && Workplace->Progress >= 1.0
			&& JsHypot(Npc.X - (Workplace->X + 0.5), Npc.Y - (Workplace->Y + 0.5)) <= IndoorBuildingRadius;
		// Hors de son poste, la reference livre au marche, a l'entrepot ou au camp :
		// aucun n'existe dans ce portage, `deliver` echoue comme elle.
		if (!bAtOwn || Workplace->Type != GranaryType) return false;
		// `resourceWithCargo(npc, "food")`.
		if (Npc.InventoryFood <= 0) return false;
		const int32 Room = FMath::Max(0, GranaryFoodCap - Workplace->FoodPhysical);
		const int32 Amount = FMath::Min(Room, FMath::Min(Npc.InventoryFood, AnastasisGather::DepotMaxBatch));
		if (Amount <= 0) return false;
		// `transferNpcToBuilding` puis `creditStock` (la place a deja ete comptee).
		const int32 Moved = FMath::Min(Amount, FMath::Min(Npc.InventoryFood, Room));
		if (Moved <= 0) return false;
		Npc.InventoryFood -= Moved;
		Workplace->FoodPhysical += Moved;
		Npc.DeliveredFood += Moved;
		++Npc.Deliveries;
		Npc.Needs.Morale = Clamp(Npc.Needs.Morale + 1.0, 0.0, 100.0);
		// `gainSkill(npc, 0.002)` : domaine du but, le marche.
		AnastasisGather::GainDomainSkill(Npc.Skill, Npc.SkillTrade, AnastasisGather::DeliverSkillGain);
		// `if (depot) { workPresence ; maybeChatOnHaul }` : le depot est toujours son poste ici.
		// `workPresence` n'est pas porte (ecart n°26).
		MaybeChatOnHaul(Npc);
		return true;
	}

	// --- Repousse des champs (simulation.js regrowFieldsDaily) -------------------
	bool FVillage::GetSoilWaterAt(int32 TileX, int32 TileY, double& OutStored) const
	{
		if (!bSoilWaterEnabled || !World || TileX < 0 || TileY < 0 || TileX >= World->W || TileY >= World->H) return false;
		const double* Stored = SoilWaterByTile.Find(TileY * World->W + TileX);
		if (!Stored) return false;
		OutStored = *Stored;
		return true;
	}

	int32 FVillage::RegrowFieldsDaily(int32 Day)
	{
		if (!World) return 0;
		// ecart n°37: opt-in reservoir driven by the previous day's weather index.
		const double PreviousRain = bSoilWaterEnabled && bForcedWeather ? ForcedWeather.Rain
			: bSoilWaterEnabled && bWeatherSeeded
				? AnastasisWeather::WeatherAt(WeatherSeed, FMath::Max(1, Day - 1)).Rain : 0.0;
		const AnastasisWeather::ESeason Season = AnastasisWeather::FieldSeasonFromDay(Day);
		const double EvaporationIndex = Season == AnastasisWeather::ESeason::Summer ? 1.0
			: Season == AnastasisWeather::ESeason::Winter ? 0.35 : 0.70;
		int32 Grown = 0;
		for (int32 Index = 0; Index < World->Tiles.Num(); ++Index)
		{
			if (World->Tiles[Index].Type != AnastasisWorld::ETileType::Field && !LiveTiles.Contains(Index)) continue;
			if (FoodSources.ContainsByPredicate([&](const FFoodSource& S) { return S.TileIndex == Index; })) continue;
			AnastasisWorld::FTile Tile = LiveTile(Index);
			const int32 Before = Tile.Resource == AnastasisWorld::EResource::Food ? Tile.Amount : 0;
			const double BaseFertility = Tile.Fertility;
			if (bSoilWaterEnabled && Tile.Type == AnastasisWorld::ETileType::Field)
			{
				const double Initial = SoilWaterByTile.Contains(Index) ? SoilWaterByTile[Index] : Tile.Wetness;
				const AnastasisSoilWater::FStep Step = AnastasisSoilWater::Advance(Initial, PreviousRain, EvaporationIndex);
				if (Step.bValid)
				{
					SoilWaterByTile.Add(Index, Step.After);
					Tile.Fertility = (BaseFertility != 0.0 ? BaseFertility : 1.0)
						* AnastasisSoilWater::GrowthFactor(Step.After);
				}
			}
			if (AnastasisFields::RegrowTileDaily(Tile, Day))
			{
				Tile.Fertility = BaseFertility;
				RegrownFood += Tile.Amount - Before;
				LiveTiles.Add(Index, Tile);
				++Grown;
			}
		}
		return Grown;
	}

	// --- Socialiser, souffler (npc.js, villageRhythm.js, domestic.js) ---------------

	double FVillage::SocialRowScore(const FNpc& Npc, const FString& Goal, double NeedScore, double PhaseBias, double Noise) const
	{
		namespace G = AnastasisGather;
		// `socialize` : needs.socialize + jobPriority + planBias + goalNoise ;
		// `relax` : needs.relax + goalNoise + hearthInviteScore * 0,45. Ni plan, ni bruit, ni scene.
		double Score = Goal == GoalSocialize ? NeedScore + G::JobPriority(Npc.JobId, Goal) : NeedScore;
		// Le bruit ferme la somme de la ligne, avant les biais (`planBias` et la scene de foyer valent 0).
		Score += Noise;
		Score += PhaseBias;
		// workplaceGoalBias : 0 (aubergiste, pretre seulement) ; completionBias pour tous.
		const int32 DepotLoad = IsGranaryWorker(Npc) ? Npc.InventoryFood : 0;
		const FString SessionGoal = SessionGoalOf(Npc);
		Score += G::CompletionBias(Goal, Npc.InventoryFood, DepotLoad, SessionGoal, NeedsCritical(Npc.Needs));
		// `moodletGoalBias` (lu sur une copie : `tickMoodlets` elague deja a chaque tick).
		TArray<AnastasisBonds::FMoodlet> Moodlets = Npc.Moodlets;
		Score += AnastasisBonds::MoodletGoalBias(Moodlets, Goal, Now);
		Score += G::TraitGoalBias(G::TraitAt(Npc.TraitIndex), Goal);
		// natureGoalBias (coeur 1) et skillGoalBias (soin 1) : 0.
		if (Goal == GoalSocialize)
		{
			// `socialMemoryBias` puis `socialSeekBias` (une personne memorisee a rejoindre).
			Score += AnastasisBonds::SocialMemoryBiasSocialize(Npc.People);
			Score += PickRememberedSeekFor(Npc).IsEmpty() ? 0.0 : AnastasisBonds::SeekBias;
		}
		// « Pression morale : sociabilite (deuil / desespoir) sans ecraser les besoins. »
		Score += (G::MoralSocialMul(Npc.Needs.Morale, MarketFood(), Day()) - 1.0) * 18.0;
		return Score;
	}

	bool FVillage::SocialPos(FNpc& Npc, FPoint& OutTarget, FString& OutSource)
	{
		// `workCommutePos(socialize)` (aubergiste, pretre) et `districtSocialBuilding` : non portes.
		// Le premier batiment acheve dont `function` = socialiser ou `dailyMorale` > 0 : le puits.
		for (FBuilding& B : Buildings.GetItemsMutable())
		{
			if (B.Progress < 1.0 || B.Type != WellType) continue;
			if (BuildingAccessPoint(B, &Npc, OutTarget))
			{
				OutSource = TEXT("well");
				return true;
			}
		}
		// `plazaMeetingPoint` (routes) puis `marketAccessPoint` (site du marche) : non portes.
		if (AccessPointNear(Settlement.X, Settlement.Y, &Npc, OutTarget))
		{
			OutSource = TEXT("settlement");
			return true;
		}
		OutSource = TEXT("none");
		return false;
	}

	bool FVillage::SocializeTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource)
	{
		// Base `socialPos` ; rythme : soir et nuit -> taverne (aucune) puis socialPos ; midi ->
		// premier puits acheve, sinon place / socialPos ; sinon socialPos. Tous menent au puits.
		// Puis `bondSocialTarget` (rejoindre un ami), `rememberedSocialTarget` (la personne
		// memorisee) ; la couche age n'a rien a faire sans enfants ni aines.
		bool bHave = SocialPos(Npc, OutTarget, OutSource);
		if (BondSocialTarget(Npc, OutTarget))
		{
			bHave = true;
			OutSource = TEXT("bonds");
		}
		if (RememberedSocialTarget(Npc, OutTarget))
		{
			bHave = true;
			OutSource = TEXT("memory");
		}
		return bHave;
	}

	bool FVillage::RelaxTarget(FNpc& Npc, FPoint& OutTarget, FString& OutSource)
	{
		// Couche rythme (recouvre toujours la base) : soir avec un foyer -> le foyer ;
		// midi -> le puits (ou socialPos) ; sinon foyer, sinon socialPos. `rhythmTarget` lit la phase
		// PERSONNELLE (`villagePhaseFor`, reconsider-001).
		const AnastasisRhythm::EPhase Phase = PersonalPhaseOf(Npc);
		bool bHave = false;
		if (Phase != AnastasisRhythm::EPhase::Midday && !Npc.HomeId.IsEmpty() && BuildingAccessPointById(Npc.HomeId, &Npc, OutTarget))
		{
			bHave = true;
			OutSource = TEXT("home");
		}
		if (!bHave)
		{
			bHave = SocialPos(Npc, OutTarget, OutSource);
		}
		// Couche domestique : le foyer ou l'abri, sinon un abri ouvert.
		const FString Living = Npc.LivingHomeId();
		FPoint Domestic;
		if (!Living.IsEmpty())
		{
			if (BuildingAccessPointById(Living, &Npc, Domestic))
			{
				OutTarget = Domestic;
				bHave = true;
				OutSource = Living == Npc.HomeId ? TEXT("home") : TEXT("shelter");
			}
		}
		else if (const FBuilding* Open = FindOpenShelter(Npc))
		{
			if (BuildingAccessPointById(Open->Id, &Npc, Domestic))
			{
				OutTarget = Domestic;
				bHave = true;
				OutSource = TEXT("open-shelter");
			}
		}
		return bHave;
	}
	// --- Liens, paroles, rumeurs (bonds.js, talk.js, socialMemory.js, speechActs.js) ---

	namespace
	{
		namespace BD = AnastasisBonds;

		bool IsWorkingGoalForBonds(const FString& Goal)
		{
			return Goal.StartsWith(TEXT("gather"), ESearchCase::CaseSensitive)
				|| Goal == TEXT("craft") || Goal == TEXT("build") || Goal == TEXT("deliver")
				|| Goal == TEXT("sell") || Goal == TEXT("maintain");
		}

		double& RelationRef(TArray<TPair<FString, double>>& Relations, const FString& Id)
		{
			for (TPair<FString, double>& R : Relations)
			{
				if (R.Key == Id) return R.Value;
			}
			Relations.Add(TPair<FString, double>(Id, 0.0));
			return Relations.Last().Value;
		}

		/** `noteBondStageCross` : montee vers ami / proche / intrigue -> moodlet newFriend aux deux. */
		void NoteStageCross(double PrevRel, double NextRel, TArray<BD::FMoodlet>& MoodletsA, double& MoraleA,
			TArray<BD::FMoodlet>& MoodletsB, double& MoraleB, double Now)
		{
			const int32 From = BD::BondStageRank(PrevRel);
			const int32 To = BD::BondStageRank(NextRel);
			if (From == To) return;
			// Descente vers « rival » (rivalHeat) : la conversation ne fait que monter les relations.
			if (To > From && To >= 2)
			{
				BD::StampNewFriend(MoodletsA, MoraleA, Now);
				BD::StampNewFriend(MoodletsB, MoraleB, Now);
			}
		}

		FLastTalk MakeLastTalk(const FString& WithId, double At, bool bRefuse)
		{
			FLastTalk T;
			T.bValid = true;
			T.WithId = WithId;
			T.At = At;
			T.bRefuse = bRefuse;
			return T;
		}
	}

	bool CanStartTalkFor(double Now, bool bALast, double ALastAt, bool bBLast, double BLastAt,
		int32 FatigueAB, double FatigueABAt, int32 FatigueBA, double FatigueBAAt)
	{
		if (bALast && Now - ALastAt < BD::PairCooldownSeconds) return false;
		if (bBLast && Now - BLastAt < BD::PairCooldownSeconds) return false;
		auto Level = [Now](int32 Count, double At) { return Count > 0 && !(Now - At > BD::FatigueWindowSeconds) ? Count : 0; };
		return FMath::Max(Level(FatigueAB, FatigueABAt), Level(FatigueBA, FatigueBAAt)) < BD::FatigueBlockCount;
	}

	void BumpRelationPair(FBondPair& Pair, double DeltaA, double DeltaB, double Now)
	{
		const double Prev = Pair.RelAB;
		Pair.RelAB = Clamp(Prev + DeltaA, -100.0, 100.0);
		Pair.RelBA = Clamp(Pair.RelBA + DeltaB, -100.0, 100.0);
		NoteStageCross(Prev, Pair.RelAB, Pair.MoodletsA, Pair.MoraleA, Pair.MoodletsB, Pair.MoraleB, Now);
	}

	TArray<FSpotAct> CreateInformSpotActs(const TArray<FResourceSpot>& Source, const TArray<FResourceSpot>& Target,
		const FString& SourceId, double R01, int32 Limit)
	{
		TArray<FSpotAct> Acts;
		const int32 N = Source.Num();
		if (N == 0) return Acts;
		const int32 Start = static_cast<int32>(AnastasisJs::Floor(R01 * N));
		for (int32 I = 0; I < N && Acts.Num() < FMath::Max(1, Limit); ++I)
		{
			const FResourceSpot& Spot = Source[(Start + I) % N];
			// Seul ce qu'on a vu de ses yeux se colporte ; rien que l'autre sait deja.
			if (Spot.bHearsay || Target.ContainsByPredicate([&](const FResourceSpot& T) { return T.Key == Spot.Key; })) continue;
			FSpotAct Act;
			Act.SourceId = SourceId;
			Act.Key = Spot.Key;
			Act.Resource = Spot.Resource;
			Act.X = Spot.X;
			Act.Y = Spot.Y;
			Act.Amount = Spot.Amount;
			Act.Day = Spot.Day;
			Act.HopCount = Spot.HopCount;
			Act.OriginalSourceId = Spot.OriginalSourceId.IsEmpty() ? SourceId : Spot.OriginalSourceId;
			Acts.Add(MoveTemp(Act));
		}
		return Acts;
	}

	bool CommitHearsaySpot(TArray<FResourceSpot>& Target, const FSpotAct& Act, int32 Day, double Time)
	{
		const FString Key = FString::Printf(TEXT("%d,%d"),
			static_cast<int32>(AnastasisJs::Floor(Act.X)), static_cast<int32>(AnastasisJs::Floor(Act.Y)));
		if (Target.ContainsByPredicate([&](const FResourceSpot& T) { return T.Key == Key; })) return false;
		FResourceSpot Spot;
		Spot.Key = Key;
		Spot.X = Act.X;
		Spot.Y = Act.Y;
		Spot.Resource = Act.Resource;
		Spot.Amount = Act.Amount;
		// Le jour d'OBSERVATION de la source, pas le jour d'ecoute : l'on-dit herite de l'age du souvenir.
		Spot.Day = Act.Day;
		Spot.bHearsay = true;
		Spot.SourceId = Act.SourceId;
		Spot.OriginalSourceId = Act.OriginalSourceId;
		Spot.HopCount = FMath::Max(1, Act.HopCount + 1);
		Spot.ReceivedDay = Day;
		Spot.ReceivedAt = Time;
		Target.Add(MoveTemp(Spot));
		TrimSpots(Target);
		return true;
	}

	double FVillage::RelationOf(const FNpc& Npc, const FString& OtherId)
	{
		for (const TPair<FString, double>& R : Npc.Relations)
		{
			if (R.Key == OtherId) return R.Value;
		}
		return 0.0;
	}

	bool FVillage::IsTalking(const FNpc& Npc) const
	{
		return !Npc.TalkWithId.IsEmpty() && FMath::IsFinite(Npc.TalkUntil) && Now < Npc.TalkUntil;
	}

	void FVillage::ClearTalkSession(FNpc& Npc)
	{
		Npc.TalkWithId.Reset();
		Npc.TalkUntil = 0.0;
		Npc.TalkTurn = 0;
		Npc.TalkMaxTurns = 0;
		Npc.TalkStarterId.Reset();
		Npc.TalkNextAt = 0.0;
		Npc.bTalkChain = false;
	}

	bool FVillage::IsSociallyAvailable(const FNpc& Other) const
	{
		if (!Other.TalkWithId.IsEmpty()) return true;
		if (Other.Goal == GoalSocialize || Other.Goal == TEXT("visitFamily") || Other.Goal == TEXT("play")) return true;
		return Other.Activity == TEXT("socialise") || Other.Activity == TEXT("joue");
	}

	int32 FVillage::TalkFatigueLevel(const FNpc& A, const FNpc& B) const
	{
		const FTalkFatigue* E = A.TalkFatigue.FindByPredicate([&](const FTalkFatigue& F) { return F.Id == B.Id; });
		if (!E) return 0;
		if (Now - E->At > BD::FatigueWindowSeconds) return 0;
		return E->Count;
	}

	bool FVillage::CanStartTalk(const FNpc& A, const FNpc& B) const
	{
		if (A.LastTalk.bValid && A.LastTalk.WithId == B.Id && Now - A.LastTalk.At < BD::PairCooldownSeconds) return false;
		if (B.LastTalk.bValid && B.LastTalk.WithId == A.Id && Now - B.LastTalk.At < BD::PairCooldownSeconds) return false;
		return FMath::Max(TalkFatigueLevel(A, B), TalkFatigueLevel(B, A)) < BD::FatigueBlockCount;
	}

	int32 FVillage::VillageEmitCount() const
	{
		int32 N = 0;
		for (const double At : RecentVillageEmits)
		{
			if (Now - At <= BD::VillageEmitWindow) ++N;
		}
		return N;
	}

	FNpc* FVillage::PickSocialCompanion(FNpc& Npc, double MaxDistance)
	{
		TArray<FNpc>& Items = Actors.GetItemsMutable();
		FNpc* Best = nullptr;
		double BestScore = -AnastasisNav::Infinity;
		Grid.ForEachNear(Npc.X, Npc.Y, MaxDistance, [&](int32 Index)
		{
			if (!Items.IsValidIndex(Index)) return;
			FNpc& Other = Items[Index];
			if (&Other == &Npc) return;
			const double D = Dist(Npc.X, Npc.Y, Other.X, Other.Y);
			// EXTENSION (player-minimal-001, ecart n°22) : presence 1, exactement `D > MaxDistance`.
			if (!Sees(Other, D, MaxDistance)) return;
			const BD::FPersonRow* Row = BD::FindPerson(Npc.People, Other.Id);
			const double Affinity = BD::CompanionAffinity(RelationOf(Npc, Other.Id), Row ? Row->Trust : 0.0,
				Row ? Row->Tag : BD::EPersonTag::None, Npc.JobId == Other.JobId, IsSociallyAvailable(Other))
				+ ReputationAffinity(Other);
			const double Score = Affinity - D * BD::DistWeight;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = &Other;
			}
		});
		return Best;
	}

	void FVillage::BumpRelation(FNpc& A, FNpc& B, double DeltaA, double DeltaB)
	{
		double& AB = RelationRef(A.Relations, B.Id);
		const double Prev = AB;
		AB = Clamp(Prev + DeltaA, -100.0, 100.0);
		const double Next = AB;
		double& BA = RelationRef(B.Relations, A.Id);
		BA = Clamp(BA + DeltaB, -100.0, 100.0);
		NoteStageCross(Prev, Next, A.Moodlets, A.Needs.Morale, B.Moodlets, B.Needs.Morale, Now);
	}

	void FVillage::NoteMeeting(FNpc& A, FNpc& B)
	{
		const int32 Today = Day();
		BD::NotePersonDirect(A.People, B.Id, Today, BD::MeetTrust);
		BD::NotePersonDirect(B.People, A.Id, Today, BD::MeetTrust * 0.85);
		BD::ObserveMind(A.Tom, B.Id, B.Goal, RelationOf(B, A.Id), Today);
		BD::ObserveMind(B.Tom, A.Id, A.Goal, RelationOf(A, B.Id), Today);
	}

	bool FVillage::BeginTalkSession(FNpc& Speaker, FNpc& Listener, bool bContinue)
	{
		if (Dist(Speaker.X, Speaker.Y, Listener.X, Listener.Y) > BD::SessionMaxDistance) return false;
		if (Speaker.Inside.bActive && Listener.Inside.bActive && Speaker.Inside.BuildingId != Listener.Inside.BuildingId) return false;
		if (!bContinue)
		{
			// `canBeginTalkSession` : budget de gels simultanes.
			const bool bPair = (IsTalking(Speaker) && Speaker.TalkWithId == Listener.Id)
				|| (IsTalking(Listener) && Listener.TalkWithId == Speaker.Id);
			if (!bPair)
			{
				int32 Talking = 0;
				for (const FNpc& N : Actors.GetItems()) Talking += IsTalking(N) ? 1 : 0;
				const int32 Need = (IsTalking(Speaker) ? 0 : 1) + (IsTalking(Listener) ? 0 : 1);
				const int32 Budget = FMath::Max(BD::MaxConcurrentTalkers,
					static_cast<int32>(FMath::CeilToDouble(Actors.Num() * BD::ConcurrentTalkFrac)));
				if (Talking + Need > Budget) return false;
			}
		}
		const bool bUrgent = BD::IsTalkUrgent(Speaker.Needs) || BD::IsTalkUrgent(Listener.Needs);
		const bool bWork = BD::IsTalkWorkBusy(Speaker.Goal, Speaker.Inside.bActive) || BD::IsTalkWorkBusy(Listener.Goal, Listener.Inside.bActive);
		const double Beat = BD::TalkHoldDuration(bUrgent, bWork);
		if (Beat <= 0.0) return false;
		if (!bContinue)
		{
			const int32 Fatigue = FMath::Max(TalkFatigueLevel(Speaker, Listener), TalkFatigueLevel(Listener, Speaker));
			const BD::EBondKind Kind = BD::BondKindBetween(RelationOf(Speaker, Listener.Id), RelationOf(Listener, Speaker.Id), Speaker.JobId, Listener.JobId);
			const int32 MaxTurns = BD::TalkMaxTurns(Speaker.Id, Listener.Id, bUrgent, bWork, Kind, Fatigue);
			const double Total = MaxTurns <= 1 ? Beat : BD::TurnSeconds * MaxTurns;
			const double Until = Now + Total;
			const double NextAt = Now + (MaxTurns <= 1 ? Beat : BD::TurnSeconds);
			for (FNpc* N : { &Speaker, &Listener })
			{
				N->TalkWithId = N == &Speaker ? Listener.Id : Speaker.Id;
				N->TalkUntil = Until;
				N->TalkTurn = 1;
				N->TalkMaxTurns = MaxTurns;
				N->TalkStarterId = Speaker.Id;
				N->TalkNextAt = NextAt;
			}
			return true;
		}
		// Tour suivant : prolonge d'un tour sans remettre les compteurs.
		const double Until = Now + BD::TurnSeconds;
		Speaker.TalkWithId = Listener.Id;
		Speaker.TalkUntil = FMath::Max(Speaker.TalkUntil, Until);
		Listener.TalkWithId = Speaker.Id;
		Listener.TalkUntil = FMath::Max(Listener.TalkUntil, Until);
		return true;
	}

	void FVillage::RecordTalk(FNpc& Speaker, FNpc& Listener, bool bContinue, TOptional<BD::EBondKind> KindOverride, double AmbientChance)
	{
		// `options.kind || bondKindBetween(speaker, listener)`.
		const BD::EBondKind Kind = KindOverride.IsSet() ? KindOverride.GetValue()
			: BD::BondKindBetween(RelationOf(Speaker, Listener.Id), RelationOf(Listener, Speaker.Id), Speaker.JobId, Listener.JobId);
		if (!bContinue)
		{
			if (!CanStartTalk(Speaker, Listener)) return;
			if (Speaker.LastTalk.bValid && Speaker.LastTalk.WithId == Listener.Id && Now - Speaker.LastTalk.At < BD::PairCooldownSeconds) return;
			// Porte d'impulsion : `shouldSpeakNow`.
			const double Worth = BD::SpeakWorth(Speaker.Needs, Listener.Needs, Speaker.bTalkChain, Kind);
			if (!BD::ShouldSpeakNow(Worth, VillageEmitCount(), Speaker.Id, Listener.Id, Now, AmbientChance)) return;
		}
		const bool bAllowReply = bContinue || !Listener.LastTalk.bValid || Now - Listener.LastTalk.At > BD::ReplyQuietSeconds;
		const int32 Fatigue = FMath::Max(TalkFatigueLevel(Listener, Speaker), TalkFatigueLevel(Speaker, Listener));
		const int64 BaseSalt = static_cast<int64>(AnastasisJs::Floor(Now * 10.0))
			+ (bContinue ? static_cast<int64>(Speaker.TalkTurn != 0 ? Speaker.TalkTurn : 1) * 13 : 0);
		// Ecart n°16 : le refus est evalue au premier tirage (`attempt` = 0) ; le texte n'est pas porte.
		const bool bRefuse = bAllowReply
			&& BD::RefusesReply(Listener.Needs, Kind, Fatigue, BD::HashTalk(Listener.Id, Speaker.Id, BaseSalt + 17));

		Speaker.LastTalk = MakeLastTalk(Listener.Id, Now, false);
		Speaker.bTalkChain = true;
		Listener.bTalkChain = true;
		if (bAllowReply)
		{
			Listener.LastTalk = MakeLastTalk(Speaker.Id, Now, bRefuse);
			if (bRefuse)
			{
				Speaker.TalkMaxTurns = Speaker.TalkTurn != 0 ? Speaker.TalkTurn : 1;
				Listener.TalkMaxTurns = Listener.TalkTurn != 0 ? Listener.TalkTurn : 1;
				const double Cut = Now + BD::SessionSecondsUrgent;
				Speaker.TalkUntil = FMath::Min(Speaker.TalkUntil != 0.0 ? Speaker.TalkUntil : Cut, Cut);
				Listener.TalkUntil = FMath::Min(Listener.TalkUntil != 0.0 ? Listener.TalkUntil : Cut, Cut);
			}
		}
		if (!bContinue)
		{
			// `bumpTalkFatigue` dans les deux sens, puis `rememberVillageEmit`.
			for (TPair<FNpc*, FNpc*> P : { TPair<FNpc*, FNpc*>(&Speaker, &Listener), TPair<FNpc*, FNpc*>(&Listener, &Speaker) })
			{
				FTalkFatigue* E = P.Key->TalkFatigue.FindByPredicate([&](const FTalkFatigue& F) { return F.Id == P.Value->Id; });
				if (!E)
				{
					FTalkFatigue New;
					New.Id = P.Value->Id;
					E = &P.Key->TalkFatigue.Add_GetRef(New);
					E->Count = 0;
					E->At = -AnastasisNav::Infinity;
				}
				if (Now - E->At > BD::FatigueWindowSeconds) E->Count = 1;
				else E->Count += 1;
				E->At = Now;
			}
			RecentVillageEmits.Add(Now);
			RecentVillageEmits.RemoveAll([&](double At) { return !(Now - At <= BD::VillageEmitWindow); });
			const int32 Keep = FMath::Max(8, BD::VillageEmitLimit * 3);
			if (RecentVillageEmits.Num() > Keep) RecentVillageEmits.RemoveAt(0, RecentVillageEmits.Num() - Keep);
		}
		BeginTalkSession(Speaker, Listener, bContinue);
		// Refus apres l'ouverture : on recoupe si l'ouverture a re-etendu.
		if (Listener.LastTalk.bValid && Listener.LastTalk.bRefuse)
		{
			const double Cut = Now + BD::SessionSecondsUrgent;
			Speaker.TalkMaxTurns = Speaker.TalkTurn != 0 ? Speaker.TalkTurn : 1;
			Listener.TalkMaxTurns = Listener.TalkTurn != 0 ? Listener.TalkTurn : 1;
			Speaker.TalkUntil = Cut;
			Listener.TalkUntil = Cut;
		}
	}

	void FVillage::AdvanceTalkTurn(FNpc& Driver, FNpc& Partner)
	{
		const int32 Turn = Driver.TalkTurn + 1;
		if (Turn > Driver.TalkMaxTurns) return;
		const FString StarterId = Driver.TalkStarterId.IsEmpty() ? Driver.Id : Driver.TalkStarterId;
		FNpc& Starter = Driver.Id == StarterId ? Driver : Partner;
		FNpc& Other = &Starter == &Driver ? Partner : Driver;
		// Tours impairs : le starter ; pairs : l'autre.
		FNpc& Speaker = Turn % 2 == 1 ? Starter : Other;
		FNpc& Listener = &Speaker == &Starter ? Other : Starter;
		Driver.TalkTurn = Turn;
		Partner.TalkTurn = Turn;
		Driver.TalkNextAt = Now + BD::TurnSeconds;
		Partner.TalkNextAt = Now + BD::TurnSeconds;
		RecordTalk(Speaker, Listener, /*bContinue=*/true);
	}

	bool FVillage::HoldTalk(FNpc& Npc)
	{
		if (!IsTalking(Npc))
		{
			if (!Npc.TalkWithId.IsEmpty()) ClearTalkSession(Npc);
			return false;
		}
		FNpc* Partner = Actors.FindById(Npc.TalkWithId);
		if (!Partner)
		{
			ClearTalkSession(Npc);
			return false;
		}
		// Urgence vitale ou partenaire trop loin : on lache tout de suite, les deux.
		if (BD::IsTalkUrgent(Npc.Needs) || BD::IsTalkUrgent(Partner->Needs)
			|| Dist(Npc.X, Npc.Y, Partner->X, Partner->Y) > BD::SessionMaxDistance * 1.35)
		{
			ClearTalkSession(Npc);
			if (Partner->TalkWithId == Npc.Id) ClearTalkSession(*Partner);
			return false;
		}
		if (Partner->TalkWithId != Npc.Id || !IsTalking(*Partner))
		{
			ClearTalkSession(Npc);
			return false;
		}
		// Multi-tours : le starter avance la conversation pendant le gel.
		if (Npc.TalkStarterId == Npc.Id && Npc.TalkTurn < Npc.TalkMaxTurns && Now >= Npc.TalkNextAt
			&& !BD::IsTalkUrgent(Npc.Needs) && !BD::IsTalkUrgent(Partner->Needs))
		{
			AdvanceTalkTurn(Npc, *Partner);
		}
		SetActivity(Npc, TEXT("socialise"));
		if (!Npc.Inside.bActive)
		{
			// Le regard ancre sur le partenaire, sans marcher : la cible d'origine est ecrasee.
			Npc.Target = { Partner->X, Partner->Y };
			Npc.bHasTarget = true;
			Npc.bTalkAnchor = true;
		}
		return true;
	}

	void FVillage::ExchangeSpotRumors(FNpc& A, FNpc& B)
	{
		// Les deux sens sont prepares AVANT tout depot (chacun voit l'etat d'avant l'echange).
		const double RA = A.Spots.Num() > 0 ? VillageRng.Next() : 0.0;
		const TArray<FSpotAct> AB = CreateInformSpotActs(A.Spots, B.Spots, A.Id, RA, 2);
		const double RB = B.Spots.Num() > 0 ? VillageRng.Next() : 0.0;
		const TArray<FSpotAct> BA = CreateInformSpotActs(B.Spots, A.Spots, B.Id, RB, 2);
		const int32 Today = Day();
		for (const FSpotAct& Act : AB)
		{
			if (CommitHearsaySpot(B.Spots, Act, Today, Now))
			{
				++A.RumorsShared;
				++B.RumorsHeard;
			}
		}
		for (const FSpotAct& Act : BA)
		{
			if (CommitHearsaySpot(A.Spots, Act, Today, Now))
			{
				++B.RumorsShared;
				++A.RumorsHeard;
			}
		}
	}

	int32 FVillage::TellSpots(FNpc& From, FNpc& To)
	{
		// `createInformResourceSpotActs(sim, from, to, { limit: PERCEPTION.rumorSpots })` : sans gisement,
		// aucun tirage. Puis `commitSpeechActs` : deposes tout de suite, avant l'autre sens.
		if (From.Spots.Num() == 0) return 0;
		const double R = VillageRng.Next();
		const TArray<FSpotAct> Acts = CreateInformSpotActs(From.Spots, To.Spots, From.Id, R, 2);
		const int32 Today = Day();
		int32 Applied = 0;
		for (const FSpotAct& Act : Acts)
		{
			if (CommitHearsaySpot(To.Spots, Act, Today, Now))
			{
				++From.RumorsShared;
				++To.RumorsHeard;
				++Applied;
			}
		}
		return Applied;
	}

	void FVillage::MaybeChatOnHaul(FNpc& Npc)
	{
		// « Soft : pas a chaque depot. » Le tirage est inconditionnel : il se consomme toujours.
		if (VillageRng.Next() > 0.42) return;
		FNpc* Other = PickSocialCompanion(Npc, 3.4);
		if (!Other) return;
		if (!CanStartTalk(Npc, *Other)) return;
		const bool bSameJob = !Npc.JobId.IsEmpty() && !Other->JobId.IsEmpty() && Npc.JobId == Other->JobId;
		// `recordTalk(sim, npc, other, sameJob ? { kind: "coworker", ambientChance: 0.1 } : { ambientChance: 0.1 })`.
		RecordTalk(Npc, *Other, /*bContinue=*/false,
			bSameJob ? TOptional<BD::EBondKind>(BD::EBondKind::Coworker) : TOptional<BD::EBondKind>(), 0.1);
		// `sim.shareRumors(npc, other)` : les gisements, dans les deux sens, l'un apres l'autre. Ni acces
		// bloques, ni savoir negatif, ni croyance de marche, ni croyances (eau, lits, dangers), ni
		// `spreadRumorExchange` (personnes, episodes, chronique) : ecart n°16.
		TellSpots(Npc, *Other);
		TellSpots(*Other, Npc);
		// `maybeCounselPair` : aucun aine dans ce portage (ecart n°8).
		// `gain = max(2, floor(bondTalkGain(npc, other) * 0.5))`, sans `bumpRelation` : pas de palier note.
		const int32 Gain = FMath::Max(2, static_cast<int32>(AnastasisJs::Floor(BD::BondTalkGain(RelationOf(Npc, Other->Id) >= BD::FriendAt) * 0.5)));
		double& AB = RelationRef(Npc.Relations, Other->Id);
		AB = Clamp(AB + Gain, -100.0, 100.0);
		double& BA = RelationRef(Other->Relations, Npc.Id);
		BA = Clamp(BA + FMath::Max(1, Gain - 1), -100.0, 100.0);
	}

	bool FVillage::RollCraftMiss(FNpc& Npc, const FString& CraftId)
	{
		namespace CM = AnastasisCraftMiss;
		if (!CM::CanRoll(CraftId, Now, Npc.CraftMissAt, Npc.CraftMissStampAt)) return false;
		const double Chance = CM::MissChance(CraftId, Npc.Skill, /*Mastery=*/0.0, Npc.WorkSession.SwingsDone, Npc.Needs.Energy);
		if (Chance <= 0.0) return false;
		if (VillageRng.Next() >= Chance) return false;
		// `stampCraftMiss`.
		Npc.CraftMissKind = CM::MissKindFor(CraftId);
		Npc.CraftMissCraftId = CraftId;
		Npc.CraftMissStampAt = Now;
		Npc.CraftMissAt = Now;
		return true;
	}

	void FVillage::ApplyCraftMissRecovery(FNpc& Npc, double Period)
	{
		if (!Npc.WorkSession.bActive) return;
		Npc.WorkSession.LastSwingAt = Now;
		Npc.WorkSession.NextSwingAt = Now + Period * AnastasisCraftMiss::RecoveryMul;
		Npc.WorkSession.SwingsDone += 1;
	}

	void FVillage::SocializeWithCompanion(FNpc& Npc, FNpc& Other)
	{
		namespace C = AnastasisNeeds::Constants;
		if (!CanStartTalk(Npc, Other))
		{
			// Trop parle ensemble : compagnie ambiante, sans nouvel echange ni moral +1.
			AnastasisNeeds::SatisfySocial(Npc.Needs, C::SocialAmbient, Npc.Inside.bActive);
			return;
		}
		const int32 Gain = BD::BondTalkGain(RelationOf(Npc, Other.Id) >= BD::FriendAt);
		BumpRelation(Npc, Other, Gain, FMath::Max(4, Gain - 1));
		AnastasisNeeds::SatisfySocial(Npc.Needs, C::SocialRelief + (Gain > 6 ? 6.0 : 0.0), Npc.Inside.bActive);
		Other.Needs.Social = Clamp(Other.Needs.Social + 12.0 + (Gain > 6 ? 4.0 : 0.0), 0.0, 100.0);
		Other.Needs.Morale = Clamp(Other.Needs.Morale + (Gain > 6 ? 2.0 : 1.0), 0.0, 100.0);
		// Echo de rumeur locale : toujours vide a ce commit. Actes de parole sur les gisements :
		// prepares avant la parole, deposes apres (`commitResourceSpotActs`).
		const double RA = Npc.Spots.Num() > 0 ? VillageRng.Next() : 0.0;
		const TArray<FSpotAct> AB = CreateInformSpotActs(Npc.Spots, Other.Spots, Npc.Id, RA, 2);
		const double RB = Other.Spots.Num() > 0 ? VillageRng.Next() : 0.0;
		const TArray<FSpotAct> BA = CreateInformSpotActs(Other.Spots, Npc.Spots, Other.Id, RB, 2);
		RecordTalk(Npc, Other, /*bContinue=*/false);
		const int32 Today = Day();
		for (const FSpotAct& Act : AB)
		{
			if (CommitHearsaySpot(Other.Spots, Act, Today, Now))
			{
				++Npc.RumorsShared;
				++Other.RumorsHeard;
			}
		}
		for (const FSpotAct& Act : BA)
		{
			if (CommitHearsaySpot(Npc.Spots, Act, Today, Now))
			{
				++Other.RumorsShared;
				++Npc.RumorsHeard;
			}
		}
		// `shareRumors(..., { resourceSpots: false })` : croyances non portees (ecart n°16).
		NoteMeeting(Npc, Other);
		++Npc.TalksWithCompanion;
	}

	bool FVillage::BondSocialTarget(FNpc& Npc, FPoint& InOutTarget)
	{
		const bool bLonely = Npc.Needs.Social <= BD::LonelySocialAt;
		const double Range = (bLonely || Npc.Goal == GoalSocialize) ? BD::SeekSocialRange : BD::SeekKinRange;
		TArray<FNpc>& Items = Actors.GetItemsMutable();
		const FNpc* BestMutual = nullptr;
		double BestMutualScore = -AnastasisNav::Infinity;
		const FNpc* BestKin = nullptr;
		double BestKinScore = -AnastasisNav::Infinity;
		const FString Living = Npc.LivingHomeId();
		Grid.ForEachNear(Npc.X, Npc.Y, Range, [&](int32 Index)
		{
			if (!Items.IsValidIndex(Index)) return;
			const FNpc& Other = Items[Index];
			if (&Other == &Npc) return;
			// Sans famille ni partenaire : seuls les amis comptent.
			if (RelationOf(Npc, Other.Id) < BD::FriendAt) return;
			const double D = Dist(Npc.X, Npc.Y, Other.X, Other.Y);
			// EXTENSION (player-minimal-001, ecart n°22) : presence 1, exactement `D > Range`.
			if (!Sees(Other, D, Range)) return;
			if (Other.Inside.bActive && Other.Inside.BuildingId != Living && !IsSociallyAvailable(Other)) return;
			const BD::FPersonRow* Row = BD::FindPerson(Npc.People, Other.Id);
			const double Affinity = BD::CompanionAffinity(RelationOf(Npc, Other.Id), Row ? Row->Trust : 0.0,
				Row ? Row->Tag : BD::EPersonTag::None, Npc.JobId == Other.JobId, IsSociallyAvailable(Other))
				+ ReputationAffinity(Other);
			if (IsSociallyAvailable(Other))
			{
				const double Score = Affinity + BD::MutualSocialBonus - D * 0.55;
				if (Score > BestMutualScore)
				{
					BestMutualScore = Score;
					BestMutual = &Other;
				}
			}
			else if (bLonely)
			{
				if (IsWorkingGoalForBonds(Other.Goal)) return;
				const double Score = Affinity - D * 0.8;
				if (Score > BestKinScore)
				{
					BestKinScore = Score;
					BestKin = &Other;
				}
			}
		});
		// `rendezvousPoint` : son batiment s'il est dedans, le milieu si les deux sont disponibles, sinon lui.
		auto Rendezvous = [&](const FNpc& Other)
		{
			if (Other.Inside.bActive)
			{
				FPoint Access;
				if (BuildingAccessPointById(Other.Inside.BuildingId, &Npc, Access)) InOutTarget = Access;
				return;
			}
			if (IsSociallyAvailable(Other) && IsSociallyAvailable(Npc))
			{
				InOutTarget = { (Npc.X + Other.X) * 0.5, (Npc.Y + Other.Y) * 0.5 };
				return;
			}
			InOutTarget = { Other.X, Other.Y };
		};
		if (BestMutual)
		{
			Rendezvous(*BestMutual);
			return true;
		}
		if (BestKin)
		{
			if (!BestKin->Inside.bActive) Rendezvous(*BestKin);
			else
			{
				FPoint Access;
				if (BuildingAccessPointById(BestKin->Inside.BuildingId, &Npc, Access)) InOutTarget = Access;
			}
			return true;
		}
		return false;
	}

	FString FVillage::PickRememberedSeekFor(const FNpc& Npc) const
	{
		return BD::PickRememberedSeek(Npc.People, Npc.Tom, Npc.Id, Npc.X, Npc.Y, Npc.Needs.Social,
			[&](const FString& Id, double& X, double& Y)
			{
				const FNpc* Other = Actors.FindById(Id);
				// EXTENSION (player-minimal-001, ecart n°22) : on ne va plus chercher quelqu'un qu'on ne voit plus.
				if (!Other || Other->Presence < Standing::RememberPresenceMin) return false;
				X = Other->X;
				Y = Other->Y;
				return true;
			});
	}

	bool FVillage::RememberedSocialTarget(FNpc& Npc, FPoint& InOutTarget)
	{
		const FString SeekId = PickRememberedSeekFor(Npc);
		if (SeekId.IsEmpty())
		{
			Npc.SocialSeekId.Reset();
			return false;
		}
		Npc.SocialSeekId = SeekId;
		const FNpc* Seek = Actors.FindById(SeekId);
		if (Seek->Inside.bActive)
		{
			FPoint Access;
			InOutTarget = BuildingAccessPointById(Seek->Inside.BuildingId, &Npc, Access) ? Access : FPoint{ Seek->X, Seek->Y };
			return true;
		}
		InOutTarget = { Seek->X, Seek->Y };
		return true;
	}

	void FVillage::ForgetStaleDaily(int32 DayNow)
	{
		for (FNpc& Npc : Actors.GetItemsMutable())
		{
			// `forgetStale` : un gisement jamais revu depuis 14 jours s'efface (on-dit compris,
			// avec l'age du souvenir d'origine). Les croyances de stock : non portees (ecart n°9).
			Npc.Spots.RemoveAll([&](const FResourceSpot& S) { return DayNow - S.Day > 14; });
			BD::ForgetStalePeople(Npc.People, DayNow);
		}
	}
	// --- Chantier (npc.js progressBuildWork, simulation.js workConstruction) -------

	bool FVillage::SetJob(const FString& NpcId, const FString& JobId)
	{
		FNpc* Npc = Actors.FindById(NpcId);
		if (!Npc) return false;
		if (JobId != AnastasisGather::JobSettler && JobId != AnastasisGather::JobFarmer && JobId != AnastasisBuild::JobBuilder && JobId != TEXT("woodcutter")) return false;
		Npc->JobId = JobId;
		return true;
	}

	FString FVillage::OpenSite(const FString& Type, int32 TileX, int32 TileY, bool bDelivered)
	{
		namespace B = AnastasisBuild;
		B::FBuildCost Base;
		if (!B::BaseCost(Type, Base)) return FString();
		// `buildCost(type)` : le multiplicateur compte les acheves du type, avant l'ajout.
		const B::FBuildCost Cost = B::BuildCost(Type, CountBuildings(Type));
		const FString Id = AddBuilding(Type, TileX, TileY, 0.0, Day());
		if (Id.IsEmpty()) return Id;
		FBuilding* Site = Buildings.FindById(Id);
		Site->PiecesPlaced = 0;
		Site->bHasMaterials = true;
		Site->Materials.NeedWood = Cost.Wood;
		Site->Materials.NeedStone = Cost.Stone;
		if (bDelivered)
		{
			CreditSiteMaterials(Id, Cost.Wood, Cost.Stone);
		}
		return Id;
	}

	TArray<const FBuilding*> FVillage::ActiveSites() const
	{
		TArray<const FBuilding*> Sites;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Progress >= 1.0) continue;
			Sites.Add(&B);
		}
		return Sites;
	}

	int32 FVillage::CreditSiteMaterials(const FString& BuildingId, int32 Wood, int32 Stone)
	{
		namespace B = AnastasisBuild;
		FBuilding* Site = Buildings.FindById(BuildingId);
		if (!Site || Site->Progress >= 1.0) return 0;
		// `creditStock` : borne par la capacite du profil de chantier.
		const int32 W = FMath::Min(FMath::Max(0, Wood), FMath::Max(0, B::SiteWoodCap - Site->Materials.StockWood));
		const int32 S = FMath::Min(FMath::Max(0, Stone), FMath::Max(0, B::SiteStoneCap - Site->Materials.StockStone));
		Site->Materials.StockWood += W;
		Site->Materials.StockStone += S;
		return W + S;
	}

	FString FVillage::SessionGoalOf(const FNpc& Npc)
	{
		if (!Npc.WorkSession.bActive) return FString();
		if (Npc.WorkSession.CraftId == TEXT("farm")) return GoalGatherFood;
		if (Npc.WorkSession.CraftId == TEXT("chop")) return TEXT("gatherWood");
		if (Npc.WorkSession.CraftId == AnastasisBuild::CraftBuild) return AnastasisBuild::GoalBuild;
		return FString();
	}

	bool FVillage::SitePieceReady(const FBuilding& Site)
	{
		// `siteCanPlacePiece` : sans devis, toujours posable.
		return !Site.bHasMaterials || AnastasisBuild::SiteCanPlacePiece(Site.Materials, Site.PiecesPlaced);
	}

	FBuilding* FVillage::BoundBuildSite(FNpc& Npc)
	{
		if (Npc.BuildBinding.IsEmpty()) return nullptr;
		FBuilding* Site = Buildings.FindById(Npc.BuildBinding);
		// Le lien meurt avec son objet : chantier disparu ou acheve.
		if (!Site || Site->Progress >= 1.0)
		{
			Npc.BuildBinding.Reset();
			return nullptr;
		}
		return Site;
	}

	FBuilding* FVillage::PickBuildSite(FNpc& Npc)
	{
		const bool bFarmDone = CountBuildings(TEXT("farm")) > 0;
		FBuilding* Best = nullptr;
		double BestScore = -AnastasisNav::Infinity;
		for (FBuilding& Site : Buildings.GetItemsMutable())
		{
			if (Site.Progress >= 1.0) continue;
			FPoint Access{ Site.X + 0.5, Site.Y + 0.5 };
			BuildingAccessPoint(Site, &Npc, Access);
			const double D = Dist(Npc.X, Npc.Y, Access.X, Access.Y);
			const bool bSessionHere = Npc.WorkSession.bActive && !Npc.WorkSession.BuildingId.IsEmpty() && Npc.WorkSession.BuildingId == Site.Id;
			const double Score = AnastasisBuild::SiteScore(D, bSessionHere, Site.BuilderId == Npc.Id, SitePieceReady(Site),
				bFarmDone, Site.Type == TEXT("farm"));
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = &Site;
			}
		}
		return Best;
	}

	bool FVillage::ConstructionAccessPoint(FNpc& Npc, FPoint& OutTarget)
	{
		// `activeConstruction()` : le premier chantier du tableau, pas le mieux note.
		for (FBuilding& Site : Buildings.GetItemsMutable())
		{
			if (Site.Progress >= 1.0) continue;
			if (!BuildingAccessPoint(Site, &Npc, OutTarget)) return false;
			Npc.DestBuildingId = Site.Id;
			return true;
		}
		return false;
	}

	void FVillage::EnsureBuildSession(FNpc& Npc, const FBuilding& Site)
	{
		namespace B = AnastasisBuild;
		const FWorkSession& Existing = Npc.WorkSession;
		if (Existing.bActive && Existing.CraftId == B::CraftBuild && Existing.BuildingId == Site.Id)
		{
			return;
		}
		// `craftToolSwitchSeconds(from, "build")` : un autre metier en main se range d'abord.
		const FString From = Existing.bActive && Existing.CraftId != B::CraftBuild ? Existing.CraftId : FString();
		const double SwitchUntil = Now + B::CraftToolSwitchSeconds(From, B::CraftBuild);
		FWorkSession Session;
		Session.bActive = true;
		Session.CraftId = B::CraftBuild;
		Session.TileX = static_cast<int32>(Site.X);
		Session.TileY = static_cast<int32>(Site.Y);
		Session.BuildingId = Site.Id;
		Session.ArrivedAt = Now;
		Session.NextSwingAt = SwitchUntil + B::ArriveSeconds;
		Npc.WorkSession = Session;
		// `noteShiftArrival(sim, npc)` : arrivee physique au travail, COMMUTING -> ON_SHIFT.
		AnastasisWorkShift::NoteShiftArrival(Npc.WorkShift);
	}

	bool FVillage::WorkConstruction(FBuilding& Site, FNpc& Npc)
	{
		namespace B = AnastasisBuild;
		if (Site.Progress >= 1.0) return false;
		if (Site.bHasMaterials && !B::ConsumeSiteMaterials(Site.Materials, Site.PiecesPlaced))
		{
			// `requestSiteDeliveries` : livraisons non portees (ecart n°18), le chantier attend.
			return false;
		}
		TPair<FString, int32>* Worker = Site.Workers.FindByPredicate([&](const TPair<FString, int32>& W) { return W.Key == Npc.Id; });
		if (Worker) ++Worker->Value;
		else Site.Workers.Add(TPair<FString, int32>(Npc.Id, 1));
		// Un appel = une piece posee. Plus de flottant.
		const bool bPiece = B::PlaceConstructionPiece(Site.PiecesPlaced, Site.Progress);
		++Npc.PiecesPlaced;
		Npc.Needs.Morale = Clamp(Npc.Needs.Morale + B::PieceMorale, 0.0, 100.0);
		if (!bPiece) return false;
		if (Site.Progress < 1.0) return true;

		// Acheve. Episode, reputation, annonce, postes, restitution du reliquat : non portes.
		Site.CompletedDay = Day();
		Site.CompletedById = Npc.Id;
		++Npc.BuildingsCompleted;
		Npc.Needs.Morale = Clamp(Npc.Needs.Morale + B::CompletionMorale, 0.0, 100.0);
		return true;
	}

	int32 FVillage::ProgressBuildWork(FNpc& Npc, double Dt)
	{
		namespace B = AnastasisBuild;
		// `tryOpenNewConstruction` : l'ouverture n'est pas portee (ecart n°18), aucun creneau.
		// Preference, pas epinglage : on garde l'objet de l'intention tant qu'il est posable.
		FBuilding* Bound = BoundBuildSite(Npc);
		FBuilding* Site = (Bound && SitePieceReady(*Bound)) ? Bound : PickBuildSite(Npc);
		if (Site) Npc.BuildBinding = Site->Id;
		if (!Site)
		{
			// `tryBuild` : rien a ouvrir, aucun chantier a travailler.
			return 0;
		}
		FPoint SiteTarget{ Site->X + 0.5, Site->Y + 0.5 };
		BuildingAccessPoint(*Site, &Npc, SiteTarget);
		Npc.Target = SiteTarget;
		Npc.bHasTarget = true;
		if (Dist(Npc.X, Npc.Y, SiteTarget.X, SiteTarget.Y) > B::SiteReachDistance)
		{
			SetActivity(Npc, TEXT("chantier"));
			MoveActor(Npc, SiteTarget, Dt);
			return 1;
		}
		EnsureBuildSession(Npc, *Site);
		SetActivity(Npc, TEXT("chantier"));
		if (Now < Npc.WorkSession.NextSwingAt) return 1;

		// Piece mal calee : swing visible, pas de pose, reprise (`rollCraftMiss(sim, npc, "build")`).
		if (RollCraftMiss(Npc, TEXT("build")))
		{
			ApplyCraftMissRecovery(Npc, B::SwingPeriod(Npc.Skill, Npc.WorkSession.SwingsDone, Npc.Needs.Energy));
			return 1;
		}
		// `markCraftSwing` :
		FWorkSession& S = Npc.WorkSession;
		S.SwingsDone += 1;
		S.ActionAcc += 1;
		S.LastSwingAt = Now;
		S.NextSwingAt = Now + B::SwingPeriod(Npc.Skill, S.SwingsDone, Npc.Needs.Energy);
		if (S.ActionAcc < B::SwingsPerAction) return 1;
		S.ActionAcc = 0;

		const bool bOk = WorkConstruction(*Site, Npc);
		// `gainSkill(npc, 0.004)` : domaine du but `build`, la main (craft).
		if (bOk) AnastasisGather::GainDomainSkill(Npc.Skill, Npc.SkillCraft, B::BuildSkillGain);
		if (bOk && Site->Progress >= 1.0)
		{
			ClearWorkSession(Npc);
			return 2;
		}
		if (!bOk)
		{
			// Croyance de danger et memoire d'echec : non portees. Materiaux incomplets :
			// casser le collant et basculer vers un chantier pret.
			Npc.WorkSession.BuildingId.Reset();
			FBuilding* Alt = PickBuildSite(Npc);
			if (Alt && Alt->Id != Site->Id && SitePieceReady(*Alt))
			{
				FPoint AltTarget{ Alt->X + 0.5, Alt->Y + 0.5 };
				BuildingAccessPoint(*Alt, &Npc, AltTarget);
				Npc.Target = AltTarget;
				return 1;
			}
			ClearWorkSession(Npc);
			return 2;
		}
		return 1;
	}

	double FVillage::BuildRowScore(const FNpc& Npc, double PhaseBias, const FWorkRowContext& Work, double Noise,
		const FCollectiveDecision& Collective) const
	{
		namespace B = AnastasisBuild;
		namespace G = AnastasisGather;
		const G::FTrait& Trait = G::TraitAt(Npc.TraitIndex);
		// `buildScore` : avec une colonie, le besoin et la liquidite du planificateur, puis
		// `planBias` (nul, ecart n°24), `colonizationBuildBias` et `colonySiteBuildBias` (nuls, ecart n°27),
		// et le biais collectif. Sans colonie, le chantier ouvert du village cree par le C++.
		double Base = 0.0;
		if (!Collective.bHasColony) Base = B::BuildScoreActiveSite(Trait.Build, Npc.JobId);
		else if (!Collective.bBuildIdle)
		{
			Base = B::BuildScoreFromNeed(Collective.BuildNeedTerm, Trait.Build, Npc.JobId)
				+ 0.0  // planBias
				+ 0.0  // colonizationBuildBias
				+ 0.0  // colonySiteBuildBias
				+ Collective.BiasOf(B::GoalBuild);
		}
		// `(buildScore + goalNoise(sim, 14)) * wf("build")` : le bruit est tire par ChooseGoal, a sa place dans la table.
		double Score = (Base + Noise) * G::SurvivalWorkFactor(B::GoalBuild, Work.WorkFactor, Work.bMealBlocked);
		// Les biais, dans l'ordre d'adultScores. Le poste (grenier) ne pese pas sur `build` ;
		// statut, age, district, foyer, episodes, scenes, humeur, plans, meteo, nature
		// (moyenne), memoire sociale et d'echec, ordres, prevision, risque, urgence : nuls.
		Score += PhaseBias;
		const int32 DepotLoad = IsGranaryWorker(Npc) ? Npc.InventoryFood : 0;
		Score += G::CompletionBias(B::GoalBuild, Npc.InventoryFood, DepotLoad, SessionGoalOf(Npc), NeedsCritical(Npc.Needs));
		Score += G::TraitGoalBias(Trait, B::GoalBuild);
		Score += G::SkillGoalBias(Npc.SkillCraft);
		return Score;
	}
}
