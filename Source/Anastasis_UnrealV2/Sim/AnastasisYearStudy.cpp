// annee-valmire-001 -- une annee a Valmire, mesuree. Voir AnastasisYearStudy.h.

#include "Sim/AnastasisYearStudy.h"

#include "Anastasis_UnrealV2.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Geo/AnastasisGeo.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Life/AnastasisBonds.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Sim/AnastasisTimeWarp.h"
#include "Sim/AnastasisVillageChronicle.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisBuild.h"
#include "Work/AnastasisGather.h"
#include "World/AnastasisWeather.h"
#include "World/AnastasisWorld.h"

namespace AnastasisYearStudy
{
	namespace
	{
		using namespace AnastasisVillage;

		/** Un monde de jeu jete apres usage, comme les tests de l'hote. */
		struct FScratchHost
		{
			UWorld* World = nullptr;
			UAnastasisSimulationSubsystem* Host = nullptr;

			FScratchHost()
			{
				World = GEngine ? UWorld::CreateWorld(EWorldType::Game, false) : nullptr;
				if (World)
				{
					FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
					Context.SetCurrentWorld(World);
					Host = World->GetSubsystem<UAnastasisSimulationSubsystem>();
				}
			}

			~FScratchHost()
			{
				if (World)
				{
					GEngine->DestroyWorldContext(World);
					World->DestroyWorld(false);
				}
			}
		};

		bool HasOption(const FVillage& V, const TCHAR* Goal)
		{
			for (const FPlayerGoalOption& Option : V.GetPlayerGoalOptions())
			{
				if (Option.Goal == Goal) return true;
			}
			return false;
		}

		/**
		 * Le joueur scripte, une fois par heure simulee. Il pose une intention comme le ferait quelqu'un aux
		 * touches 1 a 5 : un remede tant que le corps le demande (avec une marge pour ne pas osciller), puis,
		 * s'il travaille, le premier travail que sa table propose. Il ne marche jamais a la main.
		 */
		void DrivePlayer(FVillage& V, EPlayer Mode)
		{
			const FNpc* P = V.PlayerActor();
			if (!P || Mode == EPlayer::None) return;
			const AnastasisNeeds::FNeeds& N = P->Needs;
			const FPlayerGoalChoice* Choice = V.GetPlayerGoalChoice();
			const FString Current = Choice ? Choice->Goal : FString();
			FString Want;
			if (Current == GoalDrink && N.Thirst > 10.0) Want = GoalDrink;
			else if (Current == GoalEat && N.Hunger > 10.0) Want = GoalEat;
			else if (Current == GoalRest && N.Energy < 85.0) Want = GoalRest;
			else if (N.Thirst >= 40.0) Want = GoalDrink;
			else if (N.Hunger >= 36.0) Want = GoalEat;
			else if (N.Energy <= 25.0) Want = GoalRest;
			else if (Mode == EPlayer::Works)
			{
				const TCHAR* Work[] = { AnastasisBuild::GoalBuild, AnastasisGather::GoalDeliver, AnastasisGather::GoalGatherFood };
				for (const TCHAR* Goal : Work)
				{
					if (Current == Goal || HasOption(V, Goal)) { Want = Goal; break; }
				}
				// La table de la derniere pensee peut ne rien proposer : il tente la recolte, au risque d'un refus.
				if (Want.IsEmpty()) Want = AnastasisGather::GoalGatherFood;
			}
			if (Want != Current) V.ChoosePlayerGoal(Want);
		}

		FSample Measure(const FAnastasisSimulation& Sim, int32 Day, int32 Arrivals, int32 EmptyEvenings)
		{
			const FVillage& V = Sim.GetVillage();
			FSample S;
			S.Day = Day;
			S.Season = AnastasisWeather::SeasonId(AnastasisWeather::FieldSeasonFromDay(Day));
			S.Population = V.GetActors().Num();
			S.Families = V.GetFamilies().Num();
			S.Deaths = V.GetDeaths().Num();
			S.Arrivals = Arrivals;
		S.GranaryEmptyEvenings = EmptyEvenings;

			for (const FBuilding& B : V.GetBuildings())
			{
				if (B.Progress < 1.0) { ++S.SitesOpen; continue; }
				if (B.Type == HouseType) ++S.Houses;
				else if (B.Type == WellType) ++S.Wells;
				else if (B.Type == GranaryType) { ++S.Granaries; S.GranaryFood += B.FoodPhysical; }
				else ++S.OtherBuildings;
			}
			const AnastasisWorld::FWorld& W = Sim.GetWorld();
			for (const AnastasisWorld::FTile& Tile : W.Tiles)
			{
				if (Tile.Resource != AnastasisWorld::EResource::Food) continue;
				const AnastasisWorld::FTile Live = V.LiveTileAt(Tile.X, Tile.Y);
				if (Live.Resource == AnastasisWorld::EResource::Food) S.FieldFood += Live.Amount;
			}

			TMap<FString, int32> Jobs;
			TSet<FString> FriendPairs;
			double RelationSum = 0.0;
			int32 RelationCount = 0;
			double ReputationSum = 0.0;
			for (const FNpc& N : V.GetActors())
			{
				if (N.HomeId.IsEmpty()) ++S.Homeless;
				S.Meals += N.MealsTaken;
				S.FoodGathered += N.GatheredFood;
				S.FoodDelivered += N.DeliveredFood;
				S.WoodGathered += N.GatheredWood;
				S.MaterialsCarried += N.MaterialsDelivered;
				S.PiecesPlaced += N.PiecesPlaced;
				++Jobs.FindOrAdd(N.JobId.IsEmpty() ? FString(TEXT("-")) : N.JobId);
				S.Hunger += N.Needs.Hunger;
				S.Thirst += N.Needs.Thirst;
				S.Energy += N.Needs.Energy;
				S.Health += N.Needs.Health;
				S.Morale += N.Needs.Morale;
				S.Social += N.Needs.Social;
				if (N.Needs.Hunger >= AnastasisNeeds::Constants::HungerCritical || N.Needs.Thirst >= AnastasisNeeds::Constants::ThirstCritical
					|| N.Needs.Health <= AnastasisNeeds::Constants::HealthCritical) ++S.Critical;
				for (const TPair<FString, double>& Rel : N.Relations)
				{
					RelationSum += Rel.Value;
					++RelationCount;
					if (Rel.Value >= AnastasisBonds::FriendAt)
					{
						FriendPairs.Add(N.Id < Rel.Key ? N.Id + TEXT("|") + Rel.Key : Rel.Key + TEXT("|") + N.Id);
					}
				}
				S.Conversations += N.SocialsTaken;
				S.Memories += N.Chronicle.Events.Num();
				S.StoriesTold += N.Chronicle.Told;
				ReputationSum += N.Reputation;
			}
			const double Alive = FMath::Max(1, S.Population);
			S.Hunger /= Alive; S.Thirst /= Alive; S.Energy /= Alive; S.Health /= Alive; S.Morale /= Alive; S.Social /= Alive;
			S.MeanReputation = ReputationSum / Alive;
			S.MeanRelation = RelationCount > 0 ? RelationSum / RelationCount : 0.0;
			S.Friendships = FriendPairs.Num();
			Jobs.KeySort(TLess<FString>());
			for (const TPair<FString, int32>& J : Jobs)
			{
				S.Jobs += FString::Printf(TEXT("%s%s:%d"), S.Jobs.IsEmpty() ? TEXT("") : TEXT(" "), *J.Key, J.Value);
			}
			for (const FVillage::FHelpAnswer& Answer : V.GetHelpLog())
			{
				++S.HelpAsked;
				if (Answer.bAccepted) ++S.HelpAccepted;
			}
			if (const FNpc* P = V.PlayerActor())
			{
				S.bPlayerAlive = true;
				S.PlayerReputation = P->Reputation;
				S.PlayerPieces = P->PiecesPlaced;
				S.PlayerFoodDelivered = P->DeliveredFood;
				S.PlayerMeals = P->MealsTaken;
			}
			return S;
		}

		FString Escape(const FString& In)
		{
			return In.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
		}
	}

	TArray<FScenario> DefaultScenarios()
	{
		return {
			{ TEXT("sans-joueur"), TEXT("Sans joueur"), EPlayer::None, false },
			{ TEXT("joueur-qui-survit"), TEXT("Joueur qui ne fait que survivre"), EPlayer::Survives, false },
			{ TEXT("joueur-qui-travaille"), TEXT("Joueur qui travaille"), EPlayer::Works, false },
			{ TEXT("sans-joueur-village-ferme"), TEXT("Sans joueur, village ferme au monde exterieur"), EPlayer::None, true },
		};
	}

	FRun Run(const FScenario& Scenario, int32 Days, int32 Every, uint32 Seed)
	{
		FRun R;
		R.Scenario = Scenario;
		R.Days = Days;
		const double Started = FPlatformTime::Seconds();
		FScratchHost Scratch;
		if (!Scratch.Host) return R;
		Scratch.Host->ResetCanonical(Seed);
		const FPoint Settlement = Scratch.Host->GetSimulation().GetVillage().GetSettlement();
		// Le village ferme : le monde exterieur ne se charge pas avec les fondateurs (rendu tel quel ensuite).
		IConsoleVariable* AutoLoad = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Geo.AutoLoad"));
		const int32 AutoLoadBefore = AutoLoad ? AutoLoad->GetInt() : 1;
		if (AutoLoad && Scenario.bClosedValley) AutoLoad->Set(0, ECVF_SetByCode);
		R.bSeeded = !Scratch.Host->SeedStartVillage(12, FMath::FloorToInt32(Settlement.X), FMath::FloorToInt32(Settlement.Y)).IsEmpty();
		if (AutoLoad && Scenario.bClosedValley) AutoLoad->Set(AutoLoadBefore, ECVF_SetByCode);
		if (!R.bSeeded) return R;

		FAnastasisSimulation& Sim = Scratch.Host->GetSimulation();
		FVillage& V = Sim.GetVillage();
		if (Scenario.Player != EPlayer::None)
		{
			R.PlayerId = V.ArriveAsPlayer();
		}
		TSet<FString> Seen;
		for (const FNpc& N : V.GetActors()) Seen.Add(N.Id);
		const int32 Founders = Seen.Num();
		TSet<FString> Built;
		for (const FBuilding& B : V.GetBuildings()) if (B.Progress >= 1.0) Built.Add(B.Id);
		int32 DeathsLogged = 0;

		AnastasisChronicle::FVillageChronicle Chronicle = Scratch.Host->GetChronicle();
		Chronicle.Observe(Sim);
		int32 EmptyEvenings = 0;
		R.Samples.Add(Measure(Sim, 0, 0, 0));
		const double Hour = FAnastasisSimulation::DayLength / 24.0;
		for (int32 Day = 1; Day <= Days; ++Day)
		{
			for (int32 H = 0; H < 24; ++H)
			{
				DrivePlayer(V, Scenario.Player);
				AnastasisTimeWarp::Advance(Sim, Hour);
				Chronicle.Observe(Sim);
				for (const FNpc& N : V.GetActors()) Seen.Add(N.Id);
			}
			int32 Stock = 0;
			for (const FBuilding& B : V.GetBuildings()) if (B.Type == GranaryType && B.Progress >= 1.0) Stock += B.FoodPhysical;
			if (Stock == 0) ++EmptyEvenings;
			for (const FBuilding& B : V.GetBuildings())
			{
				if (B.Progress >= 1.0 && !Built.Contains(B.Id))
				{
					Built.Add(B.Id);
					R.BuildLines.Add(FString::Printf(TEXT("jour %d : %s %s"), Day, *B.Type, *B.Id));
				}
			}
			for (; DeathsLogged < V.GetDeaths().Num(); ++DeathsLogged)
			{
				const FVillage::FDeath& D = V.GetDeaths()[DeathsLogged];
				R.DeathLines.Add(FString::Printf(TEXT("jour %d : %s (%s)%s"), D.Day, *D.NpcId, *D.Cause, D.NpcId == R.PlayerId ? TEXT(" -- le joueur") : TEXT("")));
			}
			if (Day % FMath::Max(1, Every) == 0 || Day == Days)
			{
				R.Samples.Add(Measure(Sim, Day, Seen.Num() - Founders, EmptyEvenings));
			}
		}
		R.Chronicle = Chronicle.Render();
		R.RealSeconds = FPlatformTime::Seconds() - Started;
		return R;
	}

	FString ToCsv(const FRun& Run)
	{
		FString Out = TEXT("jour;saison;habitants;familles;morts;arrivants;sans_maison;maisons;puits;greniers;autres_batiments;chantiers;"
			"grenier_portions;champs_portions;repas;recolte;livre;bois;materiaux_portes;pieces_posees;metiers;"
			"faim;soif;energie;sante;moral;social;en_crise;relation_moyenne;amities;conversations;souvenirs;histoires_racontees;"
			"aide_demandee;aide_acceptee;reputation_moyenne;joueur_vivant;joueur_reputation;joueur_pieces;joueur_livre;joueur_repas;soirs_grenier_vide\n");
		for (const FSample& S : Run.Samples)
		{
			Out += FString::Printf(
				TEXT("%d;%s;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%s;%.1f;%.1f;%.1f;%.1f;%.1f;%.1f;%d;%.1f;%d;%d;%d;%d;%d;%d;%.1f;%d;%.1f;%d;%d;%d;%d\n"),
				S.Day, *S.Season, S.Population, S.Families, S.Deaths, S.Arrivals, S.Homeless, S.Houses, S.Wells, S.Granaries,
				S.OtherBuildings, S.SitesOpen, S.GranaryFood, S.FieldFood, S.Meals, S.FoodGathered, S.FoodDelivered, S.WoodGathered,
				S.MaterialsCarried, S.PiecesPlaced, *S.Jobs, S.Hunger, S.Thirst, S.Energy, S.Health, S.Morale, S.Social, S.Critical,
				S.MeanRelation, S.Friendships, S.Conversations, S.Memories, S.StoriesTold, S.HelpAsked, S.HelpAccepted, S.MeanReputation,
				S.bPlayerAlive ? 1 : 0, S.PlayerReputation, S.PlayerPieces, S.PlayerFoodDelivered, S.PlayerMeals, S.GranaryEmptyEvenings);
		}
		return Out;
	}

	FString SummaryJson(const TArray<FRun>& Runs, uint32 Seed)
	{
		FString Out = FString::Printf(TEXT("{\n  \"seed\": %u,\n  \"days_per_year\": %d,\n  \"runs\": ["), Seed, DaysPerYear);
		for (int32 I = 0; I < Runs.Num(); ++I)
		{
			const FRun& R = Runs[I];
			const FSample First = R.Samples.Num() > 0 ? R.Samples[0] : FSample();
			const FSample Last = R.Samples.Num() > 0 ? R.Samples.Last() : FSample();
			FString Deaths, Builds;
			for (const FString& L : R.DeathLines) Deaths += FString::Printf(TEXT("%s\"%s\""), Deaths.IsEmpty() ? TEXT("") : TEXT(", "), *Escape(L));
			for (const FString& L : R.BuildLines) Builds += FString::Printf(TEXT("%s\"%s\""), Builds.IsEmpty() ? TEXT("") : TEXT(", "), *Escape(L));
			Out += FString::Printf(
				TEXT("%s\n    { \"name\": \"%s\", \"label\": \"%s\", \"seeded\": %s, \"days\": %d, \"real_seconds\": %.1f, \"player\": \"%s\",\n")
				TEXT("      \"start\": { \"population\": %d, \"houses\": %d, \"granary_food\": %d, \"field_food\": %d },\n")
				TEXT("      \"end\": { \"population\": %d, \"deaths\": %d, \"arrivals\": %d, \"houses\": %d, \"wells\": %d, \"granaries\": %d, \"sites_open\": %d, \"granary_food\": %d, \"granary_empty_evenings\": %d, \"field_food\": %d, \"meals\": %d, \"friendships\": %d, \"memories\": %d, \"help_asked\": %d, \"help_accepted\": %d, \"morale\": %.1f, \"health\": %.1f, \"player_alive\": %s },\n")
				TEXT("      \"deaths\": [%s],\n      \"completed\": [%s] }"),
				I == 0 ? TEXT("") : TEXT(","), *R.Scenario.Name, *Escape(R.Scenario.Label), R.bSeeded ? TEXT("true") : TEXT("false"), R.Days, R.RealSeconds, *R.PlayerId,
				First.Population, First.Houses, First.GranaryFood, First.FieldFood,
				Last.Population, Last.Deaths, Last.Arrivals, Last.Houses, Last.Wells, Last.Granaries, Last.SitesOpen, Last.GranaryFood, Last.GranaryEmptyEvenings, Last.FieldFood,
				Last.Meals, Last.Friendships, Last.Memories, Last.HelpAsked, Last.HelpAccepted, Last.Morale, Last.Health, Last.bPlayerAlive ? TEXT("true") : TEXT("false"),
				*Deaths, *Builds);
		}
		Out += TEXT("\n  ]\n}\n");
		return Out;
	}

	TArray<FString> WriteRuns(const TArray<FRun>& Runs, uint32 Seed, const FString& Dir)
	{
		TArray<FString> Written;
		const auto Save = [&Written](const FString& Text, const FString& Path)
		{
			if (FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) Written.Add(Path);
		};
		for (const FRun& R : Runs)
		{
			Save(ToCsv(R), FPaths::Combine(Dir, R.Scenario.Name + TEXT(".csv")));
			Save(R.Chronicle, FPaths::Combine(Dir, R.Scenario.Name + TEXT("-chronique.txt")));
		}
		Save(SummaryJson(Runs, Seed), FPaths::Combine(Dir, TEXT("summary.json")));
		return Written;
	}
}

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisYearStudy(
	TEXT("Anastasis.Etude.Annee"),
	TEXT("Anastasis.Etude.Annee [jours=240] [releve=10] [graines=12345[+7+...]] [quit] - annee-valmire-001 : le village du lancement sans rendu, "
		"quatre scenarios (sans joueur, joueur qui survit, joueur qui travaille, village ferme), un releve economique et social "
		"tous les N jours -> Saved/YearStudy/<horodatage>/seed-<graine>/ (csv, chroniques, summary.json). Ne touche pas au monde courant."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
	{
		const int32 Days = Args.IsValidIndex(0) ? FMath::Max(1, FCString::Atoi(*Args[0])) : 2 * AnastasisYearStudy::DaysPerYear;
		const int32 Every = Args.IsValidIndex(1) ? FMath::Max(1, FCString::Atoi(*Args[1])) : 10;
		// Une ou plusieurs graines, separees par des virgules ou des + (ExecCmds coupe sur la virgule) : un sous-dossier par graine.
		TArray<FString> SeedArgs;
		(Args.IsValidIndex(2) ? Args[2] : FString(TEXT("12345"))).Replace(TEXT("+"), TEXT(",")).ParseIntoArray(SeedArgs, TEXT(","), true);
		const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("YearStudy"),
			FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
		bool bAllSeeded = SeedArgs.Num() > 0;
		int32 Files = 0;
		for (const FString& SeedArg : SeedArgs)
		{
			const uint32 Seed = static_cast<uint32>(FCString::Strtoui64(*SeedArg, nullptr, 10));
			TArray<AnastasisYearStudy::FRun> Runs;
			for (const AnastasisYearStudy::FScenario& Scenario : AnastasisYearStudy::DefaultScenarios())
			{
				Runs.Add(AnastasisYearStudy::Run(Scenario, Days, Every, Seed));
				const AnastasisYearStudy::FRun& R = Runs.Last();
				const AnastasisYearStudy::FSample Last = R.Samples.Num() > 0 ? R.Samples.Last() : AnastasisYearStudy::FSample();
				UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("YEAR_STUDY seed=%u %s seeded=%d days=%d real=%.1fs population=%d deaths=%d arrivals=%d houses=%d sites=%d granary=%d empty_evenings=%d player_alive=%d"),
					Seed, *Scenario.Name, R.bSeeded ? 1 : 0, Days, R.RealSeconds, Last.Population, Last.Deaths, Last.Arrivals, Last.Houses,
					Last.SitesOpen, Last.GranaryFood, Last.GranaryEmptyEvenings, Last.bPlayerAlive ? 1 : 0);
				bAllSeeded &= R.bSeeded;
			}
			Files += AnastasisYearStudy::WriteRuns(Runs, Seed, FPaths::Combine(Root, FString::Printf(TEXT("seed-%u"), Seed))).Num();
		}
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("YEAR_STUDY %s dir=%s seeds=%d files=%d"),
			bAllSeeded && Files > 0 ? TEXT("COMPLETE") : TEXT("FAIL"), *Root, SeedArgs.Num(), Files);
		// `quit` : l'editeur sans rendu de year-study.ps1 se ferme de lui-meme (un `Quit` d'ExecCmds n'y passait pas).
		if (Args.IsValidIndex(3) && Args[3].Equals(TEXT("quit"), ESearchCase::IgnoreCase))
		{
			FPlatformMisc::RequestExit(false);
		}
	}));
