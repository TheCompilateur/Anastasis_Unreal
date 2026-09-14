// Invariants de la SCENE RENDUE, pas des donnees.
//
// Les tests qui preexistent verifient des donnees : coordonnees, determinisme,
// comptes, geometrie, et depuis ff423af/d4f1e76 le fait que le dressing se pose
// sur le sol rendu. Deux relations restaient non couvertes, et les deux ont
// casse en silence :
//
//   1. le GameMode spawnait un embodiment dans un niveau qui en placait deja un
//      -- deux mondes identiques empiles a la meme origine, Z-fighting, tous les
//      compteurs doubles ;
//   2. rien ne garantissait que les composants remplis a l'execution restent
//      transients -- une sauvegarde du niveau figeait les instances dans le .umap
//      et le niveau cessait d'etre vide de verite de monde.
//
// Les deux sont ici. Ils echouent si le correctif est defait.

#include "Anastasis_UnrealV2GameMode.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisWorldView.h"
#include "World/AnastasisWorld.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr uint32 TestSeed = 12345u;

	UWorld* FindEditorWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Editor && Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSingleEmbodiment,
	"Anastasis.Visual.SingleEmbodiment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSingleEmbodiment::RunTest(const FString&)
{
	// Monde neuf plutot que le monde editeur : la regle se verifie sur un etat connu,
	// sans avoir a monter une session PIE.
	UWorld* Fresh = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("scratch world"), Fresh)) return false;
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(Fresh);

	TestTrue(
		TEXT("empty world needs the GameMode spawn"),
		AAnastasis_UnrealV2GameMode::ShouldSpawnEmbodiment(Fresh));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AAnastasisWorldEmbodiment* Placed = Fresh->SpawnActor<AAnastasisWorldEmbodiment>(
		AAnastasisWorldEmbodiment::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	TestNotNull(TEXT("level-placed embodiment"), Placed);

	// Le coeur de la regression : c'est ici que le GameMode en empilait un second.
	TestFalse(
		TEXT("a level-placed embodiment suppresses the spawn"),
		AAnastasis_UnrealV2GameMode::ShouldSpawnEmbodiment(Fresh));
	TestFalse(TEXT("null world never spawns"), AAnastasis_UnrealV2GameMode::ShouldSpawnEmbodiment(nullptr));

	GEngine->DestroyWorldContext(Fresh);
	Fresh->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisLevelHoldsNoWorldTruth,
	"Anastasis.Level.HoldsNoWorldTruth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisLevelHoldsNoWorldTruth::RunTest(const FString&)
{
	// Le CDO d'abord, pas un acteur spawne : un spawn de test passe RF_Transient dans
	// FActorSpawnParameters::ObjectFlags, ce dont les sous-objets heritent -- le test
	// passerait alors meme si le constructeur ne posait plus le flag. Les sous-objets du
	// CDO ne portent que ce que le constructeur leur donne.
	const AAnastasisWorldEmbodiment* CDO = GetDefault<AAnastasisWorldEmbodiment>();
	if (!TestNotNull(TEXT("CDO"), CDO)) return false;

	TArray<UHierarchicalInstancedStaticMeshComponent*> GroundMeshes;
	CDO->GetComponents(GroundMeshes);
	// Le dressing n'est pas un sous-objet par defaut sur ce tronc : il est cree a
	// l'incarnation depuis le registre de presentation. Seul le sol est ici.
	TestEqual(TEXT("7 ground meshes on the CDO"), GroundMeshes.Num(), AnastasisWorld::TileTypeCount);
	for (const UHierarchicalInstancedStaticMeshComponent* Mesh : GroundMeshes)
	{
		TestTrue(
			*FString::Printf(
				TEXT("%s is transient (else saving the level bakes its instances into the .umap)"),
				*Mesh->GetName()),
			Mesh->HasAnyFlags(RF_Transient));
	}

	const UProceduralMeshComponent* Surface = CDO->FindComponentByClass<UProceduralMeshComponent>();
	if (TestNotNull(TEXT("surface component exists on the CDO"), Surface))
	{
		TestTrue(TEXT("surface is transient"), Surface->HasAnyFlags(RF_Transient));
	}

	// Puis une incarnation reelle : c'est seulement la que les instances et les composants
	// de dressing existent, et c'est eux que la sauvegarde du niveau figerait.
	UWorld* World = FindEditorWorld();
	if (!TestNotNull(TEXT("editor world"), World)) return false;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AAnastasisWorldEmbodiment* Actor = World->SpawnActor<AAnastasisWorldEmbodiment>(
		AAnastasisWorldEmbodiment::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
	if (!TestNotNull(TEXT("embodiment"), Actor)) return false;

	TestTrue(
		TEXT("embodied"),
		Actor->Embody(TestSeed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight));
	TestTrue(TEXT("ground instances exist after embody"), Actor->GetInstanceCount() > 0);
	// Non-vacuite : sans dressing, la boucle ci-dessous ne verifierait que le sol.
	TestTrue(TEXT("dressing instances exist after embody"), Actor->GetDressingInstanceCount() > 0);

	TArray<UHierarchicalInstancedStaticMeshComponent*> Live;
	Actor->GetComponents(Live);
	TestTrue(TEXT("dressing components were created"), Live.Num() > AnastasisWorld::TileTypeCount);
	for (const UHierarchicalInstancedStaticMeshComponent* Mesh : Live)
	{
		TestTrue(
			*FString::Printf(TEXT("%s transient after embody"), *Mesh->GetName()),
			Mesh->HasAnyFlags(RF_Transient));
	}

	Actor->Destroy();
	return true;
}

#endif
