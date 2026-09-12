#pragma once

#include "CoreMinimal.h"

/**
 * Explicit presentation mode. DEBUG is diagnostic HISMC cubes.
 * PLAYER is reserved and unimplemented. NONE spawns no world embodiment.
 */
enum class EAnastasisVisualMode : uint8
{
	None = 0,
	Debug = 1,
	Player = 2,
};

namespace AnastasisVisualMode
{
	EAnastasisVisualMode Get();
	const TCHAR* Name(EAnastasisVisualMode Mode);
}
