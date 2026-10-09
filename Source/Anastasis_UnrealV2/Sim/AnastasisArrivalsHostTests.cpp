// arrivants-001 (ecart n°53) -- soixante jours de Valmire ouverte au monde : des groupes arrivent par la route, le
// conseil du soir les accueille ou les renvoie, les accueillis batissent, et leurs histoires courent.

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Life/AnastasisEpisodes.h"
#include "Sim/AnastasisArrivals.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Sim/AnastasisTimeWarp.h"
#include "Sim/AnastasisVillageChronicle.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr uint32 kArrivalsSeed = 12345u;

	struct FArrivalsScratch
	{
		UWorld* World = nullptr;
		UAnastasisSimulationSubsystem* Host = nullptr;

		FArrivalsScratch()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (World)
			{
				FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.SetCurrentWorld(World);
				Host = World->GetSubsystem<UAnastasisSimulationSubsystem>();
			}
		}

		~FArrivalsScratch()
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
			Host->ResetCanonical(kArrivalsSeed);
			const AnastasisVillage::FPoint Settlement = Host->GetSimulation().GetVillage().GetSettlement();
			return !Host->SeedStartVillage(12, FMath::FloorToInt32(Settlement.X), FMath::FloorToInt32(Settlement.Y)).IsEmpty();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisArrivalPoolTest,
	"Anastasis.Arrivants.Groupes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisArrivalPoolTest::RunTest(const FString&)
{
	const TArray<AnastasisVillage::FVillage::FArrivalGroup>& Pool = AnastasisArrivals::DefaultPool();
	if (!TestTrue(TEXT("des groupes"), Pool.Num() >= 4)) return false;
	TSet<FString> Names;
	for (const AnastasisVillage::FVillage::FArrivalGroup& Group : Pool)
	{
		TestFalse(TEXT("un nom de foyer"), Group.FamilyName.IsEmpty());
		TestTrue(TEXT("six personnes au plus par groupe, prevues"), Group.Members.Num() >= 6);
		if (Group.Members.Num() > 0) TestTrue(TEXT("le premier est un adulte"), Group.Members[0].bAdult);
		for (const AnastasisVillage::FVillage::FArrivalMember& Member : Group.Members)
		{
			TestFalse(FString::Printf(TEXT("%s : un seul de ce nom"), *Member.Name), Names.Contains(Member.Name));
			Names.Add(Member.Name);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisArrivalsSixtyDaysTest,
	"Anastasis.Arrivants.SoixanteJours",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisArrivalsSixtyDaysTest::RunTest(const FString&)
{
	FArrivalsScratch Scratch;
	if (!TestTrue(TEXT("Valmire est fondee"), Scratch.Found())) return false;
	FAnastasisSimulation& Sim = Scratch.Host->GetSimulation();
	TestTrue(TEXT("le monde exterieur est charge avec Valmire"), Sim.GetGeo().IsLoaded());
	AnastasisChronicle::FVillageChronicle Chronicle = Scratch.Host->GetChronicle();
	Chronicle.Observe(Sim);
	const double Began = FPlatformTime::Seconds();
	const double Chunk = FAnastasisSimulation::DayLength / 6.0;
	for (int32 Index = 0; Index < 60 * 6; ++Index)
	{
		AnastasisTimeWarp::Advance(Sim, Chunk);
		Chronicle.Observe(Sim);
	}
	const double Seconds = FPlatformTime::Seconds() - Began;
	const AnastasisVillage::FVillage& Village = Sim.GetVillage();
	using AnastasisChronicle::EKind;
	for (const AnastasisVillage::FVillage::FCouncil& Council : Village.GetCouncilLog())
	{
		FString Votes;
		for (const AnastasisVillage::FVillage::FWelcomeVote& Vote : Council.Votes)
		{
			Votes += FString::Printf(TEXT(" %s:%s/%s/%.0f[%s]"), *Vote.VoterId, Vote.bYes ? TEXT("oui") : TEXT("non"), *Vote.Reason, Vote.Score, *Vote.Terms);
		}
		AddInfo(FString::Printf(TEXT("ARRIVANTS_CONSEIL jour=%d famille=%s %s cause=%s voix=%s"), Council.Day, *Council.FamilyId,
			Council.bAccepted ? TEXT("ACCUEILLI") : TEXT("REFUSE"), *Council.Cause, *Votes));
	}
	// Jusqu'ou les histoires vont : combien de souvenirs a 0, 1, 2, 3 bouches et plus, et leur poids.
	{
		int32 ByHops[5] = {};
		double MaxWeightAtHop2 = 0.0;
		for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
		{
			for (const AnastasisEpisodes::FEpisode& Event : Npc.Chronicle.Events)
			{
				++ByHops[FMath::Clamp(Event.Hops, 0, 4)];
				if (Event.Hops == 2) MaxWeightAtHop2 = FMath::Max(MaxWeightAtHop2, Event.Weight);
			}
		}
		AddInfo(FString::Printf(TEXT("ARRIVANTS_BOUCHES 0=%d 1=%d 2=%d 3=%d 4+=%d poids_max_a_2=%.1f"), ByHops[0], ByHops[1], ByHops[2], ByHops[3], ByHops[4], MaxWeightAtHop2));
	}
	// Une maison levee par des arrivants : une maison achevee qui revient a un groupe accueilli.
	int32 ArrivalHouses = 0;
	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
	{
		const AnastasisVillage::FVillage::FFamily* Owner = Building.OwnerFamilyId.IsEmpty() ? nullptr : Village.FindFamily(Building.OwnerFamilyId);
		if (Owner && Owner->ArrivedDay > 0 && Building.Progress >= 1.0) ++ArrivalHouses;
	}
	AddInfo(FString::Printf(TEXT("ARRIVANTS_60J seconds=%.1f groups=%d councils=%d welcomed=%d refused=%d houses=%d legends=%d people=%d %s"),
		Seconds, Chronicle.CountOf(EKind::GroupArrival), Village.GetCouncilLog().Num(), Chronicle.CountOf(EKind::Welcomed),
		Chronicle.CountOf(EKind::TurnedAway), ArrivalHouses, Chronicle.CountOf(EKind::Legend), Village.GetActors().Num(), *Chronicle.StatusJson()));
	const FString Path = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Chronicle"), TEXT("test-arrivants-60-jours.txt")));
	TestTrue(TEXT("chronique ecrite"), FFileHelper::SaveStringToFile(Chronicle.Render(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	// La mecanique, ici, dans un monde de test plus pauvre que la carte du jeu (le grenier s'y vide) : des groupes
	// arrivent, chacun passe au conseil, chaque voix a sa raison et son detail, la chronique dit le verdict. Le
	// « fini quand » d'Alexandre (accueil ET refus, maison des arrivants, legende) se juge dans le jeu : memory-pie
	// et arrivants-pie (ARRIVANTS_001.md).
	TestTrue(TEXT("des groupes arrivent par la route"), Chronicle.CountOf(EKind::GroupArrival) >= 2);
	TestEqual(TEXT("chaque groupe passe au conseil"), Village.GetCouncilLog().Num(), Chronicle.CountOf(EKind::GroupArrival));
	TestEqual(TEXT("chaque conseil a son verdict"), Chronicle.CountOf(EKind::Welcomed) + Chronicle.CountOf(EKind::TurnedAway), Village.GetCouncilLog().Num());
	for (const AnastasisVillage::FVillage::FCouncil& Council : Village.GetCouncilLog())
	{
		TestTrue(TEXT("les chefs de famille votent"), Council.Votes.Num() >= 4);
		for (const AnastasisVillage::FVillage::FWelcomeVote& Vote : Council.Votes)
		{
			TestFalse(TEXT("une raison"), Vote.Reason.IsEmpty());
			TestFalse(TEXT("son detail"), Vote.Terms.IsEmpty());
		}
	}
	return true;
}

#endif
