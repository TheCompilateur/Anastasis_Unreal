#pragma once

#include "CoreMinimal.h"

class FAnastasisSimulation;

/**
 * TIME_WARP_001 -- accelerer le temps, pour le joueur comme pour les agents.
 *
 * Deux voies, toutes deux dans l'hote Unreal. Le socle AnastasisSim (PumpFrame, StepPlan) est
 * un port de parite JS : il n'est pas touche, et `anastasis.Sim.Warp 1` reprend exactement
 * l'ancien chemin.
 *
 *  1. `anastasis.Sim.Warp` (temps reel accelere) : multiplie le temps simule par frame. Le pas
 *     reste celui que la reference autorise (au plus 10 x FixedDt, AnastasisSimClock::MaxStepMult :
 *     au-dela les habitants se coincent) ; c'est le NOMBRE de pas par frame qui monte, sous un
 *     budget mur (`anastasis.Sim.WarpBudgetMs`). Machine trop lente : le retard est abandonne
 *     plutot qu'accumule, et l'acceleration REELLE est affichee a cote de la demandee.
 *
 *  2. `Anastasis.Sim.Advance <duree>` (avance instantanee) : la simulation saute la duree dans la
 *     frame, en pas de 10 x FixedDt, sans budget. Pour les preuves qui attendaient la nuit.
 *
 * Le temoin (FWitness) : un joueur qui accelere ne fait rien aux yeux des habitants. Sa PRESENCE
 * (1 = vu, 0 = invisible) s'efface avec le temps passe en accelere et revient en jouant a vitesse
 * normale ; les JOURS OISIFS vus par le village s'accumulent et ne s'effacent pas. Aucun habitant
 * ne lit encore ces valeurs : PLAYER et la reputation (standing.js) ne sont pas portes. Le temoin
 * est le contrat qu'ils liront.
 */
namespace AnastasisTimeWarp
{
	/** Plafond de `anastasis.Sim.Warp`. */
	inline constexpr double MaxWarp = 1000.0;

	/** Paliers du joueur (Anastasis.Sim.Faster / Slower). 0 n'en fait pas partie : c'est Pause. */
	inline constexpr double Presets[] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0 };

	/** Palier suivant (Direction > 0) ou precedent (< 0) ; depuis 0 (pause), Faster rend 1. */
	double StepPreset(double Current, int32 Direction);

	/**
	 * Duree simulee d'une commande Advance, a partir du temps courant `Now` :
	 *   "45"     45 s simulees           "3d"  3 jours (3 x DayLength)
	 *   "6h"     6 heures du jour simule "@22" ou "@22:30" jusqu'a la prochaine 22:00 / 22:30
	 * Faux et `OutError` renseigne si l'argument ne se lit pas ou ne fait pas avancer.
	 */
	bool ParseAdvance(const FString& Arg, double Now, double DayLength, double& OutSeconds, FString& OutError);

	/** Pas d'une avance : le fat step 10x de la reference, 1/6 s simulee. */
	double AdvanceStepDt();

	/** Fait avancer `Sim` de `Seconds` exactement (dernier pas raccourci). Rend le nombre de Tick. */
	int32 Advance(FAnastasisSimulation& Sim, double Seconds, int32 MaxSteps = 2000000);

	struct FPumpResult
	{
		int32 Steps = 0;
		/** Fraction du pas en cours, pour interpoler les cartes. */
		double StepAlpha = 1.0;
		/** Vrai si le budget mur a coupe : du retard a ete abandonne. */
		bool bBudgetCut = false;
	};

	/** Accumulateur de la voie acceleree. Remis a zero quand on revient a Warp 1. */
	struct FWarpPump
	{
		double Accumulator = 0.0;

		/**
		 * @param SimWallSeconds secondes reelles deja passees par anastasis.Sim.TimeScale
		 * @param Multiplier     max(1, Sim.Speed) x Sim.Warp ; 0 = pause
		 * @param BudgetMs       temps mur max passe dans la simulation cette frame (<= 0 : illimite)
		 */
		FPumpResult Pump(FAnastasisSimulation& Sim, double SimWallSeconds, double Multiplier, double BudgetMs);

		void Reset() { Accumulator = 0.0; }
	};

	/**
	 * Le village regarde le joueur. Une seconde simulee a l'acceleration M est, pour lui, une
	 * fraction 1 - 1/M de seconde ou le joueur n'a rien fait ; Advance (M infini) est oisif en
	 * entier. La presence decroit en exp(-oisif / FadeDays) et remonte vers 1 en exp(-actif / RecoverDays).
	 */
	struct FWitness
	{
		/** Jours simules d'oisivete qui divisent la presence par e (une semaine : ~3 %). */
		static constexpr double FadeDays = 2.0;
		/** Jours joues a vitesse normale qui rendent 63 % de la presence perdue. */
		static constexpr double RecoverDays = 1.0;

		double Presence = 1.0;
		/** Secondes simulees ou le village a vu le joueur ne rien faire. Jamais effacees. */
		double IdleSeconds = 0.0;

		void Observe(double SimSeconds, double Multiplier, double DayLength);
		double IdleDays(double DayLength) const { return DayLength > 0.0 ? IdleSeconds / DayLength : 0.0; }
	};
}
