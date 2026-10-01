#include "Village/AnastasisVillagerVisual.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Village/AnastasisVillagerLooks.h"

namespace
{
	const FName PortraitParam(TEXT("Portrait"));
	const FName MirrorParam(TEXT("Mirror"));
	/** Below this step (cm) the villager is standing: keep the facing, do not flicker. */
	constexpr double MinStepCm = 0.5;
}

AAnastasisVillagerVisual::AAnastasisVillagerVisual()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the camera has moved this frame, or the card lags one frame behind it.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	FeetRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Feet"));
	SetRootComponent(FeetRoot);

	Card = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Card"));
	Card->SetupAttachment(FeetRoot);
	Card->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Card->SetCanEverAffectNavigation(false);
	Card->CastShadow = true;
	Card->bCastShadowAsTwoSided = true;
	BuildCard();
}

void AAnastasisVillagerVisual::BuildCard()
{
	using namespace AnastasisVillagerLooks;
	// The card lies in the local YZ plane, facing +X. Seen from +X the viewer's left is +Y,
	// so U = 0 (the left of the PNG) is at +Y. V = 0 is the top of the PNG.
	const double HalfW = CanvasWidthCm * 0.5;
	const double Bottom = -FootMarginCm;
	const double Top = CanvasHeightCm - FootMarginCm;
	const TArray<FVector> Vertices = {
		FVector(0.0, HalfW, Top), FVector(0.0, -HalfW, Top),
		FVector(0.0, -HalfW, Bottom), FVector(0.0, HalfW, Bottom),
	};
	const TArray<FVector2D> UVs = { FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1) };
	const TArray<FVector> Normals = { FVector::ForwardVector, FVector::ForwardVector, FVector::ForwardVector, FVector::ForwardVector };
	const FProcMeshTangent Tangent(0.0, -1.0, 0.0);
	const TArray<FProcMeshTangent> Tangents = { Tangent, Tangent, Tangent, Tangent };
	const TArray<int32> Triangles = { 0, 2, 1, 0, 3, 2 };
	Card->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, false);
}

bool AAnastasisVillagerVisual::SetLook(FName InLookId, UTexture2D* Portrait, UMaterialInterface* Material)
{
	if (!Portrait || !Material)
	{
		return false;
	}
	PortraitMaterial = UMaterialInstanceDynamic::Create(Material, this);
	PortraitMaterial->SetTextureParameterValue(PortraitParam, Portrait);
	PortraitMaterial->SetScalarParameterValue(MirrorParam, bMirrored ? 1.0f : 0.0f);
	Card->SetMaterial(0, PortraitMaterial);
	LookId = InLookId;
	return true;
}

void AAnastasisVillagerVisual::FaceTowards(const FVector& ViewLocation)
{
	const FVector ToView = ViewLocation - GetActorLocation();
	if (ToView.SizeSquared2D() < 1.0)
	{
		return;
	}
	SetActorRotation(FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(ToView.Y, ToView.X)), 0.0));
}

void AAnastasisVillagerVisual::SetMirrored(bool bInMirrored)
{
	if (bMirrored == bInMirrored)
	{
		return;
	}
	bMirrored = bInMirrored;
	if (PortraitMaterial)
	{
		PortraitMaterial->SetScalarParameterValue(MirrorParam, bMirrored ? 1.0f : 0.0f);
	}
}

void AAnastasisVillagerVisual::MoveFeetTo(const FVector& Feet)
{
	const FVector Step = Feet - GetActorLocation();
	if (Step.SizeSquared2D() > MinStepCm * MinStepCm)
	{
		LastStep = Step;
	}
	SetActorLocation(Feet);
}

void AAnastasisVillagerVisual::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	if (!Player || !Player->PlayerCameraManager)
	{
		return;
	}
	FaceTowards(Player->PlayerCameraManager->GetCameraLocation());

	if (!LastStep.IsNearlyZero())
	{
		// Facing the camera, the card's +X points at the viewer: the screen's right is (n.Y, -n.X).
		const FVector Normal = GetActorForwardVector();
		const FVector ScreenRight(Normal.Y, -Normal.X, 0.0);
		const double Lateral = FVector::DotProduct(LastStep, ScreenRight);
		if (FMath::Abs(Lateral) > MinStepCm)
		{
			SetMirrored(Lateral > 0.0);
		}
		LastStep = FVector::ZeroVector;
	}
}
