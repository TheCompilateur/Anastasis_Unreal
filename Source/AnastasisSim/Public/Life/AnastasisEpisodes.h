#pragma once

// MEMOIRE EPISODIQUE ET FOLKLORE -- port de src/ai/episodes.js (memoire-decisions-001, ecart n°47).
//
// « memory.js repond a "ou est le bois". Ce module repond a "qu'est-ce qui m'est arrive". Un habitant ne
// retient QUE ce qu'il a vecu, vu de ses yeux, ou entendu d'un autre. » Une rancune a une cause nommee ;
// un fait colporte s'affaiblit toujours, le temperament du conteur dose la perte et enfle le chiffre ; au
// bout de trois bouches, plus personne ne l'a vecu : c'est une legende.
//
// Porte ici, fidelement : KINDS (tone, weight, spread), EPISODE, createChronicle, recordEpisode (la
// structure ; l'enregistrement vit dans FVillage), trimChronicle / standing, alreadyKnowsRoot,
// storytellerBias, retell, episodeFeeling, KIND_GOAL_BIAS / episodeGoalBias, reasonFor (le plus fort
// souvenir sur quelqu'un) et le choix de variante de texte (variantIndex, FNV-1a). Le texte lui-meme
// (EPISODE_LINES) est dans la base de repliques de l'hote.
//
// Non porte (ecart n°47) : socialAppraisal / noteSocialFromEpisode (croyances sur les personnes),
// cultureEpisodeSpread (vaut 1 : pas de culture), placeId / provenance / parentEventId, repairChronicleReferences
// (aucune sauvegarde ancienne a reparer). EXTENSION : trois souvenirs du premier soir au feu (`fall`,
// `carried`, `leftBehind`) et deux de l'aide (`helped`, `refusedHelp`), qui n'existent pas dans la reference.

#include "CoreMinimal.h"

namespace AnastasisEpisodes
{
	/** `EPISODE` d'episodes.js. */
	namespace Constants
	{
		inline constexpr int32 Capacity = 8;
		inline constexpr int32 ForgetAfterDays = 30;
		inline constexpr double FadePerDay = 0.7;
		inline constexpr double MinWeight = 4.0;
		inline constexpr int32 TellPerMeeting = 1;
		inline constexpr double TellMinWeight = 12.0;
		inline constexpr int32 LegendHops = 3;
		inline constexpr int32 MaxHops = 6;
		inline constexpr double RetellKeepMax = 0.92;
		inline constexpr double RetellKeepMin = 0.5;
		inline constexpr double FirsthandEdge = 6.0;
		inline constexpr double FeelingScale = 0.35;
		inline constexpr double FeelingCap = 26.0;
		/** `HEARSAY_EPISODE_SCALE` : un on-dit pese moins qu'un vecu. */
		inline constexpr double HearsayScale = 0.45;
		/** `EPISODE_GOAL`. */
		inline constexpr double GoalScale = 0.2;
		inline constexpr double GoalCap = 22.0;
	}

	/** Une ligne de `KINDS`. */
	struct FKindModel
	{
		double Tone = 0.0;
		double Weight = 0.0;
		double Spread = 0.6;
	};

	/** Le modele d'un type de souvenir ; nul si le type est inconnu (la reference rend `null`). */
	ANASTASISSIM_API const FKindModel* KindModel(const FString& Kind);

	/** Un souvenir, tel que `recordEpisode` puis `createGossipEpisode` le batissent. */
	struct FEpisode
	{
		FString Id;
		FString RootId;
		FString Kind;
		int32 Day = 0;
		double Tone = 0.0;
		double Weight = 0.0;
		FString AboutId;
		FString AboutName;
		int32 X = 0;
		int32 Y = 0;
		/** Le chiffre de l'histoire (or vole, ration donnee) ; 0 sans chiffre. */
		double Detail = 0.0;
		FString Note;
		/** 0 = il y etait. */
		int32 Hops = 0;
		bool bFirsthand = true;
		/** Ce qu'ajoute un on-dit (`createGossipEpisode`) : qui l'a raconte, d'ou vient l'histoire. */
		FString SourceId;
		FString SourceEpisodeId;
		FString OriginalSourceId;
		/** < 0 : non pose (un souvenir vecu n'a pas de confiance). */
		double Confidence = -1.0;
	};

	/** `createChronicle` : les souvenirs, du plus fort au plus faible apres tri. */
	struct FChronicle
	{
		TArray<FEpisode> Events;
		int32 Lived = 0;
		int32 Told = 0;
		int32 Heard = 0;
		int32 NextId = 1;
	};

	/** `standing(event)` : le poids, et l'avance du vecu. */
	ANASTASISSIM_API double Standing(const FEpisode& Event);

	/** `trimChronicle` : au-dela de la capacite, on garde ce qui pese (tri stable, comme Array.sort de V8). */
	ANASTASISSIM_API void Trim(FChronicle& Chronicle);

	/** `alreadyKnowsRoot`. */
	ANASTASISSIM_API bool KnowsRoot(const FChronicle& Chronicle, const FString& RootId);

	/** `storytellerBias(npc)` : peur et vantardise du conteur. */
	struct FBias
	{
		double Fear = 1.0;
		double Boast = 1.0;
	};
	ANASTASISSIM_API FBias StorytellerBias(double TraitExplore, double Morale, double Reputation);

	/**
	 * `retell(sim, event, bias)` : le meme fait, passe par une bouche de plus. `NextRandom` est `sim.rng`,
	 * tire seulement a partir de la deuxieme bouche (le nom qui se perd, puis le lieu qui derive), dans l'ordre.
	 */
	ANASTASISSIM_API FEpisode Retell(const FEpisode& Event, const FBias& Bias, TFunctionRef<double()> NextRandom);

	/** `episodeFeeling(npc, otherId)` : le ressenti envers quelqu'un, tire du seul vecu (et des on-dit, moins). */
	ANASTASISSIM_API double Feeling(const FChronicle& Chronicle, const FString& OtherId);

	/** `episodeGoalBias(npc, goal)` : ce qu'on a vecu tire les buts du jour, borne. */
	ANASTASISSIM_API double GoalBias(const FChronicle& Chronicle, const FString& Goal);

	/** `reasonFor(npc, otherId)` : le souvenir le plus fort sur quelqu'un ; nul s'il n'y en a pas. */
	ANASTASISSIM_API const FEpisode* StrongestAbout(const FChronicle& Chronicle, const FString& OtherId);

	/** `variantIndex(event, count)` : FNV-1a 32 de l'identifiant, la meme variante de texte pour le meme souvenir. */
	ANASTASISSIM_API int32 VariantIndex(const FEpisode& Event, int32 Count);

	/** Vrai si l'histoire est devenue une legende (`hops >= legendHops`). */
	inline bool IsLegend(const FEpisode& Event) { return Event.Hops >= Constants::LegendHops; }
}
