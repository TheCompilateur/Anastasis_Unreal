#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "Core/AnastasisRng.h"
#include "Harness/AnastasisHarnessTrace.h"
#include "Harness/AnastasisJsSave.h"
#include "Sim/AnastasisSimulation.h"
#include "World/AnastasisExplore.h"
#include "World/AnastasisNavGrid.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Exploration et tirages de la decision — mission perception-explore-001.
 *
 * Vecteurs generes par tools/migration/gen-explore-vectors.mjs : `exploreTarget` et
 * `randomWalkTarget` executes tels quels sur un monde de test, et les decisions MESUREES
 * du scenario endurance (photo de l'habitant, etat du flux, nombre de tirages). Ne jamais
 * corriger un vecteur a la main.
 */
namespace AnastasisExploreParity
{
	namespace Vecteurs
	{
#include "AnastasisExploreVectors.inl"
	}

	static double ExploreFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	static uint64 ExploreToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	/** Le monde de test : generateWorld, eau bloquee, et le bloc de bati si demande. */
	struct FExploreFixture
	{
		AnastasisWorld::FWorld World;
		AnastasisNav::FNavGrid Grid;
		AnastasisExplore::FExploreWorld Explore;

		FExploreFixture(uint32 Seed, bool bBlock)
		{
			World = AnastasisWorld::GenerateWorld(Seed, Vecteurs::ExploreWorldW, Vecteurs::ExploreWorldH);
			AnastasisNav::InitFromWorld(Grid, World);
			if (bBlock)
			{
				for (int32 Y = Vecteurs::ExploreBlockY0; Y <= Vecteurs::ExploreBlockY1; ++Y)
				{
					for (int32 X = Vecteurs::ExploreBlockX0; X <= Vecteurs::ExploreBlockX1; ++X)
					{
						Grid.Blocked[Y * Grid.W + X] = 1;
					}
				}
			}
			Explore.W = Grid.W;
			Explore.H = Grid.H;
			Explore.IsBlocked = [this](double X, double Y)
			{
				return AnastasisNav::BlockedAt(Grid, FMath::FloorToInt32(X), FMath::FloorToInt32(Y));
			};
			Explore.IsFootBlocked = [this](double X, double Y)
			{
				return AnastasisNav::FootBlockedAt(Grid, World, FMath::FloorToInt32(X), FMath::FloorToInt32(Y));
			};
			Explore.Settlement = { Vecteurs::ExploreSettlementX, Vecteurs::ExploreSettlementY };
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisExploreParityTest,
	"Anastasis.Sim.Parite.Exploration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisExploreParityTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisExploreParity;
	namespace V = AnastasisExploreParity::Vecteurs;
	namespace E = AnastasisExplore;

	int32 Compared = 0;
	int32 Errors = 0;
	auto Fail = [this, &Errors](const FString& Message)
	{
		if (++Errors <= 30) AddError(Message);
	};
	TMap<uint64, TUniquePtr<P::FExploreFixture>> Fixtures;
	auto FixtureFor = [&Fixtures](uint32 Seed, bool bBlock) -> P::FExploreFixture&
	{
		const uint64 Key = (static_cast<uint64>(Seed) << 1) | (bBlock ? 1u : 0u);
		if (!Fixtures.Contains(Key)) Fixtures.Add(Key, MakeUnique<P::FExploreFixture>(Seed, bBlock));
		return *Fixtures[Key];
	};
	auto Check = [&](const FString& Where, const E::FExploreResult& Got, const FAnastasisRng& Rng,
		uint64 PX, uint64 PY, uint32 StateAfter, int32 Draws, int32 Settlement)
	{
		Compared += 5;
		if (P::ExploreToBits(Got.Point.X) != PX || P::ExploreToBits(Got.Point.Y) != PY)
		{
			Fail(FString::Printf(TEXT("%s : cible (%g, %g), attendu (%g, %g)"), *Where,
				Got.Point.X, Got.Point.Y, P::ExploreFromBits(PX), P::ExploreFromBits(PY)));
		}
		if (Rng.GetState() != StateAfter) Fail(FString::Printf(TEXT("%s : etat apres %u, attendu %u"), *Where, Rng.GetState(), StateAfter));
		if (Got.Draws != Draws) Fail(FString::Printf(TEXT("%s : %d tirages, attendu %d"), *Where, Got.Draws, Draws));
		if (Got.bSettlement != (Settlement != 0)) Fail(FString::Printf(TEXT("%s : repli au centre %d, attendu %d"), *Where, Got.bSettlement ? 1 : 0, Settlement));
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::ExploreCases); ++I)
	{
		const V::FExploreCaseVector& C = V::ExploreCases[I];
		P::FExploreFixture& F = FixtureFor(C.Seed, C.Block != 0);
		TSet<int32> Cells;
		for (int32 K = 0; K < C.CellCount; ++K) Cells.Add(V::ExploreCells[C.FirstCell + K]);
		FAnastasisRng Rng(C.State);
		const E::FExploreResult Got = E::ExploreTarget(F.Explore, Rng, P::ExploreFromBits(C.XBits), P::ExploreFromBits(C.YBits), Cells);
		Check(FString::Printf(TEXT("exploreTarget[%d] %s"), I, UTF8_TO_TCHAR(C.Name)), Got, Rng, C.PXBits, C.PYBits, C.StateAfter, C.Draws, C.Settlement);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::RandomWalkCases); ++I)
	{
		const V::FRandomWalkVector& C = V::RandomWalkCases[I];
		P::FExploreFixture& F = FixtureFor(C.Seed, C.Block != 0);
		FAnastasisRng Rng(C.State);
		const E::FExploreResult Got = E::RandomWalkTarget(F.Explore, Rng, P::ExploreFromBits(C.XBits), P::ExploreFromBits(C.YBits));
		Check(FString::Printf(TEXT("randomWalkTarget[%d]"), I), Got, Rng, C.PXBits, C.PYBits, C.StateAfter, C.Draws, C.Settlement);
	}

	// markCell / cellIndex : hors du monde = -1 ; une region n'est comptee qu'une fois.
	{
		TSet<int32> Cells;
		int32 Count = 0;
		TestEqual(TEXT("cellIndex hors du monde"), E::CellIndex(108, 114, -1, 3), -1);
		TestEqual(TEXT("cellIndex bord"), E::CellIndex(108, 114, 107, 113), 14 * 14 + 13);
		TestTrue(TEXT("markCell nouvelle region"), E::MarkCell(Cells, Count, 108, 114, 9, 17));
		TestFalse(TEXT("markCell meme region"), E::MarkCell(Cells, Count, 108, 114, 15, 23));
		TestEqual(TEXT("cellCount"), Count, 1);
		Compared += 5;
	}

	if (Errors > 30) AddError(FString::Printf(TEXT("... %d ecarts en tout"), Errors));
	AddInfo(FString::Printf(TEXT("%d valeurs comparees (%d exploreTarget, %d randomWalkTarget), %d ecarts"), Compared,
		static_cast<int32>(UE_ARRAY_COUNT(V::ExploreCases)), static_cast<int32>(UE_ARRAY_COUNT(V::RandomWalkCases)), Errors));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisDecisionDrawsTest,
	"Anastasis.Sim.Village.TiragesDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisDecisionDrawsTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisExploreParity;
	namespace V = AnastasisExploreParity::Vecteurs;

	// Le scenario du harnais, repris par l'hote (comme Anastasis.Sim.Harnais.Trace).
	const FString Scenario = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("tools/migration/scenarios/endurance.json")));
	AnastasisHarnessTrace::FScenarioInfo Info;
	AnastasisJsSave::FState Read;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("scenario lu (%s)"), *Error), AnastasisHarnessTrace::LoadScenario(Scenario, Info, Read, Error)))
	{
		return false;
	}
	FAnastasisSimulation Sim;
	if (!TestTrue(FString::Printf(TEXT("reprise (%s)"), *Error), AnastasisHarnessTrace::Restore(Read, Sim, Error)))
	{
		return false;
	}
	AnastasisVillage::FVillage& Village = Sim.GetVillage();

	// Chaque decision mesuree : la photo de l'habitant posee, le flux pose a l'etat du
	// premier tirage porte, puis la decision C++. Elle doit tirer exactement ce que la
	// reference a tire (exploreTarget puis les bruits) et laisser le flux au meme etat.
	int32 Replayed = 0;
	int32 Skipped = 0;
	int32 Errors = 0;
	for (int32 I = 0; I < UE_ARRAY_COUNT(V::DecisionDraws); ++I)
	{
		const V::FDecisionDrawVector& D = V::DecisionDraws[I];
		const FString Where = FString::Printf(TEXT("tick %d %s"), D.Tick, UTF8_TO_TCHAR(D.Npc));
		// Une intention du jour « explore » ferait tirer intentExploreHint : non porte (ecart n°24).
		if (D.Contiguous == 0 || FString(UTF8_TO_TCHAR(D.DayIntent)) == TEXT("explore"))
		{
			++Skipped;
			continue;
		}
		AnastasisVillage::FNpc* Npc = Village.FindNpcMutable(FString(UTF8_TO_TCHAR(D.Npc)));
		if (!Npc)
		{
			AddError(Where + TEXT(" : habitant absent du scenario"));
			continue;
		}
		Npc->X = P::ExploreFromBits(D.XBits);
		Npc->Y = P::ExploreFromBits(D.YBits);
		Npc->KnownCells.Reset();
		for (int32 K = 0; K < D.CellCount; ++K) Npc->KnownCells.Add(V::DecisionCells[D.FirstCell + K]);
		Npc->CellCount = D.CellCount;
		Village.SetSimRngState(D.State);
		Village.ChooseGoalNow(Npc->Id);
		const AnastasisVillage::FNpc* After = Village.FindNpc(FString(UTF8_TO_TCHAR(D.Npc)));
		++Replayed;
		if (After->LastDecision.ExploreDraws != D.ExploreDraws || After->LastDecision.NoiseDraws != D.NoiseDraws
			|| Village.GetSimRngState() != D.StateAfter)
		{
			if (++Errors <= 30)
			{
				AddError(FString::Printf(TEXT("%s : exploreTarget %d / %d, bruits %d / %d, etat %u / %u (obtenu / attendu)"), *Where,
					After->LastDecision.ExploreDraws, D.ExploreDraws, After->LastDecision.NoiseDraws, D.NoiseDraws,
					Village.GetSimRngState(), D.StateAfter));
			}
		}
	}
	if (Errors > 30) AddError(FString::Printf(TEXT("... %d decisions fausses en tout"), Errors));
	TestTrue(TEXT("au moins une decision rejouee"), Replayed > 0);
	AddInfo(FString::Printf(TEXT("%d decisions mesurees rejouees, %d ecartees (intention explore ou bloc non contigu), %d fausses"),
		Replayed, Skipped, Errors));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
