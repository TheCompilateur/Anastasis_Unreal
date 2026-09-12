#pragma once

#include "CoreMinimal.h"
#include "Core/AnastasisJsNumeric.h"

/**
 * Horloge sim haute vitesse — port de src/runtime/simClock.js.
 *
 * Contraintes (inchangees depuis le JS):
 *  1) Ne jamais figer le rendu (budget mur serre, <= 2 ticks/frame).
 *  2) Ne jamais sauter trop loin d'un coup (PNJ / pathfinding cassent).
 *  3) Toute vitesse > 1x = 1 fat step (temps xN, cout CPU ~ 1x).
 *     L'ancien 2x = 2 ticks fins: le budget mur coupait le 2e -> 2x ~ 1x en charge.
 *
 * Plan:
 *  1x  -> 1 x FixedDt
 *  2x  -> 1 x (2 . FixedDt)
 *  5x  -> 1 x (5 . FixedDt)
 *  10x -> 1 x (10 . FixedDt) = 10x le temps, 1 tick CPU
 *
 * NOTE DE PORTAGE — ce qui n'est PAS ici:
 *  - startGameLoop (boucle requestAnimationFrame): remplace par le Tick d'Unreal.
 *  - simSpeedRenderGate3d / applySimSpeedRenderGate3d: ce sont des reglages de
 *    rendu (ombres, particules, pixel ratio) lies a three.js. Ils appartiennent
 *    a la couche de presentation Unreal, pas au socle de simulation.
 */
namespace AnastasisSimClock
{
	/** Pas de temps fixe de la simulation, en secondes. */
	inline constexpr double FixedDt = 1.0 / 60.0;

	/** Saut max par tick (10x = 1 x 10.dt). Au-dela, les PNJ se coincent. */
	inline constexpr double MaxStepMult = 10.0;

	/** Cout tick nominal (ms). */
	inline constexpr double WallTickCostMs = 4.5;

	/** Jamais plus que ca par frame. */
	inline constexpr double WallBudgetMsCap = 10.0;

	/** Au-dela, la frame est deja lourde: on rend la main plus tot. */
	inline constexpr double WallHeavyFrameMs = 28.0;

	/** Plan de pas pour une frame donnee. */
	struct FStepPlan
	{
		/** Duree simulee par tick (peut valoir plusieurs FixedDt: fat step). */
		double StepDt = FixedDt;

		/** Nombre de ticks vises cette frame (1 ou 2). */
		int32 TargetSteps = 1;

		/** Temps de simulation total vise sur la frame. */
		double TimePerFrame = FixedDt;

		/** Facteur d'acceleration reellement obtenu (TimePerFrame / FixedDt). */
		double TimeGain = 1.0;
	};

	/**
	 * @param Scale vitesse demandee (1, 2, 5, 10). Toute valeur < 1 ou NaN vaut 1.
	 * @param InFixedDt pas fixe, expose pour les tests et le rejeu headless.
	 */
	ANASTASISSIM_API FStepPlan StepPlan(double Scale = 1.0, double InFixedDt = FixedDt);

	/** Budget mur (ms). Scale avec TargetSteps, jamais avec la vitesse brute. */
	ANASTASISSIM_API double WallBudgetMs(
		double Scale = 1.0,
		double WallFrameMs = 1000.0 / 60.0,
		bool bWorldBuilding = false);

	/**
	 * Nombre de ticks garantis avant que le budget mur puisse couper.
	 * Vaut 1: le plan nominal est deja un fat step des 2x.
	 */
	ANASTASISSIM_API int32 MinStepsBeforeBudget(double Scale = 1.0);

	/** Retard accumule qu'on accepte de conserver d'une frame a l'autre. */
	ANASTASISSIM_API double KeepLagSeconds(double Scale = 1.0, double InFixedDt = FixedDt);

	/** Resultat du calcul de delta d'une frame de rendu. */
	struct FFrameDelta
	{
		/** Delta borne, en secondes, a injecter dans l'accumulateur sim. */
		double Dt = 0.0;

		/** Delta mur brut, en millisecondes (diagnostic). */
		double WallMs = 0.0;

		/** Delta mur borne, pour les heuristiques de charge. */
		double LastWallFrameMs = 0.0;
	};

	/**
	 * Horloge frame: dt borne apres une pause (onglet en arriere-plan cote web,
	 * breakpoint ou hitch de chargement cote Unreal). Sans ce plafond, la
	 * premiere frame apres reprise injecte plusieurs secondes d'un coup et
	 * teleporte tout le village.
	 *
	 * Port de frameDeltaSeconds (src/runtime/gameLoop.js). Prend un delta deja
	 * mesure en millisecondes plutot que deux horodatages: Unreal fournit
	 * directement le DeltaTime de la frame.
	 */
	ANASTASISSIM_API FFrameDelta FrameDelta(
		double WallMs,
		double MaxDt = 0.1,
		double MinWallMs = 1.0,
		double MaxWallMs = 250.0);
}
