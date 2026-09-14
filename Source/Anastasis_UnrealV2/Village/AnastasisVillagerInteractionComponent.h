// Le cote agent du pont : ce qu'un villageois tient d'une interaction en cours.
//
// Pourquoi un composant et pas un etat dans le StateTree : une reservation
// traverse plusieurs etats de l'arbre (trouver, marcher, utiliser, relacher), et
// les donnees d'instance d'une tache StateTree ne survivent pas au changement
// d'etat. Un claim oublie entre deux etats est une place perdue pour la partie
// entiere. Le composant est donc le proprietaire de la reservation, et l'arbre
// n'est qu'un ordonnanceur qui lui parle.
//
// OU EST LA DECISION
//
// Nulle part ici. `SetIntent` est une boite aux lettres : la simulation y depose
// « ce villageois veut dormir », et le composant ne fait que la relayer. Il ne
// choisit pas l'activite, ne la deduit pas d'un etat interne, ne la declenche pas
// sur un minuteur. Si un jour ce fichier contient un `if (Fatigue > 0.8)`,
// l'autorite comportementale a change de camp et la mission a ete trahie.
//
// LE DEPLACEMENT N'EST PAS ICI NON PLUS
//
// Le composant expose la pose a atteindre et repond a « suis-je arrive ? ». Aller
// jusque la est le travail d'une tache de deplacement -- celle du moteur, ou
// celle qu'un futur chantier IA ecrira. Mettre un MoveTo ici forcerait un
// NavMesh, donc un niveau, donc une scene : la fondation cesserait d'etre
// testable a vide.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Village/AnastasisVillageInteraction.h"

#include "AnastasisVillagerInteractionComponent.generated.h"

class UAnastasisVillageSubsystem;

/**
 * Ou en est le villageois dans la sequence d'interaction.
 *
 * C'est la forme executable de la chaine demandee :
 *   Idle -> (intention) -> Reserved -> (deplacement) -> InUse -> Complete
 * avec Failed comme sortie nommee de tout refus.
 */
UENUM(BlueprintType)
enum class EAnastasisInteractionPhase : uint8
{
	/** Rien en cours. Une intention peut etre en attente sans qu'une place soit tenue. */
	Idle,
	/** Une place est reservee et attend son occupant. Personne d'autre ne peut la prendre. */
	Reserved,
	/** L'occupant est arrive, la place est Occupied, l'acte se joue. */
	InUse,
	/** L'acte est fini et la place relachee. */
	Complete,
	/** Aucune place trouvee, ou reservation refusee. La simulation decide de la suite. */
	Failed,
};

ANASTASIS_UNREALV2_API const TCHAR* LexToString(EAnastasisInteractionPhase Phase);

UCLASS(ClassGroup = (Anastasis), meta = (BlueprintSpawnableComponent))
class ANASTASIS_UNREALV2_API UAnastasisVillagerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAnastasisVillagerInteractionComponent();

	// --- Ce que la simulation depose ----------------------------------------

	/**
	 * Depose une intention. N'entreprend rien : la place n'est cherchee qu'au
	 * RequestInteraction suivant, ce qui laisse l'arbre maitre du moment.
	 */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Village")
	void SetIntent(FGameplayTag ActivityTag, double SearchRadius);

	UFUNCTION(BlueprintCallable, Category = "Anastasis|Village")
	void ClearIntent();

	UFUNCTION(BlueprintPure, Category = "Anastasis|Village")
	bool HasPendingIntent() const { return PendingActivity.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Anastasis|Village")
	FGameplayTag GetPendingActivity() const { return PendingActivity; }

	/** Identite de l'agent, confrontee aux RequiredUserTags des slots. Posee par la simulation. */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Village")
	void SetUserTags(const FGameplayTagContainer& InUserTags) { UserTags = InUserTags; }

	UFUNCTION(BlueprintPure, Category = "Anastasis|Village")
	const FGameplayTagContainer& GetUserTags() const { return UserTags; }

	// --- La sequence ---------------------------------------------------------

	/**
	 * Cherche et reserve une place pour l'intention en attente.
	 *
	 * Relache d'abord toute place encore tenue : un agent qui redemande sans avoir
	 * relache abandonnait sa place precedente, et elle serait restee prise.
	 *
	 * @return vrai si une place est reservee. Faux place la phase a Failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Village")
	bool RequestInteraction();

	/** Meme chose, en deposant l'intention au passage. */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Village")
	bool RequestInteractionFor(FGameplayTag ActivityTag, double SearchRadius);

	/** Vrai si le proprietaire est assez pres de la place reservee pour y agir. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Village")
	bool HasArrived() const;

	/**
	 * Passe la place de Reserved a InUse. Refuse si l'agent n'est pas arrive : sans
	 * cette garde, un PNJ dormirait a l'autre bout du village dans un lit qu'il
	 * tient a distance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Village")
	bool BeginUse();

	/** Relache la place et passe a Complete. L'intention est consommee. */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Village")
	bool CompleteAndRelease();

	/** Relache sans declarer l'acte accompli, et revient a Idle. L'intention reste en attente. */
	UFUNCTION(BlueprintCallable, Category = "Anastasis|Village")
	void AbandonInteraction();

	// --- Lecture -------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Anastasis|Village")
	EAnastasisInteractionPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Anastasis|Village")
	bool HasClaim() const { return Claim.IsValid(); }

	const FAnastasisInteractionClaim& GetClaim() const { return Claim; }

	/** Pose a atteindre. Identite si aucune place n'est tenue. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Village")
	FTransform GetInteractionTransform() const { return Claim.SlotTransform; }

	/** Adresse simulation de la place tenue, pour le compte rendu a la simulation. */
	UFUNCTION(BlueprintPure, Category = "Anastasis|Village")
	FAnastasisInteractionAddress GetInteractionAddress() const { return Claim.Address; }

	/** Distance a laquelle on considere l'agent arrive, en centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village", meta = (ClampMin = "1.0"))
	double ArrivalTolerance = 120.0;

	// UActorComponent
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UAnastasisVillageSubsystem* GetVillage() const;

	UPROPERTY(Transient)
	FAnastasisInteractionClaim Claim;

	UPROPERTY(Transient)
	EAnastasisInteractionPhase Phase = EAnastasisInteractionPhase::Idle;

	UPROPERTY(Transient)
	FGameplayTag PendingActivity;

	UPROPERTY(Transient)
	double PendingSearchRadius = 0.0;

	UPROPERTY(Transient)
	FGameplayTagContainer UserTags;
};
