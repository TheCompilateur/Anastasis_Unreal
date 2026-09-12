#pragma once

#include "CoreMinimal.h"
#include "AnastasisWorldContract.generated.h"

class FJsonObject;

/**
 * Frontiere de donnees entre la simulation ANASTASIS et un runtime produit.
 *
 * Traduction de `ANASTASIS_WONDERLAND_PROBE/js/anastasis-world-contract.js`.
 * Ce fichier n'inclut deliberement ni Engine, ni rendu : cote JS il n'importe
 * ni Wonderland ni Three.js, et c'est cette propriete-la qui le rend portable.
 *
 * DEUX CONVENTIONS QUI NE SE DEVINENT PAS :
 *
 * 1. UNITES ET REPERE. Les coordonnees sont celles de la SOURCE
 *    (`C:\dev\Jeux IV Kingdoms\src\`) : plan X/Y, unites de tuile, telles que
 *    la simulation les ecrit. Aucune conversion vers les centimetres Unreal ni
 *    vers le Z-up n'est faite ici, et il ne faut pas en ajouter : le contrat
 *    est ce que les deux cotes comparent pendant la porte de parite [SCN]. La
 *    conversion appartient a la couche de projection qui posera les acteurs
 *    dans le niveau, en un seul endroit, apres le contrat.
 *
 * 2. `double`, PAS `float`. Chaque nombre du contrat est un `Number` JS, donc
 *    un double IEEE-754. Un `float` cote Unreal perdrait des bits a chaque
 *    conversion et ferait diverger un checkpoint C++ d'un checkpoint JS sans
 *    qu'aucune regle du monde ait change — une divergence de parite fabriquee
 *    par le port lui-meme, exactement le genre de faux positif que le piege
 *    n.1 de PITFALLS_FROM_WONDERLAND.md a coute une journee a isoler.
 *
 * CONVENTION `null` : cote JS, `stringOrNull` / `textOrNull` produisent `null`
 * pour une valeur absente ou vide. Cote C++, une `FString` VIDE porte ce
 * `null`. La distinction JS entre `""` et `null` n'existe pas dans le contrat
 * source (les deux passent par le meme helper et ressortent `null`), donc
 * aucune information n'est perdue par cet aplatissement.
 */

/** `anastasis-world-contract.js:6` — toute divergence de version invalide l'instantane. */
ANASTASISCORE_API extern const TCHAR* const AnastasisWorldContractVersion;

/**
 * Resultat d'une validation de contrat.
 *
 * Les chaines d'erreur sont RECOPIEES MOT POUR MOT depuis
 * `validateAnastasisWorld` (anastasis-world-contract.js:139-168) : un outil de
 * comparaison qui lit les deux cotes doit voir le meme texte pour le meme
 * defaut, sinon la porte [SCN] compare des vocabulaires au lieu d'etats.
 */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisValidation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	TArray<FString> Errors;
};

/** `copyPoint` (anastasis-world-contract.js:19) : un point non fini vaut `null`. */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisPoint2D
{
	GENERATED_BODY()

	/** Faux = `null` cote JS. Ne pas lire X/Y quand ce drapeau est faux. */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	bool bIsSet = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	double X = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	double Y = 0.0;
};

/** `copyActor(...).activity` — ce que l'acteur FAIT, pas ce a quoi il ressemble. */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisActivity
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	FString JobId;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	FString State;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	FString Goal;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	FString WorkplaceId;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	FString DestinationBuildingId;

	/** `copyPoint(actor.pathGoal) ?? copyPoint(actor.target)` — dans cet ordre. */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	FAnastasisPoint2D Destination;
};

/** `copyActor(...).needs`. Les bornes appartiennent a la source, pas au contrat. */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisNeeds
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Hunger = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Energy = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Thirst = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Social = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Leisure = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Hygiene = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Health = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Morale = 0.0;
};

/** `copyActor(...).navigation` — etat de cheminement, tous entiers non negatifs. */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisNavigation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") int32 PathLength = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") int32 PathIndex = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") int32 StuckTicks = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") int32 StuckStage = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") bool bAwaitingPath = false;
};

/** `copyActor` (anastasis-world-contract.js:27). Copie detachee : jamais mutee vers la source. */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisActor
{
	GENERATED_BODY()

	/** `npc-<n>` cote source (Jeux IV Kingdoms/src/sim/simulation.js:3128). Vide = `null`. */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double X = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Y = 0.0;

	/**
	 * Identite visuelle minimale. Ces champs sont COPIES, jamais interpretes
	 * ici : la simulation reste proprietaire de leur sens, et ce module ne
	 * decide pas ce qu'un `jobId` implique a l'ecran.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FString JobId;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FString Gender;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FString LifeStage;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Age = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FString Biome;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FString SocialClass;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FString HomeId;

	/**
	 * SITUATION, pas apparence. Un habitant entre dans un batiment garde les
	 * coordonnees de l'entree : sans ce champ, un runtime le dessine debout
	 * dehors, immobile, alors qu'il dort ou mange a l'interieur.
	 * Mesure graine 4242, une journee echantillonnee : 48,7 % du temps.
	 * (Mesure heritee de anastasis-world-contract.js:47-51 — a refaire cote
	 * Unreal seulement si la SOURCE change, pas parce que le port a change.)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	FString InsideBuildingId;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FAnastasisActivity Activity;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FAnastasisNeeds Needs;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FAnastasisNavigation Navigation;
};

/** `copyBuilding` (anastasis-world-contract.js:76). */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisBuilding
{
	GENERATED_BODY()

	/** `building-<n>` cote source (Jeux IV Kingdoms/src/sim/simulation.js:5680). */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FString Type;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double X = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Y = 0.0;

	/** Borne a [0,1] ; ABSENT vaut 1 (batiment acheve), pas 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Progress = 1.0;
};

/** Le point d'ancrage du peuplement. `bIsSet` faux = `settlement: null`. */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisSettlement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") bool bIsSet = false;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double X = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Y = 0.0;
};

/** `dimensions` — largeur et hauteur du monde en tuiles. */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisDimensions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") int32 Width = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") int32 Height = 0;
};

/**
 * Instantane monde detache et serialisable.
 *
 * `projectAnastasisWorld` (anastasis-world-contract.js:104) n'a PAS d'equivalent
 * ici : cette fonction lit une `Simulation` JavaScript et ne peut pas etre
 * traduite sans traduire la simulation elle-meme — c'est precisement la
 * question laissee ouverte par la section FORK de UNREAL_EXTRACTION_MAP_V1.md.
 * Cote Unreal l'instantane ENTRE (depuis JSON, option A) ou est PRODUIT par un
 * `IAnastasisWorldSource` (option B). Le contrat est identique dans les deux
 * cas, ce qui est exactement pourquoi il pouvait etre porte avant l'arbitrage.
 */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisWorldSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	FString ContractVersion;

	/**
	 * `Number(simulation.seed) >>> 0` : entier non signe 32 bits.
	 * Stocke en `int64` parce que `uint32` n'est pas un type d'UPROPERTY —
	 * un `int32` retournerait la moitie haute des graines en un negatif
	 * silencieux, et deux runs de graines differentes deviendraient
	 * indistinguables dans un checkpoint.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis")
	int64 Seed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Time = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") int32 Day = 1;

	/** Position dans la journee, dans [0,1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double DayFraction = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FAnastasisDimensions Dimensions;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") FAnastasisSettlement Settlement;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") TArray<FAnastasisActor> Actors;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") TArray<FAnastasisBuilding> Buildings;

	/** `copyStock(simulation.market.stock)` — quantites bornees a >= 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") TMap<FString, double> Resources;
};

/**
 * Producteur d'instantanes. C'est le seul point d'entree de la verite monde.
 *
 * Sous l'option A (runtime JS embarque) l'implementation decode du JSON ;
 * sous l'option B (coeur reimplemente en C++) elle projette un etat natif.
 * Rien d'autre dans ce module ne sait laquelle des deux tourne — c'est ce qui
 * permet d'ecrire la suite avant que la section FORK soit tranchee.
 */
class ANASTASISCORE_API IAnastasisWorldSource
{
public:
	virtual ~IAnastasisWorldSource() = default;

	/** Faux = pas d'instantane disponible ; NE PAS inventer d'etat monde. */
	virtual bool ProjectWorld(FAnastasisWorldSnapshot& OutSnapshot) const = 0;
};

namespace AnastasisWorld
{
	/**
	 * `validateAnastasisWorld` (anastasis-world-contract.js:139).
	 *
	 * Quatre controles JS n'apparaissent pas ici parce que le TYPE les rend
	 * inatteignables, et non parce qu'ils ont ete abandonnes : « snapshot is
	 * not an object », « actors is not an array », « buildings is not an
	 * array », « resources are invalid ». Un `FAnastasisWorldSnapshot` mal
	 * forme ne compile pas. Les memes defauts restent detectables a l'entree
	 * JSON, ou ils sont reellement possibles : voir `FromJson`.
	 */
	ANASTASISCORE_API FAnastasisValidation Validate(const FAnastasisWorldSnapshot& Snapshot);

	/** `assertAnastasisWorld` : rapporte, ne fabrique jamais d'etat de secours. */
	ANASTASISCORE_API bool Assert(const FAnastasisWorldSnapshot& Snapshot, FString& OutError);

	/**
	 * Decode un instantane produit par le cote JS.
	 *
	 * Les noms de champs sont ceux du contrat source, en camelCase, sans
	 * exception : c'est le meme document qui traverse les deux runtimes.
	 * Retourne faux et remplit `OutError` si la structure est absente ou mal
	 * typee — les defauts que `Validate` ne peut plus voir une fois la
	 * structure C++ construite.
	 */
	ANASTASISCORE_API bool FromJson(const TSharedPtr<FJsonObject>& Json,
		FAnastasisWorldSnapshot& OutSnapshot, FString& OutError);

	/**
	 * Reencode l'instantane au format du contrat.
	 *
	 * Sert la porte [SCN] : meme graine, meme sequence de commandes, deux
	 * checkpoints a comparer. COMPARER LES VALEURS DECODEES, JAMAIS LE TEXTE :
	 * le formatage des doubles differe entre `JSON.stringify` et le
	 * serialiseur Unreal, et un diff textuel signalerait une divergence de
	 * monde la ou il n'y a qu'une difference de chiffres imprimes.
	 */
	ANASTASISCORE_API TSharedRef<FJsonObject> ToJson(const FAnastasisWorldSnapshot& Snapshot);
}
