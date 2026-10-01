#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AnastasisVillagerVisual.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UTexture2D;

/**
 * The face of one simulated villager: a cut-out portrait on a vertical card (VILLAGER_PNG_001).
 *
 * The actor origin is the villager's FEET. The card is the shared 100 x 200 cm canvas, so the
 * stature painted into the PNG is the stature in the world. It turns about the vertical axis
 * only (a cylindrical billboard: it stays upright under a high camera), toward the first local
 * player's camera in a game world. Portraits are painted facing three-quarters LEFT; walking
 * toward the right of the screen mirrors the card, standing still keeps the last facing.
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

	virtual void Tick(float DeltaSeconds) override;

private:
	void BuildCard();

	UPROPERTY(VisibleAnywhere, Category = "Anastasis|Villagers")
	TObjectPtr<USceneComponent> FeetRoot;

	UPROPERTY(VisibleAnywhere, Category = "Anastasis|Villagers")
	TObjectPtr<UProceduralMeshComponent> Card;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PortraitMaterial;

	FName LookId;
	bool bMirrored = false;
	FVector LastStep = FVector::ZeroVector;
};
