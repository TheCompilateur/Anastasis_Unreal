#include "Misc/AutomationTest.h"

#include "Core/AnastasisSimMath.h"
#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisFields.h"
#include "Work/AnastasisGather.h"
#include "World/AnastasisPathfinding.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// La repousse des champs, puis le village qui tient plusieurs jours.
//
// Repousse.* : l'assemblage de `regrowFieldsDaily` (la fonction pure est prouvee
// bit a bit par Anastasis.Sim.Parite.Repousse) : jachere qui repart, plafond,
// extension food-supply jamais regarnie, file de minuit de l'hote.
//
// Endurance : puits, maison, grenier, fermiers et sans-metier ensemble sur le
// monde canonique, plusieurs jours. Ce n'est pas une trajectoire JS (la table
// reste reduite) : c'est la preuve que les quatre tranches forment une boucle
// qui ne meurt pas, et la mesure de ce qu'elle fait.

namespace AnastasisVillageEnduranceTest
{
	using namespace AnastasisVillage;
	namespace G = AnastasisGather;
	namespace F = AnastasisFields;

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

	void SetField(AnastasisWorld::FWorld& World, int32 X, int32 Y, int32 Amount, AnastasisWorld::ECropId Crop, double Fertility = 1.0)
	{
		AnastasisWorld::FTile& Tile = World.Tiles[Y * World.W + X];
		Tile.Type = AnastasisWorld::ETileType::Field;
		Tile.Resource = Amount > 0 ? AnastasisWorld::EResource::Food : AnastasisWorld::EResource::None;
		Tile.Amount = Amount;
		Tile.CropId = Crop;
		Tile.Fertility = Fertility;
	}

	/**
	 * Les seules tuiles qui peuvent porter de la nourriture : champs et tuiles de
	 * nourriture generees (la recolte laisse un champ, la repousse ne touche que
	 * les champs). Les lister une fois evite de balayer toute la carte a chaque tick.
	 */
	TArray<FIntPoint> FoodCapable(const AnastasisWorld::FWorld& World)
	{
		TArray<FIntPoint> Out;
		for (const AnastasisWorld::FTile& Tile : World.Tiles)
		{
			if (Tile.Type == AnastasisWorld::ETileType::Field || Tile.Resource == AnastasisWorld::EResource::Food) Out.Add(FIntPoint(Tile.X, Tile.Y));
		}
		return Out;
	}

	int32 FieldFood(const FVillage& Village, const TArray<FIntPoint>& Capable)
	{
		int32 Total = 0;
		for (const FIntPoint& P : Capable)
		{
			const AnastasisWorld::FTile Tile = Village.LiveTileAt(P.X, P.Y);
			if (Tile.Resource == AnastasisWorld::EResource::Food) Total += Tile.Amount;
		}
		return Total;
	}

	int32 FieldFood(const FVillage& Village, const AnastasisWorld::FWorld& World)
	{
		return FieldFood(Village, FoodCapable(World));
	}

	/** Toute la nourriture presente ou mangee, moins ce que la repousse a cree. */
	int64 FoodLedger(const FVillage& Village, const TArray<FIntPoint>& Capable)
	{
		int64 Total = FieldFood(Village, Capable);
		for (const FNpc& Npc : Village.GetActors()) Total += Npc.InventoryFood + Npc.MealsTaken;
		for (const FBuilding& B : Village.GetBuildings()) Total += B.FoodPhysical;
		return Total - Village.GetRegrownFood();
	}

	AnastasisNeeds::FNeeds Needs(double Hunger)
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = Hunger;
		N.Energy = 85.0;
		N.Social = 80.0;
		N.Leisure = 80.0;
		N.Hygiene = 80.0;
		N.Thirst = 10.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		return N;
	}

	/** Premiere case libre a la distance de Chebyshev R de (CX, CY), hors champ, dont un seuil atteint Goal. */
	FString PlaceNear(FVillage& V, const AnastasisWorld::FWorld& W, const FString& Type, int32 CX, int32 CY, int32 RMin, int32 RMax, const FPoint& Goal)
	{
		const AnastasisPath::FWorldNavSource Nav(V.GetNavGrid(), W);
		for (int32 R = RMin; R <= RMax; ++R)
		{
			for (int32 DY = -R; DY <= R; ++DY)
			{
				for (int32 DX = -R; DX <= R; ++DX)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
					const int32 X = CX + DX;
					const int32 Y = CY + DY;
					if (X < 2 || Y < 2 || X > W.W - 3 || Y > W.H - 3) continue;
					if (V.LiveTileAt(X, Y).Resource != AnastasisWorld::EResource::None || V.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
					const FString Id = V.AddBuilding(Type, X, Y);
					if (Id.IsEmpty()) continue;
					for (const FPoint& Door : V.FindBuilding(Id)->AccessPoints)
					{
						TArray<FPoint> Path;
						if (AnastasisPath::FindPath(Nav, Door, Goal, {}, Path)) return Id;
					}
					V.RemoveBuilding(Id);
				}
			}
		}
		return FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisRegrowFallowTest,
	"Anastasis.Sim.Village.Repousse.Jachere",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisRegrowFallowTest::RunTest(const FString&)
{
	using namespace AnastasisVillageEnduranceTest;
	// Une parcelle videe par le fermier (jachere) repart : culture tiree, stock regagne.
	AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
	SetField(World, 8, 11, 4, AnastasisWorld::ECropId::Grain, 1.0);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
	const FString Id = Village.SpawnNpc(13.5, 11.5, Needs(10.0));
	Village.AssignWorkplace(Id, G::JobFarmer, Granary);
	double Time = 27.0;
	for (int32 I = 0; I < 60 * 25 && Village.LiveTileAt(8, 11).Amount > 0; ++I)
	{
		Time += Dt;
		Village.UpdateActors(Time, Dt);
	}
	AnastasisWorld::FTile Tile = Village.LiveTileAt(8, 11);
	if (!TestTrue(TEXT("parcelle videe, en jachere"), Tile.Amount == 0 && Tile.CropId == AnastasisWorld::ECropId::Fallow))
	{
		return false;
	}

	// Jour apres jour, le premier tirage qui passe la regarnit.
	int32 GrownDay = -1;
	for (int32 Day = 2; Day <= 40 && GrownDay < 0; ++Day)
	{
		const bool bExpected = F::RegrowRoll(8, 11, Day) <= F::DailyChance(Day);
		const int32 Grown = Village.RegrowFieldsDaily(Day);
		Tile = Village.LiveTileAt(8, 11);
		TestEqual(*FString::Printf(TEXT("jour %d : le tirage decide"), Day), Tile.Amount > 0, bExpected);
		if (Tile.Amount > 0)
		{
			GrownDay = Day;
			TestEqual(TEXT("une tuile regarnie"), Grown, 1);
			TestEqual(TEXT("3 x regen de saison (fertilite 1)"), Tile.Amount, F::RegenAmount(F::FieldRegenPerDay, Day));
			TestTrue(TEXT("de nouveau de la nourriture"), Tile.Resource == AnastasisWorld::EResource::Food);
			TestTrue(TEXT("sortie de jachere"), Tile.CropId == F::CropAfterFallow(8, 11, Day));
			TestEqual(TEXT("comptee comme repousse"), Village.GetRegrownFood(), static_cast<int64>(Tile.Amount));
		}
	}
	TestTrue(TEXT("repousse en moins de 40 jours"), GrownDay > 0);
	TestEqual(TEXT("le monde genere n'a pas bouge"), World.Tiles[11 * World.W + 8].Amount, 4);
	AddInfo(FString::Printf(TEXT("jachere regarnie au jour %d : %d portions"), GrownDay, Village.LiveTileAt(8, 11).Amount));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisRegrowCapTest,
	"Anastasis.Sim.Village.Repousse.Plafond",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisRegrowCapTest::RunTest(const FString&)
{
	using namespace AnastasisVillageEnduranceTest;
	// 200 jours sur une parcelle de 40 champs : jamais plus de 37, chaque saison compte.
	AnastasisWorld::FWorld World = MakeFlatWorld(32, 32);
	for (int32 Y = 4; Y < 8; ++Y)
	{
		for (int32 X = 4; X < 14; ++X)
		{
			SetField(World, X, Y, (X * 7 + Y * 3) % 38, (X + Y) % 2 ? AnastasisWorld::ECropId::Greens : AnastasisWorld::ECropId::Fallow, 0.5 + 0.1 * (X % 6));
		}
	}
	// Une source de l'extension food-supply : elle se declare sans repousse.
	AnastasisWorld::FTile& Grass = World.Tiles[20 * World.W + 20];
	Grass.Resource = AnastasisWorld::EResource::Food;
	Grass.Amount = 3;
	FVillage Village;
	Village.Bind(World);
	TestTrue(TEXT("source ouverte"), Village.ActivateFoodSource(20, 20));

	int32 Days = 0;
	int32 TotalGrown = 0;
	for (int32 Day = 1; Day <= 200; ++Day)
	{
		TotalGrown += Village.RegrowFieldsDaily(Day);
		++Days;
		for (int32 Y = 4; Y < 8; ++Y)
		{
			for (int32 X = 4; X < 14; ++X)
			{
				const AnastasisWorld::FTile Tile = Village.LiveTileAt(X, Y);
				if (Tile.Amount > F::FieldFoodCap)
				{
					AddError(FString::Printf(TEXT("jour %d : (%d,%d) a %d > 37"), Day, X, Y, Tile.Amount));
					return false;
				}
			}
		}
	}
	int32 AtCap = 0;
	for (int32 Y = 4; Y < 8; ++Y)
	{
		for (int32 X = 4; X < 14; ++X)
		{
			AtCap += Village.LiveTileAt(X, Y).Amount == F::FieldFoodCap ? 1 : 0;
		}
	}
	TestEqual(TEXT("apres 200 jours, tout est au plafond"), AtCap, 40);
	TestEqual(TEXT("la source food-supply n'a jamais repousse"), Village.GetFoodSources()[0].Remaining, 3);
	TestTrue(TEXT("des repousses"), TotalGrown > 0);
	AddInfo(FString::Printf(TEXT("%d jours, %d repousses, %lld portions creees"), Days, TotalGrown, Village.GetRegrownFood()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisRegrowHostTest,
	"Anastasis.Sim.Village.Repousse.Hote",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisRegrowHostTest::RunTest(const FString&)
{
	using namespace AnastasisVillageEnduranceTest;
	// La file de minuit de l'hote : `landRegen` passe au tick du changement de jour.
	FAnastasisSimulation A;
	A.Reset(12345u, 96, 96);
	FAnastasisSimulation B;
	B.Reset(12345u, 96, 96);
	const int32 FieldsBefore = FieldFood(A.GetVillage(), A.GetWorld());
	int32 Midnights = 0;
	int32 Grown = 0;
	for (int32 I = 0; I < 60 * 90 * 3; ++I)
	{
		const int32 DayBefore = A.GetDay();
		A.Tick(Dt);
		B.Tick(Dt);
		if (A.GetDay() != DayBefore)
		{
			++Midnights;
			Grown += A.GetLastRegrownFields();
			TestEqual(TEXT("file de minuit videe au meme tick"), A.GetDeferredRemaining(), 0);
		}
	}
	TestEqual(TEXT("trois minuits"), Midnights, 3);
	TestTrue(TEXT("des champs du monde canonique ont repousse"), Grown > 0);
	TestEqual(TEXT("portions creees = gain des champs (personne ne cueille)"),
		static_cast<int64>(FieldFood(A.GetVillage(), A.GetWorld()) - FieldsBefore), A.GetVillage().GetRegrownFood());
	TestTrue(TEXT("deterministe"), A.GetVillage().Digest() == B.GetVillage().Digest());
	AddInfo(FString::Printf(TEXT("3 jours : %d repousses, %lld portions ; champs %d -> %d"),
		Grown, A.GetVillage().GetRegrownFood(), FieldsBefore, FieldFood(A.GetVillage(), A.GetWorld())));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageEnduranceTest,
	"Anastasis.Sim.Village.Endurance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageEnduranceTest::RunTest(const FString&)
{
	using namespace AnastasisVillageEnduranceTest;
	FAnastasisSimulation Sim;
	Sim.Reset(12345u, 96, 96);
	FVillage& V = Sim.GetVillage();
	const AnastasisWorld::FWorld& W = Sim.GetWorld();

	// Le champ genere le plus proche du centre ; grenier, puits, maison autour.
	int32 FX = -1;
	int32 FY = -1;
	double Best = AnastasisNav::Infinity;
	for (const AnastasisWorld::FTile& Tile : W.Tiles)
	{
		if (Tile.Resource != AnastasisWorld::EResource::Food || Tile.Amount <= 0) continue;
		const double D = AnastasisMath::Dist(Tile.X, Tile.Y, 48.0, 48.0);
		if (D < Best)
		{
			Best = D;
			FX = Tile.X;
			FY = Tile.Y;
		}
	}
	const FPoint Field = { FX + 0.5, FY + 0.5 };
	const FString Granary = PlaceNear(V, W, GranaryType, FX, FY, 3, 4, Field);
	const FString Well = PlaceNear(V, W, WellType, FX, FY, 5, 8, Field);
	const FString House = PlaceNear(V, W, HouseType, FX, FY, 5, 9, Field);
	if (!TestFalse(TEXT("village pose"), Granary.IsEmpty() || Well.IsEmpty() || House.IsEmpty()))
	{
		return false;
	}
	const FPoint Door = V.FindBuilding(Granary)->AccessPoints[0];
	TArray<FString> Farmers;
	TArray<FString> Settlers;
	for (int32 K = 0; K < 2; ++K)
	{
		Farmers.Add(V.SpawnNpc(Door.X, Door.Y, Needs(10.0 + 5.0 * K)));
		V.AssignWorkplace(Farmers.Last(), G::JobFarmer, Granary);
	}
	for (int32 K = 0; K < 3; ++K)
	{
		Settlers.Add(V.SpawnNpc(Door.X, Door.Y, Needs(20.0 + 10.0 * K)));
	}

	const TArray<FIntPoint> Capable = FoodCapable(W);
	const int64 Ledger = FoodLedger(V, Capable);
	constexpr int32 DaysToRun = 8;
	TArray<FString> Rows;
	int32 LastDay = Sim.GetDay();
	int32 MealsAtDayStart = 0;
	int32 DeliveredAtDayStart = 0;
	int32 DaysWithoutMeal = 0;
	int32 DaysWithoutDelivery = 0;
	// Un jour sans livraison doit avoir une CAUSE : un fermier en besoin critique
	// (`workWillFactor` = 0 : aucun but de travail ne gagne par son score propre).
	int32 UnexplainedIdleDays = 0;
	int32 FirstIdleDay = -1;
	bool bFarmerCriticalToday = false;
	FString CriticalNeedToday;
	double MaxHunger = 0.0;
	double MaxThirst = 0.0;
	double MinHealth = 100.0;
	double MinEnergy = 100.0;
	const int32 Ticks = FMath::CeilToInt32(DaysToRun * FAnastasisSimulation::DayLength / Dt);
	for (int32 I = 0; I < Ticks; ++I)
	{
		Sim.Tick(Dt);
		if (FoodLedger(V, Capable) != Ledger)
		{
			AddError(FString::Printf(TEXT("t=%.3f : nourriture creee ou perdue (%lld != %lld)"), Sim.GetTime(), FoodLedger(V, Capable), Ledger));
			return false;
		}
		for (const FNpc& N : V.GetActors())
		{
			if (!N.Inside.bActive && V.IsFootBlocked(N.X, N.Y))
			{
				AddError(FString::Printf(TEXT("t=%.3f : %s sur une case bloquee"), Sim.GetTime(), *N.Id));
				return false;
			}
			MaxHunger = FMath::Max(MaxHunger, N.Needs.Hunger);
			MaxThirst = FMath::Max(MaxThirst, N.Needs.Thirst);
			MinHealth = FMath::Min(MinHealth, N.Needs.Health);
			MinEnergy = FMath::Min(MinEnergy, N.Needs.Energy);
		}
		for (const FString& Id : Farmers)
		{
			const AnastasisNeeds::FNeeds& N = V.FindNpc(Id)->Needs;
			if (!NeedsCritical(N)) continue;
			bFarmerCriticalToday = true;
			using namespace AnastasisNeeds::Constants;
			if (CriticalNeedToday.IsEmpty())
			{
				CriticalNeedToday = N.Social <= 100.0 - LonelyCritical ? TEXT("solitude")
					: N.Leisure <= 100.0 - BoredCritical ? TEXT("ennui")
					: N.Hygiene <= 100.0 - HygieneCritical ? TEXT("hygiene")
					: N.Hunger >= HungerCritical ? TEXT("faim")
					: N.Thirst >= ThirstCritical ? TEXT("soif")
					: N.Energy <= 100.0 - FatigueCritical ? TEXT("fatigue")
					: TEXT("sante ou moral");
			}
		}
		if (Sim.GetDay() != LastDay)
		{
			int32 Meals = 0;
			int32 Delivered = 0;
			for (const FNpc& N : V.GetActors())
			{
				Meals += N.MealsTaken;
				Delivered += N.DeliveredFood;
			}
			// Le premier jour est entame (on demarre le matin) : il compte comme les autres.
			DaysWithoutMeal += Meals == MealsAtDayStart ? 1 : 0;
			const bool bIdle = Delivered == DeliveredAtDayStart;
			DaysWithoutDelivery += bIdle ? 1 : 0;
			UnexplainedIdleDays += bIdle && !bFarmerCriticalToday ? 1 : 0;
			if (bIdle && FirstIdleDay < 0) FirstIdleDay = LastDay;
			Rows.Add(FString::Printf(TEXT("jour %d : grenier %d, repas +%d, livre +%d, champs %d, repousse %lld, faim max %.0f, sante min %.0f%s"),
				LastDay, V.FindBuilding(Granary)->FoodPhysical, Meals - MealsAtDayStart, Delivered - DeliveredAtDayStart,
				FieldFood(V, Capable), V.GetRegrownFood(), MaxHunger, MinHealth,
				bFarmerCriticalToday ? *FString::Printf(TEXT(", fermier en %s critique"), *CriticalNeedToday) : TEXT("")));
			bFarmerCriticalToday = false;
			CriticalNeedToday.Reset();
			for (const FString& Id : Farmers)
			{
				const FNpc* N = V.FindNpc(Id);
				int32 FoodSpots = 0;
				for (const FResourceSpot& Spot : N->Spots) FoodSpots += Spot.Resource == TEXT("food") ? 1 : 0;
				Rows.Add(FString::Printf(TEXT("    %s : but %s (%s), gisements nourriture %d / %d, sac %d, cueilli %d, livre %d, table gather %.1f plancher %s %.1f, facteur %.3f, pos (%.1f,%.1f)"),
					*N->Id, *N->Goal, *N->Activity, FoodSpots, N->Spots.Num(), N->InventoryFood, N->GatheredFood, N->DeliveredFood,
					N->LastDecision.GatherRowTable, *N->LastDecision.FloorGoal, N->LastDecision.FloorScore, N->LastDecision.WorkFactor, N->X, N->Y));
				Rows.Add(FString::Printf(TEXT("        besoins : faim %.0f soif %.0f energie %.0f social %.0f loisir %.0f hygiene %.0f sante %.0f moral %.0f, critique %s"),
					N->Needs.Hunger, N->Needs.Thirst, N->Needs.Energy, N->Needs.Social, N->Needs.Leisure, N->Needs.Hygiene, N->Needs.Health, N->Needs.Morale,
					NeedsCritical(N->Needs) ? TEXT("OUI") : TEXT("non")));
			}
			MealsAtDayStart = Meals;
			DeliveredAtDayStart = Delivered;
			LastDay = Sim.GetDay();
		}
	}
	for (const FString& Row : Rows) AddInfo(Row);
	AddInfo(FString::Printf(TEXT("sur %d jours : faim max %.1f, soif max %.1f, energie min %.1f, sante min %.1f"),
		DaysToRun, MaxHunger, MaxThirst, MinEnergy, MinHealth));

	TestEqual(TEXT("chaque jour, quelqu'un mange"), DaysWithoutMeal, 0);
	TestTrue(TEXT("personne ne meurt de faim ni de soif (sante > seuil critique)"), MinHealth > AnastasisNeeds::Constants::HealthCritical);
	TestTrue(TEXT("les fermiers livrent tant qu'ils peuvent travailler (les premiers jours)"), FirstIdleDay != 1);
	// LIMITE CONNUE de la table reduite : `socialize` n'est pas porte, le besoin social
	// descend sans remede ; a 35 la solitude devient critique et la reference coupe
	// tout travail (`workWillFactor` = 0). Le grenier ne recoit plus : c'est EXPLIQUE,
	// jamais silencieux. Le jour ou `socialize` sera porte, ce compteur doit tomber a 0.
	TestEqual(TEXT("aucun jour sans livraison n'est inexplique"), UnexplainedIdleDays, 0);
	AddInfo(FString::Printf(TEXT("jours sans livraison : %d (le premier : %d), tous expliques par un besoin critique d'un fermier"),
		DaysWithoutDelivery, FirstIdleDay));
	return true;
}

#endif
