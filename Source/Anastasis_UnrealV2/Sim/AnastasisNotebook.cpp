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
				const FString Origin = Event.OriginalSourceId.IsEmpty() || Event.OriginalSourceId == Event.SourceId ? FString() : NameOf(Event.OriginalSourceId);
				Note.Text = Capitalize(Lines.TellerVersion(Event, Origin));
			}
		}
		// ecart n°49 : le conseil du soir se tient au feu, devant tout le village ; le joueur l'entend s'il est la.
		const TArray<AnastasisVillage::FVillage::FCouncil>& Councils = Village.GetCouncilLog();
		for (; CouncilSeen < Councils.Num(); ++CouncilSeen)
		{
			if (!Me) continue;
			const AnastasisVillage::FVillage::FCouncil& Council = Councils[CouncilSeen];
			for (const AnastasisVillage::FVillage::FWelcomeVote& Vote : Council.Votes)
			{
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

		// Une page par habitant, dans l'ordre ou on l'a entendu pour la premiere fois.
		TArray<FString> Tellers;
		for (const FNote& Note : Notes)
		{
			if (!Tellers.Contains(Note.TellerId)) Tellers.Add(Note.TellerId);
		}
		Out += TEXT("\nCE QUE CHACUN M'A DIT\n");
		for (const FString& TellerId : Tellers)
		{
			const FNote* First = Notes.FindByPredicate([&TellerId](const FNote& N) { return N.TellerId == TellerId; });
			Out += FString::Printf(TEXT("\n%s\n"), First ? *First->TellerName : *TellerId);
			for (const FNote& Note : Notes)
			{
				if (Note.TellerId != TellerId) continue;
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
			// Une voix au conseil n'est pas une histoire qui court.
			if (Note.Kind != TEXT("conseil") && !Roots.Contains(Note.RootId)) Roots.Add(Note.RootId);
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
		for (const FNote& Note : Notes)
		{
			Roots.Add(Note.RootId);
			Tellers.Add(Note.TellerId);
			if (Note.bLegend) ++Legends;
			if (Note.bToMe) ++ToMe;
		}
		return FString::Printf(TEXT("{\"player\":\"%s\",\"notes\":%d,\"stories\":%d,\"tellers\":%d,\"legends\":%d,\"to_me\":%d}"),
			*PlayerId, Notes.Num(), Roots.Num(), Tellers.Num(), Legends, ToMe);
	}
}
