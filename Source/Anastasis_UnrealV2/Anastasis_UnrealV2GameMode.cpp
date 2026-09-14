// Copyright Epic Games, Inc. All Rights Reserved.

#include "Anastasis_UnrealV2GameMode.h"

#include "Anastasis_UnrealV2.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "WorldView/AnastasisVisualMode.h"
#include "WorldView/AnastasisWorldAtmosphere.h"
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

		// Light before geometry: the atmosphere adopts whatever the level already carries and
		// spawns the rest, so the embodiment lands in an authored, reproducible exposure
		// instead of whichever template lighting the map happens to have.
		if (AAnastasisWorldAtmosphere::IsEnabledByCVar())
		{
			World->SpawnActor<AAnastasisWorldAtmosphere>(
				AAnastasisWorldAtmosphere::StaticClass(),
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				Params);
		}
		else
		{
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_ATMOSPHERE applied=0 reason=cvar_off"));
		}

		if (ShouldSpawnEmbodiment(World))
		{
			World->SpawnActor<AAnastasisWorldEmbodiment>(
				AAnastasisWorldEmbodiment::StaticClass(),
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				Params);
		}
		else
		{
			UE_LOG(
				LogAnastasis_UnrealV2,
				Display,
				TEXT("ANASTASIS_VISUAL_MODE embodiment already placed in level; no spawn"));
		}
	}
}

bool AAnastasis_UnrealV2GameMode::ShouldSpawnEmbodiment(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	for (TActorIterator<AAnastasisWorldEmbodiment> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return false;
	}
	return true;
}
