#include "Life/AnastasisBonds.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"

namespace AnastasisBonds
{
	using AnastasisMath::Clamp;
	using namespace AnastasisNeeds::Constants;

	namespace
	{
		uint32 Fnv(const FString& Text)
		{
			uint32 H = 2166136261u;
			for (const TCHAR C : Text)
			{
				H ^= static_cast<uint32>(C);
				H = AnastasisJs::Imul(H, 16777619u);
			}
			return H;
		}

		/** `natureSocialMods(npc).talk` d'une nature moyenne : clamp(0,7 + coeur x 0,35). */
		double NeutralTalkMod()
		{
			return Clamp(0.7 + 1.0 * 0.35, 0.65, 1.4);
		}

		double RefuseBase(EBondKind Kind)
		{
			switch (Kind)
			{
			case EBondKind::Rival: return 0.32;
			case EBondKind::Coworker: return 0.08;
			case EBondKind::Friend: return 0.05;
			default: return 0.16;
			}
		}

		bool IsWorkingGoal(const FString& Goal)
		{
			return Goal.StartsWith(TEXT("gather"), ESearchCase::CaseSensitive)
				|| Goal == TEXT("craft") || Goal == TEXT("build") || Goal == TEXT("deliver")
				|| Goal == TEXT("sell") || Goal == TEXT("maintain") || Goal == TEXT("haulJob") || Goal == TEXT("haulCart");
		}

		/** `(npc.morale || 50)`. */
		double MoraleOr50(double Morale)
		{
			return Morale != 0.0 && !FMath::IsNaN(Morale) ? Morale : 50.0;
		}
	}

	uint32 HashTalk(const FString& AId, const FString& BId, int64 Salt)
	{
		return Fnv(FString::Printf(TEXT("%s:%s:%lld"), *AId, *BId, Salt));
	}

	bool Chance(uint32 Seed, int32 Salt, double Rate)
	{
		const uint32 H = Fnv(FString::Printf(TEXT("%u:%d"), Seed, Salt));
		return static_cast<double>(H % 1000u) / 1000.0 < Rate;
	}

	bool IsTalkUrgent(const AnastasisNeeds::FNeeds& N)
	{
		return N.Hunger >= HungerCritical
			|| N.Thirst >= ThirstCritical
			|| N.Health <= HealthCritical
			|| N.Energy <= 100.0 - FatigueCritical;
	}

	bool IsTalkWorkBusy(const FString& Goal, bool bInside)
	{
		if (bInside) return false;
		return Goal == TEXT("gatherWood") || Goal == TEXT("gatherStone") || Goal == TEXT("gatherFood")
			|| Goal == TEXT("build") || Goal == TEXT("craft") || Goal == TEXT("maintain") || Goal == TEXT("deliver")
			|| Goal == TEXT("sell") || Goal == TEXT("buy") || Goal == TEXT("helpFarm") || Goal == TEXT("apprentice");
	}

	EBondKind BondKindBetween(double RelAB, double RelBA, const FString& JobA, const FString& JobB)
	{
		if (RelAB <= RivalAt || RelBA <= RivalAt) return EBondKind::Rival;
		if (RelAB >= FriendAt) return EBondKind::Friend;
		if (!JobA.IsEmpty() && !JobB.IsEmpty() && JobA == JobB) return EBondKind::Coworker;
		return EBondKind::Stranger;
	}

	FDominantNeed DominantNeed(const AnastasisNeeds::FNeeds& N)
	{
		const FDominantNeed Pressures[] = {
			{ TEXT("hunger"), N.Hunger },
			{ TEXT("thirst"), N.Thirst },
			{ TEXT("fatigue"), 100.0 - N.Energy },
			{ TEXT("lonely"), 100.0 - N.Social },
			{ TEXT("bored"), 100.0 - N.Leisure },
			{ TEXT("filthy"), 100.0 - N.Hygiene },
			{ TEXT("health"), 100.0 - N.Health },
			{ TEXT("despair"), FMath::Max(0.0, MoraleUrge - N.Morale) * 2.2 },
		};
		// Tri decroissant STABLE : le premier des maxima gagne.
		FDominantNeed Best = Pressures[0];
		for (const FDominantNeed& P : Pressures)
		{
			if (P.Value > Best.Value) Best = P;
		}
		return Best;
	}

	double SpeakWorth(const AnastasisNeeds::FNeeds& Speaker, const AnastasisNeeds::FNeeds& Listener, bool bSpeakerChain, EBondKind Kind)
	{
		double Worth = 0.1;
		if (IsTalkUrgent(Speaker) || IsTalkUrgent(Listener)) return 1.0;
		const FDominantNeed Need = DominantNeed(Speaker);
		const FString Id = Need.Id;
		if (Need.Value >= 70.0 && (Id == TEXT("hunger") || Id == TEXT("thirst") || Id == TEXT("fatigue") || Id == TEXT("health")))
		{
			Worth = FMath::Max(Worth, 0.88);
		}
		else if (Need.Value >= 58.0 && (Id == TEXT("hunger") || Id == TEXT("fatigue")))
		{
			Worth = FMath::Max(Worth, 0.56);
		}
		if (bSpeakerChain) Worth = FMath::Max(Worth, 0.64);
		// Sceau frais : pas de colonie.
		if (Kind == EBondKind::Rival) return 1.0;
		if (Kind == EBondKind::Friend) Worth = FMath::Max(Worth, 0.2);
		// Joueur proche : pas de joueur. Ragot : chronique vide.
		return FMath::Min(1.0, Worth);
	}

	bool ShouldSpeakNow(double Worth, int32 RecentEmits, const FString& SpeakerId, const FString& ListenerId, double Now, double AmbientChance)
	{
		if (Worth >= SpeakWorthThreshold) return true;
		if (RecentEmits >= VillageEmitLimit) return false;
		// `options.ambientChance` (0,1 pour la causette en livrant), sinon `TALK.speakWorthAmbientChance`.
		const double Rate = FMath::Min(0.5, FMath::Max(0.0, AmbientChance + Worth * 0.4));
		const int64 Salt = static_cast<int64>(AnastasisJs::Floor(Now * 10.0)) + 41;
		return Chance(HashTalk(SpeakerId, ListenerId, Salt), 71, Rate);
	}

	double TalkHoldDuration(bool bUrgent, bool bWorkBusy)
	{
		if (bUrgent) return SessionSecondsUrgent;
		if (bWorkBusy) return SessionSecondsWork;
		return SessionSeconds;
	}

	int32 TalkMaxTurns(const FString& SpeakerId, const FString& ListenerId, bool bUrgent, bool bWorkBusy, EBondKind Kind, int32 Fatigue)
	{
		if (bUrgent || bWorkBusy) return 1;
		int32 Turns = MinTurns;
		if (Kind == EBondKind::Rival) Turns = 2;
		else if (Kind == EBondKind::Friend)
		{
			const int32 Span = FMath::Max(0, MaxTurns - MinTurns);
			Turns = MinTurns + (Span ? static_cast<int32>(HashTalk(SpeakerId, ListenerId, 11) % static_cast<uint32>(Span + 1)) : 0);
		}
		if (Fatigue >= FatigueBlockCount) return 1;
		if (Fatigue >= 1) return FMath::Min(2, FMath::Max(1, Turns - 1));
		return Turns;
	}

	bool RefusesReply(const AnastasisNeeds::FNeeds& Replier, EBondKind Kind, int32 Fatigue, uint32 Seed)
	{
		const FDominantNeed Need = DominantNeed(Replier);
		const FString Id = Need.Id;
		if (Id == TEXT("hunger") && Need.Value >= 70.0) return false;
		if (Id == TEXT("fatigue") && Need.Value >= 70.0) return false;
		const double Rate = FMath::Min(0.55, RefuseBase(Kind) + FMath::Max(0, Fatigue) * 0.08);
		return Chance(Seed, 19, Rate);
	}

	int32 BondTalkGain(bool bFriend)
	{
		double Gain = 6.0;
		if (bFriend) Gain += 2.0;
		Gain *= (NeutralTalkMod() + NeutralTalkMod()) * 0.5;
		return FMath::Max(3, static_cast<int32>(AnastasisJs::Round(Gain)));
	}

	double CompanionAffinity(double Rel, double Trust, EPersonTag Tag, bool bSameJob, bool bOtherAvailable)
	{
		double Score = 0.0;
		if (Rel >= FriendAt) Score += Rel * FriendScale;
		else if (Rel > 0.0) Score += Rel * 0.18;
		else Score += StrangerFloor + Rel * 0.25;
		if (Rel <= GrudgeAt) Score -= GrudgePenalty;
		if (bSameJob) Score += JobBonus;
		if (bOtherAvailable) Score += 8.0;
		// `socialCompanionBonus`.
		double Bonus = Trust * CompanionWeight;
		if (Tag == EPersonTag::Ally) Bonus += 8.0;
		if (Tag == EPersonTag::Rival) Bonus -= 22.0;
		Score += Bonus;
		return Score;
	}

	int32 BondStageRank(double Rel)
	{
		if (Rel <= -45.0) return -2;
		if (Rel >= 85.0) return 4;
		if (Rel >= 70.0) return 3;
		if (Rel >= FriendAt) return 2;
		if (Rel >= 15.0) return 1;
		return 0;
	}

	void NotePersonDirect(TArray<FPersonRow>& People, const FString& OtherId, int32 Day, double TrustDelta)
	{
		FPersonRow* Row = People.FindByPredicate([&](const FPersonRow& R) { return R.Id == OtherId; });
		if (!Row)
		{
			FPersonRow New;
			New.Id = OtherId;
			New.Day = Day;
			Row = &People.Add_GetRef(New);
		}
		Row->Day = Day;
		Row->Meets += 1;
		if (TrustDelta != 0.0) Row->Trust = Clamp(Row->Trust + TrustDelta, -50.0, 50.0);
		// `promoteTag` (aucune etiquette verrouillee ici).
		if (Row->Trust >= AllyAt) Row->Tag = EPersonTag::Ally;
		else if (Row->Trust <= PersonRivalAt) Row->Tag = EPersonTag::Rival;
		// `trimPeople` : tri stable croissant de |trust| x 2 + rencontres + 4 (direct).
		if (People.Num() > PeopleCapacity)
		{
			TArray<int32> Order;
			for (int32 I = 0; I < People.Num(); ++I) Order.Add(I);
			Order.StableSort([&](int32 A, int32 B)
			{
				const double SA = FMath::Abs(People[A].Trust) * 2.0 + People[A].Meets + 4.0;
				const double SB = FMath::Abs(People[B].Trust) * 2.0 + People[B].Meets + 4.0;
				return SA < SB;
			});
			const int32 Drop = People.Num() - PeopleCapacity;
			TSet<FString> Dropped;
			for (int32 I = 0; I < Drop; ++I) Dropped.Add(People[Order[I]].Id);
			People.RemoveAll([&](const FPersonRow& R) { return Dropped.Contains(R.Id); });
		}
	}

	void ForgetStalePeople(TArray<FPersonRow>& People, int32 Day)
	{
		People.RemoveAll([&](const FPersonRow& R) { return Day - R.Day > PeopleForgetAfterDays; });
	}

	void ObserveMind(TArray<FTomEntry>& Tom, const FString& OtherId, const FString& OtherGoal, double Attitude, int32 Day)
	{
		FTomEntry* Prior = Tom.FindByPredicate([&](const FTomEntry& E) { return E.Id == OtherId; });
		const double Confidence = Clamp((Prior ? Prior->Confidence : 0.2) + 0.15, 0.0, 0.9);
		FTomEntry Entry;
		Entry.Id = OtherId;
		Entry.EstimatedGoal = OtherGoal;
		Entry.Attitude = Clamp(Attitude, -100.0, 100.0);
		Entry.Confidence = Confidence;
		Entry.Day = Day;
		// Une cle deja presente garde sa place (objet JS).
		if (Prior) *Prior = Entry;
		else Tom.Add(Entry);
		// `trimToM` : oubli au-dela de 18 jours, puis capacite 6 (les plus anciens).
		Tom.RemoveAll([&](const FTomEntry& E) { return Day - E.Day > TomForgetAfterDays; });
		if (Tom.Num() > TomCapacity)
		{
			TArray<int32> Order;
			for (int32 I = 0; I < Tom.Num(); ++I) Order.Add(I);
			Order.StableSort([&](int32 A, int32 B) { return Tom[A].Day < Tom[B].Day; });
			TSet<FString> Dropped;
			for (int32 I = 0; I < Tom.Num() - TomCapacity; ++I) Dropped.Add(Tom[Order[I]].Id);
			Tom.RemoveAll([&](const FTomEntry& E) { return Dropped.Contains(E.Id); });
		}
	}

	FString EstimatedGoalOf(const TArray<FTomEntry>& Tom, const FString& OtherId)
	{
		const FTomEntry* E = Tom.FindByPredicate([&](const FTomEntry& T) { return T.Id == OtherId; });
		if (!E || E->Confidence < TomMinConfidence) return FString();
		return E->EstimatedGoal;
	}

	double SocialMemoryBiasSocialize(const TArray<FPersonRow>& People)
	{
		if (People.Num() == 0) return 0.0;
		double TrustSum = 0.0;
		int32 Allies = 0;
		for (const FPersonRow& R : People)
		{
			TrustSum += R.Trust;
			if (R.Tag == EPersonTag::Ally) ++Allies;
		}
		const double Avg = TrustSum / People.Num();
		return Clamp(Avg * 0.28 + Allies * 2.8, -BiasCap, BiasCap);
	}

	const FPersonRow* FindPerson(const TArray<FPersonRow>& People, const FString& Id)
	{
		return People.FindByPredicate([&](const FPersonRow& R) { return R.Id == Id; });
	}

	FString PickRememberedSeek(const TArray<FPersonRow>& People, const TArray<FTomEntry>& Tom,
		const FString& SelfId, double SelfX, double SelfY, double SelfSocial,
		TFunctionRef<bool(const FString&, double&, double&)> Locate)
	{
		FString Best;
		double BestScore = -TNumericLimits<double>::Max();
		bool bAny = false;
		for (const FPersonRow& Row : People)
		{
			if (Row.Id.IsEmpty() || Row.Tag == EPersonTag::Rival) continue;
			const bool bTagged = Row.Tag == EPersonTag::Ally;
			if (!bTagged && Row.Trust < SeekMinTrust) continue;
			// Fiches directes : provenance pleine, toujours actionnable.
			double X = 0.0;
			double Y = 0.0;
			if (Row.Id == SelfId || !Locate(Row.Id, X, Y)) continue;
			const double D = AnastasisMath::JsHypot(X - SelfX, Y - SelfY);
			if (D > SeekRange) continue;
			const FString Believed = EstimatedGoalOf(Tom, Row.Id);
			if (!Believed.IsEmpty() && IsWorkingGoal(Believed) && SelfSocial > 45.0) continue;
			const double Score = Row.Trust + 10.0 - D * 0.55;
			if (!bAny || Score > BestScore)
			{
				bAny = true;
				BestScore = Score;
				Best = Row.Id;
			}
		}
		return Best;
	}

	void PruneMoodlets(TArray<FMoodlet>& List, double Now)
	{
		List.RemoveAll([&](const FMoodlet& M) { return !FMath::IsFinite(M.Until) || Now > M.Until; });
	}

	void StampNewFriend(TArray<FMoodlet>& List, double& Morale, double Now)
	{
		PruneMoodlets(List, Now);
		const double Until = Now + FMath::Max(4.0, NewFriendSeconds);
		if (FMoodlet* Existing = List.FindByPredicate([](const FMoodlet& M) { return M.Id == TEXT("newFriend"); }))
		{
			Existing->Until = FMath::Max(Existing->Until, Until);
			Existing->At = Now;
			while (List.Num() > MoodletMaxSlots)
			{
				const int32 Drop = List.IndexOfByPredicate([&](const FMoodlet& M) { return &M != Existing; });
				if (Drop < 0) break;
				List.RemoveAt(Drop);
			}
			return;
		}
		while (List.Num() >= MoodletMaxSlots) List.RemoveAt(0);
		FMoodlet Entry;
		Entry.Id = TEXT("newFriend");
		Entry.At = Now;
		Entry.Until = Until;
		List.Add(Entry);
		Morale = Clamp(MoraleOr50(Morale) + NewFriendMoraleOnStamp, 0.0, 100.0);
	}

	void TickMoodlets(TArray<FMoodlet>& List, double& Morale, double Dt, double Now)
	{
		if (!(Dt > 0.0)) return;
		PruneMoodlets(List, Now);
		if (List.Num() == 0) return;
		double Rate = 0.0;
		for (const FMoodlet& M : List)
		{
			if (M.Id == TEXT("newFriend")) Rate += NewFriendMoralePerSecond;
		}
		if (Rate <= 0.0) return;
		const double Capped = FMath::Min(Rate, MoodletMoralePerSecondCap * List.Num());
		Morale = Clamp(MoraleOr50(Morale) + Capped * Dt, 0.0, 100.0);
	}

	double MoodletGoalBias(TArray<FMoodlet>& List, const FString& Goal, double Now)
	{
		PruneMoodlets(List, Now);
		double Score = 0.0;
		for (const FMoodlet& M : List)
		{
			if (M.Id != TEXT("newFriend")) continue;
			if (Goal == TEXT("socialize")) Score += NewFriendSocializeBias;
			else if (Goal == TEXT("visitFamily")) Score += 4.0;
			else if (Goal == TEXT("confront")) Score -= 4.0;
		}
		return Score;
	}
}
