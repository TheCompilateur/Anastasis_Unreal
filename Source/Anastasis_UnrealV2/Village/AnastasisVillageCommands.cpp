// Commandes Anastasis.Village.* : liaisons minces sur UAnastasisVillageSubsystem.
// Aucune logique ici -- ce fichier resout le sous-systeme du monde et transmet.
//
// Elles existent parce que la fondation d'interaction est invisible : sans mesh,
// un lieu utilisable ne se distingue pas du vide. Ces commandes sont le seul
// moyen de VOIR, en jeu, que le village est bien la avant que les batiments
// n'arrivent.

#include "Anastasis_UnrealV2.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Village/AnastasisVillageArchetypes.h"
#include "Village/AnastasisVillageSubsystem.h"
#include "Village/AnastasisVillageTags.h"

namespace
{
	UAnastasisVillageSubsystem* ResolveVillage(UWorld* World)
	{
		return World != nullptr ? World->GetSubsystem<UAnastasisVillageSubsystem>() : nullptr;
	}
}

static FAutoConsoleCommandWithWorld CmdAnastasisVillageStatus(
	TEXT("Anastasis.Village.Status"),
	TEXT("Logs the number of registered buildings and interaction slots, and whether the Smart Objects runtime is ready."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (const UAnastasisVillageSubsystem* Village = ResolveVillage(World))
		{
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("Anastasis.Village: ready=%s buildings=%d slots=%d"),
				Village->IsReady() ? TEXT("yes") : TEXT("no"),
				Village->GetRegisteredBuildingCount(),
				Village->GetRegisteredSlotCount());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisVillageDraw(
	TEXT("Anastasis.Village.Draw"),
	TEXT("Draws every registered building and interaction slot for N seconds (default 10)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (const UAnastasisVillageSubsystem* Village = ResolveVillage(World))
		{
			const float Duration = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 10.0f;
			Village->DrawDebugInteractions(Duration);
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisVillageSpawnProbe(
	TEXT("Anastasis.Village.SpawnProbe"),
	TEXT("Registers one house, one well and one workshop around the origin. Diagnostic scaffolding, not the village."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UAnastasisVillageSubsystem* Village = ResolveVillage(World);
		if (Village == nullptr)
		{
			return;
		}

		// Identifiants prefixes `probe.` pour qu'ils ne puissent jamais entrer en
		// collision avec ceux qu'un vrai generateur de village attribuera.
		FSmartObjectHandle Handle;
		const EAnastasisVillageRegistrationResult Results[] =
		{
			Village->RegisterBuilding(
				AnastasisVillageArchetypes::MakeHouse(TEXT("probe.house"), FTransform(FVector(0, 0, 0)), 2), Handle),
			Village->RegisterBuilding(
				AnastasisVillageArchetypes::MakeWell(TEXT("probe.well"), FTransform(FVector(1500, 0, 0)), 3), Handle),
			Village->RegisterBuilding(
				AnastasisVillageArchetypes::MakeWorkshop(TEXT("probe.workshop"), FTransform(FVector(0, 1500, 0)), 2), Handle),
		};

		for (const EAnastasisVillageRegistrationResult Result : Results)
		{
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("Anastasis.Village.SpawnProbe: %s"), LexToString(Result));
		}

		Village->DrawDebugInteractions(30.0f);
	}));
