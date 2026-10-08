#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisBuild.h"
#include "Work/AnastasisGather.h"

#if WITH_DEV_AUTOMATION_TESTS

// opening-in-sim-001 (ecart n°40) : le village d'ouverture est decide par la simulation, sans UWorld ni hote.
// Le puits et les 12 colons sont poses comme `UAnastasisSimulationSubsystem::SeedFirstWell` les pose.

namespace
{
	FString OpeningTestSeedWell(FAnastasisSimulation& Sim, int32 NpcCount, int32 TileX, int32 TileY)
	{
		AnastasisVillage::FVillage& Village = Sim.GetVillage();
		const AnastasisNav::FNavGrid& Nav = Village.GetNavGrid();
		auto FreeNeighbours = [&](int32 X, int32 Y)
		{
			int32 Count = 0;
			for (int32 DY = -1; DY <= 1; ++DY)
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				if ((DX || DY) && !Village.IsFootBlocked(X + DX + 0.5, Y + DY + 0.5)) ++Count;
			}
			return Count;
		};
		FString WellId;
		int32 WellX = 0, WellY = 0;
		for (int32 Radius = 0; Radius <= 24 && WellId.IsEmpty(); ++Radius)
		for (int32 DY = -Radius; DY <= Radius && WellId.IsEmpty(); ++DY)
		for (int32 DX = -Radius; DX <= Radius && WellId.IsEmpty(); ++DX)
		{
			if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
			const int32 X = TileX + DX, Y = TileY + DY;
			if (X < 2 || Y < 2 || X > Nav.W - 3 || Y > Nav.H - 3) continue;
			if (Village.IsFootBlocked(X + 0.5, Y + 0.5) || FreeNeighbours(X, Y) < 3) continue;
			WellId = Village.AddBuilding(AnastasisVillage::WellType, X, Y, 1.0, Sim.GetDay());
			WellX = X;
			WellY = Y;
		}
		if (WellId.IsEmpty()) return WellId;
		for (int32 K = 0; K < NpcCount; ++K)
		{
			const double Angle = 2.0 * UE_DOUBLE_PI * K / FMath::Max(1, NpcCount);
			const int32 CX = WellX + FMath::RoundToInt32(7.0 * FMath::Cos(Angle));
			const int32 CY = WellY + FMath::RoundToInt32(7.0 * FMath::Sin(Angle));
			bool bPlaced = false;
			for (int32 R = 0; R <= 6 && !bPlaced; ++R)
			for (int32 DY = -R; DY <= R && !bPlaced; ++DY)
			for (int32 DX = -R; DX <= R && !bPlaced; ++DX)
			{
				const int32 X = CX + DX, Y = CY + DY;
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R || !Nav.IsInBounds(X, Y)) continue;
				if (Village.IsFootBlocked(X + 0.5, Y + 0.5)) continue;
				AnastasisNeeds::FNeeds Needs;
				Needs.Hunger = 10.0;
				Needs.Energy = 80.0;
				Needs.Social = 70.0;
				Needs.Leisure = 70.0;
				Needs.Hygiene = 60.0;
				Needs.Thirst = FMath::Max(5.0, 80.0 - 12.0 * K);
				Needs.Health = 90.0;
				Needs.Morale = 55.0;
				Village.SpawnNpc(X + 0.5, Y + 0.5, Needs, 4.0);
				bPlaced = true;
			}
		}
		return WellId;
	}

	FString OpeningTestDescribe(const AnastasisVillage::FOpeningReport& R)
	{
		return FString::Printf(TEXT("npc=%s home=%s work=%s site=%s tile=(%d,%d) builders=%s courier=%s stock=%d/%d farmers=%s"),
			*R.ResidentId, *R.HomeId, *R.WorkId, *R.SiteId, R.SiteX, R.SiteY,
			*FString::Join(R.Builders, TEXT(",")), *R.CourierId, R.StockWood, R.StockStone,
			*FString::Join(R.Farmers, TEXT(",")));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisVillageOpeningTest,
	"Anastasis.Sim.Village.Ouverture.DecideeParLaSimulation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageOpeningTest::RunTest(const FString&)
{
	using namespace AnastasisVillage;
	constexpr uint32 Seed = 12345;
	constexpr int32 Size = 96;
	constexpr int32 Settlers = 12;

	TUniquePtr<FAnastasisSimulation> Runs[2] = { MakeUnique<FAnastasisSimulation>(), MakeUnique<FAnastasisSimulation>() };
	FOpeningReport Reports[2];
	for (int32 Run = 0; Run < 2; ++Run)
	{
		FAnastasisSimulation& Sim = *Runs[Run];
		Sim.Reset(Seed, Size, Size);
		const FPoint Centre = Sim.GetVillage().GetSettlement();
		const FString WellId = OpeningTestSeedWell(Sim, Settlers, FMath::FloorToInt32(Centre.X), FMath::FloorToInt32(Centre.Y));
		if (!TestFalse(TEXT("puits pose"), WellId.IsEmpty())) return false;
		if (!TestEqual(TEXT("douze colons"), Sim.GetVillage().GetActors().Num(), Settlers)) return false;
		Reports[Run] = Sim.GetVillage().SeedOpeningVillage(Sim.GetDay(), /*bOpenConstruction=*/true);
	}

	const FOpeningReport& R = Reports[0];
	const FVillage& V = Runs[0]->GetVillage();
	AddInfo(FString::Printf(TEXT("OPENING_IN_SIM seed=%u %s"), Seed, *OpeningTestDescribe(R)));
	TestTrue(TEXT("foyer traite"), R.bHousehold);
	TestFalse(TEXT("maison du premier colon"), R.HomeId.IsEmpty());
	TestFalse(TEXT("grenier du premier colon"), R.WorkId.IsEmpty());
	TestTrue(TEXT("chantier tente"), R.bConstructionTried);
	TestFalse(TEXT("chantier ouvert"), R.SiteId.IsEmpty());
	TestTrue(TEXT("un ou deux batisseurs"), R.Builders.Num() >= 1 && R.Builders.Num() <= 2);
	TestEqual(TEXT("porteur = premier batisseur"), R.CourierId, R.Builders.IsEmpty() ? FString() : R.Builders[0]);
	TestTrue(TEXT("embauche au grenier"), R.bWorkforce);

	// L'etat de la simulation porte ce que le rapport dit.
	const FNpc* Resident = V.FindNpc(R.ResidentId);
	TestTrue(TEXT("colon loge"), Resident && Resident->HomeId == R.HomeId);
	TestTrue(TEXT("colon au grenier"), Resident && Resident->WorkplaceId == R.WorkId && Resident->JobId == AnastasisGather::JobFarmer);
	for (const FString& Id : R.Builders)
	{
		const FNpc* Builder = V.FindNpc(Id);
		TestTrue(FString::Printf(TEXT("%s batisseur"), *Id), Builder && Builder->JobId == AnastasisBuild::JobBuilder);
	}
	for (const FString& Id : R.Farmers)
	{
		const FNpc* Farmer = V.FindNpc(Id);
		TestTrue(FString::Printf(TEXT("%s fermier"), *Id), Farmer && Farmer->WorkplaceId == R.WorkId);
	}
	TestEqual(TEXT("porteur pose dans la simulation"), V.GetMaterialCourier(), R.CourierId);
	TestEqual(TEXT("verrou du chantier dans la simulation"), V.GetOpeningSiteId(), R.SiteId);
	TestTrue(TEXT("attribution en attente"), V.GetOpeningHome().Status == EOpeningHomeStatus::Pending);
	const FBuilding* Site = V.FindBuilding(R.SiteId);
	TestTrue(TEXT("chantier sec, inacheve"), Site && !Site->IsCompleted() && Site->Materials.StockWood == 0 && Site->Materials.StockStone == 0);

	// Deux executions, meme graine : meme ouverture, meme etat.
	TestEqual(TEXT("ouverture deterministe"), OpeningTestDescribe(Reports[1]), OpeningTestDescribe(R));
	TestEqual(TEXT("etat deterministe apres ouverture"), Runs[1]->GetVillage().Digest(), V.Digest());

	// La simulation seule, pas a pas : la maison s'acheve et la simulation l'attribue, sans aucun appel d'hote.
	constexpr double Dt = 1.0 / 60.0;
	// Le porteur seul (prouve ailleurs : Chantier.PorteurMateriaux, material-courier-pie) peut ne pas finir
	// le devis dans la borne : passe la moitie, le reste du devis est credite au chantier par la simulation
	// (CreditSiteMaterials), dans les deux runs au meme pas. Ce qui est juge ici, c'est l'attribution.
	constexpr int32 MaxSteps = 4 * 90 * 60; // quatre jours simules
	constexpr int32 CreditAt = MaxSteps / 2;
	int32 Steps = 0;
	int32 Credited = 0;
	for (; Steps < MaxSteps; ++Steps)
	{
		if (Steps == CreditAt)
		{
			for (int32 Run = 0; Run < 2; ++Run)
			{
				const FBuilding* Pending = Runs[Run]->GetVillage().FindBuilding(R.SiteId);
				if (!Pending || Pending->IsCompleted()) continue;
				const AnastasisBuild::FSiteMaterials& M = Pending->Materials;
				const int32 In = Runs[Run]->GetVillage().CreditSiteMaterials(R.SiteId,
					FMath::Max(0, M.NeedWood - M.ConsumedWood - M.StockWood),
					FMath::Max(0, M.NeedStone - M.ConsumedStone - M.StockStone));
				if (Run == 0) Credited = In;
			}
		}
		Runs[0]->Tick(Dt);
		Runs[1]->Tick(Dt);
		if (Runs[0]->GetVillage().GetOpeningHome().Status != EOpeningHomeStatus::Pending) break;
	}
	const FOpeningHomeOutcome& Outcome = V.GetOpeningHome();
	const FBuilding* House = V.FindBuilding(R.SiteId);
	const FNpc* Owner = V.FindNpc(Outcome.NpcId);
	const FNpc* Courier = V.FindNpc(R.CourierId);
	AddInfo(FString::Printf(TEXT("OPENING_IN_SIM home status=%s npc=%s home=%s steps=%d time=%.4f pieces=%d delivered=%d credited=%d"),
		OpeningHomeStatusName(Outcome.Status), *Outcome.NpcId, *R.SiteId, Steps, Outcome.Time,
		House ? House->PiecesPlaced : -1, Courier ? Courier->MaterialsDelivered : -1, Credited));
	TestTrue(TEXT("maison achevee dans la borne"), House && House->IsCompleted());
	TestTrue(TEXT("attribuee par la simulation"), Outcome.Status == EOpeningHomeStatus::Assigned);
	TestTrue(TEXT("proprietaire = un batisseur"), R.Builders.Contains(Outcome.NpcId));
	TestTrue(TEXT("maison au proprietaire"), House && Owner && House->Owner == Owner->Id && Owner->HomeId == House->Id);
	TestTrue(TEXT("verrou leve"), V.GetOpeningSiteId().IsEmpty());
	TestEqual(TEXT("meme proprietaire au second run"), Runs[1]->GetVillage().GetOpeningHome().NpcId, Outcome.NpcId);
	TestEqual(TEXT("etat deterministe apres attribution"), Runs[1]->GetVillage().Digest(), V.Digest());
	return true;
}

#endif
