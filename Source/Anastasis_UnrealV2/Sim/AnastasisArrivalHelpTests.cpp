// arrivant-seul-001 -- le panneau de l'habitant arrive seul n'annonce que des touches qui marchent.
//
// Depuis player-start-002 le Play incarne un arrivant sans famille. La scene d'entraide (Entree, E, F, X, J) ne s'ouvre
// pas pour lui (StartHelpScene refuse quand un joueur existe) ; le panneau annoncait pourtant [F] Batir, [X], [J] Carnet.
// Alexandre (2026-10-09) : « je veux rester seul arrivant ». Le texte est une fonction pure : on le lit sans monde.

#include "Misc/AutomationTest.h"

#include "Sim/AnastasisSimulationSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisArrivalHelpText,
	"Anastasis.Sim.Joueur.ArrivantSeul.Panneau",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisArrivalHelpText::RunTest(const FString&)
{
	const FString Text = UAnastasisSimulationSubsystem::ArrivalHelpText(3, TEXT("Nikolaos"));

	// Ce qu'il dit : qui il est, comment marcher, ou lire ses buts, comment regler le temps.
	TestTrue(TEXT("le jour et le nom"), Text.Contains(TEXT("JOUR 3")) && Text.Contains(TEXT("Nikolaos")));
	TestTrue(TEXT("il est arrive seul"), Text.Contains(TEXT("arrive seul")));
	TestTrue(TEXT("marcher au clavier AZERTY et QWERTY"), Text.Contains(TEXT("Z Q S D")) && Text.Contains(TEXT("W A S D")));
	TestTrue(TEXT("la ligne des buts"), Text.Contains(TEXT("BUTS")));
	TestTrue(TEXT("le temps : 8 et 9"), Text.Contains(TEXT("8 ")) && Text.Contains(TEXT("9 ")));

	// Ce qu'il ne dit jamais : les touches de la scene d'entraide, qui n'agissent que dans cette scene.
	for (const TCHAR* Dead : { TEXT("[E]"), TEXT("[F]"), TEXT("[X]"), TEXT("[J]"), TEXT("[ENTREE]"), TEXT("Batir"), TEXT("Carnet") })
	{
		TestFalse(*FString::Printf(TEXT("pas de %s"), Dead), Text.Contains(Dead));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
