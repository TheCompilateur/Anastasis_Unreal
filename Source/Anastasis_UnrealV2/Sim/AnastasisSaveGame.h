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
 * Deux historiques de presentation ne sont PAS sauves (SAVE_STATE_001.md) : la biographie des batiments
 * (FAnastasisVillagePresentation::Ledger) et la memoire des passages (UAnastasisAnthropicSubsystem).
 */
UCLASS()
class UAnastasisSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Version du conteneur (les champs ci-dessous) ; la version de l'etat est dans SimState. */
	static constexpr int32 HostFormatVersion = 1;

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
	FString OpeningSiteId;

	UPROPERTY()
	FString OpeningWorkId;

	UPROPERTY()
	FString FarmerGranaryId;

	UPROPERTY()
	FString FirstSiteId;

	UPROPERTY()
	FIntPoint FarmerField = FIntPoint(-1, -1);
};
