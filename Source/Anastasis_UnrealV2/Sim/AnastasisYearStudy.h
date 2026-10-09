// annee-valmire-001 -- une annee a Valmire, mesuree.
//
// Le vrai village du lancement (`SeedStartVillage`, comme le jeu), sans rendu, dans un monde de travail jete
// apres usage. La meme graine pour chaque scenario : seul change ce que fait le joueur (absent, il survit, il
// travaille) ou si le village est ferme au monde exterieur. Un releve tous les `Every` jours : economie (habitants,
// batiments, nourriture, travail) et vie sociale (liens, souvenirs, entraide, moral). Rien n'est sauve dans
// Content/ ; les fichiers vont dans Saved/YearStudy/. C'est un instrument : il ne juge pas, il compte.
//
// Une annee du jeu = quatre saisons de 30 jours (`AnastasisWeather::SeasonDays`) = 120 jours.

#pragma once

#include "CoreMinimal.h"

namespace AnastasisYearStudy
{
	inline constexpr int32 DaysPerYear = 120;

	enum class EPlayer : uint8
	{
		/** Personne n'est incarne : le village tel qu'il vit sans joueur. */
		None,
		/** Un joueur arrive et ne fait que survivre (boire, manger, dormir) : il ne travaille pas. */
		Survives,
		/** Un joueur arrive, survit, et travaille des qu'il peut (batir, livrer, recolter). */
		Works,
	};

	struct FScenario
	{
		FString Name;
		FString Label;
		EPlayer Player = EPlayer::None;
		/**
		 * Village ferme : `anastasis.Geo.AutoLoad 0` le temps de poser le village. Par defaut, comme dans le jeu, les
		 * fondateurs ouvrent Valmire au monde exterieur et ses groupes d'arrivants viennent au conseil (ecart n°53).
		 */
		bool bClosedValley = false;
	};

	/** Un releve, a la fin du jour `Day`. */
	struct FSample
	{
		int32 Day = 0;
		FString Season;
		// --- Population
		int32 Population = 0;      // vivants, joueur compris
		int32 Families = 0;
		int32 Deaths = 0;          // cumul
		int32 Arrivals = 0;        // cumul : habitants apparus depuis le jour 0 (vivants ou morts)
		int32 Homeless = 0;        // vivants sans maison attribuee
		// --- Economie
		int32 Houses = 0;          // maisons achevees
		int32 Wells = 0;
		int32 Granaries = 0;
		int32 OtherBuildings = 0;
		int32 SitesOpen = 0;       // chantiers en cours
		int32 GranaryFood = 0;     // portions dans les greniers
		int32 GranaryEmptyEvenings = 0; // cumul : soirs ou tous les greniers sont vides
		int32 FieldFood = 0;       // nourriture encore sur pied dans le monde (tuiles vivantes)
		int32 Meals = 0;           // repas pris (cumul des vivants)
		int32 FoodGathered = 0;    // cumul des vivants
		int32 FoodDelivered = 0;   // cumul des vivants
		int32 WoodGathered = 0;    // cumul des vivants
		int32 MaterialsCarried = 0;// cumul des vivants
		int32 PiecesPlaced = 0;    // cumul des vivants
		FString Jobs;              // "farmer:3 builder:2 ..."
		// --- Corps (moyennes des vivants, 0-100)
		double Hunger = 0.0, Thirst = 0.0, Energy = 0.0, Health = 0.0, Morale = 0.0, Social = 0.0;
		int32 Critical = 0;        // vivants en faim, soif ou sante critique
		// --- Vie sociale
		double MeanRelation = 0.0; // relation moyenne declaree (sur les liens existants)
		int32 Friendships = 0;     // paires dont un cote au moins est ami (>= FriendAt)
		int32 Conversations = 0;   // cumul des vivants (socialisations)
		int32 Memories = 0;        // souvenirs portes par les vivants
		int32 StoriesTold = 0;     // cumul des vivants
		int32 HelpAsked = 0;       // cumul du village
		int32 HelpAccepted = 0;
		double MeanReputation = 0.0;
		// --- Joueur (vide sans joueur)
		bool bPlayerAlive = false;
		double PlayerReputation = 0.0;
		int32 PlayerPieces = 0;
		int32 PlayerFoodDelivered = 0;
		int32 PlayerMeals = 0;
	};

	struct FRun
	{
		FScenario Scenario;
		bool bSeeded = false;
		int32 Days = 0;
		double RealSeconds = 0.0;
		TArray<FSample> Samples;
		/** "jour 12 : npc-3 (soif)" */
		TArray<FString> DeathLines;
		/** Batiments acheves : "jour 9 house building-5". */
		TArray<FString> BuildLines;
		FString Chronicle;
		FString PlayerId;
	};

	TArray<FScenario> DefaultScenarios();

	/** Une partie de `Days` jours, releve tous les `Every` jours (et au dernier). */
	FRun Run(const FScenario& Scenario, int32 Days, int32 Every, uint32 Seed);

	FString ToCsv(const FRun& Run);
	FString SummaryJson(const TArray<FRun>& Runs, uint32 Seed);

	/** Ecrit csv, chroniques et resume sous `Dir` ; rend la liste des fichiers ecrits. */
	TArray<FString> WriteRuns(const TArray<FRun>& Runs, uint32 Seed, const FString& Dir);
}
