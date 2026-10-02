#include "Misc/AutomationTest.h"

#include "Life/AnastasisNature.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisNatureParity
{
	namespace Vecteurs
	{
#include "AnastasisNatureVectors.inl"
	}

	double FromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	uint64 ToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	AnastasisNature::FNature Make(double Corps, double Esprit, double Coeur, const ANSICHAR* Q1, const ANSICHAR* Q2, const ANSICHAR* F)
	{
		AnastasisNature::FNature N;
		N.Corps = Corps;
		N.Esprit = Esprit;
		N.Coeur = Coeur;
		const FString A = UTF8_TO_TCHAR(Q1);
		const FString B = UTF8_TO_TCHAR(Q2);
		const FString C = UTF8_TO_TCHAR(F);
		if (!A.IsEmpty()) N.Qualities.Add(A);
		if (!B.IsEmpty()) N.Qualities.Add(B);
		if (!C.IsEmpty()) N.Flaws.Add(C);
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisNatureParityTest,
	"Anastasis.Sim.Parite.Nature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisNatureParityTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisNatureParity;
	using namespace AnastasisNatureParity::Vecteurs;
	int32 Failures = 0;

	for (int32 I = 0; I < UE_ARRAY_COUNT(NatureGoalBiasVectors); ++I)
	{
		const FNatureGoalBiasVector& V = NatureGoalBiasVectors[I];
		const AnastasisNature::FNature N = Make(FromBits(V.A0Bits), FromBits(V.A1Bits), FromBits(V.A2Bits), V.A3, V.A4, V.A5);
		const FString Goal = UTF8_TO_TCHAR(V.A6);
		const double Got = AnastasisNature::NatureGoalBias(N, Goal);
		if (ToBits(Got) != V.AttenduBits)
		{
			++Failures;
			if (Failures < 20) AddError(FString::Printf(TEXT("NatureGoalBias[%d] %s : %.17g attendu %.17g"), I, *Goal, Got, FromBits(V.AttenduBits)));
		}
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(NatureWorkFactorVectors); ++I)
	{
		const FNatureWorkFactorVector& V = NatureWorkFactorVectors[I];
		const double Got = AnastasisNature::NatureWorkFactor(Make(FromBits(V.A0Bits), 1.0, 1.0, V.A1, V.A2, V.A3));
		if (ToBits(Got) != V.AttenduBits)
		{
			++Failures;
			if (Failures < 20) AddError(FString::Printf(TEXT("NatureWorkFactor[%d] : %.17g attendu %.17g"), I, Got, FromBits(V.AttenduBits)));
		}
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(NatureStickBonusVectors); ++I)
	{
		const FNatureStickBonusVector& V = NatureStickBonusVectors[I];
		const double Got = AnastasisNature::NatureStickBonus(Make(1.0, 1.0, 1.0, V.A0, V.A1, V.A2));
		if (ToBits(Got) != V.AttenduBits)
		{
			++Failures;
			if (Failures < 20) AddError(FString::Printf(TEXT("NatureStickBonus[%d] : %.17g attendu %.17g"), I, Got, FromBits(V.AttenduBits)));
		}
	}
	const int32 Total = UE_ARRAY_COUNT(NatureGoalBiasVectors) + UE_ARRAY_COUNT(NatureWorkFactorVectors) + UE_ARRAY_COUNT(NatureStickBonusVectors);
	AddInfo(FString::Printf(TEXT("Nature : %d vecteurs, %d ecarts"), Total, Failures));
	return Failures == 0;
}

#endif
