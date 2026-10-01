#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Core/AnastasisJson.h"
#include "Core/AnastasisStateDigest.h"
#include "Harness/AnastasisJsSave.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Le lecteur de sauvegarde JS (mission sim-state-reader-001).
 *
 * Harnais.Json : l'arbre JSON relit chaque nombre au bit pres, et son
 * empreinte est celle de `digestValue`.
 *
 * Harnais.Lecture : le scenario `endurance` (tools/migration/scenarios/) est
 * lu en etat C++, puis reprojete sur les sections de `serialize`. Les
 * empreintes attendues sont celles du tick 0 de la REFERENCE, generees par
 * tools/migration/gen-scenario-vectors.mjs — pas recalculees depuis le texte
 * que le C++ vient de lire.
 */
namespace AnastasisJsSaveTest
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

	/** Le double immediatement superieur. */
	static double NextUp(double Value)
	{
		return FromBits(ToBits(Value) + 1);
	}

	#include "AnastasisScenarioVectors.inl"

	static const FScenarioSection* FindSection(const FString& Name)
	{
		for (const FScenarioSection& S : ScenarioSections)
		{
			if (Name.Equals(S.Name, ESearchCase::CaseSensitive)) return &S;
		}
		return nullptr;
	}

	static uint64 ProjectedDigest(FAutomationTestBase& Test, const AnastasisJsSave::FState& State, const FString& Section)
	{
		AnastasisJson::FValue Value;
		FString Error;
		if (!AnastasisJsSave::Project(State, Section, Value, Error))
		{
			Test.AddError(Error);
			return 0;
		}
		return AnastasisJson::DigestOf(Value);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisHarnessJsonTest,
	"Anastasis.Sim.Harnais.Json",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisHarnessJsonTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisJsSaveTest;
	using AnastasisJson::FValue;

	// 1. Les nombres, au bit pres. Ecritures de `JSON.stringify` et leurs motifs.
	struct FCase { const TCHAR* Text; uint64 Bits; };
	const FCase Cases[] = {
		{ TEXT("0.1"), 0x3FB999999999999Aull },
		{ TEXT("0.30000000000000004"), 0x3FD3333333333334ull },
		{ TEXT("5e-324"), 0x0000000000000001ull },
		{ TEXT("2.2250738585072014e-308"), 0x0010000000000000ull },
		{ TEXT("1.7976931348623157e+308"), 0x7FEFFFFFFFFFFFFFull },
		{ TEXT("-0"), 0x8000000000000000ull },
		{ TEXT("9007199254740993"), 0x4340000000000000ull },
		{ TEXT("2576143622"), 0x41E3319AA0C00000ull },
	};
	for (const FCase& C : Cases)
	{
		FValue V;
		FString Error;
		if (!AnastasisJson::Parse(C.Text, V, Error) || !V.IsNumber())
		{
			AddError(FString::Printf(TEXT("%s : non lu (%s)"), C.Text, *Error));
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s relu au bit pres"), C.Text), ToBits(V.Number), C.Bits);
	}

	// 2. Chaines: echappements, paire de substitution gardee en deux unites UTF-16.
	{
		FValue V;
		FString Error;
		TestTrue(TEXT("chaine echappee lue"), AnastasisJson::Parse(TEXT("\"a\\\"b\\\\c\\n\\u00e9\\ud83d\\ude00\""), V, Error));
		TestEqual(TEXT("contenu"), V.String, FString(TEXT("a\"b\\c\né")) + FString::Chr(0xD83D) + FString::Chr(0xDE00));
	}

	// 3. Ce qui n'est pas du JSON strict est refuse.
	for (const TCHAR* Bad : { TEXT("[1,]"), TEXT("{\"a\":1,}"), TEXT("01"), TEXT("1."), TEXT("[1] x"), TEXT("{a:1}"), TEXT("\"sans fin") })
	{
		FValue V;
		FString Error;
		TestFalse(FString::Printf(TEXT("refuse %s"), Bad), AnastasisJson::Parse(Bad, V, Error));
	}

	// 4. L'empreinte d'un arbre est celle que l'ecrivain d'etat rend pour la
	//    meme valeur decrite a la main — cles dans un autre ordre comprises.
	{
		FValue V;
		FString Error;
		TestTrue(TEXT("objet lu"), AnastasisJson::Parse(TEXT("{\"b\":[1,null,true],\"a\":{\"y\":\"z\"}}"), V, Error));
		AnastasisDigest::FStateWriter W;
		W.BeginObject();
		W.Key(TEXT("a")).BeginObject().Key(TEXT("y")).String(TEXT("z")).EndObject();
		W.Key(TEXT("b")).BeginArray(3).Number(1.0).Null().Bool(true).EndArray();
		W.EndObject();
		TestEqual(TEXT("empreinte de l'arbre = empreinte decrite a la main"), AnastasisJson::DigestOf(V), W.Digest());
	}

	// 5. Cle repetee: comme `JSON.parse`, la derniere valeur, a la place de la premiere.
	{
		FValue V;
		FString Error;
		TestTrue(TEXT("cle repetee lue"), AnastasisJson::Parse(TEXT("{\"a\":1,\"b\":2,\"a\":3}"), V, Error));
		TestEqual(TEXT("deux cles"), V.Keys.Num(), 2);
		TestEqual(TEXT("a vaut la derniere"), V.Find(TEXT("a"))->Number, 3.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisHarnessReadTest,
	"Anastasis.Sim.Harnais.Lecture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisHarnessReadTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisJsSaveTest;
	using AnastasisJson::FValue;

	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), ScenarioPath));
	FString Text;
	if (!TestTrue(FString::Printf(TEXT("scenario lisible : %s"), *Path), FFileHelper::LoadFileToString(Text, *Path)))
	{
		return false;
	}
	FValue Root;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("JSON du scenario lu (%s)"), *Error), AnastasisJson::Parse(Text, Root, Error)))
	{
		return false;
	}
	const FValue* Empreinte = Root.Find(TEXT("empreinte"));
	if (!TestTrue(TEXT("le scenario est celui des vecteurs (sinon : gen-scenario-vectors.mjs)"),
		Empreinte && Empreinte->IsString() && Empreinte->String == ScenarioEmpreinte))
	{
		return false;
	}
	const FValue* Save = Root.Find(TEXT("save"));
	if (!TestTrue(TEXT("sauvegarde presente"), Save && Save->IsObject()))
	{
		return false;
	}

	// 1. Le texte relu rend, section par section, l'empreinte que la reference
	//    calcule au tick 0. Preuve du lecteur JSON et de l'empreinte d'arbre,
	//    avant toute lecture typee.
	int32 TextSections = 0;
	for (const FScenarioSection& S : ScenarioSections)
	{
		const FValue* Section = Save->Find(S.Name);
		if (!Section)
		{
			AddError(FString::Printf(TEXT("section %s absente du fichier"), S.Name));
			continue;
		}
		const uint64 Got = AnastasisJson::DigestOf(*Section);
		if (Got != S.Digest)
		{
			AddError(FString::Printf(TEXT("texte relu, section %s : attendu %016llx, obtenu %016llx"), S.Name, S.Digest, Got));
		}
		++TextSections;
	}
	TestEqual(TEXT("toutes les sections de serialize presentes"), TextSections, static_cast<int32>(UE_ARRAY_COUNT(ScenarioSections)));

	// 2. Lecture typee.
	AnastasisJsSave::FState State;
	if (!TestTrue(FString::Printf(TEXT("lecture typee (%s)"), *Error), AnastasisJsSave::Read(*Save, State, Error)))
	{
		return false;
	}
	TestEqual(TEXT("carte"), State.W * 10000 + State.H, ScenarioW * 10000 + ScenarioH);
	TestEqual(TEXT("batiments lus"), State.Buildings.Num(), ScenarioBuildings);
	TestEqual(TEXT("habitants lus"), State.Actors.Num(), ScenarioActors);

	// 3. Projection de chaque section du perimetre depuis l'etat C++: meme
	//    empreinte qu'au tick 0 de la reference.
	for (const FString& Section : AnastasisJsSave::PortedSections())
	{
		const FScenarioSection* Expected = FindSection(Section);
		if (!Expected)
		{
			AddError(FString::Printf(TEXT("section %s absente des vecteurs"), *Section));
			continue;
		}
		if (!Expected->bPerimetre)
		{
			AddError(FString::Printf(TEXT("section %s portee mais hors du perimetre du scenario"), *Section));
		}
		const uint64 Got = ProjectedDigest(*this, State, Section);
		if (Got != Expected->Digest)
		{
			AddError(FString::Printf(TEXT("projection C++, section %s : attendu %016llx, obtenu %016llx"), *Section, Expected->Digest, Got));
		}
	}
	{
		FValue Diff;
		AnastasisJsSave::Project(State, TEXT("tileDiff"), Diff, Error);
		TestEqual(TEXT("tileDiff recalcule : autant de cases que la reference"), Diff.Items.Num(), ScenarioTileDiffRows);
	}

	// 4. L'etat entier au tick 0: les 10 sections du perimetre projetees, les
	//    autres RECOPIEES de la sauvegarde (non lues) — empreinte globale de la reference.
	{
		AnastasisDigest::FStateWriter Global;
		for (const FScenarioSection& S : ScenarioSections)
		{
			const bool bPorted = AnastasisJsSave::PortedSections().Contains(FString(S.Name));
			const uint64 H = bPorted ? ProjectedDigest(*this, State, S.Name) : AnastasisJson::DigestOf(*Save->Find(S.Name));
			Global.String(S.Name).String(AnastasisDigest::ToHex(H));
		}
		TestEqual(TEXT("empreinte globale du tick 0 (perimetre projete, reste recopie)"), Global.Digest(), ScenarioGlobalDigest);
	}

	// 5. La projection lit l'etat C++, pas le fichier: un ulp, une portion, une
	//    tuile changes cote C++ changent l'empreinte de leur section — et d'elle seule.
	{
		AnastasisJsSave::FState Mutated = State;
		Mutated.Actors[0].Needs.Hunger = NextUp(Mutated.Actors[0].Needs.Hunger);
		TestNotEqual(TEXT("un ulp sur la faim de actors[0] change `actors`"), ProjectedDigest(*this, Mutated, TEXT("actors")), FindSection(TEXT("actors"))->Digest);
		TestEqual(TEXT("... et pas `buildings`"), ProjectedDigest(*this, Mutated, TEXT("buildings")), FindSection(TEXT("buildings"))->Digest);
	}
	{
		AnastasisJsSave::FState Mutated = State;
		Mutated.Actors[1].X = NextUp(Mutated.Actors[1].X);
		TestNotEqual(TEXT("un ulp sur actors[1].x change `actors`"), ProjectedDigest(*this, Mutated, TEXT("actors")), FindSection(TEXT("actors"))->Digest);
	}
	{
		AnastasisJsSave::FState Mutated = State;
		Mutated.Buildings[0].FoodPhysical += 1;
		TestNotEqual(TEXT("une portion de plus au grenier change `buildings`"), ProjectedDigest(*this, Mutated, TEXT("buildings")), FindSection(TEXT("buildings"))->Digest);
	}
	{
		// Une tuile hors du diff qui change entre dans le diff.
		AnastasisJsSave::FState Mutated = State;
		FValue Diff;
		AnastasisJsSave::Project(State, TEXT("tileDiff"), Diff, Error);
		TSet<int32> InDiff;
		for (const FValue& Row : Diff.Items) InDiff.Add(static_cast<int32>(Row.Items[0].Number));
		int32 Free = 0;
		while (InDiff.Contains(Free)) ++Free;
		Mutated.World.Tiles[Free].Amount += 1;
		FValue MutatedDiff;
		AnastasisJsSave::Project(Mutated, TEXT("tileDiff"), MutatedDiff, Error);
		TestEqual(TEXT("une tuile changee hors diff y entre"), MutatedDiff.Items.Num(), ScenarioTileDiffRows + 1);
		TestNotEqual(TEXT("... et change `tileDiff`"), AnastasisJson::DigestOf(MutatedDiff), FindSection(TEXT("tileDiff"))->Digest);
	}

	// 6. Ce qui n'est pas representable est refuse, pas arrondi.
	{
		FValue Bad = *Save;
		FValue* Actors = Bad.Find(TEXT("actors"));
		Actors->Items[0].Set(TEXT("inside"), FValue::MakeObject());
		AnastasisJsSave::FState Ignored;
		FString BadError;
		TestFalse(TEXT("habitant a l'interieur : refuse"), AnastasisJsSave::Read(Bad, Ignored, BadError));
		AddInfo(FString::Printf(TEXT("refus attendu : %s"), *BadError));
	}
	{
		FValue Bad = *Save;
		FValue* Buildings = Bad.Find(TEXT("buildings"));
		Buildings->Items[0].Set(TEXT("createdDay"), FValue::MakeNumber(1.5));
		AnastasisJsSave::FState Ignored;
		FString BadError;
		TestFalse(TEXT("jour de creation non entier : refuse"), AnastasisJsSave::Read(Bad, Ignored, BadError));
		AddInfo(FString::Printf(TEXT("refus attendu : %s"), *BadError));
	}

	AddInfo(FString::Printf(TEXT("scenario %s : %d x %d, %d cases de tileDiff, %d batiments, %d habitants ; %d sections projetees"),
		ScenarioName, State.W, State.H, ScenarioTileDiffRows, State.Buildings.Num(), State.Actors.Num(), AnastasisJsSave::PortedSections().Num()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
