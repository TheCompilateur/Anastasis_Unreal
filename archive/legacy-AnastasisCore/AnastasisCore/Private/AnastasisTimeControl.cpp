#include "AnastasisTimeControl.h"

namespace AnastasisTime
{
	const TArray<int32>& Speeds()
	{
		static const TArray<int32> Values = {1, 2, 5, 10};
		return Values;
	}

	int32 NormalizeSpeed(int32 Value)
	{
		return Speeds().Contains(Value) ? Value : 1;
	}

	bool SpeedFromKey(TCHAR Key, int32& OutSpeed)
	{
		switch (Key)
		{
		case TEXT('1'): OutSpeed = 1; return true;
		case TEXT('2'): OutSpeed = 2; return true;
		case TEXT('5'): OutSpeed = 5; return true;
		// '0' porte la vitesse 10 : c'est la touche a droite du '9' sur une
		// rangee de chiffres, pas une faute de frappe pour '1'.
		case TEXT('0'): OutSpeed = 10; return true;
		default: return false;
		}
	}
}
