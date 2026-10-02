// player-minimal-001 -- le joueur est un habitant.
//
// Port de la reference : `simulation.js` (incarnate, release, arriveAsPlayer, setPlayerMovementInput,
// drivePlayerActor), `decisionProvider.js` (sans commande, l'habitant incarne attend : `idle`) et
// `updateNpc` de npc.js (branche `playerControlled`). Porte : l'incarnation, l'attente, la marche
// directe, la reputation (`standing.js`, structure seule). PAS porte : le choix de but par le joueur
// (`choosePlayerGoal`, refus motives), la parole dirigee (`requestPlayerTellResourceSpot`), le flux
// aleatoire joueur (`spawnNpc` n'en tire aucun ici).
//
// EXTENSION (TIME_WARP_001, demande d'Alexandre) : presence et oisivete. Un joueur qui accelere le
// temps ne fait rien aux yeux du village ; il s'efface de leur vue et sa reputation baisse.

#include "Village/AnastasisVillage.h"

#include "Core/AnastasisSimMath.h"

namespace AnastasisVillage
{
	using AnastasisMath::Clamp;
	using AnastasisMath::JsHypot;

	const FNpc* FVillage::PlayerActor() const
	{
		return PlayerPersonId.IsEmpty() ? nullptr : Actors.FindById(PlayerPersonId);
	}

	FNpc* FVillage::PlayerActor()
	{
		return PlayerPersonId.IsEmpty() ? nullptr : Actors.FindById(PlayerPersonId);
	}

	bool FVillage::Incarnate(const FString& NpcId)
	{
		FNpc* Npc = Actors.FindById(NpcId);
		if (!Npc)
		{
			return false;
		}
		PlayerPersonId = Npc->Id;
		PlayerDrive = FPoint();
		// `npc.aiThinkAt = this.time` : il pense des le prochain tick, et attend.
		Npc->AiThinkAt = Now;
		return true;
	}

	FString FVillage::Release()
	{
		const FString Previous = PlayerPersonId;
		PlayerPersonId.Reset();
		PlayerDrive = FPoint();
		// Rendu a Nous : sa prochaine pensee redecide (le but `idle` n'est pas porte pour un PNJ).
		if (FNpc* Npc = Actors.FindById(Previous))
		{
			Npc->AiThinkAt = Now;
			Npc->bHasTarget = false;
		}
		return Previous;
	}

	FString FVillage::ArriveAsPlayer(double InX, double InY)
	{
		// `if (!this._foundingLifeReady || !this.settlement) return null` : pas de monde, pas d'arrivee.
		if (!World)
		{
			return FString();
		}
		const double WantX = InX >= 0.0 ? InX : Settlement.X + 2.0;
		const double WantY = InY >= 0.0 ? InY : Settlement.Y + 3.0;
		// Premier sol libre en anneaux : la reference tombe sur `spawnNpc`, qui ne verifie rien ; ici
		// un habitant pose dans un mur ne pourrait pas en sortir a la main.
		const int32 CX = FMath::FloorToInt32(WantX);
		const int32 CY = FMath::FloorToInt32(WantY);
		for (int32 R = 0; R <= 12; ++R)
		{
			for (int32 DY = -R; DY <= R; ++DY)
			{
				for (int32 DX = -R; DX <= R; ++DX)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (X < 1 || Y < 1 || X > Nav.W - 2 || Y > Nav.H - 2) continue;
					const double PX = R == 0 ? WantX : X + 0.5;
					const double PY = R == 0 ? WantY : Y + 0.5;
					if (IsFootBlocked(PX, PY)) continue;
					// `createNpc` : besoins par defaut d'un arrivant (AnastasisNeeds::FNeeds).
					const FString Id = SpawnNpc(PX, PY, AnastasisNeeds::FNeeds());
					Incarnate(Id);
					return Id;
				}
			}
		}
		return FString();
	}

	bool FVillage::SetPlayerMovementInput(double DX, double DY)
	{
		FNpc* Actor = PlayerActor();
		const double RawX = FMath::IsFinite(DX) ? DX : 0.0;
		const double RawY = FMath::IsFinite(DY) ? DY : 0.0;
		const double Length = JsHypot(RawX, RawY);
		const bool bWasMoving = JsHypot(PlayerDrive.X, PlayerDrive.Y) > 1e-5;
		if (!Actor || Length <= 1e-5)
		{
			PlayerDrive = FPoint();
			// S'arreter fait penser tout de suite : l'habitant reprend son attente.
			if (Actor && bWasMoving) Actor->AiThinkAt = Now;
			return false;
		}
		PlayerDrive = { RawX / Length, RawY / Length };
		if (!bWasMoving) Actor->AiThinkAt = Now;
		return true;
	}

	void FVillage::ObservePlayer(double InPresence, double IdleSecondsDelta)
	{
		if (FNpc* Actor = PlayerActor())
		{
			Actor->Presence = Clamp(FMath::IsFinite(InPresence) ? InPresence : 1.0, 0.0, 1.0);
			Actor->IdleSeconds += FMath::Max(0.0, FMath::IsFinite(IdleSecondsDelta) ? IdleSecondsDelta : 0.0);
		}
	}

	void FVillage::UpdateReputationDaily()
	{
		for (FNpc& Npc : Actors.GetItemsMutable())
		{
			// Merite de la reference : constructions, ambitions, jalons, conseils, vols -- non portes.
			// Seule l'oisivete compte, et elle ne s'efface pas (un acte reste un acte).
			const double IdleDays = Npc.IdleSeconds / AnastasisRhythm::DayLength;
			const double Target = Clamp(Standing::Base - IdleDays * Standing::IdleMeritPerDay, 0.0, 100.0);
			Npc.Reputation = Clamp(Npc.Reputation + (Target - Npc.Reputation) * Standing::DriftToBase, 0.0, 100.0);
		}
	}

	bool FVillage::Sees(const FNpc& Other, double D, double Range)
	{
		if (Other.Presence >= 1.0)
		{
			return D <= Range;
		}
		return Other.Presence >= Standing::MinPresenceSeen && D <= Range * Other.Presence;
	}

	double FVillage::ReputationAffinity(const FNpc& Other)
	{
		return Other.Reputation == Standing::Base ? 0.0 : (Other.Reputation - Standing::Base) * Standing::AffinityWeight;
	}

	void FVillage::UpdatePlayer(FNpc& Npc, double Dt)
	{
		// `syncVillagePhase` : tenu a jour pour le jour ou il sera rendu a Nous.
		Npc.VillagePhase = AnastasisRhythm::PhaseId(AnastasisRhythm::VillagePhase(AnastasisRhythm::DayFracOf(Now)));

		// Branche `playerControlled` de updateNpc : a l'heure de penser, sortir, percevoir, decider.
		if (Npc.AiThinkAt < 0.0 || Now >= Npc.AiThinkAt)
		{
			if (Npc.Inside.bActive) ExitBuilding(Npc);
			Perceive(Npc, false);
			// `decideAsPlayer` sans `playerGoalChoice` : PLAYER_IDLE_GOAL. Nous ne choisit pas.
			if (Npc.Goal != GoalIdle)
			{
				Npc.GoalSince = Now;
			}
			Npc.Goal = GoalIdle;
			Npc.bHasTarget = false;
			Npc.WorkTimer = 0.0;
			ClearNavigation(Npc);
			Npc.LastDecision = FDecisionTrace();
			Npc.LastDecision.Time = Now;
			Npc.LastDecision.Winner = GoalIdle;
			Npc.LastDecision.CommitGate = TEXT("player");
			Npc.AiThinkAt = Now + AnastasisNous::DecisionIntervalSeconds(NeedsCritical(Npc.Needs));
		}

		if (JsHypot(PlayerDrive.X, PlayerDrive.Y) > 1e-5)
		{
			DrivePlayer(Npc, Dt);
			Npc.Activity = TEXT("marche");
			return;
		}
		if (Npc.Inside.bActive)
		{
			UpdateInside(Npc);
			return;
		}
		// `act` pour PLAYER_IDLE_GOAL : pas de cible, pas de travail, il attend.
		Npc.bHasTarget = false;
		Npc.WorkTimer = 0.0;
		Npc.Activity = TEXT("attend");
	}

	void FVillage::DrivePlayer(FNpc& Npc, double Dt)
	{
		if (Npc.Inside.bActive)
		{
			return;
		}
		// `actor.speed * movementSpeedFactor(...)` : comme MoveActor, seul le bloc pluie est porte.
		const double Step = Npc.Speed * AnastasisWeatherBehavior::RainSpeedFactor(TickDailyRain, Npc.Goal, Npc.JobId)
			* FMath::Max(0.0, Dt);
		const double NextX = Npc.X + PlayerDrive.X * Step;
		const double NextY = Npc.Y + PlayerDrive.Y * Step;
		const bool bStuck = IsFootBlocked(Npc.X, Npc.Y);
		if (bStuck || !IsFootBlocked(NextX, Npc.Y)) Npc.X = Clamp(NextX, 1.0, Nav.W - 2);
		if (bStuck || !IsFootBlocked(Npc.X, NextY)) Npc.Y = Clamp(NextY, 1.0, Nav.H - 2);
		Npc.bHasTarget = false;
		ClearNavigation(Npc);
		Npc.StuckTimer = 0.0;
	}
}
