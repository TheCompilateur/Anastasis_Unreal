#include "Village/AnastasisVillage.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"
#include "Core/AnastasisStateDigest.h"
#include "World/AnastasisWorld.h"

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
			return Type == WellType || Type == HouseType;
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
		using namespace AnastasisNeeds::Constants;
		return N.Hunger >= HungerCritical
			|| N.Energy <= 100.0 - FatigueCritical
			|| N.Social <= 100.0 - LonelyCritical
			|| N.Leisure <= 100.0 - BoredCritical
			|| N.Hygiene <= 100.0 - HygieneCritical
			|| N.Thirst >= ThirstCritical
			|| N.Health <= HealthCritical
			|| N.Morale < MoraleCritical;
	}

	int32 HousingOfType(const FString& Type)
	{
		// sim/batiments/catalog.js : house `housing: 3`. Le puits n'abrite personne.
		if (Type == HouseType) return 3;
		return 0;
	}

	const TArray<FString>& UnportedGoals()
	{
		// Table adulte de npc.js (adultScores), moins `rest` et `drink`.
		static const TArray<FString> Goals = {
			TEXT("eat"), TEXT("eatTogether"), TEXT("relax"), TEXT("relieve"),
			TEXT("gatherWood"), TEXT("gatherStone"), TEXT("gatherFood"), TEXT("helpFarm"),
			TEXT("sell"), TEXT("buy"), TEXT("build"), TEXT("craft"), TEXT("maintain"), TEXT("deliver"),
			TEXT("fetchInput"), TEXT("haulJob"), TEXT("aidHousehold"), TEXT("visitFamily"), TEXT("explore"),
			TEXT("socialize"), TEXT("confront"), TEXT("shelterRain"), TEXT("closeWorkplace"),
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
		NavVersion = 0;
		Settlement = { InWorld.W * 0.5, InWorld.H * 0.5 };
		NextBuildingId = 0;
		NextNpcId = 0;
		Now = 0.0;
		Buildings = TAnastasisEntityTable<FBuilding>();
		Actors = TAnastasisEntityTable<FNpc>();
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

		Nav.Blocked[Index] = 1;
		Nav.MoveCost[Index] = std::numeric_limits<float>::infinity();
		// `bumpNavVersion` + `clearNavCache` : les chemins qui traversaient la case sont perimes.
		++NavVersion;

		Building.AccessPoints = ComputeBuildingAccessPoints(Building);
		const FString Id = Building.Id;
		Buildings.Add(MoveTemp(Building));
		return Id;
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
			if (bLostPlace)
			{
				ClearNavigation(Npc);
				Npc.bHasTarget = false;
				Npc.DestBuildingId.Reset();
				Npc.Goal = GoalObserver;
				Npc.WorkTimer = 0.0;
				Npc.DoorStuckAt = 0.0;
				Npc.DoorApproachAt = 0.0;
				Npc.Activity = TEXT("attend");
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

	const FBuilding* FVillage::BuildingForIndoorAction(const FNpc& Npc) const
	{
		// Foyer d'abord : dormir se fait DANS la maison ou l'abri.
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
		// `if (goal === "rest" || goal === "relieve") return afford(atOwn || data.housing)`.
		// AUCUN controle de proprietaire ni de capacite : c'est la reference.
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

	bool FVillage::TryEnterIndoorAction(FNpc& Npc)
	{
		if (Npc.Inside.bActive || Npc.Goal != GoalRest) return false;
		const FBuilding* Building = NearLivingHome(Npc);
		if (!Building) Building = BuildingForIndoorAction(Npc);
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
		// `indoorNeedDuration(rest)` : la nuit se decide UNE fois, a l'entree.
		const bool bNight = IsNight();
		const double Duration = bNight ? AnastasisNeeds::Constants::SleepDuration : AnastasisNeeds::Constants::NapDuration;
		return EnterBuilding(Npc, *Building, RestActivity(bNight), Duration);
	}

	void FVillage::UpdateInside(FNpc& Npc)
	{
		Npc.Activity = Npc.Inside.Activity;
		if (Now < Npc.Inside.Until) return;
		const bool bWorked = Perform(Npc);
		ExitBuilding(Npc);
		if (!bWorked)
		{
			Npc.Activity = TEXT("attend");
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
		const FString Id = Npc.Id;
		Actors.Add(MoveTemp(Npc));
		return Id;
	}

	bool FVillage::RemoveNpc(const FString& Id)
	{
		if (!Actors.FindById(Id))
		{
			return false;
		}
		// Un proprietaire qui disparait libere sa maison : la reference le fait a la
		// mort (mortality.js) ; sans cela la maison resterait close pour toujours.
		for (FBuilding& B : Buildings.GetItemsMutable())
		{
			if (B.Owner == Id) B.Owner.Reset();
		}
		return Actors.RemoveById(Id);
	}

	void FVillage::UpdateActors(double Time, double Dt)
	{
		if (!World)
		{
			return;
		}
		Now = Time;
		TArray<FNpc>& Items = Actors.GetItemsMutable();
		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			UpdateNpc(Items[Index], Dt);
		}
	}

	bool FVillage::IsNight() const
	{
		return AnastasisRhythm::IsNightPhase(Now);
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
		if (Npc.Inside.bActive && Npc.Inside.Goal == GoalRest)
		{
			AnastasisNeeds::TickNeedsRestInside(Npc.Needs, Dt, IsNight(), SleepQualityOf(Npc));
		}
		else
		{
			const bool bDrinking = Npc.Goal == GoalDrink && !Npc.Inside.bActive && AtDrinkSpot(Npc.X, Npc.Y);
			AnastasisNeeds::TickNeeds(Npc.Needs, Dt, bDrinking, /*bWorking=*/false);
		}

		// Dedans : ni pensee, ni marche, seulement le temps qui passe.
		if (Npc.Inside.bActive)
		{
			UpdateInside(Npc);
			return;
		}

		const bool bCritical = NeedsCritical(Npc.Needs);
		if (Npc.AiThinkAt < 0.0)
		{
			Npc.AiThinkAt = Now + AiThinkStagger(Npc.Id);
		}
		if (Now >= Npc.AiThinkAt)
		{
			Npc.AiThinkAt = Now + (bCritical ? ThinkEveryCritical : ThinkEvery);
			// `perceive` n'est pas porte (croyances). Reconsideration seulement sans cible (ecart n°2).
			if (!Npc.bHasTarget)
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

	void FVillage::ChooseGoal(FNpc& Npc)
	{
		const AnastasisRhythm::EPhase Phase = AnastasisRhythm::VillagePhase(AnastasisRhythm::DayFracOf(Now));
		const AnastasisRhythm::FPhaseSubject Subject = PhaseSubjectOf(Npc);
		const int32 Wells = CountBuildings(WellType);

		FDecisionTrace Trace;
		Trace.Time = Now;
		Trace.Phase = AnastasisRhythm::PhaseId(Phase);
		Trace.NeedScores = AnastasisNeeds::NeedGoalScores(Npc.Needs, Wells, /*CompletedTaverns=*/0);
		// Lignes de adultScores, puis `rhythmBias` = `phaseBias` (ecart n°1 pour le reste).
		Trace.RestRowScore = Trace.NeedScores.Rest + RestJobPriorityBias
			+ AnastasisRhythm::PhaseBias(Phase, Subject, GoalRest);
		Trace.DrinkRowScore = Trace.NeedScores.Drink + (Wells > 0 ? 6.0 : 0.0)
			+ AnastasisRhythm::PhaseBias(Phase, Subject, GoalDrink);
		Trace.FloorScore = -AnastasisNav::Infinity;
		for (const FString& Goal : UnportedGoals())
		{
			const double Score = UnportedGoalsFloor + AnastasisRhythm::PhaseBias(Phase, Subject, Goal);
			if (Score > Trace.FloorScore)
			{
				Trace.FloorScore = Score;
				Trace.FloorGoal = Goal;
			}
		}

		// Ordre de la table : rest avant drink ; un but porte ne l'emporte que
		// s'il depasse strictement le meilleur but non porte.
		FString Wanted;
		double Best = Trace.FloorScore;
		if (Trace.RestRowScore > Best)
		{
			Best = Trace.RestRowScore;
			Wanted = GoalRest;
		}
		if (Trace.DrinkRowScore > Best)
		{
			Best = Trace.DrinkRowScore;
			Wanted = GoalDrink;
		}

		Npc.DestBuildingId.Reset();
		if (!Wanted.IsEmpty())
		{
			FPoint Target;
			FString Source;
			const bool bFound = Wanted == GoalDrink
				? DrinkTarget(Npc, Target, Source)
				: RestTarget(Npc, Target, Source);
			Trace.TargetSource = Source;
			if (bFound)
			{
				if (Npc.Goal != Wanted) Npc.WorkTimer = 0.0;
				Npc.Goal = Wanted;
				Npc.bHasTarget = true;
				Npc.Target = Target;
				Npc.DoorStuckAt = 0.0;
				Npc.DoorApproachAt = 0.0;
				Trace.Winner = Wanted;
				Trace.BuildingId = Npc.DestBuildingId;
				Npc.LastDecision = MoveTemp(Trace);
				return;
			}
		}

		Npc.Goal = GoalObserver;
		Npc.bHasTarget = false;
		Trace.Winner = GoalObserver;
		Npc.LastDecision = MoveTemp(Trace);
	}

	void FVillage::RedirectAfterFailure(FNpc& Npc)
	{
		ClearNavigation(Npc);
		Npc.bHasTarget = false;
		Npc.DestBuildingId.Reset();
		Npc.Goal = GoalObserver;
		Npc.WorkTimer = 0.0;
		Npc.FailedActions = 0;
		Npc.StuckStage = 0;
		Npc.DoorStuckAt = 0.0;
		Npc.DoorApproachAt = 0.0;
		Npc.Activity = TEXT("attend");
	}

	void FVillage::RedirectDomesticDoorFailure(FNpc& Npc)
	{
		// Porte inaccessible : le besoin se fait dehors, puis `explore` (non porte : observer).
		Npc.DoorStuckAt = 0.0;
		Npc.DoorApproachAt = 0.0;
		ClearNavigation(Npc);
		Npc.PathCooldown = 0.6;
		if (Npc.Goal == GoalRest || Npc.Goal == GoalDrink)
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

	void FVillage::Act(FNpc& Npc, double Dt)
	{
		if (Npc.bHasTarget && !ReachedMoveTarget(Npc, Npc.Target))
		{
			Npc.Activity = TEXT("marche");
			MoveActor(Npc, Npc.Target, Dt);
			return;
		}

		if (Npc.Goal == GoalRest)
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
				Npc.Activity = RestActivity(IsNight());
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

		if (Npc.Goal != GoalDrink && Npc.Goal != GoalRest)
		{
			// But non porte : il n'accomplit rien. La reference ferait `perform`.
			Npc.Activity = TEXT("attend");
			Npc.WorkTimer = 0.0;
			return;
		}

		Npc.WorkTimer += Dt;
		if (Npc.WorkTimer < 1.0)
		{
			// `waitingActivity` : « boit » ; pour rest, `restActivity`.
			Npc.Activity = Npc.Goal == GoalDrink ? TEXT("boit") : RestActivity(IsNight());
			return;
		}
		Npc.WorkTimer = 0.0;
		const bool bWorked = Perform(Npc);
		if (!bWorked)
		{
			Npc.Activity = TEXT("attend");
			++Npc.FailedActions;
		}
		else
		{
			Npc.FailedActions = 0;
		}
		// `if (npc.target === target) npc.target = null;` — force une nouvelle decision.
		Npc.bHasTarget = false;
		ClearNavigation(Npc);
	}

	bool FVillage::Perform(FNpc& Npc)
	{
		if (Npc.Goal == GoalDrink)
		{
			// `case "drink"` : setActivity("boit"), satisfyDrink, markDrink (gestuelle, non portee).
			Npc.Activity = TEXT("boit");
			AnastasisNeeds::SatisfyDrink(Npc.Needs);
			++Npc.DrinksTaken;
			return true;
		}
		if (Npc.Goal == GoalRest)
		{
			// `case "rest"` : setActivity(restActivity), satisfyRest.
			const bool bNight = IsNight();
			const FString& Living = Npc.LivingHomeId();
			const bool bAtHome = !Living.IsEmpty() && Npc.Inside.bActive && Npc.Inside.BuildingId == Living;
			Npc.Activity = RestActivity(bNight);
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
		double Budget = Npc.Speed * FMath::Max(0.0, Dt);
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
			const double Step = FMath::Min(Len, Budget);
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
			Budget -= MovedSeg;
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
		if (Npc.Goal == GoalRest && !Npc.LivingHomeId().IsEmpty())
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
			const bool bHeading = (Npc.Goal == GoalDrink || Npc.Goal == GoalRest) && Npc.DestBuildingId == BuildingId;
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
			Writer.EndObject();
		}
		Writer.EndArray();
		Writer.EndObject();
		return Writer.Digest();
	}
}
