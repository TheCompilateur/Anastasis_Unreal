// arrivants-001 -- qui sont les arrivants, et ce qu'on dit au conseil du soir (hote, ecart n°53).
//
// La simulation decide (`FVillage::UpdateArrivalCouncilDaily`) ; l'hote donne les noms des groupes
// (`Content/Anastasis/Scenario/valmire-arrivants.json`) et les mots du conseil (`repliques-valmire.json`).

#pragma once

#include "CoreMinimal.h"
#include "Village/AnastasisVillage.h"

class FAnastasisSimulation;

namespace AnastasisDialogue
{
	class FLibrary;
}

namespace AnastasisArrivals
{
	/** Lit les groupes d'un fichier `valmire-arrivants.json`. Faux et une erreur si le JSON ne se lit pas. */
	ANASTASIS_UNREALV2_API bool ParsePool(const FString& Json, TArray<AnastasisVillage::FVillage::FArrivalGroup>& Out, FString& OutError);

	/** Les groupes du jeu, lus une fois ; vide si le fichier manque (les arrivants restent alors des inconnus). */
	ANASTASIS_UNREALV2_API const TArray<AnastasisVillage::FVillage::FArrivalGroup>& DefaultPool();

	/** Ce que dit un chef au conseil, d'apres sa raison (`accueil.oui.<raison>` / `accueil.non.<raison>`). */
	ANASTASIS_UNREALV2_API FString VoteLine(const AnastasisDialogue::FLibrary& Lines, uint32 Seed,
		const AnastasisVillage::FVillage::FWelcomeVote& Vote, const FString& Cause, int32 Rank);

	/**
	 * Les arrivants en JSON : le monde exterieur charge ou non, chaque groupe (jour, cause, origine, en attente,
	 * reparti, membres, maison levee) et chaque conseil (verdict, voix, raisons, detail).
	 */
	ANASTASIS_UNREALV2_API FString StatusJson(const FAnastasisSimulation& Sim);
}
