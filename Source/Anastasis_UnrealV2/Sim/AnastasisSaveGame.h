#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AnastasisSaveGame.generated.h"

/**
 * SAVE_STATE_001 -- le conteneur Unreal d'une partie sauvee (Saved/SaveGames/<slot>.sav).
 *
 * Ce n'est PAS le modele de donnees : l'etat est `SimState`, produit par FAnastasisSimulation::SaveState
 * (le meme parcours que l'empreinte d'etat). Le reste est ce que l'hote garde lui-meme et qui decide de la
 * suite : les verrous de ses scenarios et le chemin du scenario exterieur. La presentation (acteurs,
 * cartes, sentiers rendus) n'y est pas : elle se reconstruit depuis la simulation.
 *
 * Un historique de presentation n'est PAS sauve (SAVE_STATE_001.md) : la memoire des passages
 * (UAnastasisAnthropicSubsystem, experimentale, desactivee par defaut). La biographie des batiments, elle,
 * est dans la simulation depuis save-history-001 (ecart n°46), donc dans SimState.
 */
UCLASS()
class UAnastasisSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/**
	 * Version du conteneur (les champs ci-dessous) ; la version de l'etat est dans SimState. 2 : le verrou de la
	 * maison d'ouverture n'est plus un champ de l'hote, il vit dans la simulation (opening-in-sim-001, ecart n°40).
	 */
	static constexpr int32 HostFormatVersion = 2;

	UPROPERTY()
	int32 HostVersion = 0;

	UPROPERTY()
	TArray<uint8> SimState;

	/** Empreinte d'etat au moment de la sauvegarde (hexadecimal), relue apres chargement. */
	UPROPERTY()
	FString StateDigest;

	UPROPERTY()
	FString SavedAtUtc;

	UPROPERTY()
	FString GeoScenarioPath;

	UPROPERTY()
	bool bStartVillage = false;



	UPROPERTY()
	FString FarmerGranaryId;

	UPROPERTY()
	FString FirstSiteId;

	UPROPERTY()
	FIntPoint FarmerField = FIntPoint(-1, -1);
};
