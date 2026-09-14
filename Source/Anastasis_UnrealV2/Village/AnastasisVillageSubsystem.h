// Le registre des lieux utilisables du village.
//
// Ce sous-systeme est la seule porte entre l'intention de la simulation et le
// framework Smart Objects d'Unreal. Il n'ajoute pas un second systeme de
// reservation par-dessus celui du moteur : la verite de « qui tient quelle
// place » vit dans SmartObjectSubsystem, et ici on ne garde que ce que le
// moteur ne sait pas -- l'adresse stable cote simulation de chaque place.
//
// CE QU'IL FAIT
//
//   RegisterBuilding    un batiment genere devient N places reservables
//   FindInteractions    « ou puis-je faire X, pres de P, dans un rayon R »
//   ClaimInteraction    reserve une place, exclusivement
//   BeginInteraction    l'occupation commence
//   ReleaseInteraction  la place redevient libre
//   UnregisterBuilding  le batiment disparait, ses places avec lui
//
// CE QU'IL NE FAIT PAS
//
// Il ne decide jamais qu'un PNJ a faim, ni quel metier il exerce, ni combien de
// pain sort d'un four. Il ne connait ni besoins, ni economie, ni demographie.
// Rendre un slot « meilleur » qu'un autre pour une autre raison que la distance
// serait deja une decision, donc une usurpation de l'autorite de la simulation.
//
// AUCUNE DEPENDANCE A UNE SCENE EDITEUR
//
// Rien ici ne passe par USmartObjectComponent, ASmartObjectPersistentCollection
// ni un USmartObjectDefinition sauvegarde en asset. Les definitions sont
// construites en memoire a l'enregistrement, et l'objet entre dans la simulation
// par USmartObjectSubsystem::CreateSmartObject -- le chemin dynamique, sans
// acteur. Le village ANASTASIS etant genere, un lieu qu'il faudrait poser a la
// main dans un .umap ne serait pas un lieu du village.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Village/AnastasisVillageInteraction.h"

#include "AnastasisVillageSubsystem.generated.h"

class USmartObjectDefinition;
class USmartObjectSubsystem;

/** Ce que le registre retient d'un batiment enregistre. */
USTRUCT()
struct FAnastasisRegisteredBuilding
{
	GENERATED_BODY()

	UPROPERTY()
	FName BuildingId;

	UPROPERTY()
	FGameplayTag BuildingTag;

	/**
	 * Definition construite a l'enregistrement, gardee ici pour deux raisons :
	 * FSmartObjectRuntime n'en detient qu'un pointeur non possedant, et le
	 * ramasse-miettes n'a aucune autre raison de la garder vivante.
	 */
	UPROPERTY()
	TObjectPtr<USmartObjectDefinition> Definition;

	/**
	 * Adresse simulation de chaque slot Smart Object, indexee par l'index de slot
	 * du moteur. C'est ce tableau qui rend le chemin retour possible : un
	 * FSmartObjectSlotHandle donne un index, l'index donne une adresse, et
	 * l'adresse a un sens pour la simulation.
	 */
	UPROPERTY()
	TArray<FAnastasisInteractionAddress> SlotAddresses;

	/** Activite de chaque slot, meme indexation. Evite de relire la definition a chaque resultat. */
	UPROPERTY()
	TArray<FGameplayTag> SlotActivities;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;
};

UCLASS()
class UAnastasisVillageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAnastasisVillageSubsystem* Get(const UWorld* World);

	// --- Cycle de vie d'un lieu ---------------------------------------------

	/**
	 * Enregistre un batiment et toutes ses places d'interaction.
	 *
	 * @param Spec      le batiment tel que la simulation le declare
	 * @param OutHandle poignee Smart Object du batiment, valide seulement si le retour est Succeeded
	 * @return          Succeeded, ou la raison nommee du refus
	 *
	 * Echoue sans rien enregistrer partiellement : un spec invalide est refuse
	 * avant le premier appel au moteur.
	 */
	EAnastasisVillageRegistrationResult RegisterBuilding(const FAnastasisBuildingSpec& Spec, FSmartObjectHandle& OutHandle);

	/**
	 * Retire un batiment de la simulation.
	 *
	 * Les reservations en cours sur ses places sont avortees par le moteur
	 * (USmartObjectSubsystem::DestroySmartObject). Un FAnastasisInteractionClaim
	 * detenu ailleurs devient alors invalide : le relacher ensuite echoue, ce qui
	 * est le comportement voulu -- mieux vaut un release qui renvoie false qu'une
	 * place fantome eternellement prise.
	 */
	bool UnregisterBuilding(FSmartObjectHandle BuildingHandle);

	/** Retire tout. Appele en fin de monde ; expose pour les tests et un futur rechargement de village. */
	void UnregisterAllBuildings();

	// --- Requetes ------------------------------------------------------------

	/**
	 * Toutes les places libres accueillant ActivityTag, triees par distance croissante.
	 *
	 * Rayon : distance euclidienne de Origin a la PLACE, pas au batiment. Le
	 * moteur ne sait filtrer spatialement que sur l'origine de l'objet, et par
	 * boite ; on elargit donc la boite du plus grand offset de slot enregistre
	 * pour que le prefiltre moteur soit un sur-ensemble, puis on applique le test
	 * spherique exact sur la place. Sans cet elargissement, une margelle a
	 * portee serait manquee parce que le centre du puits, lui, est hors boite.
	 *
	 * Le tri est totalement ordonne : a distance egale, l'adresse tranche. La
	 * grille spatiale du moteur n'a pas d'ordre d'iteration garanti, et deux
	 * executions de la meme graine doivent choisir le meme lit.
	 *
	 * @param UserTags tags de l'agent, confrontes aux RequiredUserTags des slots
	 */
	bool FindInteractions(
		const FGameplayTag& ActivityTag,
		const FVector& Origin,
		double Radius,
		const FGameplayTagContainer& UserTags,
		TArray<FAnastasisInteractionQueryResult>& OutResults) const;

	/** La plus proche. Meme contrat que FindInteractions. */
	bool FindNearestInteraction(
		const FGameplayTag& ActivityTag,
		const FVector& Origin,
		double Radius,
		const FGameplayTagContainer& UserTags,
		FAnastasisInteractionQueryResult& OutResult) const;

	// --- Reservation ---------------------------------------------------------

	/**
	 * Reserve la place decrite par un resultat de requete.
	 *
	 * Exclusivite : a priorite egale (Normal), une place deja reservee est
	 * refusee. C'est la semantique du moteur, pas une couche ajoutee ici.
	 * Une capacite N se traduit par N places distinctes, donc N reservations
	 * concurrentes possibles, puis refus.
	 */
	bool ClaimInteraction(const FAnastasisInteractionQueryResult& Query, FAnastasisInteractionClaim& OutClaim);

	/** Passe la place de Claimed a Occupied. L'agent est arrive, l'acte commence. */
	bool BeginInteraction(const FAnastasisInteractionClaim& Claim);

	/** Libere la place et invalide le claim. Idempotent dans son effet : un claim deja invalide renvoie false sans rien casser. */
	bool ReleaseInteraction(FAnastasisInteractionClaim& Claim);

	// --- Lecture -------------------------------------------------------------

	/** Adresse simulation d'un slot moteur. Le chemin retour, expose pour tout consommateur. */
	bool ResolveAddress(const FSmartObjectSlotHandle& SlotHandle, FAnastasisInteractionAddress& OutAddress) const;

	/** Poignee d'un batiment par son identifiant simulation. */
	bool FindBuildingByIdentifier(FName BuildingId, FSmartObjectHandle& OutHandle) const;

	int32 GetRegisteredBuildingCount() const { return Buildings.Num(); }

	/** Nombre de places Smart Object enregistrees, capacites depliees. */
	int32 GetRegisteredSlotCount() const;

	/** Etat moteur d'une place. Invalid si la place n'existe plus. */
	ESmartObjectSlotState GetSlotState(const FSmartObjectSlotHandle& SlotHandle) const;

	/**
	 * Vrai si le runtime Smart Objects est pret a accepter des enregistrements.
	 * Faux avant UWorld::BeginPlay : y appeler RegisterBuilding renverrait
	 * SubsystemUnavailable plutot que de declencher l'ensure du moteur.
	 */
	bool IsReady() const;

	/** Trace les places enregistrees. Debug uniquement ; aucune place dans la logique. */
	void DrawDebugInteractions(float Duration) const;

	// UWorldSubsystem
	virtual void Deinitialize() override;

private:
	USmartObjectSubsystem* GetSmartObjects() const;

	/** Construit la definition en memoire correspondant a un spec. Deplie les capacites. */
	USmartObjectDefinition* BuildDefinition(const FAnastasisBuildingSpec& Spec, FAnastasisRegisteredBuilding& OutRecord);

	static EAnastasisVillageRegistrationResult ValidateSpec(const FAnastasisBuildingSpec& Spec);

	UPROPERTY()
	TMap<FSmartObjectHandle, FAnastasisRegisteredBuilding> Buildings;

	UPROPERTY()
	TMap<FName, FSmartObjectHandle> BuildingsByIdentifier;

	/**
	 * Plus grande distance entre l'origine d'un batiment et l'une de ses places,
	 * tous batiments confondus. Sert a elargir la boite de requete pour que le
	 * prefiltre spatial du moteur ne perde aucune place a portee. Ne redescend
	 * jamais : un registre qui se vide garderait une boite trop large, ce qui
	 * coute un peu de prefiltre et ne fausse aucun resultat.
	 */
	double MaxSlotOffsetLength = 0.0;
};
