#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// Le grenier, de bout en bout, par Noûs — le chemin actif par defaut de la reference.
//
//   la faim monte -> Noûs (2,2 s) : seek_food vers le grenier CONNU ->
//   table : `eat` gagne -> RESERVATION d'une portion -> cible (mealPlace) ->
//   A* -> ENTREE (2,1 s) -> tickNeeds branche repas -> a `until` :
//   confirmMeal (stock -1) -> inventaire +1 -1 -> satisfyEat -> SORTIE
//
// Les fonctions pures (besoins, Noûs) sont prouvees bit a bit par
// Anastasis.Sim.Parite.Besoins / .Nous. Ici on prouve l'ASSEMBLAGE.

namespace AnastasisVillageGranaryTest
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

	AnastasisNeeds::FNeeds Hungry(double Hunger)
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = Hunger;
		N.Energy = 80.0;
		N.Social = 70.0;
		N.Leisure = 70.0;
		N.Hygiene = 60.0;
		N.Thirst = 5.0;
		N.Health = 90.0;
		N.Morale = 55.0;
		return N;
	}

	/** Avance ; rend false si un habitant DEHORS est sur une case bloquee ou si un stock devient incoherent. */
	bool Run(FVillage& Village, double& Time, double Seconds, TFunctionRef<bool()> Stop)
	{
		const int32 Ticks = FMath::CeilToInt32(Seconds / Dt);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Time += Dt;
			Village.UpdateActors(Time, Dt);
			for (const FNpc& Npc : Village.GetActors())
			{
				if (!Npc.Inside.bActive && Village.IsFootBlocked(Npc.X, Npc.Y)) return false;
			}
			for (const FBuilding& B : Village.GetBuildings())
			{
				if (B.FoodPhysical < 0 || B.FoodReserved < 0 || B.FoodReserved > B.FoodPhysical) return false;
			}
			if (Stop()) break;
		}
		return true;
	}

	bool IsAccessPointOf(const FBuilding& B, const FPoint& P)
	{
		for (const FPoint& A : B.AccessPoints)
		{
			if (A.X == P.X && A.Y == P.Y) return true;
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGranaryRegistrationTest,
	"Anastasis.Sim.Village.Grenier.Enregistrement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGranaryRegistrationTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGranaryTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 10, 10);
	const FString House = Village.AddBuilding(HouseType, 20, 20);
	TestEqual(TEXT("identifiant"), Granary, FString(TEXT("building-0")));
	TestTrue(TEXT("groupe food"), IsFoodGroupType(GranaryType) && !IsFoodGroupType(HouseType) && !IsFoodGroupType(WellType));
	TestEqual(TEXT("ACCESS_BUDGET.default = 3 seuils"), Village.FindBuilding(Granary)->AccessPoints.Num(), 3);
	TestEqual(TEXT("vide a la pose"), Village.FindBuilding(Granary)->FoodPhysical, 0);

	const uint64 Empty = Village.Digest();
	TestEqual(TEXT("creditStock 12"), Village.CreditFood(Granary, 12), 12);
	TestTrue(TEXT("l'empreinte voit le stock"), Village.Digest() != Empty);
	TestEqual(TEXT("capacite 300 : 400 demandes, 288 entrent"), Village.CreditFood(Granary, 400), 288);
	TestEqual(TEXT("plein"), Village.FindBuilding(Granary)->FoodPhysical, GranaryFoodCap);
	TestEqual(TEXT("une maison n'accepte pas la nourriture"), Village.CreditFood(House, 5), 0);
	TestEqual(TEXT("negatif refuse"), Village.CreditFood(Granary, -3), 0);
	TestEqual(TEXT("disponible = physique - reserve"), Village.FindBuilding(Granary)->FoodAvailable(), GranaryFoodCap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGranaryPerceptionTest,
	"Anastasis.Sim.Village.Grenier.Perception",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGranaryPerceptionTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGranaryTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 10, 10);
	Village.CreditFood(Granary, 12);

	// `spawnNpc` percoit d'office : a 5 cases on voit le grenier, a 15 non.
	const FString Near = Village.SpawnNpc(15.5, 10.5, Hungry(20.0));
	const FString Far = Village.SpawnNpc(25.5, 10.5, Hungry(20.0));
	const FNpc* N = Village.FindNpc(Near);
	if (!TestEqual(TEXT("proche : une croyance"), N->KnownStocks.Num(), 1))
	{
		return false;
	}
	const FStockBelief& B = N->KnownStocks[0];
	TestEqual(TEXT("cle de la reference"), B.Key, FString(TEXT("stock:food:building-0")));
	TestEqual(TEXT("par identifiant"), B.BuildingId, Granary);
	TestTrue(TEXT("estime = stock vu"), B.EstimatedAmount == 12.0);
	TestTrue(TEXT("confiance 0,9"), B.Confidence == 0.9);
	TestEqual(TEXT("loin : rien vu"), Village.FindNpc(Far)->KnownStocks.Num(), 0);

	// La croyance ne suit pas le stock tant qu'on ne revient pas voir.
	Village.CreditFood(Granary, 30);
	TestTrue(TEXT("croyance figee"), Village.FindNpc(Near)->KnownStocks[0].EstimatedAmount == 12.0);
	Village.PerceiveNow(Near);
	TestTrue(TEXT("revue : mise a jour"), Village.FindNpc(Near)->KnownStocks[0].EstimatedAmount == 42.0);

	// Le lointain s'approche : il voit au prochain balayage.
	Village.FindNpcMutable(Far)->X = 12.5;
	Village.PerceiveNow(Far);
	TestEqual(TEXT("approche : il voit"), Village.FindNpc(Far)->KnownStocks.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGranaryMealTest,
	"Anastasis.Sim.Village.Grenier.Repas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGranaryMealTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGranaryTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 16, 16);
	Village.CreditFood(Granary, 12);
	const FString Id = Village.SpawnNpc(11.5, 16.5, Hungry(70.0));

	// 1. Noûs decide, la table suit, une portion est RESERVEE.
	double Time = 0.0;
	Run(Village, Time, 2.5, [&] { return Village.FindNpc(Id)->Goal == GoalEat; });
	const FNpc* Npc = Village.FindNpc(Id);
	if (!TestEqual(TEXT("but eat"), Npc->Goal, FString(GoalEat)))
	{
		return false;
	}
	const FDecisionTrace& Why = Npc->LastDecision;
	TestEqual(TEXT("Noûs : chercher une source connue"), Why.NousType, FString(TEXT("seek_food")));
	TestTrue(TEXT("la ligne eat bat tous les buts non portes"), Why.EatRowScore > Why.FloorScore);
	TestEqual(TEXT("tete de table : eat"), Why.TableWinner, FString(GoalEat));
	const FMealReservation* R = Village.FindMealReservation(Id);
	if (!TestNotNull(TEXT("une reservation"), R))
	{
		return false;
	}
	TestEqual(TEXT("reservation au grenier"), R->BuildingId, Granary);
	TestEqual(TEXT("source colony"), R->Source, FString(TEXT("colony")));
	TestEqual(TEXT("identifiant meal_1"), R->Id, FString(TEXT("meal_1")));
	TestEqual(TEXT("stock : 1 reserve"), Village.FindBuilding(Granary)->FoodReserved, 1);
	TestEqual(TEXT("stock : 11 disponibles"), Village.FindBuilding(Granary)->FoodAvailable(), 11);
	TestEqual(TEXT("action faim : en route"), Npc->HungerAction.State, FString(TEXT("navigating")));
	TestEqual(TEXT("cible : mealPlace (le grenier, pour un sans-toit)"), Why.TargetSource, FString(TEXT("meal-place")));
	TestTrue(TEXT("cible = un seuil du grenier"), IsAccessPointOf(*Village.FindBuilding(Granary), Npc->Target));

	// 2. Il entre.
	bool bSafe = Run(Village, Time, 10.0, [&] { return Village.FindNpc(Id)->Inside.bActive; });
	Npc = Village.FindNpc(Id);
	TestTrue(TEXT("jamais sur une case bloquee, stock toujours coherent"), bSafe);
	if (!TestTrue(TEXT("entre au grenier"), Npc->Inside.bActive && Npc->Inside.BuildingId == Granary))
	{
		return false;
	}
	TestEqual(TEXT("dedans : « mange »"), Npc->Inside.Activity, FString(TEXT("mange")));
	TestTrue(TEXT("eatDuration 2,1 s"), FMath::IsNearlyEqual(Npc->Inside.Until - Npc->Inside.EnteredAt, 2.1, 1e-9));

	// 3. Un tick dedans : la branche repas de tickNeeds, au bit pres.
	{
		const AnastasisNeeds::FNeeds Before = Npc->Needs;
		Time += Dt;
		Village.UpdateActors(Time, Dt);
		AnastasisNeeds::FNeeds Expected = Before;
		AnastasisNeeds::TickNeedsEatInside(Expected, Dt);
		Npc = Village.FindNpc(Id);
		TestTrue(TEXT("tick dedans : faim exacte (-2,4/s)"), Npc->Needs.Hunger == Expected.Hunger);
		TestTrue(TEXT("tick dedans : soif exacte"), Npc->Needs.Thirst == Expected.Thirst);
		TestEqual(TEXT("pas encore preleve"), Village.FindBuilding(Granary)->FoodPhysical, 12);
	}

	// 4. A `until` : confirmMeal (stock -1), satisfyEat, sortie. Au bit pres.
	AnastasisNeeds::FNeeds LastInside;
	AnastasisNeeds::FNeeds AfterMeal;
	bool bAte = false;
	Run(Village, Time, 5.0, [&]
	{
		const FNpc* N = Village.FindNpc(Id);
		if (N->MealsTaken > 0)
		{
			AfterMeal = N->Needs;
			bAte = true;
			return true;
		}
		LastInside = N->Needs;
		return false;
	});
	if (!TestTrue(TEXT("a mange"), bAte))
	{
		return false;
	}
	Npc = Village.FindNpc(Id);
	AnastasisNeeds::FNeeds Expected = LastInside;
	AnastasisNeeds::TickNeedsEatInside(Expected, Dt);
	AnastasisNeeds::SatisfyEat(Expected, /*bIndoor=*/true, /*bAtHome=*/false);
	TestTrue(TEXT("repas : faim exacte (bits)"), AfterMeal.Hunger == Expected.Hunger);
	TestTrue(TEXT("repas : moral exact (bits)"), AfterMeal.Morale == Expected.Morale);
	TestTrue(TEXT("repas : sante exacte (bits)"), AfterMeal.Health == Expected.Health);
	TestTrue(TEXT("repas : loisir exact (bits)"), AfterMeal.Leisure == Expected.Leisure);
	TestEqual(TEXT("stock physique 12 -> 11 au premier repas"), Village.FindBuilding(Granary)->FoodPhysical, 11);
	TestEqual(TEXT("plus rien de reserve"), Village.FindBuilding(Granary)->FoodReserved, 0);
	TestEqual(TEXT("inventaire +1 -1"), Npc->InventoryFood, 0);
	TestNull(TEXT("reservation consommee"), Village.FindMealReservation(Id));
	TestEqual(TEXT("action faim : terminee"), Npc->HungerAction.State, FString(TEXT("completed")));
	TestFalse(TEXT("sorti"), Npc->Inside.bActive);
	// satisfyEat dedans : cible 18, on retire 75 % de l'ecart.
	TestTrue(TEXT("la faim a reellement baisse"), AfterMeal.Hunger < LastInside.Hunger - 30.0);
	AddInfo(FString::Printf(TEXT("faim %.3f -> %.3f ; grenier 12 -> 11"), LastInside.Hunger, AfterMeal.Hunger));

	// 5. COMPORTEMENT DE LA REFERENCE : l'inertie de Noûs GARDE la decision
	// `seek_food` prise a faim 70 (evaluateInertia : il faudrait un candidat
	// meilleur de +0,20), et le pont continue de pousser `eat` de ~+62. Il
	// remange, une portion reservee a chaque fois, jusqu'a ce que la fatigue fasse
	// gagner `rest`. Le stock suit exactement les repas, la boucle s'arrete.
	Run(Village, Time, 12.0, [&] { return Village.FindNpc(Id)->Goal != GoalEat && !Village.FindNpc(Id)->Inside.bActive; });
	Npc = Village.FindNpc(Id);
	TestNotEqual(TEXT("rassasie : il finit par ne plus manger"), Npc->Goal, FString(GoalEat));
	TestTrue(TEXT("plusieurs repas, pas une boucle infinie"), Npc->MealsTaken >= 1 && Npc->MealsTaken < 12);
	TestEqual(TEXT("le grenier a perdu exactement un repas par repas"), Village.FindBuilding(Granary)->FoodPhysical, 12 - Npc->MealsTaken);
	TestEqual(TEXT("rien ne reste reserve"), Village.FindBuilding(Granary)->FoodReserved, 0);
	AddInfo(FString::Printf(TEXT("repas consecutifs : %d, faim finale %.3f, but %s"), Npc->MealsTaken, Npc->Needs.Hunger, *Npc->Goal));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGranaryCrowdTest,
	"Anastasis.Sim.Village.Grenier.MultiAgents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGranaryCrowdTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGranaryTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);

	// Trois affames, DEUX portions : la reservation tranche, jamais le hasard.
	{
		FVillage Village;
		Village.Bind(World);
		const FString Granary = Village.AddBuilding(GranaryType, 16, 16);
		Village.CreditFood(Granary, 2);
		const FString A = Village.SpawnNpc(11.5, 16.5, Hungry(75.0));
		const FString B = Village.SpawnNpc(21.5, 16.5, Hungry(72.0));
		const FString C = Village.SpawnNpc(16.5, 11.5, Hungry(70.0));

		double Time = 0.0;
		int32 MaxReservations = 0;
		bool bSawUnavailable = false;
		const bool bSafe = Run(Village, Time, 40.0, [&]
		{
			MaxReservations = FMath::Max(MaxReservations, Village.GetMealReservations().Num());
			for (const FNpc& N : Village.GetActors())
			{
				bSawUnavailable |= N.HungerAction.LastFailure == TEXT("unavailable");
			}
			int32 Meals = 0;
			for (const FNpc& N : Village.GetActors()) Meals += N.MealsTaken;
			return Meals >= 2 && Village.GetMealReservations().Num() == 0;
		});
		TestTrue(TEXT("jamais de stock incoherent (reserve <= physique, >= 0)"), bSafe);
		TestTrue(TEXT("jamais plus de reservations que de portions"), MaxReservations <= 2);
		TestTrue(TEXT("le troisieme est refuse : « unavailable »"), bSawUnavailable);
		int32 Meals = 0;
		for (const FNpc& N : Village.GetActors()) Meals += N.MealsTaken;
		TestEqual(TEXT("deux repas, pas trois"), Meals, 2);
		TestEqual(TEXT("grenier vide"), Village.FindBuilding(Granary)->FoodPhysical, 0);
		TestEqual(TEXT("rien de reserve"), Village.FindBuilding(Granary)->FoodReserved, 0);
		AddInfo(FString::Printf(TEXT("repas : A=%d B=%d C=%d, reservations max %d"),
			Village.FindNpc(A)->MealsTaken, Village.FindNpc(B)->MealsTaken, Village.FindNpc(C)->MealsTaken, MaxReservations));
	}

	// Un habitant retire avec sa portion reservee : elle revient au stock.
	{
		FVillage Village;
		Village.Bind(World);
		const FString Granary = Village.AddBuilding(GranaryType, 16, 16);
		Village.CreditFood(Granary, 1);
		const FString A = Village.SpawnNpc(9.5, 16.5, Hungry(75.0));
		double Time = 0.0;
		Run(Village, Time, 2.5, [&] { return Village.FindMealReservation(A) != nullptr; });
		TestEqual(TEXT("portion reservee"), Village.FindBuilding(Granary)->FoodReserved, 1);
		TestTrue(TEXT("retrait"), Village.RemoveNpc(A));
		TestEqual(TEXT("reserve rendue"), Village.FindBuilding(Granary)->FoodReserved, 0);
		TestEqual(TEXT("portion de nouveau disponible"), Village.FindBuilding(Granary)->FoodAvailable(), 1);
		TestEqual(TEXT("registre vide"), Village.GetMealReservations().Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGranaryRemovalTest,
	"Anastasis.Sim.Village.Grenier.Destruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGranaryRemovalTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGranaryTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);

	// Demoli pendant qu'on s'y rend, portion reservee.
	{
		FVillage Village;
		Village.Bind(World);
		const FString Granary = Village.AddBuilding(GranaryType, 16, 16);
		Village.CreditFood(Granary, 5);
		const FString A = Village.SpawnNpc(9.5, 16.5, Hungry(75.0));
		double Time = 0.0;
		Run(Village, Time, 2.5, [&] { return Village.FindMealReservation(A) != nullptr; });
		if (!TestNotNull(TEXT("reservation posee"), Village.FindMealReservation(A)))
		{
			return false;
		}
		TestTrue(TEXT("demolition"), Village.RemoveBuilding(Granary));
		TestEqual(TEXT("registre vide"), Village.GetMealReservations().Num(), 0);
		const FNpc* N = Village.FindNpc(A);
		TestTrue(TEXT("plus de reference au grenier"), N->DestBuildingId != Granary && !(N->Inside.bActive && N->Inside.BuildingId == Granary));
		TestFalse(TEXT("plus de cible vers lui"), N->bHasTarget);

		// La croyance survit (un souvenir peut etre faux) : Noûs retente, echoue
		// proprement (« no_known_source »), et personne ne mange du vide.
		const bool bSafe = Run(Village, Time, 15.0, [] { return false; });
		TestTrue(TEXT("apres : sain"), bSafe);
		TestEqual(TEXT("personne n'a mange"), Village.FindNpc(A)->MealsTaken, 0);
		for (const FMealReservation& R : Village.GetMealReservations())
		{
			TestTrue(TEXT("aucune reservation vers un batiment absent"), R.BuildingId.IsEmpty() || Village.FindBuilding(R.BuildingId) != nullptr);
		}
	}

	// Demoli pendant qu'on y mange.
	{
		FVillage Village;
		Village.Bind(World);
		const FString Granary = Village.AddBuilding(GranaryType, 16, 16);
		Village.CreditFood(Granary, 5);
		const FString A = Village.SpawnNpc(12.5, 16.5, Hungry(75.0));
		double Time = 0.0;
		Run(Village, Time, 10.0, [&] { return Village.FindNpc(A)->Inside.bActive; });
		if (!TestTrue(TEXT("dedans"), Village.FindNpc(A)->Inside.bActive))
		{
			return false;
		}
		TestTrue(TEXT("demolition, habitant dedans"), Village.RemoveBuilding(Granary));
		const FNpc* N = Village.FindNpc(A);
		TestFalse(TEXT("ressorti"), N->Inside.bActive);
		TestFalse(TEXT("sur une case libre"), Village.IsFootBlocked(N->X, N->Y));
		TestEqual(TEXT("sa portion n'existe plus"), Village.GetMealReservations().Num(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGranaryHomeFirstTest,
	"Anastasis.Sim.Village.Grenier.FoyerDabord",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGranaryHomeFirstTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGranaryTest;
	// COMPORTEMENT DE LA REFERENCE, reproduit et documente : qui a un foyer va
	// manger CHEZ LUI (couches rythme + domestique), meme avec une portion
	// reservee au grenier. Il y « mange » sans nourriture (la branche repas de
	// tickNeeds baisse sa faim) ; le grenier ne perd rien ; la reserve est rendue.
	const AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 14, 10);
	const FString House = Village.AddBuilding(HouseType, 22, 10);
	Village.CreditFood(Granary, 5);
	const FString A = Village.SpawnNpc(12.5, 10.5, Hungry(70.0));
	Village.AssignHome(A, House);
	TestEqual(TEXT("il voit le grenier"), Village.FindNpc(A)->KnownStocks.Num(), 1);

	double Time = 0.0;
	Run(Village, Time, 2.5, [&] { return Village.FindNpc(A)->Goal == GoalEat; });
	const FNpc* N = Village.FindNpc(A);
	TestEqual(TEXT("but eat"), N->Goal, FString(GoalEat));
	TestTrue(TEXT("portion reservee AU GRENIER"), Village.FindMealReservation(A) && Village.FindMealReservation(A)->BuildingId == Granary);
	TestEqual(TEXT("mais la cible est SON FOYER"), N->LastDecision.TargetSource, FString(TEXT("home")));

	bool bAteAtHome = false;
	double LowestHunger = N->Needs.Hunger;
	const bool bSafe = Run(Village, Time, 45.0, [&]
	{
		const FNpc* M = Village.FindNpc(A);
		bAteAtHome |= M->Inside.bActive && M->Inside.BuildingId == House && M->Inside.Goal == GoalEat;
		LowestHunger = FMath::Min(LowestHunger, M->Needs.Hunger);
		return false;
	});
	TestTrue(TEXT("sain"), bSafe);
	TestTrue(TEXT("il « mange » chez lui"), bAteAtHome);
	TestTrue(TEXT("sa faim baisse quand meme (branche repas de tickNeeds)"), LowestHunger < 55.0);
	TestEqual(TEXT("le grenier ne perd RIEN"), Village.FindBuilding(Granary)->FoodPhysical, 5);
	TestEqual(TEXT("aucun repas confirme"), Village.FindNpc(A)->MealsTaken, 0);
	AddInfo(FString::Printf(TEXT("faim 70 -> %.2f sans une miette ; reserve au grenier : %d"),
		LowestHunger, Village.FindBuilding(Granary)->FoodReserved));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGranaryHostTest,
	"Anastasis.Sim.Village.Grenier.Hote",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGranaryHostTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGranaryTest;
	FAnastasisSimulation Sim;
	Sim.Reset(12345u, 96, 96);
	FVillage& Village = Sim.GetVillage();

	FString Granary;
	int32 GX = 0;
	int32 GY = 0;
	for (int32 R = 0; R < 30 && Granary.IsEmpty(); ++R)
	{
		for (int32 DY = -R; DY <= R && Granary.IsEmpty(); ++DY)
		{
			for (int32 DX = -R; DX <= R && Granary.IsEmpty(); ++DX)
			{
				const int32 X = 48 + DX;
				const int32 Y = 48 + DY;
				if (Village.IsFootBlocked(X + 0.5, Y + 0.5) || Village.IsFootBlocked(X + 1.5, Y + 0.5)) continue;
				Granary = Village.AddBuilding(GranaryType, X, Y);
				GX = X;
				GY = Y;
			}
		}
	}
	if (!TestFalse(TEXT("grenier pose"), Granary.IsEmpty()))
	{
		return false;
	}
	Village.CreditFood(Granary, 6);
	const FPoint Door = Village.FindBuilding(Granary)->AccessPoints[0];
	// L'hote demarre le matin : le travail pese 42 + 62 ; une faim de 80 l'emporte.
	const FString A = Village.SpawnNpc(Door.X, Door.Y, Hungry(80.0));

	for (int32 I = 0; I < 60 * 30 && Village.FindNpc(A)->MealsTaken == 0; ++I)
	{
		Sim.Tick(Dt);
	}
	TestTrue(TEXT("l'hote fait manger l'habitant au grenier"), Village.FindNpc(A)->MealsTaken > 0);
	TestEqual(TEXT("grenier 6 -> 5"), Village.FindBuilding(Granary)->FoodPhysical, 5);
	AddInfo(FString::Printf(TEXT("grenier %s en (%d,%d), t=%.3f, faim=%.3f"), *Granary, GX, GY, Sim.GetTime(), Village.FindNpc(A)->Needs.Hunger));
	return true;
}

#endif
