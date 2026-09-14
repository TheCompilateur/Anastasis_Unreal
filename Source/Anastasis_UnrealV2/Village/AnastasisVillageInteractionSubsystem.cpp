#include "Village/AnastasisVillageInteractionSubsystem.h"

#include "Engine/World.h"
#include "SmartObjectComponent.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectRequestTypes.h"
#include "SmartObjectSubsystem.h"
#include "Village/AnastasisVillageTags.h"

bool UAnastasisVillageInteractionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->IsGameWorld() || World->WorldType == EWorldType::Editor);
}

void UAnastasisVillageInteractionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	HouseDef = MakeDefinition(EAnastasisVillageBuildingKind::House);
	WellDef = MakeDefinition(EAnastasisVillageBuildingKind::Well);
	WorkshopDef = MakeDefinition(EAnastasisVillageBuildingKind::Workshop);
}

FGameplayTag UAnastasisVillageInteractionSubsystem::ActivityTagFor(EAnastasisVillageBuildingKind Kind)
{
	switch (Kind)
	{
	case EAnastasisVillageBuildingKind::House: return TAG_Anastasis_Activity_Sleep;
	case EAnastasisVillageBuildingKind::Well: return TAG_Anastasis_Activity_Drink;
	case EAnastasisVillageBuildingKind::Workshop: return TAG_Anastasis_Activity_Work;
	}
	return TAG_Anastasis_Activity_Sleep;
}

FGameplayTag UAnastasisVillageInteractionSubsystem::BuildingTagFor(EAnastasisVillageBuildingKind Kind)
{
	switch (Kind)
	{
	case EAnastasisVillageBuildingKind::House: return TAG_Anastasis_Building_House;
	case EAnastasisVillageBuildingKind::Well: return TAG_Anastasis_Building_Well;
	case EAnastasisVillageBuildingKind::Workshop: return TAG_Anastasis_Building_Workshop;
	}
	return TAG_Anastasis_Building_House;
}

USmartObjectDefinition* UAnastasisVillageInteractionSubsystem::DefinitionFor(EAnastasisVillageBuildingKind Kind) const
{
	switch (Kind)
	{
	case EAnastasisVillageBuildingKind::House: return HouseDef;
	case EAnastasisVillageBuildingKind::Well: return WellDef;
	case EAnastasisVillageBuildingKind::Workshop: return WorkshopDef;
	}
	return HouseDef;
}

void UAnastasisVillageInteractionSubsystem::AddSlot(
	USmartObjectDefinition* Def,
	FGameplayTag Activity,
	const FVector3f& Offset)
{
	FSmartObjectSlotDefinition& Slot = Def->DebugAddSlot();
	Slot.Offset = Offset;
	Slot.bEnabled = true;
	Slot.ActivityTags.AddTag(Activity);
	UAnastasisVillageBehaviorDefinition* Behavior = NewObject<UAnastasisVillageBehaviorDefinition>(Def);
	Slot.BehaviorDefinitions.Add(Behavior);
}

USmartObjectDefinition* UAnastasisVillageInteractionSubsystem::MakeDefinition(EAnastasisVillageBuildingKind Kind)
{
	USmartObjectDefinition* Def = NewObject<USmartObjectDefinition>(this);
	const FGameplayTag Activity = ActivityTagFor(Kind);
	const FGameplayTag Building = BuildingTagFor(Kind);
	Def->SetActivityTags(FGameplayTagContainer(Building));
	Def->SetActivityTagsMergingPolicy(ESmartObjectTagMergingPolicy::Override);

	if (Kind == EAnastasisVillageBuildingKind::House)
	{
		AddSlot(Def, Activity, FVector3f(80.f, 40.f, 0.f));
		AddSlot(Def, Activity, FVector3f(80.f, -40.f, 0.f));
	}
	else
	{
		AddSlot(Def, Activity, FVector3f(80.f, 0.f, 0.f));
	}
	return Def;
}

AAnastasisVillageBuilding* UAnastasisVillageInteractionSubsystem::SpawnBuilding(
	EAnastasisVillageBuildingKind Kind,
	FName SimId,
	const FTransform& Transform)
{
	UWorld* World = GetWorld();
	USmartObjectDefinition* Def = DefinitionFor(Kind);
	if (!World || !Def)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAnastasisVillageBuilding* Building = World->SpawnActor<AAnastasisVillageBuilding>(
		AAnastasisVillageBuilding::StaticClass(),
		Transform,
		Params);
	if (!Building)
	{
		return nullptr;
	}

	Building->Configure(Kind, SimId, Def);

	if (USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(World))
	{
		SmartObjects->RegisterSmartObjectActor(*Building);
	}
	return Building;
}

bool UAnastasisVillageInteractionSubsystem::DestroyBuilding(AAnastasisVillageBuilding* Building)
{
	if (!Building)
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(World))
	{
		SmartObjects->UnregisterSmartObjectActor(*Building);
	}
	Building->Destroy();
	return true;
}

FAnastasisVillageQueryResult UAnastasisVillageInteractionSubsystem::FindNearestInteraction(
	FGameplayTag ActivityTag,
	const FVector& Origin,
	float Radius) const
{
	FAnastasisVillageQueryResult Best;
	UWorld* World = GetWorld();
	USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(World);
	if (!SmartObjects || !ActivityTag.IsValid() || Radius <= 0.f)
	{
		return Best;
	}

	FSmartObjectRequestFilter Filter;
	Filter.ActivityRequirements = FGameplayTagQuery::MakeQuery_MatchTag(ActivityTag);

	FSmartObjectRequest Request;
	Request.QueryBox = FBox(Origin - FVector(Radius), Origin + FVector(Radius));
	Request.Filter = Filter;

	TArray<FSmartObjectRequestResult> Hits;
	SmartObjects->FindSmartObjects(Request, Hits, nullptr);

	double BestDistSq = TNumericLimits<double>::Max();
	for (const FSmartObjectRequestResult& Hit : Hits)
	{
		if (!Hit.IsValid())
		{
			continue;
		}
		const TOptional<FVector> SlotLocation = SmartObjects->GetSlotLocation(Hit.SlotHandle);
		if (!SlotLocation.IsSet())
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Origin, SlotLocation.GetValue());
		if (DistSq > static_cast<double>(Radius) * static_cast<double>(Radius) || DistSq >= BestDistSq)
		{
			continue;
		}

		USmartObjectComponent* Component = SmartObjects->GetSmartObjectComponentByRequestResult(Hit);
		AAnastasisVillageBuilding* Building = Component
			? Cast<AAnastasisVillageBuilding>(Component->GetOwner())
			: nullptr;
		if (!Building)
		{
			continue;
		}

		BestDistSq = DistSq;
		Best.Request = Hit;
		Best.SimId = Building->GetSimId();
		Best.Kind = Building->GetKind();
		Best.SlotLocation = SlotLocation.GetValue();
	}
	return Best;
}

FSmartObjectClaimHandle UAnastasisVillageInteractionSubsystem::Claim(const FSmartObjectSlotHandle& SlotHandle)
{
	USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (!SmartObjects || !SlotHandle.IsValid())
	{
		return FSmartObjectClaimHandle::InvalidHandle;
	}
	return SmartObjects->MarkSlotAsClaimed(SlotHandle, ESmartObjectClaimPriority::Normal);
}

bool UAnastasisVillageInteractionSubsystem::Release(const FSmartObjectClaimHandle& ClaimHandle)
{
	USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(GetWorld());
	return SmartObjects && SmartObjects->MarkSlotAsFree(ClaimHandle);
}

bool UAnastasisVillageInteractionSubsystem::Use(const FSmartObjectClaimHandle& ClaimHandle)
{
	USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(GetWorld());
	if (!SmartObjects)
	{
		return false;
	}
	return SmartObjects->MarkSlotAsOccupied(
		ClaimHandle,
		UAnastasisVillageBehaviorDefinition::StaticClass()) != nullptr;
}
