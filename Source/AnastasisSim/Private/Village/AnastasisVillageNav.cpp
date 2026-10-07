// nav-wiring-001 -- la marche : le service de navigation branche sur le village, et le pas.
//
// Port de `simulation.js` (`nextWaypoint`, `moveActor`, `resolveStuckActor`, `steerAroundBlock`,
// `movementSpeedFactor`, `recordPassage`), de `crowdNav.js` (`resolveDoorQueue`, `doorQueueWaypoint`) et de
// `navGrid.js` (`ensureNavigation`, `syncNavigationFromActor`, `buildingForAccessTarget`). Le service lui-meme
// (`navService.js` : file budgetee, cache exact et de zone) est `World/AnastasisNavService`.
//
// Le service travaille sur une VUE de l'habitant (`FNavAgent`). Le village en est l'hote : il remplit la vue
// avant l'appel, et recopie a la sortie (`FlushNavAgents`) ce que le service a ecrit. Quand le service a pose
// un chemin (`applyPathToActor`), il a aussi resynchronise `navigation.path` / `pathIndex` : la recopie le fait.
//
// Reduit : dans `movementSpeedFactor`, la famille (pas de familles, ecart n° 7), les routes (pas de routes) et
// les quartiers (pas de quartiers, ecart n° 3) valent 1. `separateCrowdedActors` reste masque (ecart n° 4).

#include "Village/AnastasisVillage.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"
#include "Life/AnastasisLifestyle.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Life/AnastasisWeatherBehavior.h"

#include <cmath>

namespace AnastasisVillage
{
	using AnastasisMath::Dist;
	using AnastasisMath::JsHypot;

	namespace
	{
		int32 NavFloor(double V) { return static_cast<int32>(AnastasisJs::Floor(V)); }

		/** `clamp` de util.js. */
		double NavClamp(double V, double Min, double Max) { return FMath::Max(Min, FMath::Min(Max, V)); }

		/** `hashText01(text, salt)` de simulation.js (variante locale, distincte de `HashText`). */
		double SimHashText01(const FString& Text, double Salt)
		{
			uint32 Hash = AnastasisJs::ToUint32(Salt * 2654435761.0);
			for (const TCHAR C : Text)
			{
				Hash ^= static_cast<uint32>(C);
				Hash = Hash * 16777619u;
			}
			return static_cast<double>(Hash ^ (Hash >> 16)) / 4294967295.0;
		}

		/** `DOOR_QUEUE` (crowdNav.js). */
		namespace DoorQueue
		{
			constexpr double ClaimRadius = 2.9;
			constexpr double OccupyRadius = 0.72;
			constexpr double WaitDistance = 1.4;
			constexpr double WaitSlot = 0.32;
		}

		const TCHAR* const RoleSolo = TEXT("solo");
		const TCHAR* const RoleLeader = TEXT("leader");
		const TCHAR* const RoleWaiter = TEXT("waiter");
	}

	// --- Hote du service ---------------------------------------------------------

	const AnastasisPath::INavSource& FVillage::GetNavSource() const
	{
		if (!NavSourceShared.IsValid())
		{
			NavSourceShared = MakeShared<AnastasisPath::FWorldNavSource>(Nav, *World);
		}
		return *NavSourceShared;
	}

	AnastasisNavService::FNavAgent* FVillage::FindLiveAgent(const FString& Id)
	{
		if (AnastasisNavService::FNavAgent* Known = NavAgents.Find(Id)) return Known;
		const FNpc* Npc = Actors.FindById(Id);
		if (!Npc) return nullptr;
		AnastasisNavService::FNavAgent& Agent = NavAgents.Add(Id);
		FillNavAgent(*Npc, Agent);
		return &Agent;
	}

	void FVillage::FillNavAgent(const FNpc& Npc, AnastasisNavService::FNavAgent& Agent)
	{
		Agent.Id = Npc.Id;
		Agent.X = Npc.X;
		Agent.Y = Npc.Y;
		Agent.Goal = Npc.Goal;
		Agent.Hunger = Npc.Needs.Hunger;
		Agent.Thirst = Npc.Needs.Thirst;
		Agent.Energy = Npc.Needs.Energy;
		Agent.bHasTarget = Npc.bHasTarget;
		Agent.Target = Npc.Target;
		Agent.Path = Npc.Path;
		Agent.PathStep = Npc.PathStep;
		Agent.bHasPathGoal = Npc.bHasPathGoal;
		Agent.PathGoal = Npc.PathGoal;
		Agent.bPathFailed = Npc.bPathFailed;
		Agent.PathFailStreak = Npc.PathFailStreak;
		Agent.PathCooldown = Npc.PathCooldown;
		Agent.NavTargetKey = Npc.NavTargetKey;
		Agent.NavRequestedAt = Npc.NavRequestedAt;
		Agent.NavVersion = Npc.NavVersion;
		Agent.ApplyCount = 0;
	}

	void FVillage::WriteBackNavAgent(const AnastasisNavService::FNavAgent& Agent, FNpc& Npc)
	{
		Npc.Path = Agent.Path;
		Npc.PathStep = Agent.PathStep;
		Npc.bHasPathGoal = Agent.bHasPathGoal;
		Npc.PathGoal = Agent.PathGoal;
		Npc.bPathFailed = Agent.bPathFailed;
		Npc.PathFailStreak = Agent.PathFailStreak;
		Npc.PathCooldown = Agent.PathCooldown;
		Npc.NavTargetKey = Agent.NavTargetKey;
		Npc.NavRequestedAt = Agent.NavRequestedAt;
		Npc.NavVersion = Agent.NavVersion;
		// `applyPathToActor` finit par `syncNavigationFromActor(actor)`.
		if (Agent.ApplyCount > 0) SyncNavigationFromActor(Npc);
	}

	void FVillage::FlushNavAgents()
	{
		for (const TPair<FString, AnastasisNavService::FNavAgent>& Entry : NavAgents)
		{
			if (FNpc* Npc = Actors.FindById(Entry.Key)) WriteBackNavAgent(Entry.Value, *Npc);
		}
		NavAgents.Reset();
	}

	bool FVillage::RequestPathFor(FNpc& Npc, const FPoint& Target, bool bAllowBlockedTarget)
	{
		NavAgents.Reset();
		NavAgents.Reserve(Actors.Num());
		AnastasisNavService::FNavAgent& Agent = NavAgents.Add(Npc.Id);
		FillNavAgent(Npc, Agent);
		AnastasisNavService::FRequestOptions Options;
		Options.bAllowBlockedTarget = bAllowBlockedTarget;
		const bool bPath = NavService.RequestPath(*this, Agent, Target, Options, nullptr);
		FlushNavAgents();
		return bPath;
	}

	void FVillage::ProcessNavQueue()
	{
		NavAgents.Reset();
		NavAgents.Reserve(Actors.Num());
		NavService.ProcessNavQueue(*this);
		FlushNavAgents();
	}

	void FVillage::RestoreNavigationForHarness(int32 InNavVersion,
		const TArray<TPair<FString, AnastasisNavService::FNavCacheEntry>>& InCache)
	{
		// `sim.navVersion = data.navVersion | 0`, puis le cache filtre sur cette version.
		NavVersion = InNavVersion;
		NavService.RestoreCache(InCache, NavVersion);
	}

	// --- navGrid.js ----------------------------------------------------------------

	void FVillage::SyncNavigationFromActor(FNpc& Npc)
	{
		Npc.NavPath = Npc.Path;
		Npc.NavPathIndex = Npc.PathStep;
	}

	void FVillage::DropPath(FNpc& Npc, double Cooldown)
	{
		Npc.Path.Reset();
		Npc.PathCooldown = Cooldown;
	}

	// --- Le chemin ------------------------------------------------------------------

	FPoint FVillage::NextWaypoint(FNpc& Npc, const FPoint& Target, double Dt)
	{
		Npc.PathCooldown = FMath::Max(0.0, Npc.PathCooldown - Dt);
		const int32 WorldNav = NavVersion;
		const FString GoalKey = AnastasisNavService::NavigationTargetKey(Target);
		const bool bHasStep = Npc.Path.IsValidIndex(Npc.PathStep);
		const bool bVersionStale = Npc.NavVersion != WorldNav;
		const bool bTargetChanged = Npc.NavTargetKey != GoalKey;
		const bool bStepBlocked = bHasStep && IsFootBlocked(Npc.Path[Npc.PathStep].X, Npc.Path[Npc.PathStep].Y);
		const bool bNeedsPath =
			Npc.Path.Num() == 0
			|| Npc.PathStep >= Npc.Path.Num()
			|| !Npc.bHasPathGoal
			|| Dist(Npc.PathGoal.X, Npc.PathGoal.Y, Target.X, Target.Y) > 1.2
			|| bVersionStale
			|| bTargetChanged
			|| bStepBlocked;
		if (bNeedsPath && Npc.PathCooldown <= 0.0)
		{
			const bool bTargetBlocked = IsFootBlocked(Target.X, Target.Y);
			const bool bPath = RequestPathFor(Npc, Target, bTargetBlocked);
			if (bPath)
			{
				Npc.bAwaitingPath = false;
			}
			else if (Npc.Path.Num() > 0 && Npc.NavTargetKey == GoalKey && Npc.NavVersion == WorldNav)
			{
				// `requestPath` a applique via la file dans le meme appel.
				Npc.bAwaitingPath = false;
			}
			else
			{
				// En file : on contourne localement sans relancer A* chaque frame.
				Npc.bAwaitingPath = true;
				Npc.NavTargetKey = GoalKey;
				Npc.NavRequestedAt = Now;
				Npc.bHasPathGoal = true;
				Npc.PathGoal = Target;
				Npc.bPathFailed = false;
				Npc.PathCooldown = FMath::Max(0.12, Dt * 2.0);
			}
			SyncNavigationFromActor(Npc);
		}
		const FPoint Here = { Npc.X, Npc.Y };
		if (Npc.Path.IsValidIndex(Npc.PathStep))
		{
			const FPoint Point = Npc.Path[Npc.PathStep];
			if (IsFootBlocked(Point.X, Point.Y) && !IsFootBlocked(Npc.X, Npc.Y))
			{
				Npc.PathStep = Npc.Path.Num();
				SyncNavigationFromActor(Npc);
				return Here;
			}
			if (Dist(Npc.X, Npc.Y, Point.X, Point.Y) < 0.5)
			{
				++Npc.PathStep;
				SyncNavigationFromActor(Npc);
				const FPoint Next = Npc.Path.IsValidIndex(Npc.PathStep) ? Npc.Path[Npc.PathStep] : Target;
				if (IsFootBlocked(Next.X, Next.Y) && !IsFootBlocked(Npc.X, Npc.Y))
				{
					Npc.PathStep = Npc.Path.Num();
					SyncNavigationFromActor(Npc);
					return Here;
				}
				return Next;
			}
			return Point;
		}
		if (Npc.bAwaitingPath || Npc.bPathFailed || (Npc.Path.Num() == 0 && Npc.PathCooldown > 0.0))
		{
			return SteerAroundBlock(Npc, Target);
		}
		if (IsFootBlocked(Target.X, Target.Y) && !IsFootBlocked(Npc.X, Npc.Y)) return Here;
		return Target;
	}

	FPoint FVillage::SteerAroundBlock(const FNpc& Npc, const FPoint& Target) const
	{
		const double TX = Target.X - Npc.X;
		const double TY = Target.Y - Npc.Y;
		double Len = JsHypot(TX, TY);
		if (Len == 0.0 || FMath::IsNaN(Len)) Len = 1.0;
		const double DirX = TX / Len;
		const double DirY = TY / Len;
		const FPoint Candidates[] = {
			{ Npc.X + DirX, Npc.Y + DirY },
			{ Npc.X + DirX, Npc.Y },
			{ Npc.X, Npc.Y + DirY },
			{ Npc.X - DirY, Npc.Y + DirX },
			{ Npc.X + DirY, Npc.Y - DirX },
			{ Npc.X - DirX * 0.5, Npc.Y + DirY },
			{ Npc.X + DirX, Npc.Y - DirY * 0.5 },
		};
		bool bFound = false;
		FPoint Best;
		double BestScore = -AnastasisNav::Infinity;
		for (const FPoint& C : Candidates)
		{
			const double CX = NavClamp(C.X, 1.0, Nav.W - 2);
			const double CY = NavClamp(C.Y, 1.0, Nav.H - 2);
			// Jamais entrer dans un obstacle pied (meme proche de la cible).
			if (IsFootBlocked(CX, CY)) continue;
			const double Closer = Dist(Npc.X, Npc.Y, Target.X, Target.Y) - Dist(CX, CY, Target.X, Target.Y);
			const double Score = Closer * 4.0 - FMath::Abs(CX - AnastasisJs::Round(CX)) - FMath::Abs(CY - AnastasisJs::Round(CY));
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = { CX, CY };
				bFound = true;
			}
		}
		return bFound ? Best : FPoint{ Npc.X, Npc.Y };
	}

	// --- File de porte (crowdNav.js) ------------------------------------------------------

	FPoint FVillage::DoorQueueWaypoint(FNpc& Npc, const FPoint& Target, const FPoint& Waypoint)
	{
		// `resolveDoorQueue(sim, actor, target)`.
		FString Role = RoleSolo;
		int32 Rank = 0;
		bool bHasWait = false;
		FPoint Wait;
		auto IsDoorLike = [&]()
		{
			if (!Npc.DestBuildingId.IsEmpty() && Buildings.FindById(Npc.DestBuildingId)) return true;
			// `buildingForAccessTarget(sim, target)` : le batiment lui-meme, un de ses seuils, sinon l'anneau 1.
			const int32 TX = NavFloor(Target.X);
			const int32 TY = NavFloor(Target.Y);
			for (const FBuilding& B : Buildings.GetItems())
			{
				if (B.X == TX && B.Y == TY) return true;
				for (const FPoint& P : B.AccessPoints)
				{
					if (NavFloor(P.X) == TX && NavFloor(P.Y) == TY) return true;
				}
			}
			for (const FBuilding& B : Buildings.GetItems())
			{
				if (FMath::Max(FMath::Abs(B.X - TX), FMath::Abs(B.Y - TY)) == 1.0) return true;
			}
			return false;
		};
		if (IsDoorLike() && Dist(Npc.X, Npc.Y, Target.X, Target.Y) <= DoorQueue::ClaimRadius)
		{
			struct FClaimant { const FNpc* Actor; double Dist; FString Id; };
			TArray<FClaimant> Claimants;
			for (const FNpc& Other : Actors.GetItems())
			{
				if (Other.Inside.bActive) continue;
				if (!Other.bHasTarget) continue;
				if (NavFloor(Other.Target.X) != NavFloor(Target.X) || NavFloor(Other.Target.Y) != NavFloor(Target.Y)) continue;
				const double D = Dist(Other.X, Other.Y, Target.X, Target.Y);
				if (D > DoorQueue::ClaimRadius) continue;
				Claimants.Add({ &Other, D, Other.Id });
			}
			if (Claimants.Num() > 1)
			{
				// Priorite : deja sur la porte, sinon plus proche, puis identifiant (ordre des unites UTF-16).
				Claimants.StableSort([](const FClaimant& A, const FClaimant& B)
				{
					const int32 AOn = A.Dist <= DoorQueue::OccupyRadius ? 0 : 1;
					const int32 BOn = B.Dist <= DoorQueue::OccupyRadius ? 0 : 1;
					if (AOn != BOn) return AOn < BOn;
					if (A.Dist != B.Dist) return A.Dist < B.Dist;
					return A.Id.Compare(B.Id, ESearchCase::CaseSensitive) < 0;
				});
				const int32 Index = Claimants.IndexOfByPredicate([&Npc](const FClaimant& C) { return C.Actor == &Npc; });
				if (Index <= 0)
				{
					Role = RoleLeader;
					Rank = FMath::Max(0, Index);
				}
				else
				{
					// Point d'attente : en retrait du seuil, decale pour ne pas re-empiler.
					Role = RoleWaiter;
					Rank = Index;
					const double BaseAng = std::atan2(Npc.Y - Target.Y, Npc.X - Target.X);
					const double Ang = BaseAng + (Index - 1) * 0.42;
					const double Radius = DoorQueue::WaitDistance + (Index - 1) * DoorQueue::WaitSlot;
					double WaitX = Target.X + std::cos(Ang) * Radius;
					double WaitY = Target.Y + std::sin(Ang) * Radius;
					if (IsFootBlocked(WaitX, WaitY))
					{
						WaitX = Target.X + std::cos(BaseAng) * (Radius + 0.55);
						WaitY = Target.Y + std::sin(BaseAng) * (Radius + 0.55);
					}
					Wait = IsFootBlocked(WaitX, WaitY) ? FPoint{ Npc.X, Npc.Y } : FPoint{ WaitX, WaitY };
					bHasWait = true;
				}
			}
		}
		Npc.DoorQueueRole = Role;
		Npc.DoorQueueRank = Rank;
		if (Role != RoleWaiter || !bHasWait) return Waypoint;
		// Assez pres du point d'attente : rester (file polie).
		if (Dist(Npc.X, Npc.Y, Wait.X, Wait.Y) < 0.28) return { Npc.X, Npc.Y };
		return Wait;
	}

	// --- Le pas ----------------------------------------------------------------------------

	double FVillage::MovementSpeedFactor(FNpc& Npc, const FPoint* Target)
	{
		const AnastasisNeeds::FNeeds& N = Npc.Needs;
		double Factor = N.Energy < 18.0 ? 0.55 : 1.0;
		if (N.Hunger > 86.0) Factor *= 0.82;
		if (N.Health < 35.0) Factor *= 0.72;
		if (N.Health < 18.0) Factor *= 0.55;
		// `(actor.morale || 100) < 16`.
		if ((N.Morale != 0.0 ? N.Morale : 100.0) < 16.0) Factor *= 0.9;
		if (Npc.HesitationTimer > 0.0) Factor *= 0.16;
		// `weatherAt(sim.seed, 1 + sim.time / DAY_LENGTH)` : la pluie du jour (Life/AnastasisWeatherBehavior).
		if (TickDailyRain > 0.2) Factor *= AnastasisWeatherBehavior::RainSpeedFactor(TickDailyRain, Npc.Goal, Npc.JobId);
		if (N.Energy < 22.0 || N.Hunger > 88.0 || N.Morale < 12.0 || N.Health < 22.0)
		{
			const double Phase = FMath::Fmod(Now * 0.58 + SimHashText01(Npc.Id, 17.0) * 6.0, 6.0);
			if (Phase > 4.9) Factor *= 0.18;
		}
		// Un parent pres d'un enfant de sa famille : pas de familles (ecart n° 7).
		if (Target && Dist(Npc.X, Npc.Y, Target->X, Target->Y) < 2.4)
		{
			int32 Nearby = 0;
			const TArray<FNpc>& Items = Actors.GetItems();
			Grid.ForEachNear(Npc.X, Npc.Y, 1.05, [&](int32 Index)
			{
				if (Nearby >= 4 || !Items.IsValidIndex(Index)) return;
				const FNpc& Other = Items[Index];
				if (&Other == &Npc || Other.Inside.bActive) return;
				if (JsHypot(Npc.X - Other.X, Npc.Y - Other.Y) < 1.05) Nearby += 1;
			});
			if (Nearby >= 2) Factor *= FMath::Max(0.62, 1.0 - Nearby * 0.08);
		}
		// `sim.roadSpeedFactorAt(x, y) || 1` : pas de routes dans ce portage.
		// `lifestyleTravelFactor(sim, npc)` : seulement pour un habitant qui a un mode de vie. Sans lui, la
		// reference en tirerait un dans `sim.rng` ; ceux du C++ restent sans (ecart n° 8).
		if (Npc.Lifestyle.IsSet())
		{
			AnastasisLifestyle::FLifestyleSubject Subject;
			Subject.Goal = Npc.Goal;
			Subject.JobId = Npc.JobId;
			Subject.HomeId = Npc.HomeId;
			Subject.Skill = Npc.Skill;
			Subject.Energy = N.Energy;
			Subject.bHasTarget = Npc.bHasTarget;
			Subject.FavoriteBuildingId = Npc.FavoriteBuildingId;
			Factor *= AnastasisLifestyle::LifestyleTravelFactor(Npc.Lifestyle, Subject, &VillageRng, AnastasisRhythm::DayFracOf(Now));
		}
		// `districtTravelFactor` : pas de quartiers (ecart n° 3).
		return Factor;
	}

	void FVillage::RecordPassage(const FNpc& Npc)
	{
		const int32 TX = NavFloor(Npc.X);
		const int32 TY = NavFloor(Npc.Y);
		if (!World || !Nav.IsInBounds(TX, TY)) return;
		const int32 Index = TY * World->W + TX;
		if (World->Tiles[Index].Type == AnastasisWorld::ETileType::Water) return;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.X == TX && B.Y == TY) return;
		}
		if (Traffic.Num() != World->W * World->H) Traffic.SetNumZeroed(World->W * World->H);
		Traffic[Index] = FMath::Min(180, Traffic[Index] + 1);
	}

	void FVillage::MoveActor(FNpc& Npc, const FPoint& Target, double Dt)
	{
		// On ne marche plus vers la cible mais vers le prochain point du chemin calcule par l'A*.
		const double BeforeX = Npc.X;
		const double BeforeY = Npc.Y;
		Npc.bHasHesitation = true;
		Npc.HesitationCooldown = FMath::Max(0.0, Npc.HesitationCooldown - Dt);

		// Premier noeud (consomme pathCooldown une fois). Hesitation AVANT le pas.
		FPoint Waypoint = NextWaypoint(Npc, Target, Dt);
		Waypoint = DoorQueueWaypoint(Npc, Target, Waypoint);
		{
			const double DX0 = Waypoint.X - Npc.X;
			const double DY0 = Waypoint.Y - Npc.Y;
			double Len0 = JsHypot(DX0, DY0);
			if (Len0 == 0.0 || FMath::IsNaN(Len0)) Len0 = 1.0;
			const double DirX0 = DX0 / Len0;
			const double DirY0 = DY0 / Len0;
			if (Len0 > 0.35 && Npc.bHasLastMoveDir && Npc.HesitationCooldown <= 0.0)
			{
				const double Dot = Npc.LastMoveDir.X * DirX0 + Npc.LastMoveDir.Y * DirY0;
				if (Dot < 0.45)
				{
					const FString Seed = FString::Printf(TEXT("%s:%d"), Npc.Id.IsEmpty() ? TEXT("actor") : *Npc.Id,
						static_cast<int32>(AnastasisJs::Floor(Now * 2.0)));
					Npc.HesitationTimer = 0.07 + SimHashText01(Seed, 29.0) * 0.16;
					Npc.HesitationCooldown = 0.8 + SimHashText01(Seed, 31.0) * 0.55;
				}
			}
		}

		// Les waiters marchent ralentis vers leur point d'attente.
		const double QueueSlow = Npc.DoorQueueRole == RoleWaiter ? 0.55 : 1.0;
		const double Speed = Npc.Speed * MovementSpeedFactor(Npc, &Target) * QueueSlow;
		// Pas de marche : budget de distance, enchaine jusqu'a 8 noeuds.
		double Budget = Speed * FMath::Max(0.0, Dt);
		// ecart n°29 : multiplicateur de terrain du dernier segment, pour juger le blocage a la meme echelle.
		double LastTravelCost = 1.0;
		for (int32 Guard = 0; Guard < 8 && Budget > 1e-4; ++Guard)
		{
			if (Guard > 0)
			{
				Waypoint = DoorQueueWaypoint(Npc, Target, NextWaypoint(Npc, Target, 0.0));
			}
			const double DX = Waypoint.X - Npc.X;
			const double DY = Waypoint.Y - Npc.Y;
			const double Len = JsHypot(DX, DY);
			if (Len < 1e-4) break;
			const double DirX = DX / Len;
			const double DirY = DY / Len;
			Npc.bHasLastMoveDir = true;
			Npc.LastMoveDir = { DirX, DirY };
			// ecart n°29 (route-cost-001) : l'A* paie deja le cout de la case visee ; en mode jeu, chaque segment
			// depense le meme multiplicateur en temps de marche. Le mode reference garde exactement son calcul.
			const double TravelCost = bTerrainTravelCostEnabled
				? AnastasisNav::MoveCostAt(Nav, NavFloor(Waypoint.X), NavFloor(Waypoint.Y)) : 1.0;
			if (!FMath::IsFinite(TravelCost) || TravelCost <= 0.0) break;
			LastTravelCost = TravelCost;
			const double Step = FMath::Min(Len, Budget / TravelCost);
			const double NextX = Npc.X + DirX * Step;
			const double NextY = Npc.Y + DirY * Step;
			// Exception unique : sortir d'une case deja bloquee.
			const bool bCellStuck = IsFootBlocked(Npc.X, Npc.Y);
			const double X0 = Npc.X;
			const double Y0 = Npc.Y;
			if (bCellStuck || !IsFootBlocked(NextX, Npc.Y))
			{
				Npc.X = NavClamp(NextX, 1.0, Nav.W - 2);
			}
			if (bCellStuck || !IsFootBlocked(Npc.X, NextY))
			{
				Npc.Y = NavClamp(NextY, 1.0, Nav.H - 2);
			}
			const double MovedSeg = JsHypot(Npc.X - X0, Npc.Y - Y0);
			if (MovedSeg < 1e-5) break;
			Budget -= MovedSeg * TravelCost;
			if (JsHypot(Npc.X - Waypoint.X, Npc.Y - Waypoint.Y) >= 0.45) break;
		}

		Npc.HesitationTimer = FMath::Max(0.0, Npc.HesitationTimer - Dt);

		// Blocage progressif — jamais de teleport. File d'attente ≠ blocage.
		const double Moved = JsHypot(Npc.X - BeforeX, Npc.Y - BeforeY);
		if (Npc.DoorQueueRole == RoleWaiter)
		{
			Npc.StuckTimer = 0.0;
			Npc.StuckTicks = 0;
		}
		// ecart n°29 : en mode jeu, un sol lourd ralentit chaque pas du meme multiplicateur ; le seuil de blocage
		// de la reference (0.05) le suit, sinon un marcheur lent sur herbe detrempee passe pour coince et
		// recalcule son chemin sans fin. Mode reference : LastTravelCost reste 1, calcul identique.
		else if (Dist(Npc.X, Npc.Y, Target.X, Target.Y) > 1.2 && Moved < 0.05 / FMath::Max(1.0, LastTravelCost))
		{
			Npc.StuckTimer += Dt;
			Npc.StuckTicks += 1;
			if (Npc.StuckTimer > 0.75)
			{
				ResolveStuckActor(Npc, Target);
			}
		}
		else
		{
			Npc.StuckTimer = 0.0;
			Npc.StuckTicks = 0;
			if (Moved >= 0.05) Npc.StuckStage = 0;
		}
		Npc.TrafficTimer += Dt;
		if (Npc.TrafficTimer >= 0.85)
		{
			Npc.TrafficTimer = 0.0;
			RecordPassage(Npc);
		}
	}
}
