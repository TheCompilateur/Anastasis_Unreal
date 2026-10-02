#include "Misc/AutomationTest.h"

#include "Life/AnastasisConditioning.h"
#include "Life/AnastasisGenome.h"
#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Parite des facteurs de besoins PAR HABITANT (mission needs-factors-001).
 *
 * Chaque vecteur est un appel de `tickNeeds` de la reference sur un habitant qui
 * porte un phenotype et un conditionnement, dans l'une des quatorze situations
 * de tools/migration/parity/needs-factors.mjs. Le C++ rejoue la meme chose dans
 * l'ordre de la reference : facteurs lus, branche (+ tickVitality), puis
 * tickConditioning sur les metres d'apres. On compare les huit metres et les
 * trois meres du conditionnement.
 */
namespace AnastasisNeedsFactorsParity
{
	namespace Vecteurs
	{
#include "AnastasisNeedsFactorsVectors.inl"
	}

	static double FactorsFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	static uint64 FactorsToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	/** `npcLieu` de la declaration : 2 = dans son foyer, 3 = chez autrui, 0 = dehors. */
	static double SleepQualityForPlace(int32 Place)
	{
		const FString Inside = Place == 3 ? TEXT("b2") : Place == 2 ? TEXT("b1") : TEXT("");
		const FString Home = Place >= 1 && Place <= 3 ? TEXT("b1") : TEXT("");
		return AnastasisNeeds::SleepQuality(Inside, Home, FString());
	}

	/**
	 * Une situation de la declaration : la branche de tickNeeds, et les deux
	 * drapeaux du conditionnement (but de travail, but de repos).
	 */
	static void TickSituation(
		int32 Code,
		AnastasisNeeds::FNeeds& N,
		double Dt,
		double DayFrac,
		int32 Place,
		const AnastasisNeeds::FNeedFactors& F,
		bool& bWorkGoal,
		bool& bRestGoal)
	{
		bWorkGoal = Code == 3 || Code == 11;
		bRestGoal = Code == 4 || Code == 5 || Code == 12;
		switch (Code)
		{
		case 0: AnastasisNeeds::TickNeeds(N, Dt, true, false, F); break;
		case 1:
		case 2:
		case 11:
		case 12: AnastasisNeeds::TickNeeds(N, Dt, false, false, F); break;
		case 3: AnastasisNeeds::TickNeeds(N, Dt, false, true, F); break;
		case 4:
		case 5:
		{
			const bool bNight = AnastasisRhythm::VillagePhase(DayFrac) == AnastasisRhythm::EPhase::Night;
			AnastasisNeeds::TickNeedsRestInside(N, Dt, bNight, SleepQualityForPlace(Place), F);
			break;
		}
		case 6: AnastasisNeeds::TickNeedsEatInside(N, Dt, F); break;
		case 7: AnastasisNeeds::TickNeedsSocialize(N, Dt, true, F); break;
		case 8: AnastasisNeeds::TickNeedsSocialize(N, Dt, false, F); break;
		case 9:
		case 13: AnastasisNeeds::TickNeedsRelax(N, Dt, F); break;
		case 10: AnastasisNeeds::TickNeedsRelieveInside(N, Dt, F); break;
		default: break;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisNeedsFactorsParityTest,
	"Anastasis.Sim.Parite.BesoinsFacteurs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisNeedsFactorsParityTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisNeedsFactorsParity;
	namespace V = AnastasisNeedsFactorsParity::Vecteurs;

	int32 Compared = 0;
	int32 Errors = 0;
	auto Check = [this, &Compared, &Errors](int32 Index, int32 Code, const TCHAR* Field, double Got, uint64 Expected)
	{
		Compared += 1;
		if (P::FactorsToBits(Got) != Expected)
		{
			// Une branche fausse rend des centaines d'ecarts : les vingt premiers suffisent.
			if (++Errors <= 20)
			{
				AddError(FString::Printf(TEXT("TickNeedsFactors[%d] (situation %d).%s : attendu %016llx, obtenu %016llx"),
					Index, Code, Field, Expected, P::FactorsToBits(Got)));
			}
		}
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::TickNeedsFactorsVectors); ++I)
	{
		const V::FTickNeedsFactorsVector& Vec = V::TickNeedsFactorsVectors[I];
		AnastasisNeeds::FNeeds N;
		N.Hunger = P::FactorsFromBits(Vec.A0Bits);
		N.Energy = P::FactorsFromBits(Vec.A1Bits);
		N.Social = P::FactorsFromBits(Vec.A2Bits);
		N.Leisure = P::FactorsFromBits(Vec.A3Bits);
		N.Hygiene = P::FactorsFromBits(Vec.A4Bits);
		N.Thirst = P::FactorsFromBits(Vec.A5Bits);
		N.Health = P::FactorsFromBits(Vec.A6Bits);
		N.Morale = P::FactorsFromBits(Vec.A7Bits);

		const bool bAbsent = Vec.A11 != 0;
		AnastasisGenome::FPhenotype Phenotype;
		Phenotype.HydrationLossMultiplier = P::FactorsFromBits(Vec.A12Bits);
		Phenotype.MetabolicDemandMultiplier = P::FactorsFromBits(Vec.A13Bits);
		Phenotype.FatigueRecoveryMultiplier = P::FactorsFromBits(Vec.A14Bits);
		AnastasisConditioning::FConditioning Conditioning;
		if (!bAbsent)
		{
			Conditioning.WorkConditioning = P::FactorsFromBits(Vec.A15Bits);
			Conditioning.FatigueAdaptation = P::FactorsFromBits(Vec.A16Bits);
			Conditioning.RecoveryConditioning = P::FactorsFromBits(Vec.A17Bits);
		}

		// Lus AVANT le tick, comme les `const ...Mul` de tete de tickNeeds.
		const AnastasisNeeds::FNeedFactors Factors = bAbsent
			? AnastasisNeeds::NeedFactorsFor(nullptr, nullptr)
			: AnastasisNeeds::NeedFactorsFor(&Phenotype, &Conditioning);

		const double Dt = P::FactorsFromBits(Vec.A18Bits);
		bool bWorkGoal = false;
		bool bRestGoal = false;
		P::TickSituation(Vec.A8, N, Dt, P::FactorsFromBits(Vec.A9Bits), Vec.A10, Factors, bWorkGoal, bRestGoal);
		// Absent : `ensureConditioning` le cree neutre dans tickConditioning, comme ici.
		AnastasisNeeds::TickNeedsConditioning(Conditioning, N, Dt, bWorkGoal, bRestGoal);

		Check(I, Vec.A8, TEXT("hunger"), N.Hunger, Vec.AttenduHungerBits);
		Check(I, Vec.A8, TEXT("energy"), N.Energy, Vec.AttenduEnergyBits);
		Check(I, Vec.A8, TEXT("social"), N.Social, Vec.AttenduSocialBits);
		Check(I, Vec.A8, TEXT("leisure"), N.Leisure, Vec.AttenduLeisureBits);
		Check(I, Vec.A8, TEXT("hygiene"), N.Hygiene, Vec.AttenduHygieneBits);
		Check(I, Vec.A8, TEXT("thirst"), N.Thirst, Vec.AttenduThirstBits);
		Check(I, Vec.A8, TEXT("health"), N.Health, Vec.AttenduHealthBits);
		Check(I, Vec.A8, TEXT("morale"), N.Morale, Vec.AttenduMoraleBits);
		Check(I, Vec.A8, TEXT("workConditioning"), Conditioning.WorkConditioning, Vec.AttenduWorkConditioningBits);
		Check(I, Vec.A8, TEXT("fatigueAdaptation"), Conditioning.FatigueAdaptation, Vec.AttenduFatigueAdaptationBits);
		Check(I, Vec.A8, TEXT("recoveryConditioning"), Conditioning.RecoveryConditioning, Vec.AttenduRecoveryConditioningBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::NeedsCriticalVectors); ++I)
	{
		const V::FNeedsCriticalVector& Vec = V::NeedsCriticalVectors[I];
		AnastasisNeeds::FNeeds N;
		N.Hunger = P::FactorsFromBits(Vec.A0Bits);
		N.Energy = P::FactorsFromBits(Vec.A1Bits);
		N.Social = P::FactorsFromBits(Vec.A2Bits);
		N.Leisure = P::FactorsFromBits(Vec.A3Bits);
		N.Hygiene = P::FactorsFromBits(Vec.A4Bits);
		N.Thirst = P::FactorsFromBits(Vec.A5Bits);
		N.Health = P::FactorsFromBits(Vec.A6Bits);
		N.Morale = P::FactorsFromBits(Vec.A7Bits);
		Compared += 1;
		if (AnastasisNeeds::AreNeedsCritical(N) != (Vec.Attendu != 0))
		{
			AddError(FString::Printf(TEXT("NeedsCritical[%d] : attendu %d"), I, Vec.Attendu));
		}
	}

	// Le defaut ne change aucun bit : multiplier par 1,0 est exact.
	{
		const AnastasisNeeds::FNeedFactors Neutral = AnastasisNeeds::NeedFactorsFor(nullptr, nullptr);
		TestTrue(TEXT("facteurs absents = 1 partout"),
			Neutral.Hydration == 1.0 && Neutral.Metabolic == 1.0 && Neutral.FatigueRecovery == 1.0
			&& Neutral.FatigueAdaptation == 1.0 && Neutral.RecoveryConditioning == 1.0);
	}

	if (Errors > 20)
	{
		AddError(FString::Printf(TEXT("... %d ecarts en tout"), Errors));
	}
	AddInfo(FString::Printf(TEXT("%d valeurs comparees, %d vecteurs de tick"),
		Compared, static_cast<int32>(UE_ARRAY_COUNT(V::TickNeedsFactorsVectors))));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
