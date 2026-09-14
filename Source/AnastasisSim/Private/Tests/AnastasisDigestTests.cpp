#include "Misc/AutomationTest.h"

#include "Core/AnastasisStateDigest.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Parite de l'empreinte d'etat entre le C++ et la specification JS.
 *
 * Le harnais differentiel (tools/migration/) compare une trace JS et une trace
 * Unreal tick par tick, et rend le premier tick divergent. Ce nombre ne vaut
 * que si les deux cotes hachent identiquement: sinon, devant une divergence, il
 * faudrait d'abord se demander si la simulation a devie ou seulement le
 * hacheur. Ces tests retirent la question une fois pour toutes.
 *
 * Les vecteurs sont generes par tools/migration/gen-digest-vectors.mjs, qui
 * declare chaque cas UNE fois et en tire a la fois la valeur JS hachee et la
 * fonction C++ qui la rejoue. Ne jamais corriger une valeur attendue a la main:
 * soit le C++ a devie, soit la specification a change et il faut regenerer.
 */
namespace AnastasisDigestParity
{
	/** Relit un motif binaire en double, sans passer par un litteral decimal. */
	static double FromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(double));
		return Value;
	}

	#include "AnastasisDigestVectors.inl"
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisDigestParityTest,
	"Anastasis.Sim.Empreinte.Parite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisDigestParityTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisDigestParity;

	for (const FDigestVector& Vector : DigestVectors)
	{
		AnastasisDigest::FStateWriter Writer;
		Vector.Build(Writer);
		const uint64 Actual = Writer.Digest();
		if (Actual != Vector.Expected)
		{
			AddError(FString::Printf(
				TEXT("empreinte %s: attendu %016llx, obtenu %016llx"),
				Vector.Name, Vector.Expected, Actual));
		}
	}

	return true;
}

/**
 * Les invariants que le format promet, verifies plutot que supposes.
 *
 * Un vecteur qui passe prouve qu'une valeur donnee rend les bons bits. Il ne
 * prouve pas que le format tient ses promesses — que l'ordre d'ecriture des
 * cles ne compte pas, et qu'un ulp compte. Ce sont les deux proprietes sur
 * lesquelles repose tout le harnais.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisDigestInvariantsTest,
	"Anastasis.Sim.Empreinte.Invariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisDigestInvariantsTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisDigestParity;

	// 1. L'ordre d'ecriture des cles ne change pas l'empreinte. Sans cela, deux
	//    portages corrects du meme etat divergeraient parce qu'ils n'ecrivent
	//    pas leurs champs dans le meme ordre.
	{
		AnastasisDigest::FStateWriter Desordre;
		Desordre.BeginObject();
		Desordre.Key(TEXT("b")).Number(2.0);
		Desordre.Key(TEXT("a")).Number(1.0);
		Desordre.EndObject();

		AnastasisDigest::FStateWriter Ordre;
		Ordre.BeginObject();
		Ordre.Key(TEXT("a")).Number(1.0);
		Ordre.Key(TEXT("b")).Number(2.0);
		Ordre.EndObject();

		TestEqual(TEXT("l'ordre d'ecriture des cles ne compte pas"), Desordre.Digest(), Ordre.Digest());
		TestEqual(TEXT("et c'est bien l'empreinte que le JS annonce"), Ordre.Digest(), DigestTriOrdre);
	}

	// 2. Un ulp change l'empreinte. C'est la raison d'etre du format: sur une
	//    comparaison `dist < radius`, un ulp fait basculer une decision de PNJ.
	{
		AnastasisDigest::FStateWriter A;
		A.Number(FromBits(0x4029000000000000ull)); // 12.5
		AnastasisDigest::FStateWriter B;
		B.Number(FromBits(0x4029000000000001ull)); // 12.5 + 1 ulp

		TestNotEqual(TEXT("un ulp change l'empreinte"), A.Digest(), B.Digest());
		TestEqual(TEXT("12.5 rend l'empreinte annoncee par le JS"), A.Digest(), DigestUlpA);
		TestEqual(TEXT("12.5 + 1 ulp aussi"), B.Digest(), DigestUlpB);
	}

	// 3. Un tampon vide rend le basis de FNV-1a, pas zero. Verification bete,
	//    mais c'est l'erreur qui rendrait toutes les empreintes egales.
	{
		AnastasisDigest::FStateWriter Vide;
		TestEqual(TEXT("empreinte du vide = offset basis FNV-1a 64"),
			Vide.Digest(), AnastasisDigest::FFnv1a64::OffsetBasis);
	}

	// 4. La version de specification du .inl correspond a celle que le C++ sait
	//    produire. Deux formats differents donneraient des empreintes
	//    differentes sans qu'aucun des deux cotes soit fautif.
	TestEqual(TEXT("version de specification"), DigestSpecVersion, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
