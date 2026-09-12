#pragma once

#include "CoreMinimal.h"
#include "AnastasisWorldContract.h"
#include "AnastasisInfrastructure.generated.h"

class FJsonObject;

/**
 * Frontiere d'infrastructure ANASTASIS portable, cote Unreal.
 *
 * Traduction de `ANASTASIS_WONDERLAND_PROBE/js/anastasis-infrastructure.js`.
 * Ce module n'a aucune dependance vers Wonderland, Three.js, ou le depot
 * source : la simulation courante est INJECTEE par un adaptateur, de sorte que
 * le runtime produit ne depend que de cette interface semantique.
 *
 * `createAnastasisInfrastructure` recoit cote JS une classe de simulation
 * qu'il instancie lui-meme. Ici c'est l'appelant qui construit son
 * `IAnastasisSimulation` et le passe deja vivant : le C++ n'a pas d'equivalent
 * a « passer une classe », et le faire fabriquer par une fabrique injectee
 * n'ajouterait qu'une indirection sans changer qui possede la simulation.
 */

/** `SIM_FIXED_DT` (anastasis-kernel.js:19) — pas fixe semantique, 60 Hz. */
ANASTASISCORE_API extern const double AnastasisSimFixedDt;

/** `SIM_MAX_STEP_MULT` (anastasis-kernel.js:20) — plafond du multiplicateur de pas. */
ANASTASISCORE_API extern const int32 AnastasisSimMaxStepMult;

/** `ANASTASIS_INFRASTRUCTURE_CONTRACT_VERSION`. */
ANASTASISCORE_API extern const TCHAR* const AnastasisInfrastructureContractVersion;

/**
 * Plan de pas pour une trame hote.
 *
 * `simStepPlan` (anastasis-kernel.js:100) : au-dela de la vitesse 1, le monde
 * n'avance pas en faisant plus de pas mais en faisant des pas PLUS GROS,
 * plafonnes a `AnastasisSimMaxStepMult`, avec au plus deux pas par trame.
 */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisStepPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double StepDt = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") int32 TargetSteps = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double TimePerFrame = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double TimeGain = 1.0;
};

/**
 * Commande joueur.
 *
 * `Payload` reste un objet JSON et non une structure typee : le vocabulaire
 * des commandes appartient a la simulation, pas a la frontiere. Le contrat
 * n'exige qu'une chose de la charge utile — etre un OBJET, jamais un tableau
 * ni un scalaire.
 */
struct ANASTASISCORE_API FAnastasisCommand
{
	FString Id;
	FString Kind;
	TSharedPtr<FJsonObject> Payload;
};

/** Accuse de reception d'une commande. Un refus porte toujours un `Reason`. */
struct ANASTASISCORE_API FAnastasisCommandResult
{
	FString CommandId;
	FString Kind;
	bool bAccepted = false;

	/** `invalid-command`, `command-handler-unwired`, ou la raison de la simulation. */
	FString Reason;

	TArray<FString> Errors;

	/**
	 * Instantane joint UNIQUEMENT quand la commande est acceptee — comme le
	 * JS. Une commande refusee ne doit rien renvoyer qui ressemble a un etat
	 * monde, sinon un appelant negligent affiche l'ancien monde comme s'il
	 * avait ete mis a jour.
	 */
	bool bHasSnapshot = false;
	FAnastasisWorldSnapshot Snapshot;

	/** Charge utile libre du gestionnaire (`result` cote JS). */
	TSharedPtr<FJsonObject> Result;
};

/**
 * La simulation vue par la frontiere.
 *
 * Sous l'option A (runtime JS embarque) l'implementation delegue a la
 * `Simulation` JavaScript inchangee ; sous l'option B elle appelle un coeur
 * C++. Voir la section FORK de `unreal/UNREAL_EXTRACTION_MAP_V1.md` — cette
 * interface est ecrite pour que le choix reste ouvert, pas pour le prejuger.
 */
class ANASTASISCORE_API IAnastasisSimulation : public IAnastasisWorldSource
{
public:
	virtual ~IAnastasisSimulation() = default;

	/** Avance le monde de `Dt` secondes semantiques. */
	virtual void Tick(double Dt) = 0;

	/**
	 * Declare le point du monde que l'hote regarde reellement.
	 * Defaut : non cable, comme un `viewFocusHandler` nul cote JS.
	 */
	virtual bool SetViewFocus(double X, double Y) { return false; }

	/**
	 * Traite une commande deja validee.
	 * @return faux si aucun gestionnaire n'est cable — la frontiere repond
	 *         alors `command-handler-unwired` au lieu d'inventer un refus.
	 */
	virtual bool HandleCommand(const FAnastasisCommand& Command, FAnastasisCommandResult& OutResult)
	{
		return false;
	}
};

/**
 * La frontiere elle-meme.
 *
 * Construite par `Create` et non par un constructeur public parce que la
 * construction PEUT ECHOUER (pas fixe invalide, instantane initial hors
 * contrat) et qu'un objet a moitie construit ici serait un monde a moitie
 * vrai.
 */
class ANASTASISCORE_API FAnastasisInfrastructure
{
public:
	/**
	 * @param FixedDt pas fixe semantique, strictement positif et fini.
	 * @return null en cas d'echec, avec la raison dans `OutError`.
	 */
	static TSharedPtr<FAnastasisInfrastructure> Create(
		const TSharedRef<IAnastasisSimulation>& InSimulation, double FixedDt, FString& OutError);

	const TCHAR* GetContractVersion() const { return AnastasisInfrastructureContractVersion; }
	double GetFixedDt() const { return FixedDt; }
	int64 GetStepCount() const { return StepCount; }

	/** Vrai si la simulation a accepte un point de focus. Voir `SetViewFocus`. */
	bool IsViewFocused() const { return bViewFocused; }

	/**
	 * FOCUS SEMANTIQUE : le point du monde que l'hote regarde reellement.
	 *
	 * CE N'EST PAS une preoccupation de rendu que la simulation peut ignorer.
	 * La simulation source fait tourner chaque acteur a une cadence qui depend
	 * de sa DISTANCE A CE POINT (proche 60 Hz, moyen 10 Hz, loin 1 Hz, au-dela
	 * 0,25 Hz). Un hote qui ne declare jamais de focus le laisse a l'origine du
	 * monde, qui est un COIN de la carte : un peuplement au centre se retrouve
	 * alors hors de toutes les bandes et ses acteurs sont simules 240 fois trop
	 * lentement, avancant d'un intervalle accumule entier d'un coup. Ils se
	 * lisent comme des statues qui se teleportent de temps en temps.
	 *
	 * L'adaptateur possede la FACON dont la simulation courante implemente
	 * cela ; la frontiere ne possede que l'intention.
	 */
	bool SetViewFocus(double X, double Y);

	/** `simStepPlan(scale, fixedDt)`. */
	FAnastasisStepPlan StepPlan(double Scale = 1.0) const;

	/**
	 * Avancer le monde d'un pas. NE PROJETTE PAS.
	 *
	 * `step` rendait `snapshot()` cote JS, donc chaque pas construisait une
	 * projection complete qu'AUCUN appelant du depot ne lisait. Inoffensif tant
	 * que la projection etait maigre ; couteux des qu'elle porte les poses, la
	 * surcouche sociale, le fondu, l'identite et la carrure de chaque acteur.
	 * Mesure graine 4242, 2000 pas : 5545 ms avec projection contre 3607 ms
	 * sans. Qui veut un instantane appelle `Snapshot()`.
	 *
	 * @return faux si `Dt` n'est pas strictement positif et fini.
	 */
	bool Step(double Dt, FString& OutError);

	/** Variante qui utilise le pas fixe semantique. */
	bool Step(FString& OutError) { return Step(FixedDt, OutError); }

	/** Valide, puis delegue. Ne mute jamais le monde sur une commande invalide. */
	FAnastasisCommandResult Dispatch(const FAnastasisCommand& Command);

	/** Projette l'etat courant et le verifie contre le contrat monde. */
	bool Snapshot(FAnastasisWorldSnapshot& OutSnapshot, FString& OutError) const;

	/** `validateAnastasisCommand` (anastasis-infrastructure.js:17). */
	static FAnastasisValidation ValidateCommand(const FAnastasisCommand& Command);

	/** `simStepPlan` en fonction libre, pour les appelants sans frontiere. */
	static FAnastasisStepPlan MakeStepPlan(double Scale, double FixedDt);

private:
	FAnastasisInfrastructure(const TSharedRef<IAnastasisSimulation>& InSimulation, double InFixedDt);

	TSharedRef<IAnastasisSimulation> Simulation;
	double FixedDt;
	int64 StepCount = 0;
	bool bViewFocused = false;
};
