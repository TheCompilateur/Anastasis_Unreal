#include "Misc/AutomationTest.h"

#include "Core/AnastasisStateDigest.h"
#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisWeatherBehavior.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS

// STATE_ORACLE_001 -- `StateDigest()`, l'oracle d'egalite d'etat, contre `Digest()`, la projection JS.
//
// IRON_CRUSADE_001 a pose deux simulations identiques, ecrit dans UNE un champ que `Digest()` ne lit pas,
// et vu l'empreinte rester egale pendant que les futurs divergeaient (Speed : 7,8 s ; sim.rng : 9,0 s).
// Ce test fixe le contrat de l'oracle :
//   1. deux simulations construites et avancees pareil ont le meme etat complet (determinisme, 2 jours) ;
//   2. chaque ecriture que `Digest()` ne voit pas change `StateDigest()` a l'instant meme ;
//   3. `Digest()` reste aveugle a ces ecritures : la projection de parite ne bouge pas (elle est figee).

namespace AnastasisStateDigestTest
{
	using namespace AnastasisVillage;

	constexpr double Dt = 1.0 / 60.0;
	constexpr int32 NpcCount = 8;
	constexpr int32 WarmupTicks = 60 * 20;

	/** Le scenario de la breche : puits au centre du village, huit habitants assoiffes autour (graine 12345, 96x96). */
	void Populate(FAnastasisSimulation& Sim)
	{
		Sim.Reset(12345u, 96, 96);
		FVillage& Village = Sim.GetVillage();
		const FPoint Centre = Village.GetSettlement();
		const int32 CX = FMath::FloorToInt32(Centre.X), CY = FMath::FloorToInt32(Centre.Y);
		Village.AddBuilding(WellType, CX, CY);
		int32 Spawned = 0;
		for (int32 R = 2; R <= 8 && Spawned < NpcCount; ++R)
		for (int32 DY = -R; DY <= R && Spawned < NpcCount; DY += 2)
		for (int32 DX = -R; DX <= R && Spawned < NpcCount; DX += 2)
		{
			if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
			const double X = CX + DX + 0.5, Y = CY + DY + 0.5;
			if (Village.IsFootBlocked(X, Y)) continue;
			AnastasisNeeds::FNeeds Needs;
			Needs.Thirst = 45.0 + 4.0 * Spawned;
			Needs.Hunger = 20.0 + 3.0 * Spawned;
			Village.SpawnNpc(X, Y, Needs);
			++Spawned;
		}
	}

	struct FWrite
	{
		const TCHAR* Name;
		TFunction<void(FAnastasisSimulation&)> Apply;
	};

	TArray<FWrite> Writes()
	{
		auto First = [](FAnastasisSimulation& Sim) { return Sim.GetVillage().FindNpcMutable(Sim.GetVillage().GetActors()[0].Id); };
		return {
			{ TEXT("FNpc::Speed"), [First](FAnastasisSimulation& S) { First(S)->Speed *= 1.25; } },
			{ TEXT("FNpc::AiThinkAt"), [First](FAnastasisSimulation& S) { First(S)->AiThinkAt = S.GetTime() + 3.0; } },
			{ TEXT("FNpc::Reputation"), [First](FAnastasisSimulation& S) { First(S)->Reputation += 0.2; } },
			{ TEXT("FNpc::Relations"), [First](FAnastasisSimulation& S) { First(S)->Relations.Add(TPair<FString, double>(TEXT("npc-99"), 1.0)); } },
			{ TEXT("FNpc::KnownCells"), [First](FAnastasisSimulation& S) { First(S)->KnownCells.Add(9999); } },
			{ TEXT("FNpc::Path"), [First](FAnastasisSimulation& S) { First(S)->Path.Add({ 1.5, 1.5 }); } },
			{ TEXT("sim.rng"), [](FAnastasisSimulation& S) { S.GetVillage().SetSimRngState(S.GetVillage().GetSimRngState() ^ 1u); } },
			{ TEXT("meteo forcee"), [](FAnastasisSimulation& S)
				{
					AnastasisWeatherBehavior::FSimWeather Storm;
					Storm.Rain = 0.9;
					S.GetVillage().SetForcedWeather(Storm);
				} },
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisStateDigestDeterminismTest,
	"Anastasis.Sim.Empreinte.Etat.Deterministe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisStateDigestDeterminismTest::RunTest(const FString&)
{
	using namespace AnastasisStateDigestTest;
	FAnastasisSimulation A, B;
	Populate(A);
	Populate(B);
	TestEqual(TEXT("habitants poses"), A.GetVillage().GetActors().Num(), NpcCount);
	TestEqual(TEXT("meme etat au depart"), A.StateDigest(), B.StateDigest());
	const uint64 Start = A.StateDigest();
	for (int32 I = 0; I < 60 * 90 * 2; ++I) { A.Tick(Dt); B.Tick(Dt); }
	TestEqual(TEXT("deux jours plus tard : meme etat complet"), A.StateDigest(), B.StateDigest());
	TestEqual(TEXT("... et meme projection JS"), A.GetVillage().Digest(), B.GetVillage().Digest());
	TestNotEqual(TEXT("l'etat a bien change en deux jours"), A.StateDigest(), Start);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisStateDigestSeesWritesTest,
	"Anastasis.Sim.Empreinte.Etat.VoitLesEcritures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisStateDigestSeesWritesTest::RunTest(const FString&)
{
	using namespace AnastasisStateDigestTest;
	for (const FWrite& Write : Writes())
	{
		FAnastasisSimulation A, B;
		Populate(A);
		Populate(B);
		for (int32 I = 0; I < WarmupTicks; ++I) { A.Tick(Dt); B.Tick(Dt); }
		if (!TestEqual(*FString::Printf(TEXT("%s : meme etat avant l'ecriture"), Write.Name), A.StateDigest(), B.StateDigest()))
		{
			continue;
		}
		Write.Apply(B);
		TestNotEqual(*FString::Printf(TEXT("%s : StateDigest voit l'ecriture"), Write.Name),
			A.StateDigest(), B.StateDigest());
		TestNotEqual(*FString::Printf(TEXT("%s : FVillage::StateDigest la voit aussi"), Write.Name),
			A.GetVillage().StateDigest(), B.GetVillage().StateDigest());
		TestEqual(*FString::Printf(TEXT("%s : Digest(), projection JS figee, ne la voit pas"), Write.Name),
			A.GetVillage().Digest(), B.GetVillage().Digest());
	}
	return true;
}

#endif
