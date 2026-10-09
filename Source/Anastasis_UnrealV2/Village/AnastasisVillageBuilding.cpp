#include "Village/AnastasisVillageBuilding.h"

#include "Anastasis_UnrealV2.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "SmartObjectComponent.h"
#include "Village/AnastasisVillagePresentation.h"

namespace
{
	// Candelas a pleine puissance (Hearth = 1). Le ciel de nuit est expose tres bas : la valeur
	// se regle a l'oeil sur une capture de nuit, sans recompiler.
	TAutoConsoleVariable<float> CVarHearthCandela(
		TEXT("anastasis.Village.HearthCandela"),
		400.f,
		TEXT("ICEBERG_001 : intensite (cd) du foyer d'une maison habitee, a pleine nuit."),
		ECVF_Default);

	// dormir-couche-001 : l'exposition dans le volume habite d'un logis. La nuit, le ciel est expose pour la lune
	// (EV100 autour de -1) : le foyer, a deux metres, y brulait tout en blanc. Dedans, l'oeil s'accoutume au feu.
	TAutoConsoleVariable<int32> CVarInteriorLight(
		TEXT("anastasis.Village.InteriorLight"),
		1,
		TEXT("dormir-couche-001 : 1 = dans un logis, exposition d'interieur (anastasis.Village.InteriorNightEV / DayEV) ; 0 = celle du dehors."),
		ECVF_Default);
	TAutoConsoleVariable<float> CVarInteriorNightEV(
		TEXT("anastasis.Village.InteriorNightEV"),
		5.5f,
		TEXT("dormir-couche-001 : EV100 dans un logis la nuit (piece eclairee par son foyer)."),
		ECVF_Default);
	TAutoConsoleVariable<float> CVarInteriorDayEV(
		TEXT("anastasis.Village.InteriorDayEV"),
		9.0f,
		TEXT("dormir-couche-001 : EV100 dans un logis en plein jour (piece eclairee par sa porte et sa fenetre)."),
		ECVF_Default);

	/** Dans le volume de la maison : la lumiere sort par la porte (+Y d'auteur) et la fenetre, pas par les murs. */
	const FVector HearthLocal(0.0, 0.0, 120.0);
	int32 VisibleStockBundles(const int32 Stock, const int32 Need)
	{
		if (Stock <= 0 || Need <= 0) return 0;
		return FMath::Clamp(static_cast<int32>((3LL * Stock + Need - 1) / Need), 1, 3);
	}

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

	WoodStockVisual = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WoodStockVisual"));
	WoodStockVisual->SetupAttachment(SmartObject);
	WoodStockVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WoodStockVisual->SetCanEverAffectNavigation(false);
	WoodStockVisual->SetMobility(EComponentMobility::Movable);
	StoneStockVisual = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StoneStockVisual"));
	StoneStockVisual->SetupAttachment(SmartObject);
	StoneStockVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StoneStockVisual->SetCanEverAffectNavigation(false);
	StoneStockVisual->SetMobility(EComponentMobility::Movable);

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

	InteriorBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteriorBox"));
	InteriorBox->SetupAttachment(SmartObject);
	// Requete seulement, aucun canal : le post-process mesure la distance de la camera a la boite par sa collision
	// (UPostProcessComponent::EncompassesPoint) ; sans collision, la boite ne contient personne.
	InteriorBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteriorBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteriorBox->SetGenerateOverlapEvents(false);
	InteriorBox->SetCanEverAffectNavigation(false);
	InteriorBox->SetMobility(EComponentMobility::Movable);
	InteriorBox->SetHiddenInGame(true);
	InteriorBox->SetBoxExtent(FVector(1.0));
	InteriorLight = CreateDefaultSubobject<UPostProcessComponent>(TEXT("InteriorLight"));
	// Borne par sa boite parente ; passe devant le volume d'exposition du ciel (non borne, priorite 0).
	InteriorLight->SetupAttachment(InteriorBox);
	InteriorLight->bUnbound = false;
	InteriorLight->Priority = 10.0f;
	InteriorLight->BlendRadius = 60.0f;
	InteriorLight->BlendWeight = 1.0f;
	InteriorLight->bEnabled = false;
}

bool AAnastasisVillageBuilding::IsInteriorLightOn() const
{
	return InteriorLight && InteriorLight->bEnabled;
}

bool AAnastasisVillageBuilding::InteriorEncompasses(const FVector& Point) const
{
	return InteriorLight && InteriorLight->EncompassesPoint(Point, 0.f, nullptr);
}

FBox AAnastasisVillageBuilding::GetInteriorWorldBox() const
{
	return InteriorBox ? InteriorBox->Bounds.GetBox() : FBox(ForceInit);
}

void AAnastasisVillageBuilding::SetInteriorDaylight(const double Daylight)
{
	if (!InteriorLight || !bHasArchitecture) return;
	const AnastasisArchitecture::FArchetype& A = AnastasisArchitecture::Get(Variant);
	const bool bOn = A.Interior.IsValid && CVarInteriorLight.GetValueOnGameThread() != 0;
	if (InteriorLight->bEnabled != bOn) InteriorLight->bEnabled = bOn;
	if (!bOn)
	{
		InteriorEV = NoInteriorEV;
		return;
	}
	const double EV = FMath::Lerp(static_cast<double>(CVarInteriorNightEV.GetValueOnGameThread()),
		static_cast<double>(CVarInteriorDayEV.GetValueOnGameThread()), FMath::Clamp(Daylight, 0.0, 1.0));
	if (InteriorEV == NoInteriorEV || FMath::Abs(EV - InteriorEV) > 0.01)
	{
		InteriorEV = EV;
		FPostProcessSettings& S = InteriorLight->Settings;
		S.bOverride_AutoExposureMinBrightness = true;
		S.bOverride_AutoExposureMaxBrightness = true;
		S.AutoExposureMinBrightness = static_cast<float>(EV);
		S.AutoExposureMaxBrightness = static_cast<float>(EV);
	}
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
	if (bHasArchitecture)
	{
		// Corps d'archetype : un seul MID permanent, l'abandon n'est qu'un parametre (0 compris).
		if (UMaterialInstanceDynamic* Mid = EnsureArchitectureMaterial())
		{
			NeglectLevel = Clamped;
			Mid->SetScalarParameterValue(TEXT("Neglect"), static_cast<float>(Clamped));
		}
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
		if (Footing) Footing->SetMaterial(0, nullptr);
	}
	OriginalMaterial = nullptr;
	AgedMaterial = nullptr;
	NeglectLevel = -1.0;
	WeatheringLevel = -1.0;
	Body->SetStaticMesh(BodyMesh);
	Body->SetVisibility(true);
	if (Footing)
	{
		Footing->SetStaticMesh(FootMesh);
		Footing->SetVisibility(FootMesh != nullptr);
	}
	// Le foyer dans l'atre, 60 cm au-dessus de la sole : la lumiere sort par la porte, les fenetres, la galerie.
	HearthAuthored = A.bHasHearth ? A.HearthLocal + FVector(0.0, 60.0, 40.0) : HearthLocal;
	// dormir-couche-001 : la boite du volume habite (l'exposition s'y regle a SetInteriorDaylight).
	if (InteriorBox)
	{
		InteriorCenter = A.Interior.IsValid ? A.Interior.GetCenter() : FVector::ZeroVector;
		InteriorBox->SetBoxExtent(A.Interior.IsValid ? A.Interior.GetExtent() : FVector(1.0));
		if (!A.Interior.IsValid && InteriorLight) InteriorLight->bEnabled = false;
	}
	SetPadOffset(PadOffset);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_ARCH %s archetype=%s tier=%d body=%s footing=%d"),
		*SimId.ToString(), A.Id, A.Tier, *BodyMesh->GetName(), FootMesh ? 1 : 0);
	return true;
}

UMaterialInstanceDynamic* AAnastasisVillageBuilding::EnsureArchitectureMaterial()
{
	if (!Body)
	{
		return nullptr;
	}
	if (AgedMaterial && Body->GetMaterial(0) == AgedMaterial)
	{
		return AgedMaterial;
	}
	UMaterialInterface* Base = Body->GetMaterial(0);
	if (!Base)
	{
		return nullptr;
	}
	OriginalMaterial = Base;
	AgedMaterial = UMaterialInstanceDynamic::Create(Base, this);
	Body->SetMaterial(0, AgedMaterial);
	if (Footing)
	{
		Footing->SetMaterial(0, AgedMaterial);
	}
	return AgedMaterial;
}

void AAnastasisVillageBuilding::SetWeathering(const double Level)
{
	if (!bHasArchitecture)
	{
		return;
	}
	const double Clamped = FMath::Clamp(Level, 0.0, 1.0);
	if (FMath::IsNearlyEqual(Clamped, WeatheringLevel, 0.005))
	{
		return;
	}
	if (UMaterialInstanceDynamic* Mid = EnsureArchitectureMaterial())
	{
		WeatheringLevel = Clamped;
		Mid->SetScalarParameterValue(TEXT("Weathering"), static_cast<float>(Clamped));
	}
}

void AAnastasisVillageBuilding::SetPadOffset(const double OffsetCm)
{
	PadOffset = OffsetCm;
	const FVector Offset(0.0, 0.0, PadOffset);
	if (Body) Body->SetRelativeLocation(Offset);
	if (Footing) Footing->SetRelativeLocation(Offset);
	if (Hearth) Hearth->SetRelativeLocation(HearthAuthored + Offset);
	if (InteriorBox) InteriorBox->SetRelativeLocation(InteriorCenter + Offset);
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

float AAnastasisVillageBuilding::StockGroundLocalZ(const FVector& LocalAnchor) const
{
	UWorld* World = GetWorld();
	if (!World) return 0.f;
	const FVector WorldAnchor = GetActorTransform().TransformPosition(LocalAnchor);
	double GroundZ = 0.0;
	if (FAnastasisVillagePresentation::TraceGround(World, WorldAnchor.X, WorldAnchor.Y, GroundZ))
		return static_cast<float>(GetActorTransform().InverseTransformPosition(
			FVector(WorldAnchor.X, WorldAnchor.Y, GroundZ)).Z);
	return 0.f;
}

void AAnastasisVillageBuilding::SetSiteStock(
	int32 WoodStock, int32 WoodNeed, int32 StoneStock, int32 StoneNeed, bool bActiveSite)
{
	const int32 WoodCount = bActiveSite ? VisibleStockBundles(WoodStock, WoodNeed) : 0;
	const int32 StoneCount = bActiveSite ? VisibleStockBundles(StoneStock, StoneNeed) : 0;
	if (WoodCount == VisibleWoodBundles && StoneCount == VisibleStoneBundles) return;
	VisibleWoodBundles = WoodCount;
	VisibleStoneBundles = StoneCount;
	// +Y is the authored entry side. Keep the approach free and place the two
	// material groups outside the actual footing, on opposite sides of the entry.
	float EntryX = 0.f;
	float Side = 150.f;
	float Front = 220.f;
	if (bHasArchitecture)
	{
		const AnastasisArchitecture::FArchetype& A = AnastasisArchitecture::Get(Variant);
		EntryX = static_cast<float>(A.EntryLocal.X);
		Side = 250.f;
		Front = static_cast<float>(A.Footprint.Max.Y + 70.0);
	}
	else if (Body && Body->GetStaticMesh())
	{
		const FBox Bounds = Body->GetStaticMesh()->GetBoundingBox();
		Side = FMath::Clamp(static_cast<float>(Bounds.GetExtent().X * .4), 150.f, 250.f);
		Front = static_cast<float>(Bounds.Max.Y + 90.0);
	}
	const FVector WoodStockAnchor(EntryX - Side, Front, 0.f);
	const FVector StoneStockAnchor(EntryX + Side, Front, 0.f);
	const auto Place = [this](UInstancedStaticMeshComponent* Component, const int32 Count,
		const FVector& Anchor, const float LayerHeight)
	{
		if (!Component) return;
		Component->ClearInstances();
		if (!Component->GetStaticMesh() || Count == 0) return;
		const float GroundZ = StockGroundLocalZ(Anchor);
		for (int32 I = 0; I < Count; ++I)
		{
			const FVector Position(Anchor.X, Anchor.Y, GroundZ + I * LayerHeight);
			Component->AddInstance(FTransform(FRotator(0.f, I % 2 ? 4.f : -4.f, 0.f), Position));
		}
	};
	Place(WoodStockVisual, WoodCount, WoodStockAnchor, 28.f);
	Place(StoneStockVisual, StoneCount, StoneStockAnchor, 24.f);
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
	UStaticMesh* Timber = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Anastasis/SiteStock001/SM_Site_TimberBundle_01.SM_Site_TimberBundle_01"));
	UStaticMesh* Stone = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Game/Anastasis/SiteStock001/SM_Site_StoneBundle_01.SM_Site_StoneBundle_01"));
	WoodStockVisual->SetStaticMesh(Timber);
	StoneStockVisual->SetStaticMesh(Stone);
	if (!Timber || !Stone)
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning,
			TEXT("ANASTASIS_VILLAGE site stock assets missing: timber=%d stone=%d"), Timber ? 1 : 0, Stone ? 1 : 0);
	}
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_VILLAGE mesh %s kind=%d loaded=%d"),
		Path ? Path : TEXT("(none)"),
		static_cast<int32>(InKind),
		Mesh ? 1 : 0);
}
