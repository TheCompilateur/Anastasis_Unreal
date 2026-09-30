#include "Misc/AutomationTest.h"

#include "Ai/AnastasisNous.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisNousParity
{
	namespace Vecteurs
	{
#include "AnastasisNousVectors.inl"
#include "AnastasisNousInertiaVectors.inl"
	}

	double NousFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	uint64 NousToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}
}

/**
 * Noûs, la decision du cycle faim — compare BIT A BIT a `src/ai/algorithmic`
 * execute. Vecteurs : tools/migration/parity/nous.mjs, nous-inertia.mjs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisParityNousTest,
	"Anastasis.Sim.Parite.Nous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisParityNousTest::RunTest(const FString&)
{
	using AnastasisNousParity::NousFromBits;
	using AnastasisNousParity::NousToBits;
	using namespace AnastasisNousParity::Vecteurs;
	int32 Failures = 0;

	auto CheckD = [&](int32 I, const TCHAR* Field, double Got, uint64 Expected)
	{
		if (NousToBits(Got) != Expected)
		{
			++Failures;
			AddError(FString::Printf(TEXT("ScoreHunger[%d].%s : %.17g attendu %.17g"), I, Field, Got, NousFromBits(Expected)));
		}
	};
	auto CheckS = [&](const TCHAR* Case, int32 I, const TCHAR* Field, const FString& Got, const ANSICHAR* Expected)
	{
		const FString Want = UTF8_TO_TCHAR(Expected);
		if (Got != Want)
		{
			++Failures;
			AddError(FString::Printf(TEXT("%s[%d].%s : %s attendu %s"), Case, I, Field, *Got, *Want));
		}
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(ScoreHungerVectors); ++I)
	{
		const FScoreHungerVector& V = ScoreHungerVectors[I];
		AnastasisNous::FFoodContext C;
		C.Hunger = NousFromBits(V.A0Bits);
		C.InventoryFood = V.A2;
		C.BestSourceBuildingId = UTF8_TO_TCHAR(V.A3);
		C.BestSourceDistance = NousFromBits(V.A4Bits);
		C.BestSourceEstimated = NousFromBits(V.A5Bits);
		C.Certainty = NousFromBits(V.A6Bits);
		C.BestSourceConfidence = C.Certainty;
		C.Gold = V.A7;
		C.BelievedFood = NousFromBits(V.A8Bits);
		AnastasisNous::FHungerSubject S;
		S.Energy = NousFromBits(V.A1Bits);
		S.Speed = 4.0;
		TArray<FString> Exclude;
		const FString Ex = UTF8_TO_TCHAR(V.A9);
		if (!Ex.IsEmpty()) Exclude.Add(Ex);

		const AnastasisNous::FScored R = AnastasisNous::ScoreHungerCandidates(C, S, 12.5, Exclude);
		auto ScoreOf = [&](const TCHAR* Type)
		{
			for (const AnastasisNous::FDecision& D : R.Candidates)
			{
				if (D.Type == Type) return D.Score;
			}
			return -1.0;
		};
		TArray<FString> Order;
		for (const AnastasisNous::FDecision& D : R.Candidates) Order.Add(D.Type);

		CheckS(TEXT("ScoreHunger"), I, TEXT("bestType"), R.bHasBest ? R.Best.Type : FString(), V.AttenduBestType);
		CheckD(I, TEXT("bestScore"), R.bHasBest ? R.Best.Score : -1.0, V.AttenduBestScoreBits);
		CheckD(I, TEXT("bestUrgency"), R.bHasBest ? R.Best.Urgency : -1.0, V.AttenduBestUrgencyBits);
		CheckS(TEXT("ScoreHunger"), I, TEXT("order"), FString::Join(Order, TEXT(",")), V.AttenduOrder);
		CheckD(I, TEXT("eat"), ScoreOf(TEXT("eat")), V.AttenduEatBits);
		CheckD(I, TEXT("seek"), ScoreOf(TEXT("seek_food")), V.AttenduSeekBits);
		CheckD(I, TEXT("buy"), ScoreOf(TEXT("buy_food")), V.AttenduBuyBits);
		CheckD(I, TEXT("work"), ScoreOf(TEXT("work")), V.AttenduWorkBits);
		CheckD(I, TEXT("sleep"), ScoreOf(TEXT("sleep")), V.AttenduSleepBits);
		CheckD(I, TEXT("wait"), ScoreOf(TEXT("wait")), V.AttenduWaitBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(InertiaVectors); ++I)
	{
		const FInertiaVector& V = InertiaVectors[I];
		AnastasisNous::FDecision Current;
		Current.Type = UTF8_TO_TCHAR(V.A0);
		Current.Score = NousFromBits(V.A1Bits);
		AnastasisNous::FDecision Candidate;
		Candidate.Type = UTF8_TO_TCHAR(V.A2);
		Candidate.Score = NousFromBits(V.A3Bits);
		Candidate.Urgency = NousFromBits(V.A4Bits);
		FString Reason;
		const bool bKeep = AnastasisNous::EvaluateInertia(&Current, &Candidate, NousFromBits(V.A5Bits), NousFromBits(V.A6Bits), 0.0, Reason);
		if (bKeep != (V.AttenduKeep != 0))
		{
			++Failures;
			AddError(FString::Printf(TEXT("Inertia[%d].keep : %d attendu %d"), I, bKeep ? 1 : 0, V.AttenduKeep));
		}
		CheckS(TEXT("Inertia"), I, TEXT("reason"), Reason, V.AttenduReason);
	}

	const int32 Total = UE_ARRAY_COUNT(ScoreHungerVectors) + UE_ARRAY_COUNT(InertiaVectors);
	AddInfo(FString::Printf(TEXT("Nous : %d vecteurs, %d ecarts"), Total, Failures));
	return Failures == 0;
}

#endif
