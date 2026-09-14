#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Village/AnastasisVillageTags.h"
#include "AnastasisVillageBuilding.generated.h"

class USmartObjectComponent;
class USmartObjectDefinition;

UENUM()
enum class EAnastasisVillageBuildingKind : uint8
{
	House,
	Well,
	Workshop
};

/**
 * Batiment fonctionnel, pas un mesh. Enregistre ses slots dans SmartObjectSubsystem.
 * SimId remonte a la simulation JS — Unreal n'invente pas l'identite.
 */
UCLASS()
class AAnastasisVillageBuilding : public AActor
{
	GENERATED_BODY()

public:
	AAnastasisVillageBuilding();

	void Configure(EAnastasisVillageBuildingKind InKind, FName InSimId, USmartObjectDefinition* Definition);

	FName GetSimId() const { return SimId; }
	EAnastasisVillageBuildingKind GetKind() const { return Kind; }
	USmartObjectComponent* GetSmartObject() const { return SmartObject; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Anastasis")
	TObjectPtr<USmartObjectComponent> SmartObject;

	UPROPERTY()
	FName SimId;

	UPROPERTY()
	EAnastasisVillageBuildingKind Kind = EAnastasisVillageBuildingKind::House;
};
