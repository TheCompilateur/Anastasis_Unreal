#include "Village/AnastasisVillagerVisual.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Village/AnastasisVillagerLooks.h"

static TAutoConsoleVariable<int32> CVarVillageBodies(
	TEXT("anastasis.Village.Bodies"),
	2,
	TEXT("3D villagers in game: 0 hides bodies for diagnosis; 1 or 2 shows bodies at every distance. Portrait cards are editor previews only."),
	ECVF_Default);

namespace
{
	const FName PortraitParam(TEXT("Portrait"));
	const FName MirrorParam(TEXT("Mirror"));
	/** Faster than this between two frames is a jump (scenario, reset), not a walk: cm/s. */
	constexpr double MaxWalkCmPerSecond = 2000.0;
	/** Below this the body stands: cm/s. Under it the heading is kept. */
	constexpr float StandSpeed = 15.0f;
	/** How fast the drawn speed follows the measured one (1/s) -- the steps of the simulation arrive in bursts. */
	constexpr float SpeedFollow = 5.0f;
	/** Turning rate of the body, degrees per second. */
	constexpr float TurnRate = 300.0f;
	/** The Epic mannequins face +Y in mesh space: the actor's +X is their -90 yaw. */
	constexpr float MeshYawOffset = -90.0f;
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
	Card->SetVisibility(false);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(FeetRoot);
	// The actor turns toward the camera for the card; the body turns toward where it walks.
	Body->SetUsingAbsoluteRotation(true);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	Body->SetGenerateOverlapEvents(false);
	Body->CastShadow = true;
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Body->SetVisibility(false);
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
	LookId = InLookId;
	// Runtime villagers keep the style ID and measured colours, but never load the reference image.
	if (!Portrait && !Material)
	{
		return true;
	}
	if (!Portrait || !Material)
	{
		return false;
	}
	PortraitMaterial = UMaterialInstanceDynamic::Create(Material, this);
	PortraitMaterial->SetTextureParameterValue(PortraitParam, Portrait);
	PortraitMaterial->SetScalarParameterValue(MirrorParam, bMirrored ? 1.0f : 0.0f);
	Card->SetMaterial(0, PortraitMaterial);
	const UWorld* World = GetWorld();
	Card->SetVisibility(!bShowingBody && World && !World->IsGameWorld());
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
	SetActorLocation(Feet);
}

bool AAnastasisVillagerVisual::SetBody(USkeletalMesh* Mesh, UBlendSpace* Locomotion, UMaterialInterface* Material, const AnastasisVillagerLooks::FBodyLook& Look)
{
	if (!Mesh || !Locomotion || !Material)
	{
		return false;
	}
	Body->SetSkeletalMesh(Mesh);
	BodyScale = FMath::Max(Look.Scale, 0.1f);
	Body->SetRelativeScale3D(FVector(BodyScale));
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->PlayAnimation(Locomotion, true);
	if (UAnimSingleNodeInstance* Single = Body->GetSingleNodeInstance())
	{
		Single->SetPlayRate(Look.PlayRate);
		Single->SetBlendSpacePosition(FVector::ZeroVector);
	}
	UMaterialInstanceDynamic* Dress = UMaterialInstanceDynamic::Create(Material, this);
	Dress->SetVectorParameterValue(TEXT("Skin"), Look.Skin);
	Dress->SetVectorParameterValue(TEXT("Hair"), Look.Hair);
	Dress->SetVectorParameterValue(TEXT("Garment"), Look.Garment);
	Dress->SetVectorParameterValue(TEXT("Trim"), Look.Trim);
	Dress->SetScalarParameterValue(TEXT("Hem"), Look.Hem);
	Dress->SetScalarParameterValue(TEXT("HairLow"), Look.HairLow);
	// The bands are fractions of the mesh's own height: Manny and Quinn are not the same size.
	Dress->SetScalarParameterValue(TEXT("Height"), static_cast<float>(Mesh->GetBounds().BoxExtent.Z * 2.0));
	for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
	{
		Body->SetMaterial(Slot, Dress);
	}
	return true;
}

bool AAnastasisVillagerVisual::HasBody() const
{
	return Body && Body->GetSkeletalMeshAsset() != nullptr;
}

void AAnastasisVillagerVisual::ShowBody(bool bBody)
{
	bBody = bBody && HasBody();
	if (bShowingBody == bBody)
	{
		return;
	}
	bShowingBody = bBody;
	Body->SetVisibility(bBody);
	// A missing 3D asset must not silently turn a villager into a painted billboard.
	const UWorld* World = GetWorld();
	Card->SetVisibility(!bBody && PortraitMaterial != nullptr && World && !World->IsGameWorld());
}

void AAnastasisVillagerVisual::UpdateBodyMotion(float DeltaSeconds)
{
	const FVector Feet = GetActorLocation();
	if (!bHasLastFeet || DeltaSeconds <= UE_KINDA_SMALL_NUMBER)
	{
		LastFeet = Feet;
		bHasLastFeet = true;
		return;
	}
	const FVector Step = Feet - LastFeet;
	LastFeet = Feet;
	double Measured = Step.Size2D() / DeltaSeconds;
	if (Measured > MaxWalkCmPerSecond)
	{
		Measured = 0.0;
	}
	BodySpeed = FMath::FInterpTo(BodySpeed, static_cast<float>(Measured), DeltaSeconds, SpeedFollow);
	if (Measured > StandSpeed && Step.SizeSquared2D() > 0.0)
	{
		const float Target = FMath::RadiansToDegrees(FMath::Atan2(Step.Y, Step.X));
		const float Delta = FMath::FindDeltaAngleDegrees(BodyHeading, Target);
		BodyHeading = FRotator::NormalizeAxis(BodyHeading + FMath::Clamp(Delta, -TurnRate * DeltaSeconds, TurnRate * DeltaSeconds));
	}
}

void AAnastasisVillagerVisual::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBodyMotion(DeltaSeconds);
	ShowBody(CVarVillageBodies.GetValueOnGameThread() != 0);
	if (bShowingBody)
	{
		Body->SetWorldRotation(FRotator(0.0, BodyHeading + MeshYawOffset, 0.0));
		if (UAnimSingleNodeInstance* Single = Body->GetSingleNodeInstance())
		{
			// BS_Idle_Walk_Run: X = direction relative to the facing (-180..180), Y = speed (idle 0, walk 300,
			// jog 600). The body faces where it goes, so the direction is 0. The blend space is authored
			// for the full-size mannequin: a smaller body takes more strides for the same ground speed,
			// so it is fed the speed it would have at full size.
			Single->SetBlendSpacePosition(FVector(0.0, BodySpeed / BodyScale, 0.0));
		}
	}
}
