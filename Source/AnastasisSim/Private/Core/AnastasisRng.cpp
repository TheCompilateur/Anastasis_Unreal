#include "Core/AnastasisRng.h"

FAnastasisRng& GetAnastasisFallbackRng()
{
	// Initialisation a la premiere demande, avec le meme seed que le JS.
	static FAnastasisRng FallbackRng(AnastasisFallbackRngSeed);
	return FallbackRng;
}
