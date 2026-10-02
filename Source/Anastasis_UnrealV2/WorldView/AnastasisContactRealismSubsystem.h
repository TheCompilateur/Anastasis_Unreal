#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldView/AnastasisContactRealism.h"
#include "AnastasisContactRealismSubsystem.generated.h"

class AActor;
class AAnastasisWorldEmbodiment;
class UDecalComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UPrimitiveComponent;

/**
 * AAA_CONTACT_REALISM_001, cote monde.
 *
 * Lit l'incarnation (AAnastasisWorldEmbodiment) SANS la modifier : ses HISM donnent les arbres,
 * rochers, roseaux et billes reellement poses, AnastasisTerrainForge le sol et l'eau rendus,
 * le reseau de drainage l'eau vive et calme. Le planificateur pur (AnastasisContactRealism)
 * decide ; ce sous-systeme pose des decalques DBuffer (MI_ACR_*) et des cailloux dans un acteur
 * transitoire qui lui appartient. Aucun fichier d'un autre proprietaire n'est touche.
 *
 * Mondes de jeu : la couche se reconstruit seule, une fois l'incarnation stable. Monde d'editeur
 * (pas de tick) : `anastasis.Contact.Rebuild` apres EmbodyCanonical.
 */
UCLASS()
class UAnastasisContactRealismSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Rebatit la couche depuis l'incarnation courante. Faux = rien a poser (journalise pourquoi). */
	bool Rebuild();
	/** Retire decalques et cailloux, garde les composants de cailloux (assertion HISM, cf. PlaceGroundCover). */
	void Clear();
	/**
	 * Diffusion autour d'un point de vue : seuls les decalques a moins de `anastasis.Contact.Radius` sont
	 * des composants ; le plan, lui, couvre tout le monde. Point de vue = premier joueur en jeu, sinon la
	 * valeur forcee (`anastasis.Contact.Focus x y`, editeur et preuves), sinon le centre du sol rendu.
	 */
	void UpdateLive(bool bForce);
	void SetFocusOverride(bool bOn, const FVector2D& XY);
	/** Une ligne par famille : ce qui est pose et ce qui a ete examine. */
	void LogStatus() const;
	/** Ecrit le dernier plan en JSON : preuve et visee des cameras, pas un format stable. */
	void DumpPlan(const FString& Path) const;

	int32 GetDecalCount() const { return Live.Num(); }
	int32 GetPlannedDecalCount() const { return LastPlan.Decals.Num(); }
	int32 GetPebbleCount() const;
	const AnastasisContactRealism::FPlan& GetPlan() const { return LastPlan; }

private:
	AAnastasisWorldEmbodiment* FindEmbodiment() const;
	uint64 EmbodimentSignature(const AAnastasisWorldEmbodiment& Embodiment) const;
	void GatherAnchors(const AAnastasisWorldEmbodiment& Embodiment, TArray<AnastasisContactRealism::FAnchor>& Out) const;
	AActor* EnsureActor();
	void SuppressVegetationReceivers(const AAnastasisWorldEmbodiment& Embodiment);
	UMaterialInterface* DecalMaterial(AnastasisContactRealism::EDecal Kind);

	UPROPERTY(Transient)
	TObjectPtr<AActor> HostActor;
	/** Index dans LastPlan.Decals -> composant vivant. */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UDecalComponent>> Live;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> PebbleMeshes;
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UMaterialInterface>> Materials;

	/** Composants de vegetation dont on a coupe la reception des decalques ; restitues par Clear(). */
	TArray<TWeakObjectPtr<UPrimitiveComponent>> ReceiveSwitched;
	AnastasisContactRealism::FPlan LastPlan;
	bool bFocusOverride = false;
	FVector2D FocusOverride = FVector2D::ZeroVector;
	FVector2D LiveFocus = FVector2D::ZeroVector;
	bool bLiveBuilt = false;
	uint64 BuiltSignature = 0;
	uint64 PendingSignature = 0;
	int32 PendingPolls = 0;
	int32 BuiltEnabled = -1;
	float PollClock = 0.0f;
};
