#include "Life/AnastasisLifestyle.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisRng.h"
#include "Life/AnastasisVillageRhythm.h"

namespace AnastasisLifestyle
{
	namespace
	{
		/** `LIFESTYLES`, dans l'ordre de `Object.keys`. */
		const FLifestyleInfo Infos[NumLifestyles] = {
			{ TEXT("leve-tot"), TEXT("commence avant les autres"), TEXT("#f0d28a"), TEXT("sun") },
			{ TEXT("noctambule"), TEXT("vit mieux quand Valmire s'assombrit"), TEXT("#8fb0dd"), TEXT("moon") },
			{ TEXT("travailleur acharne"), TEXT("revient toujours vers le travail"), TEXT("#e0a257"), TEXT("hammer") },
			{ TEXT("flaneur"), TEXT("prend les chemins de traverse"), TEXT("#86b6a8"), TEXT("path") },
			{ TEXT("parent present"), TEXT("revient vers les siens"), TEXT("#c6a76b"), TEXT("home") },
			{ TEXT("habitue de taverne"), TEXT("cherche les voix et les rumeurs"), TEXT("#d8845b"), TEXT("cup") },
		};

		const TCHAR* const Ids[NumLifestyles] = {
			TEXT("earlyBird"), TEXT("nightOwl"), TEXT("workhorse"),
			TEXT("wanderer"), TEXT("familyFirst"), TEXT("tavernRegular"),
		};

		constexpr int32 Idx(ELifestyle L) { return static_cast<int32>(L); }

		/** `rng ? rng() : fallbackRng()`. */
		double Draw(FAnastasisRng* Rng)
		{
			return Rng ? Rng->Next() : GetAnastasisFallbackRng().Next();
		}

		/** `SOCIAL_GOALS`. */
		bool IsSocialGoal(const FString& Goal)
		{
			return Goal == TEXT("socialize") || Goal == TEXT("visitFamily") || Goal == TEXT("play") || Goal == TEXT("relax");
		}

		bool IsWork(const FString& Goal)
		{
			return AnastasisRhythm::IsWorkGoal(Goal);
		}

		bool Is(const FLifestyle& L, ELifestyle Which)
		{
			return L.Id == Ids[Idx(Which)];
		}

		bool JobIn(const FString& JobId, std::initializer_list<const TCHAR*> Jobs)
		{
			for (const TCHAR* Job : Jobs)
			{
				if (JobId == Job)
				{
					return true;
				}
			}
			return false;
		}
	}

	const TCHAR* LifestyleId(ELifestyle Lifestyle)
	{
		const int32 I = Idx(Lifestyle);
		return I >= 0 && I < NumLifestyles ? Ids[I] : TEXT("");
	}

	bool LifestyleFromId(const FString& Id, ELifestyle& Out)
	{
		for (int32 I = 0; I < NumLifestyles; ++I)
		{
			// Comparaison sensible a la casse : `LIFESTYLES[id]` l'est.
			if (Id.Equals(Ids[I], ESearchCase::CaseSensitive))
			{
				Out = static_cast<ELifestyle>(I);
				return true;
			}
		}
		return false;
	}

	const FLifestyleInfo& LifestyleForId(const FString& Id)
	{
		ELifestyle L;
		return LifestyleFromId(Id, L) ? Infos[Idx(L)] : Infos[Idx(ELifestyle::Wanderer)];
	}

	EDayPhase DayPhase(double Frac)
	{
		switch (AnastasisRhythm::VillagePhase(Frac))
		{
		case AnastasisRhythm::EPhase::Night: return EDayPhase::Night;
		case AnastasisRhythm::EPhase::Dawn: return EDayPhase::Morning;
		case AnastasisRhythm::EPhase::Morning: return EDayPhase::Morning;
		case AnastasisRhythm::EPhase::Midday: return EDayPhase::Midday;
		case AnastasisRhythm::EPhase::Afternoon: return EDayPhase::Day;
		case AnastasisRhythm::EPhase::Evening: return EDayPhase::Evening;
		default: return EDayPhase::Day;
		}
	}

	const TCHAR* DayPhaseId(EDayPhase Phase)
	{
		switch (Phase)
		{
		case EDayPhase::Night: return TEXT("night");
		case EDayPhase::Morning: return TEXT("morning");
		case EDayPhase::Midday: return TEXT("midday");
		case EDayPhase::Day: return TEXT("day");
		case EDayPhase::Evening: return TEXT("evening");
		default: return TEXT("");
		}
	}

	FLifestyle AssignLifestyle(FAnastasisRng* Rng, const FLifestyleSubject& Npc, const FString& Preferred)
	{
		double W[NumLifestyles] = { 1.0, 0.8, 1.0, 1.0, 0.9, 0.7 };
		double& EarlyBird = W[Idx(ELifestyle::EarlyBird)];
		double& NightOwl = W[Idx(ELifestyle::NightOwl)];
		double& Workhorse = W[Idx(ELifestyle::Workhorse)];
		double& Wanderer = W[Idx(ELifestyle::Wanderer)];
		double& FamilyFirst = W[Idx(ELifestyle::FamilyFirst)];
		double& TavernRegular = W[Idx(ELifestyle::TavernRegular)];

		ELifestyle PreferredLifestyle;
		if (!Preferred.IsEmpty() && LifestyleFromId(Preferred, PreferredLifestyle))
		{
			W[Idx(PreferredLifestyle)] += 6.0;
		}
		// L'ordre des operations est celui de la reference : un `*=` puis un `+=`
		// ne donnent pas le meme double qu'un `+=` puis un `*=`.
		if (Npc.LifeStage == TEXT("child"))
		{
			Wanderer += 2.5;
			FamilyFirst += 1.4;
			Workhorse *= 0.25;
		}
		if (Npc.LifeStage == TEXT("teen"))
		{
			Wanderer += 0.6;
			Workhorse += Npc.bApprenticing ? 1.8 : 0.8;
			TavernRegular += 0.3;
			FamilyFirst += 0.5;
		}
		if (Npc.LifeStage == TEXT("elder"))
		{
			FamilyFirst += 2.2;
			Workhorse *= 0.35;
			Wanderer += 0.4;
			TavernRegular += 0.5;
		}
		if (Npc.JobId == TEXT("guard")) NightOwl += 2.2;
		if (JobIn(Npc.JobId, { TEXT("builder"), TEXT("woodcutter"), TEXT("quarryman"), TEXT("artisan"), TEXT("blacksmith"), TEXT("tanner"), TEXT("weaver") })) Workhorse += 1.6;
		if (JobIn(Npc.JobId, { TEXT("farmer"), TEXT("herder"), TEXT("fisherman"), TEXT("baker"), TEXT("cheesemaker"), TEXT("butcher") })) EarlyBird += 2.4;
		if (JobIn(Npc.JobId, { TEXT("merchant"), TEXT("innkeeper") })) TavernRegular += 1.7;
		if (Npc.JobId == TEXT("priest")) FamilyFirst += 1.1;
		if (AnastasisJs::NumberOrZero(Npc.TraitExplore) > 1.08) Wanderer += 1.5;
		if (AnastasisJs::NumberOrZero(Npc.TraitBuild) > 1.08) Workhorse += 0.8;
		if (AnastasisJs::NumberOrZero(Npc.TraitTrade) > 1.08) TavernRegular += 0.8;
		if (!Npc.FamilyId.IsEmpty() || Npc.ChildCount > 0) FamilyFirst += 1.2;

		double Total = 0.0;
		for (int32 I = 0; I < NumLifestyles; ++I)
		{
			Total += FMath::Max(0.0, AnastasisJs::NumberOrZero(W[I]));
		}
		double Roll = Draw(Rng) * Total;
		ELifestyle Chosen = ELifestyle::Wanderer;
		for (int32 I = 0; I < NumLifestyles; ++I)
		{
			Roll -= FMath::Max(0.0, AnastasisJs::NumberOrZero(W[I]));
			if (Roll <= 0.0)
			{
				Chosen = static_cast<ELifestyle>(I);
				break;
			}
		}

		FLifestyle Out;
		Out.Id = Ids[Idx(Chosen)];
		Out.SinceDay = 1.0;
		Out.RhythmScore = 0.0;
		Out.LastNotedDay = 0.0;
		return Out;
	}

	FLifestyle& EnsureLifestyle(TOptional<FLifestyle>& Lifestyle, const FLifestyleSubject& Npc, FAnastasisRng* Rng)
	{
		ELifestyle Known;
		if (!Lifestyle.IsSet() || !LifestyleFromId(Lifestyle->Id, Known))
		{
			Lifestyle = AssignLifestyle(Rng, Npc);
		}
		return Lifestyle.GetValue();
	}

	double LifestyleBias(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Npc,
		FAnastasisRng* SimRng,
		double DayFrac,
		const FString& Goal)
	{
		const FLifestyle& L = EnsureLifestyle(Lifestyle, Npc, SimRng);
		const EDayPhase Phase = DayPhase(DayFrac);
		const bool bHasHome = !Npc.HomeId.IsEmpty();
		const bool bHasFamily = !Npc.FamilyId.IsEmpty() && (!Npc.PartnerId.IsEmpty() || Npc.ChildCount > 0);
		const double Energy = AnastasisJs::NumberOr(Npc.Energy, 100.0);

		if (Is(L, ELifestyle::EarlyBird))
		{
			if (Phase == EDayPhase::Morning && IsWork(Goal)) return 18.0;
			if (Phase == EDayPhase::Evening && (Goal == TEXT("rest") || Goal == TEXT("visitFamily"))) return 8.0;
			if (Phase == EDayPhase::Night) return Goal == TEXT("rest") ? 18.0 : IsSocialGoal(Goal) ? -12.0 : -8.0;
		}
		if (Is(L, ELifestyle::NightOwl))
		{
			if (Phase == EDayPhase::Night && (Goal == TEXT("socialize") || Goal == TEXT("maintain") || Goal == TEXT("explore"))) return 18.0;
			if (Phase == EDayPhase::Morning) return Goal == TEXT("rest") ? 14.0 : IsWork(Goal) ? -10.0 : 0.0;
			if (Phase == EDayPhase::Evening && Goal == TEXT("socialize")) return 9.0;
		}
		if (Is(L, ELifestyle::Workhorse))
		{
			if (IsWork(Goal)) return 12.0 + FMath::Min(8.0, AnastasisJs::NumberOrZero(Npc.Skill) * 2.0);
			if (Goal == TEXT("rest") && Energy > 34.0) return -10.0;
			if (Goal == TEXT("socialize") || Goal == TEXT("relax")) return -5.0;
		}
		if (Is(L, ELifestyle::Wanderer))
		{
			if (Goal == TEXT("explore")) return 20.0;
			if (Goal == TEXT("play") || Goal == TEXT("relax")) return 12.0;
			if (Goal == TEXT("socialize") && Phase != EDayPhase::Morning) return 5.0;
			if (IsWork(Goal) && Phase == EDayPhase::Midday) return -5.0;
		}
		if (Is(L, ELifestyle::FamilyFirst))
		{
			if (Goal == TEXT("visitFamily")) return bHasFamily ? 22.0 : bHasHome ? 10.0 : 0.0;
			if (Goal == TEXT("socialize")) return bHasFamily ? (Phase == EDayPhase::Evening ? 16.0 : 8.0) : 5.0;
			if (Goal == TEXT("rest") && bHasHome && (Phase == EDayPhase::Evening || Phase == EDayPhase::Night)) return 14.0;
			if (Goal == TEXT("relax") && bHasHome) return 8.0;
			if (Goal == TEXT("explore") && Phase == EDayPhase::Evening) return -12.0;
		}
		if (Is(L, ELifestyle::TavernRegular))
		{
			if (Goal == TEXT("socialize")) return Phase == EDayPhase::Evening || Phase == EDayPhase::Night ? 24.0 : 14.0;
			if (Goal == TEXT("visitFamily")) return Phase == EDayPhase::Evening ? 12.0 : 6.0;
			if (Goal == TEXT("relax")) return Phase == EDayPhase::Midday || Phase == EDayPhase::Evening ? 10.0 : 4.0;
			if (Goal == TEXT("sell") || Goal == TEXT("deliver")) return 5.0;
			if (Goal == TEXT("rest") && Phase == EDayPhase::Evening && Energy > 24.0) return -8.0;
		}
		return 0.0;
	}

	double LifestyleTravelFactor(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Npc,
		FAnastasisRng* SimRng,
		double DayFrac)
	{
		const FLifestyle& L = EnsureLifestyle(Lifestyle, Npc, SimRng);
		const EDayPhase Phase = DayPhase(DayFrac);
		const FString& Goal = Npc.Goal;
		if (Is(L, ELifestyle::EarlyBird)) return Phase == EDayPhase::Morning && Npc.bHasTarget ? 1.12 : Phase == EDayPhase::Night ? 0.86 : 1.0;
		if (Is(L, ELifestyle::NightOwl)) return Phase == EDayPhase::Night ? 1.14 : Phase == EDayPhase::Morning ? 0.86 : 1.0;
		if (Is(L, ELifestyle::Workhorse)) return IsWork(Goal) ? 1.1 : 0.98;
		if (Is(L, ELifestyle::Wanderer)) return Goal == TEXT("explore") ? 0.92 : 0.97;
		if (Is(L, ELifestyle::FamilyFirst)) return Goal == TEXT("visitFamily") || Goal == TEXT("rest") || Goal == TEXT("relax") ? 1.08 : 1.0;
		if (Is(L, ELifestyle::TavernRegular)) return Goal == TEXT("socialize") && (Phase == EDayPhase::Evening || Phase == EDayPhase::Night) ? 1.12 : 1.0;
		return 1.0;
	}

	double LifestyleIndoorDuration(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Npc,
		const FString& Goal,
		double Base)
	{
		// `ensureLifestyle(npc)` sans flux : le secours.
		const FLifestyle& L = EnsureLifestyle(Lifestyle, Npc, nullptr);
		if (Is(L, ELifestyle::Workhorse) && IsWork(Goal)) return Base * 0.82;
		if (Is(L, ELifestyle::TavernRegular) && (Goal == TEXT("socialize") || Goal == TEXT("relax"))) return Base * 1.55;
		if (Is(L, ELifestyle::FamilyFirst) && (Goal == TEXT("visitFamily") || Goal == TEXT("relax"))) return Base * 1.45;
		if (Is(L, ELifestyle::NightOwl) && Goal == TEXT("rest")) return Base * 1.18;
		if (Is(L, ELifestyle::Wanderer) && Goal == TEXT("relax")) return Base * 1.25;
		if (Is(L, ELifestyle::Wanderer) && Goal == TEXT("study")) return Base * 0.8;
		return Base;
	}

	FString LifestyleTargetBuilding(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Npc,
		FAnastasisRng* SimRng,
		const ILifestyleWorld& World,
		const FString& Goal)
	{
		const FLifestyle& L = EnsureLifestyle(Lifestyle, Npc, SimRng);
		if (Is(L, ELifestyle::TavernRegular) && (Goal == TEXT("socialize") || Goal == TEXT("relax")))
		{
			const FString Tavern = World.FirstCompletedTavernId();
			if (!Tavern.IsEmpty())
			{
				return Tavern;
			}
		}
		if (Is(L, ELifestyle::FamilyFirst)
			&& (Goal == TEXT("visitFamily") || Goal == TEXT("rest") || Goal == TEXT("relax"))
			&& !Npc.HomeId.IsEmpty())
		{
			return Npc.HomeId;
		}
		if (Is(L, ELifestyle::Workhorse) && Goal == TEXT("socialize") && !Npc.FavoriteBuildingId.IsEmpty())
		{
			if (World.HasBuilding(Npc.FavoriteBuildingId))
			{
				return Npc.FavoriteBuildingId;
			}
		}
		return FString();
	}

	void LifestyleNotePlaceUse(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Actor,
		const FString& Kind,
		FPlaceUseEntry& Entry,
		double Amount)
	{
		const FLifestyle& L = EnsureLifestyle(Lifestyle, Actor, nullptr);
		const double Gain = FMath::Max(0.05, Amount);
		TPair<FString, double>* Slot = Entry.Lifestyle.FindByPredicate([&L](const TPair<FString, double>& P)
		{
			return P.Key == L.Id;
		});
		if (Slot)
		{
			Slot->Value = FMath::Min(80.0, AnastasisJs::NumberOrZero(Slot->Value) + Gain);
		}
		else
		{
			Entry.Lifestyle.Emplace(L.Id, FMath::Min(80.0, 0.0 + Gain));
		}
		if (Is(L, ELifestyle::Workhorse) && IsWork(Kind)) Entry.Work += Amount * 0.35;
		if (Is(L, ELifestyle::TavernRegular)
			&& (Kind == TEXT("socialize") || Kind == TEXT("socialise") || Kind == TEXT("relax") || Kind == TEXT("relaxe")))
		{
			Entry.Social += Amount * 0.45;
		}
		if (Is(L, ELifestyle::FamilyFirst) && (Kind == TEXT("visitFamily") || Actor.HomeId == Entry.BuildingId))
		{
			Entry.Home += Amount * 0.45;
		}
	}

	void LifestyleDailyUpdate(
		TOptional<FLifestyle>& Lifestyle,
		const FLifestyleSubject& Npc,
		FAnastasisRng* SimRng,
		double Day,
		double DayFrac)
	{
		FLifestyle& L = EnsureLifestyle(Lifestyle, Npc, SimRng);
		if (L.LastNotedDay == Day)
		{
			return;
		}
		const EDayPhase Phase = DayPhase(DayFrac);
		const FString& Goal = Npc.Goal;
		const bool bAligned =
			(Is(L, ELifestyle::EarlyBird) && Phase == EDayPhase::Morning && IsWork(Goal))
			|| (Is(L, ELifestyle::NightOwl) && Phase == EDayPhase::Night && (Goal == TEXT("socialize") || Goal == TEXT("maintain")))
			|| (Is(L, ELifestyle::Workhorse) && IsWork(Goal))
			|| (Is(L, ELifestyle::Wanderer) && Goal == TEXT("explore"))
			|| (Is(L, ELifestyle::FamilyFirst) && Goal == TEXT("visitFamily"))
			|| (Is(L, ELifestyle::TavernRegular) && Goal == TEXT("socialize"));
		if (bAligned)
		{
			L.RhythmScore = FMath::Min(100.0, AnastasisJs::NumberOrZero(L.RhythmScore) + 1.0);
		}
		else
		{
			L.RhythmScore = FMath::Max(0.0, AnastasisJs::NumberOrZero(L.RhythmScore) - 0.2);
		}
		L.LastNotedDay = Day;
	}
}
