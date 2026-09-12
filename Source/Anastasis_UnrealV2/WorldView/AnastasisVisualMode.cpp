#include "WorldView/AnastasisVisualMode.h"

#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarVisualMode(
	TEXT("anastasis.Visual.Mode"),
	1,
	TEXT("Presentation mode: 0=None, 1=Debug (HISMC metrology), 2=Player (unimplemented)."),
	ECVF_Default);

namespace AnastasisVisualMode
{
	EAnastasisVisualMode Get()
	{
		switch (CVarVisualMode.GetValueOnGameThread())
		{
		case 0:
			return EAnastasisVisualMode::None;
		case 2:
			return EAnastasisVisualMode::Player;
		default:
			return EAnastasisVisualMode::Debug;
		}
	}

	const TCHAR* Name(EAnastasisVisualMode Mode)
	{
		switch (Mode)
		{
		case EAnastasisVisualMode::None:
			return TEXT("NONE");
		case EAnastasisVisualMode::Debug:
			return TEXT("DEBUG");
		case EAnastasisVisualMode::Player:
			return TEXT("PLAYER");
		}
		return TEXT("DEBUG");
	}
}
