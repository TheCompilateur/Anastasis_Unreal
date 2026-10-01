#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnastasisVillagerVisual.generated.h"

class UBlendSpace;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UTexture2D;
namespace AnastasisVillagerLooks { struct FBodyLook; }

/**
 * The face of one simulated villager: a cut-out portrait on a vertical card (VILLAGER_PNG_001).
 *
 * The actor origin is the villager's FEET. The card is the shared 100 x 200 cm canvas, so the
 * stature painted into the PNG is the stature in the world. It turns about the vertical axis
 * only (a cylindrical billboard: it stays upright under a high camera), toward the first local
 * player's camera in a game world. Portraits are painted facing three-quarters LEFT; walking
 * toward the right of the screen mirrors the card, standing still keeps the last facing.
 *
 * VILLAGER_BODY_3D_001 -- near the camera the villager is a 3D body instead: a skinned mannequin,
 * dressed by its material, that walks with its legs at the speed it is drawn at, breathes when it
 * stands, and turns toward where it goes. Beyond `anastasis.Village.BodyDistance` the card takes
 * over (a person is a dozen pixels tall there). `anastasis.Village.Bodies` 0 = cards only,
 * 1 = bodies near (default), 2 = bodies everywhere. No body bound (assets missing): the card, always.
 *
 * Pure presentation: spawned and moved by FAnastasisVillagePresentation from the simulation,
 * it reads nothing back into it.
 */
UCLASS(NotBlueprintable)
class AAnastasisVillagerVisual : public AActor
{
	GENERATED_BODY()

public:
	AAnastasisVillagerVisual();

	/** Binds the portrait through a dynamic instance of `Material` (`Portrait` texture parameter). False if either is missing. */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Villagers")
	bool SetLook(FName InLookId, UTexture2D* Portrait, UMaterialInterface* Material);

	/** Turns the card toward a point (yaw only). Called every frame in a game world; by hand on a lineup. */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Villagers")
	void FaceTowards(const FVector& ViewLocation);

	UFUNCTION(BlueprintCallable, Category = "Anastasis|Villagers")
	void SetMirrored(bool bInMirrored);

	UFUNCTION(BlueprintPure, Category = "Anastasis|Villagers")
	FName GetLookId() const { return LookId; }

	UFUNCTION(BlueprintPure, Category = "Anastasis|Villagers")
	bool IsMirrored() const { return bMirrored; }

	/** Puts the feet at `Feet` and remembers the step, which picks the facing in Tick. */
	void MoveFeetTo(const FVector& Feet);

	/**
	 * Binds the 3D body: `Mesh` played through `Locomotion` (single-node blend space, X = direction
	 * relative to the facing, Y = speed in cm/s), `Material` instanced on every slot with the dress of `Look`. False if a piece is missing:
	 * the villager then keeps its card at every distance.
	 */
	bool SetBody(USkeletalMesh* Mesh, UBlendSpace* Locomotion, UMaterialInterface* Material, const AnastasisVillagerLooks::FBodyLook& Look);

	UFUNCTION(BlueprintPure, Category = "Anastasis|Villagers")
	bool HasBody() const;

	/** True when the body, not the card, is drawn this frame. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Villagers")
	bool IsShowingBody() const { return bShowingBody; }

	/** Ground speed the body is animated at, cm/s (smoothed). */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Villagers")
	float GetBodySpeed() const { return BodySpeed; }

	/** Where the body faces, degrees (world yaw of its walking direction). */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Villagers")
	float GetBodyHeading() const { return BodyHeading; }

	/** Card or body, by hand (a lineup has no player camera to measure the distance from). */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Villagers")
	void ShowBody(bool bBody);

	virtual void Tick(float DeltaSeconds) override;

private:
	void BuildCard();
	/** Speed and heading from the feet's motion since the last frame. */
	void UpdateBodyMotion(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Anastasis|Villagers")
	TObjectPtr<USceneComponent> FeetRoot;

	UPROPERTY(VisibleAnywhere, Category = "Anastasis|Villagers")
	TObjectPtr<UProceduralMeshComponent> Card;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PortraitMaterial;

	UPROPERTY(VisibleAnywhere, Category = "Anastasis|Villagers")
	TObjectPtr<USkeletalMeshComponent> Body;

	FName LookId;
	bool bMirrored = false;
	FVector LastStep = FVector::ZeroVector;

	bool bShowingBody = false;
	float BodyScale = 1.0f;
	float BodySpeed = 0.0f;
	float BodyHeading = 0.0f;
	bool bHasLastFeet = false;
	FVector LastFeet = FVector::ZeroVector;
};
