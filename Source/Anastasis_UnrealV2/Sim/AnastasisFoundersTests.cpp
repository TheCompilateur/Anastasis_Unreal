// familles-feu-001 -- la base de repliques, le scenario des fondateurs, la fondation de Valmire (quatre familles
// et le moine, a portee du puits, le premier soir au feu dans la chronique) et le portrait qui suit le sexe et l'age.

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Sim/AnastasisDialogueLines.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Sim/AnastasisValmireFounders.h"
#include "Village/AnastasisVillage.h"
#include "Village/AnastasisVillagerLooks.h"
#include "World/AnastasisPathfinding.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr uint32 kFoundersSeed = 12345u;

	struct FFoundersScratch
	{
		UWorld* World = nullptr;
		UAnastasisSimulationSubsystem* Host = nullptr;

		FFoundersScratch()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (World)
			{
				FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.SetCurrentWorld(World);
				Host = World->GetSubsystem<UAnastasisSimulationSubsystem>();
			}
		}

		~FFoundersScratch()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}

		bool Found(uint32 Seed)
		{
			if (!Host) return false;
			Host->ResetCanonical(Seed);
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			return !Host->SeedStartVillage(12, FMath::FloorToInt32(Settlement.X), FMath::FloorToInt32(Settlement.Y)).IsEmpty();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisDialogueLibraryTest,
	"Anastasis.Familles.Repliques",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisDialogueLibraryTest::RunTest(const FString&)
{
	using AnastasisDialogue::FLibrary;
	const FLibrary& Lines = FLibrary::Get();
	AddInfo(FString::Printf(TEXT("REPLIQUES pools=%d lines=%d"), Lines.PoolCount(), Lines.LineCount()));
	TestTrue(TEXT("la base de la reference et celle de Valmire sont lues (au moins 1400 repliques)"), Lines.LineCount() >= 1400);
	for (const TCHAR* Pool : { TEXT("feu.moine.ouverture"), TEXT("feu.moine.question_ville"), TEXT("feu.moine.question_emporte"),
		TEXT("feu.moine.question_absent"), TEXT("feu.moine.benediction"), TEXT("aide.demande.chantier"), TEXT("aide.accepte"),
		TEXT("aide.refuse.dette"), TEXT("aide.refuse.son_toit"), TEXT("quotidien.faim"), TEXT("quotidien.mort"),
		TEXT("talkCatalog.BOND_LINES.friend"), TEXT("talkCatalog.REPLY_LINES.refuse") })
	{
		TestTrue(FString::Printf(TEXT("situation %s"), Pool), Lines.HasPool(Pool));
	}
	const uint32 Key = FLibrary::KeyOf(kFoundersSeed, TEXT("npc-3"), TEXT("quotidien.faim"));
	TestEqual(TEXT("meme cle, meme replique"), Lines.Pick(TEXT("quotidien.faim"), Key), Lines.Pick(TEXT("quotidien.faim"), Key));
	TestTrue(TEXT("situation inconnue : rien"), Lines.Pick(TEXT("nulle.part"), Key).IsEmpty());

	FLibrary Local;
	FString Error;
	TestTrue(TEXT("json local"), Local.AddJson(TEXT("{\"data\":{\"t\":[\"Que Dieu garde {absent}.\"]}}"), Error));
	TestEqual(TEXT("trou rempli"), Local.Pick(TEXT("t"), 7, { { TEXT("absent"), TEXT("Stephanos") } }), FString(TEXT("Que Dieu garde Stephanos.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisFoundersScenarioTest,
	"Anastasis.Familles.Scenario",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisFoundersScenarioTest::RunTest(const FString&)
{
	using namespace AnastasisFounders;
	const FScenario* Scenario = FScenario::Get();
	if (!TestNotNull(TEXT("valmire-fondateurs.json se lit"), Scenario)) return false;
	TestEqual(TEXT("quatre familles"), Scenario->Families.Num(), 4);
	TestEqual(TEXT("quatorze personnes, le moine compris"), Scenario->PeopleCount(), 14);
	TestEqual(TEXT("chacun a sa place dans l'ordre de pose"), Scenario->PoseOrder.Num(), 14);
	for (const FFamilyDef& Family : Scenario->Families)
	{
		int32 FamilyIndex = INDEX_NONE;
		TestNotNull(FString::Printf(TEXT("%s : le repondant existe"), *Family.Key), Scenario->FindMember(Family.Respondent, FamilyIndex));
		TestTrue(FString::Printf(TEXT("%s : deux phrases"), *Family.Key), FamilyIntro(Family).Contains(Family.Presentation));
	}
	// Deux parties ne racontent pas la meme chose : sur vingt graines, plusieurs combinaisons de reponses.
	TSet<FString> Tellings;
	for (uint32 Seed = 1; Seed <= 20; ++Seed)
	{
		FString Telling;
		for (const FFamilyDef& Family : Scenario->Families)
		{
			Telling += FString::Printf(TEXT("%d%d%d"), Variant(Seed, Family.Key, TEXT("ville"), Family.Where.Num()),
				Variant(Seed, Family.Key, TEXT("emporte"), Family.Carried.Num()), Variant(Seed, Family.Key, TEXT("absent"), Family.Missing.Num()));
		}
		Tellings.Add(Telling);
	}
	AddInfo(FString::Printf(TEXT("FONDATEURS recits differents sur 20 graines : %d"), Tellings.Num()));
	TestTrue(TEXT("au moins dix recits differents sur vingt graines"), Tellings.Num() >= 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisFoundingTest,
	"Anastasis.Familles.Fondation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisFoundingTest::RunTest(const FString&)
{
	using namespace AnastasisFounders;
	FFoundersScratch Scratch;
	if (!TestTrue(TEXT("Valmire est fondee"), Scratch.Found(kFoundersSeed))) return false;
	const TArray<FFounder>& Founders = Scratch.Host->GetFounders();
	const AnastasisVillage::FVillage& Village = Scratch.Host->GetSimulation().GetVillage();
	TestEqual(TEXT("quatorze fondateurs poses"), Founders.Num(), 14);
	TestEqual(TEXT("autant d'habitants que de fondateurs"), Village.GetActors().Num(), Founders.Num());
	TestEqual(TEXT("quatre foyers dans la simulation"), Village.GetFamilies().Num(), 4);

	// Chacun porte son nom, et peut atteindre le puits : personne ne meurt de soif a deux pas.
	const AnastasisVillage::FBuilding* Well = nullptr;
	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
	{
		if (Building.Type == AnastasisVillage::WellType) Well = &Building;
	}
	if (!TestNotNull(TEXT("le puits"), Well)) return false;
	const AnastasisPath::FWorldNavSource Nav(Village.GetNavGrid(), Scratch.Host->GetSimulation().GetWorld());
	for (const FFounder& Founder : Founders)
	{
		const AnastasisVillage::FNpc* Npc = Village.FindNpc(Founder.NpcId);
		if (!TestNotNull(TEXT("habitant du fondateur"), Npc)) continue;
		TestFalse(FString::Printf(TEXT("%s porte un nom"), *Founder.Key), Npc->Name.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s a un sexe"), *Founder.Key), Npc->Gender.IsEmpty());
		bool bReach = false;
		for (const AnastasisVillage::FPoint& Door : Well->AccessPoints)
		{
			TArray<AnastasisVillage::FPoint> Path;
			if (AnastasisPath::FindPath(Nav, { Npc->X, Npc->Y }, Door, {}, Path)) { bReach = true; break; }
		}
		TestTrue(FString::Printf(TEXT("%s atteint le puits"), *Founder.Key), bReach);
	}

	// La chronique s'ouvre sur les familles, puis le premier soir au feu.
	const AnastasisChronicle::FVillageChronicle& Chronicle = Scratch.Host->GetChronicle();
	const int32 SceneLines = Chronicle.CountOf(AnastasisChronicle::EKind::Scene);
	AddInfo(FString::Printf(TEXT("FONDATION scene=%d %s"), SceneLines, *Chronicle.StatusJson()));
	TestTrue(TEXT("le feu : le moine ouvre, interroge quatre familles, benit (au moins 20 repliques)"), SceneLines >= 20);
	const FString Text = Chronicle.Render();
	TestTrue(TEXT("les familles"), Text.Contains(TEXT("LES FAMILLES")));
	TestTrue(TEXT("la maison du Scribe se presente"), Text.Contains(TEXT("La maison du Scribe : Konstantinos le Scribe")));
	TestTrue(TEXT("le moine parle au feu"), Text.Contains(TEXT("Arsenios : « ")));
	TestTrue(TEXT("Konstantinos repond"), Text.Contains(TEXT("Konstantinos : « Dans la Ville")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisFounderPortraitTest,
	"Anastasis.Familles.Portraits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisFounderPortraitTest::RunTest(const FString&)
{
	using namespace AnastasisVillagerLooks;
	EAnastasisVillagerCategory Category = EAnastasisVillagerCategory::AdultMale;
	TestFalse(TEXT("anonyme : pas de categorie imposee"), CategoryFor(FString(), 0.0, Category));
	TestTrue(TEXT("Eudokia"), CategoryFor(TEXT("female"), 36.0, Category));
	TestEqual(TEXT("une femme adulte"), Category, EAnastasisVillagerCategory::AdultFemale);
	TestTrue(TEXT("Euphrosyne"), CategoryFor(TEXT("female"), 9.0, Category));
	TestEqual(TEXT("une fille"), Category, EAnastasisVillagerCategory::ChildFemale);
	TestTrue(TEXT("Arsenios"), CategoryFor(TEXT("male"), 61.0, Category));
	TestEqual(TEXT("un ancien"), Category, EAnastasisVillagerCategory::ElderMale);
	TestTrue(TEXT("Leon"), CategoryFor(TEXT("male"), 16.0, Category));
	TestEqual(TEXT("un homme"), Category, EAnastasisVillagerCategory::AdultMale);
	return true;
}

#endif
