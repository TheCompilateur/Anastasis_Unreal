#include "Village/AnastasisVillageStateTree.h"

#include "Engine/World.h"
#include "StateTreeExecutionContext.h"
#include "Village/AnastasisVillageInteractionSubsystem.h"

EStateTreeRunStatus FAnastasisFindSmartObjectTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	UWorld* World = Context.GetWorld();
	UAnastasisVillageInteractionSubsystem* Village = World
		? World->GetSubsystem<UAnastasisVillageInteractionSubsystem>()
		: nullptr;
	if (!Village)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FAnastasisVillageQueryResult Found = Village->FindNearestInteraction(
		Data.ActivityTag,
		Data.Origin,
		Data.Radius);
	if (!Found.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.SlotHandle = Found.Request.SlotHandle;
	Data.SlotLocation = Found.SlotLocation;
	Data.SimId = Found.SimId;
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FAnastasisMoveToSlotTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ArrivedAt = Data.SlotLocation;
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FAnastasisUseSmartObjectTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	UWorld* World = Context.GetWorld();
	UAnastasisVillageInteractionSubsystem* Village = World
		? World->GetSubsystem<UAnastasisVillageInteractionSubsystem>()
		: nullptr;
	if (!Village)
	{
		return EStateTreeRunStatus::Failed;
	}

	Data.ClaimHandle = Village->Claim(Data.SlotHandle);
	if (!Data.ClaimHandle.IsValid() || !Village->Use(Data.ClaimHandle))
	{
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FAnastasisReleaseSmartObjectTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult&) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	UWorld* World = Context.GetWorld();
	UAnastasisVillageInteractionSubsystem* Village = World
		? World->GetSubsystem<UAnastasisVillageInteractionSubsystem>()
		: nullptr;
	if (!Village || !Village->Release(Data.ClaimHandle))
	{
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Succeeded;
}
