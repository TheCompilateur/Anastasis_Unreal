// Copyright Epic Games, Inc. All Rights Reserved.

#include "Anastasis_UnrealV2GameMode.h"

#include "Anastasis_UnrealV2.h"
#include "Engine/World.h"
#include "WorldView/AnastasisVisualMode.h"
#include "WorldView/AnastasisWorldEmbodiment.h"

AAnastasis_UnrealV2GameMode::AAnastasis_UnrealV2GameMode()
{
	// stub
}

void AAnastasis_UnrealV2GameMode::BeginPlay()
{
	Super::BeginPlay();

	const EAnastasisVisualMode Mode = AnastasisVisualMode::Get();
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VISUAL_MODE %s"), AnastasisVisualMode::Name(Mode));

	if (Mode == EAnastasisVisualMode::None)
	{
		return;
	}

	if (Mode == EAnastasisVisualMode::Player)
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VISUAL_MODE PLAYER is unimplemented; no world renderer spawned"));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<AAnastasisWorldEmbodiment>(
			AAnastasisWorldEmbodiment::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			Params);
	}
}
