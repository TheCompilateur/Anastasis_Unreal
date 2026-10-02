#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// La boucle du premier batiment, de bout en bout, sur des mondes poses a la main.
//
//   la soif monte -> `drink` gagne -> puits le plus proche -> seuil -> A* ->
//   arrivee -> une seconde sur place -> satisfyDrink -> npc.thirst baisse
//
// Les fonctions de besoins sont prouvees bit a bit par Anastasis.Sim.Parite.Besoins.
// Ici on prouve l'ASSEMBLAGE : que la boucle se ferme, qu'elle modifie l'etat
// qu'elle annonce, et qu'elle ne laisse aucune reference morte derriere elle.

namespace AnastasisVillageSimTest
{
	using namespace AnastasisVillage;

	constexpr double Dt = 1.0 / 60.0; // SIM_FIXED_DT

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

	void SetWater(AnastasisWorld::FWorld& World, int32 X, int32 Y)
	{
		World.Tiles[Y * World.W + X].Type = AnastasisWorld::ETileType::Water;
	}

	AnastasisNeeds::FNeeds Thirsty(double Thirst)
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 10.0;
		N.Energy = 80.0;
		N.Social = 70.0;
		N.Leisure = 70.0;
		N.Hygiene = 60.0;
		N.Thirst = Thirst;
		N.Health = 90.0;
		N.Morale = 55.0;
		return N;
	}

	/** Avance la simulation ; rend false si un habitant s'est retrouve sur une case bloquee. */
	bool Run(FVillage& Village, double& Time, double Seconds, TFunctionRef<bool()> Stop)
	{
		const int32 Ticks = FMath::CeilToInt32(Seconds / Dt);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Time += Dt;
			Village.UpdateActors(Time, Dt);
			for (const FNpc& Npc : Village.GetActors())
			{
				if (Village.IsFootBlocked(Npc.X, Npc.Y))
				{
					return false;
				}
			}
			if (Stop())
			{
				break;
			}
		}
		return true;
	}

	/** Le plancher des buts non portes a une phase donnee (ecart n°1 de AnastasisVillage.h). */
	double FloorAt(AnastasisRhythm::EPhase Phase, const AnastasisRhythm::FPhaseSubject& Subject)
	{
		double Best = -AnastasisNav::Infinity;
		for (const FString& Goal : UnportedGoals())
		{
			Best = FMath::Max(Best, UnportedGoalsFloor + AnastasisRhythm::PhaseBias(Phase, Subject, Goal));
		}
		return Best;
	}

	/** Ligne `drink` de la table, un puits existant. */
	double DrinkRowAt(AnastasisRhythm::EPhase Phase, const AnastasisNeeds::FNeeds& Needs, const AnastasisRhythm::FPhaseSubject& Subject)
	{
		return AnastasisNeeds::NeedGoalScores(Needs, 1, 0).Drink + 6.0 + AnastasisRhythm::PhaseBias(Phase, Subject, GoalDrink);
	}

	bool SamePoint(const FPoint& A, const FPoint& B)
	{
		return A.X == B.X && A.Y == B.Y;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageWellRegistrationTest,
	"Anastasis.Sim.Village.Puits.Enregistrement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageWellRegistrationTest::RunTest(const FString&)
{
	using namespace AnastasisVillageSimTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
	FVillage Village;
	Village.Bind(World);
	const uint64 EmptyDigest = Village.Digest();

	const FString Id = Village.AddBuilding(WellType, 10, 10);
	TestEqual(TEXT("identifiant de la reference"), Id, FString(TEXT("building-0")));
	TestEqual(TEXT("countBuildings(well)"), Village.CountBuildings(WellType), 1);
	TestTrue(TEXT("la case du puits est bloquee"), Village.IsFootBlocked(10.5, 10.5));
	TestEqual(TEXT("version de navigation incrementee"), Village.GetNavVersion(), 1);
	TestTrue(TEXT("l'empreinte voit le batiment"), Village.Digest() != EmptyDigest);

	const FBuilding* Well = Village.FindBuilding(Id);
	if (!TestNotNull(TEXT("retrouve par identifiant"), Well))
	{
		return false;
	}
	// ACCESS_BUDGET.well = 4 ; anneau 1 seulement, jamais le centre.
	TestEqual(TEXT("4 seuils"), Well->AccessPoints.Num(), 4);
	for (const FPoint& P : Well->AccessPoints)
	{
		TestFalse(TEXT("seuil libre"), Village.IsFootBlocked(P.X, P.Y));
		const int32 Cheb = FMath::Max(FMath::Abs(FMath::FloorToInt32(P.X) - 10), FMath::Abs(FMath::FloorToInt32(P.Y) - 10));
		TestEqual(TEXT("seuil sur l'anneau 1"), Cheb, 1);
	}
	// Camp au centre (16,16) : |sx| >= |sy| -> la porte principale regarde +X.
	TestTrue(TEXT("porte principale vers le camp"), SamePoint(Well->AccessPoints[0], FPoint{ 11.5, 10.5 }));
	TestTrue(TEXT("puits a portee depuis son seuil"), Village.AtDrinkSpot(11.5, 10.5));
	TestFalse(TEXT("pas d'eau loin du puits"), Village.AtDrinkSpot(20.5, 20.5));

	TestTrue(TEXT("une seconde pose sur la meme case est refusee"), Village.AddBuilding(WellType, 10, 10).IsEmpty());
	TestTrue(TEXT("hors bornes refuse"), Village.AddBuilding(WellType, 40, 3).IsEmpty());

	// Un puits inacheve existe mais ne compte pas et ne donne pas a boire.
	const FString Unfinished = Village.AddBuilding(WellType, 20, 20, 0.4);
	TestEqual(TEXT("identifiant suivant"), Unfinished, FString(TEXT("building-1")));
	TestEqual(TEXT("inacheve non compte"), Village.CountBuildings(WellType), 1);
	TestFalse(TEXT("inacheve ne desaltere pas"), Village.AtDrinkSpot(21.5, 20.5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageWellSelectionTest,
	"Anastasis.Sim.Village.Puits.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageWellSelectionTest::RunTest(const FString&)
{
	using namespace AnastasisVillageSimTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	const FString Near = Village.AddBuilding(WellType, 12, 10);
	const FString Far = Village.AddBuilding(WellType, 30, 10);
	const FString ThirstyId = Village.SpawnNpc(4.5, 10.5, Thirsty(55.0));
	const FString CalmId = Village.SpawnNpc(4.5, 20.5, Thirsty(20.0));

	double Time = 0.0;
	Run(Village, Time, 2.5, [] { return false; }); // Noûs : une pensee chacun (cadence 2,2 s)

	const FNpc* Npc = Village.FindNpc(ThirstyId);
	const FNpc* Calm = Village.FindNpc(CalmId);
	if (!TestNotNull(TEXT("habitant assoiffe"), Npc) || !TestNotNull(TEXT("habitant calme"), Calm))
	{
		return false;
	}

	TestEqual(TEXT("but drink"), Npc->Goal, FString(GoalDrink));
	TestEqual(TEXT("vise le puits le plus proche"), Npc->DestBuildingId, Near);
	TestTrue(TEXT("a une cible"), Npc->bHasTarget);
	const FBuilding* NearWell = Village.FindBuilding(Near);
	bool bTargetIsAccessPoint = false;
	for (const FPoint& P : NearWell->AccessPoints)
	{
		bTargetIsAccessPoint |= SamePoint(P, Npc->Target);
	}
	TestTrue(TEXT("la cible est un seuil du puits, pas son centre"), bTargetIsAccessPoint);

	// Pourquoi : la ligne drink de la reference, et ce qu'elle a battu.
	const FDecisionTrace& Why = Npc->LastDecision;
	TestEqual(TEXT("trace : vainqueur"), Why.Winner, FString(GoalDrink));
	TestEqual(TEXT("trace : source"), Why.TargetSource, FString(TEXT("well")));
	TestEqual(TEXT("trace : batiment"), Why.BuildingId, Near);
	// `needs.drink + 6 + goalNoise(sim, 6)`, puis le rythme (perception-explore-001 : le bruit est tire).
	TestTrue(TEXT("trace : ligne drink = needGoalScores.drink + 6 + bruit + phaseBias (exact)"),
		Why.DrinkRowScore == Why.NeedScores.Drink + 6.0 + Why.RowNoise.FindRef(GoalDrink)
			+ AnastasisRhythm::PhaseBias(AnastasisRhythm::EPhase::Night, AnastasisRhythm::FPhaseSubject(), GoalDrink));
	TestTrue(TEXT("trace : au-dessus du plancher des buts non portes"), Why.DrinkRowScore > Why.FloorScore);
	TestTrue(TEXT("trace : au-dessus de la ligne rest"), Why.DrinkRowScore > Why.RestRowScore);
	TestEqual(TEXT("trace : l'horloge commence la nuit"), Why.Phase, FString(TEXT("night")));
	TestTrue(TEXT("usagers du puits proche"), Village.UsersOf(Near).Num() == 1 && Village.UsersOf(Near)[0] == ThirstyId);
	TestEqual(TEXT("personne au puits lointain"), Village.UsersOf(Far).Num(), 0);

	// Sous le seuil, il ne boit pas ; et la nuit, c'est le repos qui l'emporte.
	TestNotEqual(TEXT("sous le seuil : ne va pas boire"), Calm->Goal, FString(GoalDrink));
	TestTrue(TEXT("sous le seuil : ligne drink sous le plancher"), Calm->LastDecision.DrinkRowScore <= Calm->LastDecision.FloorScore);
	TestEqual(TEXT("la nuit, sans toit : il va se reposer"), Calm->Goal, FString(GoalRest));

	// Le seuil de soif DEPEND de la phase : c'est le rythme de la reference.
	// Nuit, aube : exactement thirstUrge (40). Matin : il faut ~64. (Midi n'est plus
	// dans la liste : depuis granary-eat-001, `eat` est une ligne calculee et ne pese
	// plus 42 + son rythme ; a midi le plancher est `relax`.)
	AnastasisRhythm::FPhaseSubject Homeless;
	for (const AnastasisRhythm::EPhase Phase : { AnastasisRhythm::EPhase::Night, AnastasisRhythm::EPhase::Dawn })
	{
		const FString Name = AnastasisRhythm::PhaseId(Phase);
		TestTrue(*(Name + TEXT(" : 39.99 ne declenche pas")), DrinkRowAt(Phase, Thirsty(39.99), Homeless) <= FloorAt(Phase, Homeless));
		TestTrue(*(Name + TEXT(" : 40 declenche")), DrinkRowAt(Phase, Thirsty(40.0), Homeless) > FloorAt(Phase, Homeless));
	}
	TestTrue(TEXT("matin : 60 ne suffit pas contre le travail"),
		DrinkRowAt(AnastasisRhythm::EPhase::Morning, Thirsty(60.0), Homeless) <= FloorAt(AnastasisRhythm::EPhase::Morning, Homeless));
	TestTrue(TEXT("matin : 65 l'emporte"),
		DrinkRowAt(AnastasisRhythm::EPhase::Morning, Thirsty(65.0), Homeless) > FloorAt(AnastasisRhythm::EPhase::Morning, Homeless));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageWellLoopTest,
	"Anastasis.Sim.Village.Puits.AtteinteEtEffet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageWellLoopTest::RunTest(const FString&)
{
	using namespace AnastasisVillageSimTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
	// Un mur d'eau entre l'habitant et le puits, ouvert en bas : la ligne droite
	// est coupee, l'A* doit contourner.
	for (int32 Y = 4; Y < 26; ++Y)
	{
		SetWater(World, 12, Y);
	}
	FVillage Village;
	Village.Bind(World);
	const FString WellId = Village.AddBuilding(WellType, 18, 10);
	const FString NpcId = Village.SpawnNpc(6.5, 10.5, Thirsty(60.0));

	double Time = 0.0;
	bool bSawPath = false;
	bool bArrived = false;
	AnastasisNeeds::FNeeds BeforeDrink;
	FString GoalBefore;
	bool bSpotBefore = false;
	bool bDrankOnce = false;
	AnastasisNeeds::FNeeds AfterDrink;
	double PeakThirst = 0.0;

	const bool bSafe = Run(Village, Time, 40.0, [&]
	{
		const FNpc* N = Village.FindNpc(NpcId);
		bSawPath |= N->Path.Num() > 0;
		PeakThirst = FMath::Max(PeakThirst, N->Needs.Thirst);
		if (N->bHasTarget && FMath::Sqrt(FMath::Square(N->X - N->Target.X) + FMath::Square(N->Y - N->Target.Y)) <= ArrivalDistance)
		{
			bArrived = true;
		}
		if (N->DrinksTaken > 0)
		{
			AfterDrink = N->Needs;
			bDrankOnce = true;
			return true;
		}
		BeforeDrink = N->Needs;
		GoalBefore = N->Goal;
		bSpotBefore = Village.AtDrinkSpot(N->X, N->Y);
		return false;
	});

	TestTrue(TEXT("jamais sur une case bloquee"), bSafe);
	TestTrue(TEXT("un chemin A* a ete suivi"), bSawPath);
	TestTrue(TEXT("arrive au seuil"), bArrived);
	TestTrue(TEXT("a bu"), bDrankOnce);
	if (!bDrankOnce)
	{
		return false;
	}
	const FNpc* Npc = Village.FindNpc(NpcId);
	TestTrue(TEXT("de l'autre cote du mur"), Npc->X > 12.0);
	TestTrue(TEXT("au puits"), Village.AtDrinkSpot(Npc->X, Npc->Y));
	TestEqual(TEXT("activite boit"), Npc->Activity, FString(TEXT("boit")));

	// L'effet EXACT du tick ou l'acte s'accomplit : tickNeeds (au point d'eau),
	// puis satisfyDrink. Pas « la soif a baisse » : les bits attendus.
	AnastasisNeeds::FNeeds Expected = BeforeDrink;
	AnastasisNeeds::TickNeeds(Expected, Dt, GoalBefore == GoalDrink && bSpotBefore, false);
	AnastasisNeeds::SatisfyDrink(Expected);
	TestTrue(TEXT("thirst exact (bits)"), AfterDrink.Thirst == Expected.Thirst);
	TestTrue(TEXT("hygiene exact (bits)"), AfterDrink.Hygiene == Expected.Hygiene);
	TestTrue(TEXT("morale exact (bits)"), AfterDrink.Morale == Expected.Morale);
	TestTrue(TEXT("health exact (bits)"), AfterDrink.Health == Expected.Health);
	// Sur l'interaction entiere (la seconde au puits a -9,5/s, puis -62 borne a 0),
	// pas sur le seul tick de l'acte : satisfyDrink est bornee a zero.
	TestTrue(TEXT("la soif a reellement baisse depuis le pic"), AfterDrink.Thirst < PeakThirst - 50.0);
	AddInfo(FString::Printf(TEXT("soif pic=%.3f avant l'acte=%.3f apres=%.3f"), PeakThirst, BeforeDrink.Thirst, AfterDrink.Thirst));
	TestFalse(TEXT("cible effacee apres l'acte"), Npc->bHasTarget);

	// La boucle se referme : a la pensee suivante, la soif est basse, il vaque.
	Run(Village, Time, 2.5, [] { return false; }); // prochaine pensee Noûs
	Npc = Village.FindNpc(NpcId);
	TestNotEqual(TEXT("desaltere : ne va plus boire"), Npc->Goal, FString(GoalDrink));
	TestEqual(TEXT("desaltere : plus usager du puits"), Village.UsersOf(WellId).Num(), 0);

	// ... et recommence quand la soif remonte (0,48/s -> ~40 s pour repasser 40).
	const int32 DrinksBefore = Npc->DrinksTaken;
	Run(Village, Time, 240.0, [&] { return Village.FindNpc(NpcId)->DrinksTaken > DrinksBefore; });
	TestTrue(TEXT("revient boire quand la soif remonte"), Village.FindNpc(NpcId)->DrinksTaken > DrinksBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageWellRemovalTest,
	"Anastasis.Sim.Village.Puits.Destruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageWellRemovalTest::RunTest(const FString&)
{
	using namespace AnastasisVillageSimTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	const FString WellId = Village.AddBuilding(WellType, 20, 10);
	const FString A = Village.SpawnNpc(4.5, 10.5, Thirsty(60.0));
	const FString B = Village.SpawnNpc(4.5, 14.5, Thirsty(65.0));

	double Time = 0.0;
	Run(Village, Time, 2.5, [] { return false; }); // Noûs : une pensee chacun
	TestEqual(TEXT("deux usagers en route"), Village.UsersOf(WellId).Num(), 2);
	const int32 VersionBefore = Village.GetNavVersion();

	TestTrue(TEXT("demolition"), Village.RemoveBuilding(WellId));
	TestFalse(TEXT("identifiant inconnu apres coup"), Village.RemoveBuilding(WellId));
	TestNull(TEXT("introuvable"), Village.FindBuilding(WellId));
	TestFalse(TEXT("la case redevient libre"), Village.IsFootBlocked(20.5, 10.5));
	TestEqual(TEXT("les chemins sont perimes"), Village.GetNavVersion(), VersionBefore + 1);
	for (const FNpc& Npc : Village.GetActors())
	{
		TestFalse(TEXT("aucune reference au puits demoli"), Npc.DestBuildingId == WellId);
		TestFalse(TEXT("plus de cible vers lui"), Npc.bHasTarget);
		TestEqual(TEXT("chemin abandonne"), Npc.Path.Num(), 0);
	}

	// Plus d'eau du tout (monde sec) : ils redecident, ne trouvent rien, et restent sains.
	const bool bSafe = Run(Village, Time, 5.0, [] { return false; });
	TestTrue(TEXT("sans puits : jamais sur une case bloquee"), bSafe);
	for (const FNpc& Npc : Village.GetActors())
	{
		TestEqual(TEXT("sans eau : observer"), Npc.Goal, FString(GoalObserver));
		TestEqual(TEXT("sans eau : raison tracee"), Npc.LastDecision.TargetSource, FString(TEXT("none")));
		TestEqual(TEXT("n'a pas bu"), Npc.DrinksTaken, 0);
	}

	// Un nouveau puits : nouvel identifiant (jamais recycle), et ils y vont.
	const FString NewWell = Village.AddBuilding(WellType, 8, 20);
	TestEqual(TEXT("identifiant jamais recycle"), NewWell, FString(TEXT("building-1")));
	Run(Village, Time, 60.0, [&]
	{
		return Village.FindNpc(A)->DrinksTaken > 0 && Village.FindNpc(B)->DrinksTaken > 0;
	});
	TestTrue(TEXT("A boit au nouveau puits"), Village.FindNpc(A)->DrinksTaken > 0);
	TestTrue(TEXT("B boit au nouveau puits"), Village.FindNpc(B)->DrinksTaken > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageWellUnreachableTest,
	"Anastasis.Sim.Village.Puits.Inaccessible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageWellUnreachableTest::RunTest(const FString&)
{
	using namespace AnastasisVillageSimTest;

	// 1. Puits emmure : son anneau 1 est de l'eau. Aucun seuil ; la reference
	//    retombe sur la case libre la plus proche (accessPointNear), a portee.
	{
		AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				if (DX || DY) SetWater(World, 16 + DX, 16 + DY);
			}
		}
		FVillage Village;
		Village.Bind(World);
		const FString WellId = Village.AddBuilding(WellType, 16, 16);
		TestEqual(TEXT("emmure : aucun seuil"), Village.FindBuilding(WellId)->AccessPoints.Num(), 0);
		const FString NpcId = Village.SpawnNpc(6.5, 16.5, Thirsty(60.0));
		double Time = 0.0;
		const bool bSafe = Run(Village, Time, 30.0, [&] { return Village.FindNpc(NpcId)->DrinksTaken > 0; });
		TestTrue(TEXT("emmure : jamais sur une case bloquee"), bSafe);
		TestTrue(TEXT("emmure : a bu depuis la case la plus proche"), Village.FindNpc(NpcId)->DrinksTaken > 0);
	}

	// 2. Puits hors d'atteinte : un fleuve coupe le monde en deux. L'A* echoue,
	//    l'escalade anti-blocage abandonne, et l'habitant ne traverse jamais l'eau.
	{
		AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
		for (int32 Y = 0; Y < 32; ++Y)
		{
			SetWater(World, 16, Y);
		}
		FVillage Village;
		Village.Bind(World);
		const FString WellId = Village.AddBuilding(WellType, 22, 10);
		const FString NpcId = Village.SpawnNpc(6.5, 10.5, Thirsty(60.0));
		double Time = 0.0;
		bool bStayedOnHisSide = true;
		bool bAbandoned = false;
		const bool bSafe = Run(Village, Time, 20.0, [&]
		{
			const FNpc* N = Village.FindNpc(NpcId);
			bStayedOnHisSide &= N->X < 16.0;
			bAbandoned |= N->Goal == GoalObserver && N->LastDecision.Winner == GoalDrink;
			return false;
		});
		const FNpc* Npc = Village.FindNpc(NpcId);
		TestTrue(TEXT("hors d'atteinte : jamais sur une case bloquee"), bSafe);
		TestTrue(TEXT("hors d'atteinte : ne traverse pas le fleuve"), bStayedOnHisSide);
		TestTrue(TEXT("hors d'atteinte : l'escalade abandonne la cible"), bAbandoned);
		TestEqual(TEXT("hors d'atteinte : n'a pas bu"), Npc->DrinksTaken, 0);
		TestTrue(TEXT("hors d'atteinte : le puits est intact"), Village.FindBuilding(WellId) != nullptr);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageWellCrowdTest,
	"Anastasis.Sim.Village.Puits.MultiAgents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageWellCrowdTest::RunTest(const FString&)
{
	using namespace AnastasisVillageSimTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 40);

	auto Build = [&](FVillage& Village)
	{
		Village.Bind(World);
		Village.AddBuilding(WellType, 20, 20);
		const double Ring[6][2] = { { 12.5, 20.5 }, { 28.5, 20.5 }, { 20.5, 12.5 }, { 20.5, 28.5 }, { 14.5, 14.5 }, { 26.5, 26.5 } };
		for (int32 K = 0; K < 6; ++K)
		{
			// La nuit, sans toit, `rest` pese ~93,6 : il faut une soif > ~53 pour que
			// `drink` l'emporte. 60+ : tous vont au puits.
			Village.SpawnNpc(Ring[K][0], Ring[K][1], Thirsty(60.0 + K));
		}
	};

	FVillage Village;
	Build(Village);
	const FString WellId = TEXT("building-0");
	const FBuilding Snapshot = *Village.FindBuilding(WellId);

	double Time = 0.0;
	Run(Village, Time, 2.3, [] { return false; }); // Noûs : une pensee chacun
	TestEqual(TEXT("six usagers simultanes"), Village.UsersOf(WellId).Num(), 6);
	TSet<FIntPoint> Targets;
	for (const FNpc& Npc : Village.GetActors())
	{
		Targets.Add(FIntPoint(FMath::FloorToInt32(Npc.Target.X), FMath::FloorToInt32(Npc.Target.Y)));
	}
	// L'occupation (`localOccupancy`, x3,2) repartit les arrivants sur les seuils.
	TestTrue(TEXT("les seuils sont repartis"), Targets.Num() >= 3);
	AddInfo(FString::Printf(TEXT("6 habitants -> %d seuils distincts"), Targets.Num()));

	// Un habitant retire EN PLEINE interaction : sur place, minuteur engage.
	FString Removed;
	Run(Village, Time, 30.0, [&]
	{
		for (const FNpc& Npc : Village.GetActors())
		{
			if (Npc.Goal == GoalDrink && Npc.WorkTimer > 0.2)
			{
				Removed = Npc.Id;
				return true;
			}
		}
		return false;
	});
	TestFalse(TEXT("un habitant a ete surpris en train de puiser"), Removed.IsEmpty());
	TestTrue(TEXT("retrait"), Village.RemoveNpc(Removed));
	TestFalse(TEXT("plus usager"), Village.UsersOf(WellId).Contains(Removed));
	TestEqual(TEXT("cinq restent"), Village.GetActors().Num(), 5);

	bool bUsersConsistent = true;
	const bool bSafe = Run(Village, Time, 60.0, [&]
	{
		int32 Expected = 0;
		for (const FNpc& Npc : Village.GetActors())
		{
			Expected += Npc.Goal == GoalDrink && Npc.DestBuildingId == WellId ? 1 : 0;
		}
		bUsersConsistent &= Village.UsersOf(WellId).Num() == Expected;
		for (const FNpc& Npc : Village.GetActors())
		{
			if (Npc.DrinksTaken == 0) return false;
		}
		return true;
	});
	TestTrue(TEXT("jamais sur une case bloquee"), bSafe);
	TestTrue(TEXT("usagers = habitants qui le visent, a chaque tick"), bUsersConsistent);
	for (const FNpc& Npc : Village.GetActors())
	{
		TestTrue(*FString::Printf(TEXT("%s a bu"), *Npc.Id), Npc.DrinksTaken > 0);
	}

	// Le puits n'a pas d'etat que la foule pourrait corrompre : l'enregistrement est intact.
	const FBuilding* After = Village.FindBuilding(WellId);
	TestEqual(TEXT("identifiant"), After->Id, Snapshot.Id);
	TestTrue(TEXT("case"), After->X == Snapshot.X && After->Y == Snapshot.Y);
	TestTrue(TEXT("progress"), After->Progress == Snapshot.Progress);
	TestEqual(TEXT("seuils"), After->AccessPoints.Num(), Snapshot.AccessPoints.Num());

	// Determinisme : meme village, memes ticks -> meme empreinte.
	FVillage Twin;
	Build(Twin);
	FVillage Again;
	Build(Again);
	double TA = 0.0;
	double TB = 0.0;
	Run(Twin, TA, 45.0, [] { return false; });
	Run(Again, TB, 45.0, [] { return false; });
	TestTrue(TEXT("empreinte deterministe"), Twin.Digest() == Again.Digest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageWellHostTest,
	"Anastasis.Sim.Village.Puits.Hote",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageWellHostTest::RunTest(const FString&)
{
	using namespace AnastasisVillageSimTest;
	// Le vrai hote, le vrai monde canonique : c'est `tick(dt)` qui fait vivre l'habitant.
	FAnastasisSimulation Sim;
	Sim.Reset(12345u, 96, 96);
	FVillage& Village = Sim.GetVillage();
	TestTrue(TEXT("village lie au monde par Reset"), Village.IsBound());

	FString WellId;
	int32 WX = 0;
	int32 WY = 0;
	for (int32 R = 0; R < 30 && WellId.IsEmpty(); ++R)
	{
		for (int32 DY = -R; DY <= R && WellId.IsEmpty(); ++DY)
		{
			for (int32 DX = -R; DX <= R && WellId.IsEmpty(); ++DX)
			{
				const int32 X = 48 + DX;
				const int32 Y = 48 + DY;
				if (Village.IsFootBlocked(X + 0.5, Y + 0.5) || Village.IsFootBlocked(X + 1.5, Y + 0.5)) continue;
				WellId = Village.AddBuilding(WellType, X, Y);
				WX = X;
				WY = Y;
			}
		}
	}
	if (!TestFalse(TEXT("puits pose sur le monde canonique"), WellId.IsEmpty()))
	{
		return false;
	}
	// L'habitant se tient sur le seuil principal : l'atteinte est prouvee ailleurs,
	// ici on prouve que c'est l'hote qui pompe la boucle.
	const FPoint Door = Village.FindBuilding(WellId)->AccessPoints[0];
	// L'hote demarre a 0,42 jour (le matin, comme la reference) : la soif doit
	// battre le travail, qui pese 42 + 62 a cette heure.
	const FString NpcId = Village.SpawnNpc(Door.X, Door.Y, Thirsty(80.0));

	for (int32 I = 0; I < 60 * 10 && Village.FindNpc(NpcId)->DrinksTaken == 0; ++I)
	{
		Sim.Tick(Dt);
	}
	const FNpc* Npc = Village.FindNpc(NpcId);
	TestTrue(TEXT("l'hote fait boire l'habitant"), Npc->DrinksTaken > 0);
	TestTrue(TEXT("soif reellement baissee"), Npc->Needs.Thirst < 20.0);
	AddInfo(FString::Printf(TEXT("puits %s en (%d,%d), t=%.3f, soif=%.3f"), *WellId, WX, WY, Sim.GetTime(), Npc->Needs.Thirst));

	// Reset remet le village a zero : pas d'habitant fantome d'une partie a l'autre.
	Sim.Reset(12345u, 96, 96);
	TestEqual(TEXT("Reset vide les habitants"), Sim.GetVillage().GetActors().Num(), 0);
	TestEqual(TEXT("Reset vide les batiments"), Sim.GetVillage().GetBuildings().Num(), 0);
	return true;
}

#endif
