// arrivants-001 -- les arrivants et le conseil du soir.
//
// EXTENSION — ecart n°49. La reference n'a ni arrivees venues d'une cause (`maybeImmigrate` n'est pas porte), ni
// conseil de village : son seul accueil est celui d'un foyer qui prend un hote (`tryHostGuest`, et la decision du
// joueur de `kosmos1204UneBouchePlus.js`). Ici, dans un village de familles (ecart n°44), un groupe venu du monde
// exterieur (ecart n°38) devient une famille en attente ; le soir suivant, chaque chef de famille dit oui ou non
// (Alexandre, 2026-10-08 : « les chefs de famille », « ils repartent », « du monde exterieur »), et le village s'en
// souvient (`hosting`, `hostingRefusal`, `departure`, les genres de souvenir de la reference).

#include "Village/AnastasisVillage.h"

#include "Life/AnastasisEpisodes.h"

namespace AnastasisVillage
{
	namespace
	{
		/** Les souvenirs d'un exil vecu : on sait ce que c'est que d'arriver sans rien. */
		bool IsExileKind(const FString& Kind)
		{
			return Kind == TEXT("fall") || Kind == TEXT("leftBehind") || Kind == TEXT("carried") || Kind == TEXT("fled");
		}
	}

	TArray<FString> FVillage::AdmitExternalArrivals(int32 Count, double Radius, double StartAngle, const FString& Cause, const FString& Origin, int32 OnDay)
	{
		const TArray<FString> Ids = AdmitExternalArrivals(Count, Radius, StartAngle);
		// Sans foyer (le harnais, la partie d'avant) : des habitants ordinaires, comme avant, au bit pres.
		if (Families.IsEmpty() || Ids.IsEmpty()) return Ids;

		FArrivalGroup Group;
		if (ArrivalPool.IsValidIndex(NextArrivalGroup))
		{
			Group = ArrivalPool[NextArrivalGroup];
		}
		++NextArrivalGroup;
		if (Group.FamilyName.IsEmpty())
		{
			Group.FamilyName = Origin.IsEmpty() ? FString(TEXT("des gens de la route")) : FString::Printf(TEXT("des gens de %s"), *Origin);
		}
		const FString FamilyId = AddFamily(Group.FamilyName);
		{
			FFamily& Family = *Families.FindByPredicate([&FamilyId](const FFamily& F) { return F.Id == FamilyId; });
			Family.bGuest = true;
			// Le jour de la simulation (celui du minuit qui les admet), pas l'horloge des habitants qui n'a pas encore tourne.
			Family.ArrivedDay = OnDay;
			Family.Cause = Cause;
			Family.Origin = Origin;
		}
		for (int32 I = 0; I < Ids.Num(); ++I)
		{
			FArrivalMember Member;
			if (Group.Members.IsValidIndex(I))
			{
				Member = Group.Members[I];
			}
			else
			{
				Member.Name = FString::Printf(TEXT("l'inconnu %d"), I + 1);
				Member.Gender = TEXT("male");
				Member.Age = 30.0;
				Member.KinRole = I == 0 ? TEXT("chef") : TEXT("compagnon");
			}
			if (I == 0)
			{
				// Le premier parle pour les siens : un groupe a toujours un chef, et c'est un adulte.
				Member.KinRole = TEXT("chef");
				Member.bAdult = true;
			}
			SetIdentity(Ids[I], Member.Name, Group.FamilyName, Member.Gender, Member.Age);
			JoinFamily(Ids[I], FamilyId, Member.bAdult, Member.KinRole);
		}
		// Chaque adulte se souvient de ce qu'il a fui : c'est ce qu'il racontera, et ce qui courra apres lui.
		const FFamily* Family = FindFamily(FamilyId);
		for (const FString& Id : Family->Adults)
		{
			FEpisodeOptions Fled;
			Fled.Note = Cause;
			Fled.Detail = Ids.Num();
			Fled.RootId = FString::Printf(TEXT("fui-%s"), *FamilyId);
			RecordEpisode(Id, TEXT("fled"), Fled);
		}
		return Ids;
	}

	double FVillage::PortionsPerMouth(int32 ExtraMouths) const
	{
		int64 Portions = 0;
		for (const FBuilding& Building : Buildings.GetItems())
		{
			if (Building.Type == GranaryType && Building.Progress >= 1.0) Portions += Building.FoodPhysical;
		}
		const int32 Mouths = Actors.GetItems().Num() + FMath::Max(0, ExtraMouths);
		return Mouths > 0 ? static_cast<double>(Portions) / static_cast<double>(Mouths) : static_cast<double>(Portions);
	}

	FVillage::FWelcomeVote FVillage::EvaluateWelcome(const FNpc& Chief, const FFamily& Guests, double Insecurity) const
	{
		namespace E = AnastasisEpisodes;
		TArray<FString> GuestIds = Guests.Adults;
		GuestIds.Append(Guests.Dependents);
		// Bible §29 : « une somme ponderee dont la raison dominante est toujours retournee ».
		TArray<TPair<FString, double>> Terms;
		// Ce qu'il a vecu, ce qu'il a fait la derniere fois, ce qu'il a entendu d'eux.
		bool bExile = false;
		double Remorse = 0.0;
		bool bHeardThem = false;
		for (const E::FEpisode& Event : Chief.Chronicle.Events)
		{
			if (Event.bFirsthand && IsExileKind(Event.Kind)) bExile = true;
			if (Event.Kind == TEXT("hostingRefusal")) Remorse += Event.bFirsthand ? 25.0 : 8.0;
			if (!Event.bFirsthand && (GuestIds.Contains(Event.OriginalSourceId) || GuestIds.Contains(Event.SourceId))) bHeardThem = true;
		}
		Terms.Emplace(TEXT("nous_aussi"), bExile ? 25.0 : 0.0);
		Terms.Emplace(TEXT("remords"), FMath::Min(Remorse, 40.0));
		Terms.Emplace(TEXT("leur_histoire"), bHeardThem ? 12.0 : 0.0);
		Terms.Emplace(TEXT("bras"), Guests.Adults.Num() >= 2 ? 8.0 : 0.0);
		// Chacun parle de chez lui : sa famille, ses enfants, son toit.
		const FFamily* Own = Chief.FamilyId.IsEmpty() ? nullptr : FindFamily(Chief.FamilyId);
		const bool bChildren = Own && !Own->Dependents.IsEmpty();
		const bool bOwnRoofless = Own && Own->HomeId.IsEmpty() && Chief.HomeId.IsEmpty();
		// Le grenier, nouveaux venus compris ; qui a des enfants a table le compte plus serre.
		const double Portions = PortionsPerMouth(GuestIds.Num());
		Terms.Emplace(TEXT("grenier_plein"), Portions >= 15.0 ? 12.0 : 0.0);
		const double Hunger = Portions < 4.0 ? -45.0 : (Portions < 8.0 ? -20.0 : 0.0);
		Terms.Emplace(TEXT("grenier"), bChildren ? Hunger * 1.5 : Hunger);
		// Ce qui se passe dehors : on ne sait pas qui ils sont.
		Terms.Emplace(TEXT("peur"), Insecurity > 0.05 ? -50.0 * FMath::Min(Insecurity, 1.0) : 0.0);
		Terms.Emplace(TEXT("nombre"), -6.0 * FMath::Max(0, GuestIds.Num() - 3));
		// Des familles du village dorment encore sans toit a elles ; la sienne d'abord.
		int32 OthersRoofless = 0;
		for (const FFamily& Family : Families)
		{
			if (Family.bGuest || Family.bLeft || !Family.HomeId.IsEmpty() || Family.Adults.IsEmpty() || (Own && Family.Id == Own->Id)) continue;
			++OthersRoofless;
		}
		Terms.Emplace(TEXT("toits"), (bOwnRoofless ? -20.0 : 0.0) - 5.0 * OthersRoofless);
		Terms.Emplace(TEXT("inconnu"), -10.0);

		FWelcomeVote Vote;
		Vote.VoterId = Chief.Id;
		for (const TPair<FString, double>& Term : Terms)
		{
			Vote.Score += Term.Value;
			if (Term.Value != 0.0) Vote.Terms += FString::Printf(TEXT("%s%s=%.0f"), Vote.Terms.IsEmpty() ? TEXT("") : TEXT(" "), *Term.Key, Term.Value);
		}
		Vote.bYes = Vote.Score > 0.0;
		double Strongest = 0.0;
		for (const TPair<FString, double>& Term : Terms)
		{
			const bool bSameSide = Vote.bYes ? Term.Value > 0.0 : Term.Value < 0.0;
			if (bSameSide && FMath::Abs(Term.Value) > Strongest)
			{
				Strongest = FMath::Abs(Term.Value);
				Vote.Reason = Term.Key;
			}
		}
		return Vote;
	}

	void FVillage::UpdateArrivalCouncilDaily(double Insecurity, int32 Today)
	{
		if (Families.IsEmpty()) return;
		// Les familles qui votent : accueillies, encore la, avec un chef. Copiees d'abord : le conseil en change la liste.
		TArray<FString> Pending;
		for (const FFamily& Family : Families)
		{
			if (Family.bGuest && !Family.bLeft && Family.ArrivedDay < Today && !Family.Adults.IsEmpty()) Pending.Add(Family.Id);
		}
		for (const FString& GuestFamilyId : Pending)
		{
			const FFamily* Guests = FindFamily(GuestFamilyId);
			if (!Guests) continue;
			FCouncil Council;
			Council.Day = Today;
			Council.FamilyId = GuestFamilyId;
			Council.Cause = Guests->Cause;
			for (const FFamily& Family : Families)
			{
				if (Family.bGuest || Family.bLeft) continue;
				// Le chef parle pour les siens ; sans chef vivant, le premier des adultes qui restent.
				const FNpc* Chief = nullptr;
				for (const FString& Id : Family.Adults)
				{
					const FNpc* Member = Actors.FindById(Id);
					if (!Member) continue;
					if (Member->KinRole == TEXT("chef")) { Chief = Member; break; }
					if (!Chief) Chief = Member;
				}
				// ecart n°50 : le joueur ne vote que par sa main (CastPlayerVote), meme chef d'un foyer.
				if (!Chief || Chief->Id == PlayerPersonId) continue;
				const FWelcomeVote Vote = EvaluateWelcome(*Chief, *Guests, Insecurity);
				Council.Votes.Add(Vote);
			}
			// ecart n°50 : la voix du joueur, s'il l'a donnee pour ce groupe ; une demi-voix tant qu'il n'est pas eprouve.
			const FNpc* Player = PlayerPersonId.IsEmpty() ? nullptr : Actors.FindById(PlayerPersonId);
			if (Player && PlayerVoteFamilyId == GuestFamilyId)
			{
				FWelcomeVote Voice;
				Voice.VoterId = PlayerPersonId;
				Voice.bYes = bPlayerVoteYes;
				Voice.Reason = TEXT("joueur");
				Voice.Weight = IsUnproven(*Player) ? 0.5 : 1.0;
				Voice.Score = bPlayerVoteYes ? 1.0 : -1.0;
				Council.Votes.Add(Voice);
				// Ceux qui sont au feu l'ont entendu : sa voix devient une histoire, a lui.
				for (const FWelcomeVote& Heard : Council.Votes)
				{
					if (Heard.VoterId == PlayerPersonId) continue;
					FEpisodeOptions Said;
					Said.AboutId = PlayerPersonId;
					Said.Note = Guests->Name;
					Said.RootId = FString::Printf(TEXT("voix-%s-%s"), *GuestFamilyId, *PlayerPersonId);
					RecordEpisode(Heard.VoterId, bPlayerVoteYes ? TEXT("votedYes") : TEXT("votedNo"), Said);
				}
			}
			if (PlayerVoteFamilyId == GuestFamilyId) PlayerVoteFamilyId.Reset();
			// La majorite stricte des voix pesees : a egalite, on garde le grain.
			double YesWeight = 0.0;
			double AllWeight = 0.0;
			for (const FWelcomeVote& Vote : Council.Votes)
			{
				AllWeight += Vote.Weight;
				if (Vote.bYes) YesWeight += Vote.Weight;
			}
			Council.bAccepted = AllWeight > 0.0 && YesWeight * 2.0 > AllWeight;
			const FString GuestChief = Guests->Adults[0];
			if (Council.bAccepted)
			{
				FFamily& Accepted = *Families.FindByPredicate([&GuestFamilyId](const FFamily& F) { return F.Id == GuestFamilyId; });
				Accepted.bGuest = false;
				for (const FWelcomeVote& Vote : Council.Votes)
				{
					if (!Vote.bYes) continue;
					FEpisodeOptions Hosted;
					Hosted.AboutId = GuestChief;
					Hosted.Note = Council.Cause;
					Hosted.RootId = FString::Printf(TEXT("accueil-%s"), *GuestFamilyId);
					RecordEpisode(Vote.VoterId, TEXT("hosting"), Hosted);
				}
				// Le soir de l'accueil, on raconte le village aux nouveaux : chacun de ceux qui ont dit oui dit a l'un
				// d'eux ce que le village se raconte -- l'histoire entendue qui a deja le plus voyage, puis la plus lourde --
				// et qu'il ne sait pas encore. Elle passe ainsi de groupe en groupe : c'est la que naissent les legendes.
				const TArray<FString> Newcomers = Accepted.Adults;
				// Ceux qui ont dit oui, puis les arrivants des vagues d'avant : les derniers venus accueillent les suivants.
				TArray<FString> Tellers;
				for (const FWelcomeVote& Vote : Council.Votes)
				{
					if (Vote.bYes) Tellers.Add(Vote.VoterId);
				}
				for (const FFamily& Earlier : Families)
				{
					if (Earlier.ArrivedDay <= 0 || Earlier.bGuest || Earlier.bLeft || Earlier.Id == GuestFamilyId) continue;
					for (const FString& Id : Earlier.Adults) Tellers.AddUnique(Id);
				}
				int32 Turn = 0;
				for (const FString& TellerId : Tellers)
				{
					if (Newcomers.IsEmpty()) break;
					const FNpc* Teller = Actors.FindById(TellerId);
					const FString& ListenerId = Newcomers[Turn++ % Newcomers.Num()];
					const FNpc* Listener = Actors.FindById(ListenerId);
					if (!Teller || !Listener) continue;
					const AnastasisEpisodes::FEpisode* Heaviest = nullptr;
					for (const AnastasisEpisodes::FEpisode& Event : Teller->Chronicle.Events)
					{
						if (Event.Hops >= AnastasisEpisodes::Constants::MaxHops || Event.AboutId == ListenerId) continue;
						if (AnastasisEpisodes::KnowsRoot(Listener->Chronicle, Event.RootId.IsEmpty() ? Event.Id : Event.RootId)) continue;
						const bool bBetter = !Heaviest || Event.Hops > Heaviest->Hops || (Event.Hops == Heaviest->Hops && Event.Weight > Heaviest->Weight);
						if (bBetter) Heaviest = &Event;
					}
					if (Heaviest) TellEpisode(TellerId, ListenerId, Heaviest->Id);
				}
			}
			else
			{
				for (const FWelcomeVote& Vote : Council.Votes)
				{
					if (Vote.bYes) continue;
					FEpisodeOptions Refused;
					Refused.AboutId = GuestChief;
					Refused.Note = Council.Cause;
					Refused.RootId = FString::Printf(TEXT("refus-accueil-%s"), *GuestFamilyId);
					RecordEpisode(Vote.VoterId, TEXT("hostingRefusal"), Refused);
				}
				// Ils repartent au matin : ceux qui les voient partir s'en souviennent.
				TArray<FString> Leaving = Guests->Adults;
				Leaving.Append(Guests->Dependents);
				for (const FString& Id : Leaving)
				{
					FEpisodeOptions Gone;
					Gone.AboutId = Id;
					Gone.Note = Council.Cause;
					Gone.RootId = FString::Printf(TEXT("depart-%s"), *Id);
					RecordWitnesses(Id, TEXT("departure"), Gone, 7.0);
				}
				for (const FString& Id : Leaving) RemoveNpc(Id);
				if (FFamily* Left = Families.FindByPredicate([&GuestFamilyId](const FFamily& F) { return F.Id == GuestFamilyId; }))
				{
					Left->bLeft = true;
					Left->bGuest = false;
				}
			}
			CouncilLog.Add(MoveTemp(Council));
		}
	}
}
