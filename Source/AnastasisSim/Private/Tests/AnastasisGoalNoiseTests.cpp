#include "Misc/AutomationTest.h"

#include "Ai/AnastasisGoalNoise.h"
#include "Core/AnastasisRng.h"
#include "Sim/AnastasisSimulation.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Parite du bruit de decision (`goalNoise`, src/sim/npc.js) — mission sim-rng-001.
 *
 * Vecteurs generes par tools/migration/gen-goal-noise-vectors.mjs : la fonction
 * de la reference executee telle qu'elle est ecrite, les amplitudes lues dans sa
 * source, et les decisions MESUREES du scenario endurance. Pour ces dernieres, le
 * C++ rejoue les bruits depuis l'etat mesure avec SA table, dans SON ordre : une
 * ligne de trop, de moins ou deplacee, et la suite des valeurs ne colle plus.
 * Ne jamais corriger un vecteur a la main.
 */
namespace AnastasisGoalNoiseParity
{
	namespace Vecteurs
	{
#include "AnastasisGoalNoiseVectors.inl"
	}

	static double NoiseFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	static uint64 NoiseToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisGoalNoiseParityTest,
	"Anastasis.Sim.Parite.BruitDeBut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisGoalNoiseParityTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisGoalNoiseParity;
	namespace V = AnastasisGoalNoiseParity::Vecteurs;
	namespace N = AnastasisGoalNoise;

	int32 Compared = 0;
	int32 Errors = 0;
	auto Fail = [this, &Errors](const FString& Message)
	{
		if (++Errors <= 30)
		{
			AddError(Message);
		}
	};

	// 1. La fonction pure : valeur au bit pres, et un tirage consomme.
	for (int32 I = 0; I < UE_ARRAY_COUNT(V::GoalNoisePureVectors); ++I)
	{
		const V::FGoalNoisePureVector& Vec = V::GoalNoisePureVectors[I];
		FAnastasisRng Rng(Vec.State);
		const double Got = N::GoalNoise(Rng, P::NoiseFromBits(Vec.AmpBits));
		Compared += 2;
		if (P::NoiseToBits(Got) != Vec.ValueBits)
		{
			Fail(FString::Printf(TEXT("pure[%d] : attendu %016llx, obtenu %016llx"), I, Vec.ValueBits, P::NoiseToBits(Got)));
		}
		if (Rng.GetState() != Vec.StateAfter)
		{
			Fail(FString::Printf(TEXT("pure[%d] : etat apres %u, attendu %u"), I, Rng.GetState(), Vec.StateAfter));
		}
	}

	// 2. La table C++ contre la source : chaque entree pointe une ligne de npc.js
	//    qui appelle goalNoise avec CETTE amplitude.
	const TConstArrayView<N::FTableNoise> Table = N::AdultTableNoises();
	for (const N::FTableNoise& Row : Table)
	{
		Compared += 1;
		const V::FGoalNoiseSourceVector* Source = nullptr;
		for (const V::FGoalNoiseSourceVector& S : V::GoalNoiseSourceVectors)
		{
			if (S.Line == Row.SourceLine)
			{
				Source = &S;
				break;
			}
		}
		if (!Source)
		{
			Fail(FString::Printf(TEXT("table %s : la ligne %d de npc.js n'appelle pas goalNoise"), Row.Goal, Row.SourceLine));
		}
		else if (P::NoiseToBits(Row.Amp) != Source->AmpBits)
		{
			Fail(FString::Printf(TEXT("table %s : amplitude %g, la source (l. %d) dit %g"),
				Row.Goal, Row.Amp, Row.SourceLine, P::NoiseFromBits(Source->AmpBits)));
		}
	}

	// 3. Les decisions mesurees : rejouees depuis l'etat du premier bruit, avec la table
	//    C++ dans son ordre. Une ligne conditionnelle est prise si la mesure l'a tiree ;
	//    une ligne inconditionnelle DOIT etre la prochaine tiree.
	for (int32 D = 0; D < UE_ARRAY_COUNT(V::GoalNoiseDecisions); ++D)
	{
		const V::FGoalNoiseDecisionVector& Dec = V::GoalNoiseDecisions[D];
		const FString Where = FString::Printf(TEXT("decision tick %d %s"), Dec.Tick, UTF8_TO_TCHAR(Dec.Npc));
		if (Dec.Contiguous == 0)
		{
			Fail(Where + TEXT(" : bruits non contigus dans la mesure, rejeu impossible"));
			continue;
		}
		FAnastasisRng Rng(Dec.State);
		int32 Next = 0;
		for (const N::FTableNoise& Row : Table)
		{
			const bool bMeasured = Next < Dec.Count && V::GoalNoiseDraws[Dec.First + Next].Line == Row.SourceLine;
			if (Row.Condition != N::ENoiseCondition::Always && !bMeasured)
			{
				continue;
			}
			Compared += 1;
			if (!bMeasured)
			{
				Fail(FString::Printf(TEXT("%s : la table tire %s (l. %d) en position %d, la reference tirait la ligne %d"),
					*Where, Row.Goal, Row.SourceLine, Next,
					Next < Dec.Count ? V::GoalNoiseDraws[Dec.First + Next].Line : -1));
				break;
			}
			const V::FGoalNoiseDrawVector& Draw = V::GoalNoiseDraws[Dec.First + Next];
			const double Got = N::GoalNoise(Rng, Row.Amp);
			if (P::NoiseToBits(Got) != Draw.ValueBits)
			{
				Fail(FString::Printf(TEXT("%s : bruit %s attendu %016llx, obtenu %016llx"),
					*Where, Row.Goal, Draw.ValueBits, P::NoiseToBits(Got)));
			}
			++Next;
		}
		Compared += 1;
		if (Next != Dec.Count)
		{
			Fail(FString::Printf(TEXT("%s : %d bruits rejoues, la reference en a tire %d"), *Where, Next, Dec.Count));
		}
	}

	if (Errors > 30)
	{
		AddError(FString::Printf(TEXT("... %d ecarts en tout"), Errors));
	}
	AddInfo(FString::Printf(TEXT("%d valeurs comparees, %d decisions mesurees rejouees, %d ecarts"),
		Compared, static_cast<int32>(UE_ARRAY_COUNT(V::GoalNoiseDecisions)), Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSimRngStateTest,
	"Anastasis.Sim.Village.FluxSimRng",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSimRngStateTest::RunTest(const FString& Parameters)
{
	// `this.rng = makeRng(this.seed)` : apres Reset, l'etat du flux EST la graine.
	FAnastasisSimulation Sim;
	Sim.Reset(12345u, 32, 32);
	AnastasisVillage::FVillage& Village = Sim.GetVillage();
	TestEqual(TEXT("etat initial = graine (makeRng)"), Village.GetSimRngState(), 12345u);

	// `save.rng` relu : l'etat pose est l'etat rendu, et la suite repart de lui.
	Village.SetSimRngState(2576143622u);
	TestEqual(TEXT("etat pose = etat rendu"), Village.GetSimRngState(), 2576143622u);

	// L'etat mesure au tick 32 du scenario endurance (P3_RNG_RELEVE.md) : un tirage
	// mulberry32 depuis cet etat rend l'etat suivant de la mesure (112742139).
	FAnastasisRng Probe(Village.GetSimRngState());
	Probe.Next();
	TestEqual(TEXT("un tirage depuis l'etat mesure"), Probe.GetState(), 112742139u);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
