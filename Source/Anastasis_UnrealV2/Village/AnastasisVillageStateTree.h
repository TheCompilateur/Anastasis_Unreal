#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectTypes.h"
#include "StateTreeTaskBase.h"
#include "AnastasisVillageStateTree.generated.h"

/**
 * Pont StateTree : orchestre Find → MoveTo → Use → Release.
 * Ne decide pas POURQUOI — l'ActivityTag arrive de la sim.
 */

USTRUCT()
struct FAnastasisFindSmartObjectInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FGameplayTag ActivityTag;

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Input")
	float Radius = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Output")
	FSmartObjectSlotHandle SlotHandle;

	UPROPERTY(EditAnywhere, Category = "Output")
	FVector SlotLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Output")
	FName SimId;
};

USTRUCT(DisplayName = "Anastasis Find Smart Object", Category = "Anastasis|Village")
struct FAnastasisFindSmartObjectTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAnastasisFindSmartObjectInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FAnastasisMoveToSlotInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector SlotLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Output")
	FVector ArrivedAt = FVector::ZeroVector;
};

USTRUCT(DisplayName = "Anastasis Move To Slot", Category = "Anastasis|Village")
struct FAnastasisMoveToSlotTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAnastasisMoveToSlotInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FAnastasisUseSmartObjectInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FSmartObjectSlotHandle SlotHandle;

	UPROPERTY(EditAnywhere, Category = "Output")
	FSmartObjectClaimHandle ClaimHandle;
};

USTRUCT(DisplayName = "Anastasis Use Smart Object", Category = "Anastasis|Village")
struct FAnastasisUseSmartObjectTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAnastasisUseSmartObjectInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FAnastasisReleaseSmartObjectInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FSmartObjectClaimHandle ClaimHandle;
};

USTRUCT(DisplayName = "Anastasis Release Smart Object", Category = "Anastasis|Village")
struct FAnastasisReleaseSmartObjectTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAnastasisReleaseSmartObjectInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
