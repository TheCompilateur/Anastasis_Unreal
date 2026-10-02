#include "WorldView/AnastasisRain.h"

namespace AnastasisRain
{
	FRainVisual VisualFor(const AnastasisWeather::FWeather& Weather, const bool bWeatherDrivesSky, const double PinnedAmount)
	{
		FRainVisual Out;
		double Amount = 0.0;
		if (PinnedAmount >= 0.0)
		{
			Amount = PinnedAmount;
		}
		else if (bWeatherDrivesSky)
		{
			Amount = Weather.Rain;
		}
		Amount = FMath::Clamp(Amount, 0.0, 1.0);
		Out.Amount = Amount >= MinVisibleAmount ? Amount : 0.0;

		// The simulation's ground plane is (x, z) -> Unreal (X, Y), as SimToUnreal maps tiles.
		const double Wind = FMath::Clamp(Weather.Wind, 0.0, 1.0) * MaxWindUUPerSecond;
		Out.WindUUPerSecond = FVector2D(Weather.DirX * Wind, Weather.DirZ * Wind);
		Out.FallUUPerSecond = AnastasisRain::FallUUPerSecond;
		return Out;
	}
}
