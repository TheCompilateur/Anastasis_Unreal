#include "Misc/AutomationTest.h"

#include "Core/AnastasisStateDigest.h"
#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS

// IRON_CRUSADE_001 -- premiere breche : l'empreinte `FVillage::Digest` est-elle un oracle d'egalite d'etat ?
//
// Elle est la projection de parite JS (perimetre fige pour que les vecteurs JS ne bougent pas), mais les
// tests C++ s'en servent comme oracle general : « deterministe » (Endurance, Repousse.Hote, Puits.MultiAgents),
// « la presentation n'ecrit pas dans la simulation » (Villager), « observer mode: same digest » (Player),
// « l'empreinte ne bouge pas » (Mortality). Cette experience pose deux simulations identiques, ecrit dans
// UNE SEULE un champ que l'empreinte ne lit pas, puis :
//   1. compare l'empreinte juste apres l'ecriture (l'assertion des tests ci-dessus) ;
//   2. avance les deux et releve le premier pas ou les empreintes divergent.
// Un temoin sans ecriture borne la mesure : il ne doit jamais diverger.
//
// Experience, pas un test de non-regression : elle ne corrige rien et ne touche aucun code du jeu.

namespace AnastasisIronDigestBlindness
{
	using namespace AnastasisVillage;

	constexpr double Dt = 1.0 / 60.0;
	constexpr uint32 Seed = 12345u;
	constexpr int32 Size = 96;
	constexpr int32 NpcCount = 8;
	constexpr int32 WarmupTicks = 60 * 20;        // 20 s simulees : chacun a pense et bouge
	constexpr int32 HorizonTicks = 60 * 90 * 2;   // deux jours simules apres l'ecriture

	enum class EMutation : uint8 { None, Speed, ThinkClock, Reputation, SharedRng };

	const TCHAR* Name(EMutation M)
	{
		switch (M)
		{
		case EMutation::None: return TEXT("temoin (aucune ecriture)");
		case EMutation::Speed: return TEXT("FNpc::Speed x1,25 sur un habitant");
		case EMutation::ThinkClock: return TEXT("FNpc::AiThinkAt +3 s sur un habitant");
		case EMutation::Reputation: return TEXT("FNpc::Reputation +0,2 sur un habitant");
		case EMutation::SharedRng: return TEXT("etat du flux partage sim.rng ^ 1");
		}
		return TEXT("?");
	}

	/** Puits au centre du village et habitants assoiffes autour, sur la carte canonique 96x96 de la graine 12345. */
	void Populate(FAnastasisSimulation& Sim)
	{
		Sim.Reset(Seed, Size, Size);
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

	void Mutate(FAnastasisSimulation& Sim, EMutation M)
	{
		FVillage& Village = Sim.GetVillage();
		FNpc* Npc = Village.GetActors().Num() > 0 ? Village.FindNpcMutable(Village.GetActors()[0].Id) : nullptr;
		switch (M)
		{
		case EMutation::None: break;
		case EMutation::Speed: if (Npc) Npc->Speed *= 1.25; break;
		case EMutation::ThinkClock: if (Npc) Npc->AiThinkAt = Sim.GetTime() + 3.0; break;
		case EMutation::Reputation: if (Npc) Npc->Reputation += 0.2; break;
		case EMutation::SharedRng: Village.SetSimRngState(Village.GetSimRngState() ^ 1u); break;
		}
	}

	struct FResult
	{
		bool bEqualAfterWrite = false;
		int32 FirstDivergentTick = INDEX_NONE; // pas apres l'ecriture, INDEX_NONE = jamais dans l'horizon
		int32 Npcs = 0;
	};

	FResult Run(EMutation M)
	{
		FAnastasisSimulation A, B;
		Populate(A);
		Populate(B);
		for (int32 I = 0; I < WarmupTicks; ++I) { A.Tick(Dt); B.Tick(Dt); }
		FResult R;
		R.Npcs = A.GetVillage().GetActors().Num();
		Mutate(B, M);
		R.bEqualAfterWrite = A.GetVillage().Digest() == B.GetVillage().Digest();
		for (int32 I = 1; I <= HorizonTicks; ++I)
		{
			A.Tick(Dt);
			B.Tick(Dt);
			if (A.GetVillage().Digest() != B.GetVillage().Digest())
			{
				R.FirstDivergentTick = I;
				break;
			}
		}
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisIronDigestBlindnessTest,
	"Anastasis.Iron.Empreinte.AveugleAuxEcritures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisIronDigestBlindnessTest::RunTest(const FString&)
{
	using namespace AnastasisIronDigestBlindness;

	const FResult Control = Run(EMutation::None);
	AddInfo(FString::Printf(TEXT("IRON_DIGEST %s : habitants=%d egale_apres_ecriture=%d premiere_divergence=%d"),
		Name(EMutation::None), Control.Npcs, Control.bEqualAfterWrite ? 1 : 0, Control.FirstDivergentTick));
	TestEqual(TEXT("habitants poses"), Control.Npcs, NpcCount);
	TestTrue(TEXT("temoin : egal au depart"), Control.bEqualAfterWrite);
	TestEqual(TEXT("temoin : jamais de divergence sur deux jours (la simulation est deterministe)"),
		Control.FirstDivergentTick, static_cast<int32>(INDEX_NONE));

	for (const EMutation M : { EMutation::Speed, EMutation::ThinkClock, EMutation::Reputation, EMutation::SharedRng })
	{
		const FResult R = Run(M);
		const double Seconds = R.FirstDivergentTick == INDEX_NONE ? -1.0 : R.FirstDivergentTick * Dt;
		AddInfo(FString::Printf(TEXT("IRON_DIGEST %s : egale_apres_ecriture=%d premiere_divergence=%d pas (%.2f s simulees)"),
			Name(M), R.bEqualAfterWrite ? 1 : 0, R.FirstDivergentTick, Seconds));
		// Mesure 1, attendue par lecture du code : l'empreinte ne voit pas l'ecriture.
		TestTrue(*FString::Printf(TEXT("%s : l'empreinte ne voit pas l'ecriture"), Name(M)), R.bEqualAfterWrite);
	}
	// Mesure 2 (le futur diverge-t-il ?) est rapportee par les lignes IRON_DIGEST, pas assertee :
	// c'est l'observation que l'experience vient chercher.
	return true;
}

#endif
