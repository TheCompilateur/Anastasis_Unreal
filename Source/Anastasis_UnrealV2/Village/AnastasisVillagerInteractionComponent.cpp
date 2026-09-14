#include "Village/AnastasisVillagerInteractionComponent.h"

#include "GameFramework/Actor.h"
#include "Village/AnastasisVillageSubsystem.h"

const TCHAR* LexToString(const EAnastasisInteractionPhase Phase)
{
	switch (Phase)
	{
	case EAnastasisInteractionPhase::Idle:     return TEXT("Idle");
	case EAnastasisInteractionPhase::Reserved: return TEXT("Reserved");
	case EAnastasisInteractionPhase::InUse:    return TEXT("InUse");
	case EAnastasisInteractionPhase::Complete: return TEXT("Complete");
	case EAnastasisInteractionPhase::Failed:   return TEXT("Failed");
	}
	return TEXT("Unknown");
}

UAnastasisVillagerInteractionComponent::UAnastasisVillagerInteractionComponent()
{
	// Rien a ticker : la sequence avance sur appel, pas sur horloge. Un composant
	// qui tickerait finirait par vouloir decider quand chercher une place.
	PrimaryComponentTick.bCanEverTick = false;
}

UAnastasisVillageSubsystem* UAnastasisVillagerInteractionComponent::GetVillage() const
{
	return UAnastasisVillageSubsystem::Get(GetWorld());
}

void UAnastasisVillagerInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Un villageois qui disparait en tenant un lit le tiendrait jusqu'a la fin de la
	// partie : le moteur n'a aucun moyen de savoir que son occupant n'existe plus.
	AbandonInteraction();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Intention
// ---------------------------------------------------------------------------

void UAnastasisVillagerInteractionComponent::SetIntent(const FGameplayTag ActivityTag, const double SearchRadius)
{
	PendingActivity = ActivityTag;
	PendingSearchRadius = SearchRadius;
}

void UAnastasisVillagerInteractionComponent::ClearIntent()
{
	PendingActivity = FGameplayTag();
	PendingSearchRadius = 0.0;
}

// ---------------------------------------------------------------------------
// Sequence
// ---------------------------------------------------------------------------

bool UAnastasisVillagerInteractionComponent::RequestInteraction()
{
	if (!PendingActivity.IsValid() || PendingSearchRadius <= 0.0)
	{
		Phase = EAnastasisInteractionPhase::Failed;
		return false;
	}

	const AActor* Owner = GetOwner();
	UAnastasisVillageSubsystem* Village = GetVillage();
	if (Owner == nullptr || Village == nullptr)
	{
		Phase = EAnastasisInteractionPhase::Failed;
		return false;
	}

	// Une place encore tenue serait perdue. Relacher avant de chercher.
	if (Claim.IsValid())
	{
		Village->ReleaseInteraction(Claim);
	}

	const FVector Origin = Owner->GetActorLocation();

	// On parcourt les candidats dans l'ordre de proximite : la place la plus proche
	// peut avoir ete prise entre la requete et la reservation -- un autre agent, le
	// meme tick -- et abandonner au premier refus rendrait la concurrence fatale
	// alors qu'une place voisine est libre.
	TArray<FAnastasisInteractionQueryResult> Candidates;
	if (!Village->FindInteractions(PendingActivity, Origin, PendingSearchRadius, UserTags, Candidates))
	{
		Phase = EAnastasisInteractionPhase::Failed;
		return false;
	}

	for (const FAnastasisInteractionQueryResult& Candidate : Candidates)
	{
		if (Village->ClaimInteraction(Candidate, Claim))
		{
			Phase = EAnastasisInteractionPhase::Reserved;
			return true;
		}
	}

	Phase = EAnastasisInteractionPhase::Failed;
	return false;
}

bool UAnastasisVillagerInteractionComponent::RequestInteractionFor(const FGameplayTag ActivityTag, const double SearchRadius)
{
	SetIntent(ActivityTag, SearchRadius);
	return RequestInteraction();
}

bool UAnastasisVillagerInteractionComponent::HasArrived() const
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Claim.IsValid())
	{
		return false;
	}

	return FVector::DistSquared(Owner->GetActorLocation(), Claim.SlotTransform.GetLocation())
		<= ArrivalTolerance * ArrivalTolerance;
}

bool UAnastasisVillagerInteractionComponent::BeginUse()
{
	if (Phase != EAnastasisInteractionPhase::Reserved || !Claim.IsValid())
	{
		return false;
	}

	if (!HasArrived())
	{
		return false;
	}

	UAnastasisVillageSubsystem* Village = GetVillage();
	if (Village == nullptr || !Village->BeginInteraction(Claim))
	{
		// Le batiment a pu etre detruit pendant le trajet. La place n'existe plus.
		Claim.Invalidate();
		Phase = EAnastasisInteractionPhase::Failed;
		return false;
	}

	Phase = EAnastasisInteractionPhase::InUse;
	return true;
}

bool UAnastasisVillagerInteractionComponent::CompleteAndRelease()
{
	if (!Claim.IsValid())
	{
		return false;
	}

	UAnastasisVillageSubsystem* Village = GetVillage();
	const bool bReleased = Village != nullptr && Village->ReleaseInteraction(Claim);

	// L'intention est consommee meme si le relachement a echoue : l'acte a eu lieu,
	// et une intention qui survivrait ferait boucler l'agent sur un lieu disparu.
	ClearIntent();
	Claim.Invalidate();
	Phase = EAnastasisInteractionPhase::Complete;
	return bReleased;
}

void UAnastasisVillagerInteractionComponent::AbandonInteraction()
{
	if (Claim.IsValid())
	{
		if (UAnastasisVillageSubsystem* Village = GetVillage())
		{
			Village->ReleaseInteraction(Claim);
		}
		Claim.Invalidate();
	}

	// L'intention reste : abandonner un trajet ne veut pas dire ne plus vouloir dormir.
	Phase = EAnastasisInteractionPhase::Idle;
}
