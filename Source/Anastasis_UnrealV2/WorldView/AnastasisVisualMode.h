#pragma once

#include "CoreMinimal.h"

/**
 * Explicit presentation mode. DEBUG is diagnostic HISMC cubes.
 * PLAYER embodies the same world and forces the player's arrival at the start of play
 * (player-start-001). NONE spawns no world embodiment.
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
