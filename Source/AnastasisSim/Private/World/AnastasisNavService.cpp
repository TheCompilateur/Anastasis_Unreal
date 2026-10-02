#include "World/AnastasisNavService.h"

#include "Algo/StableSort.h"
#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimClock.h"

namespace AnastasisNavService
{
	namespace
	{
		/** `sim.speedScale || 1`. */
		double SpeedOf(const INavServiceHost& Host)
		{
			return AnastasisJs::NumberOr(Host.GetSpeedScale(), 1.0);
		}

		/** `sim.time || 0` — NaN et 0 rendent 0, le reste passe. */
		double TimeOf(const INavServiceHost& Host)
		{
			return AnastasisJs::NumberOrZero(Host.GetTime());
		}

		/**
		 * Un `Math.floor(v)` ecrit comme JS ecrit un nombre dans un gabarit.
		 * La valeur est entiere: seuls `-0`, NaN et les infinis demandent un soin.
		 */
		FString JsFlooredToString(double Value)
		{
			const double Floored = AnastasisJs::Floor(Value);
			if (FMath::IsNaN(Floored))
			{
				return TEXT("NaN");
			}
			if (!FMath::IsFinite(Floored))
			{
				return Floored > 0.0 ? TEXT("Infinity") : TEXT("-Infinity");
			}
			// Au-dela de 2^53 la conversion resterait exacte, mais JS passe en
			// notation exponentielle des 1e21: aucune case n'en est la.
			return FString::Printf(TEXT("%lld"), static_cast<long long>(Floored));
		}

		/** `isCacheEntryFresh`. */
		bool IsCacheEntryFresh(const FNavCacheEntry* Entry, int32 Version, double Time, double Ttl)
		{
			if (!Entry)
			{
				return false;
			}
			if (Entry->NavVersion != Version)
			{
				return false;
			}
			if (Time - Entry->StoredAt > Ttl)
			{
				return false;
			}
			return true;
		}

		void BumpCacheHit(FNavMetrics* Metrics)
		{
			if (Metrics)
			{
				Metrics->CacheHits += 1;
				Metrics->PathCacheHits += 1;
			}
		}
	}

	// --- Fonctions pures --------------------------------------------------------

	double NavStepMultForSpeed(double SpeedScale)
	{
		const AnastasisSimClock::FStepPlan Plan = AnastasisSimClock::StepPlan(SpeedScale);
		return FMath::Max(1.0, Plan.StepDt / AnastasisSimClock::FixedDt);
	}

	double NavCacheTtlForSpeed(double SpeedScale)
	{
		const double StepMult = NavStepMultForSpeed(SpeedScale);
		if (StepMult <= 1.0 + 1e-9)
		{
			return NavCacheTtl;
		}
		return FMath::Min(NavCacheTtlMax, NavCacheTtl * StepMult);
	}

	double NavCacheSweepIntervalForSpeed(double SpeedScale)
	{
		const double StepMult = NavStepMultForSpeed(SpeedScale);
		if (StepMult <= 1.0 + 1e-9)
		{
			return NavCacheSweepInterval;
		}
		return NavCacheSweepInterval * StepMult;
	}

	int32 PathBudgetForSpeed(double SpeedScale)
	{
		const double StepMult = NavStepMultForSpeed(SpeedScale);
		if (StepMult <= 1.0 + 1e-9)
		{
			return 6;
		}
		// `Math.round(6 * stepMult ** 0.75)`. Le pow peut differer d'un ulp de
		// celui de V8; l'arrondi entier l'absorbe sauf a un demi-entier pile,
		// que la batterie de vecteurs balaie.
		const double Scaled = AnastasisJs::Round(6.0 * AnastasisJs::Pow(StepMult, 0.75));
		return static_cast<int32>(FMath::Min(
			static_cast<double>(NavPathBudgetMaxCalcs),
			FMath::Max(8.0, Scaled)));
	}

	FString CacheKeyFor(const FPoint& Start, const FPoint& Target, int32 NavVersion, bool bZone)
	{
		const FString Tx = JsFlooredToString(Target.X);
		const FString Ty = JsFlooredToString(Target.Y);
		if (bZone)
		{
			const FString Zx = JsFlooredToString(Start.X / static_cast<double>(NavZone));
			const FString Zy = JsFlooredToString(Start.Y / static_cast<double>(NavZone));
			return FString::Printf(TEXT("z%s,%s:%s,%s:%d"), *Zx, *Zy, *Tx, *Ty, NavVersion);
		}
		return FString::Printf(
			TEXT("%s,%s:%s,%s:%d"),
			*JsFlooredToString(Start.X), *JsFlooredToString(Start.Y), *Tx, *Ty, NavVersion);
	}

	FString NavigationTargetKey(const FPoint& Target)
	{
		if (!FMath::IsFinite(Target.X) || !FMath::IsFinite(Target.Y))
		{
			return FString();
		}
		return FString::Printf(TEXT("%s,%s"), *JsFlooredToString(Target.X), *JsFlooredToString(Target.Y));
	}

	FString NavigationTargetKey(const FPoint* Target)
	{
		return Target ? NavigationTargetKey(*Target) : FString();
	}

	int32 PriorityForGoal(const FString& Goal, double Hunger, double Thirst, double Energy)
	{
		const FString& G = Goal;
		if (G == TEXT("flee")
			|| G == TEXT("shelter")
			|| (G == TEXT("eat") && AnastasisJs::NumberOrZero(Hunger) > 78.0)
			|| (G == TEXT("drink") && AnastasisJs::NumberOrZero(Thirst) > 78.0)
			|| (G == TEXT("rest") && AnastasisJs::NumberOr(Energy, 100.0) < 18.0))
		{
			return Priority::Urgent;
		}
		if (G == TEXT("deliver")
			|| G == TEXT("sell")
			|| G == TEXT("craft")
			|| G == TEXT("build")
			|| G == TEXT("haulJob")
			|| G == TEXT("gatherWood")
			|| G == TEXT("gatherStone")
			|| G == TEXT("gatherFood")
			|| G == TEXT("helpFarm"))
		{
			return Priority::High;
		}
		if (G == TEXT("explore") || G == TEXT("observer") || G == TEXT("wander") || G == TEXT("socialize"))
		{
			return Priority::Low;
		}
		return Priority::Normal;
	}

	// --- Metriques et anneau de trace -----------------------------------------

	TArray<FNavTraceSample> FNavMetrics::TraceSnapshot() const
	{
		if (NavTrace.Num() < NavTraceCap || !(NavTraceNext > 0))
		{
			return NavTrace;
		}
		const int32 Next = NavTraceNext % NavTrace.Num();
		TArray<FNavTraceSample> Ordered;
		Ordered.Reserve(NavTrace.Num());
		for (int32 Index = Next; Index < NavTrace.Num(); ++Index)
		{
			Ordered.Add(NavTrace[Index]);
		}
		for (int32 Index = 0; Index < Next; ++Index)
		{
			Ordered.Add(NavTrace[Index]);
		}
		return Ordered;
	}

	bool RecordNavTransition(INavServiceHost& Host, const FNavService& Service, const FNavTransitionEvent& Event)
	{
		FNavMetrics* Metrics = Host.GetMetrics();
		if (!Metrics)
		{
			return false;
		}

		const FString Type = Event.Type ? FString(Event.Type) : FString(TEXT("NAV_TRANSITION"));
		const FString TargetKey = NavigationTargetKey(Event.Target);
		FString DedupeKey;
		if (Type == TEXT("ARRIVAL"))
		{
			DedupeKey = FString::Printf(
				TEXT("%s|%s|%s"),
				Event.Actor ? *Event.Actor->Id : TEXT("?"),
				TargetKey.IsEmpty() ? TEXT("?") : *TargetKey,
				*Type);
			if (Metrics->NavLastTransitionKey == DedupeKey)
			{
				return false;
			}
		}
		Metrics->NavLastTransitionKey = DedupeKey;

		FNavTraceSample Sample;
		Sample.Type = Type;
		Sample.Time = TimeOf(Host);
		Sample.ActorId = Event.Actor ? Event.Actor->Id : FString();
		Sample.Goal = Event.Actor ? Event.Actor->Goal : FString();
		Sample.TargetKey = TargetKey;
		Sample.NavVersion = Host.GetNavVersion();
		Sample.From = Event.From ? FString(Event.From) : FString();
		Sample.To = Event.To ? FString(Event.To) : FString();
		Sample.Reason = Event.Reason ? FString(Event.Reason) : FString();
		Sample.PathLength = Event.PathLength;
		Sample.QueueDepth = Service.Queue.Num();

		if (Metrics->NavTrace.Num() < NavTraceCap)
		{
			Metrics->NavTrace.Add(MoveTemp(Sample));
		}
		else
		{
			const int32 Next = Metrics->NavTraceNext;
			Metrics->NavTrace[Next] = MoveTemp(Sample);
			Metrics->NavTraceNext = (Next + 1) % NavTraceCap;
		}
		Metrics->NavTraceTotal += 1;
		return true;
	}

	// --- Le cache ---------------------------------------------------------------

	const FNavCacheEntry* FNavCache::Find(const FString& Key) const
	{
		const int32* Index = IndexByKey.Find(Key);
		return Index ? &Entries[*Index] : nullptr;
	}

	void FNavCache::Set(const FString& Key, const FNavCacheEntry& Entry)
	{
		if (const int32* Index = IndexByKey.Find(Key))
		{
			// `Map.set` sur une cle existante: la valeur change, la place reste.
			Entries[*Index] = Entry;
			return;
		}
		IndexByKey.Add(Key, Keys.Num());
		Keys.Add(Key);
		Entries.Add(Entry);
	}

	bool FNavCache::Remove(const FString& Key)
	{
		const int32* Found = IndexByKey.Find(Key);
		if (!Found)
		{
			return false;
		}
		const int32 Index = *Found;
		Keys.RemoveAt(Index);
		Entries.RemoveAt(Index);
		RebuildIndex();
		return true;
	}

	void FNavCache::Empty()
	{
		Keys.Reset();
		Entries.Reset();
		IndexByKey.Reset();
	}

	int32 FNavCache::RemoveIf(TFunctionRef<bool(const FNavCacheEntry&)> ShouldRemove)
	{
		int32 Removed = 0;
		int32 Write = 0;
		for (int32 Read = 0; Read < Keys.Num(); ++Read)
		{
			if (ShouldRemove(Entries[Read]))
			{
				Removed += 1;
				continue;
			}
			if (Write != Read)
			{
				Keys[Write] = MoveTemp(Keys[Read]);
				Entries[Write] = MoveTemp(Entries[Read]);
			}
			Write += 1;
		}
		if (Removed > 0)
		{
			Keys.SetNum(Write);
			Entries.SetNum(Write);
			RebuildIndex();
		}
		return Removed;
	}

	void FNavCache::RemoveFirst(int32 Count)
	{
		const int32 N = FMath::Clamp(Count, 0, Keys.Num());
		if (N == 0)
		{
			return;
		}
		Keys.RemoveAt(0, N);
		Entries.RemoveAt(0, N);
		RebuildIndex();
	}

	void FNavCache::RebuildIndex()
	{
		IndexByKey.Reset();
		for (int32 Index = 0; Index < Keys.Num(); ++Index)
		{
			IndexByKey.Add(Keys[Index], Index);
		}
	}

	// --- Le service --------------------------------------------------------------

	FNavService::FNavService()
		: MaxCalcs(PathBudgetForSpeed(1.0))
		, CacheTtl(NavCacheTtlForSpeed(1.0))
	{
	}

	void FNavService::BeginNavTick(INavServiceHost& Host, double BudgetMul)
	{
		const double Speed = SpeedOf(Host);
		CalcThisTick = 0;
		const int32 BaseMaxCalcs = PathBudgetForSpeed(Speed);
		// `Math.max(0.15, Math.min(1, Number(budgetMul) || 1))`.
		const double Mul = FMath::Max(0.15, FMath::Min(1.0, AnastasisJs::NumberOr(BudgetMul, 1.0)));
		MaxCalcs = FMath::Max(1, static_cast<int32>(AnastasisJs::Floor(static_cast<double>(BaseMaxCalcs) * Mul)));
		CacheTtl = NavCacheTtlForSpeed(Speed);
		const double Time = TimeOf(Host);
		// Purge amortie: TTL et version sont aussi verifies a la lecture.
		const double SweepEvery = NavCacheSweepIntervalForSpeed(Speed);
		if (Cache.Num() > 0 && Time - LastSweepAt >= SweepEvery)
		{
			SweepNavCache(Host);
		}
		if (FNavMetrics* Metrics = Host.GetMetrics())
		{
			Metrics->QueueDepth = Queue.Num();
			Metrics->CacheSize = Cache.Num();
		}
	}

	int32 FNavService::SweepNavCache(INavServiceHost& Host)
	{
		const int32 Version = Host.GetNavVersion();
		const double Time = TimeOf(Host);
		const double Ttl = CacheTtl;
		LastSweepAt = Time;
		if (Cache.Num() == 0)
		{
			return 0;
		}
		return Cache.RemoveIf([Version, Time, Ttl](const FNavCacheEntry& Entry)
		{
			return !IsCacheEntryFresh(&Entry, Version, Time, Ttl);
		});
	}

	bool FNavService::LookupCachedPath(INavServiceHost& Host, const FPoint& Start, const FPoint& Target, TArray<FPoint>& OutPath)
	{
		FNavMetrics* Metrics = Host.GetMetrics();
		const int32 Version = Host.GetNavVersion();
		const double Time = TimeOf(Host);
		const double Ttl = CacheTtl;

		const FString ExactKey = CacheKeyFor(Start, Target, Version, false);
		const FNavCacheEntry* Exact = Cache.Find(ExactKey);
		if (Exact && !IsCacheEntryFresh(Exact, Version, Time, Ttl))
		{
			Cache.Remove(ExactKey);
		}
		else if (Exact)
		{
			// `exact?.path` est vrai pour un tableau vide: on le sert.
			BumpCacheHit(Metrics);
			OutPath = Exact->Path;
			return true;
		}

		const FString ZoneKey = CacheKeyFor(Start, Target, Version, true);
		const FNavCacheEntry* Zone = Cache.Find(ZoneKey);
		if (Zone && !IsCacheEntryFresh(Zone, Version, Time, Ttl))
		{
			Cache.Remove(ZoneKey);
		}
		else if (Zone && Zone->Path.Num() > 0)
		{
			const FPoint& First = Zone->Path[0];
			const double Near = FMath::Max(
				FMath::Abs(AnastasisJs::Floor(Start.X) - AnastasisJs::Floor(First.X)),
				FMath::Abs(AnastasisJs::Floor(Start.Y) - AnastasisJs::Floor(First.Y)));
			if (Near <= static_cast<double>(NavZone + 1))
			{
				BumpCacheHit(Metrics);
				OutPath = Zone->Path;
				return true;
			}
		}
		if (Metrics)
		{
			Metrics->CacheMisses += 1;
		}
		return false;
	}

	void FNavService::StoreCachedPath(INavServiceHost& Host, const FPoint& Start, const FPoint& Target, const TArray<FPoint>& Path)
	{
		const int32 Version = Host.GetNavVersion();
		FNavCacheEntry Entry;
		Entry.Path = Path;
		Entry.NavVersion = Version;
		Entry.StoredAt = TimeOf(Host);
		Cache.Set(CacheKeyFor(Start, Target, Version, false), Entry);
		Cache.Set(CacheKeyFor(Start, Target, Version, true), Entry);
		// Garde-fou memoire: les 80 premieres cles dans l'ordre d'insertion.
		if (Cache.Num() > NavCacheMaxEntries)
		{
			Cache.RemoveFirst(NavCacheEvictCount);
		}
	}

	bool FNavService::RequestPath(
		INavServiceHost& Host,
		FNavAgent& Actor,
		const FPoint& Target,
		const FRequestOptions& Options,
		TArray<FPoint>* OutPath)
	{
		TArray<FPoint> Cached;
		if (LookupCachedPath(Host, { Actor.X, Actor.Y }, Target, Cached))
		{
			// Sans `source`: le JS journalise un succes de cache comme "astar".
			ApplyPathToActor(Host, *this, Actor, Target, Cached, false);
			if (OutPath)
			{
				*OutPath = MoveTemp(Cached);
			}
			return true;
		}

		const int32 RequestPriority = Options.Priority.IsSet()
			? Options.Priority.GetValue()
			: PriorityForGoal(Actor.Goal, Actor.Hunger, Actor.Thirst, Actor.Energy);
		const FString GoalKey = NavigationTargetKey(Target);
		const bool bAllowBlockedTarget = Options.bAllowBlockedTarget.Get(false);

		int32 JobIndex = INDEX_NONE;
		bool bNewlyQueued = false;
		// `pendingByActor` peut pointer un rang perime: le JS ne verifie que la
		// presence d'un job a ce rang, pas son proprietaire. Copie fidele.
		const int32* Existing = PendingByActor.Find(Actor.Id);
		if (Existing && Queue.IsValidIndex(*Existing))
		{
			JobIndex = *Existing;
			FNavJob& Job = Queue[JobIndex];
			Job.Start = { Actor.X, Actor.Y };
			Job.Destination = Target;
			Job.GoalKey = GoalKey;
			Job.Priority = FMath::Min(Job.Priority, RequestPriority);
			Job.bAllowBlockedTarget = bAllowBlockedTarget;
		}
		else
		{
			FNavJob Job;
			Job.ActorId = Actor.Id;
			Job.Start = { Actor.X, Actor.Y };
			Job.Destination = Target;
			Job.GoalKey = GoalKey;
			Job.Priority = RequestPriority;
			Job.bAllowBlockedTarget = bAllowBlockedTarget;
			Job.RequestedAt = TimeOf(Host);
			JobIndex = Queue.Add(MoveTemp(Job));
			PendingByActor.Add(Actor.Id, JobIndex);
			bNewlyQueued = true;
			if (FNavMetrics* Metrics = Host.GetMetrics())
			{
				Metrics->PathQueueEnqueued += 1;
			}
		}

		// Resoudre CE job sous budget, sans trier ni vider toute la file.
		bool bPending = BudgetExhausted();
		if (!bPending)
		{
			const bool bHandled = ResolveNavJob(Host, Queue[JobIndex]);
			if (bHandled)
			{
				RemoveJobFromQueue(JobIndex);
				if (Actor.Path.Num() > 0 && NavigationTargetKey(Actor.bHasPathGoal ? &Actor.PathGoal : nullptr) == GoalKey)
				{
					if (OutPath)
					{
						*OutPath = Actor.Path;
					}
					return true;
				}
			}
			else
			{
				bPending = true;
			}
		}
		if (bNewlyQueued && bPending)
		{
			FNavTransitionEvent Event;
			Event.Type = TEXT("PENDING_QUEUE");
			Event.Actor = &Actor;
			Event.Target = &Target;
			Event.From = TEXT("PATH_REQUEST");
			Event.To = TEXT("AWAITING_PATH");
			Event.Reason = TEXT("budget");
			RecordNavTransition(Host, *this, Event);
		}
		return false;
	}

	void ApplyPathToActor(
		INavServiceHost& Host,
		const FNavService& Service,
		FNavAgent& Actor,
		const FPoint& Target,
		const TArray<FPoint>& Path,
		bool bFromCache)
	{
		const int32 WorldNav = Host.GetNavVersion();
		// `path && path.length ? path : null` — un chemin vide devient un echec.
		Actor.Path = Path;
		const bool bHasPath = Actor.Path.Num() > 0;
		Actor.PathStep = 0;
		Actor.bHasPathGoal = true;
		Actor.PathGoal = Target;
		Actor.bPathFailed = !bHasPath;
		Actor.NavTargetKey = NavigationTargetKey(Target);
		Actor.NavRequestedAt = TimeOf(Host);
		Actor.NavVersion = WorldNav;
		Actor.ApplyCount += 1;
		FNavMetrics* Metrics = Host.GetMetrics();
		if (bHasPath)
		{
			Actor.PathFailStreak = 0;
			Actor.PathCooldown = 0.0;
			if (Metrics)
			{
				Metrics->PathHits += 1;
			}
		}
		else
		{
			Actor.PathFailStreak += 1;
			Actor.PathCooldown = FMath::Min(3.2, 1.5 + static_cast<double>(Actor.PathFailStreak) * 0.45);
			if (Metrics)
			{
				Metrics->PathFails += 1;
			}
		}

		FNavTransitionEvent Event;
		Event.Type = bHasPath ? TEXT("PATH_RESOLVED") : TEXT("NO_PATH");
		Event.Actor = &Actor;
		Event.Target = &Target;
		Event.From = bFromCache ? TEXT("CACHE_HIT") : TEXT("A_STAR");
		Event.To = bHasPath ? TEXT("PATH_ACTIVE") : TEXT("PATH_FAILED");
		Event.Reason = bFromCache ? TEXT("cache") : TEXT("astar");
		Event.PathLength = Actor.Path.Num();
		RecordNavTransition(Host, Service, Event);
	}

	bool FNavService::ResolveNavJob(INavServiceHost& Host, FNavJob& Job)
	{
		FNavAgent* Live = Host.FindLiveAgent(Job.ActorId);
		if (!Live)
		{
			return true; // abandonner le job
		}
		const FString CurrentKey = NavigationTargetKey(Live->bHasTarget ? &Live->Target : nullptr);
		if (!Job.GoalKey.IsEmpty() && Live->bHasTarget && !CurrentKey.IsEmpty() && CurrentKey != Job.GoalKey)
		{
			return true; // cible changee: jeter
		}

		const FPoint Start{ Live->X, Live->Y };
		// Copie: `Job` vit dans `Queue`, et la destination doit survivre a l'appel.
		const FPoint Dest = Job.Destination;
		TArray<FPoint> Path;
		bool bFromCache = LookupCachedPath(Host, Start, Dest, Path);
		bool bFound = bFromCache;
		if (!bFromCache)
		{
			if (BudgetExhausted())
			{
				return false;
			}
			FNavMetrics* Metrics = Host.GetMetrics();
			if (Metrics)
			{
				Metrics->PathRequests += 1;
			}
			AnastasisPath::FOptions PathOptions;
			PathOptions.bAllowBlockedTarget = Job.bAllowBlockedTarget;
			bFound = AnastasisPath::FindPath(Host.GetNavSource(), Start, Dest, PathOptions, Path);
			if (!bFound)
			{
				Path.Reset();
			}
			CalcThisTick += 1;
			if (bFound)
			{
				StoreCachedPath(Host, Start, Dest, Path);
			}
			if (Metrics)
			{
				Metrics->QueueSolved += 1;
			}
		}
		ApplyPathToActor(Host, *this, *Live, Dest, Path, bFromCache);
		return true;
	}

	void FNavService::RemoveJobFromQueue(int32 JobIndex)
	{
		if (!Queue.IsValidIndex(JobIndex))
		{
			return;
		}
		Queue.RemoveAt(JobIndex);
		PendingByActor.Reset();
		for (int32 Index = 0; Index < Queue.Num(); ++Index)
		{
			PendingByActor.Add(Queue[Index].ActorId, Index);
		}
	}

	int32 FNavService::ProcessNavQueue(INavServiceHost& Host, const FProcessOptions& Options)
	{
		if (Queue.Num() == 0)
		{
			PendingByActor.Reset();
			return 0;
		}

		// Tri stable: priorite, puis date de demande.
		if (Queue.Num() > 1)
		{
			Algo::StableSort(Queue, [](const FNavJob& A, const FNavJob& B)
			{
				if (A.Priority != B.Priority)
				{
					return A.Priority < B.Priority;
				}
				return AnastasisJs::NumberOrZero(A.RequestedAt) < AnastasisJs::NumberOrZero(B.RequestedAt);
			});
		}
		if (!Options.FavorActorId.IsEmpty())
		{
			const int32 FavIndex = Queue.IndexOfByPredicate([&Options](const FNavJob& Job)
			{
				return Job.ActorId == Options.FavorActorId;
			});
			if (FavIndex > 0)
			{
				FNavJob Fav = MoveTemp(Queue[FavIndex]);
				Queue.RemoveAt(FavIndex);
				Queue.Insert(MoveTemp(Fav), 0);
			}
		}

		int32 Solved = 0;
		TArray<FNavJob> Remain;
		PendingByActor.Reset();
		const bool bLimited = Options.MaxJobs >= 0;
		int32 Handled = 0;

		// `Queue` reste intacte pendant la boucle, comme le tableau JS: l'anneau
		// de trace y lit la profondeur de file d'avant le drain.
		for (int32 Index = 0; Index < Queue.Num(); ++Index)
		{
			FNavJob& Job = Queue[Index];
			if (bLimited && Handled >= Options.MaxJobs)
			{
				Remain.Add(Job);
				continue;
			}
			const int32 BeforeCalcs = CalcThisTick;
			const bool bDone = ResolveNavJob(Host, Job);
			if (!bDone)
			{
				Remain.Add(Job);
				continue;
			}
			Handled += 1;
			if (CalcThisTick > BeforeCalcs)
			{
				Solved += 1;
			}
		}

		Queue = MoveTemp(Remain);
		for (int32 Index = 0; Index < Queue.Num(); ++Index)
		{
			PendingByActor.Add(Queue[Index].ActorId, Index);
		}
		if (FNavMetrics* Metrics = Host.GetMetrics())
		{
			Metrics->QueueDepth = Queue.Num();
			Metrics->CalcsThisTick = CalcThisTick;
		}
		return Solved;
	}

	void FNavService::ClearNavCache(INavServiceHost& Host)
	{
		Cache.Empty();
		LastSweepAt = TimeOf(Host);
	}

	void FNavService::RestoreCache(const TArray<TPair<FString, FNavCacheEntry>>& SavedEntries, int32 NavVersion)
	{
		Cache.Empty();
		for (const TPair<FString, FNavCacheEntry>& Saved : SavedEntries)
		{
			if (Saved.Value.NavVersion == NavVersion)
			{
				Cache.Set(Saved.Key, Saved.Value);
			}
		}
	}
}
