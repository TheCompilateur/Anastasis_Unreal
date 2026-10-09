#include "WorldView/AnastasisCanonicalGeography.h"
#include "WorldView/AnastasisSettlementSite.h"
#include "WorldView/AnastasisSettlementSurvey.h"
#include "WorldView/AnastasisWorldView.h"
#include "Sim/AnastasisSimulation.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// WATER_NETWORK_001 -- the canonical geography: seed only, fixed recipe, no render CVar.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisCanonicalWaterTest, "Anastasis.WaterNetwork.Canonical",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisCanonicalWaterTest::RunTest(const FString&)
{
	const uint32 Seed = AnastasisWorldView::ReferenceSeed;
	const auto A = AnastasisCanonicalGeography::Compute(Seed);
	if (!TestTrue(TEXT("canonical geography computed"), A.bValid)) { AddInfo(A.Error); return false; }
	TestEqual(TEXT("whole canonical world"), A.W * A.H, AnastasisWorldView::ReferenceWidth * AnastasisWorldView::ReferenceHeight);
	TestTrue(TEXT("the network has rivers"), A.Rivers > 0);
	TestTrue(TEXT("and water at tile centres"), A.WaterTiles > 0);

	// The render CVars that GEO_MEASURE_001 saw moving the village must be invisible here.
	struct FCVar { const TCHAR* Name; FString Old; const TCHAR* Value; };
	TArray<FCVar> Changed;
	for (const auto& Pair : TArray<TPair<const TCHAR*, const TCHAR*>>{
		{TEXT("anastasis.Terrain.Drainage"), TEXT("0")}, {TEXT("anastasis.Terrain.HumanGeography"), TEXT("0")},
		{TEXT("anastasis.Terrain.Forge.Exaggerate"), TEXT("2")}, {TEXT("anastasis.Terrain.Forge.Subdiv"), TEXT("3")},
		{TEXT("anastasis.WorldView.Scale"), TEXT("2")}, {TEXT("anastasis.Terrain.WaterLook"), TEXT("0")}})
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Pair.Key))
		{
			Changed.Add({Pair.Key, Var->GetString(), Pair.Value});
			Var->Set(Pair.Value, ECVF_SetByCode);
		}
	}
	const auto B = AnastasisCanonicalGeography::Compute(Seed);
	for (const FCVar& C : Changed) IConsoleManager::Get().FindConsoleVariable(C.Name)->Set(*C.Old, ECVF_SetByCode);
	TestEqual(TEXT("six render CVars found"), Changed.Num(), 6);
	TestTrue(TEXT("render CVars cannot change the canonical water"), A.Water == B.Water);
	TestEqual(TEXT("... nor its rivers"), B.Rivers, A.Rivers);

	// The simulation takes it.
	FAnastasisSimulation Sim;
	Sim.Reset(Seed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
	int32 JsWater = 0, Both = 0;
	for (int32 I = 0; I < Sim.GetWorld().Tiles.Num(); ++I)
	{
		const bool bJs = Sim.GetWorld().Tiles[I].Type == AnastasisWorld::ETileType::Water;
		JsWater += bJs ? 1 : 0;
		Both += (bJs && A.Water[I]) ? 1 : 0;
	}
	const int32 Changes = Sim.ApplyWaterMask(A.Water);
	int32 Mismatch = 0;
	for (int32 I = 0; I < Sim.GetWorld().Tiles.Num(); ++I)
	{
		if ((Sim.GetWorld().Tiles[I].Type == AnastasisWorld::ETileType::Water) != (A.Water[I] != 0)) ++Mismatch;
	}
	TestEqual(TEXT("simulated water = network water"), Mismatch, 0);

	// The opening site reads the canonical drained relief: what the player sees, without a render CVar.
	AnastasisSettlementSite::FInputs In;
	AnastasisSettlementSurvey::ReadSimulation(Seed, Sim.GetWorld(), Sim.GetVillage(), In, AnastasisSettlementSurvey::ReliefFactor, &A);
	const auto Site = AnastasisSettlementSite::Choose(In);
	if (TestTrue(TEXT("an eligible site on the drained relief"), Site.Best.bEligible))
	{
		TestTrue(TEXT("gentle on the relief the player sees"), A.Slope[Site.Best.Index] <= 8.0f);
		bool bWaterSeen = false;
		const int32 AX = Site.Best.WaterAccess % A.W, AY = Site.Best.WaterAccess / A.W;
		for (const FIntPoint D : {FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1)})
		{
			const int32 X = AX + D.X, Y = AY + D.Y;
			if (X >= 0 && Y >= 0 && X < A.W && Y < A.H && A.Water[Y * A.W + X]) bWaterSeen = true;
		}
		TestTrue(TEXT("its water is network water (visible)"), bWaterSeen);
		AddInfo(FString::Printf(TEXT("WATER_NETWORK_SITE site=(%d,%d) score=%.2f eligible=%d slope=%.2f water_m=%.0f"),
			Site.Best.Index % A.W, Site.Best.Index / A.W, Site.Best.Score, Site.Eligible, Site.Best.Slope, Site.Best.WaterM));
	}
	AddInfo(FString::Printf(TEXT("WATER_NETWORK seed=%u network_water=%d js_water=%d both=%d changed=%d rivers=%d lakes=%d compute_s=%.2f"),
		Seed, A.WaterTiles, JsWater, Both, Changes, A.Rivers, A.Lakes, A.Seconds));
	return true;
}

// drainage-lac-vide-001 -- deux mondes ou un lac minuscule disparaissait sous le flou du contour : la cote du lac
// lisait une liste vide et le jeu plantait au lancement (annee-valmire-001, graines 99 et 2026).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisCanonicalTinyLakeTest, "Anastasis.WaterNetwork.LacMinuscule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisCanonicalTinyLakeTest::RunTest(const FString&)
{
	for (const uint32 Seed : {99u, 2026u})
	{
		const auto G = AnastasisCanonicalGeography::Compute(Seed);
		if (!TestTrue(FString::Printf(TEXT("monde %u : geographie calculee, sans plantage"), Seed), G.bValid))
		{
			AddInfo(G.Error);
			continue;
		}
		TestTrue(FString::Printf(TEXT("monde %u : de l'eau aux centres des tuiles"), Seed), G.WaterTiles > 0);
		AddInfo(FString::Printf(TEXT("WATER_TINY_LAKE seed=%u water=%d rivers=%d lakes=%d"), Seed, G.WaterTiles, G.Rivers, G.Lakes));
	}
	return true;
}

#endif
