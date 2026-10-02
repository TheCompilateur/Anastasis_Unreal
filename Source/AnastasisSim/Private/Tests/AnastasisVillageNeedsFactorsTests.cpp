#include "Misc/AutomationTest.h"

#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// Les besoins au rythme de chaque habitant, dans la boucle du village (mission needs-wiring-001).
//
// Les cinq facteurs et `tickConditioning` sont prouves bit a bit hors du village
// (Anastasis.Sim.Parite.BesoinsFacteurs, .Conditionnement). Ce test prouve leur BRANCHEMENT
// dans `FVillage::UpdateNpc` (ecart n°19) : un habitant sans phenotype ni conditionnement
// garde exactement les bits d'avant ; un phenotype personnel change la faim, la soif et la
// depense par ses facteurs ; le conditionnement avance en fin de besoins avec les drapeaux de
// la reference (travail, fatigue lue APRES la branche).

namespace AnastasisVillageNeedsFactorsTest
{
	AnastasisWorld::FWorld MakeFlatWorld(int32 W, int32 H)
	{
		AnastasisWorld::FWorld World;
		World.W = W;
		World.H = H;
		World.Tiles.SetNum(W * H);
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				AnastasisWorld::FTile& Tile = World.Tiles[Y * W + X];
				Tile.X = X;
				Tile.Y = Y;
				Tile.Type = AnastasisWorld::ETileType::Grass;
				Tile.Alt = 0.5;
				Tile.Wetness = 0.3;
			}
		}
		return World;
	}

	AnastasisVillage::FNpc MakeNpc(const TCHAR* Id, double X, double Y, const TCHAR* Goal, double Energy)
	{
		AnastasisVillage::FNpc N;
		N.Id = Id;
		N.X = X;
		N.Y = Y;
		N.Goal = Goal;
		N.Needs.Hunger = 20.0;
		N.Needs.Thirst = 10.0;
		N.Needs.Energy = Energy;
		N.Needs.Social = 80.0;
		N.Needs.Leisure = 80.0;
		N.Needs.Hygiene = 80.0;
		N.Needs.Health = 95.0;
		N.Needs.Morale = 60.0;
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageNeedsFactorsTest,
	"Anastasis.Sim.Village.BesoinsParHabitant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageNeedsFactorsTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisVillageNeedsFactorsTest;
	constexpr double Dt = 1.0 / 60.0;
	const AnastasisWorld::FWorld World = MakeFlatWorld(48, 48);

	// A : median (rien de pose, comme tout habitant cree par le C++).
	// B : phenotype personnel, conditionnement neutre.
	// C : au travail et fatigue (energie 60 : 100 - 60 >= 32), conditionnement pose.
	AnastasisVillage::FNpc A = MakeNpc(TEXT("npc-a"), 10.5, 10.5, TEXT("observer"), 85.0);
	AnastasisVillage::FNpc B = MakeNpc(TEXT("npc-b"), 12.5, 10.5, TEXT("observer"), 85.0);
	AnastasisGenome::FPhenotype& P = B.Phenotype.Emplace();
	P.MetabolicDemandMultiplier = 1.1719094149884768;
	P.HydrationLossMultiplier = 0.9246623027720489;
	P.FatigueRecoveryMultiplier = 1.1373518139589578;
	B.Conditioning.Emplace();
	AnastasisVillage::FNpc C = MakeNpc(TEXT("npc-c"), 14.5, 10.5, TEXT("gatherFood"), 60.0);
	C.Conditioning.Emplace();

	AnastasisVillage::FVillage V;
	V.Bind(World);
	FString Error;
	const bool bRestored = V.RestoreForHarness({}, { A, B, C }, {}, 0, 1, 10, Error);
	TestTrue(FString::Printf(TEXT("reprise des trois habitants (%s)"), *Error), bRestored);
	if (!bRestored) return false;

	V.UpdateActors(37.8 + Dt, Dt);
	const AnastasisVillage::FNpc* GotA = V.FindNpc(TEXT("npc-a"));
	const AnastasisVillage::FNpc* GotB = V.FindNpc(TEXT("npc-b"));
	const AnastasisVillage::FNpc* GotC = V.FindNpc(TEXT("npc-c"));
	if (!GotA || !GotB || !GotC)
	{
		AddError(TEXT("un habitant a disparu"));
		return false;
	}

	// 1. Median : les bits de la branche par defaut, sans facteurs -- ceux d'avant le branchement.
	{
		AnastasisNeeds::FNeeds Expect = A.Needs;
		AnastasisNeeds::TickNeeds(Expect, Dt, false, false);
		TestEqual(TEXT("A median : faim au bit pres"), GotA->Needs.Hunger, Expect.Hunger);
		TestEqual(TEXT("A median : soif au bit pres"), GotA->Needs.Thirst, Expect.Thirst);
		TestEqual(TEXT("A median : energie au bit pres"), GotA->Needs.Energy, Expect.Energy);
		TestFalse(TEXT("A median : aucun conditionnement cree"), GotA->Conditioning.IsSet());
	}

	// 2. Phenotype personnel : la meme branche, avec SES facteurs.
	{
		AnastasisNeeds::FNeeds Expect = B.Needs;
		const AnastasisNeeds::FNeedFactors F = AnastasisNeeds::NeedFactorsFor(&B.Phenotype.GetValue(), &B.Conditioning.GetValue());
		AnastasisNeeds::TickNeeds(Expect, Dt, false, false, F);
		TestEqual(TEXT("B : faim x metabolicDemandMultiplier"), GotB->Needs.Hunger, Expect.Hunger);
		TestEqual(TEXT("B : soif x hydrationLossMultiplier"), GotB->Needs.Thirst, Expect.Thirst);
		TestEqual(TEXT("B : energie x metabolic x fatigueAdaptation"), GotB->Needs.Energy, Expect.Energy);
		TestTrue(TEXT("B : la faim monte plus vite que celle de A"), GotB->Needs.Hunger > GotA->Needs.Hunger);
		TestTrue(TEXT("B : la soif monte moins vite que celle de A"), GotB->Needs.Thirst < GotA->Needs.Thirst);
		// Ni travail ni repos : fatigueAdaptation et recoveryConditioning restent, workConditioning aussi.
		TestEqual(TEXT("B : conditionnement immobile hors travail et repos"), GotB->Conditioning->FatigueAdaptation, 0.5);
	}

	// 3. Au travail et fatigue : le conditionnement avance, drapeaux sur les metres d'APRES la branche.
	{
		AnastasisNeeds::FNeeds ExpectNeeds = C.Needs;
		AnastasisNeeds::TickNeeds(ExpectNeeds, Dt, false, true);
		TestEqual(TEXT("C : energie au bit pres (facteurs neutres, travail)"), GotC->Needs.Energy, ExpectNeeds.Energy);
		AnastasisConditioning::FConditioning Expect;
		AnastasisNeeds::TickNeedsConditioning(Expect, ExpectNeeds, Dt, true, false);
		TestEqual(TEXT("C : workConditioning"), GotC->Conditioning->WorkConditioning, Expect.WorkConditioning);
		TestEqual(TEXT("C : fatigueAdaptation"), GotC->Conditioning->FatigueAdaptation, Expect.FatigueAdaptation);
		TestEqual(TEXT("C : recoveryConditioning"), GotC->Conditioning->RecoveryConditioning, Expect.RecoveryConditioning);
		TestTrue(TEXT("C : travailler fatigue adapte (> 0,5)"), GotC->Conditioning->FatigueAdaptation > 0.5);
		TestTrue(TEXT("C : travailler deconditionne la recuperation (< 0,5)"), GotC->Conditioning->RecoveryConditioning < 0.5);
		AddInfo(FString::Printf(TEXT("C apres un tick : work %.17g, fatigue %.17g, recovery %.17g"),
			GotC->Conditioning->WorkConditioning, GotC->Conditioning->FatigueAdaptation, GotC->Conditioning->RecoveryConditioning));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
