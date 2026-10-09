// player-goal-stall-001 -- un joueur a qui l'on choisit le remede quand le corps parle (dormir, boire, manger) vit
// trente jours dans Valmire. Le pilote de memory-pie ne choisissait jamais de dormir : epuise, le joueur refusait
// tout autre but (« le corps passe devant », ecart n°21), attendait, et mourait de faim au jour 7 ou 8.

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Sim/AnastasisTimeWarp.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr uint32 kStallSeed = 12345u;

	struct FStallScratch
	{
		UWorld* World = nullptr;
		UAnastasisSimulationSubsystem* Host = nullptr;

		FStallScratch()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (World)
			{
				FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.SetCurrentWorld(World);
				Host = World->GetSubsystem<UAnastasisSimulationSubsystem>();
			}
		}

		~FStallScratch()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisPlayerThirtyDaysTest,
	"Anastasis.Joueur.TrenteJours",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisPlayerThirtyDaysTest::RunTest(const FString&)
{
	FStallScratch Scratch;
	if (!TestNotNull(TEXT("hote"), Scratch.Host)) return false;
	Scratch.Host->ResetCanonical(kStallSeed);
	FAnastasisSimulation& Sim = Scratch.Host->GetSimulation();
	AnastasisVillage::FVillage& Village = Sim.GetVillage();
	const AnastasisVillage::FPoint Settlement = Village.GetSettlement();
	if (!TestFalse(TEXT("Valmire est fondee"), Scratch.Host->SeedStartVillage(12, FMath::FloorToInt32(Settlement.X), FMath::FloorToInt32(Settlement.Y)).IsEmpty())) return false;
	const FString Player = Village.ArriveAsPlayer();
	if (!TestFalse(TEXT("le joueur arrive"), Player.IsEmpty())) return false;

	// Comme memory-pie : toutes les six heures, le remede que le corps reclame d'abord (dormir s'il est epuise),
	// puis boire s'il a soif, manger s'il a faim, sinon aller parler.
	const double Step = FAnastasisSimulation::DayLength / 4.0;
	int32 Stalled = 0;
	for (int32 Index = 0; Index < 30 * 4; ++Index)
	{
		const AnastasisVillage::FNpc* Me = Village.FindNpc(Player);
		if (!Me) break;
		const TCHAR* Goal = Me->Needs.Energy <= 25.0 ? TEXT("rest")
			: Me->Needs.Thirst >= 40.0 ? TEXT("drink") : (Me->Needs.Hunger >= 40.0 ? TEXT("eat") : TEXT("socialize"));
		Village.ChoosePlayerGoal(Goal);
		AnastasisTimeWarp::Advance(Sim, Step);
		Me = Village.FindNpc(Player);
		if (!Me) break;
		if (Me->Activity == TEXT("attend")) ++Stalled;
		if (Index % 4 == 3 || Me->Activity == TEXT("attend"))
		{
			const auto* Refusal = Village.GetPlayerRefusal();
			const FString RefusalText = Refusal ? Refusal->Wanted + TEXT("/") + Refusal->Reason : FString(TEXT("-"));
			int32 BlockedAround = 0;
			for (int32 DY = -1; DY <= 1; ++DY)
				for (int32 DX = -1; DX <= 1; ++DX)
					BlockedAround += (DX || DY) && Village.IsFootBlocked(FMath::FloorToInt32(Me->X) + DX + 0.5, FMath::FloorToInt32(Me->Y) + DY + 0.5) ? 1 : 0;
			AddInfo(FString::Printf(TEXT("JOUEUR_PAS jour=%d pose=%s goal=%s activite=%s soif=%.1f faim=%.1f energie=%.1f echecs=%d bloque=%d x=%.2f y=%.2f inside=%d refus=%s table=%s porte=%s murs=%d pied=%d"),
				Sim.GetDay(), Goal, *Me->Goal, *Me->Activity, Me->Needs.Thirst, Me->Needs.Hunger, Me->Needs.Energy, Me->FailedActions, Me->StuckStage,
				Me->X, Me->Y, Me->Inside.bActive ? 1 : 0, *RefusalText,
				*Me->LastDecision.TableWinner, *Me->LastDecision.CommitGate, BlockedAround, Village.IsFootBlocked(Me->X, Me->Y) ? 1 : 0));
		}
	}
	const AnastasisVillage::FNpc* Me = Village.FindNpc(Player);
	TestNotNull(TEXT("le joueur vit trente jours"), Me);
	if (Me)
	{
		TestTrue(TEXT("il n'a pas soif a mourir"), Me->Needs.Thirst < 90.0);
		TestTrue(TEXT("il n'a pas faim a mourir"), Me->Needs.Hunger < 90.0);
		TestTrue(TEXT("il a dormi"), Me->Needs.Energy > 12.0);
	}
	AddInfo(FString::Printf(TEXT("JOUEUR_30J vivant=%d attentes=%d"), Me ? 1 : 0, Stalled));
	return true;
}

#endif
