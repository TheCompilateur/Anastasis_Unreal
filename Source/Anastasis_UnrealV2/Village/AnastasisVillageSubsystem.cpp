#include "Village/AnastasisVillageSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "SmartObjectDefinition.h"
// SmartObjectSubsystem.h ne fait que declarer FSmartObjectRequest/Filter/Result.
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"

namespace
{
	/**
	 * Priorite de reservation unique pour tout le village.
	 *
	 * Le moteur permet a une priorite haute de voler la place d'une priorite
	 * basse. ANASTASIS ne s'en sert pas : c'est la simulation qui arbitre les
	 * conflits entre villageois, et lui laisser cet arbitrage signifie que deux
	 * agents Unreal se disputant un lit doivent etre a egalite -- le premier
	 * arrive tient la place, le second se voit refuser et cherche ailleurs. Une
	 * priorite Unreal introduirait une hierarchie sociale dont la simulation
	 * n'aurait pas connaissance.
	 */
	constexpr ESmartObjectClaimPriority VillageClaimPriority = ESmartObjectClaimPriority::Normal;
}

const TCHAR* LexToString(EAnastasisVillageRegistrationResult Result)
{
	switch (Result)
	{
	case EAnastasisVillageRegistrationResult::Succeeded:            return TEXT("Succeeded");
	case EAnastasisVillageRegistrationResult::MissingBuildingId:    return TEXT("MissingBuildingId");
	case EAnastasisVillageRegistrationResult::DuplicateBuildingId:  return TEXT("DuplicateBuildingId");
	case EAnastasisVillageRegistrationResult::InvalidSlots:         return TEXT("InvalidSlots");
	case EAnastasisVillageRegistrationResult::DuplicateSlotId:      return TEXT("DuplicateSlotId");
	case EAnastasisVillageRegistrationResult::SubsystemUnavailable: return TEXT("SubsystemUnavailable");
	}
	return TEXT("Unknown");
}

UAnastasisVillageSubsystem* UAnastasisVillageSubsystem::Get(const UWorld* World)
{
	return World != nullptr ? World->GetSubsystem<UAnastasisVillageSubsystem>() : nullptr;
}

USmartObjectSubsystem* UAnastasisVillageSubsystem::GetSmartObjects() const
{
	return USmartObjectSubsystem::GetCurrent(GetWorld());
}

bool UAnastasisVillageSubsystem::IsReady() const
{
	const USmartObjectSubsystem* SmartObjects = GetSmartObjects();
	// HasCalledBeginPlay est le seul signal public equivalent a bRuntimeInitialized,
	// qui est prive. Les deux sont poses dans le meme UWorld::BeginPlay.
	return SmartObjects != nullptr && SmartObjects->HasCalledBeginPlay();
}

void UAnastasisVillageSubsystem::Deinitialize()
{
	// Le SmartObjectSubsystem nettoie son propre runtime, mais nos enregistrements
	// et nos definitions doivent partir aussi, sinon un monde recree heriterait
	// d'identifiants de batiments deja pris.
	UnregisterAllBuildings();
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

EAnastasisVillageRegistrationResult UAnastasisVillageSubsystem::ValidateSpec(const FAnastasisBuildingSpec& Spec)
{
	if (Spec.BuildingId.IsNone())
	{
		return EAnastasisVillageRegistrationResult::MissingBuildingId;
	}

	if (Spec.Slots.IsEmpty())
	{
		return EAnastasisVillageRegistrationResult::InvalidSlots;
	}

	TSet<FName> SeenSlotIds;
	SeenSlotIds.Reserve(Spec.Slots.Num());

	for (const FAnastasisInteractionSlotSpec& Slot : Spec.Slots)
	{
		if (Slot.SlotId.IsNone() || !Slot.ActivityTag.IsValid() || Slot.Capacity < 1)
		{
			return EAnastasisVillageRegistrationResult::InvalidSlots;
		}

		// Une liste de poses partielle est refusee plutot que completee en silence :
		// les occurrences sans pose retomberaient sur LocalOffset, et l'une des
		// places se retrouverait ailleurs que les autres sans que personne l'ait
		// demande.
		if (!Slot.OccurrencePoses.IsEmpty() && Slot.OccurrencePoses.Num() < Slot.Capacity)
		{
			return EAnastasisVillageRegistrationResult::InvalidSlots;
		}

		bool bAlreadySeen = false;
		SeenSlotIds.Add(Slot.SlotId, &bAlreadySeen);
		if (bAlreadySeen)
		{
			// Deux places qui partagent un SlotId rendraient l'adresse ambigue, et
			// donc le chemin retour vers la simulation faux plutot qu'absent.
			return EAnastasisVillageRegistrationResult::DuplicateSlotId;
		}
	}

	return EAnastasisVillageRegistrationResult::Succeeded;
}

// ---------------------------------------------------------------------------
// Enregistrement
// ---------------------------------------------------------------------------

USmartObjectDefinition* UAnastasisVillageSubsystem::BuildDefinition(
	const FAnastasisBuildingSpec& Spec,
	FAnastasisRegisteredBuilding& OutRecord)
{
	USmartObjectDefinition* Definition = NewObject<USmartObjectDefinition>(this, NAME_None, RF_Transient);
	if (Definition == nullptr)
	{
		return nullptr;
	}

	// Explicite plutot qu'herite des reglages projet : un autre chantier peut
	// changer DefaultActivityTagsMergingPolicy dans un .ini, et le routage des
	// intentions du village ne doit pas dependre de ce reglage.
	//
	// Override : les tags d'activite du slot valent seuls. Sans cela, un tag pose
	// au niveau du batiment se combinerait a chaque slot, et demander « ou dormir »
	// ramenerait aussi l'etabli de l'atelier.
	Definition->SetActivityTagsMergingPolicy(ESmartObjectTagMergingPolicy::Override);
	Definition->SetUserTagsFilteringPolicy(ESmartObjectTagFilteringPolicy::Override);

	OutRecord.SlotAddresses.Reset();
	OutRecord.SlotActivities.Reset();

	for (const FAnastasisInteractionSlotSpec& SlotSpec : Spec.Slots)
	{
		// La capacite se deplie ici. Un slot Smart Object ne tient qu'un occupant ;
		// « trois margelles » est donc trois slots, pas un compteur a trois.
		for (int32 Occurrence = 0; Occurrence < SlotSpec.Capacity; ++Occurrence)
		{
			FSmartObjectSlotDefinition& SlotDefinition = Definition->DebugAddSlot();

			const FTransform LocalPose =
				SlotSpec.OccurrencePoses.IsValidIndex(Occurrence)
					? SlotSpec.OccurrencePoses[Occurrence]
					: FTransform(SlotSpec.LocalRotation, SlotSpec.LocalOffset);

			SlotDefinition.Offset = FVector3f(LocalPose.GetLocation());
			SlotDefinition.Rotation = FRotator3f(LocalPose.Rotator());
			SlotDefinition.bEnabled = SlotSpec.bInitiallyEnabled;
			SlotDefinition.ActivityTags = FGameplayTagContainer(SlotSpec.ActivityTag);

			if (!SlotSpec.RequiredUserTags.IsEmpty())
			{
				// MatchAll : l'agent doit porter TOUS les tags exiges. Un lit reserve
				// a un foyer donne ne s'ouvre pas a qui n'en porte qu'une moitie.
				SlotDefinition.UserTagFilter = FGameplayTagQuery::MakeQuery_MatchAllTags(SlotSpec.RequiredUserTags);
			}

			// Sans definition de comportement, MarkSlotAsOccupied renverrait nullptr
			// et laisserait la place en Claimed : reservee, jamais occupee.
			UAnastasisVillageBehavior* Behavior = NewObject<UAnastasisVillageBehavior>(Definition, NAME_None, RF_Transient);
			Behavior->ActivityTag = SlotSpec.ActivityTag;
			Behavior->BuildingId = Spec.BuildingId;
			Behavior->SlotId = SlotSpec.SlotId;
			SlotDefinition.BehaviorDefinitions.Add(Behavior);

#if WITH_EDITORONLY_DATA
			SlotDefinition.Name = FName(*FString::Printf(TEXT("%s#%d"), *SlotSpec.SlotId.ToString(), Occurrence));
#endif

			FAnastasisInteractionAddress& Address = OutRecord.SlotAddresses.AddDefaulted_GetRef();
			Address.BuildingId = Spec.BuildingId;
			Address.SlotId = SlotSpec.SlotId;
			Address.Occurrence = Occurrence;

			OutRecord.SlotActivities.Add(SlotSpec.ActivityTag);
		}
	}

	return Definition;
}

EAnastasisVillageRegistrationResult UAnastasisVillageSubsystem::RegisterBuilding(
	const FAnastasisBuildingSpec& Spec,
	FSmartObjectHandle& OutHandle)
{
	OutHandle.Invalidate();

	const EAnastasisVillageRegistrationResult Validation = ValidateSpec(Spec);
	if (Validation != EAnastasisVillageRegistrationResult::Succeeded)
	{
		return Validation;
	}

	if (BuildingsByIdentifier.Contains(Spec.BuildingId))
	{
		return EAnastasisVillageRegistrationResult::DuplicateBuildingId;
	}

	// Verifie avant de construire la definition : CreateSmartObject declenche un
	// ensure si le runtime n'est pas initialise, et un ensure dans un test est un
	// echec de test, pas un refus propre.
	if (!IsReady())
	{
		return EAnastasisVillageRegistrationResult::SubsystemUnavailable;
	}

	USmartObjectSubsystem* SmartObjects = GetSmartObjects();

	FAnastasisRegisteredBuilding Record;
	Record.BuildingId = Spec.BuildingId;
	Record.BuildingTag = Spec.BuildingTag;
	Record.Transform = Spec.Transform;

	USmartObjectDefinition* Definition = BuildDefinition(Spec, Record);
	if (Definition == nullptr)
	{
		return EAnastasisVillageRegistrationResult::SubsystemUnavailable;
	}
	Record.Definition = Definition;

	const FSmartObjectHandle Handle = SmartObjects->CreateSmartObject(*Definition, Spec.Transform, FConstStructView());
	if (!Handle.IsValid())
	{
		return EAnastasisVillageRegistrationResult::SubsystemUnavailable;
	}

	// La boite de requete devra couvrir ces offsets : voir MaxSlotOffsetLength.
	// On mesure sur la definition construite, pas sur le spec : c'est elle qui porte
	// les poses reellement retenues, occurrences depliees.
	for (const FSmartObjectSlotDefinition& SlotDefinition : Definition->GetSlots())
	{
		const double OffsetLength = Spec.Transform.TransformVector(FVector(SlotDefinition.Offset)).Length();
		MaxSlotOffsetLength = FMath::Max(MaxSlotOffsetLength, OffsetLength);
	}

	Buildings.Add(Handle, MoveTemp(Record));
	BuildingsByIdentifier.Add(Spec.BuildingId, Handle);

	OutHandle = Handle;
	return EAnastasisVillageRegistrationResult::Succeeded;
}

bool UAnastasisVillageSubsystem::UnregisterBuilding(const FSmartObjectHandle BuildingHandle)
{
	const FAnastasisRegisteredBuilding* Record = Buildings.Find(BuildingHandle);
	if (Record == nullptr)
	{
		return false;
	}

	const FName BuildingId = Record->BuildingId;

	if (USmartObjectSubsystem* SmartObjects = GetSmartObjects())
	{
		// Avorte les reservations en cours, retire l'objet de la grille spatiale.
		SmartObjects->DestroySmartObject(BuildingHandle);
	}

	Buildings.Remove(BuildingHandle);
	BuildingsByIdentifier.Remove(BuildingId);
	return true;
}

void UAnastasisVillageSubsystem::UnregisterAllBuildings()
{
	TArray<FSmartObjectHandle> Handles;
	Buildings.GetKeys(Handles);
	for (const FSmartObjectHandle Handle : Handles)
	{
		UnregisterBuilding(Handle);
	}
}

// ---------------------------------------------------------------------------
// Requetes
// ---------------------------------------------------------------------------

bool UAnastasisVillageSubsystem::FindInteractions(
	const FGameplayTag& ActivityTag,
	const FVector& Origin,
	const double Radius,
	const FGameplayTagContainer& UserTags,
	TArray<FAnastasisInteractionQueryResult>& OutResults) const
{
	OutResults.Reset();

	if (!ActivityTag.IsValid() || Radius <= 0.0)
	{
		return false;
	}

	const USmartObjectSubsystem* SmartObjects = GetSmartObjects();
	if (SmartObjects == nullptr || !IsReady())
	{
		return false;
	}

	FSmartObjectRequestFilter Filter;
	Filter.UserTags = UserTags;
	Filter.ClaimPriority = VillageClaimPriority;
	// MatchAny sur un seul tag : le slot doit porter ce tag, ou l'un de ses
	// descendants -- demander Activity.Storage trouve Deposit comme Take.
	Filter.ActivityRequirements = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(ActivityTag));
	Filter.BehaviorDefinitionClasses.Add(UAnastasisVillageBehavior::StaticClass());
	Filter.bShouldIncludeClaimedSlots = false;
	Filter.bShouldIncludeDisabledSlots = false;

	// Le prefiltre moteur teste l'ORIGINE du batiment contre cette boite. Elargie
	// du plus grand offset de slot connu, elle devient un sur-ensemble strict de
	// la sphere (Origin, Radius) mesuree sur les places.
	const double QueryExtent = Radius + MaxSlotOffsetLength;
	const FSmartObjectRequest Request(FBox::BuildAABB(Origin, FVector(QueryExtent)), Filter);

	TArray<FSmartObjectRequestResult> EngineResults;
	SmartObjects->FindSmartObjects(Request, EngineResults, FConstStructView());

	const double RadiusSq = Radius * Radius;

	for (const FSmartObjectRequestResult& EngineResult : EngineResults)
	{
		const FAnastasisRegisteredBuilding* Record = Buildings.Find(EngineResult.SmartObjectHandle);
		if (Record == nullptr)
		{
			// Un Smart Object du monde qui n'est pas un batiment du village : un
			// autre chantier peut en enregistrer. Le filtre par classe de
			// comportement devrait deja l'avoir ecarte ; on ne s'y fie pas seul.
			continue;
		}

		const int32 SlotIndex = EngineResult.SlotHandle.GetSlotIndex();
		if (!Record->SlotAddresses.IsValidIndex(SlotIndex))
		{
			continue;
		}

		const TOptional<FTransform> SlotTransform = SmartObjects->GetSlotTransform(EngineResult.SlotHandle);
		if (!SlotTransform.IsSet())
		{
			continue;
		}

		// Le test exact, sur la place elle-meme.
		const double DistanceSq = FVector::DistSquared(Origin, SlotTransform->GetLocation());
		if (DistanceSq > RadiusSq)
		{
			continue;
		}

		FAnastasisInteractionQueryResult& Out = OutResults.AddDefaulted_GetRef();
		Out.SmartObjectHandle = EngineResult.SmartObjectHandle;
		Out.SlotHandle = EngineResult.SlotHandle;
		Out.Address = Record->SlotAddresses[SlotIndex];
		Out.ActivityTag = Record->SlotActivities[SlotIndex];
		Out.BuildingTag = Record->BuildingTag;
		Out.SlotTransform = SlotTransform.GetValue();
		Out.Distance = FMath::Sqrt(DistanceSq);
	}

	// Ordre total. La grille spatiale n'ordonne pas ses cellules de facon garantie,
	// et deux executions de la meme graine doivent choisir le meme lit.
	OutResults.Sort([](const FAnastasisInteractionQueryResult& A, const FAnastasisInteractionQueryResult& B)
	{
		if (A.Distance != B.Distance)
		{
			return A.Distance < B.Distance;
		}
		if (A.Address.BuildingId != B.Address.BuildingId)
		{
			return A.Address.BuildingId.LexicalLess(B.Address.BuildingId);
		}
		if (A.Address.SlotId != B.Address.SlotId)
		{
			return A.Address.SlotId.LexicalLess(B.Address.SlotId);
		}
		return A.Address.Occurrence < B.Address.Occurrence;
	});

	return !OutResults.IsEmpty();
}

bool UAnastasisVillageSubsystem::FindNearestInteraction(
	const FGameplayTag& ActivityTag,
	const FVector& Origin,
	const double Radius,
	const FGameplayTagContainer& UserTags,
	FAnastasisInteractionQueryResult& OutResult) const
{
	OutResult = FAnastasisInteractionQueryResult();

	TArray<FAnastasisInteractionQueryResult> Results;
	if (!FindInteractions(ActivityTag, Origin, Radius, UserTags, Results))
	{
		return false;
	}

	OutResult = Results[0];
	return true;
}

// ---------------------------------------------------------------------------
// Reservation
// ---------------------------------------------------------------------------

bool UAnastasisVillageSubsystem::ClaimInteraction(
	const FAnastasisInteractionQueryResult& Query,
	FAnastasisInteractionClaim& OutClaim)
{
	OutClaim = FAnastasisInteractionClaim();

	if (!Query.IsValid())
	{
		return false;
	}

	USmartObjectSubsystem* SmartObjects = GetSmartObjects();
	if (SmartObjects == nullptr)
	{
		return false;
	}

	// Le batiment a pu disparaitre entre la requete et la reservation : un village
	// genere se reconfigure, et un resultat de requete n'est pas un bail.
	if (!Buildings.Contains(Query.SmartObjectHandle))
	{
		return false;
	}

	const FSmartObjectClaimHandle ClaimHandle =
		SmartObjects->MarkSlotAsClaimed(Query.SlotHandle, VillageClaimPriority, FConstStructView());

	if (!ClaimHandle.IsValid())
	{
		// Place deja tenue a priorite egale, ou desactivee depuis la requete.
		return false;
	}

	OutClaim.ClaimHandle = ClaimHandle;
	OutClaim.Address = Query.Address;
	OutClaim.ActivityTag = Query.ActivityTag;
	OutClaim.SlotTransform = Query.SlotTransform;
	return true;
}

bool UAnastasisVillageSubsystem::BeginInteraction(const FAnastasisInteractionClaim& Claim)
{
	if (!Claim.IsValid())
	{
		return false;
	}

	USmartObjectSubsystem* SmartObjects = GetSmartObjects();
	if (SmartObjects == nullptr)
	{
		return false;
	}

	return SmartObjects->MarkSlotAsOccupied(Claim.ClaimHandle, UAnastasisVillageBehavior::StaticClass()) != nullptr;
}

bool UAnastasisVillageSubsystem::ReleaseInteraction(FAnastasisInteractionClaim& Claim)
{
	if (!Claim.IsValid())
	{
		return false;
	}

	USmartObjectSubsystem* SmartObjects = GetSmartObjects();
	if (SmartObjects == nullptr)
	{
		Claim.Invalidate();
		return false;
	}

	const bool bReleased = SmartObjects->MarkSlotAsFree(Claim.ClaimHandle);

	// Invalide dans les deux cas. Si le relachement a echoue, c'est que le
	// batiment a ete detruit et que le moteur a deja avorte la reservation :
	// garder le claim vivant ne ferait qu'inviter un second essai tout aussi vain.
	Claim.Invalidate();
	return bReleased;
}

// ---------------------------------------------------------------------------
// Lecture
// ---------------------------------------------------------------------------

bool UAnastasisVillageSubsystem::ResolveAddress(
	const FSmartObjectSlotHandle& SlotHandle,
	FAnastasisInteractionAddress& OutAddress) const
{
	OutAddress = FAnastasisInteractionAddress();

	if (!SlotHandle.IsValid())
	{
		return false;
	}

	const FAnastasisRegisteredBuilding* Record = Buildings.Find(SlotHandle.GetSmartObjectHandle());
	if (Record == nullptr || !Record->SlotAddresses.IsValidIndex(SlotHandle.GetSlotIndex()))
	{
		return false;
	}

	OutAddress = Record->SlotAddresses[SlotHandle.GetSlotIndex()];
	return true;
}

bool UAnastasisVillageSubsystem::FindBuildingByIdentifier(const FName BuildingId, FSmartObjectHandle& OutHandle) const
{
	OutHandle.Invalidate();

	if (const FSmartObjectHandle* Found = BuildingsByIdentifier.Find(BuildingId))
	{
		OutHandle = *Found;
		return true;
	}
	return false;
}

int32 UAnastasisVillageSubsystem::GetRegisteredSlotCount() const
{
	int32 Count = 0;
	for (const TPair<FSmartObjectHandle, FAnastasisRegisteredBuilding>& Pair : Buildings)
	{
		Count += Pair.Value.SlotAddresses.Num();
	}
	return Count;
}

ESmartObjectSlotState UAnastasisVillageSubsystem::GetSlotState(const FSmartObjectSlotHandle& SlotHandle) const
{
	const USmartObjectSubsystem* SmartObjects = GetSmartObjects();
	if (SmartObjects == nullptr || !SlotHandle.IsValid())
	{
		return ESmartObjectSlotState::Invalid;
	}
	return SmartObjects->GetSlotState(SlotHandle);
}

void UAnastasisVillageSubsystem::DrawDebugInteractions(const float Duration) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	const USmartObjectSubsystem* SmartObjects = GetSmartObjects();
	if (World == nullptr || SmartObjects == nullptr)
	{
		return;
	}

	for (const TPair<FSmartObjectHandle, FAnastasisRegisteredBuilding>& Pair : Buildings)
	{
		const FAnastasisRegisteredBuilding& Record = Pair.Value;

		DrawDebugBox(World, Record.Transform.GetLocation(), FVector(60.0), FColor::White, false, Duration);
		DrawDebugString(
			World,
			Record.Transform.GetLocation() + FVector(0.0, 0.0, 90.0),
			FString::Printf(TEXT("%s [%s]"), *Record.BuildingId.ToString(), *Record.BuildingTag.ToString()),
			nullptr, FColor::White, Duration);

		for (int32 SlotIndex = 0; SlotIndex < Record.SlotAddresses.Num(); ++SlotIndex)
		{
			// Aucun constructeur public de FSmartObjectSlotHandle depuis un index :
			// on recalcule la pose depuis la definition, ce qui suffit au debug.
			const FTransform SlotTransform =
				Record.Definition != nullptr
					? Record.Definition->GetSlotWorldTransform(SlotIndex, Record.Transform)
					: Record.Transform;

			DrawDebugSphere(World, SlotTransform.GetLocation(), 25.0f, 8, FColor::Cyan, false, Duration);
			DrawDebugDirectionalArrow(
				World,
				SlotTransform.GetLocation(),
				SlotTransform.GetLocation() + SlotTransform.GetRotation().GetForwardVector() * 70.0,
				20.0f, FColor::Cyan, false, Duration);
			DrawDebugString(
				World,
				SlotTransform.GetLocation() + FVector(0.0, 0.0, 40.0),
				FString::Printf(TEXT("%s\n%s"),
					*Record.SlotAddresses[SlotIndex].ToString(),
					*Record.SlotActivities[SlotIndex].ToString()),
				nullptr, FColor::Cyan, Duration);
		}
	}
#endif // ENABLE_DRAW_DEBUG
}
