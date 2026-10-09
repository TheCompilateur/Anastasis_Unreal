#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisGather.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// faim-champs-001 (ecart n°59) -- quand le grenier se vide, des bras vont aux champs : l'adulte sans metier le plus
// affame, un par soir, jamais un enfant, jamais plus de la moitie du village ; rien quand il y a assez, rien eteint.

namespace AnastasisFieldHandsTest
{
	using namespace AnastasisVillage;

	AnastasisWorld::FWorld MakeWorld()
	{
		AnastasisWorld::FWorld World;
		World.W = 40;
		World.H = 40;
		World.Tiles.SetNum(40 * 40);
		for (int32 Y = 0; Y < 40; ++Y)
		{
			for (int32 X = 0; X < 40; ++X)
			{
				AnastasisWorld::FTile& Tile = World.Tiles[Y * 40 + X];
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
		return N;
	}

	struct FSetup
	{
		FString Granary;
		FString Child;
		TArray<FString> Adults; // par faim croissante
	};

	/** Un grenier vide et son cultivateur, six adultes sans metier (faim 20 a 70), un enfant tres affame. */
	FSetup Populate(FVillage& V, bool bEnabled)
	{
		FSetup S;
		V.SetFieldHandsEnabled(bEnabled);
		S.Granary = V.AddBuilding(GranaryType, 24, 20);
		const FString Farmer = V.SpawnNpc(22.5, 22.5, Hungry(10.0));
		V.AssignWorkplace(Farmer, AnastasisGather::JobFarmer, S.Granary);
		for (int32 I = 0; I < 6; ++I) S.Adults.Add(V.SpawnNpc(14.5 + I, 18.5, Hungry(20.0 + 10.0 * I)));
		S.Child = V.SpawnNpc(14.5, 24.5, Hungry(95.0));
		V.SetIdentity(S.Child, TEXT("Michael"), TEXT("Scribe"), TEXT("m"), 12.0);
		return S;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisFieldHandsTest,
	"Anastasis.Sim.Village.Faim.BrasAuxChamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisFieldHandsTest::RunTest(const FString&)
{
	using namespace AnastasisFieldHandsTest;
	const AnastasisWorld::FWorld World = MakeWorld();
	{
		FVillage V;
		V.Bind(World);
		const FSetup S = Populate(V, true);
		const FString First = V.UpdateFieldHandsDaily(1);
		TestEqual(TEXT("grenier vide : le plus affame des adultes part aux champs"), First, S.Adults.Last());
		TestEqual(TEXT("... il devient cultivateur"), V.FindNpc(First)->JobId, FString(AnastasisGather::JobFarmer));
		TestEqual(TEXT("... de ce grenier"), V.FindNpc(First)->WorkplaceId, S.Granary);
		TestEqual(TEXT("le soir suivant, le suivant"), V.UpdateFieldHandsDaily(2), S.Adults[S.Adults.Num() - 2]);
		for (int32 Day = 3; Day < 10; ++Day) V.UpdateFieldHandsDaily(Day);
		int32 Farmers = 0;
		for (const FNpc& N : V.GetActors()) if (N.JobId == AnastasisGather::JobFarmer) ++Farmers;
		TestEqual(TEXT("jamais plus de la moitie du village aux champs (8 habitants)"), Farmers, 4);
		TestEqual(TEXT("jamais un enfant"), V.FindNpc(S.Child)->JobId, FString(AnastasisGather::JobSettler));
		TestEqual(TEXT("trois embauches comptees"), V.GetFieldHandsHired(), 3);
		AddInfo(TEXT("FIELD_HANDS ") + V.GetLastFieldHandsDecision());
	}
	{
		FVillage V;
		V.Bind(World);
		const FSetup S = Populate(V, true);
		V.CreditFood(S.Granary, 100);
		TestTrue(TEXT("assez de reserves : personne ne part"), V.UpdateFieldHandsDaily(1).IsEmpty());
		TestTrue(TEXT("... et la decision le dit"), V.GetLastFieldHandsDecision().Contains(TEXT("assez")));
	}
	{
		FVillage V;
		V.Bind(World);
		Populate(V, false);
		TestTrue(TEXT("eteint (reference) : personne ne part"), V.UpdateFieldHandsDaily(1).IsEmpty());
		TestEqual(TEXT("... aucune embauche"), V.GetFieldHandsHired(), 0);
	}
	return true;
}

#endif
