#include "Village/AnastasisVillageBuilding.h"

#include "Anastasis_UnrealV2.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "SmartObjectComponent.h"

namespace
{
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
}

bool AAnastasisVillageBuilding::HasBody() const
{
	return Body && Body->GetStaticMesh() != nullptr;
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
