#include "Life/AnastasisNature.h"

namespace AnastasisNature
{
	namespace
	{
		/** Un penchant : but, poids. */
		struct FLean
		{
			const TCHAR* Goal;
			double Weight;
		};

		/** Une entree du catalogue : penchants, et ce que la decision lit en plus (travail, collant). */
		struct FEntry
		{
			const TCHAR* Id;
			TArray<FLean> Goals;
			double Work = 0.0;
			double Stick = 0.0;
		};

		/** `QUALITIES` : penchants et `work` (les penchants sociaux, `share` et `counsel` ne sont pas lus ici). */
		const TArray<FEntry>& Qualities()
		{
			static const TArray<FEntry> Table = {
				{ TEXT("genereux"), { { TEXT("socialize"), 0.9 }, { TEXT("visitFamily"), 0.5 }, { TEXT("deliver"), 0.35 } } },
				{ TEXT("patient"), { { TEXT("craft"), 0.8 }, { TEXT("maintain"), 0.55 }, { TEXT("study"), 0.45 }, { TEXT("rest"), 0.25 } }, 0.06 },
				{ TEXT("courageux"), { { TEXT("explore"), 0.9 }, { TEXT("confront"), 0.7 }, { TEXT("build"), 0.35 } } },
				{ TEXT("loyal"), { { TEXT("visitFamily"), 1.0 }, { TEXT("socialize"), 0.45 }, { TEXT("deliver"), 0.3 } } },
				{ TEXT("curieux"), { { TEXT("explore"), 1.05 }, { TEXT("study"), 0.7 }, { TEXT("apprentice"), 0.4 } } },
				{ TEXT("soigneux"), { { TEXT("craft"), 0.85 }, { TEXT("maintain"), 0.75 }, { TEXT("helpFarm"), 0.35 } }, 0.05 },
				{ TEXT("eloquent"), { { TEXT("socialize"), 1.0 }, { TEXT("sell"), 0.55 }, { TEXT("buy"), 0.25 } } },
				{ TEXT("sobre"), { { TEXT("rest"), 0.4 }, { TEXT("craft"), 0.35 }, { TEXT("relax"), -0.35 } } },
				{ TEXT("travailleur"), { { TEXT("gatherWood"), 0.45 }, { TEXT("gatherStone"), 0.45 }, { TEXT("gatherFood"), 0.45 },
					{ TEXT("build"), 0.55 }, { TEXT("deliver"), 0.4 }, { TEXT("craft"), 0.4 } }, 0.08 },
				{ TEXT("bienveillant"), { { TEXT("socialize"), 0.7 }, { TEXT("visitFamily"), 0.65 }, { TEXT("helpFarm"), 0.3 } } },
			};
			return Table;
		}

		/** `FLAWS`. */
		const TArray<FEntry>& Flaws()
		{
			static const TArray<FEntry> Table = {
				{ TEXT("avare"), { { TEXT("sell"), 0.7 }, { TEXT("buy"), -0.5 }, { TEXT("socialize"), -0.35 }, { TEXT("deliver"), -0.25 } } },
				{ TEXT("colerique"), { { TEXT("confront"), 1.15 }, { TEXT("socialize"), -0.4 }, { TEXT("visitFamily"), -0.2 } } },
				{ TEXT("peureux"), { { TEXT("explore"), -1.0 }, { TEXT("confront"), -0.9 }, { TEXT("gatherWood"), -0.25 }, { TEXT("build"), -0.2 } } },
				{ TEXT("paresseux"), { { TEXT("rest"), 0.9 }, { TEXT("relax"), 0.7 }, { TEXT("gatherWood"), -0.45 }, { TEXT("gatherStone"), -0.45 },
					{ TEXT("build"), -0.5 }, { TEXT("craft"), -0.35 } }, -0.1 },
				{ TEXT("vaniteux"), { { TEXT("socialize"), 0.55 }, { TEXT("sell"), 0.35 }, { TEXT("craft"), 0.25 }, { TEXT("deliver"), -0.2 } } },
				{ TEXT("mefiant"), { { TEXT("socialize"), -0.7 }, { TEXT("buy"), -0.35 }, { TEXT("confront"), 0.4 }, { TEXT("visitFamily"), 0.25 } } },
				{ TEXT("gourmand"), { { TEXT("eat"), 0.85 }, { TEXT("gatherFood"), 0.55 }, { TEXT("buy"), 0.35 }, { TEXT("sell"), -0.2 } } },
				{ TEXT("tetu"), { { TEXT("confront"), 0.45 }, { TEXT("build"), 0.3 }, { TEXT("socialize"), -0.25 } }, 0.0, 0.18 },
			};
			return Table;
		}

		const FEntry* Find(const TArray<FEntry>& Table, const FString& Id)
		{
			for (const FEntry& E : Table)
			{
				if (Id == E.Id) return &E;
			}
			return nullptr;
		}

		/** `q.goals[goal]` : 0 si absent (le `if (q?.goals?.[goal])` saute aussi un poids nul). */
		double LeanOf(const FEntry* E, const FString& Goal)
		{
			if (!E) return 0.0;
			for (const FLean& L : E->Goals)
			{
				if (Goal == L.Goal) return L.Weight;
			}
			return 0.0;
		}

		/** `clamp(Number(x) || 1, min, max)` : 0 et NaN deviennent 1. */
		double Attr(double Value)
		{
			const double V = (Value == 0.0 || FMath::IsNaN(Value)) ? 1.0 : Value;
			return FMath::Min(AttrMax, FMath::Max(AttrMin, V));
		}
	}

	bool IsQuality(const FString& Id) { return Find(Qualities(), Id) != nullptr; }
	bool IsFlaw(const FString& Id) { return Find(Flaws(), Id) != nullptr; }

	FNature Normalized(const FNature& Nature)
	{
		FNature Out;
		Out.Corps = Attr(Nature.Corps);
		Out.Esprit = Attr(Nature.Esprit);
		Out.Coeur = Attr(Nature.Coeur);
		for (const FString& Id : Nature.Qualities)
		{
			if (IsQuality(Id)) Out.Qualities.Add(Id);
		}
		for (const FString& Id : Nature.Flaws)
		{
			if (IsFlaw(Id)) Out.Flaws.Add(Id);
		}
		// `while (n.qualities.length > 2) n.qualities.pop()` : les dernieres partent.
		while (Out.Qualities.Num() > QualitySlots) Out.Qualities.Pop();
		while (Out.Flaws.Num() > FlawSlots) Out.Flaws.Pop();
		return Out;
	}

	double NatureGoalBias(const FNature& Raw, const FString& Goal)
	{
		if (Goal.IsEmpty()) return 0.0;
		const FNature N = Normalized(Raw);
		double Lean = 0.0;
		for (const FString& Id : N.Qualities) Lean += LeanOf(Find(Qualities(), Id), Goal);
		for (const FString& Id : N.Flaws) Lean += LeanOf(Find(Flaws(), Id), Goal);
		if (Goal == TEXT("socialize") || Goal == TEXT("visitFamily") || Goal == TEXT("play"))
		{
			Lean += (N.Coeur - 1.0) * 0.85;
		}
		if (Goal == TEXT("explore") || Goal == TEXT("study") || Goal == TEXT("apprentice"))
		{
			Lean += (N.Esprit - 1.0) * 0.7;
		}
		if (Goal.StartsWith(TEXT("gather"), ESearchCase::CaseSensitive) || Goal == TEXT("build") || Goal == TEXT("deliver") || Goal == TEXT("haulJob"))
		{
			Lean += (N.Corps - 1.0) * 0.65;
		}
		if (Goal == TEXT("craft") || Goal == TEXT("maintain"))
		{
			Lean += (N.Esprit - 1.0) * 0.55 + (N.Corps - 1.0) * 0.25;
		}
		return Lean * GoalPush;
	}

	double NatureWorkFactor(const FNature& Raw)
	{
		const FNature N = Normalized(Raw);
		double F = 0.92 + N.Corps * 0.08;
		for (const FString& Id : N.Qualities)
		{
			if (const FEntry* E = Find(Qualities(), Id)) F += E->Work;
		}
		for (const FString& Id : N.Flaws)
		{
			if (const FEntry* E = Find(Flaws(), Id)) F += E->Work;
		}
		return FMath::Min(1.22, FMath::Max(0.72, F));
	}

	double NatureStickBonus(const FNature& Raw)
	{
		const FNature N = Normalized(Raw);
		double Stick = 0.0;
		for (const FString& Id : N.Flaws)
		{
			if (const FEntry* E = Find(Flaws(), Id)) Stick += E->Stick;
		}
		for (const FString& Id : N.Qualities)
		{
			if (Id == TEXT("patient") || Id == TEXT("loyal") || Id == TEXT("travailleur")) Stick += 0.08;
		}
		return Stick;
	}
}
