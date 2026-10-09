#pragma once

// memoire-decisions-001 -- le carnet du joueur : ce qu'il a entendu, de qui, et comment l'histoire a change.
//
// Decision d'Alexandre (2026-10-08) : « un carnet qui se remplit » -- le joueur ne sait que ce qu'il a entendu
// lui-meme -- et « elle change, on peut comparer ». La simulation fait circuler les souvenirs de bouche en
// bouche (ai/episodes.js, ecart n°47) ; le carnet retient chaque recit fait AU joueur, ou pres de lui (a portee
// de voix du conteur et de celui qui ecoute). Il range ce qu'il a entendu par habitant (une page chacun), puis
// histoire par histoire, version apres version.
//
// Comme la chronique, il ne fait que lire la simulation. Le pendant de la reference est `witnessMemory.js`
// (`noteWitnessOverheard`), le temoin qu'est le joueur ; ce carnet n'en reprend que l'idee.

#include "CoreMinimal.h"

class FAnastasisSimulation;

namespace AnastasisDialogue
{
	class FLibrary;
}

namespace AnastasisNotebook
{
	/** Une chose entendue. */
	struct FNote
	{
		int32 Day = 0;
		int32 Hour = 0;
		FString TellerId;
		FString TellerName;
		FString ListenerName;
		FString RootId;
		FString Kind;
		int32 Hops = 0;
		bool bLegend = false;
		/** Vrai si c'est au joueur qu'on l'a racontee ; faux s'il l'a seulement entendue de pres. */
		bool bToMe = false;
		/** ecart n°50 : une histoire sur le joueur lui-meme, entendue dans son dos. */
		bool bAboutMe = false;
		FString Text;
	};

	class ANASTASIS_UNREALV2_API FPlayerNotebook
	{
	public:
		/** A portee de voix, en cases (20 m) : le conteur et celui qui ecoute. */
		static constexpr double HearRadius = 6.0;

		void Reset();

		/**
		 * Lit la simulation. Sans joueur incarne, ne note rien (mais retient ce qui existe deja : le joueur qui
		 * arrive n'apprend pas d'un coup tout ce qui s'est dit avant lui).
		 */
		void Observe(const FAnastasisSimulation& Sim, const AnastasisDialogue::FLibrary& Lines, TFunctionRef<FString(const FString&)> NameOf);

		/** Le carnet en francais : une page par habitant, puis chaque histoire, version par version. */
		FString Render() const;
		FString StatusJson() const;
		const TArray<FNote>& GetNotes() const { return Notes; }

	private:
		TMap<FString, TSet<FString>> Seen;
		TArray<FNote> Notes;
		FString PlayerId;
		/** ecart n°49 : les conseils deja lus ; le joueur entend ceux auxquels il assiste (vivant, au village). */
		int32 CouncilSeen = 0;
		/** ecart n°50 : les reponses d'aide ou le joueur est en jeu, deja notees comme ses actes. */
		int32 HelpSeen = 0;
	};
}
