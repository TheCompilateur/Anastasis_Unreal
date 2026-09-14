// La preuve que la fondation d'interaction tient.
//
// Elle tient dans un monde VIDE : pas de niveau, pas de mesh, pas de NavMesh, pas
// d'acteur place a la main. C'est le coeur de ce qui est verifie ici -- le village
// ANASTASIS sera genere, et une fondation qui aurait besoin d'une scene editeur
// pour fonctionner ne servirait a rien le jour ou les batiments apparaitront par
// centaines a l'execution.
//
// Chaque test monte son propre monde, appelle BeginPlay pour que le runtime Smart
// Objects s'initialise, et le detruit. Aucun etat ne fuit entre les tests.

#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectSubsystem.h"
#include "Village/AnastasisVillageArchetypes.h"
#include "Village/AnastasisVillageSubsystem.h"
#include "Village/AnastasisVillageTags.h"
#include "Village/AnastasisVillagerInteractionComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Monde de test jetable, runtime Smart Objects initialise.
	 *
	 * UpdateWorldComponents pose la classe de partition spatiale sur le
	 * SmartObjectSubsystem (OnWorldComponentsUpdated), puis BeginPlay declenche son
	 * InitializeRuntime. Sans ces deux appels, CreateSmartObject declenche un ensure
	 * et la fondation n'accepte rien.
	 */
	struct FVillageTestWorld
	{
		UWorld* World = nullptr;

		FVillageTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false);
			if (World == nullptr)
			{
				return;
			}

			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);

			World->UpdateWorldComponents(/*bRerunConstructionScripts*/ true, /*bCurrentLevelOnly*/ false);
			World->BeginPlay();
		}

		~FVillageTestWorld()
		{
			if (World != nullptr)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World = nullptr;
			}
		}

		UAnastasisVillageSubsystem* Village() const { return UAnastasisVillageSubsystem::Get(World); }

		/** Un agent : un acteur nu porteur du composant d'interaction. */
		UAnastasisVillagerInteractionComponent* SpawnAgent(const FVector& Location) const
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transient;

			AActor* Agent = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator, Params);
			if (Agent == nullptr)
			{
				return nullptr;
			}

			// Sans composant racine, SetActorLocation ne fait rien et GetActorLocation
			// rend toujours l'origine : la garde d'arrivee serait testee a vide.
			USceneComponent* Root = NewObject<USceneComponent>(Agent, TEXT("AgentRoot"), RF_Transient);
			Agent->SetRootComponent(Root);
			Root->RegisterComponent();
			Agent->SetActorLocation(Location);

			UAnastasisVillagerInteractionComponent* Component =
				NewObject<UAnastasisVillagerInteractionComponent>(Agent, NAME_None, RF_Transient);
			Agent->AddInstanceComponent(Component);
			Component->RegisterComponent();
			return Component;
		}
	};

	FTransform At(const double X, const double Y)
	{
		return FTransform(FVector(X, Y, 0.0));
	}

	constexpr double WideRadius = 100000.0;
}

// ---------------------------------------------------------------------------
// BUILDING SPAWN -> SMART OBJECT REGISTERED -> DESTROY -> UNREGISTERED
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageRegistrationLifecycle,
	"Anastasis.Village.RegistrationLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageRegistrationLifecycle::RunTest(const FString&)
{
	FVillageTestWorld Scratch;
	if (!TestNotNull(TEXT("monde de test"), Scratch.World)) return false;

	UAnastasisVillageSubsystem* Village = Scratch.Village();
	if (!TestNotNull(TEXT("sous-systeme village"), Village)) return false;
	if (!TestTrue(TEXT("runtime Smart Objects pret apres BeginPlay"), Village->IsReady())) return false;

	// --- Trois batiments, aucun acteur, aucun asset ---------------------------

	FSmartObjectHandle House, Well, Workshop;

	TestEqual(TEXT("maison enregistree"),
		LexToString(Village->RegisterBuilding(AnastasisVillageArchetypes::MakeHouse(TEXT("house.duguay"), At(0, 0), 2), House)),
		TEXT("Succeeded"));
	TestEqual(TEXT("puits enregistre"),
		LexToString(Village->RegisterBuilding(AnastasisVillageArchetypes::MakeWell(TEXT("well.place"), At(2000, 0), 3), Well)),
		TEXT("Succeeded"));
	TestEqual(TEXT("atelier enregistre"),
		LexToString(Village->RegisterBuilding(AnastasisVillageArchetypes::MakeWorkshop(TEXT("workshop.forge"), At(0, 2000), 2), Workshop)),
		TEXT("Succeeded"));

	TestTrue(TEXT("poignee maison valide"), House.IsValid());
	TestTrue(TEXT("poignee puits valide"), Well.IsValid());
	TestTrue(TEXT("poignee atelier valide"), Workshop.IsValid());

	TestEqual(TEXT("3 batiments au registre"), Village->GetRegisteredBuildingCount(), 3);

	// 2 lits + (3 margelles + 1 poste de puisage) + (2 postes + 1 reserve) = 9 places.
	// Les capacites sont depliees : une place Smart Object par occupant possible.
	TestEqual(TEXT("9 places Smart Object, capacites depliees"), Village->GetRegisteredSlotCount(), 9);

	// --- QUERY -> SLOT FOUND --------------------------------------------------

	FAnastasisInteractionQueryResult Found;
	TestTrue(TEXT("un lit se trouve"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Sleep, FVector::ZeroVector, WideRadius, {}, Found));
	TestEqual(TEXT("le lit appartient a la maison"), Found.Address.BuildingId, FName(TEXT("house.duguay")));
	TestEqual(TEXT("le lit porte le SlotId declare"), Found.Address.SlotId, FName(TEXT("bed")));
	TestEqual(TEXT("l'activite du resultat est Sleep"), Found.ActivityTag, AnastasisVillageTags::Activity_Sleep.GetTag());
	TestEqual(TEXT("la categorie du lieu remonte"), Found.BuildingTag, AnastasisVillageTags::Building_House.GetTag());

	// Le chemin retour vers la simulation : le handle moteur redonne l'adresse stable.
	FAnastasisInteractionAddress RoundTrip;
	TestTrue(TEXT("l'adresse se resout depuis le handle moteur"), Village->ResolveAddress(Found.SlotHandle, RoundTrip));
	TestTrue(TEXT("l'adresse resolue est celle du resultat"), RoundTrip == Found.Address);

	// Un batiment expose PLUSIEURS activites : c'est ce que la mission demande de montrer.
	FAnastasisInteractionQueryResult Drink, Draw, Work, Deposit;
	TestTrue(TEXT("le puits expose Drink"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Drink, FVector::ZeroVector, WideRadius, {}, Drink));
	TestTrue(TEXT("le puits expose aussi Storage.Take"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Storage_Take, FVector::ZeroVector, WideRadius, {}, Draw));
	TestEqual(TEXT("les deux activites sont sur le meme batiment"), Drink.Address.BuildingId, Draw.Address.BuildingId);
	TestNotEqual(TEXT("mais sur des slots distincts"), Drink.Address.SlotId, Draw.Address.SlotId);

	TestTrue(TEXT("l'atelier expose Work"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Work, FVector::ZeroVector, WideRadius, {}, Work));
	TestTrue(TEXT("l'atelier expose aussi Storage.Deposit"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Storage_Deposit, FVector::ZeroVector, WideRadius, {}, Deposit));
	TestEqual(TEXT("les deux activites de l'atelier partagent le batiment"), Work.Address.BuildingId, Deposit.Address.BuildingId);

	// Une requete sur le parent trouve les deux formes de stockage.
	TArray<FAnastasisInteractionQueryResult> AnyStorage;
	Village->FindInteractions(AnastasisVillageTags::Activity_Storage, FVector::ZeroVector, WideRadius, {}, AnyStorage);
	TestEqual(TEXT("Activity.Storage trouve Take et Deposit"), AnyStorage.Num(), 2);

	// --- BUILDING DESTROY -> SMART OBJECT UNREGISTERED -----------------------

	TestTrue(TEXT("l'atelier se retire"), Village->UnregisterBuilding(Workshop));
	TestEqual(TEXT("2 batiments restants"), Village->GetRegisteredBuildingCount(), 2);
	TestEqual(TEXT("6 places restantes"), Village->GetRegisteredSlotCount(), 6);

	FAnastasisInteractionQueryResult GoneWork;
	TestFalse(TEXT("plus aucun poste de travail apres destruction"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Work, FVector::ZeroVector, WideRadius, {}, GoneWork));

	TestFalse(TEXT("retirer deux fois le meme batiment echoue"), Village->UnregisterBuilding(Workshop));

	// --- RECREATION ----------------------------------------------------------

	FSmartObjectHandle Rebuilt;
	TestEqual(TEXT("l'atelier se reenregistre sous le meme identifiant"),
		LexToString(Village->RegisterBuilding(AnastasisVillageArchetypes::MakeWorkshop(TEXT("workshop.forge"), At(0, 2000), 4), Rebuilt)),
		TEXT("Succeeded"));
	TestTrue(TEXT("la nouvelle poignee differe de l'ancienne"), Rebuilt != Workshop);

	FAnastasisInteractionQueryResult BackToWork;
	TestTrue(TEXT("les postes de travail sont revenus"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Work, FVector::ZeroVector, WideRadius, {}, BackToWork));
	TestEqual(TEXT("4 postes + 1 reserve au nouvel atelier, soit 11 places"), Village->GetRegisteredSlotCount(), 11);

	// --- Refus nommes --------------------------------------------------------

	FSmartObjectHandle Rejected;
	TestEqual(TEXT("un identifiant deja pris est refuse"),
		LexToString(Village->RegisterBuilding(AnastasisVillageArchetypes::MakeHouse(TEXT("house.duguay"), At(500, 500), 1), Rejected)),
		TEXT("DuplicateBuildingId"));

	FAnastasisBuildingSpec Anonymous = AnastasisVillageArchetypes::MakeHouse(TEXT("tmp"), At(0, 0), 1);
	Anonymous.BuildingId = NAME_None;
	TestEqual(TEXT("un batiment sans identifiant est refuse"),
		LexToString(Village->RegisterBuilding(Anonymous, Rejected)),
		TEXT("MissingBuildingId"));

	FAnastasisBuildingSpec Empty;
	Empty.BuildingId = TEXT("hollow");
	TestEqual(TEXT("un batiment sans place n'est pas un lieu"),
		LexToString(Village->RegisterBuilding(Empty, Rejected)),
		TEXT("InvalidSlots"));

	FAnastasisBuildingSpec Untagged = AnastasisVillageArchetypes::MakeHouse(TEXT("untagged"), At(0, 0), 1);
	Untagged.Slots[0].ActivityTag = FGameplayTag();
	TestEqual(TEXT("une place sans activite est refusee"),
		LexToString(Village->RegisterBuilding(Untagged, Rejected)),
		TEXT("InvalidSlots"));

	FAnastasisBuildingSpec ShortPoses = AnastasisVillageArchetypes::MakeHouse(TEXT("shortposes"), At(0, 0), 3);
	ShortPoses.Slots[0].OccurrencePoses.SetNum(2);
	TestEqual(TEXT("une liste de poses qui ne couvre pas la capacite est refusee"),
		LexToString(Village->RegisterBuilding(ShortPoses, Rejected)),
		TEXT("InvalidSlots"));

	FAnastasisBuildingSpec Collision = AnastasisVillageArchetypes::MakeHouse(TEXT("collision"), At(0, 0), 1);
	// Copie locale : TArray::Add refuse un element issu du tableau qu'il modifie.
	const FAnastasisInteractionSlotSpec Duplicate = Collision.Slots[0];
	Collision.Slots.Add(Duplicate);
	TestEqual(TEXT("deux places du meme nom rendraient l'adresse ambigue"),
		LexToString(Village->RegisterBuilding(Collision, Rejected)),
		TEXT("DuplicateSlotId"));

	// Aucun refus n'a laisse de trace : 3 batiments, comme avant la serie.
	TestEqual(TEXT("les refus n'enregistrent rien partiellement"), Village->GetRegisteredBuildingCount(), 3);

	return true;
}

// ---------------------------------------------------------------------------
// AGENT A CLAIM -> SUCCESS ; AGENT B SAME SLOT -> REJECTED ; A RELEASE -> FREE
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageConcurrentReservation,
	"Anastasis.Village.ConcurrentReservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageConcurrentReservation::RunTest(const FString&)
{
	FVillageTestWorld Scratch;
	if (!TestNotNull(TEXT("monde de test"), Scratch.World)) return false;

	UAnastasisVillageSubsystem* Village = Scratch.Village();
	if (!TestNotNull(TEXT("sous-systeme village"), Village)) return false;

	// Une maison, UN SEUL lit : la place est donc exclusive par construction.
	FSmartObjectHandle House;
	if (!TestEqual(TEXT("maison a un lit enregistree"),
		LexToString(Village->RegisterBuilding(AnastasisVillageArchetypes::MakeHouse(TEXT("house.solo"), At(0, 0), 1), House)),
		TEXT("Succeeded"))) return false;

	FAnastasisInteractionQueryResult Bed;
	if (!TestTrue(TEXT("le lit se trouve"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Sleep, FVector::ZeroVector, WideRadius, {}, Bed)))
		return false;

	TestEqual(TEXT("le lit est libre avant toute reservation"),
		static_cast<int32>(Village->GetSlotState(Bed.SlotHandle)), static_cast<int32>(ESmartObjectSlotState::Free));

	// --- AGENT A CLAIM -> SUCCESS -------------------------------------------

	FAnastasisInteractionClaim ClaimA;
	TestTrue(TEXT("agent A reserve le lit"), Village->ClaimInteraction(Bed, ClaimA));
	TestTrue(TEXT("la reservation de A est valide"), ClaimA.IsValid());
	TestEqual(TEXT("le lit est Claimed"),
		static_cast<int32>(Village->GetSlotState(Bed.SlotHandle)), static_cast<int32>(ESmartObjectSlotState::Claimed));

	// --- AGENT B, MEME SLOT -> REJECTED -------------------------------------

	FAnastasisInteractionClaim ClaimB;
	TestFalse(TEXT("agent B ne peut pas prendre le lit deja tenu"), Village->ClaimInteraction(Bed, ClaimB));
	TestFalse(TEXT("la reservation refusee reste invalide"), ClaimB.IsValid());

	// Et la place n'apparait plus dans une requete : B ne la voit meme pas.
	TArray<FAnastasisInteractionQueryResult> StillFree;
	Village->FindInteractions(AnastasisVillageTags::Activity_Sleep, FVector::ZeroVector, WideRadius, {}, StillFree);
	TestEqual(TEXT("aucun lit libre tant que A tient le sien"), StillFree.Num(), 0);

	// --- AGENT B -> ALTERNATIVE ---------------------------------------------

	// Une seconde maison apparait. B doit la trouver : « refuse » ne doit jamais
	// signifier « bloque » quand une autre place existe.
	FSmartObjectHandle Neighbour;
	TestEqual(TEXT("maison voisine enregistree"),
		LexToString(Village->RegisterBuilding(AnastasisVillageArchetypes::MakeHouse(TEXT("house.voisine"), At(1500, 0), 1), Neighbour)),
		TEXT("Succeeded"));

	UAnastasisVillagerInteractionComponent* AgentB = Scratch.SpawnAgent(FVector::ZeroVector);
	if (!TestNotNull(TEXT("agent B"), AgentB)) return false;

	TestTrue(TEXT("agent B trouve une alternative malgre le lit occupe"),
		AgentB->RequestInteractionFor(AnastasisVillageTags::Activity_Sleep, WideRadius));
	TestEqual(TEXT("l'alternative de B est la maison voisine"),
		AgentB->GetInteractionAddress().BuildingId, FName(TEXT("house.voisine")));

	// --- AGENT A RELEASE -> SLOT AVAILABLE ----------------------------------

	TestTrue(TEXT("agent A relache"), Village->ReleaseInteraction(ClaimA));
	TestFalse(TEXT("le claim de A est invalide apres relachement"), ClaimA.IsValid());
	TestEqual(TEXT("le lit est redevenu Free"),
		static_cast<int32>(Village->GetSlotState(Bed.SlotHandle)), static_cast<int32>(ESmartObjectSlotState::Free));

	TestFalse(TEXT("relacher deux fois n'a pas de sens et echoue"), Village->ReleaseInteraction(ClaimA));

	// --- SLOT REUTILISABLE ---------------------------------------------------

	FAnastasisInteractionQueryResult Again;
	TestTrue(TEXT("le lit se retrouve apres relachement"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Sleep, FVector::ZeroVector, WideRadius, {}, Again));
	TestTrue(TEXT("c'est la meme place"), Again.Address == Bed.Address);

	FAnastasisInteractionClaim Reclaim;
	TestTrue(TEXT("la place se reserve a nouveau"), Village->ClaimInteraction(Again, Reclaim));
	TestTrue(TEXT("la nouvelle reservation est valide"), Reclaim.IsValid());

	// --- CAPACITE : N places, N reservations, puis refus --------------------

	FSmartObjectHandle Dormitory;
	TestEqual(TEXT("dortoir a trois lits enregistre"),
		LexToString(Village->RegisterBuilding(AnastasisVillageArchetypes::MakeHouse(TEXT("house.dortoir"), At(0, 5000), 3), Dormitory)),
		TEXT("Succeeded"));

	const FVector DormitoryOrigin(0.0, 5000.0, 0.0);
	TArray<FAnastasisInteractionClaim> Claims;
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		FAnastasisInteractionQueryResult Slot;
		if (!Village->FindNearestInteraction(AnastasisVillageTags::Activity_Sleep, DormitoryOrigin, 500.0, {}, Slot))
		{
			break;
		}
		FAnastasisInteractionClaim Held;
		if (!Village->ClaimInteraction(Slot, Held))
		{
			break;
		}
		Claims.Add(Held);
	}
	TestEqual(TEXT("une capacite de 3 accorde exactement 3 reservations"), Claims.Num(), 3);

	// Les trois places sont distinctes : la capacite n'est pas un compteur sur une
	// seule place, et deux dormeurs ne se superposent pas.
	TSet<int32> Occurrences;
	TSet<FVector> Locations;
	for (const FAnastasisInteractionClaim& Held : Claims)
	{
		Occurrences.Add(Held.Address.Occurrence);
		Locations.Add(Held.SlotTransform.GetLocation());
	}
	TestEqual(TEXT("trois occurrences distinctes"), Occurrences.Num(), 3);
	TestEqual(TEXT("trois positions distinctes"), Locations.Num(), 3);

	return true;
}

// ---------------------------------------------------------------------------
// Recherche spatiale : rayon, ordre, determinisme, conditions d'acces
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageSpatialQuery,
	"Anastasis.Village.SpatialQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageSpatialQuery::RunTest(const FString&)
{
	FVillageTestWorld Scratch;
	if (!TestNotNull(TEXT("monde de test"), Scratch.World)) return false;

	UAnastasisVillageSubsystem* Village = Scratch.Village();
	if (!TestNotNull(TEXT("sous-systeme village"), Village)) return false;

	// Trois puits alignes, a 1000, 4000 et 20000 cm de l'origine.
	FSmartObjectHandle Near, Middle, Far;
	Village->RegisterBuilding(AnastasisVillageArchetypes::MakeWell(TEXT("well.near"), At(1000, 0), 1), Near);
	Village->RegisterBuilding(AnastasisVillageArchetypes::MakeWell(TEXT("well.middle"), At(4000, 0), 1), Middle);
	Village->RegisterBuilding(AnastasisVillageArchetypes::MakeWell(TEXT("well.far"), At(20000, 0), 1), Far);

	// --- Le plus proche est bien le plus proche ------------------------------

	FAnastasisInteractionQueryResult Nearest;
	if (!TestTrue(TEXT("un point d'eau se trouve"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Drink, FVector::ZeroVector, WideRadius, {}, Nearest)))
		return false;
	TestEqual(TEXT("c'est le puits le plus proche"), Nearest.Address.BuildingId, FName(TEXT("well.near")));

	TArray<FAnastasisInteractionQueryResult> All;
	Village->FindInteractions(AnastasisVillageTags::Activity_Drink, FVector::ZeroVector, WideRadius, {}, All);
	if (!TestEqual(TEXT("les trois margelles sont trouvees"), All.Num(), 3)) return false;
	TestEqual(TEXT("ordre 1 : near"), All[0].Address.BuildingId, FName(TEXT("well.near")));
	TestEqual(TEXT("ordre 2 : middle"), All[1].Address.BuildingId, FName(TEXT("well.middle")));
	TestEqual(TEXT("ordre 3 : far"), All[2].Address.BuildingId, FName(TEXT("well.far")));
	TestTrue(TEXT("les distances croissent"), All[0].Distance < All[1].Distance && All[1].Distance < All[2].Distance);

	// La distance est mesuree sur la PLACE, pas sur le centre du batiment : la
	// margelle du puits le plus proche est a 120 cm de son centre, du cote oppose a
	// l'origine, donc a 1120 cm et non 1000. Si la mesure portait sur le centre,
	// cette valeur serait 1000.
	TestEqual(TEXT("la distance porte sur la place, pas sur le centre du batiment"),
		All[0].Distance, 1120.0, 1.0);

	// --- Le rayon exclut vraiment -------------------------------------------

	TArray<FAnastasisInteractionQueryResult> Close;
	Village->FindInteractions(AnastasisVillageTags::Activity_Drink, FVector::ZeroVector, 5000.0, {}, Close);
	TestEqual(TEXT("un rayon de 5000 ne retient que les deux puits proches"), Close.Num(), 2);

	TArray<FAnastasisInteractionQueryResult> Tight;
	Village->FindInteractions(AnastasisVillageTags::Activity_Drink, FVector::ZeroVector, 2000.0, {}, Tight);
	TestEqual(TEXT("un rayon de 2000 ne retient que le premier"), Tight.Num(), 1);

	TArray<FAnastasisInteractionQueryResult> None;
	TestFalse(TEXT("un rayon trop court ne trouve rien"),
		Village->FindInteractions(AnastasisVillageTags::Activity_Drink, FVector::ZeroVector, 100.0, {}, None));
	TestEqual(TEXT("et ne rend aucun resultat"), None.Num(), 0);

	// --- Determinisme -------------------------------------------------------

	// La grille spatiale du moteur n'a pas d'ordre d'iteration garanti. Deux
	// requetes identiques doivent malgre tout choisir la meme place, sans quoi deux
	// executions de la meme graine divergeraient.
	TArray<FAnastasisInteractionQueryResult> Repeat;
	Village->FindInteractions(AnastasisVillageTags::Activity_Drink, FVector::ZeroVector, WideRadius, {}, Repeat);
	if (TestEqual(TEXT("meme nombre de resultats"), Repeat.Num(), All.Num()))
	{
		for (int32 Index = 0; Index < All.Num(); ++Index)
		{
			TestTrue(
				*FString::Printf(TEXT("resultat %d identique entre deux requetes (%s)"), Index, *All[Index].Address.ToString()),
				Repeat[Index].Address == All[Index].Address);
		}
	}

	// --- Une activite absente ne se trouve pas ------------------------------

	FAnastasisInteractionQueryResult NoSocial;
	TestFalse(TEXT("aucun puits n'accueille Socialize"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Socialize, FVector::ZeroVector, WideRadius, {}, NoSocial));

	// --- Conditions d'acces : RequiredUserTags ------------------------------

	FAnastasisBuildingSpec Private = AnastasisVillageArchetypes::MakeHouse(TEXT("house.privee"), At(0, 8000), 1);
	Private.Slots[0].RequiredUserTags.AddTag(AnastasisVillageTags::Building_House.GetTag());

	FSmartObjectHandle PrivateHandle;
	if (!TestEqual(TEXT("maison a acces conditionne enregistree"),
		LexToString(Village->RegisterBuilding(Private, PrivateHandle)), TEXT("Succeeded"))) return false;

	const FVector PrivateOrigin(0.0, 8000.0, 0.0);

	FAnastasisInteractionQueryResult Denied;
	TestFalse(TEXT("sans les tags requis, la place est invisible"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Sleep, PrivateOrigin, 1000.0, {}, Denied));

	FGameplayTagContainer Credentials;
	Credentials.AddTag(AnastasisVillageTags::Building_House.GetTag());

	FAnastasisInteractionQueryResult Allowed;
	TestTrue(TEXT("avec les tags requis, la place apparait"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Sleep, PrivateOrigin, 1000.0, Credentials, Allowed));
	TestEqual(TEXT("et c'est bien la maison conditionnee"), Allowed.Address.BuildingId, FName(TEXT("house.privee")));

	// --- Requetes degenerees ------------------------------------------------

	FAnastasisInteractionQueryResult Garbage;
	TestFalse(TEXT("un tag d'activite invalide est refuse"),
		Village->FindNearestInteraction(FGameplayTag(), FVector::ZeroVector, WideRadius, {}, Garbage));
	TestFalse(TEXT("un rayon nul est refuse"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Drink, FVector::ZeroVector, 0.0, {}, Garbage));
	TestFalse(TEXT("un rayon negatif est refuse"),
		Village->FindNearestInteraction(AnastasisVillageTags::Activity_Drink, FVector::ZeroVector, -500.0, {}, Garbage));

	return true;
}

// ---------------------------------------------------------------------------
// La sequence d'execution complete, sur deux agents
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageExecutionSequence,
	"Anastasis.Village.ExecutionSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageExecutionSequence::RunTest(const FString&)
{
	FVillageTestWorld Scratch;
	if (!TestNotNull(TEXT("monde de test"), Scratch.World)) return false;

	UAnastasisVillageSubsystem* Village = Scratch.Village();
	if (!TestNotNull(TEXT("sous-systeme village"), Village)) return false;

	FSmartObjectHandle Workshop;
	if (!TestEqual(TEXT("atelier a un poste enregistre"),
		LexToString(Village->RegisterBuilding(AnastasisVillageArchetypes::MakeWorkshop(TEXT("workshop.unique"), At(0, 0), 1), Workshop)),
		TEXT("Succeeded"))) return false;

	UAnastasisVillagerInteractionComponent* AgentA = Scratch.SpawnAgent(FVector(3000.0, 0.0, 0.0));
	UAnastasisVillagerInteractionComponent* AgentB = Scratch.SpawnAgent(FVector(3200.0, 0.0, 0.0));
	if (!TestNotNull(TEXT("agent A"), AgentA)) return false;
	if (!TestNotNull(TEXT("agent B"), AgentB)) return false;

	// --- Idle : rien ne part tout seul --------------------------------------

	TestEqual(TEXT("A part de Idle"), LexToString(AgentA->GetPhase()), TEXT("Idle"));
	TestFalse(TEXT("aucune intention au depart"), AgentA->HasPendingIntent());
	TestFalse(TEXT("sans intention, aucune place n'est cherchee"), AgentA->RequestInteraction());
	TestEqual(TEXT("une demande sans intention echoue en Failed"), LexToString(AgentA->GetPhase()), TEXT("Failed"));

	// --- NeedInteraction : la simulation depose l'intention -----------------

	AgentA->SetIntent(AnastasisVillageTags::Activity_Work, WideRadius);
	TestTrue(TEXT("l'intention est en attente"), AgentA->HasPendingIntent());
	TestEqual(TEXT("et c'est celle que la simulation a posee"),
		AgentA->GetPendingActivity(), AnastasisVillageTags::Activity_Work.GetTag());

	// --- FindSmartObject -> reservation --------------------------------------

	if (!TestTrue(TEXT("A trouve et reserve un poste"), AgentA->RequestInteraction())) return false;
	TestEqual(TEXT("A est en Reserved"), LexToString(AgentA->GetPhase()), TEXT("Reserved"));
	TestTrue(TEXT("A tient une reservation"), AgentA->HasClaim());
	TestEqual(TEXT("la place vient de l'atelier"), AgentA->GetInteractionAddress().BuildingId, FName(TEXT("workshop.unique")));
	TestEqual(TEXT("et c'est un poste de travail"), AgentA->GetInteractionAddress().SlotId, FName(TEXT("station")));

	// Concurrence a travers le composant : B veut le meme poste, il n'y en a qu'un.
	AgentB->SetIntent(AnastasisVillageTags::Activity_Work, WideRadius);
	TestFalse(TEXT("B ne trouve aucun poste libre"), AgentB->RequestInteraction());
	TestEqual(TEXT("B echoue explicitement"), LexToString(AgentB->GetPhase()), TEXT("Failed"));
	TestFalse(TEXT("B ne tient rien"), AgentB->HasClaim());
	TestTrue(TEXT("l'intention de B survit a l'echec : il veut toujours travailler"), AgentB->HasPendingIntent());

	// --- MoveTo : la garde d'arrivee -----------------------------------------

	TestFalse(TEXT("A n'est pas encore arrive"), AgentA->HasArrived());
	TestFalse(TEXT("et ne peut donc pas commencer a travailler a distance"), AgentA->BeginUse());
	TestEqual(TEXT("A reste en Reserved apres ce refus"), LexToString(AgentA->GetPhase()), TEXT("Reserved"));
	TestTrue(TEXT("le refus ne lui a pas fait perdre sa place"), AgentA->HasClaim());

	// Le deplacement est simule par un teleport : aller jusque la est le travail
	// d'une tache de deplacement, pas de cette fondation.
	AgentA->GetOwner()->SetActorLocation(AgentA->GetInteractionTransform().GetLocation());
	TestTrue(TEXT("A est arrive"), AgentA->HasArrived());

	// --- Use ----------------------------------------------------------------

	if (!TestTrue(TEXT("A commence a travailler"), AgentA->BeginUse())) return false;
	TestEqual(TEXT("A est en InUse"), LexToString(AgentA->GetPhase()), TEXT("InUse"));
	TestEqual(TEXT("la place est Occupied cote moteur"),
		static_cast<int32>(Village->GetSlotState(AgentA->GetClaim().ClaimHandle.SlotHandle)),
		static_cast<int32>(ESmartObjectSlotState::Occupied));

	// L'adresse tenue se resout toujours vers la simulation pendant l'acte.
	FAnastasisInteractionAddress InUseAddress;
	TestTrue(TEXT("l'adresse se resout pendant l'occupation"),
		Village->ResolveAddress(AgentA->GetClaim().ClaimHandle.SlotHandle, InUseAddress));
	TestTrue(TEXT("et designe la place tenue"), InUseAddress == AgentA->GetInteractionAddress());

	// --- Release -> Complete -------------------------------------------------

	const FAnastasisInteractionAddress Released = AgentA->GetInteractionAddress();
	TestTrue(TEXT("A relache"), AgentA->CompleteAndRelease());
	TestEqual(TEXT("A est en Complete"), LexToString(AgentA->GetPhase()), TEXT("Complete"));
	TestFalse(TEXT("A ne tient plus rien"), AgentA->HasClaim());
	TestFalse(TEXT("l'intention de A est consommee"), AgentA->HasPendingIntent());

	// --- B recupere la place ------------------------------------------------

	TestTrue(TEXT("B trouve enfin le poste libere"), AgentB->RequestInteraction());
	TestEqual(TEXT("B est en Reserved"), LexToString(AgentB->GetPhase()), TEXT("Reserved"));
	TestTrue(TEXT("B a obtenu la place que A tenait"), AgentB->GetInteractionAddress() == Released);

	// --- Destruction sous l'occupant ----------------------------------------

	AgentB->GetOwner()->SetActorLocation(AgentB->GetInteractionTransform().GetLocation());
	if (!TestTrue(TEXT("B commence a travailler"), AgentB->BeginUse())) return false;

	TestTrue(TEXT("l'atelier est detruit pendant que B y travaille"), Village->UnregisterBuilding(Workshop));
	TestEqual(TEXT("la place n'existe plus"),
		static_cast<int32>(Village->GetSlotState(AgentB->GetClaim().ClaimHandle.SlotHandle)),
		static_cast<int32>(ESmartObjectSlotState::Invalid));

	// Le relachement echoue -- le moteur a deja avorte la reservation -- mais la
	// sequence se termine proprement plutot que de laisser B accroche a un lieu
	// disparu.
	TestFalse(TEXT("relacher une place detruite echoue"), AgentB->CompleteAndRelease());
	TestEqual(TEXT("B termine quand meme sa sequence"), LexToString(AgentB->GetPhase()), TEXT("Complete"));
	TestFalse(TEXT("B ne tient plus rien"), AgentB->HasClaim());

	// --- Abandon : l'intention survit ---------------------------------------

	FSmartObjectHandle Rebuilt;
	TestEqual(TEXT("un nouvel atelier apparait"),
		LexToString(Village->RegisterBuilding(AnastasisVillageArchetypes::MakeWorkshop(TEXT("workshop.neuf"), At(0, 0), 1), Rebuilt)),
		TEXT("Succeeded"));

	AgentA->SetIntent(AnastasisVillageTags::Activity_Work, WideRadius);
	if (!TestTrue(TEXT("A reserve dans le nouvel atelier"), AgentA->RequestInteraction())) return false;

	const FSmartObjectSlotHandle AbandonedSlot = AgentA->GetClaim().ClaimHandle.SlotHandle;
	AgentA->AbandonInteraction();
	TestEqual(TEXT("A revient a Idle"), LexToString(AgentA->GetPhase()), TEXT("Idle"));
	TestFalse(TEXT("A a lache la place"), AgentA->HasClaim());
	TestTrue(TEXT("mais il veut toujours travailler"), AgentA->HasPendingIntent());
	TestEqual(TEXT("la place abandonnee est Free, pas perdue"),
		static_cast<int32>(Village->GetSlotState(AbandonedSlot)), static_cast<int32>(ESmartObjectSlotState::Free));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
