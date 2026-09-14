#include "Misc/AutomationTest.h"

#include "Core/AnastasisStateDigest.h"
#include "Core/AnastasisStateTrace.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * La moitie Unreal du harnais differentiel.
 *
 * Le C++ n'a pas encore d'etat de simulation a projeter — la genese n'est pas
 * portee. Mais l'EMETTEUR, lui, se prouve des maintenant sur un etat
 * synthetique: si la trace qu'il ecrit est declaree identique a celle du JS
 * pour le meme etat, alors brancher une vraie simulation dessus ne sera plus
 * qu'un cablage, sans second format a inventer.
 *
 * Vecteurs et trace JS de reference: tools/migration/gen-trace-vectors.mjs.
 */
namespace AnastasisTraceParity
{
	static double FromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(double));
		return Value;
	}

	#include "AnastasisTraceVectors.inl"
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisTraceParityTest,
	"Anastasis.Sim.Parite.Trace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisTraceParityTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisTraceParity;

	AnastasisTrace::FHeader Header;
	Header.Spec = TraceSpecVersion;
	Header.Source = TEXT("unreal");
	Header.Ref = TEXT("(synthetique)");
	Header.RefHead = TEXT("synthetique");
	Header.Seed = TraceSeed;
	Header.Dt = FromBits(TraceDtBits);
	Header.DayLength = FromBits(TraceDayLengthBits);
	Header.Every = 1;
	Header.Ticks = TraceSamples[TraceSampleCount - 1].T;

	AnastasisTrace::FTraceWriter Trace(Header);

	for (const FTraceSampleVector& Sample : TraceSamples)
	{
		Trace.BeginSample(Sample.T, Sample.Day, FromBits(Sample.TimeBits));

		// Volontairement dans l'ordre INVERSE des noms: l'ecrivain doit trier,
		// et une trace ecrite dans le desordre doit rendre la meme empreinte
		// qu'une trace ecrite dans l'ordre. C'est ce qui permet a deux portages
		// de decrire leurs champs comme ils veulent.
		for (int32 Index = TraceSectionCount - 1; Index >= 0; --Index)
		{
			const FString Nom = UTF8_TO_TCHAR(TraceSectionNames[Index]);
			AnastasisDigest::FStateWriter Probe;
			Sample.Build[Index](Probe);
			const FString Attendu = UTF8_TO_TCHAR(Sample.SectionHex[Index]);
			TestEqual(
				*FString::Printf(TEXT("tick %d, section %s"), Sample.T, *Nom),
				Probe.Hex(), Attendu);

			Trace.Section(Nom, [&Sample, Index](AnastasisDigest::FStateWriter& W)
			{
				Sample.Build[Index](W);
			});
		}

		Trace.EndSample();
	}

	TestEqual(TEXT("nombre d'echantillons"), Trace.NumSamples(), TraceSampleCount);

	// L'empreinte globale de chaque echantillon, relue depuis le JSONL produit.
	const FString Jsonl = Trace.ToJsonl();
	for (const FTraceSampleVector& Sample : TraceSamples)
	{
		const FString Attendu = FString::Printf(
			TEXT("\"t\":%d,\"day\":%d,"), Sample.T, Sample.Day);
		if (!TestTrue(*FString::Printf(TEXT("le tick %d est dans la trace"), Sample.T),
			Jsonl.Contains(Attendu)))
		{
			continue;
		}
		const FString GlobalAttendu = FString::Printf(
			TEXT("\"g\":\"%s\""), UTF8_TO_TCHAR(Sample.GlobalHex));
		TestTrue(
			*FString::Printf(TEXT("tick %d: empreinte globale %s"), Sample.T, UTF8_TO_TCHAR(Sample.GlobalHex)),
			Jsonl.Contains(GlobalAttendu));
	}

	// `dt` doit se reparser au bit pres cote JS, sinon le comparateur refuse de
	// comparer — et il refuserait en accusant des conditions differentes, pas un
	// defaut de format.
	{
		const FString Rendu = AnastasisTrace::JsonNumber(FromBits(TraceDtBits));
		const double Relu = FCString::Atod(*Rendu);
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Relu, sizeof(double));
		TestEqual(TEXT("dt se reparse au bit pres"), Bits, TraceDtBits);
	}
	{
		const FString Rendu = AnastasisTrace::JsonNumber(FromBits(TraceDayLengthBits));
		TestEqual(TEXT("dayLength s'ecrit en entier"), Rendu, FString(TEXT("90")));
	}

	// La trace part sur disque a cote de la trace JS: `compare-digests.mjs`
	// tranche ensuite, et c'est l'outil du harnais qui tranche, pas ce test.
	const FString Chemin = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("CanonicalVerification"), TEXT("trace-cpp.jsonl"));
	TestTrue(TEXT("la trace s'ecrit sur disque"), Trace.SaveToFile(Chemin));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
