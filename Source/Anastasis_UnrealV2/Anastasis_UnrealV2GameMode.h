// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Anastasis_UnrealV2GameMode.generated.h"

/**
 * GameMode bootstrap. World embodiment spawn follows anastasis.Visual.Mode
 * (NONE / DEBUG / PLAYER). DEBUG is diagnostic only; PLAYER is unimplemented.
 */
UCLASS(abstract)
class AAnastasis_UnrealV2GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAnastasis_UnrealV2GameMode();

	virtual void BeginPlay() override;
};



