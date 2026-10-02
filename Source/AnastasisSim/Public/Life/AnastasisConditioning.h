// Conditionnement — plasticite acquise pendant la vie (Couche 3). Portage de
// `src/life/conditioning.js`.
//
// Trois meres dans [0, 1], neutres a 0,5 a la naissance, qui derivent lentement
// (CONDITIONING_RATE = 0,012 par seconde sim vers leur asymptote) selon ce que
// fait l'habitant. Deux d'entre elles modulent les besoins :
//   fatigueAdaptation    -> la depense d'energie (`FatigueAdaptationEnergyFallMultiplier`)
//   recoveryConditioning -> le gain d'energie au repos (`RecoveryConditioningEnergyGainMultiplier`)
// `workConditioning` derive aussi mais n'a pas encore de consommateur dans les besoins.
//
// Ne lit ni n'ecrit le genome ni le phenotype : c'est l'invariant de la reference.
//
// L'appel vit en FIN de `tickNeeds` : voir `AnastasisNeeds::TickNeedsConditioning`,
// qui calcule les quatre drapeaux exactement comme la reference.

#pragma once

#include "CoreMinimal.h"

namespace AnastasisConditioning
{
	inline constexpr int32 ConditioningVersion = 2;
	inline constexpr double ConditioningRate = 0.012;
	inline constexpr double ConditioningEffectAmplitude = 0.5;

	/** `npc.conditioning`, tel que `ensureConditioning` le cree. */
	struct FConditioning
	{
		int32 Version = ConditioningVersion;
		double WorkConditioning = 0.5;
		double FatigueAdaptation = 0.5;
		double RecoveryConditioning = 0.5;
	};

	/**
	 * `tickConditioning(npc, dt, { working, resting, overworked, fatigued })`.
	 * `bOverworked` plafonne a zero le gain de TRAVAIL (work et fatigue), jamais
	 * celui du repos.
	 */
	ANASTASISSIM_API void TickConditioning(
		FConditioning& Conditioning,
		double Dt,
		bool bWorking,
		bool bResting,
		bool bOverworked,
		bool bFatigued);

	/**
	 * `fatigueAdaptationEnergyFallMultiplier(value)` — 0,5 -> 1 ; 1 -> 0,75 ; 0 -> 1,25.
	 * La valeur absente du JS (`?? 0.5`) est le defaut de l'argument.
	 */
	ANASTASISSIM_API double FatigueAdaptationEnergyFallMultiplier(double Value = 0.5);

	/** `recoveryConditioningEnergyGainMultiplier(value)` — 0,5 -> 1 ; 1 -> 1,25 ; 0 -> 0,75. */
	ANASTASISSIM_API double RecoveryConditioningEnergyGainMultiplier(double Value = 0.5);
}
