// player-minimal-001, player-goals-001 -- le joueur est un habitant.
//
// Port de la reference : `simulation.js` (incarnate, release, arriveAsPlayer, setPlayerMovementInput,
// drivePlayerActor, choosePlayerGoal, playerGoalOptions, playerRefusal), `decisionProvider.js`
// (decideAsPlayer : sans commande l'habitant incarne attend, avec une intention il la tient tant
// qu'elle passe, et cede en disant pourquoi) et `updateNpc` de npc.js (branche `playerControlled`).
// Porte aussi : la reputation (`standing.js`, merite des batiments acheves ; le reste : ecart n°23).
// PAS porte (ecart n°20) : la parole dirigee (`requestPlayerTellResourceSpot`), le flux aleatoire joueur
// (`spawnNpc` n'en tire aucun ici).
//
// EXTENSION (TIME_WARP_001, demande d'Alexandre, ecart n°22) : presence et oisivete. Un joueur qui accelere le
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

	void FVillage::ResetPlayerHand()
	{
		PlayerDrive = FPoint();
		bHasPlayerChoice = false;
		PlayerChoice = FPlayerGoalChoice();
		bPlayerChoiceDirty = false;
		bHasPlayerRefusal = false;
		PlayerRefusal = FPlayerRefusal();
		PlayerOptions.Reset();
	}

	bool FVillage::Incarnate(const FString& NpcId)
	{
		FNpc* Npc = Actors.FindById(NpcId);
		if (!Npc)
		{
			return false;
		}
		PlayerPersonId = Npc->Id;
		ResetPlayerHand();
		// `npc.aiThinkAt = this.time` : il pense des le prochain tick, et attend.
		Npc->AiThinkAt = Now;
		// Une decision de Nous prise avant l'incarnation ne pese plus : Nous ne pense pas pour lui.
		Npc->bHasAlgoDecision = false;
		return true;
	}

	FString FVillage::Release()
	{
		const FString Previous = PlayerPersonId;
		PlayerPersonId.Reset();
		ResetPlayerHand();
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
		// `this.playerGoalChoice = null` : conduire le corps a la main retire l'intention posee.
		if (bHasPlayerChoice)
		{
			bHasPlayerChoice = false;
			PlayerChoice = FPlayerGoalChoice();
			bPlayerChoiceDirty = true;
		}
		if (!bWasMoving) Actor->AiThinkAt = Now;
		return true;
	}

	bool FVillage::ChoosePlayerGoal(const FString& Goal)
	{
		FNpc* Actor = PlayerActor();
		if (!Actor)
		{
			return false;
		}
		const FString Clean = Goal.TrimStartAndEnd();
		// Retirer l'intention, ou en poser une neuve (compteurs a zero). La decision reste au point
		// de decision : on demande seulement qu'il y repense tout de suite.
		bHasPlayerChoice = !Clean.IsEmpty();
		PlayerChoice = FPlayerGoalChoice();
		PlayerChoice.Goal = Clean;
		bPlayerChoiceDirty = true;
		bHasPlayerRefusal = false;
		PlayerDrive = FPoint();
		Actor->AiThinkAt = Now;
		return true;
	}

	bool FVillage::BodyOverrides(const FNpc& Npc)
	{
		// `needsCritical` ne convient pas (vrai a ~100 % des decisions dans la reference) : les seuils
		// du verrou de chantier, qui repondent deja a « quand un but tenu doit-il ceder ? ».
		return Npc.Needs.Hunger >= PlayerDecision::HungerRelease
			|| Npc.Needs.Thirst >= PlayerDecision::ThirstRelease
			|| Npc.Needs.Energy <= PlayerDecision::EnergyRelease;
	}

	bool FVillage::IsRemedyFor(const FNpc& Npc, const FString& Goal)
	{
		return (Goal == GoalDrink && Npc.Needs.Thirst >= PlayerDecision::ThirstRelease)
			|| (Goal == GoalEat && Npc.Needs.Hunger >= PlayerDecision::HungerRelease)
			|| (Goal == GoalRest && Npc.Needs.Energy <= PlayerDecision::EnergyRelease);
	}

	bool FVillage::IsPlayerTableGoal(const FNpc& Npc, const FString& Goal) const
	{
		if (!IsPortedGoalFor(Npc, Goal)) return false;
		// La ligne `build` existe toujours dans la table, mais sans chantier ouvert ce n'est qu'un plancher
		// (`UnportedGoalsFloor`) : rien a batir, le but n'est pas reellement dans la table.
		if (Goal == AnastasisBuild::GoalBuild && ActiveSites().Num() == 0) return false;
		return true;
	}

	FString FVillage::CedePlayerGoal(const FString& Reason)
	{
		++PlayerChoice.Yields;
		PlayerChoice.CedingFor = Reason;
		bHasPlayerRefusal = true;
		PlayerRefusal.Wanted = PlayerChoice.Goal;
		PlayerRefusal.Reason = Reason;
		PlayerRefusal.Applied = GoalIdle;
		PlayerRefusal.Day = Day();
		// Le corps parle, la table ou un verrou dit non : l'humain attend, Nous ne choisit pas a sa place.
		return GoalIdle;
	}

	FString FVillage::DecideAsPlayer(FNpc& Npc, const TArray<TPair<FString, double>>& Rows, const FString& Next, bool bLocked)
	{
		if (!bHasPlayerChoice)
		{
			bHasPlayerRefusal = false;
			return GoalIdle;
		}
		const FString& Wanted = PlayerChoice.Goal;
		// 1. Le corps passe devant. L'intention n'est pas retiree : le joueur choisit le remede.
		//    EXTENSION ASSUMEE par Alexandre le 2026-10-01 (ecart n°21) : la reference teste `bodyOverrides` avant tout et refuse donc AUSSI
		//    le remede -- un joueur a soif 88 ne pourrait plus jamais boire. Son propre commentaire dit
		//    « le joueur doit choisir le remede » : le remede du besoin qui parle passe.
		if (BodyOverrides(Npc) && !IsRemedyFor(Npc, Wanted)) return CedePlayerGoal(PlayerDecision::RefusalBody);
		// 2. SYM-1 : la table fait foi. Un but absent de la table, ou que ce portage ne sait pas executer (ecart n°1), est impossible maintenant.
		const bool bInTable = IsPlayerTableGoal(Npc, Wanted)
			&& Rows.ContainsByPredicate([&](const TPair<FString, double>& Row) { return Row.Key == Wanted; });
		if (!bInTable) return CedePlayerGoal(PlayerDecision::RefusalNotInTable);
		// 3. SYM-3 : les verrous ne connaissent pas le joueur.
		if (bLocked && Wanted != Next) return CedePlayerGoal(PlayerDecision::RefusalLocked);
		++PlayerChoice.Holds;
		PlayerChoice.CedingFor.Reset();
		bHasPlayerRefusal = false;
		return Wanted;
	}

	void FVillage::CommitPlayerIdle(FNpc& Npc)
	{
		if (Npc.Goal == GoalEat)
		{
			ReleaseMeal(Npc, TEXT("player_idle"));
		}
		if (Npc.Goal != GoalIdle)
		{
			Npc.GoalSince = Now;
		}
		Npc.Goal = GoalIdle;
		Npc.bHasTarget = false;
		Npc.WorkTimer = 0.0;
		Npc.DestBuildingId.Reset();
		ClearNavigation(Npc);
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
			// Merite de la reference : `deeds.built x buildGain` (porte) ; ambitions, jalons, conseils (ecart n°23),
			// vols : non portes (ecart n°23). EXTENSION (ecart n°22) : l'oisivete du joueur, qui ne s'efface pas (un acte reste un acte).
			const double IdleDays = Npc.IdleSeconds / AnastasisRhythm::DayLength;
			const double Merit = Npc.BuildingsCompleted * Standing::BuildGain - IdleDays * Standing::IdleMeritPerDay;
			const double Target = Clamp(Standing::Base + Merit, 0.0, 100.0);
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

	double FVillage::ReputationAffinity(const FNpc& Other) const
	{
		if (!IsPlayer(Other) || Other.Reputation == Standing::Base)
		{
			return 0.0;
		}
		return (Other.Reputation - Standing::Base) * Standing::AffinityWeight;
	}

	void FVillage::UpdatePlayer(FNpc& Npc, double Dt)
	{
		// `syncVillagePhase` : tenu a jour pour le jour ou il sera rendu a Nous.
		Npc.VillagePhase = AnastasisRhythm::PhaseId(AnastasisRhythm::VillagePhase(AnastasisRhythm::DayFracOf(Now)));
		const bool bDrive = JsHypot(PlayerDrive.X, PlayerDrive.Y) > 1e-5;

		// Branche `playerControlled` de updateNpc : a l'heure de penser, percevoir, decider. Une
		// intention qui a sa cible et que rien ne remet en cause n'est pas redecidee a chaque pensee :
		// le chemin ne serait recalcule que pour rien.
		if (Npc.AiThinkAt < 0.0 || Now >= Npc.AiThinkAt)
		{
			Npc.AiThinkAt = Now + AnastasisNous::DecisionIntervalSeconds(NeedsCritical(Npc.Needs));
			const bool bReconsider = bDrive || bPlayerChoiceDirty || !bHasPlayerChoice
				|| Npc.Goal == GoalIdle || !Npc.bHasTarget || BodyOverrides(Npc);
			if (bReconsider)
			{
				// Dedans pour ce qu'il a lui-meme choisi : il y reste. Sinon (il conduit, a change d'avis,
				// ou n'a plus d'intention), il sort, comme la reference a chaque pensee du joueur.
				const bool bStayInside = Npc.Inside.bActive && !bDrive && !bPlayerChoiceDirty && bHasPlayerChoice
					&& Npc.Inside.Goal == PlayerChoice.Goal && !BodyOverrides(Npc);
				if (Npc.Inside.bActive && !bStayInside) ExitBuilding(Npc);
				if (!Npc.Inside.bActive)
				{
					Perceive(Npc, false);
					if (bDrive)
					{
						CommitPlayerIdle(Npc);
					}
					else
					{
						ChooseGoal(Npc);
					}
				}
				bPlayerChoiceDirty = false;
			}
		}

		if (bDrive)
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
		if (Npc.Goal == GoalIdle)
		{
			// `act` pour PLAYER_IDLE_GOAL : pas de cible, pas de travail, il attend.
			Npc.bHasTarget = false;
			Npc.WorkTimer = 0.0;
			Npc.Activity = TEXT("attend");
			return;
		}
		if (Npc.FailedActions >= 3)
		{
			// Comme RedirectAfterFailure, mais vers l'attente : `observer` n'est pas un but humain.
			Npc.FailedActions = 0;
			Npc.StuckStage = 0;
			CommitPlayerIdle(Npc);
			Npc.Activity = TEXT("attend");
			return;
		}
		// Le but humain s'execute comme celui de n'importe qui : memes cibles, memes portes, memes effets.
		Act(Npc, Dt);
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
