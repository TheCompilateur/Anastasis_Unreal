#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS

// valmire-grows-001 (ecart n°52) -- le village decide de ses batiments communs.
//
// La decision du soir (`UpdateCommonBuildingsDaily`), regle par regle, sur un village nu :
//   1. sans grenier, le village en ouvre un ; trop d'ames pour un puits, il ouvre un puits ; le grenier
//      plein, il en ouvre un second ; chaque fois un habitant en trace l'emplacement, avec la raison ;
//   2. rien ne manque, ou les creneaux sont pleins : rien ne s'ouvre ;
//   3. deux parties identiques restent identiques (StateDigest) ;
//   4. croissance eteinte (harnais, scenarios) : rien de tout cela, meme a minuit.
// La vie sur trente jours se juge sur le vrai village du lancement, dans l'hote
// (`Anastasis.Village.Croissance.Valmire`).

namespace AnastasisVillageGrowthTest
{
	using namespace AnastasisVillage;

	constexpr double Dt = 1.0 / 60.0;
	constexpr int32 TicksPerDay = static_cast<int32>(FAnastasisSimulation::DayLength * 60.0);

	/** Puits (et grenier si demande) sur les premieres cases libres pres du centre, `Count` habitants. */
	void Populate(FAnastasisSimulation& Sim, bool bGrowth, bool bGranary, int32 Count)
	{
		Sim.Reset(12345u, 96, 96);
		FVillage& Village = Sim.GetVillage();
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
		if (bGranary) Spiral(3, [&Village](int32 X, int32 Y) { return !Village.AddBuilding(GranaryType, X, Y).IsEmpty(); });
		int32 Spawned = 0;
		for (int32 R = 2; R <= 10 && Spawned < Count; ++R)
		for (int32 DY = -R; DY <= R && Spawned < Count; DY += 2)
		for (int32 DX = -R; DX <= R && Spawned < Count; DX += 2)
		{
			if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
			const double X = CX + DX + 0.5, Y = CY + DY + 0.5;
			if (Village.IsFootBlocked(X, Y)) continue;
			Village.SpawnNpc(X, Y, AnastasisNeeds::FNeeds());
			++Spawned;
		}
		Village.SetGrowthEnabled(bGrowth);
	}

	const FBuilding* Opened(const FVillage& Village, const FString& Id)
	{
		const FBuilding* B = Village.FindBuilding(Id);
		return B && !B->OpenedById.IsEmpty() ? B : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageCommonBuildingsTest,
	"Anastasis.Sim.Village.Croissance.BatimentsCommuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageCommonBuildingsTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGrowthTest;
	{
		// Sans grenier : le village en ouvre un.
		FAnastasisSimulation Sim;
		Populate(Sim, true, false, 8);
		FVillage& V = Sim.GetVillage();
		const FString Id = V.UpdateCommonBuildingsDaily(1);
		const FBuilding* B = Opened(V, Id);
		if (TestNotNull(TEXT("sans grenier : un chantier ouvert par le village"), B))
		{
			TestEqual(TEXT("... un grenier"), B->Type, FString(GranaryType));
			TestTrue(TEXT("... et pourquoi"), B->OpenCause.StartsWith(TEXT("aucun grenier")));
			TestTrue(TEXT("... chantier sec, en cours"), B->Progress < 1.0 && B->bHasMaterials);
			TestEqual(TEXT("... trace par un habitant, qui en est le batisseur"), B->BuilderId, B->OpenedById);
		}
		TestTrue(TEXT("un creneau pris : le soir suivant, rien de plus (un seul grenier en chantier)"), V.UpdateCommonBuildingsDaily(2).IsEmpty());
		AddInfo(TEXT("VALMIRE_GROWTH granary ") + V.GetLastBuildDecision());
	}
	{
		// Seize ames pour un puits : un puits de plus.
		FAnastasisSimulation Sim;
		Populate(Sim, true, true, 16);
		FVillage& V = Sim.GetVillage();
		TestEqual(TEXT("seize habitants poses"), V.GetActors().Num(), 16);
		const FString Id = V.UpdateCommonBuildingsDaily(1);
		const FBuilding* B = Opened(V, Id);
		if (TestNotNull(TEXT("seize ames pour un puits : un chantier ouvert"), B))
		{
			TestEqual(TEXT("... un puits"), B->Type, FString(WellType));
			TestEqual(TEXT("... et pourquoi"), B->OpenCause, FString(TEXT("16 ames pour 1 puits")));
		}
		AddInfo(TEXT("VALMIRE_GROWTH well ") + V.GetLastBuildDecision());
	}
	{
		// Le grenier plein : un second grenier.
		FAnastasisSimulation Sim;
		Populate(Sim, true, true, 8);
		FVillage& V = Sim.GetVillage();
		FString GranaryId;
		for (const FBuilding& B : V.GetBuildings()) if (B.Type == GranaryType) GranaryId = B.Id;
		TestTrue(TEXT("grenier rempli"), V.CreditFood(GranaryId, GranaryFoodCap) >= static_cast<int32>(0.9 * GranaryFoodCap));
		const FString Id = V.UpdateCommonBuildingsDaily(1);
		const FBuilding* B = Opened(V, Id);
		if (TestNotNull(TEXT("grenier plein : un chantier ouvert"), B))
		{
			TestEqual(TEXT("... un second grenier"), B->Type, FString(GranaryType));
			TestTrue(TEXT("... et pourquoi"), B->OpenCause.StartsWith(TEXT("le grenier deborde")));
		}
		AddInfo(TEXT("VALMIRE_GROWTH full ") + V.GetLastBuildDecision());
	}
	{
		// Rien ne manque : rien ne s'ouvre.
		FAnastasisSimulation Sim;
		Populate(Sim, true, true, 8);
		FVillage& V = Sim.GetVillage();
		TestTrue(TEXT("rien ne manque : aucun chantier"), V.UpdateCommonBuildingsDaily(1).IsEmpty());
		TestTrue(TEXT("... et la decision le dit"), V.GetLastBuildDecision().Contains(TEXT("rien ne manque")));
	}
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
	Populate(A, true, false, 8);
	Populate(B, true, false, 8);
	for (int32 I = 0; I < TicksPerDay * 3; ++I) { A.Tick(Dt); B.Tick(Dt); }
	TestEqual(TEXT("deux parties identiques : meme etat complet a trois jours"), A.StateDigest(), B.StateDigest());
	TestTrue(TEXT("... et le village y a bien decide un grenier"), A.GetVillage().GetGrowthSitesOpened() >= 1);
	TestEqual(TEXT("... dans les deux"), A.GetVillage().GetGrowthSitesOpened(), B.GetVillage().GetGrowthSitesOpened());
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
	Populate(Sim, false, false, 8);
	TestFalse(TEXT("croissance eteinte dans ce village"), Sim.GetVillage().IsGrowthEnabled());
	TestTrue(TEXT("la decision du soir ne fait rien"), Sim.GetVillage().UpdateCommonBuildingsDaily(1).IsEmpty());
	for (int32 I = 0; I < TicksPerDay * 3; ++I) Sim.Tick(Dt);
	TestEqual(TEXT("trois minuits plus tard : aucun chantier commun"), Sim.GetVillage().GetGrowthSitesOpened(), 0);
	for (const FBuilding& B : Sim.GetVillage().GetBuildings())
	{
		TestTrue(TEXT("aucun chantier ne porte d'ouvreur"), B.OpenedById.IsEmpty());
	}
	return true;
}

#endif
