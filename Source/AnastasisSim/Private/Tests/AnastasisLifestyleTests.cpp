#include "Misc/AutomationTest.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisRng.h"
#include "Life/AnastasisLifestyle.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Parite du mode de vie contre `src/sim/lifestyle.js` (mission lifestyle-001).
 *
 * Vecteurs generes par tools/migration/gen-parity.mjs (declaration
 * parity/lifestyle.mjs). Les suites (`PlaceUse`, `Daily`) sont tirees d'un
 * mulberry32 a graine des deux cotes : ce test les refait avec FAnastasisRng,
 * avec les memes listes de buts et d'usages que la declaration.
 * Ne jamais corriger un vecteur a la main.
 */
namespace AnastasisLifestyleParity
{
	namespace Vecteurs
	{
#include "AnastasisLifestyleVectors.inl"
	}

	static double LifeFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	static uint64 LifeToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	static FString LifeText(const ANSICHAR* Utf8)
	{
		return FString(UTF8_TO_TCHAR(Utf8));
	}

	/** `GOALS` de la declaration, dans son ordre : les suites y piochent par indice. */
	static const TCHAR* const Goals[] = {
		TEXT("rest"), TEXT("visitFamily"), TEXT("socialize"), TEXT("relax"), TEXT("play"), TEXT("explore"),
		TEXT("maintain"), TEXT("study"), TEXT("gatherWood"), TEXT("build"), TEXT("craft"), TEXT("deliver"),
		TEXT("sell"), TEXT("buy"), TEXT("eat"), TEXT("observer"),
	};

	/** `KINDS` de la declaration. */
	static const TCHAR* const Kinds[] = {
		TEXT("gatherWood"), TEXT("craft"), TEXT("socialize"), TEXT("socialise"), TEXT("relax"),
		TEXT("relaxe"), TEXT("visitFamily"), TEXT("rest"), TEXT("drink"),
	};

	/** `PROFILS` de la declaration. */
	static AnastasisLifestyle::FLifestyleSubject Profile(int32 Code, const FString& Goal)
	{
		AnastasisLifestyle::FLifestyleSubject S;
		S.Goal = Goal;
		switch (Code)
		{
		case 0: S.Skill = 0.0; S.Energy = 0.0; break;
		case 1: S.HomeId = TEXT("h1"); S.FamilyId = TEXT("f1"); S.PartnerId = TEXT("p1"); S.Skill = 1.5; S.Energy = 30.0; S.bHasTarget = true; break;
		case 2: S.HomeId = TEXT("h1"); S.FamilyId = TEXT("f1"); S.ChildCount = 2; S.Skill = 5.0; S.Energy = 80.0; break;
		case 3: S.HomeId = TEXT("h1"); S.Skill = 3.99; S.Energy = 20.0; S.bHasTarget = true; break;
		default: break;
		}
		return S;
	}

	static TOptional<AnastasisLifestyle::FLifestyle> WithMode(const FString& Id)
	{
		AnastasisLifestyle::FLifestyle L;
		L.Id = Id;
		return L;
	}

	/** `MONDES` de la declaration, pour `lifestyleTarget`. */
	class FTestWorld final : public AnastasisLifestyle::ILifestyleWorld
	{
	public:
		explicit FTestWorld(int32 InCode) : Code(InCode) {}

		virtual FString FirstCompletedTavernId() const override
		{
			// La premiere taverne ACHEVEE dans l'ordre de la liste.
			switch (Code)
			{
			case 2: return TEXT("b3");
			case 3: return TEXT("b5");
			default: return FString();
			}
		}

		virtual bool HasBuilding(const FString& Id) const override
		{
			return (Code == 2 || Code == 3) && Id == TEXT("b9");
		}

	private:
		int32 Code;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisLifestyleParityTest,
	"Anastasis.Sim.Parite.ModeDeVie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisLifestyleParityTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisLifestyleParity;
	namespace V = AnastasisLifestyleParity::Vecteurs;
	namespace L = AnastasisLifestyle;

	int32 Compared = 0;
	int32 Errors = 0;
	auto Fail = [this, &Errors](const FString& Message)
	{
		if (++Errors <= 30)
		{
			AddError(Message);
		}
	};
	auto CheckD = [&Compared, &Fail](const TCHAR* Case, int32 Index, const TCHAR* Field, double Got, uint64 Expected)
	{
		Compared += 1;
		if (P::LifeToBits(Got) != Expected)
		{
			Fail(FString::Printf(TEXT("%s[%d].%s : attendu %016llx, obtenu %016llx"), Case, Index, Field, Expected, P::LifeToBits(Got)));
		}
	};
	auto CheckS = [&Compared, &Fail](const TCHAR* Case, int32 Index, const TCHAR* Field, const FString& Got, const ANSICHAR* Expected)
	{
		Compared += 1;
		const FString Want = P::LifeText(Expected);
		if (!Got.Equals(Want, ESearchCase::CaseSensitive))
		{
			Fail(FString::Printf(TEXT("%s[%d].%s : attendu '%s', obtenu '%s'"), Case, Index, Field, *Want, *Got));
		}
	};
	auto CheckLifestyle = [&CheckD, &CheckS](const TCHAR* Case, int32 Index, const L::FLifestyle& Got,
		const ANSICHAR* Id, uint64 Since, uint64 Rhythm, uint64 Noted)
	{
		CheckS(Case, Index, TEXT("id"), Got.Id, Id);
		CheckD(Case, Index, TEXT("sinceDay"), Got.SinceDay, Since);
		CheckD(Case, Index, TEXT("rhythmScore"), Got.RhythmScore, Rhythm);
		CheckD(Case, Index, TEXT("lastNotedDay"), Got.LastNotedDay, Noted);
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::LifestyleDayPhaseVectors); ++I)
	{
		const V::FLifestyleDayPhaseVector& Vec = V::LifestyleDayPhaseVectors[I];
		CheckS(TEXT("DayPhase"), I, TEXT("phase"), L::DayPhaseId(L::DayPhase(P::LifeFromBits(Vec.A0Bits))), Vec.Attendu);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::LifestyleInfoVectors); ++I)
	{
		const V::FLifestyleInfoVector& Vec = V::LifestyleInfoVectors[I];
		const L::FLifestyleInfo& Info = L::LifestyleForId(P::LifeText(Vec.A0));
		CheckS(TEXT("Info"), I, TEXT("label"), Info.Label, Vec.AttenduLabel);
		CheckS(TEXT("Info"), I, TEXT("short"), Info.Short, Vec.AttenduShort);
		CheckS(TEXT("Info"), I, TEXT("color"), Info.Color, Vec.AttenduColor);
		CheckS(TEXT("Info"), I, TEXT("marker"), Info.Marker, Vec.AttenduMarker);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::LifestyleAssignVectors); ++I)
	{
		const V::FLifestyleAssignVector& Vec = V::LifestyleAssignVectors[I];
		FAnastasisRng Rng(AnastasisJs::ToUint32(P::LifeFromBits(Vec.A0Bits)));
		L::FLifestyleSubject S;
		S.LifeStage = P::LifeText(Vec.A2);
		S.bApprenticing = Vec.A3 != 0;
		S.JobId = P::LifeText(Vec.A4);
		S.TraitExplore = P::LifeFromBits(Vec.A5Bits);
		S.TraitBuild = P::LifeFromBits(Vec.A6Bits);
		S.TraitTrade = P::LifeFromBits(Vec.A7Bits);
		S.FamilyId = P::LifeText(Vec.A8);
		S.ChildCount = Vec.A9;
		const L::FLifestyle Got = L::AssignLifestyle(&Rng, S, P::LifeText(Vec.A1));
		CheckLifestyle(TEXT("Assign"), I, Got, Vec.AttenduId, Vec.AttenduSinceDayBits, Vec.AttenduRhythmScoreBits, Vec.AttenduLastNotedDayBits);
		CheckD(TEXT("Assign"), I, TEXT("rngState"), static_cast<double>(Rng.GetState()), Vec.AttenduRngStateBits);
	}

	// Le flux de secours est GLOBAL : on le remet a la graine du vecteur, puis a la
	// sienne a la fin, pour ne rien laisser aux tests suivants.
	{
		FAnastasisRng& Fallback = GetAnastasisFallbackRng();
		const uint32 Saved = Fallback.GetState();
		for (int32 I = 0; I < UE_ARRAY_COUNT(V::LifestyleAssignFallbackVectors); ++I)
		{
			const V::FLifestyleAssignFallbackVector& Vec = V::LifestyleAssignFallbackVectors[I];
			Fallback.SetState(AnastasisJs::ToUint32(P::LifeFromBits(Vec.A0Bits)));
			L::FLifestyleSubject S;
			S.JobId = P::LifeText(Vec.A1);
			const L::FLifestyle Got = L::AssignLifestyle(nullptr, S);
			CheckS(TEXT("AssignFallback"), I, TEXT("id"), Got.Id, Vec.AttenduId);
			CheckD(TEXT("AssignFallback"), I, TEXT("rngState"), static_cast<double>(Fallback.GetState()), Vec.AttenduRngStateBits);
		}
		Fallback.SetState(Saved);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::LifestyleEnsureVectors); ++I)
	{
		const V::FLifestyleEnsureVector& Vec = V::LifestyleEnsureVectors[I];
		FAnastasisRng Rng(AnastasisJs::ToUint32(P::LifeFromBits(Vec.A0Bits)));
		TOptional<L::FLifestyle> Life;
		if (Vec.A1 != 0)
		{
			L::FLifestyle Saved;
			Saved.Id = P::LifeText(Vec.A2);
			Saved.SinceDay = P::LifeFromBits(Vec.A3Bits);
			Saved.RhythmScore = P::LifeFromBits(Vec.A4Bits);
			Saved.LastNotedDay = P::LifeFromBits(Vec.A5Bits);
			Life = Saved;
		}
		L::FLifestyleSubject S;
		S.JobId = TEXT("farmer");
		const L::FLifestyle& Got = L::EnsureLifestyle(Life, S, &Rng);
		CheckLifestyle(TEXT("Ensure"), I, Got, Vec.AttenduId, Vec.AttenduSinceDayBits, Vec.AttenduRhythmScoreBits, Vec.AttenduLastNotedDayBits);
		CheckD(TEXT("Ensure"), I, TEXT("rngState"), static_cast<double>(Rng.GetState()), Vec.AttenduRngStateBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::LifestyleBiasVectors); ++I)
	{
		const V::FLifestyleBiasVector& Vec = V::LifestyleBiasVectors[I];
		const FString Goal = P::LifeText(Vec.A2);
		TOptional<L::FLifestyle> Life = P::WithMode(P::LifeText(Vec.A0));
		const L::FLifestyleSubject S = P::Profile(Vec.A3, Goal);
		CheckD(TEXT("Bias"), I, TEXT("bias"), L::LifestyleBias(Life, S, nullptr, P::LifeFromBits(Vec.A1Bits), Goal), Vec.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::LifestyleTravelVectors); ++I)
	{
		const V::FLifestyleTravelVector& Vec = V::LifestyleTravelVectors[I];
		TOptional<L::FLifestyle> Life = P::WithMode(P::LifeText(Vec.A0));
		const L::FLifestyleSubject S = P::Profile(Vec.A3, P::LifeText(Vec.A2));
		CheckD(TEXT("Travel"), I, TEXT("factor"), L::LifestyleTravelFactor(Life, S, nullptr, P::LifeFromBits(Vec.A1Bits)), Vec.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::LifestyleIndoorVectors); ++I)
	{
		const V::FLifestyleIndoorVector& Vec = V::LifestyleIndoorVectors[I];
		TOptional<L::FLifestyle> Life = P::WithMode(P::LifeText(Vec.A0));
		const L::FLifestyleSubject S;
		CheckD(TEXT("Indoor"), I, TEXT("duration"),
			L::LifestyleIndoorDuration(Life, S, P::LifeText(Vec.A1), P::LifeFromBits(Vec.A2Bits)), Vec.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::LifestyleTargetVectors); ++I)
	{
		const V::FLifestyleTargetVector& Vec = V::LifestyleTargetVectors[I];
		TOptional<L::FLifestyle> Life = P::WithMode(P::LifeText(Vec.A0));
		L::FLifestyleSubject S;
		if (Vec.A3 != 0)
		{
			S.HomeId = TEXT("h1");
		}
		S.FavoriteBuildingId = P::LifeText(Vec.A4);
		const P::FTestWorld World(Vec.A2);
		CheckS(TEXT("Target"), I, TEXT("building"), L::LifestyleTargetBuilding(Life, S, nullptr, World, P::LifeText(Vec.A1)), Vec.Attendu);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::LifestylePlaceUseVectors); ++I)
	{
		const V::FLifestylePlaceUseVector& Vec = V::LifestylePlaceUseVectors[I];
		FAnastasisRng Rng(AnastasisJs::ToUint32(P::LifeFromBits(Vec.A0Bits)));
		L::FPlaceUseEntry Entry;
		Entry.BuildingId = P::LifeText(Vec.A2);
		L::FLifestyleSubject Actor;
		Actor.HomeId = P::LifeText(Vec.A1);
		for (int32 K = 0; K < 12; ++K)
		{
			// Meme ordre de tirages que la declaration : mode, usage, puis la quantite
			// (un tirage, deux si le premier ne vaut pas la petite quantite).
			const FString Id = L::LifestyleId(static_cast<L::ELifestyle>(static_cast<int32>(AnastasisJs::Floor(Rng.Next() * L::NumLifestyles))));
			const FString Kind = P::Kinds[static_cast<int32>(AnastasisJs::Floor(Rng.Next() * UE_ARRAY_COUNT(P::Kinds)))];
			const double Amount = Rng.Next() < 0.25 ? 0.01 : Rng.Next() * 3.0;
			TOptional<L::FLifestyle> Life = P::WithMode(Id);
			L::LifestyleNotePlaceUse(Life, Actor, Kind, Entry, Amount);
		}
		CheckD(TEXT("PlaceUse"), I, TEXT("work"), Entry.Work, Vec.AttenduWorkBits);
		CheckD(TEXT("PlaceUse"), I, TEXT("social"), Entry.Social, Vec.AttenduSocialBits);
		CheckD(TEXT("PlaceUse"), I, TEXT("home"), Entry.Home, Vec.AttenduHomeBits);
		FString Keys;
		for (int32 K = 0; K < Entry.Lifestyle.Num(); ++K)
		{
			Keys += (K > 0 ? TEXT(",") : TEXT("")) + Entry.Lifestyle[K].Key;
		}
		CheckS(TEXT("PlaceUse"), I, TEXT("keys"), Keys, Vec.AttenduKeys);
		const uint64* Expected = &Vec.AttenduV_earlyBirdBits;
		for (int32 M = 0; M < L::NumLifestyles; ++M)
		{
			const FString Id = L::LifestyleId(static_cast<L::ELifestyle>(M));
			const TPair<FString, double>* Slot = Entry.Lifestyle.FindByPredicate([&Id](const TPair<FString, double>& P2) { return P2.Key == Id; });
			CheckD(TEXT("PlaceUse"), I, *Id, Slot ? Slot->Value : -1.0, Expected[M]);
		}
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::LifestyleDailyVectors); ++I)
	{
		const V::FLifestyleDailyVector& Vec = V::LifestyleDailyVectors[I];
		FAnastasisRng Rng(AnastasisJs::ToUint32(P::LifeFromBits(Vec.A2Bits)));
		TOptional<L::FLifestyle> Life = P::WithMode(P::LifeText(Vec.A0));
		Life->RhythmScore = P::LifeFromBits(Vec.A1Bits);
		L::FLifestyleSubject S;
		double Day = 1.0;
		int32 Rises = 0;
		for (int32 K = 0; K < 60; ++K)
		{
			Day += AnastasisJs::Floor(Rng.Next() * 3.0);
			const double Frac = Rng.Next();
			S.Goal = P::Goals[static_cast<int32>(AnastasisJs::Floor(Rng.Next() * UE_ARRAY_COUNT(P::Goals)))];
			const double Before = Life->RhythmScore;
			L::LifestyleDailyUpdate(Life, S, nullptr, Day, Frac);
			if (Life->RhythmScore > Before)
			{
				Rises += 1;
			}
		}
		CheckD(TEXT("Daily"), I, TEXT("rhythmScore"), Life->RhythmScore, Vec.AttenduRhythmScoreBits);
		CheckD(TEXT("Daily"), I, TEXT("lastNotedDay"), Life->LastNotedDay, Vec.AttenduLastNotedDayBits);
		Compared += 1;
		if (Rises != Vec.AttenduAligned)
		{
			Fail(FString::Printf(TEXT("Daily[%d].aligned : attendu %d, obtenu %d"), I, Vec.AttenduAligned, Rises));
		}
	}

	if (Errors > 30)
	{
		AddError(FString::Printf(TEXT("... %d ecarts en tout"), Errors));
	}
	AddInfo(FString::Printf(TEXT("%d valeurs comparees, %d ecarts"), Compared, Errors));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
