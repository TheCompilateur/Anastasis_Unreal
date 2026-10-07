// geopolitical-world-001 -- l'adaptateur local du monde exterieur.
//
// EXTENSION — ecart n°38. La reference n'a pas d'arrivees venues d'un monde exterieur (son
// `maybeImmigrate` n'est pas porte, et ne connait pas de cause). Ici, un groupe d'arrivants cree par
// `AnastasisGeo` devient des habitants ordinaires : `spawnNpc` avec les besoins par defaut d'un
// arrivant, comme `arriveAsPlayer`. Ensuite le village les traite comme les autres.

#include "Village/AnastasisVillage.h"

namespace AnastasisVillage
{
	TArray<FString> FVillage::AdmitExternalArrivals(int32 Count, double Radius, double StartAngle)
	{
		TArray<FString> Ids;
		if (!World || Count <= 0)
		{
			return Ids;
		}
		// L'anneau est parcouru en 64 caps a partir de `StartAngle`, puis, faute de place, a des rayons
		// croissants : le premier sol libre prend le prochain arrivant. Aucune case n'est prise deux fois.
		constexpr int32 Headings = 64;
		constexpr int32 RadiusSteps = 6;
		TSet<int32> Taken;
		for (int32 RStep = 0; RStep < RadiusSteps && Ids.Num() < Count; ++RStep)
		{
			const double R = Radius + static_cast<double>(RStep);
			for (int32 H = 0; H < Headings && Ids.Num() < Count; ++H)
			{
				const double A = StartAngle + (2.0 * UE_DOUBLE_PI) * static_cast<double>(H) / static_cast<double>(Headings);
				const int32 X = FMath::FloorToInt32(Settlement.X + R * FMath::Cos(A));
				const int32 Y = FMath::FloorToInt32(Settlement.Y + R * FMath::Sin(A));
				if (X < 1 || Y < 1 || X > Nav.W - 2 || Y > Nav.H - 2)
				{
					continue;
				}
				const int32 Key = Y * Nav.W + X;
				if (Taken.Contains(Key) || IsFootBlocked(X + 0.5, Y + 0.5))
				{
					continue;
				}
				Taken.Add(Key);
				Ids.Add(SpawnNpc(X + 0.5, Y + 0.5, AnastasisNeeds::FNeeds()));
			}
		}
		return Ids;
	}
}
