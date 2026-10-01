#include "Misc/AutomationTest.h"

#include "Algo/Reverse.h"
#include "Core/AnastasisStateDigest.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "Village/AnastasisVillagePresentation.h"
#include "Village/AnastasisVillagerLooks.h"
#include "Village/AnastasisVillagerVisual.h"
#include "WorldView/AnastasisPresentationRegistry.h"
#include "WorldView/AnastasisWorldView.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisVillagerTest
{
	UWorld* FindWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (World->WorldType == EWorldType::Editor
				|| World->WorldType == EWorldType::Game
				|| World->WorldType == EWorldType::PIE))
			{
				return World;
			}
		}
		return nullptr;
	}

	/** The manifest's shape: 8 + 8 adults, 4 + 4 elders, 4 + 4 children. */
	TArray<FAnastasisVillagerLook> ManifestShapedLooks(TFunctionRef<TSoftObjectPtr<UTexture2D>(int32)> PortraitFor)
	{
		struct FRow { const TCHAR* Prefix; EAnastasisVillagerCategory Category; int32 Count; };
		const FRow Rows[] = {
			{ TEXT("CHR_M_Adult_"), EAnastasisVillagerCategory::AdultMale, 8 },
			{ TEXT("CHR_F_Adult_"), EAnastasisVillagerCategory::AdultFemale, 8 },
			{ TEXT("CHR_M_Elder_"), EAnastasisVillagerCategory::ElderMale, 4 },
			{ TEXT("CHR_F_Elder_"), EAnastasisVillagerCategory::ElderFemale, 4 },
			{ TEXT("CHR_M_Child_"), EAnastasisVillagerCategory::ChildMale, 4 },
			{ TEXT("CHR_F_Child_"), EAnastasisVillagerCategory::ChildFemale, 4 },
		};
		TArray<FAnastasisVillagerLook> Looks;
		for (const FRow& Row : Rows)
		{
			for (int32 I = 1; I <= Row.Count; ++I)
			{
				FAnastasisVillagerLook Look;
				Look.LookId = FName(*FString::Printf(TEXT("%s%03d"), Row.Prefix, I));
				Look.Category = Row.Category;
				Look.Portrait = PortraitFor(Looks.Num());
				Looks.Add(Look);
			}
		}
		return Looks;
	}
}

/**
 * Qui porte quel visage : une fonction pure de l'identifiant `npc-N` et du registre.
 *
 * - les enfants n'entrent pas dans le tirage : la simulation n'a que des adultes (ecart n°8) ;
 * - une entree sans portrait n'y entre pas non plus ;
 * - l'ordre du tirage ne depend pas de l'ordre des entrees (reimporter ne change aucun visage) ;
 * - les Pool.Num() premiers habitants ont tous un visage different ;
 * - rien n'est tire au hasard : meme identifiant, meme visage.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillagerLookPoolTest,
	"Anastasis.Village.Villagers.LookPool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillagerLookPoolTest::RunTest(const FString&)
{
	using namespace AnastasisVillagerLooks;
	const auto FakePortrait = [](int32 Index)
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("/Game/Fake/T_%d.T_%d"), Index, Index)));
	};
	TArray<FAnastasisVillagerLook> Looks = AnastasisVillagerTest::ManifestShapedLooks(FakePortrait);
	TestEqual(TEXT("32 individus"), Looks.Num(), 32);

	const TArray<int32> Pool = VillagePool(Looks);
	TestEqual(TEXT("adultes et aines seulement : 24"), Pool.Num(), 24);
	for (int32 Index : Pool)
	{
		TestTrue(FString::Printf(TEXT("%s assignable"), *Looks[Index].LookId.ToString()), IsAssignableInVillage(Looks[Index].Category));
	}

	// Ordre des entrees inverse : memes visages pour les memes habitants.
	TArray<FAnastasisVillagerLook> Reversed = Looks;
	Algo::Reverse(Reversed);
	const TArray<int32> PoolReversed = VillagePool(Reversed);
	TSet<FName> Distinct;
	for (int32 N = 0; N < 60; ++N)
	{
		const FString Id = FString::Printf(TEXT("npc-%d"), N);
		const int32 Pick = PickLook(Pool, Id);
		const int32 PickReversed = PickLook(PoolReversed, Id);
		if (!TestTrue(TEXT("un visage pour chaque habitant"), Pick != INDEX_NONE && PickReversed != INDEX_NONE))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("%s : independant de l'ordre des entrees"), *Id), Reversed[PickReversed].LookId, Looks[Pick].LookId);
		TestEqual(FString::Printf(TEXT("%s : meme identifiant, meme visage"), *Id), PickLook(Pool, Id), Pick);
		if (N < Pool.Num())
		{
			Distinct.Add(Looks[Pick].LookId);
		}
		else
		{
			TestEqual(TEXT("au-dela du pool, le cycle reprend"), Pick, PickLook(Pool, FString::Printf(TEXT("npc-%d"), N - Pool.Num())));
		}
	}
	TestEqual(TEXT("24 premiers habitants : 24 visages differents"), Distinct.Num(), Pool.Num());

	// Equilibre : quelle que soit la taille du village, ses N premiers habitants refletent la
	// population (un tri CRC seul a donne 8 femmes sur 12 au premier run PIE).
	TSet<EAnastasisVillagerCategory> FirstFour;
	for (int32 N = 0; N < 4; ++N)
	{
		FirstFour.Add(Looks[PickLook(Pool, FString::Printf(TEXT("npc-%d"), N))].Category);
	}
	TestEqual(TEXT("4 premiers habitants : les 4 categories attribuables"), FirstFour.Num(), 4);
	for (int32 Size = 2; Size <= Pool.Num(); Size += 2)
	{
		int32 Men = 0;
		for (int32 N = 0; N < Size; ++N)
		{
			const EAnastasisVillagerCategory C = Looks[PickLook(Pool, FString::Printf(TEXT("npc-%d"), N))].Category;
			Men += (C == EAnastasisVillagerCategory::AdultMale || C == EAnastasisVillagerCategory::ElderMale) ? 1 : 0;
		}
		TestEqual(*FString::Printf(TEXT("%d habitants : autant d'hommes que de femmes"), Size), 2 * Men, Size);
	}

	TestEqual(TEXT("identifiant non numerote : stable"), PickLook(Pool, TEXT("visiteur")), PickLook(Pool, TEXT("visiteur")));
	TestEqual(TEXT("pool vide : aucun visage"), PickLook(TArray<int32>(), TEXT("npc-0")), static_cast<int32>(INDEX_NONE));

	Looks[Pool[0]].Portrait.Reset();
	TestEqual(TEXT("une entree sans portrait sort du tirage"), VillagePool(Looks).Num(), 23);
	Looks[Pool[1]].bInGame = false;
	TestEqual(TEXT("un portrait assis (bInGame faux) sort du tirage"), VillagePool(Looks).Num(), 22);
	for (int32 N = 0; N < 40; ++N)
	{
		TestTrue(TEXT("jamais attribue s'il est hors jeu"), Looks[PickLook(VillagePool(Looks), FString::Printf(TEXT("npc-%d"), N))].bInGame);
	}
	return true;
}

/**
 * La carte suit l'habitant et n'ecrit jamais dans la simulation.
 *
 * Simulation : trois `npc-N` dans FVillage.
 * Unreal     : trois AAnastasisVillagerVisual transients, pieds a SimToUnreal, trois visages
 *              differents ; cache quand l'habitant est dedans ; detruits quand il disparait ou
 *              quand le rendu est coupe. Le digest du village ne bouge pas d'un bit.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillagerPresentationTest,
	"Anastasis.Village.Villagers.Presentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillagerPresentationTest::RunTest(const FString&)
{
	UWorld* World = AnastasisVillagerTest::FindWorld();
	if (!TestNotNull(TEXT("editor/game world"), World))
	{
		return false;
	}

	UAnastasisPresentationRegistry* Registry = NewObject<UAnastasisPresentationRegistry>(GetTransientPackage());
	TArray<UTexture2D*> Textures;
	Registry->Villagers = AnastasisVillagerTest::ManifestShapedLooks([&Textures](int32)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(4, 8);
		Texture->AddToRoot();
		Textures.Add(Texture);
		return TSoftObjectPtr<UTexture2D>(Texture);
	});
	Registry->VillagerMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	Registry->AddToRoot();
	ON_SCOPE_EXIT
	{
		Registry->RemoveFromRoot();
		for (UTexture2D* Texture : Textures) { Texture->RemoveFromRoot(); }
	};

	FAnastasisSimulation Sim;
	Sim.Reset(AnastasisWorldView::ReferenceSeed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
	AnastasisVillage::FVillage& Village = Sim.GetVillage();
	TArray<FString> Ids;
	for (int32 I = 0; I < 3; ++I)
	{
		Ids.Add(Village.SpawnNpc(48.5 + I, 48.5, AnastasisNeeds::FNeeds()));
	}
	if (!TestEqual(TEXT("trois habitants simules"), Village.GetActors().Num(), 3))
	{
		return false;
	}
	const FString DigestBefore = AnastasisDigest::ToHex(Village.Digest());

	FAnastasisVillagePresentation Presentation;
	TestEqual(TEXT("trois cartes creees"), Presentation.SyncVillagers(Village, Sim.GetWorld(), World, *Registry, true), 3);
	TestEqual(TEXT("second Sync : rien a creer"), Presentation.SyncVillagers(Village, Sim.GetWorld(), World, *Registry, true), 0);
	TestEqual(TEXT("la presentation n'ecrit pas dans la simulation"), AnastasisDigest::ToHex(Village.Digest()), DigestBefore);

	TSet<FName> Faces;
	for (const FString& Id : Ids)
	{
		AAnastasisVillagerVisual* Card = Presentation.FindVillager(Id);
		if (!TestNotNull(*FString::Printf(TEXT("carte de %s"), *Id), Card))
		{
			return false;
		}
		const AnastasisVillage::FNpc* Npc = Village.FindNpc(Id);
		const FVector Feet = FAnastasisVillagePresentation::SimToUnreal(Sim.GetWorld(), Npc->X, Npc->Y, World);
		TestTrue(*FString::Printf(TEXT("%s : pieds a la position simulee"), *Id), Card->GetActorLocation().Equals(Feet, 0.01));
		TestTrue(*FString::Printf(TEXT("%s : acteur transient, jamais sauve"), *Id), Card->HasAnyFlags(RF_Transient));
		TestFalse(*FString::Printf(TEXT("%s : visible dehors"), *Id), Card->IsHidden());
		Faces.Add(Card->GetLookId());
	}
	TestEqual(TEXT("trois visages differents"), Faces.Num(), 3);

	// La carte tourne autour de la verticale seulement.
	AAnastasisVillagerVisual* First = Presentation.FindVillager(Ids[0]);
	First->FaceTowards(First->GetActorLocation() + FVector(0.0, 500.0, 900.0));
	TestTrue(TEXT("face a la vue, en lacet seulement"), First->GetActorRotation().Equals(FRotator(0.0, 90.0, 0.0), 0.01));
	First->SetMirrored(true);
	TestTrue(TEXT("miroir pose"), First->IsMirrored());

	// Dedans : la position reste le seuil, la carte se cache.
	Village.FindNpcMutable(Ids[1])->Inside.bActive = true;
	Presentation.SyncVillagers(Village, Sim.GetWorld(), World, *Registry, true);
	TestTrue(TEXT("habitant dedans : carte cachee"), Presentation.FindVillager(Ids[1])->IsHidden());
	Village.FindNpcMutable(Ids[1])->Inside.bActive = false;

	// Rendu coupe : toutes les cartes partent.
	TestEqual(TEXT("cartes coupees : trois destructions"), Presentation.SyncVillagers(Village, Sim.GetWorld(), World, *Registry, false), 3);
	TestEqual(TEXT("table vide"), Presentation.NumVillagers(), 0);

	// Habitants disparus (village reconstruit sans eux) : leurs cartes aussi.
	Presentation.SyncVillagers(Village, Sim.GetWorld(), World, *Registry, true);
	Sim.Reset(AnastasisWorldView::ReferenceSeed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
	TestEqual(TEXT("habitants disparus : trois destructions"), Presentation.SyncVillagers(Sim.GetVillage(), Sim.GetWorld(), World, *Registry, true), 3);

	// Registre sans population : aucune carte, et pas d'erreur.
	UAnastasisPresentationRegistry* Empty = NewObject<UAnastasisPresentationRegistry>(GetTransientPackage());
	Sim.GetVillage().SpawnNpc(48.5, 48.5, AnastasisNeeds::FNeeds());
	TestEqual(TEXT("registre vide : aucune carte"), Presentation.SyncVillagers(Sim.GetVillage(), Sim.GetWorld(), World, *Empty, true), 0);

	Presentation.Clear(nullptr);
	return true;
}

#endif
