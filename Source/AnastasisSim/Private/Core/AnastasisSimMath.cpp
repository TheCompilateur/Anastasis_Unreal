#include "Core/AnastasisSimMath.h"

namespace AnastasisMath
{
	double HashText(const FString& Text, int32 Channel)
	{
		// `(channel * 2654435761) >>> 0`: le produit se fait en double cote JS
		// (2654435761 depasse int32), puis seulement ToUint32.
		uint32 Hash = AnastasisJs::ToUint32(static_cast<double>(Channel) * 2654435761.0);

		const int32 Length = Text.Len();
		for (int32 Index = 0; Index < Length; ++Index)
		{
			// charCodeAt: unite de code UTF-16, non signee.
			Hash ^= static_cast<uint32>(static_cast<uint16>(Text[Index]));
			Hash = AnastasisJs::Imul(Hash, 16777619u);
		}
		return static_cast<double>(Hash ^ (Hash >> 16)) / 4294967295.0;
	}

	double Hash2d(double X, double Y, int32 Channel)
	{
		// Somme en double, puis ToUint32 — voir AnastasisJsNumeric.h pour la
		// raison detaillee de ne pas court-circuiter en arithmetique entiere.
		const double Mixed = X * 374761393.0
			+ Y * 668265263.0
			+ static_cast<double>(Channel) * 2246822519.0;

		uint32 H = AnastasisJs::ToUint32(Mixed);
		H = H ^ (H >> 13);
		H = AnastasisJs::Imul(H, 1274126177u);
		return static_cast<double>(H ^ (H >> 16)) / 4294967295.0;
	}
}
