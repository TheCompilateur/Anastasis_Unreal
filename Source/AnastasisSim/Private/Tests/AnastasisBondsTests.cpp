#include "Misc/AutomationTest.h"

#include "Life/AnastasisBonds.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisBondsParity
{
	namespace Vecteurs
	{
#include "AnastasisBondsVectors.inl"
	}

	double BondsFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	uint64 BondsToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	/** Les besoins de la declaration (bonds.mjs, `BESOINS`). */
	AnastasisNeeds::FNeeds NeedsAt(int32 Index)
	{
		static const double Table[][8] = {
			{ 10, 10, 90, 60, 90, 90, 95, 60 },
			{ 58, 10, 90, 60, 90, 90, 95, 60 },
			{ 57.9, 67.9, 48.1, 60, 90, 90, 95, 60 },
			{ 10, 10, 48, 60, 90, 90, 95, 60 },
			{ 70, 10, 90, 60, 90, 90, 95, 60 },
			{ 10, 10, 30, 60, 90, 90, 95, 60 },
			{ 60, 10, 90, 20, 90, 90, 95, 60 },
			{ 10, 10, 90, 60, 90, 90, 95, 10 },
			{ 20, 72, 90, 60, 90, 90, 95, 60 },
		};
		static const double Extra[8] = { 40, 40, 60, 60, 60, 60, 60, 38 };
		const double* R = Index == 100 ? Extra : Table[Index];
		AnastasisNeeds::FNeeds N;
		N.Hunger = R[0];
		N.Thirst = R[1];
		N.Energy = R[2];
		N.Social = R[3];
		N.Leisure = R[4];
		N.Hygiene = R[5];
		N.Health = R[6];
		N.Morale = R[7];
		return N;
	}

	TArray<double> Numbers(const FString& Csv)
	{
		TArray<FString> Parts;
		Csv.ParseIntoArray(Parts, TEXT(","), true);
		TArray<double> Out;
		for (const FString& P : Parts) Out.Add(FCString::Atod(*P));
		return Out;
	}

	AnastasisBonds::EPersonTag TagOf(double Trust)
	{
		return Trust >= 22.0 ? AnastasisBonds::EPersonTag::Ally
			: Trust <= -18.0 ? AnastasisBonds::EPersonTag::Rival
			: AnastasisBonds::EPersonTag::None;
	}

	const TCHAR* TagName(AnastasisBonds::EPersonTag Tag)
	{
		return Tag == AnastasisBonds::EPersonTag::Ally ? TEXT("ally") : Tag == AnastasisBonds::EPersonTag::Rival ? TEXT("rival") : TEXT("");
	}
}

/**
 * Les liens et les rumeurs — compare BIT A BIT a la reference executee
 * (`fee66ae`, extraction propre). Vecteurs : tools/migration/parity/bonds.mjs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisParityBondsTest,
	"Anastasis.Sim.Parite.Liens",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisParityBondsTest::RunTest(const FString&)
{
	using namespace AnastasisBondsParity;
	using namespace AnastasisBondsParity::Vecteurs;
	namespace B = AnastasisBonds;
	int32 Failures = 0;
	int32 Checked = 0;
	auto CheckD = [&](const TCHAR* Case, int32 I, const TCHAR* Field, double Got, uint64 Expected)
	{
		++Checked;
		if (BondsToBits(Got) != Expected)
		{
			++Failures;
			AddError(FString::Printf(TEXT("%s[%d].%s : %.17g attendu %.17g"), Case, I, Field, Got, BondsFromBits(Expected)));
		}
	};
	auto CheckI = [&](const TCHAR* Case, int32 I, const TCHAR* Field, int64 Got, int64 Expected)
	{
		++Checked;
		if (Got != Expected)
		{
			++Failures;
			AddError(FString::Printf(TEXT("%s[%d].%s : %lld attendu %lld"), Case, I, Field, Got, Expected));
		}
	};
	auto CheckS = [&](const TCHAR* Case, int32 I, const TCHAR* Field, const FString& Got, const ANSICHAR* Expected)
	{
		++Checked;
		const FString Want = UTF8_TO_TCHAR(Expected);
		if (Got != Want)
		{
			++Failures;
			AddError(FString::Printf(TEXT("%s[%d].%s : \"%s\" attendu \"%s\""), Case, I, Field, *Got, *Want));
		}
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(AffinityVectors); ++I)
	{
		const FAffinityVector& V = AffinityVectors[I];
		const FString Tag = UTF8_TO_TCHAR(V.A2);
		const B::EPersonTag T = Tag == TEXT("ally") ? B::EPersonTag::Ally : Tag == TEXT("rival") ? B::EPersonTag::Rival : B::EPersonTag::None;
		CheckD(TEXT("Affinity"), I, TEXT("score"), B::CompanionAffinity(BondsFromBits(V.A0Bits), BondsFromBits(V.A1Bits), T, V.A3 != 0, V.A4 != 0), V.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(TalkGainVectors); ++I)
	{
		const FTalkGainVector& V = TalkGainVectors[I];
		CheckI(TEXT("TalkGain"), I, TEXT("gain"), B::BondTalkGain(BondsFromBits(V.A0Bits) >= B::FriendAt), V.Attendu);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(CanStartVectors); ++I)
	{
		const FCanStartVector& V = CanStartVectors[I];
		// `canStartTalk` : cooldown de paire puis fatigue, rejoue par le village sur ses champs.
		const double Now = BondsFromBits(V.A0Bits);
		const double AAt = BondsFromBits(V.A1Bits);
		const double BAt = BondsFromBits(V.A2Bits);
		bool bOk = true;
		if (AAt >= 0.0 && Now - AAt < B::PairCooldownSeconds) bOk = false;
		if (BAt >= 0.0 && Now - BAt < B::PairCooldownSeconds) bOk = false;
		auto Level = [&](int32 Count, double At) { return Count > 0 && Now - At <= B::FatigueWindowSeconds ? Count : 0; };
		if (bOk) bOk = FMath::Max(Level(V.A3, BondsFromBits(V.A4Bits)), Level(V.A5, BondsFromBits(V.A6Bits))) < B::FatigueBlockCount;
		CheckI(TEXT("CanStart"), I, TEXT("ok"), bOk ? 1 : 0, V.Attendu);
		bool bVillage = AnastasisVillage::CanStartTalkFor(Now,
			AAt >= 0.0, AAt, BAt >= 0.0, BAt, V.A3, BondsFromBits(V.A4Bits), V.A5, BondsFromBits(V.A6Bits));
		CheckI(TEXT("CanStart"), I, TEXT("village"), bVillage ? 1 : 0, V.Attendu);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SpeakWorthVectors); ++I)
	{
		const FSpeakWorthVector& V = SpeakWorthVectors[I];
		const double Rel = BondsFromBits(V.A3Bits);
		const B::EBondKind Kind = B::BondKindBetween(Rel, 0.0, TEXT("settler"), TEXT("settler"));
		CheckD(TEXT("SpeakWorth"), I, TEXT("worth"), B::SpeakWorth(NeedsAt(V.A0), NeedsAt(V.A1), V.A2 != 0, Kind), V.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(ShouldSpeakVectors); ++I)
	{
		const FShouldSpeakVector& V = ShouldSpeakVectors[I];
		const double Worth = B::SpeakWorth(NeedsAt(0), NeedsAt(0), V.A4 != 0, B::EBondKind::Coworker);
		CheckI(TEXT("ShouldSpeak"), I, TEXT("speak"),
			B::ShouldSpeakNow(Worth, V.A3, UTF8_TO_TCHAR(V.A0), UTF8_TO_TCHAR(V.A1), BondsFromBits(V.A2Bits)) ? 1 : 0, V.Attendu);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(TurnsVectors); ++I)
	{
		const FTurnsVector& V = TurnsVectors[I];
		const AnastasisNeeds::FNeeds S = NeedsAt(V.A4);
		const AnastasisNeeds::FNeeds L = NeedsAt(0);
		const FString Goal = UTF8_TO_TCHAR(V.A5);
		const bool bUrgent = B::IsTalkUrgent(S) || B::IsTalkUrgent(L);
		const bool bWork = B::IsTalkWorkBusy(Goal, false);
		const B::EBondKind Kind = B::BondKindBetween(BondsFromBits(V.A2Bits), 0.0, TEXT("settler"), TEXT("settler"));
		CheckI(TEXT("Turns"), I, TEXT("turns"), B::TalkMaxTurns(UTF8_TO_TCHAR(V.A0), UTF8_TO_TCHAR(V.A1), bUrgent, bWork, Kind, V.A3), V.AttenduTurns);
		CheckD(TEXT("Turns"), I, TEXT("hold"), B::TalkHoldDuration(bUrgent, bWork), V.AttenduHoldBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(RefuseVectors); ++I)
	{
		const FRefuseVector& V = RefuseVectors[I];
		const B::EBondKind Kind = B::BondKindBetween(BondsFromBits(V.A1Bits), 0.0, TEXT("settler"), TEXT("settler"));
		CheckI(TEXT("Refuse"), I, TEXT("refuse"), B::RefusesReply(NeedsAt(V.A0), Kind, V.A2, static_cast<uint32>(BondsFromBits(V.A3Bits))) ? 1 : 0, V.Attendu);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(StageVectors); ++I)
	{
		const FStageVector& V = StageVectors[I];
		AnastasisVillage::FBondPair Pair;
		Pair.RelAB = BondsFromBits(V.A0Bits);
		Pair.RelBA = BondsFromBits(V.A1Bits);
		Pair.MoraleA = BondsFromBits(V.A4Bits);
		Pair.MoraleB = Pair.MoraleA;
		AnastasisVillage::BumpRelationPair(Pair, BondsFromBits(V.A2Bits), BondsFromBits(V.A3Bits), 50.0);
		CheckD(TEXT("Stage"), I, TEXT("relA"), Pair.RelAB, V.AttenduRelABits);
		CheckD(TEXT("Stage"), I, TEXT("relB"), Pair.RelBA, V.AttenduRelBBits);
		CheckI(TEXT("Stage"), I, TEXT("friendA"), Pair.MoodletsA.Num() > 0 ? 1 : 0, V.AttenduFriendA);
		CheckI(TEXT("Stage"), I, TEXT("friendB"), Pair.MoodletsB.Num() > 0 ? 1 : 0, V.AttenduFriendB);
		CheckD(TEXT("Stage"), I, TEXT("moraleA"), Pair.MoraleA, V.AttenduMoraleABits);
		CheckD(TEXT("Stage"), I, TEXT("moraleB"), Pair.MoraleB, V.AttenduMoraleBBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(MeetingVectors); ++I)
	{
		const FMeetingVector& V = MeetingVectors[I];
		TArray<B::FPersonRow> PeopleA;
		TArray<B::FPersonRow> PeopleB;
		TArray<B::FTomEntry> TomA;
		const FString GoalB = UTF8_TO_TCHAR(V.A1);
		for (int32 K = 0; K < V.A0; ++K)
		{
			const int32 Day = 1 + K / 4;
			B::NotePersonDirect(PeopleA, TEXT("npc-1"), Day, B::MeetTrust);
			B::NotePersonDirect(PeopleB, TEXT("npc-0"), Day, B::MeetTrust * 0.85);
			B::ObserveMind(TomA, TEXT("npc-1"), GoalB, 0.0, Day);
		}
		CheckD(TEXT("Meeting"), I, TEXT("trustA"), PeopleA[0].Trust, V.AttenduTrustABits);
		CheckS(TEXT("Meeting"), I, TEXT("tagA"), TagName(PeopleA[0].Tag), V.AttenduTagA);
		CheckI(TEXT("Meeting"), I, TEXT("meetsA"), PeopleA[0].Meets, V.AttenduMeetsA);
		CheckD(TEXT("Meeting"), I, TEXT("trustB"), PeopleB[0].Trust, V.AttenduTrustBBits);
		CheckS(TEXT("Meeting"), I, TEXT("tagB"), TagName(PeopleB[0].Tag), V.AttenduTagB);
		CheckD(TEXT("Meeting"), I, TEXT("tomConf"), TomA[0].Confidence, V.AttenduTomConfBits);
		CheckS(TEXT("Meeting"), I, TEXT("tomGoal"), TomA[0].EstimatedGoal, V.AttenduTomGoal);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(MemoryBiasVectors); ++I)
	{
		const FMemoryBiasVector& V = MemoryBiasVectors[I];
		TArray<B::FPersonRow> People;
		int32 N = 0;
		for (const double T : Numbers(UTF8_TO_TCHAR(V.A0)))
		{
			B::FPersonRow R;
			R.Id = FString::Printf(TEXT("npc-%d"), ++N);
			R.Trust = T;
			R.Tag = TagOf(T);
			R.Day = 1;
			R.Meets = 1;
			People.Add(R);
		}
		CheckD(TEXT("MemoryBias"), I, TEXT("bias"), B::SocialMemoryBiasSocialize(People), V.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SeekVectors); ++I)
	{
		const FSeekVector& V = SeekVectors[I];
		const TArray<double> Trusts = Numbers(UTF8_TO_TCHAR(V.A0));
		const TArray<double> Dists = Numbers(UTF8_TO_TCHAR(V.A1));
		TArray<FString> Goals;
		FString(UTF8_TO_TCHAR(V.A2)).ParseIntoArray(Goals, TEXT(","), true);
		TArray<B::FPersonRow> People;
		TArray<B::FTomEntry> Tom;
		for (int32 K = 0; K < Trusts.Num(); ++K)
		{
			B::FPersonRow R;
			R.Id = FString::Printf(TEXT("npc-%d"), K + 1);
			R.Trust = Trusts[K];
			R.Tag = TagOf(Trusts[K]);
			R.Day = 1;
			R.Meets = 1;
			People.Add(R);
			B::FTomEntry E;
			E.Id = R.Id;
			E.EstimatedGoal = Goals[K];
			E.Confidence = 0.35;
			E.Day = 1;
			Tom.Add(E);
		}
		const FString Got = B::PickRememberedSeek(People, Tom, TEXT("npc-0"), 10.5, 10.5, BondsFromBits(V.A3Bits),
			[&](const FString& Id, double& X, double& Y)
			{
				const int32 K = FCString::Atoi(*Id.RightChop(4)) - 1;
				if (!Dists.IsValidIndex(K)) return false;
				X = 10.5 + Dists[K];
				Y = 10.5;
				return true;
			});
		CheckS(TEXT("Seek"), I, TEXT("id"), Got, V.Attendu);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SpotRumorVectors); ++I)
	{
		const FSpotRumorVector& V = SpotRumorVectors[I];
		TArray<FString> HearsayParts;
		FString(UTF8_TO_TCHAR(V.A1)).ParseIntoArray(HearsayParts, TEXT(","), true);
		TArray<FString> KnownParts;
		FString(UTF8_TO_TCHAR(V.A2)).ParseIntoArray(KnownParts, TEXT(","), true);
		TArray<AnastasisVillage::FResourceSpot> Source;
		TArray<AnastasisVillage::FResourceSpot> Target;
		for (int32 K = 0; K < V.A0; ++K)
		{
			AnastasisVillage::FResourceSpot S;
			S.Key = FString::Printf(TEXT("%d,%d"), 4 + K, 7 + K);
			S.X = 4.5 + K;
			S.Y = 7.5 + K;
			S.Resource = K % 2 ? TEXT("wood") : TEXT("food");
			S.Amount = 10 + K;
			S.Day = 1 + K;
			S.bHearsay = HearsayParts.Contains(FString::FromInt(K));
			Source.Add(S);
			if (KnownParts.Contains(FString::FromInt(K)))
			{
				AnastasisVillage::FResourceSpot T = S;
				T.Resource = TEXT("food");
				T.Amount = 1;
				T.Day = 3;
				T.bHearsay = false;
				Target.Add(T);
			}
		}
		const TArray<AnastasisVillage::FSpotAct> Acts = AnastasisVillage::CreateInformSpotActs(Source, Target, TEXT("npc-0"), BondsFromBits(V.A3Bits), 2);
		for (const AnastasisVillage::FSpotAct& Act : Acts) AnastasisVillage::CommitHearsaySpot(Target, Act, 4, 12.5);
		TArray<FString> Added;
		const AnastasisVillage::FResourceSpot* First = nullptr;
		for (const AnastasisVillage::FResourceSpot& S : Target)
		{
			if (!S.bHearsay) continue;
			Added.Add(S.Key);
			if (!First) First = &S;
		}
		CheckS(TEXT("SpotRumor"), I, TEXT("keys"), FString::Join(Added, TEXT(";")), V.AttenduKeys);
		const FString FirstText = First
			? FString::Printf(TEXT("%s|%d|%d|%d|%s|%s|%d"), *First->Resource, First->Amount, First->Day, First->HopCount, *First->SourceId, *First->OriginalSourceId, First->ReceivedDay)
			: FString();
		CheckS(TEXT("SpotRumor"), I, TEXT("first"), FirstText, V.AttenduFirst);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(MoodletVectors); ++I)
	{
		const FMoodletVector& V = MoodletVectors[I];
		TArray<B::FMoodlet> List;
		double Morale = BondsFromBits(V.A0Bits);
		for (int32 K = 0; K < V.A3; ++K) B::StampNewFriend(List, Morale, 10.0 + K);
		const double Now = 10.0 + BondsFromBits(V.A1Bits);
		B::TickMoodlets(List, Morale, BondsFromBits(V.A2Bits), Now);
		CheckD(TEXT("Moodlet"), I, TEXT("morale"), Morale, V.AttenduMoraleBits);
		CheckD(TEXT("Moodlet"), I, TEXT("bias"), B::MoodletGoalBias(List, TEXT("socialize"), Now), V.AttenduBiasBits);
		CheckI(TEXT("Moodlet"), I, TEXT("count"), List.Num(), V.AttenduCount);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(DominantVectors); ++I)
	{
		const FDominantVector& V = DominantVectors[I];
		const B::FDominantNeed D = B::DominantNeed(NeedsAt(V.A0));
		CheckS(TEXT("Dominant"), I, TEXT("id"), D.Id, V.AttenduId);
		CheckD(TEXT("Dominant"), I, TEXT("value"), D.Value, V.AttenduValueBits);
	}

	AddInfo(FString::Printf(TEXT("Liens : %d valeurs comparees, %d ecarts"), Checked, Failures));
	return Failures == 0;
}

#endif
