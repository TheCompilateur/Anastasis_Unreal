#include "Life/AnastasisEpisodes.h"

#include "Algo/StableSort.h"
#include "Core/AnastasisJsNumeric.h"

namespace AnastasisEpisodes
{
	namespace
	{
		double Clamp(double V, double Lo, double Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

		struct FKindRow
		{
			const TCHAR* Kind;
			FKindModel Model;
		};

		// `KINDS` d'episodes.js, dans son ordre, puis les EXTENSIONS d'ANASTASIS (ecart n°47).
		const FKindRow Kinds[] = {
			{ TEXT("saved"), { 1.0, 34.0, 1.0 } },
			{ TEXT("arrival"), { 1.0, 22.0, 0.45 } },
			{ TEXT("robbed"), { -1.0, 32.0, 1.0 } },
			{ TEXT("bereaved"), { -1.0, 26.0, 0.9 } },
			{ TEXT("famine"), { -1.0, 24.0, 0.9 } },
			{ TEXT("inherited"), { 1.0, 18.0, 0.4 } },
			{ TEXT("born"), { 1.0, 22.0, 0.2 } },
			{ TEXT("union"), { 1.0, 20.0, 0.35 } },
			{ TEXT("raised"), { 1.0, 15.0, 0.45 } },
			{ TEXT("departure"), { -1.0, 14.0, 0.7 } },
			{ TEXT("hostingRefusal"), { -1.0, 18.0, 0.35 } },
			{ TEXT("hosting"), { 1.0, 18.0, 0.35 } },
			{ TEXT("ambition"), { 1.0, 22.0, 0.55 } },
			{ TEXT("crafted"), { 1.0, 14.0, 0.35 } },
			{ TEXT("counsel"), { 1.0, 16.0, 0.4 } },
			{ TEXT("reported"), { 0.0, 16.0, 0.45 } },
			{ TEXT("abandoned"), { -1.0, 12.0, 0.25 } },
			// EXTENSION (ecart n°47) -- le premier soir au feu : ou l'on etait quand la Ville est tombee, ce
			// qu'on a porte, qui n'est pas venu. Des malheurs qui courent ; l'objet sauve, moins.
			{ TEXT("fall"), { -1.0, 30.0, 0.8 } },
			{ TEXT("carried"), { 1.0, 20.0, 0.5 } },
			{ TEXT("leftBehind"), { -1.0, 28.0, 0.75 } },
			// EXTENSION (ecart n°47) -- l'aide demandee (Bible §29) : on se souvient de qui est venu, et de qui a refuse.
			{ TEXT("helped"), { 1.0, 26.0, 0.6 } },
			{ TEXT("refusedHelp"), { -1.0, 22.0, 0.65 } },
		};

		struct FGoalCoeff
		{
			const TCHAR* Goal;
			double Coeff;
		};

		struct FKindGoalBias
		{
			const TCHAR* Kind;
			TArray<FGoalCoeff> Goals;
		};

		// `KIND_GOAL_BIAS` d'episodes.js. EXTENSION : `helped` pousse a batir et a revoir les gens, `refusedHelp` a s'en tenir a soi.
		const TArray<FKindGoalBias>& KindGoalBias()
		{
			static const TArray<FKindGoalBias> Table = {
				{ TEXT("famine"), { { TEXT("gatherFood"), 1.25 }, { TEXT("deliver"), 0.55 }, { TEXT("sell"), -0.25 }, { TEXT("explore"), 0.2 } } },
				{ TEXT("saved"), { { TEXT("socialize"), 0.85 }, { TEXT("visitFamily"), 0.55 }, { TEXT("confront"), -0.45 } } },
				{ TEXT("robbed"), { { TEXT("confront"), 1.1 }, { TEXT("sell"), -0.4 }, { TEXT("maintain"), 0.35 }, { TEXT("socialize"), -0.15 } } },
				{ TEXT("bereaved"), { { TEXT("visitFamily"), 0.95 }, { TEXT("rest"), 0.45 }, { TEXT("socialize"), 0.4 }, { TEXT("sell"), -0.2 } } },
				{ TEXT("raised"), { { TEXT("build"), 0.9 }, { TEXT("craft"), 0.3 } } },
				{ TEXT("crafted"), { { TEXT("craft"), 0.95 }, { TEXT("gatherWood"), 0.25 } } },
				{ TEXT("counsel"), { { TEXT("socialize"), 0.55 }, { TEXT("visitFamily"), 0.45 }, { TEXT("apprentice"), 0.35 } } },
				{ TEXT("ambition"), { { TEXT("build"), 0.35 }, { TEXT("craft"), 0.3 }, { TEXT("socialize"), 0.25 }, { TEXT("gatherWood"), 0.2 } } },
				{ TEXT("abandoned"), { { TEXT("socialize"), 0.45 }, { TEXT("relax"), 0.4 }, { TEXT("explore"), -0.3 }, { TEXT("gatherWood"), -0.2 } } },
				{ TEXT("union"), { { TEXT("visitFamily"), 0.75 }, { TEXT("socialize"), 0.55 } } },
				{ TEXT("born"), { { TEXT("visitFamily"), 0.85 }, { TEXT("socialize"), 0.35 }, { TEXT("helpFarm"), 0.25 } } },
				{ TEXT("inherited"), { { TEXT("build"), 0.45 }, { TEXT("maintain"), 0.35 }, { TEXT("visitFamily"), 0.3 } } },
				{ TEXT("departure"), { { TEXT("socialize"), 0.5 }, { TEXT("visitFamily"), 0.35 } } },
				{ TEXT("helped"), { { TEXT("build"), 0.6 }, { TEXT("socialize"), 0.4 } } },
				{ TEXT("refusedHelp"), { { TEXT("socialize"), -0.3 }, { TEXT("build"), 0.25 } } },
			};
			return Table;
		}
	}

	const FKindModel* KindModel(const FString& Kind)
	{
		for (const FKindRow& Row : Kinds)
		{
			if (Kind == Row.Kind) return &Row.Model;
		}
		return nullptr;
	}

	double Standing(const FEpisode& Event)
	{
		return Event.Weight + (Event.bFirsthand ? Constants::FirsthandEdge : 0.0);
	}

	void Trim(FChronicle& Chronicle)
	{
		if (Chronicle.Events.Num() <= Constants::Capacity) return;
		// `events.sort((a, b) => standing(b) - standing(a))` : le tri de V8 est stable.
		Algo::StableSort(Chronicle.Events, [](const FEpisode& A, const FEpisode& B) { return Standing(B) < Standing(A); });
		Chronicle.Events.SetNum(Constants::Capacity);
	}

	bool KnowsRoot(const FChronicle& Chronicle, const FString& RootId)
	{
		for (const FEpisode& Known : Chronicle.Events)
		{
			if ((Known.RootId.IsEmpty() ? Known.Id : Known.RootId) == RootId) return true;
		}
		return false;
	}

	FBias StorytellerBias(double TraitExplore, double Morale, double Reputation)
	{
		FBias Bias;
		Bias.Fear = Clamp(1.45 - TraitExplore * 0.35 + (50.0 - Morale) / 90.0, 0.75, 1.9);
		Bias.Boast = Clamp(0.9 + Reputation / 140.0, 0.85, 1.6);
		return Bias;
	}

	FEpisode Retell(const FEpisode& Event, const FBias& Bias, TFunctionRef<double()> NextRandom)
	{
		const double Amplify = Event.Tone < 0.0 ? Bias.Fear : Bias.Boast;
		FEpisode Copy = Event;
		Copy.Id = FString::Printf(TEXT("%s/%d"), *Event.Id, Event.Hops + 1);
		Copy.RootId = Event.RootId.IsEmpty() ? Event.Id : Event.RootId;
		Copy.Hops = Event.Hops + 1;
		Copy.bFirsthand = false;
		// « Toujours une perte, jamais un gain. »
		Copy.Weight = Event.Weight * Clamp(Amplify * 0.55, Constants::RetellKeepMin, Constants::RetellKeepMax);
		// « Le chiffre enfle a chaque bouche » : `Math.round`, l'arrondi vers le haut des demis.
		if (Copy.Detail != 0.0) Copy.Detail = AnastasisJs::Floor(Copy.Detail * Amplify + 0.5);
		// « Passe deux bouches, le nom se perd avant les faits. »
		if (Copy.Hops >= 2 && NextRandom() < 0.5) Copy.AboutName.Reset();
		// « Le lieu derive. »
		if (Copy.Hops >= 2)
		{
			Copy.X += static_cast<int32>(AnastasisJs::Floor(NextRandom() * 5.0)) - 2;
			Copy.Y += static_cast<int32>(AnastasisJs::Floor(NextRandom() * 5.0)) - 2;
		}
		return Copy;
	}

	double Feeling(const FChronicle& Chronicle, const FString& OtherId)
	{
		double Value = 0.0;
		for (const FEpisode& Event : Chronicle.Events)
		{
			if (Event.AboutId != OtherId) continue;
			Value += Event.Tone * Event.Weight * (Event.bFirsthand ? 1.0 : Constants::HearsayScale);
		}
		return Value;
	}

	double GoalBias(const FChronicle& Chronicle, const FString& Goal)
	{
		if (Chronicle.Events.IsEmpty() || Goal.IsEmpty()) return 0.0;
		double Bias = 0.0;
		for (const FEpisode& Event : Chronicle.Events)
		{
			for (const FKindGoalBias& Row : KindGoalBias())
			{
				if (Event.Kind != Row.Kind) continue;
				for (const FGoalCoeff& G : Row.Goals)
				{
					if (Goal != G.Goal) continue;
					Bias += Event.Weight * G.Coeff * (Event.bFirsthand ? 1.0 : 0.45) * Constants::GoalScale;
				}
			}
		}
		return Clamp(Bias, -Constants::GoalCap, Constants::GoalCap);
	}

	const FEpisode* StrongestAbout(const FChronicle& Chronicle, const FString& OtherId)
	{
		const FEpisode* Strongest = nullptr;
		for (const FEpisode& Event : Chronicle.Events)
		{
			if (Event.AboutId != OtherId) continue;
			if (!Strongest || Event.Weight > Strongest->Weight) Strongest = &Event;
		}
		return Strongest;
	}

	int32 VariantIndex(const FEpisode& Event, int32 Count)
	{
		if (Count <= 1) return 0;
		const FString Key = !Event.Id.IsEmpty() ? Event.Id : (!Event.RootId.IsEmpty() ? Event.RootId : FString::Printf(TEXT("%s:%d"), *Event.Kind, Event.Day));
		uint32 H = 2166136261u;
		for (int32 I = 0; I < Key.Len(); ++I)
		{
			H ^= static_cast<uint32>(Key[I]);
			H *= 16777619u;
		}
		return static_cast<int32>(H % static_cast<uint32>(Count));
	}
}
