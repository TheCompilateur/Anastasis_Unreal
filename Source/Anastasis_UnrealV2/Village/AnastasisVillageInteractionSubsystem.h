#pragma once

#include "CoreMinimal.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Village/AnastasisVillageBuilding.h"
#include "AnastasisVillageInteractionSubsystem.generated.h"

class USmartObjectDefinition;
class USmartObjectSubsystem;

UCLASS()
class UAnastasisVillageBehaviorDefinition : public USmartObjectBehaviorDefinition
{
	GENERATED_BODY()
};

struct FAnastasisVillageQueryResult
{
	FSmartObjectRequestResult Request;
	FName SimId;
	EAnastasisVillageBuildingKind Kind = EAnastasisVillageBuildingKind::House;
	FVector SlotLocation = FVector::ZeroVector;

	bool IsValid() const { return Request.IsValid(); }
};

/**
 * Pompe village : spawn dynamique → Smart Object → query / claim / release.
 * N'ecrit aucune regle de sim. L'intention (Activity tag) vient de dehors.
 */
UCLASS()
class UAnastasisVillageInteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	AAnastasisVillageBuilding* SpawnBuilding(
		EAnastasisVillageBuildingKind Kind,
		FName SimId,
		const FTransform& Transform);

	bool DestroyBuilding(AAnastasisVillageBuilding* Building);

	FAnastasisVillageQueryResult FindNearestInteraction(
		FGameplayTag ActivityTag,
		const FVector& Origin,
		float Radius) const;

	FSmartObjectClaimHandle Claim(const FSmartObjectSlotHandle& SlotHandle);
	bool Release(const FSmartObjectClaimHandle& ClaimHandle);
	bool Use(const FSmartObjectClaimHandle& ClaimHandle);

	static FGameplayTag ActivityTagFor(EAnastasisVillageBuildingKind Kind);
	static FGameplayTag BuildingTagFor(EAnastasisVillageBuildingKind Kind);

private:
	USmartObjectDefinition* DefinitionFor(EAnastasisVillageBuildingKind Kind) const;
	USmartObjectDefinition* MakeDefinition(EAnastasisVillageBuildingKind Kind);
	void AddSlot(USmartObjectDefinition* Def, FGameplayTag Activity, const FVector3f& Offset);

	UPROPERTY()
	TObjectPtr<USmartObjectDefinition> HouseDef;

	UPROPERTY()
	TObjectPtr<USmartObjectDefinition> WellDef;

	UPROPERTY()
	TObjectPtr<USmartObjectDefinition> WorkshopDef;
};
