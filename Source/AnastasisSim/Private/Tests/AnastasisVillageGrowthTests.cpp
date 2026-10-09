#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS

// valmire-grows-001 (ecart n°50) -- le village grandit de lui-meme.
//
// Un village nu de douze habitants (puits, grenier, champs ouverts), la croissance allumee :
//   1. des habitants ouvrent d'eux-memes des chantiers (`tryOpenNewConstruction`), chacun avec sa raison ;
//   2. deux parties identiques restent identiques (StateDigest) ;
//   3. croissance eteinte : rien de tout cela (le harnais et les scenarios ne sont pas touches).
// Ce village nu n'est pas viable (sans croissance non plus : tout le monde y meurt en huit jours, sans
// fermiers ni familles) : la vie sur trente jours se juge sur le vrai village du lancement, dans l'hote
// (`Anastasis.Village.Croissance.Valmire`).

namespace AnastasisVillageGrowthTest
{
	using namespace AnastasisVillage;

	constexpr double Dt = 1.0 / 60.0;
	constexpr int32 TicksPerDay = static_cast<int32>(FAnastasisSimulation::DayLength * 60.0);

	void Populate(FAnastasisSimulation& Sim, bool bGrowth)
	{
		Sim.Reset(12345u, 96, 96);
		FVillage& Village = Sim.GetVillage();
		Village.SetTerrainTravelCostEnabled(true);
		Village.SetRoadEvolutionEnabled(true);
		const FPoint Centre = Village.GetSettlement();
		const int32 CX = FMath::FloorToInt32(Centre.X), CY = FMath::FloorToInt32(Centre.Y);
		const auto Spiral = [CX, CY](int32 MinR, TFunctionRef<bool(int32, int32)> Try)
		{
			for (int32 R = MinR; R <= 16; ++R)
			for (int32 DY = -R; DY <= R; ++DY)
			for (int32 DX = -R; DX <= R; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
				if (Try(CX + DX, CY + DY)) return true;
			}
			return false;
		};
		// Le centre du monde 12345 n'est pas constructible : le puits sur la premiere case libre.
		Spiral(0, [&Village](int32 X, int32 Y) { return !Village.AddBuilding(WellType, X, Y).IsEmpty(); });
		Spiral(3, [&Village](int32 X, int32 Y) { return !Village.AddBuilding(GranaryType, X, Y).IsEmpty(); });
		int32 Fields = 0;
		Spiral(4, [&Village, &Fields](int32 X, int32 Y) { if (Village.ActivateFoodSource(X, Y)) ++Fields; return Fields >= 8; });
		int32 Spawned = 0;
		for (int32 R = 2; R <= 8 && Spawned < 12; ++R)
		for (int32 DY = -R; DY <= R && Spawned < 12; DY += 2)
		for (int32 DX = -R; DX <= R && Spawned < 12; DX += 2)
		{
			if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
			const double X = CX + DX + 0.5, Y = CY + DY + 0.5;
			if (Village.IsFootBlocked(X, Y)) continue;
			AnastasisNeeds::FNeeds Needs;
			Needs.Thirst = 20.0 + 2.0 * Spawned;
			Needs.Hunger = 10.0 + 2.0 * Spawned;
			Village.SpawnNpc(X, Y, Needs);
			++Spawned;
		}
		Village.SetGrowthEnabled(bGrowth);
	}

	struct FOutcome
	{
		int32 SitesOpened = 0;
		int32 OpenedDone = 0;
		int32 Deaths = 0;
		int32 Npcs = 0;
		int32 Buildings = 0;
		TArray<FString> Lines;
		uint64 Digest = 0;
	};

	FOutcome Run(FAnastasisSimulation& Sim, int32 Days)
	{
		for (int32 I = 0; I < TicksPerDay * Days; ++I) Sim.Tick(Dt);
		FOutcome Out;
		const FVillage& Village = Sim.GetVillage();
		Out.SitesOpened = Village.GetGrowthSitesOpened();
		Out.Deaths = Village.GetDeaths().Num();
		Out.Npcs = Village.GetActors().Num();
		Out.Buildings = Village.GetBuildings().Num();
		for (const FBuilding& B : Village.GetBuildings())
		{
			if (B.OpenedById.IsEmpty()) continue;
			if (B.Progress >= 1.0) ++Out.OpenedDone;
			Out.Lines.Add(FString::Printf(TEXT("%s %s par %s (%s) %.0f%%"), *B.Id, *B.Type, *B.OpenedById, *B.OpenCause, B.Progress * 100.0));
		}
		Out.Digest = Sim.StateDigest();
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGrowthTest,
	"Anastasis.Sim.Village.Croissance.Ouverture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGrowthTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGrowthTest;
	FAnastasisSimulation Sim;
	Populate(Sim, true);
	TestTrue(TEXT("croissance allumee"), Sim.GetVillage().IsGrowthEnabled());
	TestEqual(TEXT("puits et grenier poses"), Sim.GetVillage().GetBuildings().Num(), 2);
	const FOutcome O = Run(Sim, 6);
	for (const FString& Line : O.Lines) AddInfo(TEXT("VALMIRE_GROWTH site ") + Line);
	AddInfo(FString::Printf(TEXT("VALMIRE_GROWTH days=6 sites_opened=%d opened_done=%d deaths=%d npcs=%d buildings=%d last=\"%s\""),
		O.SitesOpened, O.OpenedDone, O.Deaths, O.Npcs, O.Buildings, *Sim.GetVillage().GetLastBuildDecision()));
	TestTrue(TEXT("des habitants ouvrent d'eux-memes des chantiers"), O.SitesOpened >= 2);
	TestTrue(TEXT("chaque chantier ouvert dit pourquoi"), !O.Lines.ContainsByPredicate([](const FString& L) { return L.Contains(TEXT("()")); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGrowthDeterminismTest,
	"Anastasis.Sim.Village.Croissance.Deterministe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGrowthDeterminismTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGrowthTest;
	FAnastasisSimulation A, B;
	Populate(A, true);
	Populate(B, true);
	const FOutcome OA = Run(A, 8);
	const FOutcome OB = Run(B, 8);
	TestEqual(TEXT("deux parties identiques : meme etat complet a huit jours"), OA.Digest, OB.Digest);
	TestEqual(TEXT("... memes chantiers ouverts"), OA.SitesOpened, OB.SitesOpened);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGrowthOffTest,
	"Anastasis.Sim.Village.Croissance.Eteinte",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGrowthOffTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGrowthTest;
	FAnastasisSimulation Sim;
	Populate(Sim, false);
	TestFalse(TEXT("croissance eteinte par defaut dans ce village"), Sim.GetVillage().IsGrowthEnabled());
	const FOutcome O = Run(Sim, 8);
	TestEqual(TEXT("aucun chantier ouvert par un habitant"), O.SitesOpened, 0);
	TestEqual(TEXT("aucun chantier ne porte d'ouvreur"), O.Lines.Num(), 0);

	// Allumer puis eteindre rend le village a son etat sans colonie.
	FAnastasisSimulation Toggled;
	Populate(Toggled, false);
	Toggled.GetVillage().SetGrowthEnabled(true);
	Toggled.GetVillage().SetGrowthEnabled(false);
	FAnastasisSimulation Plain;
	Populate(Plain, false);
	TestEqual(TEXT("allumer puis eteindre ne laisse rien"), Toggled.StateDigest(), Plain.StateDigest());
	return true;
}

#endif
