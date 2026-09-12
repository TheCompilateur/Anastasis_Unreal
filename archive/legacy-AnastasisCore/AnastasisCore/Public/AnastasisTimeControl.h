#pragma once

#include "CoreMinimal.h"

/**
 * Vitesses de temps offertes a l'hote.
 *
 * Traduction de `ANASTASIS_WONDERLAND_PROBE/js/anastasis-time-control.js`.
 * La liste est FERMEE : une valeur hors liste retombe a 1 au lieu d'etre
 * acceptee telle quelle. Une vitesse arbitraire changerait la taille du pas
 * fixe et donc la trajectoire du monde pour une meme graine.
 */
namespace AnastasisTime
{
	/** `ANASTASIS_TIME_SPEEDS` — dans cet ordre. */
	ANASTASISCORE_API const TArray<int32>& Speeds();

	/** `normalizeAnastasisTimeSpeed` : hors liste = 1. */
	ANASTASISCORE_API int32 NormalizeSpeed(int32 Value);

	/**
	 * `anastasisTimeSpeedFromKey` : '1' -> 1, '2' -> 2, '5' -> 5, '0' -> 10.
	 * Retourne faux pour toute autre touche — l'hote doit alors laisser
	 * l'evenement passer au lieu de l'avaler.
	 */
	ANASTASISCORE_API bool SpeedFromKey(TCHAR Key, int32& OutSpeed);
}
