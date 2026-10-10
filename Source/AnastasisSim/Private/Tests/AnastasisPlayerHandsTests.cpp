#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisBuild.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// mains-joueur-001 (ecart n°57) -- les mains du joueur : chaque geste, sur place, a portee, et rien ne se cree
// ni ne se perd (ce que la tuile ou le grenier perd, le sac le gagne, et l'inverse).

namespace AnastasisPlayerHandsTest
{
	using namespace AnastasisVillage;
	using EHand = FVillage::EPlayerHand;

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
		auto Set = [&World](int32 X, int32 Y, AnastasisWorld::ETileType Type, AnastasisWorld::EResource Res, int32 Amount)
		{
			AnastasisWorld::FTile& T = World.Tiles[Y * 40 + X];
			T.Type = Type;
			T.Resource = Res;
			T.Amount = Amount;
		};
		Set(21, 22, AnastasisWorld::ETileType::Field, AnastasisWorld::EResource::Food, 3);
		Set(19, 22, AnastasisWorld::ETileType::Grass, AnastasisWorld::EResource::Wood, 9);
		Set(20, 22, AnastasisWorld::ETileType::Stone, AnastasisWorld::EResource::Stone, 9);
		return World;
	}

	int32 TileAmount(const FVillage& V, int32 X, int32 Y)
	{
		const AnastasisWorld::FTile T = V.LiveTileAt(X, Y);
		return T.Resource == AnastasisWorld::EResource::None ? 0 : T.Amount;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisPlayerHandsTest,
	"Anastasis.Sim.Joueur.Mains",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisPlayerHandsTest::RunTest(const FString&)
{
	using namespace AnastasisPlayerHandsTest;
	const AnastasisWorld::FWorld World = MakeWorld();

	{
		FVillage Plain;
		Plain.Bind(World);
		TestTrue(TEXT("sans joueur : aucun geste offert"), Plain.PlayerHandOffer(FString(), 21, 22) == EHand::None);
		TestFalse(TEXT("sans joueur : aucun geste fait"), Plain.PlayerHandDo(EHand::Gather, FString(), 21, 22).bDone);
	}

	FVillage V;
	V.Bind(World);
	const FString Well = V.AddBuilding(WellType, 19, 20);
	const FString Granary = V.AddBuilding(GranaryType, 21, 20);
	const FString Me = V.ArriveAsPlayer(20.5, 21.5);
	if (!TestFalse(TEXT("le joueur arrive"), Me.IsEmpty()) || !TestFalse(TEXT("puits et grenier poses"), Well.IsEmpty() || Granary.IsEmpty())) return false;
	const auto Body = [&V, &Me]() { return V.FindNpc(Me); };

	// Boire au puits.
	{
		const double Before = Body()->Needs.Thirst;
		TestTrue(TEXT("le puits offre : boire"), V.PlayerHandOffer(Well, -1, -1) == EHand::Drink);
		const FVillage::FPlayerHandResult R = V.PlayerHandDo(EHand::Drink, Well, -1, -1);
		TestTrue(TEXT("boire : fait"), R.bDone);
		TestTrue(TEXT("boire : la soif baisse ou reste nulle"), Body()->Needs.Thirst <= Before && Body()->DrinksTaken == 1);
	}

	// Recolter : la tuile perd ce que le sac gagne, jusqu'a l'epuisement.
	{
		TestTrue(TEXT("le champ offre : recolter"), V.PlayerHandOffer(FString(), 21, 22) == EHand::Gather);
		int32 Total = TileAmount(V, 21, 22) + Body()->InventoryFood;
		for (int32 I = 0; I < 3; ++I)
		{
			TestTrue(TEXT("recolter : fait"), V.PlayerHandDo(EHand::Gather, FString(), 21, 22).bDone);
			TestEqual(TEXT("recolter : rien ne se cree"), TileAmount(V, 21, 22) + Body()->InventoryFood, Total);
		}
		TestEqual(TEXT("trois portions dans le sac"), Body()->InventoryFood, 3);
		TestTrue(TEXT("champ epuise : plus rien a recolter"), V.PlayerHandOffer(FString(), 21, 22) == EHand::None);
		TestFalse(TEXT("... et le geste est refuse"), V.PlayerHandDo(EHand::Gather, FString(), 21, 22).bDone);
	}

	// Couper du bois, tailler une pierre.
	{
		TestTrue(TEXT("la tuile de bois offre : couper"), V.PlayerHandOffer(FString(), 19, 22) == EHand::Chop);
		TestTrue(TEXT("couper : fait"), V.PlayerHandDo(EHand::Chop, FString(), 19, 22).bDone);
		TestEqual(TEXT("une buche dans le sac"), Body()->InventoryWood, 1);
		TestEqual(TEXT("une de moins sur la tuile"), TileAmount(V, 19, 22), 8);
		TestTrue(TEXT("la pierre offre : tailler"), V.PlayerHandOffer(FString(), 20, 22) == EHand::Quarry);
		TestTrue(TEXT("tailler : fait"), V.PlayerHandDo(EHand::Quarry, FString(), 20, 22).bDone);
		TestEqual(TEXT("une pierre dans le sac"), V.PlayerStone(), 1);
	}

	// relay-mains-001 : une maison posee sur une tuile de bois. La tuile garde son bois dans le monde, mais la main
	// vise la maison : la tuile n'offre plus rien, et le geste force est refuse sans rien prendre.
	{
		FVillage Built;
		Built.Bind(World);
		const FString House = Built.AddBuilding(HouseType, 19, 22);
		Built.ArriveAsPlayer(20.5, 21.5);
		if (TestFalse(TEXT("maison posee sur la tuile de bois"), House.IsEmpty()))
		{
			TestEqual(TEXT("... la tuile garde son bois"), TileAmount(Built, 19, 22), 9);
			TestTrue(TEXT("tuile batie : rien a couper"), Built.PlayerHandOffer(FString(), 19, 22) == EHand::None);
			TestFalse(TEXT("... et le geste est refuse"), Built.PlayerHandDo(EHand::Chop, FString(), 19, 22).bDone);
			TestEqual(TEXT("... le bois reste sur la tuile"), TileAmount(Built, 19, 22), 9);
		}
	}

	// Trop loin : refuse, rien ne bouge.
	{
		FVillage Far;
		Far.Bind(World);
		const FString FarWell = Far.AddBuilding(WellType, 19, 20);
		Far.ArriveAsPlayer(30.5, 30.5);
		const FVillage::FPlayerHandResult R = Far.PlayerHandDo(EHand::Drink, FarWell, -1, -1);
		TestFalse(TEXT("trop loin : refuse"), R.bDone);
		TestEqual(TEXT("... et il le dit"), R.Refusal, FString(TEXT("trop loin")));
		TestEqual(TEXT("... la tuile du champ intacte"), TileAmount(Far, 21, 22), 3);
	}

	// Le grenier : deposer le sac, reprendre une portion ; la nourriture se conserve.
	{
		const FBuilding* G = V.FindBuilding(Granary);
		const int32 Total = G->FoodPhysical + Body()->InventoryFood;
		TestTrue(TEXT("sac plein de nourriture : le grenier offre deposer"), V.PlayerHandOffer(Granary, -1, -1) == EHand::Deposit);
		const FVillage::FPlayerHandResult R = V.PlayerHandDo(EHand::Deposit, Granary, -1, -1);
		TestTrue(TEXT("deposer : fait"), R.bDone && R.Amount == 3);
		TestEqual(TEXT("deposer : le sac est vide"), Body()->InventoryFood, 0);
		TestTrue(TEXT("sac vide : le grenier offre prendre"), V.PlayerHandOffer(Granary, -1, -1) == EHand::Take);
		TestTrue(TEXT("prendre : fait"), V.PlayerHandDo(EHand::Take, Granary, -1, -1).bDone);
		TestEqual(TEXT("grenier + sac : rien ne se cree ni ne se perd"), V.FindBuilding(Granary)->FoodPhysical + Body()->InventoryFood, Total);
	}

	// Manger ce qu'on porte.
	{
		const double Before = Body()->Needs.Hunger;
		TestTrue(TEXT("manger : fait"), V.PlayerHandDo(EHand::Eat, FString(), -1, -1).bDone);
		TestEqual(TEXT("manger : le sac a perdu sa portion"), Body()->InventoryFood, 0);
		TestTrue(TEXT("manger : la faim ne monte pas"), Body()->Needs.Hunger <= Before && Body()->MealsTaken == 1);
		TestFalse(TEXT("sac vide : rien a manger"), V.PlayerHandDo(EHand::Eat, FString(), -1, -1).bDone);
	}

	// Un chantier sec : donner bois et pierre, puis batir une piece.
	{
		const FString Site = V.OpenSite(HouseType, 21, 21, false);
		if (TestFalse(TEXT("chantier ouvert"), Site.IsEmpty()))
		{
			// De quoi poser une piece : quelques buches et pierres de plus.
			for (int32 I = 0; I < 6; ++I)
			{
				V.PlayerHandDo(EHand::Chop, FString(), 19, 22);
				V.PlayerHandDo(EHand::Quarry, FString(), 20, 22);
			}
			TestTrue(TEXT("sac charge : le chantier offre donner"), V.PlayerHandOffer(Site, -1, -1) == EHand::Supply);
			const int32 Carried = Body()->InventoryWood + V.PlayerStone();
			const AnastasisBuild::FSiteMaterials Before = V.FindBuilding(Site)->Materials;
			const FVillage::FPlayerHandResult R = V.PlayerHandDo(EHand::Supply, Site, -1, -1);
			TestTrue(TEXT("donner : fait"), R.bDone);
			const AnastasisBuild::FSiteMaterials& After = V.FindBuilding(Site)->Materials;
			TestEqual(TEXT("donner : ce que le sac perd, le chantier le gagne"),
				(After.StockWood - Before.StockWood) + (After.StockStone - Before.StockStone),
				Carried - (Body()->InventoryWood + V.PlayerStone()));
			TestTrue(TEXT("sac vide : le chantier offre batir"), V.PlayerHandOffer(Site, -1, -1) == EHand::Build);
			const int32 Pieces = V.FindBuilding(Site)->PiecesPlaced;
			const FVillage::FPlayerHandResult B = V.PlayerHandDo(EHand::Build, Site, -1, -1);
			AddInfo(FString::Printf(TEXT("PLAYER_HANDS build done=%d refusal=%s pieces=%d->%d stock=%d/%d"), B.bDone ? 1 : 0, *B.Refusal,
				Pieces, V.FindBuilding(Site)->PiecesPlaced, V.FindBuilding(Site)->Materials.StockWood, V.FindBuilding(Site)->Materials.StockStone));
			TestTrue(TEXT("batir : une piece posee"), B.bDone && V.FindBuilding(Site)->PiecesPlaced == Pieces + 1);
			TestEqual(TEXT("... et elle compte au joueur"), Body()->PiecesPlaced, 1);
		}
	}
	return true;
}

#endif
