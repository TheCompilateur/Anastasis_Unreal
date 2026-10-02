#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"

#include "World/AnastasisNavGrid.h"
#include "World/AnastasisNavService.h"
#include "World/AnastasisPathfinding.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Parite du service de navigation contre `src/sim/navService.js`.
 *
 * Deux preuves. Les fonctions pures (TTL, sweep, budget par vitesse, cles de
 * cache, priorites) se comparent valeur par valeur. Le reste est un module A
 * ETAT: des scenarios rejouent la meme suite d'operations des deux cotes, et
 * comparent apres CHAQUE operation le SHA-1 d'un texte canonique de l'etat —
 * file, index des attentes, cache dans son ordre d'insertion, champs de chemin
 * de chaque PNJ, metriques, anneau de trace.
 *
 * Le texte canonique est ecrit ici EXACTEMENT comme `canon()` l'ecrit dans
 * tools/migration/gen-nav-service-vectors.mjs. En cas d'echec, le texte C++ du
 * premier pas fautif part dans le journal; `-dump` cote JS donne le sien.
 *
 * Generes par tools/migration/gen-nav-service-vectors.mjs. Ne jamais corriger
 * un vecteur a la main: soit le portage a devie, soit la reference a change et
 * il faut regenerer.
 */
namespace AnastasisNavServiceParity
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

	/** Les codes de `OP` dans le generateur. */
	enum EOp : int32
	{
		OpTime = 0,
		OpSpeed = 1,
		OpVersion = 2,
		OpBegin = 3,
		OpRequest = 4,
		OpProcess = 5,
		OpMove = 6,
		OpGoal = 7,
		OpTarget = 8,
		OpRemove = 9,
		OpRestore = 10,
		OpClear = 11,
		OpSweep = 12,
		OpLookup = 13,
		OpCacheEmpty = 14,
	};

	static FString Hex(double Value)
	{
		return FString::Printf(TEXT("%016llx"), ToBits(Value));
	}

	/** FNV-1a 32 sur des octets ASCII, comme `fnv32` cote JS. */
	static uint32 Fnv32(const FString& Text)
	{
		uint32 Hash = 0x811c9dc5u;
		for (const TCHAR Char : Text)
		{
			Hash ^= static_cast<uint32>(Char) & 0xffu;
			Hash *= 0x01000193u;
		}
		return Hash;
	}

	/**
	 * `pathSig`. Le JS distingue `null` (`0:00000000`) du tableau vide
	 * (`0:811c9dc5`): un acteur n'a jamais de tableau vide, une entree de cache
	 * ou un chemin rendu peut en avoir un.
	 */
	static FString PathSig(const TArray<AnastasisPath::FPoint>& Path, bool bArray)
	{
		if (!bArray)
		{
			return TEXT("0:00000000");
		}
		FString Joined;
		for (const AnastasisPath::FPoint& Point : Path)
		{
			Joined += Hex(Point.X);
			Joined += TEXT(",");
			Joined += Hex(Point.Y);
			Joined += TEXT(";");
		}
		return FString::Printf(TEXT("%d:%08x"), Path.Num(), Fnv32(Joined));
	}

	static FString Sha1Hex(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		uint8 Digest[FSHA1::DigestSize];
		FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Digest);
		FString Out;
		for (const uint8 Byte : Digest)
		{
			Out += FString::Printf(TEXT("%02x"), Byte);
		}
		return Out;
	}

	/** Le `sim` du generateur: monde, temps, vitesse, version, acteurs, metriques. */
	class FHost final : public AnastasisNavService::INavServiceHost
	{
	public:
		explicit FHost(uint32 Seed)
			: World(AnastasisWorld::GenerateWorld(Seed, NavServiceWorldW, NavServiceWorldH))
		{
			AnastasisNav::InitFromWorld(Grid, World);
			Source = MakeUnique<AnastasisPath::FWorldNavSource>(Grid, World);
			Roster.SetNum(NavServiceActorCount);
			for (int32 Index = 0; Index < NavServiceActorCount; ++Index)
			{
				AnastasisNavService::FNavAgent& Agent = Roster[Index];
				Agent.Id = FString::Printf(TEXT("npc-%d"), Index);
				Agent.X = 0.5;
				Agent.Y = 0.5;
				Agent.Goal = TEXT("wander");
				Live.Add(Index);
			}
		}

		virtual double GetTime() const override { return Time; }
		virtual double GetSpeedScale() const override { return Speed; }
		virtual int32 GetNavVersion() const override { return NavVersion; }
		virtual const AnastasisPath::INavSource& GetNavSource() const override { return *Source; }
		virtual AnastasisNavService::FNavMetrics* GetMetrics() override { return &Metrics; }

		virtual AnastasisNavService::FNavAgent* FindLiveAgent(const FString& Id) override
		{
			for (const int32 Index : Live)
			{
				if (Roster[Index].Id == Id)
				{
					return &Roster[Index];
				}
			}
			return nullptr;
		}

		AnastasisWorld::FWorld World;
		AnastasisNav::FNavGrid Grid;
		TUniquePtr<AnastasisPath::FWorldNavSource> Source;
		/** Taille fixe: les `FNavAgent*` rendus restent valides. */
		TArray<AnastasisNavService::FNavAgent> Roster;
		/** `sim.actors`, en rangs dans `Roster`, dans l'ordre. */
		TArray<int32> Live;
		AnastasisNavService::FNavMetrics Metrics;
		double Time = 0.0;
		double Speed = 1.0;
		int32 NavVersion = 0;
	};

	static FString PathResult(bool bFound, const TArray<AnastasisPath::FPoint>& Path)
	{
		return bFound ? FString::Printf(TEXT("path:%s"), *PathSig(Path, true)) : FString(TEXT("null"));
	}

	static FString ApplyOp(FHost& Host, AnastasisNavService::FNavService& Service, const FNavServiceOp& Op)
	{
		AnastasisNavService::FNavAgent* Actor = Op.Actor >= 0 ? &Host.Roster[Op.Actor] : nullptr;
		switch (Op.Kind)
		{
		case OpTime: Host.Time = FromBits(Op.A); return FString();
		case OpSpeed: Host.Speed = FromBits(Op.A); return FString();
		case OpVersion: Host.NavVersion = Op.I; return FString();
		case OpBegin: Service.BeginNavTick(Host, FromBits(Op.A)); return FString();
		case OpRequest:
		{
			AnastasisNavService::FRequestOptions Options;
			if (Op.I >= 0)
			{
				Options.Priority = Op.I;
			}
			if (Op.J >= 0)
			{
				Options.bAllowBlockedTarget = Op.J == 1;
			}
			TArray<AnastasisPath::FPoint> Path;
			const bool bFound = Service.RequestPath(Host, *Actor, { FromBits(Op.A), FromBits(Op.B) }, Options, &Path);
			return PathResult(bFound, Path);
		}
		case OpProcess:
		{
			AnastasisNavService::FProcessOptions Options;
			Options.MaxJobs = Op.I;
			if (Actor)
			{
				Options.FavorActorId = Actor->Id;
			}
			return FString::Printf(TEXT("%d"), Service.ProcessNavQueue(Host, Options));
		}
		case OpMove: Actor->X = FromBits(Op.A); Actor->Y = FromBits(Op.B); return FString();
		case OpGoal:
			Actor->Goal = FString(Op.Text);
			Actor->Hunger = FromBits(Op.A);
			Actor->Thirst = FromBits(Op.B);
			Actor->Energy = FromBits(Op.C);
			return FString();
		case OpTarget:
			Actor->bHasTarget = Op.I == 1;
			Actor->Target = Actor->bHasTarget ? AnastasisPath::FPoint{ FromBits(Op.A), FromBits(Op.B) } : AnastasisPath::FPoint();
			return FString();
		case OpRemove: Host.Live.Remove(Op.Actor); return FString();
		case OpRestore: Host.Live.AddUnique(Op.Actor); return FString();
		case OpClear: Service.ClearNavCache(Host); return FString();
		case OpSweep: return FString::Printf(TEXT("%d"), Service.SweepNavCache(Host));
		case OpLookup:
		{
			TArray<AnastasisPath::FPoint> Path;
			const bool bFound = Service.LookupCachedPath(
				Host, { FromBits(Op.A), FromBits(Op.B) }, { FromBits(Op.C), FromBits(Op.D) }, Path);
			return PathResult(bFound, Path);
		}
		case OpCacheEmpty: Service.Cache.Empty(); return FString();
		default: return FString::Printf(TEXT("operation inconnue %d"), Op.Kind);
		}
	}

	/** `canon()` du generateur, ligne pour ligne. */
	static FString Canon(const FHost& Host, const AnastasisNavService::FNavService& Service, const FString& Result)
	{
		FString Out;
		Out.Reserve(4096);
		Out += FString::Printf(TEXT("R|%s\n"), *Result);
		Out += FString::Printf(TEXT("S|%d|%d|%s|%s\n"),
			Service.CalcThisTick, Service.MaxCalcs, *Hex(Service.CacheTtl), *Hex(Service.LastSweepAt));
		for (const AnastasisNavService::FNavJob& Job : Service.Queue)
		{
			Out += FString::Printf(TEXT("Q|%s|%d|%s|%s|%d|%s|%s|%s|%s\n"),
				*Job.ActorId, Job.Priority, *Hex(Job.RequestedAt), *Job.GoalKey, Job.bAllowBlockedTarget ? 1 : 0,
				*Hex(Job.Destination.X), *Hex(Job.Destination.Y), *Hex(Job.Start.X), *Hex(Job.Start.Y));
		}

		TArray<FString> PendingKeys;
		Service.PendingByActor.GetKeys(PendingKeys);
		PendingKeys.Sort([](const FString& A, const FString& B)
		{
			return A.Compare(B, ESearchCase::CaseSensitive) < 0;
		});
		FString Pending;
		for (int32 Index = 0; Index < PendingKeys.Num(); ++Index)
		{
			if (Index > 0)
			{
				Pending += TEXT(";");
			}
			Pending += FString::Printf(TEXT("%s=%d"), *PendingKeys[Index], Service.PendingByActor[PendingKeys[Index]]);
		}
		Out += FString::Printf(TEXT("P|%s\n"), *Pending);

		const TArray<FString>& Keys = Service.Cache.GetKeys();
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			const AnastasisNavService::FNavCacheEntry& Entry = Service.Cache.GetEntry(Index);
			Out += FString::Printf(TEXT("C|%s|%d|%s|%s\n"),
				*Keys[Index], Entry.NavVersion, *Hex(Entry.StoredAt), *PathSig(Entry.Path, true));
		}

		for (int32 Index = 0; Index < Host.Roster.Num(); ++Index)
		{
			const AnastasisNavService::FNavAgent& A = Host.Roster[Index];
			const FString Goal = A.bHasPathGoal
				? FString::Printf(TEXT("%s,%s"), *Hex(A.PathGoal.X), *Hex(A.PathGoal.Y))
				: FString(TEXT("-"));
			Out += FString::Printf(TEXT("A|%s|%d|%s|%s|%s|%d|%s|%d|%d|%s|%s|%s|%d\n"),
				*A.Id, Host.Live.Contains(Index) ? 1 : 0, *Hex(A.X), *Hex(A.Y),
				*PathSig(A.Path, A.Path.Num() > 0), A.PathStep, *Goal,
				A.bPathFailed ? 1 : 0, A.PathFailStreak, *Hex(A.PathCooldown),
				*A.NavTargetKey, *Hex(A.NavRequestedAt), A.NavVersion);
		}

		const AnastasisNavService::FNavMetrics& M = Host.Metrics;
		Out += FString::Printf(TEXT("M|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d\n"),
			M.PathRequests, M.PathHits, M.PathFails, M.CacheHits, M.CacheMisses, M.CacheSize,
			M.QueueDepth, M.QueueSolved, M.CalcsThisTick, M.PathQueueEnqueued, M.PathCacheHits,
			M.NavTraceNext, M.NavTraceTotal);
		for (const AnastasisNavService::FNavTraceSample& T : M.NavTrace)
		{
			Out += FString::Printf(TEXT("T|%s|%s|%s|%s|%s|%d|%s|%s|%s|%d|%d\n"),
				*T.Type, *Hex(T.Time), *T.ActorId, *T.Goal, *T.TargetKey, T.NavVersion,
				*T.From, *T.To, *T.Reason, T.PathLength, T.QueueDepth);
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisNavServiceFunctionsTest,
	"Anastasis.Sim.Parite.NavServiceFonctions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisNavServiceFunctionsTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisNavServiceParity;
	namespace NS = AnastasisNavService;

	for (const P::FNavServiceSpeedVector& V : P::NavServiceSpeedVectors)
	{
		const double Speed = P::FromBits(V.SpeedBits);
		const uint64 Mult = P::ToBits(NS::NavStepMultForSpeed(Speed));
		const uint64 Ttl = P::ToBits(NS::NavCacheTtlForSpeed(Speed));
		const uint64 Sweep = P::ToBits(NS::NavCacheSweepIntervalForSpeed(Speed));
		const int32 Calcs = NS::PathBudgetForSpeed(Speed);
		if (Mult != V.StepMultBits || Ttl != V.TtlBits || Sweep != V.SweepBits || Calcs != V.MaxCalcs)
		{
			AddError(FString::Printf(
				TEXT("vitesse %016llx: stepMult %016llx/%016llx ttl %016llx/%016llx sweep %016llx/%016llx calcs %d/%d (attendu/obtenu)"),
				V.SpeedBits, V.StepMultBits, Mult, V.TtlBits, Ttl, V.SweepBits, Sweep, V.MaxCalcs, Calcs));
		}
	}

	for (const P::FNavServiceKeyVector& V : P::NavServiceKeyVectors)
	{
		const FString Key = NS::CacheKeyFor(
			{ P::FromBits(V.SX), P::FromBits(V.SY) }, { P::FromBits(V.TX), P::FromBits(V.TY) }, V.Version, V.Zone != 0);
		const FString Expected(V.Key);
		if (Key != Expected)
		{
			AddError(FString::Printf(TEXT("cacheKeyFor: attendu '%s', obtenu '%s'"), *Expected, *Key));
		}
	}

	for (const P::FNavServiceTargetKeyVector& V : P::NavServiceTargetKeyVectors)
	{
		const FString Key = NS::NavigationTargetKey(NS::FPoint{ P::FromBits(V.X), P::FromBits(V.Y) });
		const FString Expected(V.Key);
		if (Key != Expected)
		{
			AddError(FString::Printf(TEXT("navigationTargetKey: attendu '%s', obtenu '%s'"), *Expected, *Key));
		}
	}

	for (const P::FNavServicePriorityVector& V : P::NavServicePriorityVectors)
	{
		const int32 Priority = NS::PriorityForGoal(
			FString(V.Goal), P::FromBits(V.Hunger), P::FromBits(V.Thirst), P::FromBits(V.Energy));
		if (Priority != V.Priority)
		{
			AddError(FString::Printf(TEXT("priorityForGoal('%s', %016llx, %016llx, %016llx): attendu %d, obtenu %d"),
				*FString(V.Goal), V.Hunger, V.Thirst, V.Energy, V.Priority, Priority));
		}
	}

	AddInfo(FString::Printf(TEXT("%d vitesses, %d cles, %d cles de cible, %d priorites"),
		static_cast<int32>(UE_ARRAY_COUNT(P::NavServiceSpeedVectors)),
		static_cast<int32>(UE_ARRAY_COUNT(P::NavServiceKeyVectors)),
		static_cast<int32>(UE_ARRAY_COUNT(P::NavServiceTargetKeyVectors)),
		static_cast<int32>(UE_ARRAY_COUNT(P::NavServicePriorityVectors))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisNavServiceScenarioTest,
	"Anastasis.Sim.Parite.NavService",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisNavServiceScenarioTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisNavServiceParity;

	int32 TotalOps = 0;
	for (const P::FNavServiceScenario& Scenario : P::NavServiceScenarios)
	{
		const FString Name(Scenario.Name);
		P::FHost Host(Scenario.Seed);
		AnastasisNavService::FNavService Service;
		for (int32 Step = 0; Step < Scenario.OpCount; ++Step)
		{
			const P::FNavServiceOp& Op = P::NavServiceOps[Scenario.FirstOp + Step];
			const FString Result = P::ApplyOp(Host, Service, Op);
			const FString Text = P::Canon(Host, Service, Result);
			const FString Sha = P::Sha1Hex(Text);
			TotalOps += 1;
			if (Sha != FString(Op.Sha1))
			{
				AddError(FString::Printf(
					TEXT("%s #%d (op %d): empreinte %s, attendu %s — file %d/%d, cache %d/%d (obtenu/attendu)"),
					*Name, Step, Op.Kind, *Sha, *FString(Op.Sha1),
					Service.Queue.Num(), Op.QueueLength, Service.Cache.Num(), Op.CacheSize));
				// Le texte complet, pour le comparer au `-dump` du generateur.
				TArray<FString> Lines;
				Text.ParseIntoArrayLines(Lines);
				for (const FString& Line : Lines)
				{
					AddInfo(FString::Printf(TEXT("  %s"), *Line));
				}
				break;
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d scenarios, %d operations comparees"),
		static_cast<int32>(UE_ARRAY_COUNT(P::NavServiceScenarios)), TotalOps));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
