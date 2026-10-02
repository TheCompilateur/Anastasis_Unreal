#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Life/AnastasisReconsider.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisRhythmParity
{
	namespace Vecteurs
	{
#include "AnastasisRhythmVectors.inl"
#include "AnastasisDomesticVectors.inl"
	}

	double RhythmFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	uint64 RhythmToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}
}

/**
 * Le rythme de Valmire (villageRhythm.js) et la qualite du repos (domestic.js),
 * compares BIT A BIT a la reference executee. Vecteurs :
 * tools/migration/parity/village-rhythm.mjs, tools/migration/parity/domestic.mjs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisParityRhythmTest,
	"Anastasis.Sim.Parite.Rythme",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisParityRhythmTest::RunTest(const FString&)
{
	using AnastasisRhythmParity::RhythmFromBits;
	using AnastasisRhythmParity::RhythmToBits;
	using namespace AnastasisRhythmParity::Vecteurs;

	int32 Failures = 0;

	for (int32 I = 0; I < UE_ARRAY_COUNT(VillagePhaseVectors); ++I)
	{
		const FVillagePhaseVector& V = VillagePhaseVectors[I];
		const FString Got = AnastasisRhythm::PhaseId(AnastasisRhythm::VillagePhase(RhythmFromBits(V.A0Bits)));
		const FString Expected = UTF8_TO_TCHAR(V.Attendu);
		if (Got != Expected)
		{
			++Failures;
			AddError(FString::Printf(TEXT("VillagePhase[%d] frac=%.17g : %s attendu %s"), I, RhythmFromBits(V.A0Bits), *Got, *Expected));
		}
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(PhaseBiasVectors); ++I)
	{
		const FPhaseBiasVector& V = PhaseBiasVectors[I];
		const FString Goal = UTF8_TO_TCHAR(V.A1);
		AnastasisRhythm::FPhaseSubject Subject;
		Subject.bHasHomeOrShelter = V.A2 != 0;
		Subject.Energy = RhythmFromBits(V.A3Bits);
		Subject.Hunger = RhythmFromBits(V.A4Bits);
		const double Got = AnastasisRhythm::PhaseBias(AnastasisRhythm::VillagePhase(RhythmFromBits(V.A0Bits)), Subject, Goal);
		if (RhythmToBits(Got) != V.AttenduBits)
		{
			++Failures;
			AddError(FString::Printf(TEXT("PhaseBias[%d] %s frac=%.17g : %.17g attendu %.17g"),
				I, *Goal, RhythmFromBits(V.A0Bits), Got, RhythmFromBits(V.AttenduBits)));
		}
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(SleepQualityVectors); ++I)
	{
		const FSleepQualityVector& V = SleepQualityVectors[I];
		const double Got = AnastasisNeeds::SleepQuality(UTF8_TO_TCHAR(V.A0), UTF8_TO_TCHAR(V.A1), UTF8_TO_TCHAR(V.A2));
		if (RhythmToBits(Got) != V.AttenduBits)
		{
			++Failures;
			AddError(FString::Printf(TEXT("SleepQuality[%d] : %.17g attendu %.17g"), I, Got, RhythmFromBits(V.AttenduBits)));
		}
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(PhaseWorkFactorVectors); ++I)
	{
		const FPhaseWorkFactorVector& V = PhaseWorkFactorVectors[I];
		const FString Job = UTF8_TO_TCHAR(V.A1);
		const FString Id = UTF8_TO_TCHAR(V.A2);
		TOptional<AnastasisLifestyle::FLifestyle> Lifestyle;
		if (!Id.IsEmpty())
		{
			AnastasisLifestyle::FLifestyle L;
			L.Id = Id;
			Lifestyle = L;
		}
		const AnastasisRhythm::EPhase Personal = AnastasisReconsider::PersonalPhase(RhythmFromBits(V.A0Bits), Lifestyle);
		const double Got = AnastasisRhythm::PhaseWorkFactor(Personal, Job, Id);
		if (RhythmToBits(Got) != V.AttenduBits)
		{
			++Failures;
			AddError(FString::Printf(TEXT("PhaseWorkFactor[%d] %s %s frac=%.17g : %.17g attendu %.17g"),
				I, *Job, *Id, RhythmFromBits(V.A0Bits), Got, RhythmFromBits(V.AttenduBits)));
		}
	}

	const int32 Total = UE_ARRAY_COUNT(VillagePhaseVectors) + UE_ARRAY_COUNT(PhaseBiasVectors) + UE_ARRAY_COUNT(SleepQualityVectors)
		+ UE_ARRAY_COUNT(PhaseWorkFactorVectors);
	AddInfo(FString::Printf(TEXT("Rythme + foyer : %d vecteurs, %d ecarts"), Total, Failures));
	return Failures == 0;
}

#endif
