// CHRONIQUE_VILLAGE_001 -- la chronique du village : un recit controle, la lecture seule, et trente jours du
// village du lancement sans rendu, ecrits dans Saved/Chronicle/ pour etre lus.

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Sim/AnastasisTimeWarp.h"
#include "Sim/AnastasisVillageChronicle.h"
#include "Village/AnastasisVillage.h"
#include "WorldView/AnastasisWorldView.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using AnastasisChronicle::EKind;
	using AnastasisChronicle::FVillageChronicle;

	constexpr uint32 kChronicleSeed = 12345u;

	/** Un monde de jeu jetable et son hote, detruits a la sortie. */
	struct FScratchHost
	{
		UWorld* World = nullptr;
		UAnastasisSimulationSubsystem* Host = nullptr;

		FScratchHost()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (World)
			{
				FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.SetCurrentWorld(World);
				Host = World->GetSubsystem<UAnastasisSimulationSubsystem>();
			}
		}

		~FScratchHost()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		/** Le village du lancement, pose au centre du monde comme le fait le jeu sans arpentage. */
		bool SeedStartVillage(uint32 Seed, int32 Count)
		{
			if (!Host) return false;
			Host->ResetCanonical(Seed);
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			return !Host->SeedStartVillage(Count, FMath::FloorToInt32(Settlement.X), FMath::FloorToInt32(Settlement.Y)).IsEmpty();
		}
	};

	/** Avance comme Anastasis.Sim.Advance : par tranches de quatre heures simulees, une lecture par tranche. */
	void AdvanceDays(FAnastasisSimulation& Sim, int32 Days, FVillageChronicle* Chronicle)
	{
		const double Chunk = FAnastasisSimulation::DayLength / 6.0;
		for (int32 Index = 0; Index < Days * 6; ++Index)
		{
			AnastasisTimeWarp::Advance(Sim, Chunk);
			if (Chronicle) Chronicle->Observe(Sim);
		}
	}

	bool HasLine(const FVillageChronicle& Chronicle, EKind Kind, const FString& Fragment)
	{
		for (const AnastasisChronicle::FEntry& Entry : Chronicle.GetEntries())
		{
			if (Entry.Kind == Kind && Entry.Text.Contains(Fragment)) return true;
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisChronicleStoryTest,
	"Anastasis.Chronique.Recit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisChronicleStoryTest::RunTest(const FString&)
{
	using namespace AnastasisVillage;
	FAnastasisSimulation Sim;
	Sim.Reset(kChronicleSeed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
	FVillage& Village = Sim.GetVillage();

	FVillageChronicle Chronicle;
	Chronicle.Reset(kChronicleSeed);
	Chronicle.Observe(Sim);
	TestFalse(TEXT("un village vide n'ouvre pas de chronique"), Chronicle.HasStarted());

	// Une case libre pres du centre : le puits, puis deux habitants a cote.
	int32 X = AnastasisWorldView::ReferenceWidth / 2;
	int32 Y = AnastasisWorldView::ReferenceHeight / 2;
	for (int32 Step = 0; Step < 40 && Village.IsFootBlocked(X + 0.5, Y + 0.5); ++Step) { ++X; }
	const FString WellId = Village.AddBuilding(WellType, X, Y, 1.0, Sim.GetDay());
	TestFalse(TEXT("puits pose"), WellId.IsEmpty());
	const AnastasisNeeds::FNeeds Needs;
	const FString First = Village.SpawnNpc(X + 2.5, Y + 0.5, Needs);
	const FString Second = Village.SpawnNpc(X + 0.5, Y + 2.5, Needs);

	Chronicle.Observe(Sim);
	TestTrue(TEXT("la chronique s'ouvre avec ses premiers habitants"), Chronicle.HasStarted());
	TestTrue(TEXT("la fondation est racontee"), HasLine(Chronicle, EKind::Founding, TEXT("Le village s'éveille. Ils sont 2")));
	const FString FirstName = Chronicle.NameOf(First);
	const FString SecondName = Chronicle.NameOf(Second);
	TestNotEqual(TEXT("un nom, pas un identifiant"), FirstName, First);
	TestNotEqual(TEXT("deux noms differents"), FirstName, SecondName);
	TestTrue(TEXT("la fondation nomme le puits"), HasLine(Chronicle, EKind::Founding, TEXT("le puits")));

	// Une maison achevee, donnee au premier : il s'y installe.
	const FString HouseId = Village.AddBuilding(HouseType, X + 4, Y, 1.0, Sim.GetDay());
	TestTrue(TEXT("maison attribuee"), Village.AssignHome(First, HouseId));
	Chronicle.Observe(Sim);
	TestTrue(TEXT("on pose la premiere maison"), HasLine(Chronicle, EKind::BuildingPlaced, TEXT("la première maison")));
	TestTrue(TEXT("il s'installe, nomme"), HasLine(Chronicle, EKind::Home, FirstName + TEXT(" s'installe dans la première maison")));

	// Un chantier qui s'ouvre.
	const FString SiteId = Village.OpenSite(HouseType, X - 4, Y, true);
	Chronicle.Observe(Sim);
	if (!SiteId.IsEmpty())
	{
		TestTrue(TEXT("chantier ouvert"), HasLine(Chronicle, EKind::SiteOpened, TEXT("deuxième maison")));
	}

	// Un arrivant, un depart.
	const FString Third = Village.SpawnNpc(X + 1.5, Y + 1.5, Needs);
	Chronicle.Observe(Sim);
	TestTrue(TEXT("arrivee racontee"), HasLine(Chronicle, EKind::Arrival, Chronicle.NameOf(Third) + TEXT(" arrive au village")));
	TestTrue(TEXT("depart"), Village.RemoveNpc(Second));
	Chronicle.Observe(Sim);
	TestTrue(TEXT("depart raconte"), HasLine(Chronicle, EKind::Departure, SecondName + TEXT(" a quitté le village")));

	// Une mort de soif, avec sa cause.
	if (FNpc* Dying = Village.FindNpcMutable(Third))
	{
		Dying->Needs.Health = 0.0;
		Dying->Needs.Thirst = 100.0;
	}
	TestEqual(TEXT("un mort"), Village.UpdateMortalityDaily(), 1);
	Chronicle.Observe(Sim);
	TestTrue(TEXT("mort racontee avec sa cause"), HasLine(Chronicle, EKind::Death, TEXT("de soif")));
	TestFalse(TEXT("un mort n'est pas un depart"), HasLine(Chronicle, EKind::Departure, Chronicle.NameOf(Third)));

	// Le recit de chacun reprend ses propres lignes.
	const FString Text = Chronicle.Render();
	TestTrue(TEXT("liste des habitants"), Text.Contains(TEXT("LES HABITANTS")));
	TestTrue(TEXT("recit par personne"), Text.Contains(TEXT("CE QU'A VÉCU CHACUN")));
	TestTrue(TEXT("statut JSON"), Chronicle.StatusJson().Contains(TEXT("\"Death\":1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisChronicleReadOnlyTest,
	"Anastasis.Chronique.LectureSeule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisChronicleReadOnlyTest::RunTest(const FString&)
{
	// Deux villages identiques ; l'un est raconte, l'autre non. Apres cinq jours, meme etat au bit pres.
	FScratchHost Told;
	FScratchHost Untold;
	if (!TestTrue(TEXT("village raconte pose"), Told.SeedStartVillage(kChronicleSeed, 12))
		|| !TestTrue(TEXT("village temoin pose"), Untold.SeedStartVillage(kChronicleSeed, 12)))
	{
		return false;
	}
	FAnastasisSimulation& A = Told.Host->GetSimulation();
	FAnastasisSimulation& B = Untold.Host->GetSimulation();
	TestEqual(TEXT("memes villages au depart"), A.StateDigest(), B.StateDigest());

	FVillageChronicle Chronicle;
	Chronicle.Reset(kChronicleSeed);
	Chronicle.Observe(A);
	AdvanceDays(A, 5, &Chronicle);
	AdvanceDays(B, 5, nullptr);

	TestTrue(TEXT("la chronique a lu le village"), Chronicle.HasStarted() && Chronicle.GetEntries().Num() > 0);
	TestEqual(TEXT("cinq jours clos"), Chronicle.GetDays().Num(), 5);
	TestEqual(TEXT("la chronique n'a rien ecrit dans la simulation (StateDigest)"), A.StateDigest(), B.StateDigest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisChronicleThirtyDaysTest,
	"Anastasis.Chronique.TrenteJours",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisChronicleThirtyDaysTest::RunTest(const FString&)
{
	FScratchHost Scratch;
	if (!TestTrue(TEXT("village du lancement pose"), Scratch.SeedStartVillage(kChronicleSeed, 12)))
	{
		return false;
	}
	FAnastasisSimulation& Sim = Scratch.Host->GetSimulation();
	// La chronique de l'hote, ouverte sur la fondation (familles et premier soir au feu quand
	// anastasis.Village.Founders vaut 1), puis tenue ici a chaque tranche du saut.
	FVillageChronicle Chronicle = Scratch.Host->GetChronicle();
	Chronicle.Observe(Sim);
	const double Began = FPlatformTime::Seconds();
	AdvanceDays(Sim, 30, &Chronicle);
	const double Seconds = FPlatformTime::Seconds() - Began;

	TestEqual(TEXT("trente jours clos"), Chronicle.GetDays().Num(), 30);
	TestTrue(TEXT("la fondation est racontee"), Chronicle.CountOf(EKind::Founding) > 0);
	const FString Text = Chronicle.Render();
	TestTrue(TEXT("le jour 30 est raconte"), Text.Contains(TEXT("\nJOUR 30\n")) || Text.Contains(TEXT(" À 30\n")));
	TestTrue(TEXT("chaque jour a son bilan"), Text.Contains(TEXT("Bilan du soir")));
	for (const AnastasisVillage::FNpc& Npc : Sim.GetVillage().GetActors())
	{
		TestNotEqual(TEXT("chaque habitant porte un nom"), Chronicle.NameOf(Npc.Id), Npc.Id);
	}

	const FString Path = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Chronicle"), TEXT("test-chronique-30-jours.txt")));
	TestTrue(TEXT("chronique ecrite"), FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	AddInfo(FString::Printf(TEXT("CHRONIQUE_30J path=%s seconds=%.1f %s"), *Path, Seconds, *Chronicle.StatusJson()));
	return true;
}

#endif
