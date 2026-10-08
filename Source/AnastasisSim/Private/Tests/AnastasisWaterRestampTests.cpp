#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// ecart n°51 (water-network-001) -- RestampWater / ApplyWaterMask : l'eau du monde reecrite d'apres un masque.

namespace AnastasisWaterRestampTest
{
	using namespace AnastasisWorld;

	FWorld MakeWorld(int32 W, int32 H)
	{
		FWorld World;
		World.W = W;
		World.H = H;
		World.Tiles.SetNum(W * H);
		for (int32 I = 0; I < W * H; ++I)
		{
			FTile& T = World.Tiles[I];
			T.X = I % W;
			T.Y = I / W;
			T.Type = ETileType::Grass;
			T.Alt = 0.4;
		}
		return World;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWaterRestampTest,
	"Anastasis.Sim.Monde.Eau.Reseau.Restamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWaterRestampTest::RunTest(const FString&)
{
	using namespace AnastasisWaterRestampTest;
	FWorld World = MakeWorld(9, 5);
	// Une ancienne tranchee d'eau en colonne 1, une foret en colonne 6 que la riviere traverse.
	for (int32 Y = 0; Y < 5; ++Y)
	{
		FTile& Trench = World.Tiles[Y * 9 + 1];
		Trench.Type = ETileType::Water;
		Trench.Alt = SeaLevel - 0.1;
		FTile& Forest = World.Tiles[Y * 9 + 6];
		Forest.Type = ETileType::Forest;
		Forest.Resource = EResource::Wood;
		Forest.Amount = 30;
	}
	TArray<uint8> Mask;
	Mask.SetNumZeroed(9 * 5);
	for (int32 Y = 0; Y < 5; ++Y) Mask[Y * 9 + 6] = 1; // le reseau passe en colonne 6

	TestEqual(TEXT("un masque de mauvaise taille est refuse"), RestampWater(World, TArray<uint8>()), -1);
	TestEqual(TEXT("dix tuiles changent (5 rendues a la terre, 5 devenues eau)"), RestampWater(World, Mask), 10);
	for (int32 Y = 0; Y < 5; ++Y)
	{
		const FTile& Old = World.Tiles[Y * 9 + 1];
		const FTile& River = World.Tiles[Y * 9 + 6];
		TestTrue(TEXT("ancienne tranchee : terre"), Old.Type == ETileType::Grass);
		TestTrue(TEXT("ancienne tranchee : au-dessus de la mer"), Old.Alt > SeaLevel);
		TestTrue(TEXT("riviere : eau"), River.Type == ETileType::Water);
		TestTrue(TEXT("riviere : plus de bois"), River.Resource == EResource::None && River.Amount == 0);
		TestEqual(TEXT("riviere : rive 0, humidite 1"), River.Shore + River.Wetness, 1.0);
		// Une tuile a 1 de l'eau : formules de la generation.
		const FTile& Bank = World.Tiles[Y * 9 + 5];
		TestEqual(TEXT("berge : Shore = 1 - (1 - 0,4) / 4,6"), Bank.Shore, 1.0 - (1.0 - 0.4) / 4.6);
		TestEqual(TEXT("berge : Wetness = 1 - 1 / 6,5"), Bank.Wetness, 1.0 - 1.0 / 6.5);
		TestEqual(TEXT("ancienne tranchee loin de l'eau : plus de rive"), World.Tiles[Y * 9 + 0].Shore, 0.0);
	}
	TestEqual(TEXT("idempotent"), RestampWater(World, Mask), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisApplyWaterMaskTest,
	"Anastasis.Sim.Monde.Eau.Reseau.Simulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisApplyWaterMaskTest::RunTest(const FString&)
{
	FAnastasisSimulation Sim;
	Sim.Reset(12345u, 96, 96);
	const int32 N = Sim.GetWorld().Tiles.Num();
	TArray<uint8> Mask;
	Mask.SetNumZeroed(N);
	for (int32 X = 0; X < 96; ++X) Mask[40 * 96 + X] = 1; // une riviere est-ouest en ligne 40
	const int32 Changed = Sim.ApplyWaterMask(Mask);
	TestTrue(TEXT("des tuiles changent"), Changed > 0);
	int32 Mismatch = 0;
	for (int32 I = 0; I < N; ++I)
	{
		if ((Sim.GetWorld().Tiles[I].Type == AnastasisWorld::ETileType::Water) != (Mask[I] != 0)) ++Mismatch;
	}
	TestEqual(TEXT("l'eau du monde est celle du masque"), Mismatch, 0);
	TestTrue(TEXT("la navigation suit : la riviere bloque"), Sim.GetVillage().IsBlocked(10.5, 40.5));
	int32 FreedBlocked = 0;
	for (int32 I = 0; I < N; ++I)
	{
		if (Mask[I] == 0 && Sim.GetVillage().IsBlocked(I % 96 + 0.5, I / 96 + 0.5)) ++FreedBlocked;
	}
	TestEqual(TEXT("... et aucune ancienne eau ne bloque plus"), FreedBlocked, 0);
	Sim.GetVillage().SpawnNpc(20.5, 20.5, AnastasisNeeds::FNeeds());
	TestEqual(TEXT("refuse quand le village a des habitants"), Sim.ApplyWaterMask(Mask), -1);
	AddInfo(FString::Printf(TEXT("WATER_RESTAMP changed=%d"), Changed));
	return true;
}

#endif
