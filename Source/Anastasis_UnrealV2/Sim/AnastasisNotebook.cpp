#include "Sim/AnastasisNotebook.h"

#include "Sim/AnastasisArrivals.h"
#include "Sim/AnastasisDialogueLines.h"
#include "Sim/AnastasisSimulation.h"
#include "Sim/AnastasisVillageChronicle.h"
#include "Village/AnastasisVillage.h"

namespace AnastasisNotebook
{
	namespace
	{
		FString Capitalize(const FString& Text)
		{
			FString Out = Text;
			if (!Out.IsEmpty()) Out[0] = FChar::ToUpper(Out[0]);
			return Out;
		}

		bool Near(const AnastasisVillage::FNpc& A, const AnastasisVillage::FNpc& B, double Radius)
		{
			const double DX = A.X - B.X;
			const double DY = A.Y - B.Y;
			return DX * DX + DY * DY <= Radius * Radius;
		}
	}

	void FPlayerNotebook::Reset()
	{
		Seen.Reset();
		Notes.Reset();
		PlayerId.Reset();
		CouncilSeen = 0;
		HelpSeen = 0;
	}

	void FPlayerNotebook::Observe(const FAnastasisSimulation& Sim, const AnastasisDialogue::FLibrary& Lines, TFunctionRef<FString(const FString&)> NameOf)
	{
		if (!Sim.IsRunning()) return;
		const AnastasisVillage::FVillage& Village = Sim.GetVillage();
		const FString& Player = Village.GetPlayerPersonId();
		const AnastasisVillage::FNpc* Me = Player.IsEmpty() ? nullptr : Village.FindNpc(Player);
		if (Me) PlayerId = Player;
		const int32 Day = Sim.GetDay();
		const int32 Hour = FMath::Clamp(FMath::FloorToInt32(Sim.DayFrac() * 24.0), 0, 23);
		for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
		{
			TSet<FString>& Known = Seen.FindOrAdd(Npc.Id);
			// Un souvenir oublie puis entendu de nouveau est une chose de plus a noter.
			TSet<FString> Present;
			for (const AnastasisEpisodes::FEpisode& Event : Npc.Chronicle.Events) Present.Add(Event.Id);
			Known = Known.Intersect(Present);
			for (const AnastasisEpisodes::FEpisode& Event : Npc.Chronicle.Events)
			{
				if (Known.Contains(Event.Id)) continue;
				Known.Add(Event.Id);
				// On ne note que ce qui se raconte, pas ce que chacun vit dans son coin.
				if (!Me || Event.bFirsthand) continue;
				const bool bToMe = Npc.Id == Player;
				const AnastasisVillage::FNpc* Teller = Village.FindNpc(Event.SourceId);
				const bool bHeard = bToMe || (Near(*Me, Npc, HearRadius) && (!Teller || Near(*Me, *Teller, HearRadius)));
				if (!bHeard) continue;
				FNote& Note = Notes.AddDefaulted_GetRef();
				Note.Day = Day;
				Note.Hour = Hour;
				Note.TellerId = Event.SourceId;
				Note.TellerName = NameOf(Event.SourceId);
				Note.ListenerName = bToMe ? FString(TEXT("moi")) : NameOf(Npc.Id);
				Note.RootId = Event.RootId.IsEmpty() ? Event.Id : Event.RootId;
				Note.Kind = Event.Kind;
				Note.Hops = Event.Hops;
				Note.bLegend = AnastasisEpisodes::IsLegend(Event);
				Note.bToMe = bToMe;
				Note.bAboutMe = Event.AboutId == Player;
				const FString Origin = Event.OriginalSourceId.IsEmpty() || Event.OriginalSourceId == Event.SourceId ? FString() : NameOf(Event.OriginalSourceId);
				Note.Text = Capitalize(Lines.TellerVersion(Event, Origin));
			}
		}
		// ecart n°53 : le conseil du soir se tient au feu, devant tout le village ; le joueur l'entend s'il est la.
		const TArray<AnastasisVillage::FVillage::FCouncil>& Councils = Village.GetCouncilLog();
		for (; CouncilSeen < Councils.Num(); ++CouncilSeen)
		{
			if (!Me) continue;
			const AnastasisVillage::FVillage::FCouncil& Council = Councils[CouncilSeen];
			for (const AnastasisVillage::FVillage::FWelcomeVote& Vote : Council.Votes)
			{
				// ecart n°54 : sa propre voix est un acte, pas une chose entendue.
				if (Vote.VoterId == Player)
				{
					FNote& Deed = Notes.AddDefaulted_GetRef();
					Deed.Day = Day;
					Deed.Hour = Hour;
					Deed.TellerId = Player;
					Deed.TellerName = NameOf(Player);
					Deed.Kind = TEXT("acte");
					const AnastasisVillage::FVillage::FFamily* Group = Village.FindFamily(Council.FamilyId);
					Deed.Text = FString::Printf(TEXT("Au conseil, j'ai dit %s pour %s%s."), Vote.bYes ? TEXT("oui") : TEXT("non"),
						Group ? *Group->Name : TEXT("les nouveaux venus"), Vote.Weight < 1.0 ? TEXT(" (une demi-voix : on ne me connaît pas encore)") : TEXT(""));
					continue;
				}
				FNote& Note = Notes.AddDefaulted_GetRef();
				Note.Day = Day;
				Note.Hour = Hour;
				Note.TellerId = Vote.VoterId;
				Note.TellerName = NameOf(Vote.VoterId);
				Note.ListenerName = TEXT("au conseil");
				Note.RootId = FString::Printf(TEXT("conseil-%s"), *Council.FamilyId);
				Note.Kind = TEXT("conseil");
				const FString Said = AnastasisArrivals::VoteLine(Lines, Sim.GetSeed(), Vote, Council.Cause, CouncilSeen);
				Note.Text = FString::Printf(TEXT("%s.%s"), Vote.bYes ? TEXT("Oui") : TEXT("Non"), Said.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" %s"), *Said));
			}
		}
		// ecart n°54 : ce que le joueur a fait des demandes d'aide, et ce qu'on a fait des siennes.
		const TArray<AnastasisVillage::FVillage::FHelpAnswer>& Help = Village.GetHelpLog();
		for (; HelpSeen < Help.Num(); ++HelpSeen)
		{
			const AnastasisVillage::FVillage::FHelpAnswer& Answer = Help[HelpSeen];
			if (!Me || (Answer.ToId != Player && Answer.FromId != Player)) continue;
			FNote& Deed = Notes.AddDefaulted_GetRef();
			Deed.Day = Day;
			Deed.Hour = Hour;
			Deed.TellerId = Player;
			Deed.TellerName = NameOf(Player);
			Deed.Kind = TEXT("acte");
			if (Answer.ToId == Player)
			{
				Deed.Text = Answer.Reason == TEXT("silence")
					? FString::Printf(TEXT("%s m'a demandé de l'aide ; je n'ai pas répondu."), *NameOf(Answer.FromId))
					: FString::Printf(TEXT("%s m'a demandé de l'aide ; j'ai dit %s."), *NameOf(Answer.FromId), Answer.bAccepted ? TEXT("oui") : TEXT("non"));
			}
			else
			{
				// La raison, en mots : ce que l'autre m'a fait comprendre.
				static const TMap<FString, FString> Why = {
					{ TEXT("porte_fermee"), TEXT("j'ai fermé la porte à d'autres, au conseil") },
					{ TEXT("on_dit"), TEXT("on lui a parlé de mes refus") },
					{ TEXT("refus_rendu"), TEXT("je lui avais dit non") },
					{ TEXT("nouveau"), TEXT("je suis nouveau ici, je n'ai encore levé aucun toit") },
					{ TEXT("inconnu"), TEXT("il ne me connaît pas") },
					{ TEXT("dette"), TEXT("je lui dois déjà des journées") },
					{ TEXT("son_toit"), TEXT("son propre toit d'abord") },
					{ TEXT("occupe"), TEXT("son ouvrage d'abord") },
					{ TEXT("faible"), TEXT("il est trop faible") },
					{ TEXT("dette_rendue"), TEXT("il me devait ça") },
					{ TEXT("amitie"), TEXT("par amitié") },
					{ TEXT("voisin"), TEXT("entre voisins") },
				};
				const FString* Reason = Why.Find(Answer.Reason);
				Deed.Text = FString::Printf(TEXT("J'ai demandé de l'aide à %s : %s, %s."), *NameOf(Answer.ToId),
					Answer.bAccepted ? TEXT("oui") : TEXT("non"), Reason ? **Reason : *Answer.Reason);
			}
		}
	}

	FString FPlayerNotebook::Render() const
	{
		FString Out = TEXT("CARNET\n");
		if (PlayerId.IsEmpty())
		{
			Out += TEXT("Personne ne tient ce carnet : aucun joueur n'est incarné (Anastasis.Player.Arrive).\n");
			return Out;
		}
		if (Notes.IsEmpty())
		{
			Out += TEXT("Rien d'entendu pour l'instant.\n");
			return Out;
		}
		Out += FString::Printf(TEXT("%d chose%s entendue%s.\n"), Notes.Num(), Notes.Num() > 1 ? TEXT("s") : TEXT(""), Notes.Num() > 1 ? TEXT("s") : TEXT(""));

		// ecart n°54 : ce que j'ai fait, puis ce qu'on dit de moi quand on ne me voit pas.
		Out += TEXT("\nCE QUE J'AI FAIT\n");
		bool bAnyDeed = false;
		for (const FNote& Note : Notes)
		{
			if (Note.Kind != TEXT("acte")) continue;
			bAnyDeed = true;
			Out += FString::Printf(TEXT("  Jour %d, %s : %s\n"), Note.Day, *AnastasisChronicle::HourLabel(Note.Hour), *Note.Text);
		}
		if (!bAnyDeed) Out += TEXT("  Rien encore.\n");
		Out += TEXT("\nCE QU'ON DIT DE MOI\n");
		bool bAnyAboutMe = false;
		for (const FNote& Note : Notes)
		{
			if (!Note.bAboutMe) continue;
			bAnyAboutMe = true;
			Out += FString::Printf(TEXT("  Jour %d, %s, %s à %s : « %s »\n"), Note.Day, *AnastasisChronicle::HourLabel(Note.Hour),
				*Note.TellerName, *Note.ListenerName, *Note.Text);
		}
		if (!bAnyAboutMe) Out += TEXT("  Rien d'entendu.\n");

		// Une page par habitant, dans l'ordre ou on l'a entendu pour la premiere fois.
		TArray<FString> Tellers;
		for (const FNote& Note : Notes)
		{
			if (Note.Kind != TEXT("acte") && !Tellers.Contains(Note.TellerId)) Tellers.Add(Note.TellerId);
		}
		Out += TEXT("\nCE QUE CHACUN M'A DIT\n");
		for (const FString& TellerId : Tellers)
		{
			const FNote* First = Notes.FindByPredicate([&TellerId](const FNote& N) { return N.TellerId == TellerId; });
			Out += FString::Printf(TEXT("\n%s\n"), First ? *First->TellerName : *TellerId);
			for (const FNote& Note : Notes)
			{
				if (Note.TellerId != TellerId || Note.Kind == TEXT("acte")) continue;
				Out += FString::Printf(TEXT("  Jour %d, %s, %s : « %s »%s\n"), Note.Day,
					*AnastasisChronicle::HourLabel(Note.Hour),
					Note.bToMe ? TEXT("à moi") : (Note.Kind == TEXT("conseil") ? TEXT("au conseil") : *FString::Printf(TEXT("à %s"), *Note.ListenerName)),
					*Note.Text, Note.bLegend ? TEXT(" (une légende : plus personne ne l'a vécue)") : TEXT(""));
			}
		}

		// Chaque histoire, version apres version : ce qui a change entre deux bouches.
		TArray<FString> Roots;
		for (const FNote& Note : Notes)
		{
			// Une voix au conseil, ou mon propre acte, n'est pas une histoire qui court.
			if (Note.Kind != TEXT("conseil") && Note.Kind != TEXT("acte") && !Roots.Contains(Note.RootId)) Roots.Add(Note.RootId);
		}
		Out += TEXT("\nLES HISTOIRES, VERSION PAR VERSION\n");
		for (const FString& Root : Roots)
		{
			TArray<const FNote*> Versions;
			for (const FNote& Note : Notes)
			{
				if (Note.RootId == Root) Versions.Add(&Note);
			}
			Out += FString::Printf(TEXT("\nUne histoire entendue %d fois\n"), Versions.Num());
			for (int32 I = 0; I < Versions.Num(); ++I)
			{
				const FNote& Note = *Versions[I];
				const bool bChanged = I > 0 && Note.Text != Versions[I - 1]->Text;
				Out += FString::Printf(TEXT("  %d. Jour %d, %s (%d bouche%s) : « %s »%s\n"), I + 1, Note.Day, *Note.TellerName,
					Note.Hops, Note.Hops > 1 ? TEXT("s") : TEXT(""), *Note.Text,
					bChanged ? TEXT(" -- racontée autrement") : TEXT(""));
			}
		}
		return Out;
	}

	FString FPlayerNotebook::StatusJson() const
	{
		TSet<FString> Roots;
		TSet<FString> Tellers;
		int32 Legends = 0;
		int32 ToMe = 0;
		int32 AboutMe = 0;
		int32 Deeds = 0;
		for (const FNote& Note : Notes)
		{
			Roots.Add(Note.RootId);
			Tellers.Add(Note.TellerId);
			if (Note.bLegend) ++Legends;
			if (Note.bToMe) ++ToMe;
			if (Note.bAboutMe) ++AboutMe;
			if (Note.Kind == TEXT("acte")) ++Deeds;
		}
		return FString::Printf(TEXT("{\"player\":\"%s\",\"notes\":%d,\"stories\":%d,\"tellers\":%d,\"legends\":%d,\"to_me\":%d,\"about_me\":%d,\"deeds\":%d}"),
			*PlayerId, Notes.Num(), Roots.Num(), Tellers.Num(), Legends, ToMe, AboutMe, Deeds);
	}
}
