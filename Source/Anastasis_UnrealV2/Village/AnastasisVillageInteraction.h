// Le contrat de donnees entre la simulation ANASTASIS et le monde Unreal.
//
//   SIMULATION -> FAnastasisBuildingSpec -> SmartObjectSubsystem
//   SmartObjectSubsystem -> FAnastasisInteractionAddress -> SIMULATION
//
// Le sens retour est la partie qui compte. Un `FSmartObjectSlotHandle` est un
// identifiant de session : il naît a l'enregistrement, meurt a la destruction du
// batiment, et ne survit ni a un rechargement ni a un rejeu. La simulation ne
// peut donc rien en faire. FAnastasisInteractionAddress est l'inverse : un
// triplet stable et lisible (batiment, slot, place) que la simulation a elle-meme
// fourni, et qu'elle retrouve intact quand Unreal lui rend un resultat.
//
// Toute information qui ne se deduit pas de la simulation est absente d'ici :
// pas de duree, pas de cout, pas de besoin satisfait, pas de metier. Un slot
// declare OU et COMMENT un acte peut avoir lieu. Il ne declare jamais POURQUOI.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectTypes.h"

#include "AnastasisVillageInteraction.generated.h"

/**
 * Comportement ANASTASIS attache a chaque slot.
 *
 * SmartObjectSubsystem::MarkSlotAsOccupied refuse d'occuper un slot qui ne porte
 * aucune definition de comportement du type demande -- il renvoie nullptr et
 * laisse le slot en Claimed. Cette classe est donc structurellement necessaire,
 * pas decorative : sans elle, Claim reussirait et Use echouerait en silence.
 *
 * Elle ne porte deliberement aucune logique. Le comportement reel est joue par
 * le StateTree du PNJ ; ce que la definition transporte, c'est l'activite
 * declaree, pour qu'un observateur (debug, StateTree, futur systeme d'animation)
 * sache quoi jouer sans reinterroger le registre.
 */
UCLASS()
class UAnastasisVillageBehavior : public USmartObjectBehaviorDefinition
{
	GENERATED_BODY()

public:
	/** Activite que ce slot accueille. Recopiee depuis le spec a l'enregistrement. */
	UPROPERTY(VisibleAnywhere, Category = "Anastasis|Village")
	FGameplayTag ActivityTag;

	/** Identifiant du batiment cote simulation. Voir FAnastasisInteractionAddress. */
	UPROPERTY(VisibleAnywhere, Category = "Anastasis|Village")
	FName BuildingId;

	/** Identifiant du slot cote simulation. */
	UPROPERTY(VisibleAnywhere, Category = "Anastasis|Village")
	FName SlotId;
};

/**
 * Adresse stable d'une place d'interaction, du point de vue de la simulation.
 *
 * `BuildingId` et `SlotId` viennent de la simulation et n'ont de sens que pour
 * elle. `Occurrence` est l'index de la place au sein d'un slot de capacite > 1 :
 * un puits a trois margelles declare un seul `SlotId` « rim » de capacite 3, et
 * les trois places se distinguent par Occurrence 0, 1, 2. La simulation peut
 * l'ignorer si la place exacte lui est indifferente.
 */
USTRUCT(BlueprintType)
struct FAnastasisInteractionAddress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FName BuildingId;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FName SlotId;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	int32 Occurrence = INDEX_NONE;

	bool IsValid() const
	{
		return !BuildingId.IsNone() && !SlotId.IsNone() && Occurrence >= 0;
	}

	bool operator==(const FAnastasisInteractionAddress& Other) const
	{
		return BuildingId == Other.BuildingId && SlotId == Other.SlotId && Occurrence == Other.Occurrence;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%s/%s#%d"), *BuildingId.ToString(), *SlotId.ToString(), Occurrence);
	}
};

/**
 * Une activite qu'un batiment expose, telle que la simulation la declare.
 *
 * La capacite n'est pas un compteur : elle est depliee en autant de slots Smart
 * Object qu'annonce. Un slot Smart Object est exclusif par construction -- une
 * reservation a priorite egale est refusee -- donc Capacity == 1 signifie
 * « exclusif » et Capacity == N signifie « N utilisateurs simultanes, chacun sur
 * sa propre place ». Il n'existe pas de troisieme mode : pas de slot partage
 * sans limite, parce qu'un tel slot ne pourrait plus rien refuser, et qu'un
 * systeme de reservation qui ne refuse rien ne reserve rien.
 */
USTRUCT(BlueprintType)
struct FAnastasisInteractionSlotSpec
{
	GENERATED_BODY()

	/** Identifiant du slot, stable et unique au sein du batiment. Vient de la simulation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village")
	FName SlotId;

	/** Activite accueillie. Un slot sans activite est refuse a l'enregistrement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village")
	FGameplayTag ActivityTag;

	/** Position d'interaction, dans le repere du batiment. Ignoree si OccurrencePoses est renseignee. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village")
	FVector LocalOffset = FVector::ZeroVector;

	/** Orientation que prend l'occupant, dans le repere du batiment. Ignoree si OccurrencePoses est renseignee. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village")
	FRotator LocalRotation = FRotator::ZeroRotator;

	/** Nombre d'occupants simultanes. Depliee en autant de slots Smart Object. Minimum 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village", meta = (ClampMin = "1"))
	int32 Capacity = 1;

	/**
	 * Pose propre a chaque occupant, dans le repere du batiment.
	 *
	 * Vide : les Capacity places partagent LocalOffset/LocalRotation. Acceptable
	 * pour une file d'attente abstraite, faux pour trois lits -- ils seraient tous
	 * au meme point, et un PNJ traverserait un autre PNJ pour y aller.
	 *
	 * Renseignee : doit couvrir au moins Capacity entrees, sans quoi le spec est
	 * refuse. L'occurrence N prend OccurrencePoses[N].
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village")
	TArray<FTransform> OccurrencePoses;

	/**
	 * Tags que l'utilisateur doit TOUS porter. Vide = ouvert a tous.
	 * Sert aux conditions d'acces que la simulation a deja tranchees (propriete
	 * d'un lit, appartenance a un atelier) ; ce n'est pas un mecanisme de
	 * decision, seulement l'application d'une decision deja prise.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village")
	FGameplayTagContainer RequiredUserTags;

	/** Un slot desactive reste enregistre et trouvable en debug, mais n'est jamais retourne par une requete. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village")
	bool bInitiallyEnabled = true;
};

/**
 * Un batiment tel que la simulation le declare au monde Unreal.
 *
 * Aucun mesh, aucune classe d'acteur, aucun asset : l'apparence est le chantier
 * d'autres agents, et la fondation d'interaction doit pouvoir etre enregistree
 * et testee sans qu'un seul triangle existe.
 */
USTRUCT(BlueprintType)
struct FAnastasisBuildingSpec
{
	GENERATED_BODY()

	/** Identifiant du batiment, stable cote simulation. Deux batiments ne peuvent pas le partager. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village")
	FName BuildingId;

	/** Categorie de lieu. Debug et presentation ; ne route aucune intention. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village")
	FGameplayTag BuildingTag;

	/** Pose du batiment dans le monde. Les offsets de slot s'y composent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village")
	FTransform Transform = FTransform::Identity;

	/** Activites exposees. Un batiment sans slot est refuse : il ne serait pas un lieu, juste une position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anastasis|Village")
	TArray<FAnastasisInteractionSlotSpec> Slots;
};

/** Une place libre trouvee par une requete. */
USTRUCT(BlueprintType)
struct FAnastasisInteractionQueryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FSmartObjectHandle SmartObjectHandle;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FSmartObjectSlotHandle SlotHandle;

	/** Le chemin de retour vers la simulation. */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FAnastasisInteractionAddress Address;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FGameplayTag ActivityTag;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FGameplayTag BuildingTag;

	/** Pose monde de la place : ou se tenir, et dans quel sens. */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FTransform SlotTransform = FTransform::Identity;

	/** Distance de l'origine de la requete a cette place. C'est la cle du tri. */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	double Distance = 0.0;

	bool IsValid() const { return SlotHandle.IsValid() && Address.IsValid(); }
};

/**
 * Une place reservee.
 *
 * Tant qu'une reservation vit, aucun autre agent ne peut prendre la place a
 * priorite egale. Elle doit etre relachee -- explicitement, ou par destruction
 * du batiment. Un claim qu'on oublie est une place perdue pour la duree de la
 * partie : c'est la raison d'etre de UAnastasisVillagerInteractionComponent,
 * qui relache dans EndPlay.
 */
USTRUCT(BlueprintType)
struct FAnastasisInteractionClaim
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FSmartObjectClaimHandle ClaimHandle;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FAnastasisInteractionAddress Address;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FGameplayTag ActivityTag;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis|Village")
	FTransform SlotTransform = FTransform::Identity;

	bool IsValid() const { return ClaimHandle.IsValid() && Address.IsValid(); }

	void Invalidate()
	{
		ClaimHandle.Invalidate();
		Address = FAnastasisInteractionAddress();
	}
};

/** Pourquoi un enregistrement a echoue. Le refus est nomme : un `false` nu ne se diagnostique pas. */
UENUM(BlueprintType)
enum class EAnastasisVillageRegistrationResult : uint8
{
	Succeeded,
	/** BuildingId vide. La simulation doit nommer ce qu'elle enregistre. */
	MissingBuildingId,
	/** Un batiment porte deja cet identifiant. */
	DuplicateBuildingId,
	/**
	 * Aucun slot, ou un slot sans SlotId, sans activite, de capacite < 1, ou dont
	 * OccurrencePoses est renseignee sans couvrir Capacity.
	 */
	InvalidSlots,
	/** Deux slots du meme batiment partagent un SlotId. */
	DuplicateSlotId,
	/** Le SmartObjectSubsystem est absent ou son runtime n'est pas initialise (avant BeginPlay). */
	SubsystemUnavailable,
};

ANASTASIS_UNREALV2_API const TCHAR* LexToString(EAnastasisVillageRegistrationResult Result);
