#include "Misc/AutomationTest.h"

#include "World/AnastasisNavGrid.h"
#include "World/AnastasisPathfinding.h"
#include "World/AnastasisPercolation.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisPercoParity
{
	static double FromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(double));
		return Value;
	}

	static uint64 ToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(double));
		return Bits;
	}

	#include "AnastasisPercolationVectors.inl"

	/** Empreinte de la carte d'identifiants — FNV-1a 32, comme le generateur. */
	static uint32 IdsFingerprint(const TArray<int32>& Ids)
	{
		uint32 Hash = 0x811c9dc5u;
		for (const int32 Id : Ids)
		{
			const uint32 V = static_cast<uint32>(Id);
			for (int32 Byte = 0; Byte < 4; ++Byte)
			{
				Hash ^= (V >> (Byte * 8)) & 0xffu;
				Hash *= 0x01000193u;
			}
		}
		return Hash;
	}

	struct FFixture
	{
		AnastasisWorld::FWorld World;
		AnastasisNav::FNavGrid Grid;
		AnastasisPercolation::FWalkComponents Components;

		explicit FFixture(uint32 Seed)
		{
			World = AnastasisWorld::GenerateWorld(Seed, PercoW, PercoH);
			AnastasisNav::InitFromWorld(Grid, World);
			Components = AnastasisPercolation::BuildWalkComponents(Grid, World);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisPercolationParityTest,
	"Anastasis.Sim.Parite.Percolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisPercolationParityTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisPercoParity;

	TMap<uint32, TSharedPtr<FFixture>> Fixtures;

	for (const FPercoWorldVector& Vector : PercoWorlds)
	{
		TSharedPtr<FFixture> Fixture = MakeShared<FFixture>(Vector.Seed);
		Fixtures.Add(Vector.Seed, Fixture);
		const AnastasisPercolation::FWalkComponents& C = Fixture->Components;

		TestEqual(*FString::Printf(TEXT("graine %u: nombre de composantes"), Vector.Seed),
			C.Count(), Vector.ComponentCount);

		// L'empreinte des identifiants, pas seulement leur nombre: deux mondes
		// peuvent avoir autant de composantes sans avoir les memes.
		TestEqual(*FString::Printf(TEXT("graine %u: empreinte des identifiants"), Vector.Seed),
			IdsFingerprint(C.Ids), Vector.IdsFingerprint);

		if (C.Count() == Vector.ComponentCount)
		{
			for (int32 Index = 0; Index < C.Count(); ++Index)
			{
				TestEqual(
					*FString::Printf(TEXT("graine %u: taille de la composante %d"), Vector.Seed, Index),
					C.Sizes[Index], PercoSizes[Vector.FirstSize + Index]);
			}
		}
	}

	for (const FPercoSettlementVector& Vector : PercoSettlements)
	{
		const TSharedPtr<FFixture>& Fixture = Fixtures[Vector.Seed];
		const AnastasisPercolation::FWalkComponents& C = Fixture->Components;
		const FString Ou = FString::Printf(TEXT("graine %u hameau (%d,%d)"), Vector.Seed, Vector.SX, Vector.SY);

		const bool bBlocked = AnastasisNav::FootBlockedAt(Fixture->Grid, Fixture->World, Vector.SX, Vector.SY);
		TestEqual(*(Ou + TEXT(": case bloquee")), bBlocked, Vector.SettlementBlocked != 0);

		const int32 Home = AnastasisPercolation::SettlementComponent(C, Vector.SX, Vector.SY);
		TestEqual(*(Ou + TEXT(": composante du hameau")), Home, Vector.HomeComponent);

		const AnastasisPercolation::FStats Stats =
			AnastasisPercolation::ComputeStats(C, Vector.SX, Vector.SY);
		TestEqual(*(Ou + TEXT(": composantes")), Stats.Components, Vector.Components);
		TestEqual(*(Ou + TEXT(": iles")), Stats.Islands, Vector.Islands);
		TestEqual(*(Ou + TEXT(": tuiles du hameau")), Stats.HomeTiles, Vector.HomeTiles);
		TestEqual(*(Ou + TEXT(": hameau est la plus grande")), Stats.bHomeIsLargest, Vector.HomeIsLargest != 0);

		if (ToBits(Stats.WalkableFrac) != Vector.WalkableFracBits)
		{
			AddError(FString::Printf(TEXT("%s: walkableFrac attendu %016llx, obtenu %016llx"),
				*Ou, Vector.WalkableFracBits, ToBits(Stats.WalkableFrac)));
		}
		if (ToBits(Stats.MainFrac) != Vector.MainFracBits)
		{
			AddError(FString::Printf(TEXT("%s: mainFrac attendu %016llx, obtenu %016llx"),
				*Ou, Vector.MainFracBits, ToBits(Stats.MainFrac)));
		}
	}

	int32 Atteignables = 0;
	for (const FPercoProbeVector& Vector : PercoProbes)
	{
		const TSharedPtr<FFixture>& Fixture = Fixtures[Vector.Seed];
		const AnastasisPercolation::FWalkComponents& C = Fixture->Components;

		const bool bReach = AnastasisPercolation::ReachableFromSettlement(
			C, Vector.SX, Vector.SY, Vector.X, Vector.Y);
		const bool bApproach = AnastasisPercolation::ApproachableFromSettlement(
			C, Vector.SX, Vector.SY, Vector.X, Vector.Y);

		TestEqual(
			*FString::Printf(TEXT("graine %u hameau (%d,%d) -> (%d,%d) atteignable"),
				Vector.Seed, Vector.SX, Vector.SY, Vector.X, Vector.Y),
			bReach, Vector.Reachable != 0);
		TestEqual(
			*FString::Printf(TEXT("graine %u hameau (%d,%d) -> (%d,%d) abordable"),
				Vector.Seed, Vector.SX, Vector.SY, Vector.X, Vector.Y),
			bApproach, Vector.Approachable != 0);

		if (Vector.Reachable != 0)
		{
			Atteignables += 1;
		}
	}

	// Garde-fou sur la batterie: des sondes toutes negatives passeraient au vert
	// sans rien prouver.
	TestTrue(TEXT("la batterie contient des cases reellement atteignables"), Atteignables >= 20);

	return true;
}

/**
 * Le contre-test: la percolation et l'A* doivent parler du meme monde.
 *
 * La reference l'exige sans detour — « un champ qui mentirait sur ce point
 * serait pire que pas de champ du tout ». Ici on ne compare pas deux portages,
 * on compare deux SYSTEMES portes: si deux cases partagent une composante,
 * l'A* doit trouver un chemin entre elles; si elles n'en partagent pas, il ne
 * doit jamais en trouver.
 *
 * Ce test attraperait une classe d'erreur que les vecteurs laissent passer: un
 * portage ou les DEUX cotes auraient la meme regle de coin fausse resterait
 * vert en parite, et faux.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisPercolationAgreesWithPathfindingTest,
	"Anastasis.Sim.Parite.PercolationAccordA",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisPercolationAgreesWithPathfindingTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisPercoParity;

	int32 MemeComposanteTestees = 0;
	int32 AutresComposantesTestees = 0;

	for (const uint32 Seed : { PercoWorlds[0].Seed, PercoWorlds[1].Seed, PercoWorlds[2].Seed })
	{
		const FFixture Fixture(Seed);
		const AnastasisPercolation::FWalkComponents& C = Fixture.Components;
		const AnastasisPath::FWorldNavSource Source(Fixture.Grid, Fixture.World);

		// Budgets larges: on teste la CONNEXITE, pas le budget. Un A* qui
		// abandonne sur `maxExpanded` ne dirait rien sur la composante.
		AnastasisPath::FOptions Options;
		Options.MaxCost = 100000.0;
		Options.MaxExpanded = PercoW * PercoH * 4;

		// Un representant par composante: la premiere case rencontree.
		TMap<int32, FIntPoint> Representants;
		for (int32 Y = 0; Y < C.H; ++Y)
		{
			for (int32 X = 0; X < C.W; ++X)
			{
				const int32 Id = C.Ids[Y * C.W + X];
				if (Id != AnastasisPercolation::NoComponent && !Representants.Contains(Id))
				{
					Representants.Add(Id, FIntPoint(X, Y));
				}
			}
		}

		// Meme composante: un chemin doit exister. On echantillonne des cases
		// eloignees de leur representant, pas seulement voisines.
		for (const TPair<int32, FIntPoint>& Pair : Representants)
		{
			const FIntPoint Depart = Pair.Value;
			int32 Testees = 0;
			for (int32 Y = C.H - 1; Y >= 0 && Testees < 3; Y -= 7)
			{
				for (int32 X = C.W - 1; X >= 0 && Testees < 3; X -= 5)
				{
					if (C.Ids[Y * C.W + X] != Pair.Key)
					{
						continue;
					}
					if (X == Depart.X && Y == Depart.Y)
					{
						continue;
					}
					TArray<AnastasisPath::FPoint> Path;
					const bool bFound = AnastasisPath::FindPath(
						Source,
						{ Depart.X + 0.5, Depart.Y + 0.5 },
						{ X + 0.5, Y + 0.5 },
						Options, Path);
					TestTrue(
						*FString::Printf(
							TEXT("graine %u: (%d,%d) et (%d,%d) partagent la composante %d, l'A* doit relier"),
							Seed, Depart.X, Depart.Y, X, Y, Pair.Key),
						bFound);
					Testees += 1;
					MemeComposanteTestees += 1;
				}
			}
		}

		// Composantes differentes: aucun chemin ne doit exister.
		TArray<int32> Ids;
		Representants.GetKeys(Ids);
		Ids.Sort();
		for (int32 I = 0; I + 1 < Ids.Num() && AutresComposantesTestees < 24; ++I)
		{
			const FIntPoint A = Representants[Ids[I]];
			const FIntPoint B = Representants[Ids[I + 1]];
			TArray<AnastasisPath::FPoint> Path;
			const bool bFound = AnastasisPath::FindPath(
				Source, { A.X + 0.5, A.Y + 0.5 }, { B.X + 0.5, B.Y + 0.5 }, Options, Path);
			TestFalse(
				*FString::Printf(
					TEXT("graine %u: (%d,%d) et (%d,%d) sont dans les composantes %d et %d, l'A* ne doit pas relier"),
					Seed, A.X, A.Y, B.X, B.Y, Ids[I], Ids[I + 1]),
				bFound);
			AutresComposantesTestees += 1;
		}
	}

	TestTrue(TEXT("des paires de meme composante ont ete testees"), MemeComposanteTestees >= 10);
	TestTrue(TEXT("des paires de composantes differentes ont ete testees"), AutresComposantesTestees >= 5);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
