#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// La maison, de bout en bout, sur des mondes poses a la main.
//
//   la nuit tombe -> `rest` gagne (phaseBias) -> foyer / abri / logement libre ->
//   seuil -> A* -> ENTREE (npc.inside) -> tickNeeds branche sommeil ->
//   a `until` : satisfyRest puis SORTIE -> redecision
//
// Les fonctions pures (besoins, rythme, qualite du lit) sont prouvees bit a bit
// par Anastasis.Sim.Parite.Besoins / .Rythme. Ici on prouve l'ASSEMBLAGE.

namespace AnastasisVillageHouseTest
{
	using namespace AnastasisVillage;

	constexpr double Dt = 1.0 / 60.0; // SIM_FIXED_DT
	/** 0,9 jour = 21h36 : la nuit, jusqu'a 5h (1,2083 jour). */
	constexpr double NightTime = 0.9 * AnastasisRhythm::DayLength;
	/** 0,42 jour = 10h : le matin, l'heure de depart de la reference. */
	constexpr double MorningTime = 0.42 * AnastasisRhythm::DayLength;

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

	AnastasisNeeds::FNeeds Tired(double Energy)
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 10.0;
		N.Energy = Energy;
		N.Social = 70.0;
		N.Leisure = 70.0;
		N.Hygiene = 60.0;
		N.Thirst = 5.0;
		N.Health = 90.0;
		N.Morale = 55.0;
		return N;
	}

	/** Avance la simulation ; rend false si un habitant DEHORS s'est retrouve sur une case bloquee. */
	bool Run(FVillage& Village, double& Time, double Seconds, TFunctionRef<bool()> Stop)
	{
		const int32 Ticks = FMath::CeilToInt32(Seconds / Dt);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Time += Dt;
			Village.UpdateActors(Time, Dt);
			for (const FNpc& Npc : Village.GetActors())
			{
				if (!Npc.Inside.bActive && Village.IsFootBlocked(Npc.X, Npc.Y))
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

	bool IsAccessPointOf(const FBuilding& B, const FPoint& P)
	{
		for (const FPoint& A : B.AccessPoints)
		{
			if (A.X == P.X && A.Y == P.Y) return true;
		}
		return false;
	}

	/** Aucune reference vers ce batiment, sous aucune forme. */
	bool NobodyReferences(const FVillage& Village, const FString& Id)
	{
		for (const FNpc& N : Village.GetActors())
		{
			if (N.HomeId == Id || N.ShelterId == Id || N.DestBuildingId == Id) return false;
			if (N.Inside.bActive && N.Inside.BuildingId == Id) return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageHouseRegistrationTest,
	"Anastasis.Sim.Village.Maison.Enregistrement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageHouseRegistrationTest::RunTest(const FString&)
{
	using namespace AnastasisVillageHouseTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
	FVillage Village;
	Village.Bind(World);

	const FString House = Village.AddBuilding(HouseType, 10, 10);
	const FString Well = Village.AddBuilding(WellType, 20, 20);
	TestEqual(TEXT("identifiant de la reference"), House, FString(TEXT("building-0")));
	TestEqual(TEXT("countBuildings(house)"), Village.CountBuildings(HouseType), 1);
	TestTrue(TEXT("case de la maison bloquee"), Village.IsFootBlocked(10.5, 10.5));
	const FBuilding* B = Village.FindBuilding(House);
	TestEqual(TEXT("ACCESS_BUDGET.house = 2 seuils"), B->AccessPoints.Num(), 2);
	TestEqual(TEXT("capacite phase 1 = 3"), Village.ShelterCapacity(*B), 3);
	TestTrue(TEXT("sans proprietaire"), B->Owner.IsEmpty());
	TestEqual(TEXT("housing(house) = 3"), HousingOfType(HouseType), 3);
	TestEqual(TEXT("housing(well) = 0"), HousingOfType(WellType), 0);

	const FString A = Village.SpawnNpc(4.5, 4.5, Tired(80.0));
	const FString C = Village.SpawnNpc(5.5, 4.5, Tired(80.0));
	const uint64 Before = Village.Digest();
	TestTrue(TEXT("assignHomeToHousehold (sans famille)"), Village.AssignHome(A, House));
	TestEqual(TEXT("proprietaire"), Village.FindBuilding(House)->Owner, A);
	TestEqual(TEXT("foyer par identifiant"), Village.FindNpc(A)->HomeId, House);
	TestTrue(TEXT("moral +12 exact"), Village.FindNpc(A)->Needs.Morale == 67.0);
	TestEqual(TEXT("occupants (foyer ou abri)"), Village.CountShelterOccupants(House), 1);
	TestTrue(TEXT("l'empreinte voit le foyer"), Village.Digest() != Before);
	TestFalse(TEXT("maison d'autrui refusee"), Village.AssignHome(C, House));
	TestFalse(TEXT("un puits n'est pas un foyer"), Village.AssignHome(C, Well));
	TestTrue(TEXT("deja a lui : accepte"), Village.AssignHome(A, House));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageHouseSelectionTest,
	"Anastasis.Sim.Village.Maison.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageHouseSelectionTest::RunTest(const FString&)
{
	using namespace AnastasisVillageHouseTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);

	// La nuit : le proprietaire rentre chez lui, le sans-toit vise un logement libre.
	{
		FVillage Village;
		Village.Bind(World);
		const FString Owned = Village.AddBuilding(HouseType, 20, 10);
		const FString Free = Village.AddBuilding(HouseType, 20, 22);
		const FString A = Village.SpawnNpc(6.5, 10.5, Tired(80.0));
		const FString B = Village.SpawnNpc(6.5, 12.5, Tired(80.0));
		Village.AssignHome(A, Owned);

		double Time = NightTime;
		Run(Village, Time, 0.3, [] { return false; });
		const FNpc* NA = Village.FindNpc(A);
		const FNpc* NB = Village.FindNpc(B);

		TestEqual(TEXT("nuit : phase tracee"), NA->LastDecision.Phase, FString(TEXT("night")));
		TestEqual(TEXT("nuit, energie 80 : rest quand meme"), NA->Goal, FString(GoalRest));
		TestTrue(TEXT("nuit : la ligne rest bat le plancher"), NA->LastDecision.RestRowScore > NA->LastDecision.FloorScore);
		TestEqual(TEXT("proprietaire -> son foyer"), NA->DestBuildingId, Owned);
		TestEqual(TEXT("source : home"), NA->LastDecision.TargetSource, FString(TEXT("home")));
		TestTrue(TEXT("cible = un seuil de sa maison"), IsAccessPointOf(*Village.FindBuilding(Owned), NA->Target));

		TestEqual(TEXT("sans-toit : rest"), NB->Goal, FString(GoalRest));
		TestEqual(TEXT("sans-toit -> la maison LIBRE, jamais celle d'autrui"), NB->DestBuildingId, Free);
		TestEqual(TEXT("source : abri ouvert"), NB->LastDecision.TargetSource, FString(TEXT("open-shelter")));
		// phaseBias(rest) de nuit : +52 avec un toit, +30 sans. La trace le montre.
		TestTrue(TEXT("le toit pese +22 sur la ligne rest"),
			FMath::IsNearlyEqual(NA->LastDecision.RestRowScore - NB->LastDecision.RestRowScore, 22.0, 0.1));
	}

	// Le matin : on travaille (plancher), sauf a tomber d'epuisement.
	{
		FVillage Village;
		Village.Bind(World);
		const FString House = Village.AddBuilding(HouseType, 20, 10);
		const FString Fresh = Village.SpawnNpc(6.5, 10.5, Tired(80.0));
		const FString Spent = Village.SpawnNpc(6.5, 12.5, Tired(5.0));
		Village.AssignHome(Fresh, House);
		double Time = MorningTime;
		Run(Village, Time, 0.3, [] { return false; });
		const FNpc* NF = Village.FindNpc(Fresh);
		const FNpc* NS = Village.FindNpc(Spent);
		TestEqual(TEXT("matin : phase tracee"), NF->LastDecision.Phase, FString(TEXT("morning")));
		TestNotEqual(TEXT("matin, repose : pas de sieste"), NF->Goal, FString(GoalRest));
		TestTrue(TEXT("matin : le travail (plancher) l'emporte"), NF->LastDecision.RestRowScore <= NF->LastDecision.FloorScore);
		TestEqual(TEXT("matin, epuise : sieste"), NS->Goal, FString(GoalRest));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageHouseSleepTest,
	"Anastasis.Sim.Village.Maison.Sommeil",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageHouseSleepTest::RunTest(const FString&)
{
	using namespace AnastasisVillageHouseTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
	FVillage Village;
	Village.Bind(World);
	const FString House = Village.AddBuilding(HouseType, 18, 10);
	const FString A = Village.SpawnNpc(6.5, 10.5, Tired(40.0));
	Village.AssignHome(A, House);

	// 1. Il marche jusqu'a sa porte et ENTRE.
	double Time = NightTime;
	const bool bSafe = Run(Village, Time, 20.0, [&] { return Village.FindNpc(A)->Inside.bActive; });
	TestTrue(TEXT("jamais sur une case bloquee en marchant"), bSafe);
	const FNpc* Npc = Village.FindNpc(A);
	if (!TestTrue(TEXT("entre dans sa maison"), Npc->Inside.bActive))
	{
		return false;
	}
	TestEqual(TEXT("dedans : quel batiment"), Npc->Inside.BuildingId, House);
	TestEqual(TEXT("dedans : but rest"), Npc->Inside.Goal, FString(GoalRest));
	TestEqual(TEXT("dedans : « dort »"), Npc->Inside.Activity, FString(TEXT("dort")));
	TestTrue(TEXT("nuit : sleepDuration 11,5 s"), FMath::IsNearlyEqual(Npc->Inside.Until - Npc->Inside.EnteredAt, 11.5, 1e-9));
	TestTrue(TEXT("pose au seuil d'entree"), Npc->X == Npc->Inside.ExitX && Npc->Y == Npc->Inside.ExitY);
	TestFalse(TEXT("plus de cible une fois dedans"), Npc->bHasTarget);
	TestTrue(TEXT("qualite du lit : son foyer (1,12)"), FVillage::SleepQualityOf(*Npc) == AnastasisNeeds::Domestic::HomeRestBonus);
	TestTrue(TEXT("usager de sa maison"), Village.UsersOf(House).Contains(A));
	TestEqual(TEXT("dedans : liste"), Village.InsideOf(House).Num(), 1);

	// 2. Un tick dedans = la branche sommeil de tickNeeds, au bit pres.
	{
		const AnastasisNeeds::FNeeds Before = Npc->Needs;
		const double X = Npc->X;
		Time += Dt;
		Village.UpdateActors(Time, Dt);
		AnastasisNeeds::FNeeds Expected = Before;
		AnastasisNeeds::TickNeedsRestInside(Expected, Dt, true, AnastasisNeeds::Domestic::HomeRestBonus);
		Npc = Village.FindNpc(A);
		TestTrue(TEXT("tick dedans : energie exacte"), Npc->Needs.Energy == Expected.Energy);
		TestTrue(TEXT("tick dedans : faim exacte"), Npc->Needs.Hunger == Expected.Hunger);
		TestTrue(TEXT("tick dedans : soif exacte"), Npc->Needs.Thirst == Expected.Thirst);
		TestTrue(TEXT("tick dedans : sante exacte"), Npc->Needs.Health == Expected.Health);
		TestTrue(TEXT("dedans : immobile"), Npc->X == X);
	}

	// 3. A `until` : satisfyRest DEDANS, puis sortie. Le tick de sortie, au bit pres.
	AnastasisNeeds::FNeeds LastInside;
	AnastasisNeeds::FNeeds AfterExit;
	double ExitX = 0.0;
	double ExitY = 0.0;
	bool bExited = false;
	Run(Village, Time, 15.0, [&]
	{
		const FNpc* N = Village.FindNpc(A);
		if (!N->Inside.bActive)
		{
			AfterExit = N->Needs;
			bExited = true;
			return true;
		}
		LastInside = N->Needs;
		ExitX = N->Inside.ExitX;
		ExitY = N->Inside.ExitY;
		return false;
	});
	if (!TestTrue(TEXT("sort apres son sommeil"), bExited))
	{
		return false;
	}
	Npc = Village.FindNpc(A);
	AnastasisNeeds::FNeeds Expected = LastInside;
	AnastasisNeeds::TickNeedsRestInside(Expected, Dt, true, AnastasisNeeds::Domestic::HomeRestBonus);
	AnastasisNeeds::SatisfyRest(Expected, true, AnastasisNeeds::Domestic::HomeRestBonus, true, true);
	TestTrue(TEXT("sortie : energie exacte (bits)"), AfterExit.Energy == Expected.Energy);
	TestTrue(TEXT("sortie : loisir exact (bits)"), AfterExit.Leisure == Expected.Leisure);
	TestTrue(TEXT("sortie : moral exact (bits, +5 au foyer)"), AfterExit.Morale == Expected.Morale);
	TestTrue(TEXT("l'energie est reellement remontee"), AfterExit.Energy > 90.0);
	TestEqual(TEXT("un repos accompli"), Npc->RestsTaken, 1);
	TestTrue(TEXT("ressort par son seuil d'entree"), Npc->X == ExitX && Npc->Y == ExitY);
	TestFalse(TEXT("dehors, sur une case libre"), Village.IsFootBlocked(Npc->X, Npc->Y));
	AddInfo(FString::Printf(TEXT("energie 40 -> %.3f apres une nuit de 11,5 s"), AfterExit.Energy));

	// 4. Toujours la nuit : il redecide, et se recouche aussitot.
	Run(Village, Time, 1.0, [&] { return Village.FindNpc(A)->Inside.bActive; });
	TestTrue(TEXT("la nuit continue : il se recouche"), Village.FindNpc(A)->Inside.bActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageHouseCrowdTest,
	"Anastasis.Sim.Village.Maison.MultiAgents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageHouseCrowdTest::RunTest(const FString&)
{
	using namespace AnastasisVillageHouseTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 40);
	FVillage Village;
	Village.Bind(World);
	const FString House = Village.AddBuilding(HouseType, 20, 20);
	TArray<FString> Ids;
	const double Spots[4][2] = { { 12.5, 20.5 }, { 28.5, 20.5 }, { 20.5, 12.5 }, { 20.5, 28.5 } };
	for (const auto& S : Spots)
	{
		Ids.Add(Village.SpawnNpc(S[0], S[1], Tired(50.0)));
	}

	// Minuit : `assignSheltersDaily` loge trois sans-toit, le quatrieme reste dehors
	// (la boucle s'arrete au premier echec, comme la reference).
	TestEqual(TEXT("trois abrites"), Village.AssignSheltersDaily(), 3);
	TestEqual(TEXT("capacite respectee a l'attribution"), Village.CountShelterOccupants(House), 3);
	TestTrue(TEXT("le quatrieme n'a pas d'abri"), Village.FindNpc(Ids[3])->ShelterId.IsEmpty());
	for (int32 I = 0; I < 3; ++I)
	{
		TestEqual(TEXT("abri par identifiant"), Village.FindNpc(Ids[I])->ShelterId, House);
		TestTrue(TEXT("moral +3 a l'attribution"), Village.FindNpc(Ids[I])->Needs.Morale == 58.0);
	}

	// La nuit, les quatre y vont. La reference ne controle PAS la capacite a l'entree :
	// le quatrieme entre aussi, et dort moins bien (0,42 contre 0,78).
	double Time = NightTime;
	int32 MaxInside = 0;
	bool bQualitiesRight = true;
	bool bUsersConsistent = true;
	const bool bSafe = Run(Village, Time, 25.0, [&]
	{
		const TArray<FString> Inside = Village.InsideOf(House);
		MaxInside = FMath::Max(MaxInside, Inside.Num());
		for (const FNpc& N : Village.GetActors())
		{
			if (!N.Inside.bActive) continue;
			const double Expected = N.ShelterId == House ? AnastasisNeeds::Domestic::ShelterRestFactor : AnastasisNeeds::Domestic::OutdoorRestFactor;
			bQualitiesRight &= FVillage::SleepQualityOf(N) == Expected;
		}
		int32 Expected = 0;
		for (const FNpc& N : Village.GetActors())
		{
			const bool bHeading = (N.Goal == GoalRest || N.Goal == GoalDrink) && N.DestBuildingId == House;
			Expected += bHeading || (N.Inside.bActive && N.Inside.BuildingId == House) ? 1 : 0;
		}
		bUsersConsistent &= Village.UsersOf(House).Num() == Expected;
		return MaxInside == 4;
	});
	TestTrue(TEXT("jamais sur une case bloquee"), bSafe);
	TestEqual(TEXT("quatre dedans a la fois : pas de controle de capacite a l'entree"), MaxInside, 4);
	TestTrue(TEXT("qualite du lit : abri 0,78, sans abri 0,42"), bQualitiesRight);
	TestTrue(TEXT("usagers = ceux qui y vont + ceux qui y sont, a chaque tick"), bUsersConsistent);
	TestEqual(TEXT("le quatrieme vise la maison par nearestHousing"), Village.FindNpc(Ids[3])->Inside.BuildingId, House);

	// Un habitant retire EN PLEIN SOMMEIL : les autres dorment, la maison reste intacte.
	TestTrue(TEXT("retrait d'un dormeur"), Village.RemoveNpc(Ids[1]));
	TestEqual(TEXT("trois dedans"), Village.InsideOf(House).Num(), 3);
	TestEqual(TEXT("plus que deux abrites"), Village.CountShelterOccupants(House), 2);
	const FBuilding* B = Village.FindBuilding(House);
	TestTrue(TEXT("maison intacte : toujours libre"), B->Owner.IsEmpty());
	TestEqual(TEXT("maison intacte : phase 1"), B->HousePhase, 1);
	const bool bSafeAfter = Run(Village, Time, 15.0, [] { return false; });
	TestTrue(TEXT("apres retrait : sain"), bSafeAfter);
	for (const FNpc& N : Village.GetActors())
	{
		TestTrue(*FString::Printf(TEXT("%s a dormi"), *N.Id), N.RestsTaken > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageHouseRemovalTest,
	"Anastasis.Sim.Village.Maison.Destruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageHouseRemovalTest::RunTest(const FString&)
{
	using namespace AnastasisVillageHouseTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	const FString Home = Village.AddBuilding(HouseType, 14, 10);
	const FString Shelter = Village.AddBuilding(HouseType, 30, 20);
	const FString A = Village.SpawnNpc(8.5, 10.5, Tired(40.0));
	const FString B = Village.SpawnNpc(6.5, 20.5, Tired(40.0));
	Village.AssignHome(A, Home);
	Village.AssignSheltersDaily();
	TestEqual(TEXT("B abrite dans la maison libre"), Village.FindNpc(B)->ShelterId, Shelter);

	double Time = NightTime;
	Run(Village, Time, 20.0, [&] { return Village.FindNpc(A)->Inside.bActive; });
	if (!TestTrue(TEXT("A dort chez lui"), Village.FindNpc(A)->Inside.bActive))
	{
		return false;
	}
	TestTrue(TEXT("B en route vers son abri"), Village.FindNpc(B)->DestBuildingId == Shelter && !Village.FindNpc(B)->Inside.bActive);

	// Demolir la maison OCCUPEE, et l'abri VISE.
	const int32 Version = Village.GetNavVersion();
	TestTrue(TEXT("demolition du foyer occupe"), Village.RemoveBuilding(Home));
	TestTrue(TEXT("demolition de l'abri vise"), Village.RemoveBuilding(Shelter));
	TestEqual(TEXT("deux versions de navigation"), Village.GetNavVersion(), Version + 2);
	TestTrue(TEXT("aucune reference au foyer"), NobodyReferences(Village, Home));
	TestTrue(TEXT("aucune reference a l'abri"), NobodyReferences(Village, Shelter));
	const FNpc* NA = Village.FindNpc(A);
	TestFalse(TEXT("A n'est plus dedans"), NA->Inside.bActive);
	TestFalse(TEXT("A est dehors sur une case libre"), Village.IsFootBlocked(NA->X, NA->Y));
	TestFalse(TEXT("la case du foyer est rendue au terrain"), Village.IsFootBlocked(14.5, 10.5));

	// La nuit continue : sans toit ni logement, ils se reposent DEHORS (0,42).
	const bool bSafe = Run(Village, Time, 8.0, [&]
	{
		return Village.FindNpc(A)->RestsTaken > 0 && Village.FindNpc(B)->RestsTaken > 0;
	});
	TestTrue(TEXT("sans maison : jamais sur une case bloquee"), bSafe);
	TestTrue(TEXT("A se repose dehors (son sommeil interrompu ne comptait pas)"), Village.FindNpc(A)->RestsTaken > 0);
	TestTrue(TEXT("B se repose dehors"), Village.FindNpc(B)->RestsTaken > 0);
	TestEqual(TEXT("source : le camp"), Village.FindNpc(B)->LastDecision.TargetSource, FString(TEXT("settlement")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageHouseUnreachableTest,
	"Anastasis.Sim.Village.Maison.Inaccessible",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageHouseUnreachableTest::RunTest(const FString&)
{
	using namespace AnastasisVillageHouseTest;
	// Un fleuve coupe le monde : le foyer est de l'autre cote.
	AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
	for (int32 Y = 0; Y < 32; ++Y)
	{
		World.Tiles[Y * 32 + 16].Type = AnastasisWorld::ETileType::Water;
	}
	FVillage Village;
	Village.Bind(World);
	const FString Home = Village.AddBuilding(HouseType, 24, 10);
	const FString A = Village.SpawnNpc(6.5, 10.5, Tired(40.0));
	Village.AssignHome(A, Home);

	double Time = NightTime;
	bool bStayed = true;
	bool bEverInside = false;
	const bool bSafe = Run(Village, Time, 20.0, [&]
	{
		const FNpc* N = Village.FindNpc(A);
		bStayed &= N->X < 16.0;
		bEverInside |= N->Inside.bActive;
		return false;
	});
	const FNpc* Npc = Village.FindNpc(A);
	TestTrue(TEXT("jamais sur une case bloquee"), bSafe);
	TestTrue(TEXT("ne traverse pas le fleuve"), bStayed);
	TestFalse(TEXT("n'entre jamais chez lui"), bEverInside);
	// `redirectDomesticDoorFailure` : le repos se fait dehors, puis il abandonne.
	TestTrue(TEXT("porte inaccessible : repos dehors"), Npc->RestsTaken > 0);
	TestEqual(TEXT("le foyer reste le sien"), Npc->HomeId, Home);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageHouseHostTest,
	"Anastasis.Sim.Village.Maison.Hote",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageHouseHostTest::RunTest(const FString&)
{
	using namespace AnastasisVillageHouseTest;
	// Le vrai hote, le vrai monde canonique : du matin jusqu'apres minuit.
	FAnastasisSimulation Sim;
	Sim.Reset(12345u, 96, 96);
	FVillage& Village = Sim.GetVillage();

	auto PlaceNear = [&](int32 CX, int32 CY)
	{
		for (int32 R = 0; R < 30; ++R)
		{
			for (int32 DY = -R; DY <= R; ++DY)
			{
				for (int32 DX = -R; DX <= R; ++DX)
				{
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (Village.IsFootBlocked(X + 0.5, Y + 0.5) || Village.IsFootBlocked(X + 1.5, Y + 0.5)) continue;
					const FString Id = Village.AddBuilding(HouseType, X, Y);
					if (!Id.IsEmpty()) return Id;
				}
			}
		}
		return FString();
	};
	const FString Home = PlaceNear(48, 48);
	const FString Free = PlaceNear(56, 48);
	if (!TestFalse(TEXT("deux maisons posees"), Home.IsEmpty() || Free.IsEmpty()))
	{
		return false;
	}
	const FPoint Door = Village.FindBuilding(Home)->AccessPoints[0];
	const FString Owner = Village.SpawnNpc(Door.X, Door.Y, Tired(70.0));
	const FString Homeless = Village.SpawnNpc(Door.X, Door.Y, Tired(70.0));
	Village.AssignHome(Owner, Home);
	TestTrue(TEXT("avant minuit : sans abri"), Village.FindNpc(Homeless)->ShelterId.IsEmpty());

	bool bSleptAtNight = false;
	const int32 Day0 = Sim.GetDay();
	for (int32 I = 0; I < 60 * 60 && Sim.GetDay() == Day0; ++I)
	{
		Sim.Tick(Dt);
		const FNpc* N = Village.FindNpc(Owner);
		bSleptAtNight |= N->Inside.bActive && AnastasisRhythm::IsNightPhase(Sim.GetTime());
	}
	TestTrue(TEXT("l'hote a passe minuit"), Sim.GetDay() > Day0);
	TestTrue(TEXT("le proprietaire a dormi chez lui la nuit"), bSleptAtNight);
	TestTrue(TEXT("minuit : assignSheltersDaily abrite le sans-toit"), Village.FindNpc(Homeless)->ShelterId == Free);
	AddInfo(FString::Printf(TEXT("t=%.3f jour=%d, repos proprietaire=%d, energie=%.3f"),
		Sim.GetTime(), Sim.GetDay(), Village.FindNpc(Owner)->RestsTaken, Village.FindNpc(Owner)->Needs.Energy));
	return true;
}

#endif
