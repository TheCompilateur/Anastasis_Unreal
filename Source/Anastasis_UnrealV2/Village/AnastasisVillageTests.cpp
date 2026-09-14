#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "SmartObjectComponent.h"
#include "SmartObjectSubsystem.h"
#include "Village/AnastasisVillageInteractionSubsystem.h"
#include "Village/AnastasisVillageTags.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UWorld* FindVillageWorld()
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageTagsExistTest,
	"Anastasis.Village.TagsExist",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageTagsExistTest::RunTest(const FString&)
{
	TestTrue(TEXT("Activity.Sleep"), static_cast<FGameplayTag>(TAG_Anastasis_Activity_Sleep).IsValid());
	TestTrue(TEXT("Activity.Drink"), static_cast<FGameplayTag>(TAG_Anastasis_Activity_Drink).IsValid());
	TestTrue(TEXT("Activity.Work"), static_cast<FGameplayTag>(TAG_Anastasis_Activity_Work).IsValid());
	TestTrue(TEXT("Building.House"), static_cast<FGameplayTag>(TAG_Anastasis_Building_House).IsValid());
	TestTrue(TEXT("Building.Well"), static_cast<FGameplayTag>(TAG_Anastasis_Building_Well).IsValid());
	TestTrue(TEXT("Building.Workshop"), static_cast<FGameplayTag>(TAG_Anastasis_Building_Workshop).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageReservationTest,
	"Anastasis.Village.Reservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageReservationTest::RunTest(const FString&)
{
	UWorld* World = FindVillageWorld();
	if (!World)
	{
		AddError(TEXT("no editor/game world"));
		return false;
	}

	UAnastasisVillageInteractionSubsystem* Village =
		World->GetSubsystem<UAnastasisVillageInteractionSubsystem>();
	USmartObjectSubsystem* SmartObjects = USmartObjectSubsystem::GetCurrent(World);
	if (!Village || !SmartObjects)
	{
		AddError(TEXT("village or SmartObject subsystem missing — plugin not loaded?"));
		return false;
	}

	AAnastasisVillageBuilding* House = Village->SpawnBuilding(
		EAnastasisVillageBuildingKind::House,
		FName(TEXT("sim.house.1")),
		FTransform(FVector(0.0, 0.0, 0.0)));
	AAnastasisVillageBuilding* Well = Village->SpawnBuilding(
		EAnastasisVillageBuildingKind::Well,
		FName(TEXT("sim.well.1")),
		FTransform(FVector(600.0, 0.0, 0.0)));
	AAnastasisVillageBuilding* Workshop = Village->SpawnBuilding(
		EAnastasisVillageBuildingKind::Workshop,
		FName(TEXT("sim.workshop.1")),
		FTransform(FVector(0.0, 600.0, 0.0)));

	const bool bSpawned = House && Well && Workshop;
	TestTrue(TEXT("BUILDING SPAWN → 3 buildings"), bSpawned);
	if (!bSpawned)
	{
		return false;
	}

	TestTrue(
		TEXT("SMART OBJECT REGISTERED house"),
		House->GetSmartObject() && House->GetSmartObject()->GetRegisteredHandle().IsValid());

	const FAnastasisVillageQueryResult SleepNear = Village->FindNearestInteraction(
		TAG_Anastasis_Activity_Sleep,
		FVector::ZeroVector,
		400.f);
	TestTrue(TEXT("QUERY → SLOT FOUND sleep"), SleepNear.IsValid());
	TestEqual(TEXT("sleep maps to sim.house.1"), SleepNear.SimId, FName(TEXT("sim.house.1")));

	const FAnastasisVillageQueryResult DrinkFar = Village->FindNearestInteraction(
		TAG_Anastasis_Activity_Drink,
		FVector::ZeroVector,
		200.f);
	TestTrue(TEXT("spatial miss drink at 200"), !DrinkFar.IsValid());

	const FAnastasisVillageQueryResult DrinkNear = Village->FindNearestInteraction(
		TAG_Anastasis_Activity_Drink,
		FVector(600.0, 0.0, 0.0),
		200.f);
	TestTrue(TEXT("QUERY drink at well"), DrinkNear.IsValid());
	TestEqual(TEXT("drink maps to sim.well.1"), DrinkNear.SimId, FName(TEXT("sim.well.1")));

	const FAnastasisVillageQueryResult WorkNear = Village->FindNearestInteraction(
		TAG_Anastasis_Activity_Work,
		FVector(0.0, 600.0, 0.0),
		200.f);
	TestTrue(TEXT("QUERY work at workshop"), WorkNear.IsValid());

	const FSmartObjectClaimHandle ClaimA = Village->Claim(SleepNear.Request.SlotHandle);
	TestTrue(TEXT("AGENT A CLAIM → SUCCESS"), ClaimA.IsValid());

	const FSmartObjectClaimHandle ClaimBSame = Village->Claim(SleepNear.Request.SlotHandle);
	TestTrue(TEXT("AGENT B SAME SLOT → REJECTED"), !ClaimBSame.IsValid());

	const FAnastasisVillageQueryResult SleepAlt = Village->FindNearestInteraction(
		TAG_Anastasis_Activity_Sleep,
		FVector::ZeroVector,
		400.f);
	TestTrue(TEXT("AGENT B ALTERNATIVE slot"), SleepAlt.IsValid());
	TestTrue(
		TEXT("alternative is not the claimed slot"),
		SleepAlt.Request.SlotHandle != SleepNear.Request.SlotHandle);

	const FSmartObjectClaimHandle ClaimBAlt = Village->Claim(SleepAlt.Request.SlotHandle);
	TestTrue(TEXT("AGENT B claims other sleep slot"), ClaimBAlt.IsValid());

	TestTrue(TEXT("Use A"), Village->Use(ClaimA));
	TestTrue(TEXT("AGENT A RELEASE → SLOT AVAILABLE"), Village->Release(ClaimA));

	const FSmartObjectClaimHandle ClaimAAgain = Village->Claim(SleepNear.Request.SlotHandle);
	TestTrue(TEXT("released slot reusable"), ClaimAAgain.IsValid());
	Village->Release(ClaimAAgain);
	Village->Release(ClaimBAlt);

	TestTrue(TEXT("BUILDING DESTROY well"), Village->DestroyBuilding(Well));
	const FAnastasisVillageQueryResult DrinkAfter = Village->FindNearestInteraction(
		TAG_Anastasis_Activity_Drink,
		FVector(600.0, 0.0, 0.0),
		400.f);
	TestTrue(TEXT("BUILDING DESTROY → SMART OBJECT UNREGISTERED"), !DrinkAfter.IsValid());

	AAnastasisVillageBuilding* Well2 = Village->SpawnBuilding(
		EAnastasisVillageBuildingKind::Well,
		FName(TEXT("sim.well.2")),
		FTransform(FVector(600.0, 0.0, 0.0)));
	const FAnastasisVillageQueryResult DrinkRecreated = Village->FindNearestInteraction(
		TAG_Anastasis_Activity_Drink,
		FVector(600.0, 0.0, 0.0),
		400.f);
	TestTrue(TEXT("recreate well registers"), Well2 && DrinkRecreated.IsValid());
	TestEqual(TEXT("new sim id"), DrinkRecreated.SimId, FName(TEXT("sim.well.2")));

	Village->DestroyBuilding(House);
	Village->DestroyBuilding(Workshop);
	Village->DestroyBuilding(Well2);
	return true;
}

#endif
