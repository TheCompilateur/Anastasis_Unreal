#pragma once

#include "CoreMinimal.h"
#include "World/AnastasisWeather.h"

/**
 * RAIN_001. The rain that falls: what the screen shows of the simulation's rain.
 *
 * The simulation already knows how hard it rains (AnastasisWeather::FWeather::Rain, the
 * VISIBLE streak share, nearly off in winter in favour of snow) and where the wind blows
 * (Wind, DirX/DirZ). Nothing drew it (handoffs/env-realism-001.md). This is the pure half:
 * weather in, streak parameters out. AAnastasisWorldAtmosphere owns the drawing -- one
 * instanced component of identical streaks, placed every frame by M_AnastasisRain
 * (tools/unreal/rain-material.py), so no particle is ever simulated on the CPU.
 *
 * Nothing here can reach AnastasisSim: the rain shown never changes the rain simulated.
 */
namespace AnastasisRain
{
	/** Streak instances. All exist; RainAmount lights a share of them (PerInstanceRandom < amount). */
	inline constexpr int32 StreakCount = 8000;

	/**
	 * The box of rain wrapped around the camera, in uu: 24 x 24 m, 14 m tall. First capture
	 * (rain-ab, 2026-10-02) at 50 x 50 x 24 m with 6000 streaks 1.2 cm wide: rain at 1 was a
	 * few threads on the sky, invisible on the ground -- most streaks were too far to cover a
	 * pixel. Beyond the box, the fog and the sky's humidity carry the rain's veil.
	 */
	inline const FVector BoxUU(2400.0, 2400.0, 1400.0);

	/** Streak size and peak opacity, written on the dynamic material (M_AnastasisRain defaults are overridden). */
	inline constexpr double StreakLengthUU = 90.0;
	inline constexpr double StreakWidthUU = 2.5;
	inline constexpr double StreakOpacity = 0.6;

	/** Terminal speed of a raindrop, ~9 m/s. */
	inline constexpr double FallUUPerSecond = 900.0;

	/** Horizontal drift at the simulation's strongest wind (Wind = 1): a slant of about 29 degrees. */
	inline constexpr double MaxWindUUPerSecond = 500.0;

	/** Below this share no streak is drawn at all: the component is hidden, it costs nothing. */
	inline constexpr double MinVisibleAmount = 0.02;

	inline const TCHAR* MaterialPath = TEXT("/Game/Anastasis/Weather/M_AnastasisRain.M_AnastasisRain");
	inline const TCHAR* MeshPath = TEXT("/Game/Anastasis/Weather/SM_AnastasisRainStreak.SM_AnastasisRainStreak");

	struct FRainVisual
	{
		/** [0,1] share of streaks lit; 0 when nothing is drawn. */
		double Amount = 0.0;
		/** Horizontal drift in uu/s, Unreal X/Y (the simulation's x/z ground axes). */
		FVector2D WindUUPerSecond = FVector2D::ZeroVector;
		double FallUUPerSecond = AnastasisRain::FallUUPerSecond;

		bool IsVisible() const { return Amount >= MinVisibleAmount; }
	};

	/**
	 * Weather in, streaks out. PinnedAmount >= 0 replaces the weather's rain (captures, tests);
	 * < 0 follows it. bWeatherDrivesSky false (profile or anastasis.Sky.Weather 0) shows the
	 * fixed fair-weather sky, so no rain unless pinned.
	 */
	FRainVisual VisualFor(const AnastasisWeather::FWeather& Weather, bool bWeatherDrivesSky, double PinnedAmount);
}
