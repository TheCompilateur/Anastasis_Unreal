#include "Sim/AnastasisVillageChronicle.h"

#include "Algo/StableSort.h"
#include "Life/AnastasisBonds.h"
#include "Sim/AnastasisArrivals.h"
#include "Sim/AnastasisDialogueLines.h"
#include "Sim/AnastasisValmireFounders.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisPathfinding.h"
#include "Work/AnastasisBuild.h"
#include "Work/AnastasisGather.h"

namespace AnastasisChronicle
{
	namespace
	{
		/** Seuils d'entree et de sortie : un ecart large evite qu'un besoin qui oscille ecrive a chaque passage. */
		constexpr double HungerAt = 85.0;
		constexpr double HungerOverAt = 30.0;
		constexpr double ThirstAt = 85.0;
		constexpr double ThirstOverAt = 30.0;
		constexpr double WeakAt = 30.0;
		constexpr double RecoveredAt = 60.0;

		/** Prenoms baptismaux byzantins. Provisoires : les familles et leurs noms arrivent avec la mission 2. */
		const TCHAR* const MaleNames[] = {
			TEXT("Theodoros"), TEXT("Georgios"), TEXT("Ioannes"), TEXT("Niketas"), TEXT("Manuel"),
			TEXT("Konstantinos"), TEXT("Michael"), TEXT("Basileios"), TEXT("Nikolaos"), TEXT("Demetrios"),
			TEXT("Leon"), TEXT("Stephanos"), TEXT("Andronikos"), TEXT("Alexios"), TEXT("Theophilos"),
			TEXT("Gregorios"), TEXT("Symeon"), TEXT("Kosmas"), TEXT("Petros"), TEXT("Athanasios"),
			TEXT("Sabas"), TEXT("Eustathios"), TEXT("Isaakios"), TEXT("Pankratios"),
		};
		const TCHAR* const FemaleNames[] = {
			TEXT("Eirene"), TEXT("Maria"), TEXT("Anna"), TEXT("Theodora"), TEXT("Eudokia"),
			TEXT("Zoe"), TEXT("Helene"), TEXT("Sophia"), TEXT("Thekla"), TEXT("Euphrosyne"),
			TEXT("Xene"), TEXT("Martha"), TEXT("Agathe"), TEXT("Theophano"), TEXT("Marina"),
			TEXT("Kale"), TEXT("Anastasia"), TEXT("Barbara"), TEXT("Paraskeve"), TEXT("Eugenia"),
			TEXT("Kyranna"), TEXT("Photeine"), TEXT("Kassia"), TEXT("Euphemia"),
		};

		const TCHAR* const Ordinals[] = {
			TEXT("première"), TEXT("deuxième"), TEXT("troisième"), TEXT("quatrième"), TEXT("cinquième"),
			TEXT("sixième"), TEXT("septième"), TEXT("huitième"), TEXT("neuvième"), TEXT("dixième"),
		};

		/** Nom commun et genre d'un type de batiment. */
		void TypeNoun(const FString& Type, FString& OutNoun, bool& bOutFeminine)
		{
			bOutFeminine = false;
			if (Type == AnastasisVillage::HouseType) { OutNoun = TEXT("maison"); bOutFeminine = true; }
			else if (Type == AnastasisVillage::WellType) { OutNoun = TEXT("puits"); }
			else if (Type == AnastasisVillage::GranaryType) { OutNoun = TEXT("grenier"); }
			else { OutNoun = Type; }
		}

		/** « le grenier » -> « du grenier », « la maison » -> « de la maison ». */
		FString De(const FString& Label)
		{
			if (Label.StartsWith(TEXT("le "))) return TEXT("du ") + Label.Mid(3);
			if (Label.StartsWith(TEXT("les "))) return TEXT("des ") + Label.Mid(4);
			return TEXT("de ") + Label;
		}

		/**
		 * valmire-grows-001 (ecart n°50) : la raison d'un chantier qu'un habitant ouvre de lui-meme, telle que
		 * la simulation la donne (`FBuilding::OpenCause`, sans accents), dite en clair.
		 */
		FString HumanOpenCause(const FString& Cause)
		{
			if (Cause.StartsWith(TEXT("aucun grenier"))) return TEXT("la récolte n'a pas où aller");
			if (Cause.StartsWith(TEXT("le grenier deborde"))) return TEXT("le grenier déborde");
			// « 17 ames pour 1 puits »
			TArray<FString> Words;
			Cause.ParseIntoArray(Words, TEXT(" "));
			if (Words.Num() >= 5 && Words[1] == TEXT("ames") && Words[4] == TEXT("puits"))
			{
				const int32 Souls = FCString::Atoi(*Words[0]);
				const int32 Wells = FCString::Atoi(*Words[3]);
				return Wells <= 1
					? FString::Printf(TEXT("%d âmes pour un seul puits"), Souls)
					: FString::Printf(TEXT("%d âmes pour %d puits"), Souls, Wells);
			}
			return TEXT("il en manque un");
		}

		/** « le grenier » -> « au grenier ». */
		FString A(const FString& Label)
		{
			if (Label.StartsWith(TEXT("le "))) return TEXT("au ") + Label.Mid(3);
			if (Label.StartsWith(TEXT("les "))) return TEXT("aux ") + Label.Mid(4);
			return TEXT("à ") + Label;
		}

		FString Capitalize(const FString& Text)
		{
			if (Text.IsEmpty()) return Text;
			FString Out = Text;
			// Les seules minuscules accentuees en tete de phrase ici : « à » et « é ».
			if (Out.StartsWith(TEXT("à"))) return TEXT("À") + Out.Mid(1);
			if (Out.StartsWith(TEXT("é"))) return TEXT("É") + Out.Mid(1);
			Out[0] = FChar::ToUpper(Out[0]);
			return Out;
		}

		FString JobLabel(const FString& JobId, bool bFemale)
		{
			if (JobId == AnastasisGather::JobFarmer) return bFemale ? TEXT("cultivatrice") : TEXT("cultivateur");
			if (JobId == AnastasisBuild::JobBuilder) return bFemale ? TEXT("bâtisseuse") : TEXT("bâtisseur");
			if (JobId == AnastasisGather::JobSettler || JobId.IsEmpty()) return TEXT("sans métier");
			return JobId;
		}

		const TCHAR* E(bool bFemale) { return bFemale ? TEXT("e") : TEXT(""); }

		const TCHAR* S(int32 Count) { return Count > 1 ? TEXT("s") : TEXT(""); }

		/** « de Euphemia » -> « d'Euphemia » : elision devant une voyelle. */
		FString DeName(const FString& Names)
		{
			const TCHAR First = Names.IsEmpty() ? TEXT(' ') : FChar::ToUpper(Names[0]);
			const bool bVowel = First == TEXT('A') || First == TEXT('E') || First == TEXT('I') || First == TEXT('O')
				|| First == TEXT('U') || First == TEXT('Y');
			return (bVowel ? TEXT("d'") : TEXT("de ")) + Names;
		}

		/** La cause que donne la simulation (`de soif`, `d'epuisement`...), accentuee pour le recit. */
		FString CauseText(const FString& Cause)
		{
			if (Cause == TEXT("d'epuisement")) return TEXT("d'épuisement");
			return Cause;
		}
	}

	const TCHAR* KindName(EKind Kind)
	{
		switch (Kind)
		{
		case EKind::Founding: return TEXT("Founding");
		case EKind::Arrival: return TEXT("Arrival");
		case EKind::Departure: return TEXT("Departure");
		case EKind::Death: return TEXT("Death");
		case EKind::SiteOpened: return TEXT("SiteOpened");
		case EKind::BuildingPlaced: return TEXT("BuildingPlaced");
		case EKind::Helped: return TEXT("Helped");
		case EKind::BuildingDone: return TEXT("BuildingDone");
		case EKind::BuildingGone: return TEXT("BuildingGone");
		case EKind::Home: return TEXT("Home");
		case EKind::HomeLost: return TEXT("HomeLost");
		case EKind::Shelter: return TEXT("Shelter");
		case EKind::Job: return TEXT("Job");
		case EKind::Friendship: return TEXT("Friendship");
		case EKind::Quarrel: return TEXT("Quarrel");
		case EKind::Hunger: return TEXT("Hunger");
		case EKind::Fed: return TEXT("Fed");
		case EKind::Thirst: return TEXT("Thirst");
		case EKind::Drank: return TEXT("Drank");
		case EKind::Weak: return TEXT("Weak");
		case EKind::Recovered: return TEXT("Recovered");
		case EKind::FoodOut: return TEXT("FoodOut");
		case EKind::FoodBack: return TEXT("FoodBack");
		case EKind::FoodLow: return TEXT("FoodLow");
		case EKind::Scene: return TEXT("Scene");
		case EKind::Rumor: return TEXT("Rumor");
		case EKind::Legend: return TEXT("Legend");
		case EKind::HouseDecided: return TEXT("HouseDecided");
		case EKind::HelpGiven: return TEXT("HelpGiven");
		case EKind::HelpRefused: return TEXT("HelpRefused");
		case EKind::GroupArrival: return TEXT("GroupArrival");
		case EKind::Council: return TEXT("Council");
		case EKind::Welcomed: return TEXT("Welcomed");
		case EKind::TurnedAway: return TEXT("TurnedAway");
		case EKind::Stalled: return TEXT("Stalled");
		}
		return TEXT("Unknown");
	}

	FString HourLabel(int32 Hour)
	{
		if (Hour < 5) return TEXT("la nuit");
		if (Hour < 8) return TEXT("à l'aube");
		if (Hour < 12) return TEXT("le matin");
		if (Hour < 14) return TEXT("vers midi");
		if (Hour < 18) return TEXT("l'après-midi");
		if (Hour < 21) return TEXT("le soir");
		return TEXT("la nuit");
	}

	void FVillageChronicle::Reset(uint32 InSeed)
	{
		Seed = InSeed;
		bStarted = false;
		FirstDay = 0;
		CurrentDay = 0;
		DeathsSeen = 0;
		DeathsToday = 0;
		FoodLowDay = 0;
		LastStats = FStats();
		PersonOrder.Reset();
		People.Reset();
		BuildingOrder.Reset();
		Buildings.Reset();
		TypeCounts.Reset();
		UsedNames.Reset();
		Entries.Reset();
		Days.Reset();
		FamilyViews.Reset();
		LegendRoots.Reset();
		HelpSeen = 0;
		CouncilSeen = 0;
		GroupsTold.Reset();
	}

	FString FVillageChronicle::Quote(const FString& SpeakerId, const TCHAR* Pool, int32 Rank, const TMap<FString, FString>& Holes) const
	{
		if (!Lines) return FString();
		const FString Line = Lines->Pick(Pool, AnastasisDialogue::FLibrary::KeyOf(Seed, SpeakerId, Pool, Rank), Holes);
		return Line.IsEmpty() ? FString() : FString::Printf(TEXT(" « %s »"), *Line);
	}

	void FVillageChronicle::ReadFamilies(const FAnastasisSimulation& Sim)
	{
		FamilyViews.Reset();
		for (const AnastasisVillage::FVillage::FFamily& Family : Sim.GetVillage().GetFamilies())
		{
			FFamilyView& View = FamilyViews.AddDefaulted_GetRef();
			View.Id = Family.Id;
			View.Name = Family.Name;
			View.Members = Family.Adults;
			View.Members.Append(Family.Dependents);
		}
	}

	int32 FVillageChronicle::CountOf(EKind Kind) const
	{
		int32 Count = 0;
		for (const FEntry& Entry : Entries)
		{
			if (Entry.Kind == Kind) ++Count;
		}
		return Count;
	}

	FString FVillageChronicle::NameOf(const FString& NpcId) const
	{
		const FPersonState* State = People.Find(NpcId);
		return State ? State->Name : NpcId;
	}

	FString FVillageChronicle::Names(const TArray<FString>& Ids) const
	{
		FString Out;
		for (int32 Index = 0; Index < Ids.Num(); ++Index)
		{
			if (Index > 0) Out += Index == Ids.Num() - 1 ? TEXT(" et ") : TEXT(", ");
			Out += NameOf(Ids[Index]);
		}
		return Out;
	}

	void FVillageChronicle::Add(int32 Day, int32 Hour, EKind Kind, TArray<FString> InPeople, FString Text, bool bDayLog)
	{
		FEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.bDayLog = bDayLog;
		Entry.Day = Day;
		Entry.Hour = Hour;
		Entry.Kind = Kind;
		Entry.People = MoveTemp(InPeople);
		Entry.Text = MoveTemp(Text);
	}

	FString FVillageChronicle::PickName(const FString& NpcId, const FPersonLook& Look)
	{
		const bool bFemale = Look.bKnown ? Look.bFemale : (FCrc::StrCrc32(*NpcId) & 1u) != 0u;
		const TCHAR* const* List = bFemale ? FemaleNames : MaleNames;
		const int32 Count = static_cast<int32>(bFemale ? UE_ARRAY_COUNT(FemaleNames) : UE_ARRAY_COUNT(MaleNames));
		// Depart tire du nom de l'habitant et de la graine : stable pour une partie, different d'une partie a l'autre.
		const uint32 Start = FCrc::StrCrc32(*NpcId, Seed);
		for (int32 Pass = 0; Pass < 4; ++Pass)
		{
			for (int32 Step = 0; Step < Count; ++Step)
			{
				FString Name = List[(Start + static_cast<uint32>(Step)) % static_cast<uint32>(Count)];
				if (Pass > 0) Name += FString::Printf(TEXT(" %s"), Pass == 1 ? TEXT("le Jeune") : Pass == 2 ? TEXT("l'Autre") : TEXT("le Troisième"));
				if (!UsedNames.Contains(Name))
				{
					UsedNames.Add(Name);
					return Name;
				}
			}
		}
		return NpcId;
	}

	FVillageChronicle::FPersonState& FVillageChronicle::Meet(const AnastasisVillage::FNpc& Npc, int32 Day)
	{
		if (FPersonState* Known = People.Find(Npc.Id))
		{
			return *Known;
		}
		FPersonState State;
		State.Look = LookResolver ? LookResolver(Npc) : FPersonLook();
		// familles-feu-001 : ce que la simulation sait d'un habitant passe avant tout tirage.
		if (!Npc.Gender.IsEmpty())
		{
			State.Look.bKnown = true;
			State.Look.bFemale = Npc.Gender == TEXT("female");
		}
		if (Npc.Age > 0.0)
		{
			State.Look.Age = Npc.Age;
			State.Look.bElder = Npc.Age >= 58.0;
			State.Look.bChild = Npc.Age < 13.0;
		}
		if (!State.Look.bKnown)
		{
			State.Look.bFemale = (FCrc::StrCrc32(*Npc.Id) & 1u) != 0u;
		}
		State.FamilyName = Npc.FamilyName;
		if (!Npc.Name.IsEmpty())
		{
			State.Name = Npc.Name;
			UsedNames.Add(Npc.Name);
		}
		else
		{
			State.Name = PickName(Npc.Id, State.Look);
		}
		State.HomeId = Npc.HomeId;
		State.ShelterId = Npc.ShelterId;
		if (!Npc.ShelterId.IsEmpty()) State.SheltersTold.Add(Npc.ShelterId);
		State.JobId = Npc.JobId;
		State.WorkplaceId = Npc.WorkplaceId;
		State.bHungry = Npc.Needs.Hunger >= HungerAt;
		State.bThirsty = Npc.Needs.Thirst >= ThirstAt;
		State.bWeak = Npc.Needs.Health <= WeakAt;
		State.Pieces = Npc.PiecesPlaced;
		State.Materials = Npc.MaterialsDelivered;
		State.FirstDay = Day;
		for (const AnastasisEpisodes::FEpisode& Event : Npc.Chronicle.Events) State.Episodes.Add(Event.Id);
		for (const TPair<FString, double>& Relation : Npc.Relations)
		{
			if (Relation.Value >= AnastasisBonds::FriendAt) State.Friends.Add(Relation.Key);
			if (Relation.Value <= AnastasisBonds::RivalAt) State.Foes.Add(Relation.Key);
		}
		PersonOrder.Add(Npc.Id);
		return People.Add(Npc.Id, MoveTemp(State));
	}

	FString FVillageChronicle::LabelFor(const FString& Type)
	{
		FString Noun;
		bool bFeminine = false;
		TypeNoun(Type, Noun, bFeminine);
		const int32 Rank = ++TypeCounts.FindOrAdd(Type);
		// Les maisons se comptent des la premiere ; un puits ou un grenier seul s'appelle « le puits ».
		if (Rank == 1 && Type != AnastasisVillage::HouseType)
		{
			return FString::Printf(TEXT("%s %s"), bFeminine ? TEXT("la") : TEXT("le"), *Noun);
		}
		const FString Ordinal = Rank <= static_cast<int32>(UE_ARRAY_COUNT(Ordinals)) ? FString(Ordinals[Rank - 1]) : FString::Printf(TEXT("%de"), Rank);
		const FString Adjective = (!bFeminine && Rank == 1) ? FString(TEXT("premier")) : Ordinal;
		return FString::Printf(TEXT("%s %s %s"), bFeminine ? TEXT("la") : TEXT("le"), *Adjective, *Noun);
	}

	FString FVillageChronicle::BuildingLabel(const FString& BuildingId) const
	{
		const FBuildingState* State = Buildings.Find(BuildingId);
		return State ? State->Label : FString(TEXT("un bâtiment"));
	}

	int32 FVillageChronicle::Reach(const FAnastasisSimulation& Sim, const AnastasisVillage::FNpc& Npc, const FString& Type) const
	{
		const AnastasisVillage::FVillage& Village = Sim.GetVillage();
		const AnastasisPath::FWorldNavSource Nav(Village.GetNavGrid(), Sim.GetWorld());
		bool bAny = false;
		for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
		{
			if (Building.Type != Type || Building.Progress < 1.0) continue;
			bAny = true;
			for (const AnastasisPath::FPoint& Door : Building.AccessPoints)
			{
				TArray<AnastasisPath::FPoint> Path;
				if (AnastasisPath::FindPath(Nav, { Npc.X, Npc.Y }, Door, {}, Path)) return 2;
			}
		}
		return bAny ? 1 : 0;
	}

	FVillageChronicle::FStats FVillageChronicle::ReadStats(const FAnastasisSimulation& Sim) const
	{
		FStats Stats;
		const AnastasisVillage::FVillage& Village = Sim.GetVillage();
		Stats.Inhabitants = Village.GetActors().Num();
		for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
		{
			const bool bDone = Building.Progress >= 1.0;
			if (!bDone) { ++Stats.Sites; continue; }
			if (Building.Type == AnastasisVillage::HouseType) ++Stats.Houses;
			if (Building.Type == AnastasisVillage::GranaryType)
			{
				Stats.bHasGranary = true;
				Stats.Food += Building.FoodAvailable();
			}
		}
		for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
		{
			if (Npc.LivingHomeId().IsEmpty()) ++Stats.Homeless;
		}
		return Stats;
	}

	void FVillageChronicle::Found(const FAnastasisSimulation& Sim, int32 Day, int32 Hour)
	{
		const AnastasisVillage::FVillage& Village = Sim.GetVillage();
		bStarted = true;
		FirstDay = Day;
		CurrentDay = Day;
		DeathsSeen = Village.GetDeaths().Num();
		HelpSeen = Village.GetHelpLog().Num();
		CouncilSeen = Village.GetCouncilLog().Num();
		ReadFamilies(Sim);

		TArray<FString> Ids;
		for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
		{
			Meet(Npc, Day);
			Ids.Add(Npc.Id);
		}
		TArray<FString> Standing;
		TArray<FString> Open;
		for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
		{
			FBuildingState& State = Buildings.Add(Building.Id);
			BuildingOrder.Add(Building.Id);
			State.Type = Building.Type;
			State.Label = LabelFor(Building.Type);
			State.bDone = Building.Progress >= 1.0;
			State.Owner = Building.Owner;
			State.Progress = Building.Progress;
			State.ProgressDay = Day;
			(State.bDone ? Standing : Open).Add(State.Label);
		}

		FString Text = Ids.Num() == 0
			? FString(TEXT("Le village n'a encore personne."))
			: FString::Printf(TEXT("Le village s'éveille. Ils sont %d : %s."), Ids.Num(), *Names(Ids));
		if (FamilyViews.Num() > 0)
		{
			TArray<FString> Houses;
			TSet<FString> InFamily;
			for (const FFamilyView& View : FamilyViews)
			{
				Houses.Add(View.Name);
				InFamily.Append(View.Members);
			}
			TArray<FString> Alone;
			for (const FString& Id : Ids)
			{
				if (!InFamily.Contains(Id)) Alone.Add(Id);
			}
			FString HouseList;
			for (int32 Index = 0; Index < Houses.Num(); ++Index)
			{
				if (Index > 0) HouseList += Index == Houses.Num() - 1 ? TEXT(" et ") : TEXT(", ");
				HouseList += Houses[Index];
			}
			Text = FString::Printf(TEXT("Le village s'éveille. Ils sont %d, en %d maisons : %s%s."), Ids.Num(), Houses.Num(), *HouseList,
				Alone.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" ; et, hors des familles, %s"), *Names(Alone)));
		}
		auto Join = [](const TArray<FString>& Labels)
		{
			FString Out;
			for (int32 Index = 0; Index < Labels.Num(); ++Index)
			{
				if (Index > 0) Out += Index == Labels.Num() - 1 ? TEXT(" et ") : TEXT(", ");
				Out += Labels[Index];
			}
			return Out;
		};
		if (Standing.Num()) Text += FString::Printf(TEXT(" Déjà debout : %s."), *Join(Standing));
		if (Open.Num()) Text += FString::Printf(TEXT(" En chantier : %s."), *Join(Open));

		// Les metiers, groupes : une phrase pour le jour, une ligne par personne pour son propre recit.
		TArray<FString> JobOrder;
		TMap<FString, TArray<FString>> ByJob;
		TArray<FString> Housed;
		for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
		{
			const FString Key = Npc.JobId.IsEmpty() ? FString(AnastasisGather::JobSettler) : Npc.JobId;
			if (!ByJob.Contains(Key)) JobOrder.Add(Key);
			ByJob.FindOrAdd(Key).Add(Npc.Id);
			if (!Npc.HomeId.IsEmpty()) Housed.Add(Npc.Id);
		}
		for (const FString& Job : JobOrder)
		{
			const TArray<FString>& Who = ByJob[Job];
			if (Job == AnastasisGather::JobSettler)
			{
				Text += FString::Printf(TEXT(" Sans métier : %s."), *Names(Who));
			}
			else if (Job == AnastasisGather::JobFarmer)
			{
				Text += FString::Printf(TEXT(" Cultive%s pour le grenier : %s."), Who.Num() > 1 ? TEXT("nt") : TEXT(""), *Names(Who));
			}
			else if (Job == AnastasisBuild::JobBuilder)
			{
				Text += FString::Printf(TEXT(" Bâti%s : %s."), Who.Num() > 1 ? TEXT("ssent") : TEXT("t"), *Names(Who));
			}
			else
			{
				Text += FString::Printf(TEXT(" %s : %s."), *Job, *Names(Who));
			}
		}
		Text += Housed.Num() == 0
			? FString(TEXT(" Personne n'a encore de maison à soi."))
			: FString::Printf(TEXT(" %s %s une maison à soi."), *Names(Housed), Housed.Num() > 1 ? TEXT("ont") : TEXT("a"));
		Add(Day, Hour, EKind::Founding, Ids, Text);

		// Ce que chacun a deja : un toit, un metier. Une ligne par personne, pour son propre recit seulement.
		for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
		{
			const FPersonState& State = People[Npc.Id];
			TArray<FString> Parts;
			if (!Npc.HomeId.IsEmpty()) Parts.Add(FString::Printf(TEXT("vit dans %s"), *BuildingLabel(Npc.HomeId)));
			if (!Npc.JobId.IsEmpty() && Npc.JobId != AnastasisGather::JobSettler)
			{
				FString Job = FString::Printf(TEXT("est %s"), *JobLabel(Npc.JobId, State.Look.bFemale));
				if (!Npc.WorkplaceId.IsEmpty()) Job += TEXT(" ") + A(BuildingLabel(Npc.WorkplaceId));
				Parts.Add(Job);
			}
			if (Parts.IsEmpty()) Parts.Add(TEXT("est parmi les fondateurs, sans métier ni maison à soi"));
			{
				Add(Day, Hour, EKind::Founding, { Npc.Id },
					FString::Printf(TEXT("%s %s."), *State.Name, *FString::Join(Parts, TEXT(" et "))), /*bDayLog=*/false);
			}
		}
		LastStats = ReadStats(Sim);
	}

	void FVillageChronicle::CloseDay(int32 Day)
	{
		// Le grenier se juge d'un soir a l'autre : ses va-et-vient dans la journee ne sont pas un evenement.
		const FDaySummary* Previous = Days.Num() ? &Days.Last() : nullptr;
		const bool bWasEmpty = Previous ? (Previous->bHasGranary && Previous->Food <= 0) : false;
		const bool bWasLean = Previous && Previous->Food > 0 && Previous->Food < Previous->Inhabitants;
		const bool bEmpty = LastStats.bHasGranary && LastStats.Food <= 0;
		const bool bLean = LastStats.Food > 0 && LastStats.Food < LastStats.Inhabitants;
		constexpr int32 EveningHour = 20;
		if (LastStats.bHasGranary)
		{
			if (bEmpty && !bWasEmpty)
			{
				Add(Day, EveningHour, EKind::FoodOut, {}, TEXT("Le grenier est vide."));
			}
			else if (!bEmpty && bWasEmpty)
			{
				Add(Day, EveningHour, EKind::FoodBack, {}, FString::Printf(TEXT("Le grenier a de nouveau de quoi : %d portion%s."), LastStats.Food, S(LastStats.Food)));
			}
			else if (bLean && !bWasLean)
			{
				Add(Day, EveningHour, EKind::FoodLow, {}, FString::Printf(TEXT("Les réserves sont maigres : %d portion%s pour %d bouches."), LastStats.Food, S(LastStats.Food), LastStats.Inhabitants));
			}
		}
		// Un chantier qui n'avance plus se dit a 3, 7, 15 et 30 jours d'arret.
		static const int32 StallSteps[] = { 3, 7, 15, 30 };
		for (const FString& Id : BuildingOrder)
		{
			FBuildingState& Site = Buildings[Id];
			if (Site.bDone || Site.bGone) continue;
			const int32 Idle = Day - Site.ProgressDay;
			for (const int32 Step : StallSteps)
			{
				if (Idle >= Step && Site.StallTold < Step)
				{
					Site.StallTold = Step;
					Add(Day, EveningHour, EKind::Stalled, Site.Helpers, Site.Helpers.Num() == 0
						? FString::Printf(TEXT("Le chantier %s n'avance plus depuis %d jours : personne n'y travaille."), *De(Site.Label), Idle)
						: FString::Printf(TEXT("Le chantier %s n'avance plus depuis %d jours."), *De(Site.Label), Idle));
				}
			}
		}

		FDaySummary& Summary = Days.AddDefaulted_GetRef();
		Summary.Day = Day;
		Summary.Inhabitants = LastStats.Inhabitants;
		Summary.Houses = LastStats.Houses;
		Summary.Sites = LastStats.Sites;
		Summary.Food = LastStats.Food;
		Summary.bHasGranary = LastStats.bHasGranary;
		Summary.Homeless = LastStats.Homeless;
		Summary.EmptyDays = bEmpty ? ((bWasEmpty && Previous) ? Previous->EmptyDays + 1 : 1) : 0;
		for (const FEntry& Entry : Entries)
		{
			if (Entry.Day != Day) continue;
			++Summary.Events;
			if (Entry.Kind == EKind::Death) ++Summary.Deaths;
		}
	}

	void FVillageChronicle::Observe(const FAnastasisSimulation& Sim)
	{
		if (!Sim.IsRunning())
		{
			return;
		}
		const AnastasisVillage::FVillage& Village = Sim.GetVillage();
		const int32 Day = Sim.GetDay();
		const int32 Hour = FMath::Clamp(FMath::FloorToInt32(Sim.DayFrac() * 24.0), 0, 23);
		if (!bStarted)
		{
			// Rien a raconter tant que le village est vide : la chronique s'ouvre avec ses premiers habitants.
			if (Village.GetActors().IsEmpty() && Village.GetBuildings().IsEmpty()) return;
			Found(Sim, Day, Hour);
			return;
		}

		ReadFamilies(Sim);

		// Les jours finis se closent sur l'etat lu au passage precedent : le soir, pas apres minuit.
		while (CurrentDay < Day)
		{
			CloseDay(CurrentDay);
			++CurrentDay;
		}

		// 1. Les morts, avec la cause que la simulation leur donne.
		const TArray<AnastasisVillage::FVillage::FDeath>& Deaths = Village.GetDeaths();
		for (; DeathsSeen < Deaths.Num(); ++DeathsSeen)
		{
			const AnastasisVillage::FVillage::FDeath& Death = Deaths[DeathsSeen];
			FPersonState* State = People.Find(Death.NpcId);
			if (!State) continue;
			State->bAlive = false;
			State->DeathDay = Day;
			State->DeathCause = CauseText(Death.Cause);
			FString Text = Death.Cause.IsEmpty()
				? FString::Printf(TEXT("%s est mort%s."), *State->Name, E(State->Look.bFemale))
				: FString::Printf(TEXT("%s est mort%s %s."), *State->Name, E(State->Look.bFemale), *State->DeathCause);
			TArray<FString> Concerned = { Death.NpcId };
			for (const FFamilyView& View : FamilyViews)
			{
				if (!View.Members.Contains(Death.NpcId)) continue;
				for (const FString& Kin : View.Members)
				{
					const FPersonState* KinState = People.Find(Kin);
					if (Kin == Death.NpcId || !KinState || !KinState->bAlive) continue;
					Text += FString::Printf(TEXT(" %s :%s"), *KinState->Name, *Quote(Kin, TEXT("quotidien.mort"), Day, { { TEXT("absent"), State->Name } }));
					Concerned.Add(Kin);
					break;
				}
			}
			Add(Day, Hour, EKind::Death, Concerned, Text);
		}

		// 2. Les batiments nouveaux : un chantier qui s'ouvre, ou un batiment pose d'un coup.
		for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
		{
			if (Buildings.Contains(Building.Id)) continue;
			FBuildingState& State = Buildings.Add(Building.Id);
			BuildingOrder.Add(Building.Id);
			State.Type = Building.Type;
			State.Label = LabelFor(Building.Type);
			State.bDone = Building.Progress >= 1.0;
			State.Owner = Building.Owner;
			State.Progress = Building.Progress;
			State.ProgressDay = Day;
			const FFamilyView* OwnerFamily = Building.OwnerFamilyId.IsEmpty() ? nullptr
				: FamilyViews.FindByPredicate([&Building](const FFamilyView& V) { return V.Id == Building.OwnerFamilyId; });
			if (State.bDone)
			{
				Add(Day, Hour, EKind::BuildingPlaced, {}, FString::Printf(TEXT("On pose %s."), *State.Label));
			}
			else if (OwnerFamily)
			{
				// ecart n°48 : une famille sans maison decide de batir ; son chef trace la parcelle.
				FString Chef;
				for (const FString& Id : OwnerFamily->Members)
				{
					if (Building.AllowedBuilders.Num() && Building.AllowedBuilders[0] == Id) Chef = Id;
				}
				// Un des siens a deja un toit (le chef de la maison d'ouverture) : les autres dorment ailleurs.
				bool bPartly = false;
				for (const FString& Id : OwnerFamily->Members)
				{
					const FPersonState* Member = People.Find(Id);
					if (Member && !Member->HomeId.IsEmpty()) bPartly = true;
				}
				const FString Family = Capitalize(OwnerFamily->Name);
				const FString Who = Chef.IsEmpty() ? FString(TEXT("la famille")) : NameOf(Chef);
				Add(Day, Hour, EKind::HouseDecided, OwnerFamily->Members, bPartly
					? FString::Printf(TEXT("%s est à l'étroit, les siens dorment chez les autres : %s décide de bâtir pour eux et trace la parcelle %s."), *Family, *Who, *De(State.Label))
					: FString::Printf(TEXT("%s n'a pas encore de toit à soi : %s décide de bâtir et trace la parcelle %s."), *Family, *Who, *De(State.Label)));
			}
			else if (!Building.OpenedById.IsEmpty())
			{
				// valmire-grows-001 : le village decide d'un batiment commun, un habitant en trace l'emplacement.
				Add(Day, Hour, EKind::SiteOpened, { Building.OpenedById }, FString::Printf(TEXT("Le village décide de bâtir %s : %s. %s en trace l'emplacement."),
					*State.Label, *HumanOpenCause(Building.OpenCause), *NameOf(Building.OpenedById)));
			}
			else
			{
				Add(Day, Hour, EKind::SiteOpened, {}, FString::Printf(TEXT("On ouvre le chantier %s."), *De(State.Label)));
			}
		}

		// Les chantiers encore ouverts : si un seul, toute piece posee ou tout apport y va.
		FString OnlyOpenSite;
		int32 OpenSites = 0;
		for (const FString& Id : BuildingOrder)
		{
			const FBuildingState& State = Buildings[Id];
			if (!State.bDone && !State.bGone)
			{
				++OpenSites;
				OnlyOpenSite = Id;
			}
		}
		if (OpenSites != 1) OnlyOpenSite.Reset();

		// 3. Les habitants : arrivees, puis ce qui a change pour chacun.
		const FStats Stats = ReadStats(Sim);
		TSet<FString> Present;
		for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
		{
			Present.Add(Npc.Id);
			if (!People.Contains(Npc.Id))
			{
				FPersonState& Newcomer = Meet(Npc, Day);
				// ecart n°49 : un groupe venu par la route se raconte ensemble, plus bas.
				const AnastasisVillage::FVillage::FFamily* Group = Npc.FamilyId.IsEmpty() ? nullptr : Village.FindFamily(Npc.FamilyId);
				if (Group && Group->ArrivedDay > 0) continue;
				Add(Day, Hour, EKind::Arrival, { Npc.Id },
					FString::Printf(TEXT("%s arrive au village."), *Newcomer.Name));
				continue;
			}
			FPersonState& State = People[Npc.Id];
			const bool bFemale = State.Look.bFemale;

			// Qui met la main a quel chantier : une ligne la premiere fois qu'il y travaille.
			const bool bPlaced = Npc.PiecesPlaced > State.Pieces;
			const bool bCarried = Npc.MaterialsDelivered > State.Materials;
			if (bPlaced || bCarried)
			{
				FString SiteId;
				if (Npc.bHasBuildBinding && Buildings.Contains(Npc.BuildBinding) && !Buildings[Npc.BuildBinding].bDone)
				{
					SiteId = Npc.BuildBinding;
				}
				else
				{
					SiteId = OnlyOpenSite;
				}
				if (!SiteId.IsEmpty())
				{
					FBuildingState& Site = Buildings[SiteId];
					if (!Site.Helpers.Contains(Npc.Id))
					{
						Site.Helpers.Add(Npc.Id);
						Add(Day, Hour, EKind::Helped, { Npc.Id }, bPlaced
							? FString::Printf(TEXT("%s met la main au chantier %s."), *State.Name, *De(Site.Label))
							: FString::Printf(TEXT("%s apporte du bois et de la pierre au chantier %s."), *State.Name, *De(Site.Label)));
					}
				}
			}
			State.Pieces = Npc.PiecesPlaced;
			State.Materials = Npc.MaterialsDelivered;

			// Un toit a soi.
			if (Npc.HomeId != State.HomeId)
			{
				if (!Npc.HomeId.IsEmpty())
				{
					const FBuildingState* Home = Buildings.Find(Npc.HomeId);
					const bool bBuiltIt = Home && Home->Helpers.Contains(Npc.Id);
					FString Text = State.HomeId.IsEmpty()
						? FString::Printf(TEXT("%s s'installe dans %s"), *State.Name, *BuildingLabel(Npc.HomeId))
						: FString::Printf(TEXT("%s quitte %s pour %s"), *State.Name, *BuildingLabel(State.HomeId), *BuildingLabel(Npc.HomeId));
					if (bBuiltIt) Text += FString::Printf(TEXT(", qu'%s a aidé à bâtir"), bFemale ? TEXT("elle") : TEXT("il"));
					Add(Day, Hour, EKind::Home, { Npc.Id }, Text + TEXT("."));
				}
				else
				{
					Add(Day, Hour, EKind::HomeLost, { Npc.Id },
						FString::Printf(TEXT("%s n'a plus de maison."), *State.Name));
				}
				State.HomeId = Npc.HomeId;
			}

			// Un abri pour la nuit, sans maison a soi : raconte la premiere fois sous chaque toit.
			if (Npc.ShelterId != State.ShelterId)
			{
				if (!Npc.ShelterId.IsEmpty() && Npc.HomeId.IsEmpty() && !State.SheltersTold.Contains(Npc.ShelterId))
				{
					State.SheltersTold.Add(Npc.ShelterId);
					Add(Day, Hour, EKind::Shelter, { Npc.Id },
						FString::Printf(TEXT("%s trouve un abri pour la nuit dans %s."), *State.Name, *BuildingLabel(Npc.ShelterId)));
				}
				State.ShelterId = Npc.ShelterId;
			}

			// Un metier.
			if (Npc.JobId != State.JobId || Npc.WorkplaceId != State.WorkplaceId)
			{
				FString Text = FString::Printf(TEXT("%s devient %s"), *State.Name, *JobLabel(Npc.JobId, bFemale));
				if (!Npc.WorkplaceId.IsEmpty()) Text += TEXT(" ") + A(BuildingLabel(Npc.WorkplaceId));
				Add(Day, Hour, EKind::Job, { Npc.Id }, Text + TEXT("."));
				State.JobId = Npc.JobId;
				State.WorkplaceId = Npc.WorkplaceId;
			}

			// La faim, et pourquoi : pas de grenier, un grenier vide, ou un grenier qui a encore de quoi.
			const double Hunger = Npc.Needs.Hunger;
			if (!State.bHungry && Hunger >= HungerAt)
			{
				State.bHungry = true;
				FString Why;
				if (!Stats.bHasGranary) Why = TEXT(" : il n'y a pas de grenier");
				else if (Stats.Food <= 0) Why = TEXT(" : le grenier est vide");
				else if (Reach(Sim, Npc, AnastasisVillage::GranaryType) == 1)
				{
					Why = FString::Printf(TEXT(" : le grenier a encore %d portion%s, mais aucun chemin n'y mène depuis là où %s se trouve"),
						Stats.Food, S(Stats.Food), bFemale ? TEXT("elle") : TEXT("il"));
				}
				else Why = FString::Printf(TEXT(", alors que le grenier a encore %d portion%s"), Stats.Food, S(Stats.Food));
				Add(Day, Hour, EKind::Hunger, { Npc.Id }, FString::Printf(TEXT("%s a faim%s.%s"), *State.Name, *Why,
					*Quote(Npc.Id, TEXT("quotidien.faim"), Day)));
			}
			else if (State.bHungry && Hunger <= HungerOverAt)
			{
				State.bHungry = false;
				Add(Day, Hour, EKind::Fed, { Npc.Id }, FString::Printf(TEXT("%s a enfin mangé."), *State.Name));
			}

			const double Thirst = Npc.Needs.Thirst;
			if (!State.bThirsty && Thirst >= ThirstAt)
			{
				State.bThirsty = true;
				const int32 WellAccess = Reach(Sim, Npc, AnastasisVillage::WellType);
				const FString Why = WellAccess == 0 ? FString(TEXT(" : il n'y a pas de puits"))
					: WellAccess == 1 ? FString::Printf(TEXT(" : aucun chemin ne mène au puits depuis là où %s se trouve"), bFemale ? TEXT("elle") : TEXT("il"))
					: FString(TEXT(", alors que le puits est à sa portée"));
				Add(Day, Hour, EKind::Thirst, { Npc.Id }, FString::Printf(TEXT("%s a soif%s.%s"), *State.Name, *Why,
					*Quote(Npc.Id, TEXT("quotidien.soif"), Day)));
			}
			else if (State.bThirsty && Thirst <= ThirstOverAt)
			{
				State.bThirsty = false;
				Add(Day, Hour, EKind::Drank, { Npc.Id }, FString::Printf(TEXT("%s a pu boire."), *State.Name));
			}

			const double Health = Npc.Needs.Health;
			if (!State.bWeak && Health <= WeakAt)
			{
				State.bWeak = true;
				Add(Day, Hour, EKind::Weak, { Npc.Id }, FString::Printf(TEXT("%s s'affaiblit."), *State.Name));
			}
			else if (State.bWeak && Health >= RecoveredAt)
			{
				State.bWeak = false;
				Add(Day, Hour, EKind::Recovered, { Npc.Id }, FString::Printf(TEXT("%s reprend des forces."), *State.Name));
			}

			// memoire-decisions-001 : les histoires qu'on lui raconte, et celles qui deviennent legendes. Ce qu'il a
			// oublie sort de la liste : l'entendre de nouveau se raconte.
			{
				TSet<FString> Remembered;
				for (const AnastasisEpisodes::FEpisode& Event : Npc.Chronicle.Events) Remembered.Add(Event.Id);
				State.Episodes = State.Episodes.Intersect(Remembered);
			}
			for (const AnastasisEpisodes::FEpisode& Event : Npc.Chronicle.Events)
			{
				if (State.Episodes.Contains(Event.Id)) continue;
				State.Episodes.Add(Event.Id);
				if (Event.bFirsthand) continue;
				const FString Teller = NameOf(Event.SourceId);
				const FString Origin = Event.OriginalSourceId.IsEmpty() ? FString() : NameOf(Event.OriginalSourceId);
				const FString Story = Lines ? Lines->TellerVersion(Event, Origin == Teller ? FString() : Origin) : FString(TEXT("une histoire"));
				Add(Day, Hour, EKind::Rumor, { Npc.Id, Event.SourceId },
					FString::Printf(TEXT("%s raconte à %s : « %s »"), *Teller, *State.Name, *Capitalize(Story)));
				const FString Root = Event.RootId.IsEmpty() ? Event.Id : Event.RootId;
				if (AnastasisEpisodes::IsLegend(Event) && !LegendRoots.Contains(Root))
				{
					LegendRoots.Add(Root);
					Add(Day, Hour, EKind::Legend, { Npc.Id },
						FString::Printf(TEXT("Une légende court à Valmire : « %s »"), *Capitalize(Lines ? Lines->EpisodeLine(Event) : FString(TEXT("une histoire")))));
				}
			}

			// Amities et brouilles : une ligne par paire, la premiere fois qu'un des deux franchit le seuil.
			for (const TPair<FString, double>& Relation : Npc.Relations)
			{
				FPersonState* Other = People.Find(Relation.Key);
				if (!Other) continue;
				if (Relation.Value >= AnastasisBonds::FriendAt && !State.Friends.Contains(Relation.Key))
				{
					const bool bAlready = Other->Friends.Contains(Npc.Id);
					State.Friends.Add(Relation.Key);
					Other->Friends.Add(Npc.Id);
					if (!bAlready)
					{
						Add(Day, Hour, EKind::Friendship, { Npc.Id, Relation.Key },
							FString::Printf(TEXT("%s et %s deviennent ami%ss.%s"), *State.Name, *Other->Name,
								(State.Look.bFemale && Other->Look.bFemale) ? TEXT("e") : TEXT(""),
								*Quote(Npc.Id, TEXT("talkCatalog.BOND_LINES.friend"), Day)));
					}
				}
				if (Relation.Value <= AnastasisBonds::RivalAt && !State.Foes.Contains(Relation.Key))
				{
					const bool bAlready = Other->Foes.Contains(Npc.Id);
					State.Foes.Add(Relation.Key);
					Other->Foes.Add(Npc.Id);
					if (!bAlready)
					{
						Add(Day, Hour, EKind::Quarrel, { Npc.Id, Relation.Key },
							FString::Printf(TEXT("%s et %s se sont brouillé%ss."), *State.Name, *Other->Name,
								(State.Look.bFemale && Other->Look.bFemale) ? TEXT("e") : TEXT("")));
					}
				}
			}
		}

		// ecart n°48 : le tour du village -- qui a demande de l'aide a qui, la reponse, et sa raison.
		const TArray<AnastasisVillage::FVillage::FHelpAnswer>& Help = Village.GetHelpLog();
		for (; HelpSeen < Help.Num(); ++HelpSeen)
		{
			const AnastasisVillage::FVillage::FHelpAnswer& Answer = Help[HelpSeen];
			FString Pool = Answer.bAccepted
				? (Answer.Reason == TEXT("voisin") || Answer.Reason.IsEmpty() ? FString(TEXT("aide.accepte")) : FString::Printf(TEXT("aide.accepte.%s"), *Answer.Reason))
				: FString::Printf(TEXT("aide.refuse.%s"), *Answer.Reason);
			// « Occupe » parle de son ouvrage : le champ pour un cultivateur, le chantier pour un batisseur.
			if (!Answer.bAccepted && Answer.Reason == TEXT("occupe") && Lines)
			{
				const FPersonState* Asked = People.Find(Answer.ToId);
				const FString ByTrade = Asked ? FString::Printf(TEXT("aide.refuse.occupe.%s"), *Asked->JobId) : FString();
				if (!ByTrade.IsEmpty() && Lines->HasPool(ByTrade)) Pool = ByTrade;
			}
			const FString Said = Lines ? Lines->Pick(Pool, AnastasisDialogue::FLibrary::KeyOf(Seed, Answer.ToId, Pool, HelpSeen)) : FString();
			const FString Ask = Lines ? Lines->Pick(TEXT("aide.demande.chantier"), AnastasisDialogue::FLibrary::KeyOf(Seed, Answer.FromId, TEXT("aide.demande.chantier"), HelpSeen)) : FString();
			const FString Text = FString::Printf(TEXT("%s va demander de l'aide à %s%s. %s %s%s"),
				*NameOf(Answer.FromId), *NameOf(Answer.ToId), Ask.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" : « %s »"), *Ask),
				*NameOf(Answer.ToId), Answer.bAccepted ? TEXT("accepte") : TEXT("refuse"),
				Said.IsEmpty() ? TEXT(".") : *FString::Printf(TEXT(" : « %s »"), *Said));
			Add(Day, Hour, Answer.bAccepted ? EKind::HelpGiven : EKind::HelpRefused, { Answer.FromId, Answer.ToId }, Text);
		}

		// ecart n°49 : les groupes arrives par la route, puis le conseil du soir.
		for (const AnastasisVillage::FVillage::FFamily& Group : Village.GetFamilies())
		{
			if (Group.ArrivedDay <= 0 || GroupsTold.Contains(Group.Id)) continue;
			GroupsTold.Add(Group.Id);
			TArray<FString> Members = Group.Adults;
			Members.Append(Group.Dependents);
			if (Members.IsEmpty()) continue;
			const FString Road = Group.Origin.IsEmpty() ? FString(TEXT("Par la route")) : FString::Printf(TEXT("Par la route %s"), *De(Group.Origin));
			const bool bAlone = Members.Num() == 1;
			FString Text = bAlone
				? FString::Printf(TEXT("%s, %s arrive seul%s."), *Road, *NameOf(Members[0]), E(People.Contains(Members[0]) && People[Members[0]].Look.bFemale))
				: FString::Printf(TEXT("%s, un groupe arrive : %s, %s."), *Road, *Names(Members), *Group.Name);
			const bool bFemale = bAlone && People.Contains(Members[0]) && People[Members[0]].Look.bFemale;
			if (!Group.Cause.IsEmpty()) Text += FString::Printf(TEXT(" %s %s."), bAlone ? (bFemale ? TEXT("Elle fuit") : TEXT("Il fuit")) : TEXT("Ils fuient"), *Group.Cause);
			Text += FString::Printf(TEXT(" %s demande à rester :%s"), *NameOf(Members[0]),
				*Quote(Members[0], TEXT("accueil.demande"), Day, { { TEXT("cause"), Group.Cause } }));
			Add(Day, Hour, EKind::GroupArrival, Members, Text);
		}
		const TArray<AnastasisVillage::FVillage::FCouncil>& Councils = Village.GetCouncilLog();
		for (; CouncilSeen < Councils.Num(); ++CouncilSeen)
		{
			const AnastasisVillage::FVillage::FCouncil& Council = Councils[CouncilSeen];
			const AnastasisVillage::FVillage::FFamily* Group = Village.FindFamily(Council.FamilyId);
			FString GroupName = Group ? Group->Name : FString(TEXT("les nouveaux venus"));
			// Une personne seule a un nom, pas un nom de groupe.
			if (Group && Group->Adults.Num() + Group->Dependents.Num() == 1)
			{
				GroupName = NameOf(Group->Adults.Num() ? Group->Adults[0] : Group->Dependents[0]);
			}
			// Le moine parle, mais ne decide pas (Alexandre, 2026-10-08).
			FString Monk;
			if (const AnastasisFounders::FScenario* Founding = AnastasisFounders::FScenario::Get())
			{
				for (const FString& Id : PersonOrder)
				{
					const FPersonState& State = People[Id];
					if (State.bAlive && !State.bGone && State.Name == Founding->Monk.Given) Monk = Id;
				}
			}
			FString Opening = FString::Printf(TEXT("Au feu, les chefs de famille parlent %s."), *De(GroupName));
			if (!Monk.IsEmpty()) Opening += FString::Printf(TEXT(" %s :%s"), *NameOf(Monk), *Quote(Monk, TEXT("accueil.moine"), CouncilSeen));
			TArray<FString> Voters;
			for (const AnastasisVillage::FVillage::FWelcomeVote& Vote : Council.Votes) Voters.Add(Vote.VoterId);
			Add(Day, Hour, EKind::Council, Voters, Opening);
			for (const AnastasisVillage::FVillage::FWelcomeVote& Vote : Council.Votes)
			{
				const FString Said = Lines ? AnastasisArrivals::VoteLine(*Lines, Seed, Vote, Council.Cause, CouncilSeen) : FString();
				Add(Day, Hour, EKind::Council, { Vote.VoterId }, FString::Printf(TEXT("%s : %s.%s"), *NameOf(Vote.VoterId),
					Vote.bYes ? TEXT("oui") : TEXT("non"), Said.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" « %s »"), *Said)));
			}
			int32 Yes = 0;
			for (const AnastasisVillage::FVillage::FWelcomeVote& Vote : Council.Votes) Yes += Vote.bYes ? 1 : 0;
			const FString Count = FString::Printf(TEXT("%d oui, %d non"), Yes, Council.Votes.Num() - Yes);
			if (Council.bAccepted)
			{
				Add(Day, Hour, EKind::Welcomed, Voters, FString::Printf(TEXT("%s : Valmire garde %s."), *Count, *GroupName));
				continue;
			}
			// Refuses : ils repartent au matin ; on ne les dit pas partis un par un.
			TArray<FString> Gone;
			for (const FString& Id : PersonOrder)
			{
				FPersonState& State = People[Id];
				if (State.bAlive && !State.bGone && State.FamilyName == GroupName && !Village.FindNpc(Id))
				{
					State.bGone = true;
					Gone.Add(Id);
				}
			}
			Add(Day, Hour, EKind::TurnedAway, Gone, FString::Printf(TEXT("%s : le conseil refuse %s. Au matin, %s la route, sans savoir où aller."),
				*Count, *GroupName, Gone.Num() == 1 ? TEXT("il reprend") : TEXT("ils reprennent")));
		}

		// 4. Ceux qui ne sont plus la sans etre morts : ils sont partis.
		for (const FString& Id : PersonOrder)
		{
			FPersonState& State = People[Id];
			if (State.bAlive && !State.bGone && !Present.Contains(Id))
			{
				State.bGone = true;
				Add(Day, Hour, EKind::Departure, { Id },
					FString::Printf(TEXT("%s a quitté le village."), *State.Name));
			}
		}

		// 5. Chantiers acheves, batiments disparus.
		for (const FString& Id : BuildingOrder)
		{
			FBuildingState& State = Buildings[Id];
			if (State.bGone) continue;
			const AnastasisVillage::FBuilding* Building = Village.FindBuilding(Id);
			if (!Building)
			{
				State.bGone = true;
				Add(Day, Hour, EKind::BuildingGone, {}, FString::Printf(TEXT("%s a disparu."), *Capitalize(State.Label)));
				continue;
			}
			const bool bAwaits = !Building->OwnerFamilyId.IsEmpty() && Village.AwaitsHelp(*Building);
			if (bAwaits && !State.bAwaitTold)
			{
				State.bAwaitTold = true;
				const FFamilyView* OwnerFamily = FamilyViews.FindByPredicate([Building](const FFamilyView& V) { return V.Id == Building->OwnerFamilyId; });
				Add(Day, Hour, EKind::Stalled, OwnerFamily ? OwnerFamily->Members : TArray<FString>(),
					FString::Printf(TEXT("Les murs %s sont debout ; %s attend des bras pour le toit : on ne lève pas un toit seul."),
						*De(State.Label), OwnerFamily ? *OwnerFamily->Name : TEXT("la famille")));
			}
			State.bAwaitsHelp = bAwaits;
			if (Building->Progress > State.Progress)
			{
				State.Progress = Building->Progress;
				State.ProgressDay = Day;
				State.StallTold = 0;
			}
			if (!State.bDone && Building->Progress >= 1.0)
			{
				State.bDone = true;
				FString Text = FString::Printf(TEXT("%s est achevé%s"), *Capitalize(State.Label),
					State.Label.StartsWith(TEXT("la ")) ? TEXT("e") : TEXT(""));
				// Ceux qui y ont pose des pieces, tels que la simulation les compte (`building.workers`) ; pour la
				// maison d'une famille, ceux du dehors seulement : les siens vont de soi.
				const FFamilyView* Owners = Building->OwnerFamilyId.IsEmpty() ? nullptr
					: FamilyViews.FindByPredicate([Building](const FFamilyView& V) { return V.Id == Building->OwnerFamilyId; });
				TArray<FString> Workers;
				for (const TPair<FString, int32>& Worker : Building->Workers)
				{
					if (Worker.Value > 0 && !(Owners && Owners->Members.Contains(Worker.Key))) Workers.Add(Worker.Key);
				}
				if (Workers.IsEmpty() && !Owners) Workers = State.Helpers;
				if (Workers.Num()) Text += FString::Printf(TEXT(", grâce à %s"), *Names(Workers));
				const FFamilyView* OwnerFamily = Building->OwnerFamilyId.IsEmpty() ? nullptr
					: FamilyViews.FindByPredicate([Building](const FFamilyView& V) { return V.Id == Building->OwnerFamilyId; });
				if (OwnerFamily) Text += FString::Printf(TEXT(". Elle revient à %s"), *OwnerFamily->Name);
				Add(Day, Hour, EKind::BuildingDone, Workers, Text + TEXT("."));
			}
		}

		// Le grenier se raconte au bilan du soir (CloseDay), d'un soir a l'autre.
		LastStats = Stats;
	}

	FString FVillageChronicle::Render() const
	{
		FString Out;
		const int32 LastEntryDay = Entries.Num() ? Entries.Last().Day : FirstDay;
		const int32 LastDay = FMath::Max(GetLastClosedDay(), LastEntryDay);
		Out += TEXT("CHRONIQUE DE VALMIRE\n");
		if (!bStarted)
		{
			Out += TEXT("Le village n'a encore personne : rien à raconter.\n");
			return Out;
		}
		Out += FString::Printf(TEXT("Graine %u. Du jour %d au jour %d ; %d jour%s clos.\n"),
			Seed, FirstDay, LastDay, Days.Num(), Days.Num() > 1 ? TEXT("s") : TEXT(""));
		// Sans foyer dans la simulation, les habitants portent des noms provisoires : on le dit, seulement alors.
		if (FamilyViews.IsEmpty()) Out += TEXT("Les noms sont provisoires : la simulation ne les porte pas pour ces habitants.\n");
		Out += TEXT("\n");

		// En bref : ce qu'un lecteur presse doit savoir avant les jours.
		{
			const int32 Founders = People.Num() - CountOf(EKind::Arrival);
			int32 Alive = 0;
			TMap<FString, int32> Causes;
			TArray<FString> CauseOrder;
			for (const FString& Id : PersonOrder)
			{
				const FPersonState& State = People[Id];
				if (State.bAlive && !State.bGone) ++Alive;
				if (!State.bAlive)
				{
					const FString Cause = State.DeathCause.IsEmpty() ? FString(TEXT("sans cause dite")) : State.DeathCause;
					if (!Causes.Contains(Cause)) CauseOrder.Add(Cause);
					++Causes.FindOrAdd(Cause);
				}
			}
			const int32 Dead = CountOf(EKind::Death);
			int32 EmptyEvenings = 0;
			for (const FDaySummary& Summary : Days)
			{
				if (Summary.bHasGranary && Summary.Food <= 0) ++EmptyEvenings;
			}
			TArray<FString> CauseParts;
			for (const FString& Cause : CauseOrder) CauseParts.Add(FString::Printf(TEXT("%d %s"), Causes[Cause], *Cause));
			Out += TEXT("EN BREF\n");
			Out += FString::Printf(TEXT("- %d fondateur%s, %d arrivé%s, %d parti%s ; %d en vie à la fin.\n"),
				Founders, S(Founders), CountOf(EKind::Arrival), S(CountOf(EKind::Arrival)),
				CountOf(EKind::Departure), S(CountOf(EKind::Departure)), Alive);
			Out += Dead == 0 ? FString(TEXT("- Personne n'est mort.\n"))
				: FString::Printf(TEXT("- %d mort%s : %s.\n"), Dead, S(Dead), *FString::Join(CauseParts, TEXT(", ")));
			int32 StillOpen = 0;
			for (const FString& Id : BuildingOrder)
			{
				const FBuildingState& Site = Buildings[Id];
				if (!Site.bDone && !Site.bGone) ++StillOpen;
			}
			const int32 Done = CountOf(EKind::BuildingDone);
			Out += FString::Printf(TEXT("- %d chantier%s mené%s à bout ; %d encore ouvert%s.\n"),
				Done, S(Done), S(Done), StillOpen, S(StillOpen));
			if (Days.Num())
			{
				Out += FString::Printf(TEXT("- Grenier vide %d soir%s sur %d.\n"), EmptyEvenings, S(EmptyEvenings), Days.Num());
			}
			Out += FString::Printf(TEXT("- %d histoire%s racontée%s de bouche en bouche, %d devenue%s légende%s.\n"),
				CountOf(EKind::Rumor), S(CountOf(EKind::Rumor)), S(CountOf(EKind::Rumor)),
				CountOf(EKind::Legend), S(CountOf(EKind::Legend)), S(CountOf(EKind::Legend)));
			Out += FString::Printf(TEXT("- %d amitié%s nouée%s, %d brouille%s.\n\n"),
				CountOf(EKind::Friendship), S(CountOf(EKind::Friendship)), S(CountOf(EKind::Friendship)),
				CountOf(EKind::Quarrel), S(CountOf(EKind::Quarrel)));
		}

		// Les habitants, famille par famille quand la simulation en a ; ceux qui n'en ont pas a la fin.
		TArray<TPair<FString, TArray<FString>>> Groups;
		{
			TSet<FString> Placed;
			for (const FFamilyView& View : FamilyViews)
			{
				TArray<FString> Members;
				for (const FString& Id : PersonOrder)
				{
					if (View.Members.Contains(Id)) Members.Add(Id);
				}
				// Un membre parti ou mort a quitte le foyer dans la simulation : on le garde dans le recit.
				for (const FString& Id : PersonOrder)
				{
					const FPersonState& State = People[Id];
					if (!Members.Contains(Id) && State.FamilyName == View.Name) Members.Add(Id);
				}
				Placed.Append(Members);
				Groups.Add(TPair<FString, TArray<FString>>(Capitalize(View.Name), Members));
			}
			TArray<FString> Others;
			for (const FString& Id : PersonOrder)
			{
				if (!Placed.Contains(Id)) Others.Add(Id);
			}
			if (Others.Num()) Groups.Add(TPair<FString, TArray<FString>>(FamilyViews.Num() ? FString(TEXT("Hors des familles")) : FString(), Others));
		}
		Out += FamilyViews.Num() ? TEXT("LES FAMILLES\n") : TEXT("LES HABITANTS\n");
		for (const TPair<FString, TArray<FString>>& Group : Groups)
		{
			if (!Group.Key.IsEmpty()) Out += FString::Printf(TEXT("%s\n"), *Group.Key);
			for (const FString& Id : Group.Value) Out += FString::Printf(TEXT("- %s\n"), *PersonLine(Id));
		}

		// Les lignes d'un jour dans l'ordre des heures : une scene racontee le soir passe apres le matin.
		TArray<const FEntry*> Sorted;
		for (const FEntry& Entry : Entries) Sorted.Add(&Entry);
		Algo::StableSort(Sorted, [](const FEntry* A, const FEntry* B) { return A->Day != B->Day ? A->Day < B->Day : A->Hour < B->Hour; });

		auto IsQuiet = [this](int32 Day)
		{
			const bool bClosed = Days.ContainsByPredicate([Day](const FDaySummary& D) { return D.Day == Day; });
			return bClosed && !Entries.ContainsByPredicate([Day](const FEntry& E) { return E.Day == Day && E.bDayLog; });
		};
		for (int32 Day = FirstDay; Day <= LastDay; ++Day)
		{
			// Des jours calmes d'affilee se lisent en un bloc : seul le dernier bilan compte.
			int32 Until = Day;
			while (IsQuiet(Day) && Until < LastDay && IsQuiet(Until + 1)) ++Until;
			const bool bClosed = Days.ContainsByPredicate([Until](const FDaySummary& D) { return D.Day == Until; });
			if (Until > Day)
			{
				Out += FString::Printf(TEXT("\nJOURS %d À %d\n  Rien de notable.\n"), Day, Until);
				Day = Until;
			}
			else
			{
				Out += FString::Printf(TEXT("\nJOUR %d%s\n"), Day, bClosed ? TEXT("") : TEXT(" (en cours)"));
				bool bAny = false;
				int32 LastHourLabel = -1;
				for (const FEntry* Entry : Sorted)
				{
					if (Entry->Day != Day || !Entry->bDayLog) continue;
					bAny = true;
					// Une scene se lit d'un bloc : son heure n'est dite qu'une fois.
					if (Entry->Kind == EKind::Scene)
					{
						if (LastHourLabel != Entry->Hour) Out += FString::Printf(TEXT("  %s, au feu.\n"), *Capitalize(HourLabel(Entry->Hour)));
						LastHourLabel = Entry->Hour;
						Out += FString::Printf(TEXT("    %s\n"), *Entry->Text);
						continue;
					}
					LastHourLabel = -1;
					Out += FString::Printf(TEXT("  %s. %s\n"), *Capitalize(HourLabel(Entry->Hour)), *Entry->Text);
				}
				if (!bAny) Out += TEXT("  Rien de notable.\n");
			}
			for (const FDaySummary& Summary : Days)
			{
				if (Summary.Day != Day) continue;
				FString Granary = TEXT("pas de grenier");
				if (Summary.bHasGranary)
				{
					Granary = Summary.Food > 0 ? FString::Printf(TEXT("grenier : %d portion%s"), Summary.Food, S(Summary.Food))
						: Summary.EmptyDays > 1 ? FString::Printf(TEXT("grenier vide depuis %d jours"), Summary.EmptyDays)
						: FString(TEXT("grenier vide"));
				}
				Out += FString::Printf(TEXT("  Bilan du soir : %d habitant%s ; %d maison%s debout ; %d chantier%s ; %s ; %d sans toit"),
					Summary.Inhabitants, S(Summary.Inhabitants), Summary.Houses, S(Summary.Houses),
					Summary.Sites, S(Summary.Sites), *Granary, Summary.Homeless);
				if (Summary.Deaths) Out += FString::Printf(TEXT(" ; %d mort%s"), Summary.Deaths, Summary.Deaths > 1 ? TEXT("s") : TEXT(""));
				Out += TEXT(".\n");
			}
		}

		Out += TEXT("\nCE QU'A VÉCU CHACUN\n");
		for (const TPair<FString, TArray<FString>>& Group : Groups)
		{
			if (!Group.Key.IsEmpty()) Out += FString::Printf(TEXT("\n%s\n"), *Group.Key.ToUpper());
			for (const FString& Id : Group.Value)
			{
				const FPersonState& State = People[Id];
				Out += FString::Printf(TEXT("\n%s\n"), *State.Name);
				for (const FEntry* Entry : Sorted)
				{
					// Le paragraphe de fondation et la presentation d'une famille nomment plusieurs personnes : chacun a sa ligne.
					if (!Entry->People.Contains(Id) || (Entry->Kind == EKind::Founding && Entry->People.Num() > 1)) continue;
					Out += FString::Printf(TEXT("  Jour %d, %s : %s\n"), Entry->Day, *HourLabel(Entry->Hour), *Entry->Text);
				}
			}
		}
		return Out;
	}

	FString FVillageChronicle::PersonLine(const FString& Id) const
	{
		const FPersonState& State = People[Id];
		FString Name = State.Look.Byname.IsEmpty() ? State.Name : FString::Printf(TEXT("%s %s"), *State.Name, *State.Look.Byname);
		if (State.Look.Age > 0.0) Name += FString::Printf(TEXT(" (%d ans)"), FMath::RoundToInt32(State.Look.Age));
		TArray<FString> Parts;
		Parts.Add(State.Look.bChild ? FString(State.Look.bFemale ? TEXT("enfant, fille") : TEXT("enfant, garçon")) : JobLabel(State.JobId, State.Look.bFemale));
		if (State.Look.bElder) Parts.Add(State.Look.bFemale ? TEXT("ancienne") : TEXT("ancien"));
		if (State.bAlive && !State.bGone)
		{
			Parts.Add(State.HomeId.IsEmpty() ? FString(TEXT("sans maison à soi")) : FString::Printf(TEXT("vit dans %s"), *BuildingLabel(State.HomeId)));
		}
		if (State.FirstDay > FirstDay) Parts.Add(FString::Printf(TEXT("arrivé%s le jour %d"), E(State.Look.bFemale), State.FirstDay));
		if (!State.bAlive)
		{
			Parts.Add(FString::Printf(TEXT("mort%s le jour %d%s%s"), E(State.Look.bFemale), State.DeathDay,
				State.DeathCause.IsEmpty() ? TEXT("") : TEXT(", "), *State.DeathCause));
		}
		if (State.bGone) Parts.Add(TEXT("parti"));
		if (State.Friends.Num())
		{
			TArray<FString> Friends;
			for (const FString& Other : PersonOrder)
			{
				if (State.Friends.Contains(Other)) Friends.Add(Other);
			}
			Parts.Add(FString::Printf(TEXT("ami%s %s"), E(State.Look.bFemale), *DeName(Names(Friends))));
		}
		return FString::Printf(TEXT("%s : %s."), *Name, *FString::Join(Parts, TEXT(" ; ")));
	}

	FString FVillageChronicle::StatusJson() const
	{
		int32 Alive = 0;
		for (const FString& Id : PersonOrder)
		{
			const FPersonState& State = People[Id];
			if (State.bAlive && !State.bGone) ++Alive;
		}
		FString Kinds;
		for (uint8 Raw = 0; Raw <= static_cast<uint8>(EKind::Stalled); ++Raw)
		{
			const EKind Kind = static_cast<EKind>(Raw);
			if (!Kinds.IsEmpty()) Kinds += TEXT(",");
			Kinds += FString::Printf(TEXT("\"%s\":%d"), KindName(Kind), CountOf(Kind));
		}
		return FString::Printf(
			TEXT("{\"started\":%s,\"seed\":%u,\"first_day\":%d,\"last_closed_day\":%d,\"days_closed\":%d,\"entries\":%d,\"people\":%d,\"alive\":%d,\"kinds\":{%s}}"),
			bStarted ? TEXT("true") : TEXT("false"), Seed, FirstDay, GetLastClosedDay(), Days.Num(), Entries.Num(),
			PersonOrder.Num(), Alive, *Kinds);
	}
}
