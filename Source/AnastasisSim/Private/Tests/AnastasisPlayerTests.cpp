#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// player-minimal-001 -- le joueur est un habitant (reference : docs/PLAYER_AS_HABITANT.md).
//
//   SYM-2  sans joueur incarne, la simulation est celle d'avant, au bit pres
//   SYM-4  release() rend la main a Nous, l'habitant continue de vivre
//   idle   sans commande, l'habitant incarne attend ; Nous ne choisit pas pour lui
//   marche la direction humaine conduit le meme corps, memes collisions
//   EXTENSION (TIME_WARP_001) : presence, oisivete, reputation

namespace AnastasisPlayerTest
{
	using namespace AnastasisVillage;

	constexpr double Dt = 1.0 / 60.0;

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

	AnastasisNeeds::FNeeds Thirsty(double Thirst)
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 10.0;
		N.Energy = 80.0;
		N.Thirst = Thirst;
		return N;
	}

	/** Un puits et six habitants a soifs echelonnees : un village qui bouge. */
	void Populate(FVillage& Village)
	{
		Village.AddBuilding(WellType, 20, 20);
		const double Ring[6][2] = { { 12.5, 20.5 }, { 28.5, 20.5 }, { 20.5, 12.5 }, { 20.5, 28.5 }, { 14.5, 14.5 }, { 26.5, 26.5 } };
		for (int32 K = 0; K < 6; ++K)
		{
			Village.SpawnNpc(Ring[K][0], Ring[K][1], Thirsty(90.0 - 8.0 * K));
		}
	}

	void Run(FVillage& Village, double& Time, double Seconds)
	{
		const int32 Ticks = FMath::CeilToInt32(Seconds / Dt);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Time += Dt;
			Village.UpdateActors(Time, Dt);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisPlayerObserverTest,
	"Anastasis.Sim.Joueur.Observateur",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisPlayerObserverTest::RunTest(const FString&)
{
	using namespace AnastasisPlayerTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 40);

	// SYM-2 : sans joueur, chaque entree joueur est sans effet et minuit laisse tout le monde a 50.
	FVillage Plain;
	Plain.Bind(World);
	Populate(Plain);
	FVillage Touched;
	Touched.Bind(World);
	Populate(Touched);
	double T1 = 0.0;
	double T2 = 0.0;
	for (int32 Round = 0; Round < 6; ++Round)
	{
		Run(Plain, T1, 10.0);
		TestFalse(TEXT("no player: movement input refused"), Touched.SetPlayerMovementInput(1.0, 0.0));
		Touched.ObservePlayer(0.0, 1000.0);
		Touched.UpdateReputationDaily();
		Run(Touched, T2, 10.0);
	}
	TestTrue(TEXT("observer mode: same digest as a village with no player API touched"), Plain.Digest() == Touched.Digest());
	TestTrue(TEXT("observer mode: no player"), Touched.GetPlayerPersonId().IsEmpty() && Touched.PlayerActor() == nullptr);
	for (const FNpc& Npc : Touched.GetActors())
	{
		TestEqual(TEXT("never idle: reputation stays exactly 50"), Npc.Reputation, Standing::Base);
		TestEqual(TEXT("presence untouched"), Npc.Presence, 1.0);
		TestEqual(TEXT("idle time untouched"), Npc.IdleSeconds, 0.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisPlayerIncarnateTest,
	"Anastasis.Sim.Joueur.Incarnation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisPlayerIncarnateTest::RunTest(const FString&)
{
	using namespace AnastasisPlayerTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 40);
	FVillage Village;
	Village.Bind(World);
	Populate(Village);

	TestFalse(TEXT("cannot incarnate an unknown person"), Village.Incarnate(TEXT("npc-99")));

	// arriveAsPlayer : un habitant ordinaire, settlement + (2, 3), incarne.
	const FString Id = Village.ArriveAsPlayer();
	TestEqual(TEXT("arrives as the next ordinary inhabitant"), Id, FString(TEXT("npc-6")));
	TestEqual(TEXT("incarnated"), Village.GetPlayerPersonId(), Id);
	const FNpc* Me = Village.FindNpc(Id);
	if (!TestNotNull(TEXT("player inhabitant"), Me)) return false;
	TestEqual(TEXT("settlement + 2"), Me->X, Village.GetSettlement().X + 2.0);
	TestEqual(TEXT("settlement + 3"), Me->Y, Village.GetSettlement().Y + 3.0);
	const double X0 = Me->X;
	const double Y0 = Me->Y;
	const double Thirst0 = Me->Needs.Thirst;

	// Sans commande : il attend, ne bouge pas, et son corps vit (la soif monte).
	double Time = 0.0;
	Run(Village, Time, 20.0);
	Me = Village.FindNpc(Id);
	TestEqual(TEXT("idle goal"), Me->Goal, FString(GoalIdle));
	TestEqual(TEXT("waits"), Me->Activity, FString(TEXT("attend")));
	TestEqual(TEXT("does not move by itself (x)"), Me->X, X0);
	TestEqual(TEXT("does not move by itself (y)"), Me->Y, Y0);
	TestTrue(TEXT("his body lives: thirst rises"), Me->Needs.Thirst > Thirst0);
	int32 Drank = 0;
	for (const FNpc& Npc : Village.GetActors()) Drank += Npc.DrinksTaken;
	TestTrue(TEXT("the others keep living"), Drank > 0);

	// SYM-4 : rendu a Nous, il redevient un habitant comme un autre.
	TestEqual(TEXT("release returns the id"), Village.Release(), Id);
	TestTrue(TEXT("observer again"), Village.PlayerActor() == nullptr);
	Run(Village, Time, 5.0);
	TestNotEqual(TEXT("Nous decides again: no longer idle"), Village.FindNpc(Id)->Goal, FString(GoalIdle));

	// On ne reste pas maitre d'un absent.
	TestTrue(TEXT("re-incarnate"), Village.Incarnate(Id));
	Village.RemoveNpc(Id);
	TestTrue(TEXT("removing the player releases"), Village.GetPlayerPersonId().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisPlayerDriveTest,
	"Anastasis.Sim.Joueur.Marche",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisPlayerDriveTest::RunTest(const FString&)
{
	using namespace AnastasisPlayerTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 40);
	// Un mur d'eau a x = 26 : la marche directe glisse, elle ne traverse pas.
	for (int32 Y = 0; Y < 40; ++Y) World.Tiles[Y * 40 + 26].Type = AnastasisWorld::ETileType::Water;
	FVillage Village;
	Village.Bind(World);
	const FString Id = Village.ArriveAsPlayer(20.5, 20.5);
	if (!TestFalse(TEXT("arrived"), Id.IsEmpty())) return false;

	TestTrue(TEXT("east"), Village.SetPlayerMovementInput(3.0, 0.0));
	TestEqual(TEXT("drive normalised"), Village.GetPlayerDrive().X, 1.0);
	double Time = 0.0;
	Run(Village, Time, 1.0);
	const FNpc* Me = Village.FindNpc(Id);
	TestEqual(TEXT("one second east = speed 4 tiles"), Me->X, 24.5, 0.1);
	TestEqual(TEXT("straight line"), Me->Y, 20.5);
	TestEqual(TEXT("walking"), Me->Activity, FString(TEXT("marche")));

	Run(Village, Time, 3.0);
	Me = Village.FindNpc(Id);
	TestTrue(TEXT("stopped by the water"), Me->X < 26.0);
	TestFalse(TEXT("never on a blocked tile"), Village.IsFootBlocked(Me->X, Me->Y));

	TestFalse(TEXT("stop"), Village.SetPlayerMovementInput(0.0, 0.0));
	const double XStop = Me->X;
	Run(Village, Time, 1.0);
	Me = Village.FindNpc(Id);
	TestEqual(TEXT("stays where he stopped"), Me->X, XStop);
	TestEqual(TEXT("idle again"), Me->Activity, FString(TEXT("attend")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisPlayerPresenceTest,
	"Anastasis.Sim.Joueur.Presence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisPlayerPresenceTest::RunTest(const FString&)
{
	using namespace AnastasisPlayerTest;
	FNpc Seen;
	TestTrue(TEXT("presence 1: within range"), FVillage::Sees(Seen, 5.2, 5.2));
	TestFalse(TEXT("presence 1: beyond range"), FVillage::Sees(Seen, 5.21, 5.2));

	FNpc Fading;
	Fading.Presence = 0.5;
	TestTrue(TEXT("presence 0.5: seen up to half the range"), FVillage::Sees(Fading, 2.6, 5.2));
	TestFalse(TEXT("presence 0.5: not beyond"), FVillage::Sees(Fading, 3.0, 5.2));

	FNpc Ghost;
	Ghost.Presence = 0.04;
	TestFalse(TEXT("presence 0.04: invisible even next to you"), FVillage::Sees(Ghost, 0.0, 18.0));

	// Le village enregistre ce que l'hote a vu : seulement pour le joueur.
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 40);
	FVillage Village;
	Village.Bind(World);
	Populate(Village);
	const FString Id = Village.ArriveAsPlayer();
	Village.ObservePlayer(0.03, 7.0 * AnastasisRhythm::DayLength);
	TestEqual(TEXT("player presence recorded"), Village.FindNpc(Id)->Presence, 0.03);
	TestEqual(TEXT("player idle week recorded"), Village.FindNpc(Id)->IdleSeconds, 7.0 * AnastasisRhythm::DayLength);
	TestEqual(TEXT("others untouched"), Village.FindNpc(TEXT("npc-0"))->Presence, 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisPlayerReputationTest,
	"Anastasis.Sim.Joueur.Reputation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisPlayerReputationTest::RunTest(const FString&)
{
	using namespace AnastasisPlayerTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 40);
	FVillage Village;
	Village.Bind(World);
	Populate(Village);
	const FString Id = Village.ArriveAsPlayer();

	// Une semaine sautee : cible 50 - 7 x 4 = 22 ; un minuit en rattrape 40 %.
	Village.ObservePlayer(0.03, 7.0 * AnastasisRhythm::DayLength);
	Village.UpdateReputationDaily();
	TestEqual(TEXT("first midnight: 50 + (22 - 50) x 0.4"), Village.FindNpc(Id)->Reputation, 50.0 - 28.0 * 0.4, 1e-9);
	for (int32 Night = 0; Night < 30; ++Night) Village.UpdateReputationDaily();
	TestEqual(TEXT("a month later: settles on 22"), Village.FindNpc(Id)->Reputation, 22.0, 1e-4);
	TestEqual(TEXT("the others are not judged"), Village.FindNpc(TEXT("npc-0"))->Reputation, Standing::Base);

	// Ce que le village en fait : moins envie de lui parler.
	TestEqual(TEXT("affinity at the base: 0"), FVillage::ReputationAffinity(*Village.FindNpc(TEXT("npc-0"))), 0.0);
	TestTrue(TEXT("affinity of the idler: negative"), FVillage::ReputationAffinity(*Village.FindNpc(Id)) < -10.0);

	// Un mois oisif de plus : la cible touche le plancher, pas en dessous.
	Village.ObservePlayer(0.0, 30.0 * AnastasisRhythm::DayLength);
	for (int32 Night = 0; Night < 60; ++Night) Village.UpdateReputationDaily();
	TestEqual(TEXT("floor 0"), Village.FindNpc(Id)->Reputation, 0.0, 1e-6);
	return true;
}

#endif
