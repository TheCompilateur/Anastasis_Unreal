#include "World/AnastasisNavCache.h"

#include "Core/AnastasisSimClock.h"

namespace AnastasisNavCache
{
	namespace
	{
		int64 JsFloor(double V)
		{
			return static_cast<int64>(FMath::FloorToDouble(V));
		}

		/**
		 * `Math.round` de JavaScript: les demis vont vers +Infini.
		 *
		 * `FMath::RoundToDouble` arrondit en s'eloignant de zero. Sur les
		 * entrees positives d'ici les deux coincident — raison de plus pour
		 * ecrire la regle plutot que de la supposer, car le jour ou une entree
		 * negative arrivera, la divergence sera silencieuse.
		 */
		double JsRound(double V)
		{
			return FMath::FloorToDouble(V + 0.5);
		}
	}

	double StepMultForSpeed(double SpeedScale)
	{
		const AnastasisSimClock::FStepPlan Plan = AnastasisSimClock::StepPlan(SpeedScale);
		return FMath::Max(1.0, Plan.StepDt / AnastasisSimClock::FixedDt);
	}

	double CacheTtlForSpeed(double SpeedScale)
	{
		const double StepMult = StepMultForSpeed(SpeedScale);
		if (StepMult <= 1.0 + 1e-9)
		{
			return CacheTtl;
		}
		return FMath::Min(CacheTtlMax, CacheTtl * StepMult);
	}

	double SweepIntervalForSpeed(double SpeedScale)
	{
		const double StepMult = StepMultForSpeed(SpeedScale);
		if (StepMult <= 1.0 + 1e-9)
		{
			return SweepInterval;
		}
		return SweepInterval * StepMult;
	}

	int32 PathBudgetForSpeed(double SpeedScale)
	{
		const double StepMult = StepMultForSpeed(SpeedScale);
		if (StepMult <= 1.0 + 1e-9)
		{
			return 6;
		}
		// ^0.75 et non une racine: la reference a mesure qu'un plafond a 20
		// affamait la file a 5x et 10x.
		const double Scaled = JsRound(6.0 * FMath::Pow(StepMult, 0.75));
		return FMath::Min(
			PathBudgetMaxCalcs,
			FMath::Max(8, static_cast<int32>(Scaled)));
	}

	FString CacheKeyFor(
		const AnastasisPath::FPoint& Start,
		const AnastasisPath::FPoint& Target,
		int32 NavVersion,
		bool bZone)
	{
		const int64 TX = JsFloor(Target.X);
		const int64 TY = JsFloor(Target.Y);
		if (bZone)
		{
			// `floor(x / NAV_ZONE)` et non une troncature: sur une coordonnee
			// negative, la troncature remonterait d'une zone entiere.
			const int64 ZX = JsFloor(Start.X / static_cast<double>(NavZone));
			const int64 ZY = JsFloor(Start.Y / static_cast<double>(NavZone));
			return FString::Printf(TEXT("z%lld,%lld:%lld,%lld:%d"), ZX, ZY, TX, TY, NavVersion);
		}
		const int64 SX = JsFloor(Start.X);
		const int64 SY = JsFloor(Start.Y);
		return FString::Printf(TEXT("%lld,%lld:%lld,%lld:%d"), SX, SY, TX, TY, NavVersion);
	}

	bool IsEntryFresh(const FEntry& Entry, int32 NavVersion, double Time, double Ttl)
	{
		if (Entry.NavVersion != NavVersion)
		{
			return false;
		}
		// Strictement superieur: une entree pile a son TTL est encore bonne.
		return !(Time - Entry.StoredAt > Ttl);
	}

	void FCache::Set(const FString& Key, const TSharedRef<FEntry>& Entry)
	{
		// Reecrire une cle existante ne la deplace PAS en fin d'ordre — c'est
		// la semantique d'une `Map` JS, et l'eviction s'en sert.
		if (!Entries.Contains(Key))
		{
			Order.Add(Key);
		}
		Entries.Add(Key, Entry);
	}

	void FCache::RemoveKey(const FString& Key)
	{
		if (Entries.Remove(Key) > 0)
		{
			// Retrait par decalage: l'ordre relatif des autres survit.
			Order.RemoveSingle(Key);
		}
	}

	void FCache::EvictIfNeeded()
	{
		if (Order.Num() <= EvictAbove)
		{
			return;
		}
		const int32 Count = FMath::Min(EvictBatch, Order.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Entries.Remove(Order[Index]);
		}
		Order.RemoveAt(0, Count, EAllowShrinking::No);
	}

	void FCache::Store(
		const AnastasisPath::FPoint& Start,
		const AnastasisPath::FPoint& Target,
		int32 NavVersion,
		double Time,
		const TArray<AnastasisPath::FPoint>& Path)
	{
		// La reference range la MEME entree sous les deux cles. Le partage est
		// reproduit plutot que copie: si un jour quelque chose mute une entree,
		// les deux cles bougeront ensemble des deux cotes.
		const TSharedRef<FEntry> Entry = MakeShared<FEntry>();
		Entry->Path = Path;
		Entry->NavVersion = NavVersion;
		Entry->StoredAt = Time;

		Set(CacheKeyFor(Start, Target, NavVersion, false), Entry);
		Set(CacheKeyFor(Start, Target, NavVersion, true), Entry);
		EvictIfNeeded();
	}

	bool FCache::Lookup(
		const AnastasisPath::FPoint& Start,
		const AnastasisPath::FPoint& Target,
		int32 NavVersion,
		double Time,
		double Ttl,
		TArray<AnastasisPath::FPoint>& OutPath)
	{
		OutPath.Reset();

		const FString ExactKey = CacheKeyFor(Start, Target, NavVersion, false);
		if (const TSharedRef<FEntry>* Found = Entries.Find(ExactKey))
		{
			if (!IsEntryFresh(**Found, NavVersion, Time, Ttl))
			{
				// La lecture PURGE ce qu'elle trouve de perime. Le taire
				// changerait la taille du cache au coup d'apres, donc
				// l'eviction, donc les chemins servis.
				RemoveKey(ExactKey);
			}
			else
			{
				// Un chemin VIDE compte comme un succes exact. Ce n'est pas un
				// oubli: le JS teste `exact?.path`, et un tableau vide est
				// truthy. `findPath` rend [] quand depart et arrivee sont la
				// meme case — « deja arrive » est une reponse, pas un echec, et
				// la servir depuis le cache evite de relancer un A* pour rien.
				OutPath = (*Found)->Path;
				return true;
			}
		}

		const FString ZoneKey = CacheKeyFor(Start, Target, NavVersion, true);
		if (const TSharedRef<FEntry>* Found = Entries.Find(ZoneKey))
		{
			if (!IsEntryFresh(**Found, NavVersion, Time, Ttl))
			{
				RemoveKey(ZoneKey);
			}
			else if ((*Found)->Path.Num() > 0)
			{
				// Asymetrie assumee avec la branche exacte: le JS teste ici
				// `zone?.path?.length`, donc un chemin VIDE ne sert pas par
				// zone — et c'est coherent, car sans premier noeud la regle
				// ci-dessous n'aurait rien a mesurer.
				//
				// Un chemin de zone ne sert que si le depart est assez pres de
				// son premier noeud: sinon l'habitant commencerait par revenir
				// sur ses pas.
				const AnastasisPath::FPoint& First = (*Found)->Path[0];
				const int64 Near = FMath::Max(
					FMath::Abs(JsFloor(Start.X) - JsFloor(First.X)),
					FMath::Abs(JsFloor(Start.Y) - JsFloor(First.Y)));
				if (Near <= NavZone + 1)
				{
					OutPath = (*Found)->Path;
					return true;
				}
			}
		}

		return false;
	}

	int32 FCache::Sweep(int32 NavVersion, double Time, double Ttl)
	{
		if (Order.Num() == 0)
		{
			return 0;
		}
		// Les cles a retirer sont collectees avant d'etre retirees: le JS itere
		// une Map et supprime pendant l'iteration, ce qui est sur la-bas; ici on
		// evite simplement de modifier ce qu'on parcourt.
		TArray<FString> APurger;
		for (const FString& Key : Order)
		{
			const TSharedRef<FEntry>* Found = Entries.Find(Key);
			if (Found == nullptr || !IsEntryFresh(**Found, NavVersion, Time, Ttl))
			{
				APurger.Add(Key);
			}
		}
		for (const FString& Key : APurger)
		{
			RemoveKey(Key);
		}
		return APurger.Num();
	}

	void FCache::Clear()
	{
		Order.Reset();
		Entries.Reset();
	}

	const FEntry* FCache::Find(const FString& Key) const
	{
		const TSharedRef<FEntry>* Found = Entries.Find(Key);
		return Found ? &Found->Get() : nullptr;
	}
}
