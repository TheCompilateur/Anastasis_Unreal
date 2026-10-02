#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Village/AnastasisVillageTags.h"
#include "AnastasisVillageBuilding.generated.h"

class USmartObjectComponent;
class USmartObjectDefinition;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
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

	/**
	 * ICEBERG_001 : le foyer. 0..1 = part de la lumiere de l'atre qui sort par la porte et la
	 * fenetre (AnastasisMetabolism::FState::Hearth). 0 = eteint, composant invisible. Un
	 * batiment sans foyer (puits, grenier) ignore l'appel. N'ecrit rien dans la simulation.
	 */
	void SetHearth(double Level);
	double GetHearth() const { return HearthLevel; }

	/**
	 * ABANDON_001 : l'usure d'une maison que personne n'habite (AnastasisMetabolism::FState::Neglect,
	 * 0..1, vient des jours de vacance de la simulation). 0 = le materiau d'origine, intact. Sans
	 * l'asset M_VillageBuilding_Aged l'appel ne fait rien (l'ancien rendu reste).
	 */
	void SetNeglect(double Level);
	double GetNeglect() const { return NeglectLevel; }
	UMaterialInstanceDynamic* GetAgedMaterial() const { return AgedMaterial; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Anastasis")
	TObjectPtr<USmartObjectComponent> SmartObject;

	UPROPERTY(VisibleAnywhere, Category = "Anastasis")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Cree pour tout batiment, mais allume seulement pour une maison (SetHearth). */
	UPROPERTY(VisibleAnywhere, Category = "Anastasis")
	TObjectPtr<UPointLightComponent> Hearth;

	double HearthLevel = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AgedMaterial;

	double NeglectLevel = 0.0;
	bool bAgedMaterialMissing = false;

	UPROPERTY()
	FName SimId;

	UPROPERTY()
	EAnastasisVillageBuildingKind Kind = EAnastasisVillageBuildingKind::House;
};
