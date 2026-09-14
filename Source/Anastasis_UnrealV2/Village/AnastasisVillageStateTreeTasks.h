// Le plus petit pont StateTree utile.
//
// Trois taches et une condition. Elles ne contiennent aucune logique : chacune
// appelle une methode de UAnastasisVillagerInteractionComponent et traduit son
// booleen en EStateTreeRunStatus. C'est deliberement mince -- le StateTree
// ORDONNE l'execution, il ne devient pas le cerveau du PNJ.
//
// L'arbre que ces noeuds permettent d'ecrire :
//
//   Idle
//     [HasInteractionIntent]            <- la simulation a depose une intention
//     -> FindInteraction                <- requete spatiale + reservation
//          -> MoveTo (InteractionLocation, tache moteur)
//               -> UseInteraction       <- occupe la place, tient la duree
//                    -> ReleaseInteraction
//                         -> Complete
//
// CE QUI RESTE DEHORS
//
// Le POURQUOI. Aucune de ces taches ne decide qu'un villageois a besoin de
// dormir : la condition ne fait que LIRE une intention deposee par la simulation.
// Si une tache se met un jour a choisir son propre ActivityTag, le StateTree est
// devenu l'autorite cognitive et la frontiere a saute.
//
// Le deplacement. `FindInteraction` publie la pose a atteindre en Output ; c'est
// une tache de deplacement -- celle du moteur -- qui la consomme. Reimplementer
// un MoveTo ici imposerait un NavMesh a une fondation qui doit rester testable
// dans un monde vide.
//
// ETAT DE VERIFICATION : ces noeuds sont verifies a la compilation et par les
// tests du composant qu'ils appellent. Aucun asset StateTree ne les cable, parce
// qu'un .uasset ne se produit pas sans editeur et n'a pas sa place dans une
// fondation qui doit tenir sans scene. Le cablage est un travail d'editeur, a
// faire le jour ou un PNJ reel existe.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeConditionBase.h"
#include "StateTreeTaskBase.h"

#include "AnastasisVillageStateTreeTasks.generated.h"

class UAnastasisVillagerInteractionComponent;

// ---------------------------------------------------------------------------
// Condition : une intention est-elle en attente ?
// ---------------------------------------------------------------------------

USTRUCT()
struct FAnastasisHasInteractionIntentConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UAnastasisVillagerInteractionComponent> InteractionComponent;

	/**
	 * Si renseigne, la condition n'est vraie que pour cette activite precise.
	 * Vide : n'importe quelle intention suffit.
	 */
	UPROPERTY(EditAnywhere, Category = "Condition")
	FGameplayTag RequiredActivity;
};

/** Lit l'intention deposee par la simulation. Ne la produit jamais. */
USTRUCT(meta = (DisplayName = "Has Interaction Intent", Category = "Anastasis|Village"))
struct FAnastasisHasInteractionIntentCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAnastasisHasInteractionIntentConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};

// ---------------------------------------------------------------------------
// Tache : trouver et reserver
// ---------------------------------------------------------------------------

USTRUCT()
struct FAnastasisFindInteractionTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UAnastasisVillagerInteractionComponent> InteractionComponent;

	/**
	 * Activite cherchee. Laisser vide pour utiliser l'intention en attente, ce qui
	 * est le cas normal : l'arbre execute ce que la simulation a decide.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag ActivityOverride;

	/** Rayon de recherche, en centimetres. Ignore si ActivityOverride est vide. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	double SearchRadius = 5000.0;

	/** Pose de la place reservee, a brancher sur une tache de deplacement. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FVector InteractionLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Output")
	FRotator InteractionRotation = FRotator::ZeroRotator;
};

/** Requete spatiale puis reservation. Succeeded si une place est tenue, Failed sinon. */
USTRUCT(meta = (DisplayName = "Find Village Interaction", Category = "Anastasis|Village"))
struct FAnastasisFindInteractionTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FAnastasisFindInteractionTask()
	{
		// Instantane : la requete aboutit ou echoue dans EnterState.
		bShouldCallTick = false;
	}

	using FInstanceDataType = FAnastasisFindInteractionTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

// ---------------------------------------------------------------------------
// Tache : occuper la place le temps de l'acte
// ---------------------------------------------------------------------------

USTRUCT()
struct FAnastasisUseInteractionTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UAnastasisVillagerInteractionComponent> InteractionComponent;

	/**
	 * Duree de l'acte, en secondes.
	 *
	 * C'est un parametre d'execution, pas une verite de simulation : combien de
	 * temps dure vraiment une nuit de sommeil ou une journee d'atelier appartient a
	 * l'horloge d'ANASTASIS. Ici, c'est le temps que l'incarnation passe a le
	 * montrer.
	 */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0"))
	float Duration = 1.0f;

	UPROPERTY()
	float Elapsed = 0.0f;
};

/**
 * Passe la place en Occupied, tient la duree, puis Succeeded.
 *
 * Ne relache pas : le relachement est une tache a lui, pour que l'arbre puisse
 * decider de garder la place entre deux actes. Un abandon d'etat laisse donc la
 * reservation en vie -- elle sera relachee par la requete suivante du composant,
 * ou par son EndPlay.
 */
USTRUCT(meta = (DisplayName = "Use Village Interaction", Category = "Anastasis|Village"))
struct FAnastasisUseInteractionTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAnastasisUseInteractionTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
};

// ---------------------------------------------------------------------------
// Tache : relacher
// ---------------------------------------------------------------------------

USTRUCT()
struct FAnastasisReleaseInteractionTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<UAnastasisVillagerInteractionComponent> InteractionComponent;

	/** Adresse simulation de la place liberee, a rendre a la simulation. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FName BuildingId;

	UPROPERTY(EditAnywhere, Category = "Output")
	FName SlotId;
};

/** Libere la place et publie son adresse, pour que la simulation sache ce qui a ete fait et ou. */
USTRUCT(meta = (DisplayName = "Release Village Interaction", Category = "Anastasis|Village"))
struct FAnastasisReleaseInteractionTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FAnastasisReleaseInteractionTask()
	{
		bShouldCallTick = false;
	}

	using FInstanceDataType = FAnastasisReleaseInteractionTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
