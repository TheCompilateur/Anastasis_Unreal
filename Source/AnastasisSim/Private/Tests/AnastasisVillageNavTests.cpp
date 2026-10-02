#include "Misc/AutomationTest.h"

#include "Core/AnastasisSimMath.h"
#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisNavService.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// La marche du village par le service de navigation (nav-service-001).
//
// Le service lui-meme (file, cache, budget) est prouve par Anastasis.Sim.Parite.NavService et NavServiceFonctions ; ici,
// l'ASSEMBLAGE dans le pas des habitants :
//   1. la demande de chemin passe par le service et pose l'etat de `navigation` (cle de cible, version,
//      copie du chemin) comme `applyPathToActor` + `syncNavigationFromActor` ;
//   2. la file de porte : trois habitants vers le meme seuil, le plus proche passe (leader), les deux
//      autres attendent (waiter, rangs 1 et 2) et ne marchent pas vers le seuil.

namespace AnastasisVillageNavTest
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

	AnastasisNeeds::FNeeds Calm()
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 10.0;
		N.Energy = 80.0;
		N.Social = 80.0;
		N.Leisure = 80.0;
		N.Hygiene = 80.0;
		N.Thirst = 10.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageNavTest,
	"Anastasis.Sim.Village.Navigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageNavTest::RunTest(const FString&)
{
	using namespace AnastasisVillageNavTest;
	const AnastasisWorld::FWorld World = MakeFlatWorld(40, 40);
	FVillage Village;
	Village.Bind(World);
	const FString Well = Village.AddBuilding(WellType, 20, 20);
	if (!TestFalse(TEXT("puits pose"), Well.IsEmpty())) return false;
	const FPoint Door = Village.FindBuilding(Well)->AccessPoints[0];

	// Trois habitants en ligne derriere le seuil, du cote oppose au puits, a 1,0 / 1,6 / 2,2 cases.
	const double AwayX = Door.X - 20.5;
	const double AwayY = Door.Y - 20.5;
	const double Norm = FMath::Sqrt(AwayX * AwayX + AwayY * AwayY);
	const double UX = AwayX / Norm;
	const double UY = AwayY / Norm;
	const double Distances[] = { 1.0, 1.6, 2.2 };
	TArray<FString> Ids;
	for (const double D : Distances)
	{
		Ids.Add(Village.SpawnNpc(Door.X + UX * D, Door.Y + UY * D, Calm()));
	}
	// Une mise a jour pour poser horloge et phase, puis plus de pensee : la cible tient.
	double Time = 27.0;
	Village.UpdateActors(Time, Dt);
	for (const FString& Id : Ids)
	{
		FNpc* N = Village.FindNpcMutable(Id);
		N->AiThinkAt = 1e9;
		N->Goal = GoalDrink;
		N->bHasTarget = true;
		N->Target = Door;
		N->DestBuildingId = Well;
	}
	TArray<FPoint> Start;
	for (const FString& Id : Ids) Start.Add({ Village.FindNpc(Id)->X, Village.FindNpc(Id)->Y });
	Time += Dt;
	Village.UpdateActors(Time, Dt);

	// 1. La demande de chemin par le service.
	const FString DoorKey = AnastasisNavService::NavigationTargetKey(Door);
	for (const FString& Id : Ids)
	{
		const FNpc* N = Village.FindNpc(Id);
		TestEqual(FString::Printf(TEXT("%s : cle de cible"), *Id), N->NavTargetKey, DoorKey);
		TestEqual(FString::Printf(TEXT("%s : version de navigation"), *Id), N->NavVersion, Village.GetNavVersion());
		TestEqual(FString::Printf(TEXT("%s : navigation.path recopie le chemin"), *Id), N->NavPath.Num(), N->Path.Num());
		TestTrue(FString::Printf(TEXT("%s : le pas a tourne (hesitation posee)"), *Id), N->bHasHesitation);
	}

	// 2. La file de porte.
	const FNpc* First = Village.FindNpc(Ids[0]);
	const FNpc* Second = Village.FindNpc(Ids[1]);
	const FNpc* Third = Village.FindNpc(Ids[2]);
	TestEqual(TEXT("le plus proche passe"), First->DoorQueueRole, FString(TEXT("leader")));
	TestEqual(TEXT("le deuxieme attend"), Second->DoorQueueRole, FString(TEXT("waiter")));
	TestEqual(TEXT("rang du deuxieme"), Second->DoorQueueRank, 1);
	TestEqual(TEXT("le troisieme attend"), Third->DoorQueueRole, FString(TEXT("waiter")));
	TestEqual(TEXT("rang du troisieme"), Third->DoorQueueRank, 2);
	const double FirstGain = AnastasisMath::Dist(Start[0].X, Start[0].Y, Door.X, Door.Y) - AnastasisMath::Dist(First->X, First->Y, Door.X, Door.Y);
	const double ThirdGain = AnastasisMath::Dist(Start[2].X, Start[2].Y, Door.X, Door.Y) - AnastasisMath::Dist(Third->X, Third->Y, Door.X, Door.Y);
	TestTrue(TEXT("le leader avance vers le seuil"), FirstGain > 0.0);
	TestTrue(TEXT("le leader a une direction de marche"), First->bHasLastMoveDir);
	// Deja a moins de 0,28 de son point d'attente, le premier waiter reste sur place : aucun segment, pas de direction.
	TestTrue(TEXT("le premier waiter reste a son point d'attente"), Second->X == Start[1].X && Second->Y == Start[1].Y && !Second->bHasLastMoveDir);
	TestTrue(TEXT("le dernier waiter ne fonce pas vers le seuil"), ThirdGain < FirstGain);
	AddInfo(FString::Printf(TEXT("NAV_VILLAGE roles=%s/%s/%s rangs=%d/%d/%d gains=%.4f/%.4f"), *First->DoorQueueRole, *Second->DoorQueueRole,
		*Third->DoorQueueRole, First->DoorQueueRank, Second->DoorQueueRank, Third->DoorQueueRank, FirstGain, ThirdGain));
	return true;
}

#endif
