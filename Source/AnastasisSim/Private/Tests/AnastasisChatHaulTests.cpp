#include "Misc/AutomationTest.h"

#include "Life/AnastasisBonds.h"
#include "Life/AnastasisNeeds.h"
#include "Work/AnastasisCraftMiss.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Coup rate et causette au depot — mission chat-on-haul-001.
 *
 * `Parite.CoupRate` : craftMissChance, bestCraftMastery (la pose de la declaration), canRollCraftMiss
 * et shouldSpeakNow avec `ambientChance` 0,1, executes par la reference (parity/chat-haul.mjs).
 * Ne jamais corriger un vecteur a la main.
 */
namespace AnastasisChatHaulParity
{
	namespace Vecteurs
	{
#include "AnastasisChatHaulVectors.inl"
#include "AnastasisChatHaulDrawVectors.inl"
	}

	static double ChFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	static uint64 ChToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	/** La pose de `habitant()` (chat-haul.mjs) : besoins `[10, 10, 90, 60, 90, 90, 95, 60]`. */
	static AnastasisNeeds::FNeeds Calm()
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 10;
		N.Thirst = 10;
		N.Energy = 90;
		N.Social = 60;
		N.Leisure = 90;
		N.Hygiene = 90;
		N.Health = 95;
		N.Morale = 60;
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisChatHaulParityTest,
	"Anastasis.Sim.Parite.CoupRate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisChatHaulParityTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisChatHaulParity;
	namespace V = AnastasisChatHaulParity::Vecteurs;
	namespace CM = AnastasisCraftMiss;

	int32 Compared = 0;
	int32 Errors = 0;
	auto Fail = [this, &Errors](const FString& M) { if (++Errors <= 30) AddError(M); };

	// La maitrise effective de chaque pose (profil, maitrise posee) : 0 si le metier n'a pas de savoir-faire.
	TMap<FString, double> Mastery;
	for (int32 I = 0; I < UE_ARRAY_COUNT(V::CraftMissMasteryVectors); ++I)
	{
		const V::FCraftMissMasteryVector& Vec = V::CraftMissMasteryVectors[I];
		Mastery.Add(FString::Printf(TEXT("%s|%016llx"), UTF8_TO_TCHAR(Vec.A0), Vec.A1Bits), P::ChFromBits(Vec.AttenduBits));
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::CraftMissChanceVectors); ++I)
	{
		const V::FCraftMissChanceVector& Vec = V::CraftMissChanceVectors[I];
		const FString Craft = UTF8_TO_TCHAR(Vec.A0);
		const double* M = Mastery.Find(FString::Printf(TEXT("%s|%016llx"), *Craft, Vec.A4Bits));
		if (!M)
		{
			Fail(FString::Printf(TEXT("craftMissChance[%d] : pose de maitrise absente"), I));
			continue;
		}
		const double Got = CM::MissChance(Craft, P::ChFromBits(Vec.A1Bits), *M, Vec.A2, P::ChFromBits(Vec.A3Bits));
		Compared += 1;
		if (P::ChToBits(Got) != Vec.AttenduBits)
		{
			Fail(FString::Printf(TEXT("craftMissChance[%d] (%s, skill %g, coups %d, energie %g, maitrise %g) : %.17g, attendu %.17g"),
				I, *Craft, P::ChFromBits(Vec.A1Bits), Vec.A2, P::ChFromBits(Vec.A3Bits), *M, Got, P::ChFromBits(Vec.AttenduBits)));
		}
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::CraftMissCanRollVectors); ++I)
	{
		const V::FCraftMissCanRollVector& Vec = V::CraftMissCanRollVectors[I];
		const bool Got = CM::CanRoll(UTF8_TO_TCHAR(Vec.A0), P::ChFromBits(Vec.A1Bits), P::ChFromBits(Vec.A2Bits), P::ChFromBits(Vec.A3Bits));
		Compared += 1;
		if (Got != (Vec.Attendu != 0))
		{
			Fail(FString::Printf(TEXT("canRollCraftMiss[%d] (%s, t %g, missAt %g, at %g) : %d, attendu %d"), I, UTF8_TO_TCHAR(Vec.A0),
				P::ChFromBits(Vec.A1Bits), P::ChFromBits(Vec.A2Bits), P::ChFromBits(Vec.A3Bits), Got ? 1 : 0, Vec.Attendu));
		}
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::ShouldSpeakAmbientVectors); ++I)
	{
		const V::FShouldSpeakAmbientVector& Vec = V::ShouldSpeakAmbientVectors[I];
		const double Worth = AnastasisBonds::SpeakWorth(P::Calm(), P::Calm(), Vec.A4 != 0, AnastasisBonds::EBondKind::Coworker);
		const bool Got = AnastasisBonds::ShouldSpeakNow(Worth, Vec.A3, UTF8_TO_TCHAR(Vec.A0), UTF8_TO_TCHAR(Vec.A1), P::ChFromBits(Vec.A2Bits), 0.1);
		Compared += 1;
		if (Got != (Vec.Attendu != 0))
		{
			Fail(FString::Printf(TEXT("shouldSpeakNow ambient 0,1 [%d] (%s -> %s) : %d, attendu %d"), I, UTF8_TO_TCHAR(Vec.A0), UTF8_TO_TCHAR(Vec.A1),
				Got ? 1 : 0, Vec.Attendu));
		}
	}

	// Les tirages mesures sur endurance (trace-chat-haul.mjs) : la porte etait ouverte, la chance au bit,
	// et le meme resultat.
	int32 Missed = 0;
	for (int32 I = 0; I < UE_ARRAY_COUNT(V::MeasuredMissDraws); ++I)
	{
		const V::FMeasuredMissDraw& D = V::MeasuredMissDraws[I];
		const FString Craft = UTF8_TO_TCHAR(D.Craft);
		const double Time = P::ChFromBits(D.TimeBits);
		const bool bOpen = CM::CanRoll(Craft, Time, P::ChFromBits(D.MissAtBits), P::ChFromBits(D.LastAtBits));
		const double Chance = CM::MissChance(Craft, P::ChFromBits(D.SkillBits), P::ChFromBits(D.MasteryBits), D.Swings, P::ChFromBits(D.EnergyBits));
		const bool bMissed = P::ChFromBits(D.DrawBits) < Chance;
		Missed += bMissed ? 1 : 0;
		Compared += 3;
		if (!bOpen || P::ChToBits(Chance) != D.ChanceBits || bMissed != (D.Missed != 0))
		{
			Fail(FString::Printf(TEXT("tirage mesure tick %d %s (%s) : porte %d, chance %.17g / %.17g, rate %d / %d"), D.Tick, UTF8_TO_TCHAR(D.Npc),
				*Craft, bOpen ? 1 : 0, Chance, P::ChFromBits(D.ChanceBits), bMissed ? 1 : 0, D.Missed));
		}
	}
	for (int32 I = 0; I < UE_ARRAY_COUNT(V::MeasuredChatDraws); ++I)
	{
		const V::FMeasuredChatDraw& D = V::MeasuredChatDraws[I];
		Compared += 1;
		if ((P::ChFromBits(D.DrawBits) > 0.42 ? 0 : 1) != D.Chats)
		{
			Fail(FString::Printf(TEXT("causette mesuree tick %d %s : seuil 0,42 mal lu"), D.Tick, UTF8_TO_TCHAR(D.Npc)));
		}
	}
	AddInfo(FString::Printf(TEXT("%d tirages rollCraftMiss mesures (%d rates), %d tirages maybeChatOnHaul"),
		static_cast<int32>(UE_ARRAY_COUNT(V::MeasuredMissDraws)), Missed, static_cast<int32>(UE_ARRAY_COUNT(V::MeasuredChatDraws))));

	if (Errors > 30) AddError(FString::Printf(TEXT("... %d ecarts en tout"), Errors));
	AddInfo(FString::Printf(TEXT("%d valeurs comparees, %d ecarts"), Compared, Errors));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
