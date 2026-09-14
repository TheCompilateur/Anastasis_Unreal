// Copyright Epic Games, Inc. All Rights Reserved.

#include "Anastasis_UnrealV2GameMode.h"

#include "Anastasis_UnrealV2.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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
		// Lvl_AnastasisSlice places its own AnastasisWorldEmbodiment at the origin. Spawning a
		// second one there embodies the identical world twice, stacked and Z-fighting, with every
		// instance count doubled. The level's actor wins; this spawn is only the fallback for
		// maps that carry no embodiment of their own.
		for (TActorIterator<AAnastasisWorldEmbodiment> It(World); It; ++It)
		{
			UE_LOG(
				LogAnastasis_UnrealV2,
				Display,
				TEXT("ANASTASIS_VISUAL_MODE embodiment already placed in level (%s); no spawn"),
				*It->GetName());
			return;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<AAnastasisWorldEmbodiment>(
			AAnastasisWorldEmbodiment::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			Params);
	}
}
