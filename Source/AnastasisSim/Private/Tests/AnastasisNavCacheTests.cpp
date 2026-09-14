#include "Misc/AutomationTest.h"

#include "World/AnastasisNavCache.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisNavCacheParity
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

	#include "AnastasisNavServiceVectors.inl"
	#include "AnastasisNavCacheVectors.inl"
}

/**
 * Parite de la part scalaire du service de navigation.
 *
 * Declaree dans tools/migration/parity/nav-service.mjs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisNavServiceParityTest,
	"Anastasis.Sim.Parite.NavService",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisNavServiceParityTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisNavCacheParity;

	auto VerifierDouble = [this](const TCHAR* Quoi, double Vitesse, double Obtenu, uint64 Attendu)
	{
		if (ToBits(Obtenu) != Attendu)
		{
			AddError(FString::Printf(
				TEXT("%s pour vitesse %.9g: attendu %016llx, obtenu %016llx"),
				Quoi, Vitesse, Attendu, ToBits(Obtenu)));
		}
	};

	for (const FNavStepMultVector& Vecteur : NavStepMultVectors)
	{
		const double V = FromBits(Vecteur.A0Bits);
		VerifierDouble(TEXT("stepMult"), V, AnastasisNavCache::StepMultForSpeed(V), Vecteur.AttenduBits);
	}
	for (const FNavCacheTtlVector& Vecteur : NavCacheTtlVectors)
	{
		const double V = FromBits(Vecteur.A0Bits);
		VerifierDouble(TEXT("ttl"), V, AnastasisNavCache::CacheTtlForSpeed(V), Vecteur.AttenduBits);
	}
	for (const FNavSweepIntervalVector& Vecteur : NavSweepIntervalVectors)
	{
		const double V = FromBits(Vecteur.A0Bits);
		VerifierDouble(TEXT("sweepInterval"), V, AnastasisNavCache::SweepIntervalForSpeed(V), Vecteur.AttenduBits);
	}
	for (const FNavPathBudgetVector& Vecteur : NavPathBudgetVectors)
	{
		const double V = FromBits(Vecteur.A0Bits);
		TestEqual(
			*FString::Printf(TEXT("budget A* pour vitesse %.9g"), V),
			AnastasisNavCache::PathBudgetForSpeed(V), Vecteur.Attendu);
	}

	for (const FNavCacheKeyVector& Vecteur : NavCacheKeyVectors)
	{
		const AnastasisPath::FPoint Start{ FromBits(Vecteur.A0Bits), FromBits(Vecteur.A1Bits) };
		const AnastasisPath::FPoint Target{ FromBits(Vecteur.A2Bits), FromBits(Vecteur.A3Bits) };
		const FString Obtenu = AnastasisNavCache::CacheKeyFor(Start, Target, Vecteur.A4, Vecteur.A5 != 0);
		TestEqual(
			*FString::Printf(TEXT("cle (%.9g,%.9g)->(%.9g,%.9g) v%d zone=%d"),
				Start.X, Start.Y, Target.X, Target.Y, Vecteur.A4, Vecteur.A5),
			Obtenu, FString(UTF8_TO_TCHAR(Vecteur.Attendu)));
	}

	return true;
}

/**
 * Le cache rejoue: meme suite d'operations, memes resultats, meme ORDRE.
 *
 * L'ordre n'est pas un detail de mise en oeuvre. L'eviction retire les 80
 * premieres cles dans l'ordre d'insertion; sur un `TMap` sans ordre, ce ne
 * seraient pas les memes entrees qui partiraient — donc pas les memes chemins
 * servis, donc des habitants ailleurs. Le dernier bloc du test compare l'ordre
 * final cle par cle, et c'est lui qui attraperait cette erreur.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisNavCacheScenarioTest,
	"Anastasis.Sim.Parite.NavCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisNavCacheScenarioTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisNavCacheParity;

	AnastasisNavCache::FCache Cache;
	const double Ttl = FromBits(NavCacheTtlBits);
	int32 NavVersion = 0;
	double Time = 0.0;

	int32 Hits = 0;
	int32 Evictions = 0;
	int32 IndexOp = 0;

	for (const FNavCacheOp& Op : NavCacheOps)
	{
		const AnastasisPath::FPoint Start{ FromBits(Op.SXBits), FromBits(Op.SYBits) };
		const AnastasisPath::FPoint Target{ FromBits(Op.TXBits), FromBits(Op.TYBits) };
		const FString Ou = FString::Printf(TEXT("operation %d"), IndexOp);

		switch (Op.Op)
		{
		case ENavCacheOp::Store:
		{
			// Chemin synthetique identique a celui du generateur: le premier
			// noeud est la case de depart, ce qui rend le succes par zone
			// possible pour un depart voisin.
			TArray<AnastasisPath::FPoint> Path;
			const double BaseX = FMath::FloorToDouble(Start.X) + 0.5;
			const double BaseY = FMath::FloorToDouble(Start.Y) + 0.5;
			for (int32 I = 0; I < Op.PathLen; ++I)
			{
				Path.Add(AnastasisPath::FPoint{ BaseX + I, BaseY });
			}
			const int32 Avant = Cache.Num();
			Cache.Store(Start, Target, NavVersion, Time, Path);
			if (Cache.Num() < Avant)
			{
				Evictions += 1;
			}
			break;
		}

		case ENavCacheOp::Lookup:
		{
			TArray<AnastasisPath::FPoint> Path;
			const bool bHit = Cache.Lookup(Start, Target, NavVersion, Time, Ttl, Path);
			TestEqual(*(Ou + TEXT(": succes")), bHit ? 1 : 0, Op.Hit);
			if (bHit)
			{
				Hits += 1;
				TestEqual(*(Ou + TEXT(": longueur")), Path.Num(), Op.OutLen);
				if (Path.Num() > 0 && Op.OutLen > 0)
				{
					if (ToBits(Path[0].X) != Op.FirstXBits || ToBits(Path[0].Y) != Op.FirstYBits)
					{
						AddError(FString::Printf(
							TEXT("%s: premier noeud attendu (%016llx,%016llx), obtenu (%016llx,%016llx)"),
							*Ou, Op.FirstXBits, Op.FirstYBits, ToBits(Path[0].X), ToBits(Path[0].Y)));
					}
				}
			}
			break;
		}

		case ENavCacheOp::Sweep:
			TestEqual(*(Ou + TEXT(": entrees purgees")), Cache.Sweep(NavVersion, Time, Ttl), Op.Removed);
			break;

		case ENavCacheOp::Time:
			Time = FromBits(Op.TimeBits);
			break;

		case ENavCacheOp::Version:
			NavVersion = Op.Version;
			break;
		}

		// La taille apres CHAQUE operation, y compris apres une lecture: c'est
		// elle qui revele la purge silencieuse faite au passage.
		TestEqual(*(Ou + TEXT(": taille du cache")), Cache.Num(), Op.Size);
		IndexOp += 1;
	}

	// L'ordre final, cle par cle.
	TestEqual(TEXT("nombre de cles finales"), Cache.Num(), NavCacheFinalOrderCount);
	if (Cache.Num() == NavCacheFinalOrderCount)
	{
		const TArray<FString>& Ordre = Cache.GetOrder();
		for (int32 Index = 0; Index < NavCacheFinalOrderCount; ++Index)
		{
			const FString Attendu = UTF8_TO_TCHAR(NavCacheFinalOrder[Index]);
			if (Ordre[Index] != Attendu)
			{
				AddError(FString::Printf(
					TEXT("ordre final, rang %d: attendu %s, obtenu %s"),
					Index, *Attendu, *Ordre[Index]));
				break;
			}
		}
	}

	// Garde-fous sur le scenario lui-meme: une suite qui ne toucherait jamais
	// le cache passerait au vert sans rien prouver.
	TestTrue(TEXT("le scenario a produit des succes de cache"), Hits >= 5);
	TestTrue(TEXT("le scenario a declenche au moins une eviction"), Evictions >= 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
