#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "Harness/AnastasisHarnessTrace.h"
#include "Harness/AnastasisJsSave.h"
#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisReconsider.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Life/AnastasisWorkShift.h"
#include "Sim/AnastasisSimulation.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Reconsideration et quart de travail — mission reconsider-001.
 *
 * `Parite.Reconsideration` : les fonctions de la reference executees telles quelles
 * (parity/reconsider.mjs). `Village.Reconsideration` : les 75 tirages l. 893 mesures sur un
 * jour du scenario endurance (trace-reconsider.mjs), rejoues sur le village du harnais : la
 * chance C++ doit etre celle que la reference a comparee, au bit, et le resultat le meme.
 * Ne jamais corriger un vecteur a la main.
 */
namespace AnastasisReconsiderParity
{
	namespace Vecteurs
	{
#include "AnastasisReconsiderPureVectors.inl"
#include "AnastasisReconsiderVectors.inl"
	}

	static double RcFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	static uint64 RcToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	static FString RcText(const ANSICHAR* Utf8)
	{
		return FString(UTF8_TO_TCHAR(Utf8));
	}

	static AnastasisNeeds::FNeeds NeedsOf(const uint64 Bits[8])
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = RcFromBits(Bits[0]);
		N.Energy = RcFromBits(Bits[1]);
		N.Social = RcFromBits(Bits[2]);
		N.Leisure = RcFromBits(Bits[3]);
		N.Hygiene = RcFromBits(Bits[4]);
		N.Thirst = RcFromBits(Bits[5]);
		N.Health = RcFromBits(Bits[6]);
		N.Morale = RcFromBits(Bits[7]);
		return N;
	}

	static AnastasisWorkShift::EState StateOf(const FString& Id)
	{
		using S = AnastasisWorkShift::EState;
		if (Id == TEXT("OFF_DUTY")) return S::OffDuty;
		if (Id == TEXT("COMMUTING")) return S::Commuting;
		if (Id == TEXT("ON_SHIFT")) return S::OnShift;
		if (Id == TEXT("BREAK")) return S::Break;
		return S::None;
	}

	// Memes metres que la declaration : un habitant calme, un habitant en crise.
	static AnastasisNeeds::FNeeds Calme()
	{
		const uint64 B[8] = {
			RcToBits(10), RcToBits(80), RcToBits(70), RcToBits(70), RcToBits(70), RcToBits(5), RcToBits(95), RcToBits(60) };
		return NeedsOf(B);
	}
	static AnastasisNeeds::FNeeds Critique()
	{
		const uint64 B[8] = {
			RcToBits(90), RcToBits(5), RcToBits(5), RcToBits(5), RcToBits(5), RcToBits(99), RcToBits(5), RcToBits(0) };
		return NeedsOf(B);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisReconsiderParityTest,
	"Anastasis.Sim.Parite.Reconsideration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisReconsiderParityTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisReconsiderParity;
	namespace V = AnastasisReconsiderParity::Vecteurs;
	namespace R = AnastasisReconsider;
	namespace W = AnastasisWorkShift;

	int32 Compared = 0;
	int32 Errors = 0;
	auto Fail = [this, &Errors](const FString& M) { if (++Errors <= 30) AddError(M); };

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::ReconsiderNeedsVectors); ++I)
	{
		const V::FReconsiderNeedsVector& Vec = V::ReconsiderNeedsVectors[I];
		const uint64 M[8] = { Vec.A0Bits, Vec.A1Bits, Vec.A2Bits, Vec.A3Bits, Vec.A4Bits, Vec.A5Bits, Vec.A6Bits, Vec.A7Bits };
		const bool bCritical = AnastasisNeeds::AreNeedsCritical(P::NeedsOf(M));
		const double Got = R::NeedsReconsiderChance(bCritical, P::RcText(Vec.A8), P::RcFromBits(Vec.A9Bits));
		Compared += 1;
		if (P::RcToBits(Got) != Vec.AttenduBits)
			Fail(FString::Printf(TEXT("needsReconsiderChance[%d] : %016llx, attendu %016llx"), I, P::RcToBits(Got), Vec.AttenduBits));
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::ReconsiderPersonalPhaseVectors); ++I)
	{
		const V::FReconsiderPersonalPhaseVector& Vec = V::ReconsiderPersonalPhaseVectors[I];
		TOptional<AnastasisLifestyle::FLifestyle> Life;
		const FString Id = P::RcText(Vec.A1);
		if (!Id.IsEmpty())
		{
			AnastasisLifestyle::FLifestyle L;
			L.Id = Id;
			Life = L;
		}
		const FString Got = AnastasisRhythm::PhaseId(R::PersonalPhase(AnastasisRhythm::DayFracOf(P::RcFromBits(Vec.A0Bits)), Life));
		Compared += 1;
		if (Got != P::RcText(Vec.Attendu))
			Fail(FString::Printf(TEXT("villagePhaseFor[%d] (%s) : %s, attendu %s"), I, *Id, *Got, *P::RcText(Vec.Attendu)));
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::ReconsiderShiftCommitVectors); ++I)
	{
		const V::FReconsiderShiftCommitVector& Vec = V::ReconsiderShiftCommitVectors[I];
		// Sans etat de depart, la reference n'a pas de `workShift` du tout : un quart vierge.
		W::FWorkShift Shift;
		Shift.State = P::StateOf(P::RcText(Vec.A0));
		if (Shift.State != W::EState::None)
		{
			Shift.Goal = P::RcText(Vec.A1);
			Shift.StartedAt = P::RcFromBits(Vec.A2Bits);
			Shift.FloorUntil = P::RcFromBits(Vec.A3Bits);
		}
		const FString Goal = P::RcText(Vec.A4);
		const double Now = P::RcFromBits(Vec.A5Bits);
		const bool bCritical = AnastasisNeeds::AreNeedsCritical(Vec.A6 != 0 ? P::Critique() : P::Calme());
		const bool bEntry = W::IsShiftEntryGoal(Goal) || W::OpensExtractionShift(Goal, P::RcText(Vec.A7),
			P::RcFromBits(Vec.A10Bits), P::RcFromBits(Vec.A8Bits), P::RcFromBits(Vec.A9Bits),
			Vec.A11 != 0, P::RcFromBits(Vec.A12Bits), P::RcFromBits(Vec.A13Bits));
		W::NoteShiftGoalCommit(Shift, Goal, Now, bCritical, bEntry);
		const bool bShields = W::ShiftShields(Shift, Goal, Now, bCritical);
		Compared += 5;
		const FString State = W::StateId(Shift.State);
		if (State != P::RcText(Vec.AttenduState) || Shift.Goal != P::RcText(Vec.AttenduGoal)
			|| P::RcToBits(Shift.StartedAt) != Vec.AttenduStartedAtBits || P::RcToBits(Shift.FloorUntil) != Vec.AttenduFloorUntilBits
			|| bShields != (Vec.AttenduShields != 0))
		{
			Fail(FString::Printf(TEXT("noteShiftGoalCommit[%d] (%s -> %s) : etat %s / %s, but %s / %s, plancher %g / %g, bouclier %d / %d"),
				I, *P::RcText(Vec.A0), *Goal, *State, *P::RcText(Vec.AttenduState), *Shift.Goal, *P::RcText(Vec.AttenduGoal),
				Shift.FloorUntil, P::RcFromBits(Vec.AttenduFloorUntilBits), bShields ? 1 : 0, Vec.AttenduShields));
		}
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::ReconsiderStickinessVectors); ++I)
	{
		const V::FReconsiderStickinessVector& Vec = V::ReconsiderStickinessVectors[I];
		const uint64 M[8] = { Vec.A0Bits, Vec.A1Bits, Vec.A2Bits, Vec.A3Bits, Vec.A4Bits, Vec.A5Bits, Vec.A6Bits, Vec.A7Bits };
		const AnastasisNeeds::FNeeds Needs = P::NeedsOf(M);
		R::FStickSubject Stick;
		Stick.CurrentGoal = P::RcText(Vec.A8);
		Stick.bCritical = AnastasisNeeds::AreNeedsCritical(Needs);
		Stick.bCurrentRelievesCritical = R::IsCriticalReliefGoal(Needs, Vec.A17, Stick.CurrentGoal);
		const double Now = 500.0;
		if (Vec.A10 != 0) Stick.PhaseChangedAt = Now - P::RcFromBits(Vec.A11Bits);
		Stick.TraitBuild = P::RcFromBits(Vec.A12Bits);
		Stick.TraitTrade = P::RcFromBits(Vec.A13Bits);
		Stick.TraitGather = P::RcFromBits(Vec.A14Bits);
		Stick.TraitExplore = P::RcFromBits(Vec.A15Bits);
		Stick.bWorkSession = Vec.A16 != 0;
		Stick.InventoryLoad = Vec.A17;
		const FString Row = P::RcText(Vec.A9);
		const double Got = R::GoalStickinessBonus(Stick, Now, Row);
		Compared += 1;
		if (P::RcToBits(Got) != Vec.AttenduBits)
			Fail(FString::Printf(TEXT("goalStickinessBonus[%d] (%s / ligne %s) : %.17g, attendu %.17g"), I, *Stick.CurrentGoal, *Row,
				Got, P::RcFromBits(Vec.AttenduBits)));
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::ReconsiderShiftArrivalVectors); ++I)
	{
		const V::FReconsiderShiftArrivalVector& Vec = V::ReconsiderShiftArrivalVectors[I];
		W::FWorkShift Shift;
		Shift.State = P::StateOf(P::RcText(Vec.A0));
		W::NoteShiftArrival(Shift);
		Compared += 1;
		if (FString(W::StateId(Shift.State)) != P::RcText(Vec.Attendu))
			Fail(FString::Printf(TEXT("noteShiftArrival[%d] : %s, attendu %s"), I, W::StateId(Shift.State), *P::RcText(Vec.Attendu)));
	}

	TestEqual(TEXT("plancher du quart : 15 s"), W::FloorSeconds, 15.0);
	if (Errors > 30) AddError(FString::Printf(TEXT("... %d ecarts en tout"), Errors));
	AddInfo(FString::Printf(TEXT("%d valeurs comparees, %d ecarts"), Compared, Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisReconsiderReplayTest,
	"Anastasis.Sim.Village.Reconsideration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisReconsiderReplayTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisReconsiderParity;
	namespace V = AnastasisReconsiderParity::Vecteurs;

	const FString Scenario = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("tools/migration/scenarios/endurance.json")));
	AnastasisHarnessTrace::FScenarioInfo Info;
	AnastasisJsSave::FState Read;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("scenario lu (%s)"), *Error), AnastasisHarnessTrace::LoadScenario(Scenario, Info, Read, Error)))
	{
		return false;
	}
	FAnastasisSimulation Sim;
	if (!TestTrue(FString::Printf(TEXT("reprise (%s)"), *Error), AnastasisHarnessTrace::Restore(Read, Sim, Error)))
	{
		return false;
	}
	AnastasisVillage::FVillage& Village = Sim.GetVillage();

	int32 Errors = 0;
	int32 Chosen = 0;
	for (int32 I = 0; I < UE_ARRAY_COUNT(V::ReconsiderDraws); ++I)
	{
		const V::FReconsiderDrawVector& D = V::ReconsiderDraws[I];
		AnastasisVillage::FNpc* Npc = Village.FindNpcMutable(P::RcText(D.Npc));
		const FString Where = FString::Printf(TEXT("tick %d %s"), D.Tick, *P::RcText(D.Npc));
		if (!Npc)
		{
			AddError(Where + TEXT(" : habitant absent"));
			continue;
		}
		// La photo mesuree : but, age du but, derniere bascule, mode de vie, quart, metres, cible.
		Npc->Goal = P::RcText(D.Goal);
		Npc->GoalSince = P::RcFromBits(D.GoalSinceBits);
		Npc->PhaseChangedAt.Reset();
		if (D.HasPhaseChangedAt != 0) Npc->PhaseChangedAt = P::RcFromBits(D.PhaseChangedAtBits);
		Npc->Lifestyle.Reset();
		if (*D.Lifestyle)
		{
			AnastasisLifestyle::FLifestyle L;
			L.Id = P::RcText(D.Lifestyle);
			Npc->Lifestyle = L;
		}
		Npc->WorkShift = AnastasisWorkShift::FWorkShift();
		Npc->WorkShift.State = P::StateOf(P::RcText(D.ShiftState));
		Npc->WorkShift.Goal = P::RcText(D.ShiftGoal);
		Npc->WorkShift.StartedAt = P::RcFromBits(D.ShiftStartedAtBits);
		Npc->WorkShift.FloorUntil = P::RcFromBits(D.ShiftFloorUntilBits);
		Npc->Needs = P::NeedsOf(D.Metres);
		Npc->bHasTarget = true;

		const double Chance = Village.ReconsiderChanceAt(*Npc, P::RcFromBits(D.TimeBits), P::RcFromBits(D.ThinkDtBits));
		const bool bChooses = P::RcFromBits(D.DrawBits) < Chance;
		Chosen += bChooses ? 1 : 0;
		if (P::RcToBits(Chance) != D.ChanceBits || bChooses != (D.Chooses != 0))
		{
			if (++Errors <= 30)
			{
				AddError(FString::Printf(TEXT("%s (%s) : chance %.6f / %.6f, choisit %d / %d (obtenu / attendu)"), *Where, *Npc->Goal,
					Chance, P::RcFromBits(D.ChanceBits), bChooses ? 1 : 0, D.Chooses));
			}
		}
	}
	if (Errors > 30) AddError(FString::Printf(TEXT("... %d tirages faux en tout"), Errors));
	AddInfo(FString::Printf(TEXT("%d tirages l. 893 mesures rejoues, %d reconsiderations, %d faux"),
		static_cast<int32>(UE_ARRAY_COUNT(V::ReconsiderDraws)), Chosen, Errors));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
