#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisBuild.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisVillageMaterialCourierTest,
	"Anastasis.Sim.Village.Chantier.PorteurMateriaux",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageMaterialCourierTest::RunTest(const FString&)
{
	using namespace AnastasisVillage;
	AnastasisWorld::FWorld World;
	World.W = 40; World.H = 32;
	World.Tiles.SetNum(World.W * World.H);
	for (int32 Y = 0; Y < World.H; ++Y)
	for (int32 X = 0; X < World.W; ++X)
	{
		auto& T = World.Tiles[Y * World.W + X];
		T.X = X; T.Y = Y; T.Type = AnastasisWorld::ETileType::Grass;
		T.Alt = 0.5; T.Wetness = 0.3;
	}
	auto& Wood = World.Tiles[10 * World.W + 10];
	Wood.Resource = AnastasisWorld::EResource::Wood; Wood.Amount = 24;
	auto& Stone = World.Tiles[10 * World.W + 12];
	Stone.Resource = AnastasisWorld::EResource::Stone; Stone.Amount = 8;
	FVillage Village;
	Village.Bind(World);
	const FString SiteId = Village.OpenSite(HouseType, 16, 11, false);
	if (!TestFalse(TEXT("chantier sec cree"), SiteId.IsEmpty())) return false;
	AnastasisNeeds::FNeeds Needs;
	Needs.Hunger = 10.0; Needs.Thirst = 5.0; Needs.Energy = 90.0;
	Needs.Health = 95.0; Needs.Morale = 60.0;
	Needs.Social = 80.0; Needs.Leisure = 80.0; Needs.Hygiene = 80.0;
	const FString CourierId = Village.SpawnNpc(10.5, 11.5, Needs);
	const FString BuilderId = Village.SpawnNpc(14.5, 12.5, Needs);
	Village.SetJob(BuilderId, AnastasisBuild::JobBuilder);
	Village.SetMaterialCourier(CourierId);
	bool bSawCarry = false, bConserved = true;
	double Time = 27.0;
	for (int32 Tick = 0; Tick < 180 * 60; ++Tick)
	{
		Time += 1.0 / 60.0;
		Village.UpdateActors(Time, 1.0 / 60.0);
		const FBuilding* Site = Village.FindBuilding(SiteId);
		const FNpc* Courier = Village.FindNpc(CourierId);
		if (!Site || !Courier) return false;
		bSawCarry |= Courier->MaterialCarry > 0;
		const int32 WoodLeft = Village.LiveTileAt(10, 10).Amount;
		const int32 StoneLeft = Village.LiveTileAt(12, 10).Amount;
		const int32 WoodAccounted = WoodLeft + Site->Materials.StockWood + Site->Materials.ConsumedWood
			+ (Courier->MaterialResource == AnastasisWorld::EResource::Wood ? Courier->MaterialCarry : 0);
		const int32 StoneAccounted = StoneLeft + Site->Materials.StockStone + Site->Materials.ConsumedStone
			+ (Courier->MaterialResource == AnastasisWorld::EResource::Stone ? Courier->MaterialCarry : 0);
		bConserved &= WoodAccounted == 24 && StoneAccounted == 8;
		if (Site->IsCompleted()) break;
	}
	const FBuilding* Site = Village.FindBuilding(SiteId);
	const FNpc* Courier = Village.FindNpc(CourierId);
	TestTrue(TEXT("charge physiquement portee"), bSawCarry);
	TestTrue(TEXT("bois et pierre conserves a chaque pas"), bConserved);
	TestTrue(TEXT("chantier sec acheve par livraison reelle"), Site && Site->IsCompleted());
	TestTrue(TEXT("porteur a livre"), Courier && Courier->MaterialsDelivered >= 32);
	TestEqual(TEXT("bois du monde epuise"), Village.LiveTileAt(10, 10).Amount, 0);
	TestEqual(TEXT("pierre du monde epuisee"), Village.LiveTileAt(12, 10).Amount, 0);
	return true;
}

#endif
