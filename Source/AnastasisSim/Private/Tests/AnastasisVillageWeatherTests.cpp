#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisWeatherBehavior.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisGather.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// Le village sous la meteo, de bout en bout (village-weather-001).
//
//   ciel sec : le fermier va cueillir ->
//   orage (pluie forcee 0,9, le `sim.forceWeather` de la reference) : a sa prochaine decision,
//   la porte d'orage lui fait lacher la cueillette pour `shelterRain`, et retient `gatherFood`
//   comme but a reprendre -> cible : son poste (le grenier, qui l'accepte toujours pour
//   s'abriter) -> entree, activite « abrite », `shelterRainDuration(0,9)` secondes ->
//   sortie : energie +14 (et la recuperation sous l'auvent), moral +2, delai de grace 18 s,
//   but repris : `gatherFood`.
//
// Les fonctions pures sont prouvees bit a bit par Anastasis.Sim.Parite.MeteoHabitants.
// Ici on prouve l'ASSEMBLAGE.

namespace AnastasisVillageWeatherTest
{
	using namespace AnastasisVillage;
	namespace G = AnastasisGather;
	namespace B = AnastasisWeatherBehavior;

	constexpr double Dt = 1.0 / 60.0;
	/** 7,2 h : le matin, la ou le travail pese le plus. */
	constexpr double Morning = 27.0;

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
		for (int32 Y = 10; Y <= 12; ++Y)
		{
			for (int32 X = 6; X <= 8; ++X)
			{
				AnastasisWorld::FTile& Tile = World.Tiles[Y * W + X];
				Tile.Type = AnastasisWorld::ETileType::Field;
				Tile.Resource = AnastasisWorld::EResource::Food;
				Tile.Amount = 20;
				Tile.CropId = AnastasisWorld::ECropId::Grain;
			}
		}
		return World;
	}

	AnastasisNeeds::FNeeds Rested()
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 10.0;
		N.Energy = 70.0;
		N.Social = 80.0;
		N.Leisure = 80.0;
		N.Hygiene = 80.0;
		N.Thirst = 5.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		return N;
	}

	/** Avance jusqu'a Stop ; rend false si le temps est ecoule sans que Stop soit vrai. */
	bool RunUntil(FVillage& Village, double& Time, double Seconds, TFunctionRef<bool()> Stop)
	{
		const int32 Ticks = FMath::CeilToInt32(Seconds / Dt);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Time += Dt;
			Village.UpdateActors(Time, Dt);
			if (Stop()) return true;
		}
		return false;
	}

	B::FSimWeather Storm(double Rain)
	{
		B::FSimWeather W;
		W.Rain = Rain;
		return W;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageWeatherStormTest,
	"Anastasis.Sim.MeteoHabitants.Orage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageWeatherStormTest::RunTest(const FString&)
{
	using namespace AnastasisVillageWeatherTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
	const FString Farmer = Village.SpawnNpc(13.5, 11.5, Rested());
	if (!TestTrue(TEXT("farmer hired at the granary"), Village.AssignWorkplace(Farmer, G::JobFarmer, Granary)))
	{
		return false;
	}

	// Ciel sec : il va cueillir.
	double Time = Morning;
	RunUntil(Village, Time, 10.0, [&] { return Village.FindNpc(Farmer)->Goal == GoalGatherFood; });
	if (!TestEqual(TEXT("dry: the farmer goes gathering"), Village.FindNpc(Farmer)->Goal, FString(GoalGatherFood)))
	{
		return false;
	}

	// L'orage eclate. A sa prochaine decision il lache la cueillette.
	constexpr double Rain = 0.9;
	Village.SetForcedWeather(Storm(Rain));
	const bool bSheltering = RunUntil(Village, Time, 90.0, [&] { return Village.FindNpc(Farmer)->Goal == GoalShelterRain; });
	const FNpc* N = Village.FindNpc(Farmer);
	if (!TestTrue(TEXT("storm: the farmer drops the harvest for shelter"), bSheltering))
	{
		AddInfo(FString::Printf(TEXT("goal=%s winner=%s rain=%.3f shelterRow=%.3f"), *N->Goal, *N->LastDecision.TableWinner,
			N->LastDecision.WeatherRain, N->LastDecision.ShelterRowScore));
		return false;
	}
	const FDecisionTrace& Why = N->LastDecision;
	TestEqual(TEXT("the decision read the storm"), Why.WeatherRain, Rain);
	// Sous un vrai orage la ligne `shelterRain` GAGNE la table (shelterRainScore + biais meteo
	// 16t + 22) : la porte d'orage n'a pas a intervenir. La reference ne retient alors AUCUN but
	// a reprendre (`shelterResumeGoal` n'est pose que par la porte) — fidele, pas un oubli.
	TestEqual(TEXT("in a real storm the shelter row wins the table itself"), Why.TableWinner, FString(GoalShelterRain));
	TestTrue(TEXT("its row beats the harvest row"), Why.ShelterRowScore > Why.GatherRowScore);
	TestFalse(TEXT("no storm gate needed"), Why.bStormGate);
	TestTrue(TEXT("so no resume goal (the reference sets it only through the gate)"), N->ShelterResumeGoal.IsEmpty());
	TestEqual(TEXT("shelter: his workplace (no home)"), Why.TargetSource, FString(TEXT("workplace")));
	TestEqual(TEXT("shelter: the granary"), Why.BuildingId, Granary);

	// Il entre, et y reste la duree de l'orage.
	const bool bInside = RunUntil(Village, Time, 60.0, [&]
	{
		const FNpc* M = Village.FindNpc(Farmer);
		return M->Inside.bActive && M->Inside.Goal == GoalShelterRain;
	});
	N = Village.FindNpc(Farmer);
	if (!TestTrue(TEXT("he gets inside"), bInside))
	{
		AddInfo(FString::Printf(TEXT("goal=%s activity=%s x=%.2f y=%.2f failed=%d"), *N->Goal, *N->Activity, N->X, N->Y, N->FailedActions));
		return false;
	}
	TestEqual(TEXT("inside the granary"), N->Inside.BuildingId, Granary);
	TestEqual(TEXT("activity: abrite"), N->Inside.Activity, FString(TEXT("abrite")));
	TestEqual(TEXT("for shelterRainDuration(0.9)"), N->Inside.Until - N->Inside.EnteredAt, B::ShelterRainDuration(Rain), 1e-9);
	const double EnergyIn = N->Needs.Energy;

	// Sortie : recupere, delai de grace, reprend la cueillette.
	const bool bOut = RunUntil(Village, Time, 60.0, [&] { return Village.FindNpc(Farmer)->SheltersTaken == 1; });
	N = Village.FindNpc(Farmer);
	if (!TestTrue(TEXT("he leaves once the shelter is done"), bOut))
	{
		return false;
	}
	TestFalse(TEXT("outside again"), N->Inside.bActive);
	TestTrue(TEXT("energy recovered (+14 and the slow recovery under the roof)"), N->Needs.Energy >= FMath::Min(100.0, EnergyIn + B::Shelter::EnergyRecover));
	TestEqual(TEXT("grace delay of 18 s"), N->ShelterCooldownUntil, Time + B::Shelter::CooldownSeconds, Dt * 1.5);
	// Sans but a reprendre, la reference passe a `craft` (non porte ici : observer).
	TestEqual(TEXT("no resume goal: craft in the reference, observer here"), N->Goal, FString(GoalObserver));

	// Pendant le delai de grace, l'orage ne le renvoie pas a l'abri.
	TestFalse(TEXT("no shelter call during the grace delay"),
		B::ShouldSeekRainShelter(Rain, false, GoalGatherFood, G::JobFarmer, Time + 1.0, N->ShelterCooldownUntil));

	// L'orage passe : il ne s'abrite plus et reprend une vraie activite, de lui-meme. Laquelle
	// depend de l'heure (l'orage a dure jusqu'au soir : le repos peut gagner) — on l'ecrit.
	Village.ClearForcedWeather();
	bool bShelteredAgain = false;
	const bool bBusy = RunUntil(Village, Time, 30.0, [&]
	{
		const FNpc* M = Village.FindNpc(Farmer);
		bShelteredAgain |= M->Goal == GoalShelterRain;
		return M->Goal != GoalObserver && M->Goal != GoalShelterRain;
	});
	N = Village.FindNpc(Farmer);
	AddInfo(FString::Printf(TEXT("ANASTASIS_VILLAGE_AFTER_STORM phase=%s goal=%s"), *N->LastDecision.Phase, *N->Goal));
	TestFalse(TEXT("storm over: no more sheltering"), bShelteredAgain);
	TestTrue(TEXT("storm over: he takes up a real activity on his own"), bBusy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageWeatherDryTest,
	"Anastasis.Sim.MeteoHabitants.TempsSec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageWeatherDryTest::RunTest(const FString&)
{
	using namespace AnastasisVillageWeatherTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);

	// Temoin : meme village, meme fermier, sans orage. Il ne s'abrite jamais, ne perd rien a la pluie.
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
	const FString Farmer = Village.SpawnNpc(13.5, 11.5, Rested());
	Village.AssignWorkplace(Farmer, G::JobFarmer, Granary);
	double Time = Morning;
	bool bEverSheltered = false;
	RunUntil(Village, Time, 90.0, [&] { bEverSheltered |= Village.FindNpc(Farmer)->Goal == GoalShelterRain; return false; });
	TestFalse(TEXT("dry: never sheltering"), bEverSheltered);
	TestEqual(TEXT("dry: no shelter taken"), Village.FindNpc(Farmer)->SheltersTaken, 0);

	// Sans hote, pas de meteo : chaque terme vaut 0 (le comportement d'avant, au bit pres).
	const B::FSimWeather Sky = Village.CurrentWeather();
	TestTrue(TEXT("no host: a dry summer sky"), Sky.Rain == 0.0 && Sky.Snow == 0.0 && Sky.Wind == 0.0
		&& Sky.Season == AnastasisWeather::ESeason::Summer);
	TestEqual(TEXT("no host: every weather bias is 0"), B::WeatherGoalBias(Sky, G::JobFarmer, GoalGatherFood), 0.0);
	TestEqual(TEXT("no host: walking pace untouched"), Village.DailyRain(), 0.0);

	// Une pluie legere (sous l'orage) ne fait lacher personne : elle teinte seulement la table.
	FVillage Drizzle;
	Drizzle.Bind(World);
	const FString G2 = Drizzle.AddBuilding(GranaryType, 16, 11);
	const FString F2 = Drizzle.SpawnNpc(13.5, 11.5, Rested());
	Drizzle.AssignWorkplace(F2, G::JobFarmer, G2);
	Drizzle.SetForcedWeather(Storm(0.4));
	double T2 = Morning;
	bool bShelter2 = false;
	RunUntil(Drizzle, T2, 60.0, [&] { bShelter2 |= Drizzle.FindNpc(F2)->LastDecision.bStormGate; return false; });
	TestFalse(TEXT("drizzle (0.4 < 0.48): no storm gate"), bShelter2);

	// Orage naissant (0,5) : la ligne d'abri est plus faible, la porte d'orage peut avoir a
	// trancher. Invariant a chaque fois qu'elle tranche : le but retenu est le travail expose
	// qu'il lachait. Le nombre de declenchements est ecrit, pas suppose.
	FVillage Edge;
	Edge.Bind(World);
	const FString G3 = Edge.AddBuilding(GranaryType, 16, 11);
	const FString F3 = Edge.SpawnNpc(13.5, 11.5, Rested());
	Edge.AssignWorkplace(F3, G::JobFarmer, G3);
	Edge.SetForcedWeather(Storm(0.5));
	double T3 = Morning;
	int32 GateFired = 0;
	int32 TableShelter = 0;
	bool bResumeOk = true;
	double LastDecisionAt = -1.0;
	RunUntil(Edge, T3, 120.0, [&]
	{
		const FNpc* M = Edge.FindNpc(F3);
		if (M->LastDecision.Time != LastDecisionAt)
		{
			LastDecisionAt = M->LastDecision.Time;
			if (M->LastDecision.bStormGate)
			{
				++GateFired;
				bResumeOk &= B::IsRainExposedGoal(M->ShelterResumeGoal);
			}
			else if (M->LastDecision.Winner == GoalShelterRain)
			{
				++TableShelter;
			}
		}
		return false;
	});
	AddInfo(FString::Printf(TEXT("ANASTASIS_VILLAGE_STORM_EDGE rain=0.5 gate_fired=%d table_shelter=%d shelters=%d"),
		GateFired, TableShelter, Edge.FindNpc(F3)->SheltersTaken));
	TestTrue(TEXT("every storm-gate decision keeps an exposed goal to resume"), bResumeOk);
	TestTrue(TEXT("rain 0.5: he does take shelter"), Edge.FindNpc(F3)->SheltersTaken > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageWeatherHostTest,
	"Anastasis.Sim.MeteoHabitants.CielDeLaSimulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageWeatherHostTest::RunTest(const FString&)
{
	// Avec l'hote, les habitants lisent la meteo de la SIMULATION : la graine du monde, le jour
	// et l'heure de son horloge — celle que le ciel d'Unreal montre (AnastasisSkyClock).
	FAnastasisSimulation Sim;
	Sim.Reset(12345u, 96, 96);
	TestEqual(TEXT("the village holds the simulation's seed"), static_cast<int64>(Sim.GetVillage().GetWeatherSeed()), static_cast<int64>(12345));
	for (int32 Step = 0; Step < 400; ++Step)
	{
		Sim.Tick(1.0 / 60.0 * 30.0);
	}
	const AnastasisWeatherBehavior::FSimWeather Seen = Sim.GetVillage().CurrentWeather();
	const AnastasisWeatherBehavior::FSimWeather Expected =
		AnastasisWeatherBehavior::ReadSimWeather(12345u, Sim.GetDay(), Sim.GetTime());
	TestEqual(TEXT("villagers read the simulation's rain"), Seen.Rain, Expected.Rain);
	TestEqual(TEXT("... its cover"), Seen.Cover, Expected.Cover);
	TestTrue(TEXT("... its season"), Seen.Season == Expected.Season);
	AddInfo(FString::Printf(TEXT("ANASTASIS_VILLAGE_WEATHER day=%d time=%.2f rain=%.3f cover=%.3f season=%s"),
		Sim.GetDay(), Sim.GetTime(), Seen.Rain, Seen.Cover, AnastasisWeather::SeasonId(Seen.Season)));
	return true;
}

#endif
