#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Village/AnastasisVillageTags.h"
#include "AnastasisVillageBuilding.generated.h"

class USmartObjectComponent;
class USmartObjectDefinition;
class UStaticMeshComponent;

UENUM()
enum class EAnastasisVillageBuildingKind : uint8
{
	House,
	Well,
	Workshop,
	Granary
};

/**
 * Batiment fonctionnel. Le mesh reflete le type (puits, maison, grenier) ;
 * l'identite reste SimId, posee par la simulation.
 * Enregistre ses slots dans SmartObjectSubsystem.
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
	bool HasBody() const;

	/**
	 * Chantier : le corps monte avec les pieces posees (0..1, 1 = acheve). Une
	 * echelle verticale tient lieu des 22 pieces du plan de la reference.
	 */
	void SetConstructionProgress(double Progress);

private:
	UPROPERTY(VisibleAnywhere, Category = "Anastasis")
	TObjectPtr<USmartObjectComponent> SmartObject;

	UPROPERTY(VisibleAnywhere, Category = "Anastasis")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY()
	FName SimId;

	UPROPERTY()
	EAnastasisVillageBuildingKind Kind = EAnastasisVillageBuildingKind::House;
};
