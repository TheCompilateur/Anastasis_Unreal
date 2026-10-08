// memoire-decisions-001 -- ce qui est dit au feu devient memoire ; le carnet du joueur retient ce qu'il entend,
// version par version ; trente jours de Valmire ou l'on decide de batir, demande de l'aide, et se raconte.

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sim/AnastasisDialogueLines.h"
#include "Sim/AnastasisNotebook.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Sim/AnastasisTimeWarp.h"
#include "Sim/AnastasisValmireFounders.h"
#include "Sim/AnastasisVillageChronicle.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr uint32 kMemorySeed = 12345u;

	struct FMemoryScratch
	{
		UWorld* World = nullptr;
		UAnastasisSimulationSubsystem* Host = nullptr;

		FMemoryScratch()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (World)
			{
				FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.SetCurrentWorld(World);
				Host = World->GetSubsystem<UAnastasisSimulationSubsystem>();
			}
		}

		~FMemoryScratch()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		bool Found()
		{
			if (!Host) return false;
			Host->ResetCanonical(kMemorySeed);
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			return !Host->SeedStartVillage(12, FMath::FloorToInt32(Settlement.X), FMath::FloorToInt32(Settlement.Y)).IsEmpty();
		}
	};

	const AnastasisFounders::FFounder* FounderByKey(const TArray<AnastasisFounders::FFounder>& Founders, const TCHAR* Key)
	{
		return Founders.FindByPredicate([Key](const AnastasisFounders::FFounder& F) { return F.Key == Key; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisFireMemoryTest,
	"Anastasis.Memoire.Feu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisFireMemoryTest::RunTest(const FString&)
{
	FMemoryScratch Scratch;
	if (!TestTrue(TEXT("Valmire est fondee"), Scratch.Found())) return false;
	const AnastasisVillage::FVillage& Village = Scratch.Host->GetSimulation().GetVillage();
	const TArray<AnastasisFounders::FFounder>& Founders = Scratch.Host->GetFounders();
	const AnastasisFounders::FFounder* Scribe = FounderByKey(Founders, TEXT("konstantinos"));
	const AnastasisFounders::FFounder* Georgios = FounderByKey(Founders, TEXT("georgios"));
	if (!TestNotNull(TEXT("Konstantinos"), Scribe) || !TestNotNull(TEXT("Georgios"), Georgios)) return false;

	// Le repondant a vecu ses trois reponses.
	const AnastasisVillage::FNpc* ScribeNpc = Village.FindNpc(Scribe->NpcId);
	TSet<FString> LivedKinds;
	for (const AnastasisEpisodes::FEpisode& Event : ScribeNpc->Chronicle.Events)
	{
		if (Event.bFirsthand) LivedKinds.Add(Event.Kind);
	}
	TestTrue(TEXT("ou il etait"), LivedKinds.Contains(TEXT("fall")));
	TestTrue(TEXT("ce qu'il a porte"), LivedKinds.Contains(TEXT("carried")));
	TestTrue(TEXT("qui n'est pas venu"), LivedKinds.Contains(TEXT("leftBehind")));

	// Georgios l'a entendu de sa bouche, au feu : une bouche.
	bool bHeardScribe = false;
	for (const AnastasisEpisodes::FEpisode& Event : Village.FindNpc(Georgios->NpcId)->Chronicle.Events)
	{
		if (!Event.bFirsthand && Event.SourceId == Scribe->NpcId && Event.Hops == 1) bHeardScribe = true;
	}
	TestTrue(TEXT("Georgios a entendu Konstantinos au feu"), bHeardScribe);

	// Le souvenir se dit : vecu par lui, raconte par un autre.
	const AnastasisDialogue::FLibrary& Lines = AnastasisDialogue::FLibrary::Get();
	for (const AnastasisEpisodes::FEpisode& Event : ScribeNpc->Chronicle.Events)
	{
		if (Event.Kind != TEXT("leftBehind")) continue;
		const FString Lived = Lines.EpisodeLine(Event);
		AddInfo(FString::Printf(TEXT("MEMOIRE vecu : %s"), *Lived));
		TestTrue(TEXT("le vecu nomme l'absent"), Lived.Contains(Event.Note));
		AnastasisEpisodes::FEpisode Told = Event;
		Told.bFirsthand = false;
		Told.Hops = 1;
		const FString Heard = Lines.TellerVersion(Told, TEXT("Konstantinos"));
		AddInfo(FString::Printf(TEXT("MEMOIRE entendu : %s"), *Heard));
		TestTrue(TEXT("l'entendu dit de qui il vient"), Heard.Contains(TEXT("Konstantinos")));
		Told.Hops = 3;
		const FString Legend = Lines.TellerVersion(Told, TEXT("Konstantinos"));
		AddInfo(FString::Printf(TEXT("MEMOIRE legende : %s"), *Legend));
		TestFalse(TEXT("la legende a oublie qui"), Legend.Contains(TEXT("Konstantinos")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisNotebookTest,
	"Anastasis.Memoire.Carnet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisNotebookTest::RunTest(const FString&)
{
	FMemoryScratch Scratch;
	if (!TestTrue(TEXT("Valmire est fondee"), Scratch.Found())) return false;
	FAnastasisSimulation& Sim = Scratch.Host->GetSimulation();
	AnastasisVillage::FVillage& Village = Sim.GetVillage();
	const AnastasisDialogue::FLibrary& Lines = AnastasisDialogue::FLibrary::Get();
	const AnastasisChronicle::FVillageChronicle& Chronicle = Scratch.Host->GetChronicle();
	auto NameOf = [&Chronicle](const FString& Id) { return Chronicle.NameOf(Id); };

	AnastasisNotebook::FPlayerNotebook Notebook;
	Notebook.Reset();
	Notebook.Observe(Sim, Lines, NameOf);
	TestTrue(TEXT("sans joueur : rien de note"), Notebook.GetNotes().IsEmpty());

	// Le joueur arrive ; trois fondateurs se tiennent pres de lui.
	const FString Me = Village.ArriveAsPlayer();
	if (!TestFalse(TEXT("joueur incarne"), Me.IsEmpty())) return false;
	const AnastasisVillage::FNpc* Player = Village.FindNpc(Me);
	const TArray<AnastasisFounders::FFounder>& Founders = Scratch.Host->GetFounders();
	const AnastasisFounders::FFounder* Maria = FounderByKey(Founders, TEXT("maria"));
	const AnastasisFounders::FFounder* Theodora = FounderByKey(Founders, TEXT("theodora"));
	const AnastasisFounders::FFounder* Zoe = FounderByKey(Founders, TEXT("zoe"));
	if (!Maria || !Theodora || !Zoe) return false;
	for (const AnastasisFounders::FFounder* F : { Maria, Theodora, Zoe })
	{
		AnastasisVillage::FNpc* Npc = Village.FindNpcMutable(F->NpcId);
		Npc->X = Player->X + 1.0;
		Npc->Y = Player->Y;
	}
	Notebook.Observe(Sim, Lines, NameOf);
	TestTrue(TEXT("ce qui s'est dit avant lui n'est pas note"), Notebook.GetNotes().IsEmpty());

	// Maria raconte son mari a Theodora, qui le raconte a Zoe : deux bouches, entendues de pres.
	FString Story;
	for (const AnastasisEpisodes::FEpisode& Event : Village.FindNpc(Maria->NpcId)->Chronicle.Events)
	{
		if (Event.bFirsthand && Event.Kind == TEXT("leftBehind")) Story = Event.Id;
	}
	if (!TestFalse(TEXT("Maria se souvient de qui n'est pas venu"), Story.IsEmpty())) return false;
	// Theodora l'a deja entendu au feu : on l'en efface pour rejouer la scene pres du joueur.
	Village.FindNpcMutable(Theodora->NpcId)->Chronicle.Events.Reset();
	Village.FindNpcMutable(Zoe->NpcId)->Chronicle.Events.Reset();
	Notebook.Observe(Sim, Lines, NameOf);
	TestTrue(TEXT("Maria raconte a Theodora"), Village.TellEpisode(Maria->NpcId, Theodora->NpcId, Story));
	FString Relay;
	for (const AnastasisEpisodes::FEpisode& Event : Village.FindNpc(Theodora->NpcId)->Chronicle.Events) Relay = Event.Id;
	TestTrue(TEXT("Theodora le repete a Zoe"), Village.TellEpisode(Theodora->NpcId, Zoe->NpcId, Relay));
	Notebook.Observe(Sim, Lines, NameOf);
	TestEqual(TEXT("deux choses entendues"), Notebook.GetNotes().Num(), 2);
	const FString Text = Notebook.Render();
	AddInfo(Text);
	TestTrue(TEXT("une page pour Maria"), Text.Contains(TEXT("\nMaria\n")));
	TestTrue(TEXT("l'histoire version par version"), Text.Contains(TEXT("Une histoire entendue 2 fois")));
	TestTrue(TEXT("statut"), Notebook.StatusJson().Contains(TEXT("\"stories\":1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisMemoryThirtyDaysTest,
	"Anastasis.Memoire.TrenteJours",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisMemoryThirtyDaysTest::RunTest(const FString&)
{
	FMemoryScratch Scratch;
	if (!TestTrue(TEXT("Valmire est fondee"), Scratch.Found())) return false;
	FAnastasisSimulation& Sim = Scratch.Host->GetSimulation();
	AnastasisChronicle::FVillageChronicle Chronicle = Scratch.Host->GetChronicle();
	Chronicle.Observe(Sim);
	const double Began = FPlatformTime::Seconds();
	const double Chunk = FAnastasisSimulation::DayLength / 6.0;
	for (int32 Index = 0; Index < 30 * 6; ++Index)
	{
		AnastasisTimeWarp::Advance(Sim, Chunk);
		Chronicle.Observe(Sim);
	}
	const double Seconds = FPlatformTime::Seconds() - Began;
	using AnastasisChronicle::EKind;
	AddInfo(FString::Printf(TEXT("MEMOIRE_30J seconds=%.1f %s"), Seconds, *Chronicle.StatusJson()));
	TestTrue(TEXT("une famille decide de batir"), Chronicle.CountOf(EKind::HouseDecided) >= 1);
	TestTrue(TEXT("on demande de l'aide"), Chronicle.CountOf(EKind::HelpGiven) + Chronicle.CountOf(EKind::HelpRefused) >= 1);
	TestTrue(TEXT("des histoires circulent"), Chronicle.CountOf(EKind::Rumor) >= 1);
	const FString Path = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Chronicle"), TEXT("test-memoire-30-jours.txt")));
	TestTrue(TEXT("chronique ecrite"), FFileHelper::SaveStringToFile(Chronicle.Render(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	return true;
}

#endif
