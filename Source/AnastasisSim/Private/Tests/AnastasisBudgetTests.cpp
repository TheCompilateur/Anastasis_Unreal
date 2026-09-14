#include "Misc/AutomationTest.h"

#include "Core/AnastasisSimBudget.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Parite du noyau causal du directeur de budget.
 *
 * Vecteurs declares dans tools/migration/parity/simulation-budget.mjs et emis
 * par tools/migration/gen-parity.mjs. Ne jamais corriger une valeur attendue a
 * la main: soit le portage a devie, soit la reference a change.
 */
namespace AnastasisBudgetParity
{
	static double FromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(double));
		return Value;
	}

	static uint64 ToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(double));
		return Bits;
	}

	#include "AnastasisBudgetVectors.inl"
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisBudgetParityTest,
	"Anastasis.Sim.Parite.Budget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisBudgetParityTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisBudgetParity;

	for (const FPressureTierVector& Vecteur : PressureTierVectors)
	{
		const double Pression = FromBits(Vecteur.A0Bits);
		const FString Obtenu = AnastasisBudget::TierName(AnastasisBudget::PressureTier(Pression));
		TestEqual(
			*FString::Printf(TEXT("palier pour %.9g"), Pression),
			Obtenu, FString(UTF8_TO_TCHAR(Vecteur.Attendu)));
	}

	for (const FBudgetMultipliersVector& Vecteur : BudgetMultipliersVectors)
	{
		const double Pression = FromBits(Vecteur.A0Bits);
		const AnastasisBudget::FMultipliers M = AnastasisBudget::Multipliers(Pression);
		const TTuple<const TCHAR*, double, uint64> Champs[] = {
			{ TEXT("pathBudgetMul"), M.PathBudgetMul, Vecteur.AttenduPathBudgetMulBits },
			{ TEXT("animalTickMul"), M.AnimalTickMul, Vecteur.AttenduAnimalTickMulBits },
			{ TEXT("transportTickMul"), M.TransportTickMul, Vecteur.AttenduTransportTickMulBits },
		};
		for (const auto& Champ : Champs)
		{
			if (ToBits(Champ.Get<1>()) != Champ.Get<2>())
			{
				AddError(FString::Printf(
					TEXT("%s pour pression %.9g: attendu %016llx, obtenu %016llx"),
					Champ.Get<0>(), Pression, Champ.Get<2>(), ToBits(Champ.Get<1>())));
			}
		}
	}

	for (const FBandIntervalVector& Vecteur : BandIntervalVectors)
	{
		const double Pression = FromBits(Vecteur.A0Bits);
		const FString NomBande = UTF8_TO_TCHAR(Vecteur.A1);
		const AnastasisBudget::EBand Bande = AnastasisBudget::BandFromName(NomBande);
		const double Obtenu = AnastasisBudget::IntervalForBand(Pression, Bande);
		if (ToBits(Obtenu) != Vecteur.AttenduBits)
		{
			AddError(FString::Printf(
				TEXT("intervalle %s pour pression %.9g: attendu %016llx, obtenu %016llx"),
				*NomBande, Pression, Vecteur.AttenduBits, ToBits(Obtenu)));
		}
	}

	for (const FNpcBandVector& Vecteur : NpcBandVectors)
	{
		AnastasisBudget::FDirector Directeur;
		Directeur.Pressure = FromBits(Vecteur.A0Bits);
		Directeur.ViewX = FromBits(Vecteur.A1Bits);
		Directeur.ViewY = FromBits(Vecteur.A2Bits);

		AnastasisBudget::FNpcView Npc;
		Npc.X = FromBits(Vecteur.A3Bits);
		Npc.Y = FromBits(Vecteur.A4Bits);
		Npc.bInside = Vecteur.A5 != 0;

		const FString Obtenu = AnastasisBudget::BandName(AnastasisBudget::ClassifyNpcBand(Directeur, Npc));
		TestEqual(
			*FString::Printf(TEXT("bande pression=%.9g vue=(%.9g,%.9g) pnj=(%.9g,%.9g) inside=%d"),
				Directeur.Pressure, Directeur.ViewX, Directeur.ViewY, Npc.X, Npc.Y, Vecteur.A5),
			Obtenu, FString(UTF8_TO_TCHAR(Vecteur.Attendu)));
	}

	return true;
}

/**
 * La cadence, qui porte de l'etat et ne se met donc pas en vecteurs scalaires.
 *
 * Ce qu'elle garantit et qui compte pour la simulation: un habitant hors vue
 * n'est pas oublie, il est simule par bouffees — et le retard qu'il rattrape
 * est plafonne, sinon un habitant longtemps invisible avalerait sa journee en
 * une enjambee.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisBudgetCadenceTest,
	"Anastasis.Sim.Parite.BudgetCadence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisBudgetCadenceTest::RunTest(const FString& Parameters)
{
	AnastasisBudget::FDirector Directeur;

	// 1. Proche: toujours simule, avec le dt complet et sans accumulateur.
	{
		AnastasisBudget::FNpcView Npc{ 1.0, 1.0, false };
		double Accumulateur = 0.0;
		const AnastasisBudget::FCadenceStep Pas =
			AnastasisBudget::ConsumeCadence(Directeur, Npc, 1.0 / 30.0, Accumulateur);
		TestTrue(TEXT("proche: simule"), Pas.bRun);
		TestEqual(TEXT("proche: dt intact"), Pas.Dt, 1.0 / 30.0);
		TestEqual(TEXT("proche: rien accumule"), Accumulateur, 0.0);
	}

	// 2. Lointain: saute jusqu'a l'intervalle, puis rend le temps accumule.
	{
		AnastasisBudget::FNpcView Npc{ 60.0, 0.0, false };
		double Accumulateur = 0.0;
		const double Dt = 1.0 / 30.0;
		const double Interval = AnastasisBudget::IntervalForBand(0.0, AnastasisBudget::EBand::Far);

		int32 Sauts = 0;
		AnastasisBudget::FCadenceStep Pas;
		for (int32 I = 0; I < 1000; ++I)
		{
			Pas = AnastasisBudget::ConsumeCadence(Directeur, Npc, Dt, Accumulateur);
			if (Pas.bRun) break;
			Sauts += 1;
		}
		TestEqual(TEXT("lointain: bande far"), AnastasisBudget::BandName(Pas.Band), TEXT("far"));
		TestTrue(TEXT("lointain: a saute des tours"), Sauts > 0);
		TestTrue(TEXT("lointain: finit par tourner"), Pas.bRun);
		// A l'epsilon pres, et l'epsilon n'est pas une commodite de test: le JS
		// compare `accum + 1e-6 < interval`, donc le pas part un cheveu AVANT
		// que l'intervalle soit plein. Sans cette tolerance, trente `dt` de
		// 1/30 accumules en double tombent a 0.9999999999999999 et le PNJ
		// attendrait un tour de plus — pour un bit.
		TestTrue(TEXT("lointain: rend l'intervalle a l'epsilon pres"), Pas.Dt >= Interval - 1e-6);
		TestEqual(TEXT("lointain: accumulateur remis a zero"), Accumulateur, 0.0);
		TestFalse(TEXT("lointain: pas symbolique"), Pas.bSymbolic);
	}

	// 3. Invisible: symbolique, et le rattrapage est plafonne a six secondes.
	{
		AnastasisBudget::FNpcView Npc{ 500.0, 0.0, false };
		double Accumulateur = 120.0; // deux minutes de retard
		const AnastasisBudget::FCadenceStep Pas =
			AnastasisBudget::ConsumeCadence(Directeur, Npc, 0.0, Accumulateur);
		TestTrue(TEXT("invisible: tourne"), Pas.bRun);
		TestTrue(TEXT("invisible: symbolique"), Pas.bSymbolic);
		TestEqual(TEXT("invisible: rattrapage plafonne"), Pas.Dt, 6.0);
	}

	// 4. Critique: passe outre la cadence, quelle que soit la distance.
	{
		AnastasisBudget::FNpcView Npc{ 500.0, 0.0, false };
		double Accumulateur = 0.0;
		const AnastasisBudget::FCadenceStep Pas =
			AnastasisBudget::ConsumeCadence(Directeur, Npc, 0.25, Accumulateur, /*bCritical=*/true);
		TestTrue(TEXT("critique: tourne"), Pas.bRun);
		TestEqual(TEXT("critique: dt intact"), Pas.Dt, 0.25);
		TestEqual(TEXT("critique: compte comme proche"), AnastasisBudget::BandName(Pas.Band), TEXT("near"));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
