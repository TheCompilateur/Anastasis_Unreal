#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisNeedsParity
{
	namespace Vecteurs
	{
#include "AnastasisNeedsVectors.inl"
	}

	double NeedsFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	uint64 NeedsToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	AnastasisNeeds::FNeeds NeedsFromVector(uint64 H, uint64 E, uint64 S, uint64 L, uint64 Y, uint64 T, uint64 He, uint64 M)
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = NeedsFromBits(H);
		N.Energy = NeedsFromBits(E);
		N.Social = NeedsFromBits(S);
		N.Leisure = NeedsFromBits(L);
		N.Hygiene = NeedsFromBits(Y);
		N.Thirst = NeedsFromBits(T);
		N.Health = NeedsFromBits(He);
		N.Morale = NeedsFromBits(M);
		return N;
	}
}

/**
 * Les besoins que met en jeu le premier batiment (le puits), compares BIT A BIT
 * a `src/life/needs.js` executee. Vecteurs : tools/migration/parity/needs.mjs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisParityNeedsTest,
	"Anastasis.Sim.Parite.Besoins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisParityNeedsTest::RunTest(const FString&)
{
	using AnastasisNeedsParity::NeedsFromBits;
	using AnastasisNeedsParity::NeedsToBits;
	using AnastasisNeedsParity::NeedsFromVector;
	using namespace AnastasisNeedsParity::Vecteurs;
	int32 Failures = 0;
	auto Check = [&](const TCHAR* Case, int32 Index, const TCHAR* Field, double Got, uint64 Expected)
	{
		if (NeedsToBits(Got) != Expected)
		{
			++Failures;
			AddError(FString::Printf(
				TEXT("%s[%d].%s : %.17g (0x%016llx) attendu %.17g (0x%016llx)"),
				Case, Index, Field, Got, NeedsToBits(Got), NeedsFromBits(Expected), Expected));
		}
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(UrgeScoreVectors); ++I)
	{
		const FUrgeScoreVector& V = UrgeScoreVectors[I];
		Check(TEXT("UrgeScore"), I, TEXT("score"),
			AnastasisNeeds::UrgeScore(NeedsFromBits(V.A0Bits), NeedsFromBits(V.A1Bits), NeedsFromBits(V.A2Bits)), V.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(NeedGoalScoresVectors); ++I)
	{
		const FNeedGoalScoresVector& V = NeedGoalScoresVectors[I];
		const AnastasisNeeds::FNeeds N = NeedsFromVector(V.A0Bits, V.A1Bits, V.A2Bits, V.A3Bits, V.A4Bits, V.A5Bits, V.A6Bits, V.A7Bits);
		const AnastasisNeeds::FNeedGoalScores S = AnastasisNeeds::NeedGoalScores(N, V.A8, V.A9);
		Check(TEXT("NeedGoalScores"), I, TEXT("eat"), S.Eat, V.AttenduEatBits);
		Check(TEXT("NeedGoalScores"), I, TEXT("rest"), S.Rest, V.AttenduRestBits);
		Check(TEXT("NeedGoalScores"), I, TEXT("socialize"), S.Socialize, V.AttenduSocializeBits);
		Check(TEXT("NeedGoalScores"), I, TEXT("relax"), S.Relax, V.AttenduRelaxBits);
		Check(TEXT("NeedGoalScores"), I, TEXT("relieve"), S.Relieve, V.AttenduRelieveBits);
		Check(TEXT("NeedGoalScores"), I, TEXT("drink"), S.Drink, V.AttenduDrinkBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(TickNeedsVectors); ++I)
	{
		const FTickNeedsVector& V = TickNeedsVectors[I];
		AnastasisNeeds::FNeeds N = NeedsFromVector(V.A0Bits, V.A1Bits, V.A2Bits, V.A3Bits, V.A4Bits, V.A5Bits, V.A6Bits, V.A7Bits);
		const FString Goal = UTF8_TO_TCHAR(V.A8);
		const bool bAtWater = V.A9 != 0;
		// Ce que calcule la reference en tete de tickNeeds, hors interieur.
		const bool bDrinking = Goal == TEXT("drink") && bAtWater;
		const bool bWorking = Goal == TEXT("gatherWood");
		AnastasisNeeds::TickNeeds(N, NeedsFromBits(V.A10Bits), bDrinking, bWorking);
		Check(TEXT("TickNeeds"), I, TEXT("hunger"), N.Hunger, V.AttenduHungerBits);
		Check(TEXT("TickNeeds"), I, TEXT("energy"), N.Energy, V.AttenduEnergyBits);
		Check(TEXT("TickNeeds"), I, TEXT("social"), N.Social, V.AttenduSocialBits);
		Check(TEXT("TickNeeds"), I, TEXT("leisure"), N.Leisure, V.AttenduLeisureBits);
		Check(TEXT("TickNeeds"), I, TEXT("hygiene"), N.Hygiene, V.AttenduHygieneBits);
		Check(TEXT("TickNeeds"), I, TEXT("thirst"), N.Thirst, V.AttenduThirstBits);
		Check(TEXT("TickNeeds"), I, TEXT("health"), N.Health, V.AttenduHealthBits);
		Check(TEXT("TickNeeds"), I, TEXT("morale"), N.Morale, V.AttenduMoraleBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SatisfyDrinkVectors); ++I)
	{
		const FSatisfyDrinkVector& V = SatisfyDrinkVectors[I];
		AnastasisNeeds::FNeeds N;
		N.Thirst = NeedsFromBits(V.A0Bits);
		N.Hygiene = NeedsFromBits(V.A1Bits);
		N.Morale = NeedsFromBits(V.A2Bits);
		N.Health = NeedsFromBits(V.A3Bits);
		AnastasisNeeds::SatisfyDrink(N);
		Check(TEXT("SatisfyDrink"), I, TEXT("thirst"), N.Thirst, V.AttenduThirstBits);
		Check(TEXT("SatisfyDrink"), I, TEXT("hygiene"), N.Hygiene, V.AttenduHygieneBits);
		Check(TEXT("SatisfyDrink"), I, TEXT("morale"), N.Morale, V.AttenduMoraleBits);
		Check(TEXT("SatisfyDrink"), I, TEXT("health"), N.Health, V.AttenduHealthBits);
	}

	const int32 Total = UE_ARRAY_COUNT(UrgeScoreVectors) + UE_ARRAY_COUNT(NeedGoalScoresVectors)
		+ UE_ARRAY_COUNT(TickNeedsVectors) + UE_ARRAY_COUNT(SatisfyDrinkVectors);
	AddInfo(FString::Printf(TEXT("Besoins : %d vecteurs, %d ecarts"), Total, Failures));
	return Failures == 0;
}

#endif
