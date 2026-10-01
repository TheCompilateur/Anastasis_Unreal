#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"

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

	/** Les cas de lieu de tools/migration/parity/needs.mjs (`npcLieu`). */
	struct FLieu
	{
		FString Inside;
		FString Home;
		FString Shelter;
	};

	FLieu LieuOf(int32 Case)
	{
		FLieu Out;
		if (Case >= 2) Out.Inside = Case == 3 ? TEXT("b2") : TEXT("b1");
		if (Case == 1 || Case == 2 || Case == 3) Out.Home = TEXT("b1");
		if (Case == 4) Out.Shelter = TEXT("b1");
		return Out;
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
	using AnastasisNeedsParity::FLieu;
	using AnastasisNeedsParity::LieuOf;
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

	for (int32 I = 0; I < UE_ARRAY_COUNT(TickNeedsRestVectors); ++I)
	{
		const FTickNeedsRestVector& V = TickNeedsRestVectors[I];
		AnastasisNeeds::FNeeds N = NeedsFromVector(V.A0Bits, V.A1Bits, V.A2Bits, V.A3Bits, V.A4Bits, V.A5Bits, V.A6Bits, V.A7Bits);
		const bool bNight = AnastasisRhythm::VillagePhase(NeedsFromBits(V.A8Bits)) == AnastasisRhythm::EPhase::Night;
		// Foyer (lieu 2) ou chez autrui (lieu 3) : la qualite vient de sleepQuality.
		const FLieu Lieu = LieuOf(V.A9 != 0 ? 2 : 3);
		const double Quality = AnastasisNeeds::SleepQuality(Lieu.Inside, Lieu.Home, Lieu.Shelter);
		AnastasisNeeds::TickNeedsRestInside(N, NeedsFromBits(V.A10Bits), bNight, Quality);
		Check(TEXT("TickNeedsRest"), I, TEXT("hunger"), N.Hunger, V.AttenduHungerBits);
		Check(TEXT("TickNeedsRest"), I, TEXT("energy"), N.Energy, V.AttenduEnergyBits);
		Check(TEXT("TickNeedsRest"), I, TEXT("social"), N.Social, V.AttenduSocialBits);
		Check(TEXT("TickNeedsRest"), I, TEXT("leisure"), N.Leisure, V.AttenduLeisureBits);
		Check(TEXT("TickNeedsRest"), I, TEXT("hygiene"), N.Hygiene, V.AttenduHygieneBits);
		Check(TEXT("TickNeedsRest"), I, TEXT("thirst"), N.Thirst, V.AttenduThirstBits);
		Check(TEXT("TickNeedsRest"), I, TEXT("health"), N.Health, V.AttenduHealthBits);
		Check(TEXT("TickNeedsRest"), I, TEXT("morale"), N.Morale, V.AttenduMoraleBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SatisfyRestVectors); ++I)
	{
		const FSatisfyRestVector& V = SatisfyRestVectors[I];
		AnastasisNeeds::FNeeds N;
		N.Energy = NeedsFromBits(V.A0Bits);
		N.Leisure = NeedsFromBits(V.A1Bits);
		N.Morale = NeedsFromBits(V.A2Bits);
		const bool bNight = AnastasisRhythm::VillagePhase(NeedsFromBits(V.A3Bits)) == AnastasisRhythm::EPhase::Night;
		const FLieu Lieu = LieuOf(V.A4);
		const FString Living = !Lieu.Home.IsEmpty() ? Lieu.Home : Lieu.Shelter;
		const bool bAtHome = !Living.IsEmpty() && Lieu.Inside == Living; // `isAtHome`
		AnastasisNeeds::SatisfyRest(N, bNight, AnastasisNeeds::SleepQuality(Lieu.Inside, Lieu.Home, Lieu.Shelter), !Lieu.Inside.IsEmpty(), bAtHome);
		Check(TEXT("SatisfyRest"), I, TEXT("energy"), N.Energy, V.AttenduEnergyBits);
		Check(TEXT("SatisfyRest"), I, TEXT("leisure"), N.Leisure, V.AttenduLeisureBits);
		Check(TEXT("SatisfyRest"), I, TEXT("morale"), N.Morale, V.AttenduMoraleBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(TickNeedsEatVectors); ++I)
	{
		const FTickNeedsEatVector& V = TickNeedsEatVectors[I];
		AnastasisNeeds::FNeeds N = NeedsFromVector(V.A0Bits, V.A1Bits, V.A2Bits, V.A3Bits, V.A4Bits, V.A5Bits, V.A6Bits, V.A7Bits);
		AnastasisNeeds::TickNeedsEatInside(N, NeedsFromBits(V.A8Bits));
		Check(TEXT("TickNeedsEat"), I, TEXT("hunger"), N.Hunger, V.AttenduHungerBits);
		Check(TEXT("TickNeedsEat"), I, TEXT("energy"), N.Energy, V.AttenduEnergyBits);
		Check(TEXT("TickNeedsEat"), I, TEXT("social"), N.Social, V.AttenduSocialBits);
		Check(TEXT("TickNeedsEat"), I, TEXT("leisure"), N.Leisure, V.AttenduLeisureBits);
		Check(TEXT("TickNeedsEat"), I, TEXT("hygiene"), N.Hygiene, V.AttenduHygieneBits);
		Check(TEXT("TickNeedsEat"), I, TEXT("thirst"), N.Thirst, V.AttenduThirstBits);
		Check(TEXT("TickNeedsEat"), I, TEXT("health"), N.Health, V.AttenduHealthBits);
		Check(TEXT("TickNeedsEat"), I, TEXT("morale"), N.Morale, V.AttenduMoraleBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SatisfyEatVectors); ++I)
	{
		const FSatisfyEatVector& V = SatisfyEatVectors[I];
		AnastasisNeeds::FNeeds N;
		N.Hunger = NeedsFromBits(V.A0Bits);
		N.Morale = NeedsFromBits(V.A1Bits);
		N.Leisure = NeedsFromBits(V.A2Bits);
		N.Health = NeedsFromBits(V.A3Bits);
		N.Hygiene = NeedsFromBits(V.A4Bits);
		const FLieu Lieu = LieuOf(V.A5);
		const FString Living = !Lieu.Home.IsEmpty() ? Lieu.Home : Lieu.Shelter;
		const bool bAtHome = !Living.IsEmpty() && Lieu.Inside == Living; // `isAtHome`
		AnastasisNeeds::SatisfyEat(N, !Lieu.Inside.IsEmpty(), bAtHome);
		Check(TEXT("SatisfyEat"), I, TEXT("hunger"), N.Hunger, V.AttenduHungerBits);
		Check(TEXT("SatisfyEat"), I, TEXT("morale"), N.Morale, V.AttenduMoraleBits);
		Check(TEXT("SatisfyEat"), I, TEXT("leisure"), N.Leisure, V.AttenduLeisureBits);
		Check(TEXT("SatisfyEat"), I, TEXT("health"), N.Health, V.AttenduHealthBits);
		Check(TEXT("SatisfyEat"), I, TEXT("hygiene"), N.Hygiene, V.AttenduHygieneBits);
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

	auto CheckAll = [&](const TCHAR* Case, int32 I, const AnastasisNeeds::FNeeds& N,
		uint64 H, uint64 E, uint64 S, uint64 L, uint64 Y, uint64 T, uint64 He, uint64 M)
	{
		Check(Case, I, TEXT("hunger"), N.Hunger, H);
		Check(Case, I, TEXT("energy"), N.Energy, E);
		Check(Case, I, TEXT("social"), N.Social, S);
		Check(Case, I, TEXT("leisure"), N.Leisure, L);
		Check(Case, I, TEXT("hygiene"), N.Hygiene, Y);
		Check(Case, I, TEXT("thirst"), N.Thirst, T);
		Check(Case, I, TEXT("health"), N.Health, He);
		Check(Case, I, TEXT("morale"), N.Morale, M);
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(TickNeedsSocialVectors); ++I)
	{
		const FTickNeedsSocialVector& V = TickNeedsSocialVectors[I];
		AnastasisNeeds::FNeeds N = NeedsFromVector(V.A0Bits, V.A1Bits, V.A2Bits, V.A3Bits, V.A4Bits, V.A5Bits, V.A6Bits, V.A7Bits);
		AnastasisNeeds::TickNeedsSocialize(N, NeedsFromBits(V.A9Bits), V.A8 != 0);
		CheckAll(TEXT("TickNeedsSocial"), I, N, V.AttenduHungerBits, V.AttenduEnergyBits, V.AttenduSocialBits, V.AttenduLeisureBits,
			V.AttenduHygieneBits, V.AttenduThirstBits, V.AttenduHealthBits, V.AttenduMoraleBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(TickNeedsRelaxVectors); ++I)
	{
		const FTickNeedsRelaxVector& V = TickNeedsRelaxVectors[I];
		AnastasisNeeds::FNeeds N = NeedsFromVector(V.A0Bits, V.A1Bits, V.A2Bits, V.A3Bits, V.A4Bits, V.A5Bits, V.A6Bits, V.A7Bits);
		AnastasisNeeds::TickNeedsRelax(N, NeedsFromBits(V.A9Bits));
		CheckAll(TEXT("TickNeedsRelax"), I, N, V.AttenduHungerBits, V.AttenduEnergyBits, V.AttenduSocialBits, V.AttenduLeisureBits,
			V.AttenduHygieneBits, V.AttenduThirstBits, V.AttenduHealthBits, V.AttenduMoraleBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SatisfySocialVectors); ++I)
	{
		const FSatisfySocialVector& V = SatisfySocialVectors[I];
		AnastasisNeeds::FNeeds N;
		N.Social = NeedsFromBits(V.A0Bits);
		N.Morale = NeedsFromBits(V.A1Bits);
		N.Leisure = NeedsFromBits(V.A2Bits);
		AnastasisNeeds::SatisfySocial(N, NeedsFromBits(V.A3Bits), V.A4 != 0);
		Check(TEXT("SatisfySocial"), I, TEXT("social"), N.Social, V.AttenduSocialBits);
		Check(TEXT("SatisfySocial"), I, TEXT("morale"), N.Morale, V.AttenduMoraleBits);
		Check(TEXT("SatisfySocial"), I, TEXT("leisure"), N.Leisure, V.AttenduLeisureBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SatisfyRelaxVectors); ++I)
	{
		const FSatisfyRelaxVector& V = SatisfyRelaxVectors[I];
		AnastasisNeeds::FNeeds N;
		N.Leisure = NeedsFromBits(V.A0Bits);
		N.Energy = NeedsFromBits(V.A1Bits);
		N.Morale = NeedsFromBits(V.A2Bits);
		AnastasisNeeds::SatisfyRelax(N, V.A3 != 0);
		Check(TEXT("SatisfyRelax"), I, TEXT("leisure"), N.Leisure, V.AttenduLeisureBits);
		Check(TEXT("SatisfyRelax"), I, TEXT("energy"), N.Energy, V.AttenduEnergyBits);
		Check(TEXT("SatisfyRelax"), I, TEXT("morale"), N.Morale, V.AttenduMoraleBits);
	}

	const int32 Total = UE_ARRAY_COUNT(UrgeScoreVectors) + UE_ARRAY_COUNT(NeedGoalScoresVectors)
		+ UE_ARRAY_COUNT(TickNeedsVectors) + UE_ARRAY_COUNT(SatisfyDrinkVectors)
		+ UE_ARRAY_COUNT(TickNeedsRestVectors) + UE_ARRAY_COUNT(SatisfyRestVectors)
		+ UE_ARRAY_COUNT(TickNeedsEatVectors) + UE_ARRAY_COUNT(SatisfyEatVectors)
		+ UE_ARRAY_COUNT(TickNeedsSocialVectors) + UE_ARRAY_COUNT(TickNeedsRelaxVectors)
		+ UE_ARRAY_COUNT(SatisfySocialVectors) + UE_ARRAY_COUNT(SatisfyRelaxVectors);
	AddInfo(FString::Printf(TEXT("Besoins : %d vecteurs, %d ecarts"), Total, Failures));
	return Failures == 0;
}

#endif
