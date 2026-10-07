#include "Village/AnastasisVillageBuilding.h"

#include "Anastasis_UnrealV2.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "SmartObjectComponent.h"

namespace
{
	// Candelas a pleine puissance (Hearth = 1). Le ciel de nuit est expose tres bas : la valeur
	// se regle a l'oeil sur une capture de nuit, sans recompiler.
	TAutoConsoleVariable<float> CVarHearthCandela(
		TEXT("anastasis.Village.HearthCandela"),
		400.f,
		TEXT("ICEBERG_001 : intensite (cd) du foyer d'une maison habitee, a pleine nuit."),
		ECVF_Default);

	/** Dans le volume de la maison : la lumiere sort par la porte (+Y d'auteur) et la fenetre, pas par les murs. */
	const FVector HearthLocal(0.0, 0.0, 120.0);

	const TCHAR* BodyMeshPath(EAnastasisVillageBuildingKind Kind)
	{
		switch (Kind)
		{
		case EAnastasisVillageBuildingKind::Well:
			return TEXT("/Game/Anastasis/VillageBuildings/SM_Well_Stone_01.SM_Well_Stone_01");
		case EAnastasisVillageBuildingKind::House:
			return TEXT("/Game/Anastasis/VillageBuildings/SM_House_Refuge_01.SM_House_Refuge_01");
		case EAnastasisVillageBuildingKind::Granary:
			return TEXT("/Game/Anastasis/VillageBuildings/SM_Granary_Raised_01.SM_Granary_Raised_01");
		default:
			return nullptr;
		}
	}
}

AAnastasisVillageBuilding::AAnastasisVillageBuilding()
{
	PrimaryActorTick.bCanEverTick = false;
	SmartObject = CreateDefaultSubobject<USmartObjectComponent>(TEXT("SmartObject"));
	SetRootComponent(SmartObject);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(SmartObject);
	Body->SetCollisionProfileName(TEXT("BlockAll"));
	Body->SetGenerateOverlapEvents(false);
	Body->SetCastShadow(true);
	Body->SetCanEverAffectNavigation(false);
	Body->SetMobility(EComponentMobility::Movable);

	Footing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Footing"));
	Footing->SetupAttachment(SmartObject);
	Footing->SetCollisionProfileName(TEXT("BlockAll"));
	Footing->SetGenerateOverlapEvents(false);
	Footing->SetCastShadow(true);
	Footing->SetCanEverAffectNavigation(false);
	Footing->SetMobility(EComponentMobility::Movable);
	Footing->SetVisibility(false);

	Hearth = CreateDefaultSubobject<UPointLightComponent>(TEXT("Hearth"));
	Hearth->SetupAttachment(SmartObject);
	Hearth->SetRelativeLocation(HearthLocal);
	Hearth->SetMobility(EComponentMobility::Movable);
	Hearth->SetIntensityUnits(ELightUnits::Candelas);
	Hearth->SetLightColor(FLinearColor(1.0f, 0.52f, 0.20f));
	Hearth->SetAttenuationRadius(900.f);
	Hearth->SetSourceRadius(14.f);
	Hearth->SetCastShadows(true);
	Hearth->SetVolumetricScatteringIntensity(0.f);
	Hearth->SetIntensity(0.f);
	Hearth->SetVisibility(false);
}

void AAnastasisVillageBuilding::SetNeglect(const double Level)
{
	if (!Body || Kind != EAnastasisVillageBuildingKind::House)
	{
		return;
	}
	const double Clamped = FMath::Clamp(Level, 0.0, 1.0);
	if (FMath::IsNearlyEqual(Clamped, NeglectLevel, 0.002))
	{
		return;
	}
	if (Clamped <= 0.0)
	{
		// Quelqu'un est revenu : la maison reprend son materiau d'origine.
		NeglectLevel = 0.0;
		if (OriginalMaterial)
		{
			Body->SetMaterial(0, OriginalMaterial);
		}
		return;
	}
	if (!AgedMaterial)
	{
		if (bAgedMaterialMissing)
		{
			return;
		}
		// ARCHITECTURE_SCALE_001 : M_AnastasisArchitecture porte deja `Neglect` (meme sens, meme courbe) ;
		// l'ancien materiau d'usure ne sert qu'aux anciens meshes.
		UMaterialInterface* Current = Body->GetMaterial(0);
		float Probe = 0.f;
		const bool bOwnNeglect = Current && Current->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("Neglect")), Probe);
		UMaterialInterface* Aged = bOwnNeglect ? Current : LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/Anastasis/VillageBuildings/M_VillageBuilding_Aged.M_VillageBuilding_Aged"));
		if (!Aged)
		{
			bAgedMaterialMissing = true;
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE aging material missing: M_VillageBuilding_Aged (run create-building-aging.ps1)"));
			return;
		}
		OriginalMaterial = Body->GetMaterial(0);
		AgedMaterial = UMaterialInstanceDynamic::Create(Aged, this);
	}
	NeglectLevel = Clamped;
	AgedMaterial->SetScalarParameterValue(TEXT("Neglect"), static_cast<float>(Clamped));
	if (Body->GetMaterial(0) != AgedMaterial)
	{
		Body->SetMaterial(0, AgedMaterial);
	}
}

void AAnastasisVillageBuilding::SetHearth(const double Level)
{
	if (!Hearth || Kind != EAnastasisVillageBuildingKind::House)
	{
		return;
	}
	const double Clamped = FMath::Clamp(Level, 0.0, 1.0);
	if (FMath::IsNearlyEqual(Clamped, HearthLevel, 0.005))
	{
		return;
	}
	HearthLevel = Clamped;
	const bool bLit = Clamped > 0.01;
	Hearth->SetIntensity(static_cast<float>(Clamped * CVarHearthCandela.GetValueOnGameThread()));
	Hearth->SetVisibility(bLit);
}

bool AAnastasisVillageBuilding::HasBody() const
{
	return Body && Body->GetStaticMesh() != nullptr;
}

bool AAnastasisVillageBuilding::ApplyArchitecture(AnastasisArchitecture::EVariant InVariant)
{
	const AnastasisArchitecture::FArchetype& A = AnastasisArchitecture::Get(InVariant);
	UStaticMesh* BodyMesh = LoadObject<UStaticMesh>(nullptr, A.BodyMesh);
	UStaticMesh* FootMesh = LoadObject<UStaticMesh>(nullptr, A.FootingMesh);
	if (!BodyMesh || !Body)
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning,
			TEXT("ANASTASIS_ARCH %s: archetype %s absent (%s) -- ancien mesh garde, lancer create-village-architecture.ps1"),
			*SimId.ToString(), A.Id, A.BodyMesh);
		return false;
	}
	Variant = InVariant;
	bHasArchitecture = true;
	if (AgedMaterial)
	{
		Body->SetMaterial(0, nullptr);
	}
	OriginalMaterial = nullptr;
	AgedMaterial = nullptr;
	NeglectLevel = 0.0;
	Body->SetStaticMesh(BodyMesh);
	Body->SetVisibility(true);
	if (Footing)
	{
		Footing->SetStaticMesh(FootMesh);
		Footing->SetVisibility(FootMesh != nullptr);
	}
	// Le foyer dans l'atre, 60 cm au-dessus de la sole : la lumiere sort par la porte, les fenetres, la galerie.
	HearthAuthored = A.bHasHearth ? A.HearthLocal + FVector(0.0, 60.0, 40.0) : HearthLocal;
	SetPadOffset(PadOffset);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_ARCH %s archetype=%s tier=%d body=%s footing=%d"),
		*SimId.ToString(), A.Id, A.Tier, *BodyMesh->GetName(), FootMesh ? 1 : 0);
	return true;
}

void AAnastasisVillageBuilding::SetPadOffset(const double OffsetCm)
{
	PadOffset = OffsetCm;
	const FVector Offset(0.0, 0.0, PadOffset);
	if (Body) Body->SetRelativeLocation(Offset);
	if (Footing) Footing->SetRelativeLocation(Offset);
	if (Hearth) Hearth->SetRelativeLocation(HearthAuthored + Offset);
}

void AAnastasisVillageBuilding::SetConstructionProgress(double Progress)
{
	if (!Body) return;
	// Un chantier sans piece montre encore ses fondations : 6 % de la hauteur.
	const double Z = Progress >= 1.0 ? 1.0 : FMath::Clamp(Progress, 0.06, 1.0);
	const FVector Current = Body->GetRelativeScale3D();
	if (!FMath::IsNearlyEqual(Current.Z, Z))
	{
		Body->SetRelativeScale3D(FVector(Current.X, Current.Y, Z));
	}
}

void AAnastasisVillageBuilding::Configure(
	EAnastasisVillageBuildingKind InKind,
	FName InSimId,
	USmartObjectDefinition* Definition)
{
	Kind = InKind;
	SimId = InSimId;
	if (SmartObject)
	{
		SmartObject->SetDefinition(Definition);
	}
	const TCHAR* Path = BodyMeshPath(InKind);
	UStaticMesh* Mesh = Path ? LoadObject<UStaticMesh>(nullptr, Path) : nullptr;
	if (Body)
	{
		Body->SetStaticMesh(Mesh);
		Body->SetVisibility(Mesh != nullptr);
	}
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_VILLAGE mesh %s kind=%d loaded=%d"),
		Path ? Path : TEXT("(none)"),
		static_cast<int32>(InKind),
		Mesh ? 1 : 0);
}
