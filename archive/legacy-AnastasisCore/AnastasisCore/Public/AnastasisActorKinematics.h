#pragma once

#include "CoreMinimal.h"
#include "AnastasisActorKinematics.generated.h"

/**
 * Cinematique d'acteur portable, extraite de la couture de pilotage du joueur.
 *
 * Traduction de `ANASTASIS_WONDERLAND_PROBE/js/anastasis-actor-kinematics.js`.
 *
 * CE MODULE NE POSSEDE QUE DEUX CHOSES : la normalisation du vecteur d'entree
 * et l'application des collisions et des bornes. Les facteurs de vitesse
 * physiologiques, les routes, la navigation, les compteurs de trafic et l'IA
 * d'acteur restent des proprietaires EXTERIEURS a cette tranche et doivent
 * etre injectes par leur adaptateur. C'est la raison pour laquelle cette
 * traduction pouvait etre faite avant l'arbitrage de la section FORK : elle ne
 * decide d'aucune regle de monde, elle applique un pas deja decide.
 */

/** `ANASTASIS_ACTOR_KINEMATICS_VERSION`. */
ANASTASISCORE_API extern const TCHAR* const AnastasisActorKinematicsVersion;

/** Vecteur de pilotage normalise. `bIsSet` faux = `null` (entree nulle). */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisDrive
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") bool bIsSet = false;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double X = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anastasis") double Y = 0.0;
};

/**
 * L'etat minimal qu'un pas de cinematique lit et ecrit.
 *
 * Ce n'est PAS un `FAnastasisActor` : le contrat monde est une copie detachee
 * en lecture seule, et faire avancer un acteur revient a ecrire dans l'etat
 * dont la simulation est proprietaire. Passer par une structure separee rend
 * cette distinction visible au lieu de la laisser dependre de la discipline
 * de l'appelant.
 */
USTRUCT(BlueprintType)
struct ANASTASISCORE_API FAnastasisKinematicBody
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Anastasis") double X = 0.0;
	UPROPERTY(BlueprintReadWrite, Category = "Anastasis") double Y = 0.0;

	/** `actor.inside` : un acteur a l'interieur d'un batiment ne se deplace pas. */
	UPROPERTY(BlueprintReadWrite, Category = "Anastasis") bool bInside = false;
};

/**
 * Parametres du monde possedant le pas.
 *
 * `Speed` est DEJA la vitesse effective apres les modificateurs specifiques a
 * la source ; ce module ne les connait pas et ne doit pas les recalculer.
 */
struct ANASTASISCORE_API FAnastasisKinematicParams
{
	double Speed = 0.0;
	double SpeedFactor = 1.0;

	/**
	 * Dimensions du monde en tuiles. Par defaut « pas de borne » : le JS passe
	 * `Infinity`, et `TNumericLimits<double>::Max()` joue le meme role ici
	 * (soustraire 2 le laisse inchange en double, donc `MaxX` reste infini).
	 */
	double Width = TNumericLimits<double>::Max();
	double Height = TNumericLimits<double>::Max();

	double MinX = 1.0;
	double MinY = 1.0;

	/**
	 * Laisser vide pour deriver `Width - 2` / `Height - 2` comme le fait le JS.
	 * Un `TOptional` plutot qu'une sentinelle : une borne « non renseignee » et
	 * une borne « renseignee a une valeur extreme » sont deux situations
	 * differentes, et les confondre donnerait un monde sans bord la ou
	 * l'appelant en voulait un tres grand.
	 */
	TOptional<double> MaxX;
	TOptional<double> MaxY;

	/**
	 * Predicat de blocage au sol, fourni par le monde proprietaire.
	 * Par defaut rien ne bloque — un port qui oublie de le cabler obtient un
	 * acteur qui traverse les murs, pas un acteur immobile.
	 */
	TFunction<bool(double /*X*/, double /*Y*/)> IsFootBlocked;
};

namespace AnastasisKinematics
{
	/**
	 * `normalizePlayerDrive` — reproduit `Simulation.setPlayerMovementInput` :
	 * TOUTE entree non nulle est normalisee. Une entree de longueur inferieure
	 * a 1e-5 rend `null` (drapeau `bIsSet` faux), ce qui signifie « pas de
	 * mouvement » et non « mouvement de longueur zero ».
	 */
	ANASTASISCORE_API FAnastasisDrive NormalizePlayerDrive(double DX, double DY);

	/**
	 * Applique un pas de deplacement direct a un acteur.
	 *
	 * L'AXE X EST APPLIQUE AVANT L'AXE Y, exactement comme `drivePlayerActor`
	 * cote source. Ce n'est pas un detail de style : contre un mur en diagonale
	 * l'ordre decide si l'acteur glisse le long de l'obstacle ou s'il s'y colle,
	 * et inverser les deux lignes produit une trajectoire differente pour la
	 * meme entree — donc un checkpoint [SCN] divergent sans qu'aucune regle
	 * n'ait bouge.
	 *
	 * @return vrai si la position a reellement change (plus de 1e-5).
	 */
	ANASTASISCORE_API bool AdvanceActorKinematics(FAnastasisKinematicBody& Body,
		const FAnastasisDrive& Drive, double Dt, const FAnastasisKinematicParams& Params);
}
