#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Village/AnastasisArchitecture.h"
#include "Village/AnastasisVillageTags.h"
#include "AnastasisVillageBuilding.generated.h"

class UBoxComponent;
class UPostProcessComponent;
class USmartObjectComponent;
class USmartObjectDefinition;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UInstancedStaticMeshComponent;
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

	/**
	 * ARCHITECTURE_SCALE_001 : pose la maisonnee a l'echelle humaine (corps + assise terrassee) d'apres son
	 * archetype. Si les assets du generateur manquent, l'ancien mesh reste (aucune regression a vide).
	 * Retourne vrai si le corps de l'archetype est charge.
	 */
	bool ApplyArchitecture(AnastasisArchitecture::EVariant InVariant);

	/**
	 * Niveau de la cour terrassee par rapport a la racine (cm) : la racine reste sur le point de la
	 * simulation (SimToUnreal), le corps, l'assise et le foyer descendent ou montent ensemble (ARCH-10).
	 */
	void SetPadOffset(double OffsetCm);

	/**
	 * dormir-couche-001 : dans le volume habite d'un logis, l'exposition n'est pas celle du dehors (le clair de lune,
	 * le plein soleil) mais celle d'une piece eclairee par son foyer, ou par sa porte le jour. `Daylight` 0..1 du ciel.
	 * Sans volume habite (puits, grenier) ou `anastasis.Village.InteriorLight 0`, rien.
	 */
	void SetInteriorDaylight(double Daylight);
	/** L'exposition (EV100) que voit une camera dans le logis ; NoInteriorEV si rien n'est pose. */
	static constexpr double NoInteriorEV = -99.0;
	double GetInteriorExposure() const { return InteriorEV; }
	bool IsInteriorLightOn() const;
	/** Instrument : ce point (une camera) est-il dans le volume habite, au sens du post-process ? Et ses bornes monde. */
	bool InteriorEncompasses(const FVector& Point) const;
	FBox GetInteriorWorldBox() const;

	/**
	 * SETTLEMENT_MORPHOGENESIS_001 : la patine de l'age (0..1), tiree des jours ecoules depuis l'achevement
	 * (biographie), distincte de l'abandon (`SetNeglect`). Seulement pour un corps d'archetype.
	 */
	void SetWeathering(double Level);
	double GetWeathering() const { return WeatheringLevel; }
	double GetPadOffset() const { return PadOffset; }
	bool HasArchitecture() const { return bHasArchitecture; }
	AnastasisArchitecture::EVariant GetVariant() const { return Variant; }
	UStaticMeshComponent* GetBody() const { return Body; }
	UStaticMeshComponent* GetFooting() const { return Footing; }

	FName GetSimId() const { return SimId; }
	EAnastasisVillageBuildingKind GetKind() const { return Kind; }
	USmartObjectComponent* GetSmartObject() const { return SmartObject; }
	bool HasBody() const;

	/**
	 * Chantier : le corps monte avec les pieces posees (0..1, 1 = acheve). Une
	 * echelle verticale tient lieu des 22 pieces du plan de la reference.
	 */
	void SetConstructionProgress(double Progress);

	/** Physical stock at an unfinished site. Read-only projection of the simulation ledger. */
	void SetSiteStock(int32 WoodStock, int32 WoodNeed, int32 StoneStock, int32 StoneNeed, bool bActiveSite);
	/** Caisses du grenier : projection grossiere du stock physique, sans ecriture dans la simulation. */
	void SetProvisionStock(int32 FoodPhysical, bool bCompletedGranary);
	int32 GetVisibleProvisionCrates() const { return VisibleProvisionCrates; }
	UInstancedStaticMeshComponent* GetProvisionVisual() const { return ProvisionVisual; }

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

	/** Assise : cour de terre battue et soutenement de pierre seche. Pleine des l'ouverture du chantier. */
	UPROPERTY(VisibleAnywhere, Category = "Anastasis")
	TObjectPtr<UStaticMeshComponent> Footing;

	AnastasisArchitecture::EVariant Variant = AnastasisArchitecture::EVariant::HousePoor;
	bool bHasArchitecture = false;
	double PadOffset = 0.0;
	double WeatheringLevel = -1.0;
	/** Le MID permanent d'un corps d'archetype : Neglect et Weathering y vivent ensemble. */
	UMaterialInstanceDynamic* EnsureArchitectureMaterial();
	FVector HearthAuthored = FVector(0.0, 0.0, 120.0);

	UPROPERTY(VisibleAnywhere, Category = "Anastasis|Site Stock")
	TObjectPtr<UInstancedStaticMeshComponent> WoodStockVisual;

	UPROPERTY(VisibleAnywhere, Category = "Anastasis|Site Stock")
	TObjectPtr<UInstancedStaticMeshComponent> StoneStockVisual;

	UPROPERTY(VisibleAnywhere, Category = "Anastasis|Provisions")
	TObjectPtr<UInstancedStaticMeshComponent> ProvisionVisual;

	/** dormir-couche-001 : le volume habite (boite) et l'exposition qui y regne (post-process borne par la boite). */
	UPROPERTY(VisibleAnywhere, Category = "Anastasis")
	TObjectPtr<UBoxComponent> InteriorBox;

	UPROPERTY(VisibleAnywhere, Category = "Anastasis")
	TObjectPtr<UPostProcessComponent> InteriorLight;

	FVector InteriorCenter = FVector::ZeroVector;
	double InteriorEV = NoInteriorEV;

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
	int32 VisibleWoodBundles = -1;
	int32 VisibleStoneBundles = -1;
	int32 VisibleProvisionCrates = -1;

	float StockGroundLocalZ(const FVector& LocalAnchor) const;

	UPROPERTY()
	FName SimId;

	UPROPERTY()
	EAnastasisVillageBuildingKind Kind = EAnastasisVillageBuildingKind::House;
};
