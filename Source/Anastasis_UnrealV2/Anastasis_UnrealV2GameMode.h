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

	/**
	 * True when this world carries no AAnastasisWorldEmbodiment yet and BeginPlay must spawn one.
	 * A level-placed embodiment always wins: Lvl_AnastasisSlice places one at the origin, and
	 * spawning a second there embodies the identical world twice, stacked and Z-fighting, with
	 * every instance count doubled. Public and static so the rule is testable without PIE.
	 */
	static bool ShouldSpawnEmbodiment(const UWorld* World);
};



