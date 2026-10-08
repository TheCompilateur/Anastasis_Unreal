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
 * One simulated villager as an animated 3D skeletal body. The actor origin is the villager's feet.
 * Source portraits guide the body palette; they are never drawn in a game world. A portrait card
 * remains available only for explicit editor lineups of the source references.
 * Missing body assets are reported by the presentation layer and never replaced by a PNG.
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

	/** Binds the style ID. A portrait and material may be supplied together for an editor-only reference lineup. */
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

	/** Puts the feet at `Feet` and remembers motion for animation and heading. */
	void MoveFeetTo(const FVector& Feet);

	/**
	 * Binds the 3D body: `Mesh` played through `Locomotion` (single-node blend space, X = direction
	 * relative to the facing, Y = speed in cm/s), `Material` instanced on every slot with the dress of `Look`.
	 * False if a piece is missing; the caller does not spawn a visual in that case.
	 */
	bool SetBody(USkeletalMesh* Mesh, UBlendSpace* Locomotion, UMaterialInterface* Material, const AnastasisVillagerLooks::FBodyLook& Look);

	UFUNCTION(BlueprintPure, Category = "Anastasis|Villagers")
	bool HasBody() const;

	/** True when the 3D body is drawn this frame. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Villagers")
	bool IsShowingBody() const { return bShowingBody; }

	/** Ground speed the body is animated at, cm/s (smoothed). */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Villagers")
	float GetBodySpeed() const { return BodySpeed; }

	/** Where the body faces, degrees (world yaw of its walking direction). */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Villagers")
	float GetBodyHeading() const { return BodyHeading; }

	/** Toggles the 3D body; editor-only reference lineups may reveal the portrait card. */
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

	bool bShowingBody = false;
	float BodyScale = 1.0f;
	float BodySpeed = 0.0f;
	float BodyHeading = 0.0f;
	bool bHasLastFeet = false;
	FVector LastFeet = FVector::ZeroVector;
};
