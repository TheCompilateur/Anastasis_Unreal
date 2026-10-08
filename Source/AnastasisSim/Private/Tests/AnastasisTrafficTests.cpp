#include "Misc/AutomationTest.h"

#include "Core/AnastasisJsNumeric.h"
#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisPathfinding.h"
#include "World/AnastasisTraffic.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// settlement-morphogenesis-001 : le passage fait le chemin (`sim.traffic`, ecart n°42).
//
//   marche -> 1 passage / 0,85 s sur la case -> decroissance de minuit -> au-dela de 14,
//   effort de defrichage -> apres 18 nuits, Road (cout 0,86) -> l'A* passe par la -> plus de passage.
//
// Intervention : un marcheur que l'on pilote (le corps du joueur) fait le va-et-vient entre deux
// points, jour apres jour. Falsificateurs : un sentier hors de sa route, un sentier sans passage,
// un sentier sans activation, un passage sans marche, une trace qui survit a l'abandon.

namespace AnastasisTrafficTest
{
	using namespace AnastasisVillage;

	constexpr double Dt = 1.0 / 60.0;
	constexpr double DayLength = 90.0;
	constexpr int32 Row = 10;
	constexpr double FromX = 5.5;
	constexpr double ToX = 30.5;

	AnastasisWorld::FWorld MakeMeadow(int32 W, int32 H)
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

	/** Le va-et-vient : le corps incarne marche vers ToX, puis revient vers FromX, sur la rangee Row. */
	struct FWalker
	{
		double Direction = 1.0;

		void Steer(FVillage& Village)
		{
			const FNpc* Body = Village.PlayerActor();
			if (!Body) return;
			if (Direction > 0.0 && Body->X >= ToX) Direction = -1.0;
			if (Direction < 0.0 && Body->X <= FromX) Direction = 1.0;
			// Rappel doux sur la rangee : la trace doit rester une ligne, pas une bande.
			const double Pull = FMath::Clamp((Row + 0.5 - Body->Y) * 2.0, -0.4, 0.4);
			Village.SetPlayerMovementInput(Direction, Pull);
		}
	};

	/** Un jour simule : marche (ou repos), puis la nuit telle que l'ordonne la simulation (decroissance, sentiers). */
	void RunDays(FVillage& Village, double& Time, int32& Day, int32 Days, FWalker* Walker)
	{
		for (int32 D = 0; D < Days; ++D)
		{
			const int32 Ticks = FMath::RoundToInt32(DayLength / Dt);
			for (int32 I = 0; I < Ticks; ++I)
			{
				if (Walker) Walker->Steer(Village); else Village.SetPlayerMovementInput(0.0, 0.0);
				Time += Dt;
				Village.UpdateActors(Time, Dt);
			}
			++Day;
			Village.DecayTrafficDaily();
			Village.UpdateRoadEvolutionDaily(Day);
		}
	}

	FString Arrive(FVillage& Village)
	{
		return Village.ArriveAsPlayer(FromX, Row + 0.5);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisTrafficPassageTest,
	"Anastasis.Sim.Village.Sentiers.Passage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisTrafficPassageTest::RunTest(const FString&)
{
	using namespace AnastasisTrafficTest;
	AnastasisWorld::FWorld World = MakeMeadow(40, 20);
	FVillage Village;
	Village.Bind(World);
	TestFalse(TEXT("le village nu ne pose pas de sentier (parite)"), Village.IsRoadEvolutionEnabled());
	if (!TestFalse(TEXT("un corps a piloter"), Arrive(Village).IsEmpty())) return false;
	double Time = 30.0;
	int32 Day = 1;

	// Au repos : aucun passage (falsificateur : un passage sans marche).
	RunDays(Village, Time, Day, 1, nullptr);
	TestEqual(TEXT("immobile : aucun passage"), Village.GetPassageCount(), static_cast<int64>(0));

	// Une journee de va-et-vient.
	FWalker Walker;
	const int32 Ticks = FMath::RoundToInt32(DayLength / Dt);
	for (int32 I = 0; I < Ticks; ++I)
	{
		Walker.Steer(Village);
		Time += Dt;
		Village.UpdateActors(Time, Dt);
	}
	const int64 Passages = Village.GetPassageCount();
	TestTrue(FString::Printf(TEXT("la marche laisse des passages (%lld)"), Passages), Passages > 50);
	// Un passage toutes les 0,85 s de marche, a 1/60 s pres : 90 s -> 105 passages.
	TestTrue(TEXT("au plus un passage par 0,85 s"), Passages <= FMath::CeilToInt(DayLength / AnastasisTraffic::PassageInterval) + 1);

	double OnRoute = 0.0;
	double OffRoute = 0.0;
	for (int32 Y = 0; Y < World.H; ++Y)
	{
		for (int32 X = 0; X < World.W; ++X)
		{
			const double T = Village.TrafficAt(X, Y);
			const bool bRoute = Y == Row && X >= 5 && X <= 30;
			(bRoute ? OnRoute : OffRoute) += T;
		}
	}
	AddInfo(FString::Printf(TEXT("passages=%lld trafic sur la route=%.1f hors route=%.1f"), Passages, OnRoute, OffRoute));
	TestTrue(TEXT("le passage est sur la route"), OnRoute > 50.0);
	TestEqual(TEXT("aucun passage hors de la route parcourue"), OffRoute, 0.0);
	TestTrue(TEXT("plafond 180 respecte"), Village.TrafficAt(15, Row) <= 180.0);

	// La nuit : la reference decroit chaque case foulee (x0,88 - 0,55).
	const double Before = Village.TrafficAt(15, Row);
	Village.DecayTrafficDaily();
	const double After = Village.TrafficAt(15, Row);
	const double Expected = Before * AnastasisTraffic::FootMul - AnastasisTraffic::FootSub;
	TestTrue(FString::Printf(TEXT("decroissance de minuit %.3f -> %.3f"), Before, After),
		After == static_cast<double>(AnastasisJs::StoreF32(Expected < AnastasisTraffic::FootEpsilon ? 0.0 : Expected)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisTrafficDesirePathTest,
	"Anastasis.Sim.Village.Sentiers.Naissance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisTrafficDesirePathTest::RunTest(const FString&)
{
	using namespace AnastasisTrafficTest;
	// Deux villages identiques, le meme marcheur ; seul l'interrupteur de l'ecart n°42 differe.
	auto Grow = [&](bool bEnabled, int32 Days, int32& OutRoads, int32& OutOffRoute, int32& OutFirstDay, double& OutCost) -> bool
	{
		AnastasisWorld::FWorld World = MakeMeadow(40, 20);
		FVillage Village;
		Village.Bind(World);
		Village.SetRoadEvolutionEnabled(bEnabled);
		if (Arrive(Village).IsEmpty()) return false;
		double Time = 30.0;
		int32 Day = 1;
		FWalker Walker;
		OutFirstDay = -1;
		for (int32 D = 0; D < Days; ++D)
		{
			RunDays(Village, Time, Day, 1, &Walker);
			if (OutFirstDay < 0 && Village.GetRoads().Num() > 0) OutFirstDay = Day;
		}
		OutRoads = Village.GetRoads().Num();
		OutOffRoute = 0;
		OutCost = -1.0;
		for (const TPair<int32, AnastasisTraffic::FRoadTile>& Road : Village.GetRoads())
		{
			const int32 X = Road.Key % World.W;
			const int32 Y = Road.Key / World.W;
			if (Y != Row || X < 5 || X > 30) ++OutOffRoute;
			OutCost = AnastasisNav::MoveCostAt(Village.GetNavGrid(), X, Y);
			if (Village.LiveTileAt(X, Y).Type != AnastasisWorld::ETileType::Road) return false;
		}
		return true;
	};
	int32 Roads = 0, Off = 0, First = 0;
	double Cost = 0.0;
	TestTrue(TEXT("trente jours de va-et-vient"), Grow(true, 30, Roads, Off, First, Cost));
	AddInfo(FString::Printf(TEXT("active : %d sentiers, premier au jour %d, hors route %d, cout %.3f"), Roads, First, Off, Cost));
	TestTrue(TEXT("le passage fixe des sentiers"), Roads >= 10);
	TestEqual(TEXT("aucun sentier hors du va-et-vient"), Off, 0);
	TestTrue(TEXT("pas avant 18 nuits d'effort"), First >= 1 + AnastasisTraffic::PathDays);
	TestEqual(TEXT("cout de marche du profil path"), Cost, static_cast<double>(AnastasisJs::StoreF32(0.86)));

	int32 RoadsOff = 0, OffOff = 0, FirstOff = 0;
	double CostOff = 0.0;
	TestTrue(TEXT("meme marche, extension coupee"), Grow(false, 30, RoadsOff, OffOff, FirstOff, CostOff));
	TestEqual(TEXT("sans activation : aucun sentier (parite)"), RoadsOff, 0);

	// Sans marche, aucun sentier (falsificateur : un sentier sans passage).
	{
		AnastasisWorld::FWorld World = MakeMeadow(40, 20);
		FVillage Village;
		Village.Bind(World);
		Village.SetRoadEvolutionEnabled(true);
		Arrive(Village);
		double Time = 30.0;
		int32 Day = 1;
		RunDays(Village, Time, Day, 30, nullptr);
		TestEqual(TEXT("immobile trente jours : aucun sentier"), Village.GetRoads().Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisTrafficAbandonTest,
	"Anastasis.Sim.Village.Sentiers.Abandon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisTrafficAbandonTest::RunTest(const FString&)
{
	using namespace AnastasisTrafficTest;
	AnastasisWorld::FWorld World = MakeMeadow(40, 20);
	FVillage Village;
	Village.Bind(World);
	Village.SetRoadEvolutionEnabled(true);
	Arrive(Village);
	double Time = 30.0;
	int32 Day = 1;
	FWalker Walker;
	// Dix jours : le passage est chaud, l'effort a commence, aucun sentier n'est encore fixe.
	RunDays(Village, Time, Day, 10, &Walker);
	const double Hot = Village.TrafficAt(15, Row);
	const int32 Efforts = Village.GetRoadEfforts().Num();
	TestTrue(FString::Printf(TEXT("passage chaud (%.1f) et effort en cours (%d cases)"), Hot, Efforts), Hot > AnastasisTraffic::DesireTraffic && Efforts > 0);
	TestEqual(TEXT("pas encore de sentier"), Village.GetRoads().Num(), 0);
	// Puis plus personne : la reference efface le passage et l'effort, le sentier ne nait jamais.
	RunDays(Village, Time, Day, 30, nullptr);
	double Left = 0.0;
	for (float T : Village.GetTraffic()) Left += T;
	AddInfo(FString::Printf(TEXT("apres trente jours d'abandon : passage %.3f, efforts %d, sentiers %d"), Left, Village.GetRoadEfforts().Num(), Village.GetRoads().Num()));
	TestEqual(TEXT("le passage s'efface"), Left, 0.0);
	TestEqual(TEXT("l'effort s'efface"), Village.GetRoadEfforts().Num(), 0);
	TestEqual(TEXT("aucun sentier abandonne avant de naitre"), Village.GetRoads().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisTrafficReinforcementTest,
	"Anastasis.Sim.Village.Sentiers.Renforcement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisTrafficReinforcementTest::RunTest(const FString&)
{
	using namespace AnastasisTrafficTest;
	// La boucle telle que le moteur la porte : la ou l'on marche, le sentier rend le meme trajet moins cher
	// (A*) et plus court en temps (ecart n°29) -- donc plus de passages par jour, donc il se maintient.
	// Constat epingle (settlement-morphogenesis-001) : l'heuristique de l'A* de la reference compte 10 par
	// case ; sous un sentier (8,6) elle surestime, et l'A* rend le premier chemin direct sans chercher le
	// detour. Un sentier n'ATTIRE PAS un trajet parallele, meme a une case : il ne renforce que l'usage
	// qui le suit deja. Ce test le fixe, pour qu'un changement de l'A* se voie.
	AnastasisWorld::FWorld World = MakeMeadow(40, 20);
	FVillage Village;
	Village.Bind(World);
	Village.SetRoadEvolutionEnabled(true);
	Arrive(Village);

	auto Measure = [&](const AnastasisPath::FPoint& From, const AnastasisPath::FPoint& To, double& OutCost) -> int32
	{
		AnastasisPath::FWorldNavSource Source(Village.GetNavGrid(), World);
		AnastasisPath::FOptions Options;
		Options.MaxCost = 5000.0;
		Options.MaxExpanded = 20000;
		TArray<AnastasisPath::FPoint> Path;
		OutCost = -1.0;
		if (!AnastasisPath::FindPath(Source, From, To, Options, Path)) return -1;
		OutCost = 0.0;
		int32 OnRoad = 0;
		for (const AnastasisPath::FPoint& P : Path)
		{
			const int32 X = FMath::FloorToInt32(P.X);
			const int32 Y = FMath::FloorToInt32(P.Y);
			OutCost += AnastasisNav::TileTraversalCost(Village.GetNavGrid(), X, Y, AnastasisPath::StraightCost);
			OnRoad += Village.LiveTileAt(X, Y).Type == AnastasisWorld::ETileType::Road;
		}
		return OnRoad;
	};
	const AnastasisPath::FPoint Along0{ 6.5, Row + 0.5 };
	const AnastasisPath::FPoint Along1{ 29.5, Row + 0.5 };
	const AnastasisPath::FPoint Beside0{ 6.5, Row + 1.5 };
	const AnastasisPath::FPoint Beside1{ 29.5, Row + 1.5 };
	double AlongBefore = 0.0;
	double BesideBefore = 0.0;
	TestEqual(TEXT("avant : aucune route"), Measure(Along0, Along1, AlongBefore), 0);
	Measure(Beside0, Beside1, BesideBefore);

	double Time = 30.0;
	int32 Day = 1;
	FWalker Walker;
	RunDays(Village, Time, Day, 30, &Walker);
	TestTrue(TEXT("des sentiers sont nes"), Village.GetRoads().Num() >= 10);
	double AlongAfter = 0.0;
	double BesideAfter = 0.0;
	const int32 AlongRoad = Measure(Along0, Along1, AlongAfter);
	const int32 BesideRoad = Measure(Beside0, Beside1, BesideAfter);
	AddInfo(FString::Printf(TEXT("le long du va-et-vient : %d cases de route, cout %.1f -> %.1f ; trajet parallele : %d cases de route, cout %.1f -> %.1f ; %d sentiers"),
		AlongRoad, AlongBefore, AlongAfter, BesideRoad, BesideBefore, BesideAfter, Village.GetRoads().Num()));
	TestTrue(TEXT("le trajet qui suit l'usage passe par le sentier"), AlongRoad >= 10);
	TestTrue(TEXT("et il coute moins cher qu'avant"), AlongAfter < AlongBefore * 0.95);
	TestEqual(TEXT("constat : un trajet parallele n'est pas attire (heuristique de la reference)"), BesideRoad, 0);
	return true;
}

#endif
