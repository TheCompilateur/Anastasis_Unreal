#include "Village/AnastasisVillageStateTreeTasks.h"

#include "StateTreeExecutionContext.h"
#include "Village/AnastasisVillagerInteractionComponent.h"

bool FAnastasisHasInteractionIntentCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const UAnastasisVillagerInteractionComponent* Component = InstanceData.InteractionComponent;
	if (!IsValid(Component) || !Component->HasPendingIntent())
	{
		return false;
	}

	if (!InstanceData.RequiredActivity.IsValid())
	{
		return true;
	}

	// MatchesTag et non l'egalite : un etat qui attend Activity.Storage doit
	// s'activer sur une intention Activity.Storage.Take.
	return Component->GetPendingActivity().MatchesTag(InstanceData.RequiredActivity);
}

EStateTreeRunStatus FAnastasisFindInteractionTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UAnastasisVillagerInteractionComponent* Component = InstanceData.InteractionComponent;
	if (!IsValid(Component))
	{
		return EStateTreeRunStatus::Failed;
	}

	const bool bReserved = InstanceData.ActivityOverride.IsValid()
		? Component->RequestInteractionFor(InstanceData.ActivityOverride, InstanceData.SearchRadius)
		: Component->RequestInteraction();

	if (!bReserved)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Publie la pose pour la tache de deplacement en aval.
	const FTransform SlotTransform = Component->GetInteractionTransform();
	InstanceData.InteractionLocation = SlotTransform.GetLocation();
	InstanceData.InteractionRotation = SlotTransform.Rotator();

	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FAnastasisUseInteractionTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UAnastasisVillagerInteractionComponent* Component = InstanceData.InteractionComponent;
	if (!IsValid(Component))
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Elapsed = 0.0f;

	// Refuse si l'agent n'est pas arrive : c'est le composant qui tient cette garde,
	// et l'arbre en herite plutot que de la redupliquer.
	if (!Component->BeginUse())
	{
		return EStateTreeRunStatus::Failed;
	}

	// Une duree nulle est un acte instantane, pas une erreur : deposer un objet
	// n'a pas besoin d'une seconde.
	return InstanceData.Duration > 0.0f ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FAnastasisUseInteractionTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const UAnastasisVillagerInteractionComponent* Component = InstanceData.InteractionComponent;
	if (!IsValid(Component) || Component->GetPhase() != EAnastasisInteractionPhase::InUse)
	{
		// Le batiment a disparu sous l'occupant, ou la place a ete avortee.
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.Elapsed += DeltaTime;
	return InstanceData.Elapsed >= InstanceData.Duration
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FAnastasisReleaseInteractionTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UAnastasisVillagerInteractionComponent* Component = InstanceData.InteractionComponent;
	if (!IsValid(Component))
	{
		return EStateTreeRunStatus::Failed;
	}

	// Lue avant le relachement : apres, le claim est invalide et l'adresse perdue.
	const FAnastasisInteractionAddress Address = Component->GetInteractionAddress();
	InstanceData.BuildingId = Address.BuildingId;
	InstanceData.SlotId = Address.SlotId;

	return Component->CompleteAndRelease() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
