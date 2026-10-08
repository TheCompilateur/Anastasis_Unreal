#include "WorldView/AnastasisCosmicNight.h"

namespace AnastasisCosmicNight
{
namespace
{
	uint32 Mix(uint32 Value)
	{
		Value ^= Value >> 16;
		Value *= 0x7feb352du;
		Value ^= Value >> 15;
		Value *= 0x846ca68bu;
		return Value ^ (Value >> 16);
	}

	double Unit(uint32 Value)
	{
		return static_cast<double>(Mix(Value)) / 4294967296.0;
	}

	double Smooth(const double Value)
	{
		const double T = FMath::Clamp(Value, 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	FVector SkyDirection(const double Azimuth, const double Elevation)
	{
		const double E = FMath::DegreesToRadians(Elevation);
		return FVector(FMath::Cos(Azimuth) * FMath::Cos(E), FMath::Sin(Azimuth) * FMath::Cos(E), FMath::Sin(E));
	}
}

int32 EveningDayFor(const int32 Day, const double Hours)
{
	return FMath::Max(1, Day - (Hours < 12.0 ? 1 : 0));
}

FNight Plan(const uint32 Seed, const int32 EveningDay, const int32 ForcedKind)
{
	FNight Out;
	Out.EveningDay = FMath::Max(1, EveningDay);
	const uint32 Roll = Mix(Seed ^ (static_cast<uint32>(Out.EveningDay) * 0x9e3779b9u) ^ 0xc35a3b27u);
	if (ForcedKind >= 0)
	{
		Out.Kind = static_cast<EKind>(FMath::Clamp(ForcedKind, 0, 3));
		if (Out.Kind == EKind::Comet)
		{
			Out.CometStartDay = Out.EveningDay; // Preview a comet on any pinned evening.
			Out.CometDayIndex = 0;
		}
	}
	else
	{
		// A single onset is discoverable for four evenings. Prefer the older onset
		// if the rare calendar ever places two within the same four-day window.
		for (int32 Age = 3; Age >= 0; --Age)
		{
			const int32 StartDay = Out.EveningDay - Age;
			if (StartDay < 1)
			{
				continue;
			}
			const uint32 CometRoll = Mix(Seed ^ (static_cast<uint32>(StartDay) * 0x9e3779b9u) ^ 0xa071f00du);
			if (CometRoll % 1000u < 12u)
			{
				Out.Kind = EKind::Comet;
				Out.CometStartDay = StartDay;
				Out.CometDayIndex = Age;
				break;
			}
		}
		if (Out.Kind != EKind::Comet && Roll % 1000u < 22u)
		{
			Out.Kind = EKind::Veil;
		}
		else if (Out.Kind != EKind::Comet && Roll % 1000u < 155u)
		{
			Out.Kind = EKind::Meteors;
		}
	}
	if (Out.Kind == EKind::Meteors)
	{
		Out.MeteorCount = 3 + static_cast<int32>(Mix(Roll ^ 0x7b1f459du) % 3u);
	}
	else if (Out.Kind == EKind::Ordinary && Mix(Roll ^ 0xa219740du) % 7u == 0u)
	{
		Out.MeteorCount = 1;
	}
	return Out;
}

FInstant Evaluate(const uint32 Seed, const int32 Day, const double Hours, const double SunElevationDegrees,
	const double Cover, const double Rain, const int32 ForcedKind)
{
	FInstant Out;
	Out.Night = Plan(Seed, EveningDayFor(Day, Hours), ForcedKind);
	const double NightHour = Hours < 12.0 ? Hours + 24.0 : Hours;
	const double Dark = 1.0 - Smooth((SunElevationDegrees + 12.0) / 8.0);
	const double Weather = FMath::Square(1.0 - FMath::Clamp(Cover, 0.0, 1.0))
		* (1.0 - 0.8 * FMath::Clamp(Rain, 0.0, 1.0));
	Out.Visibility = static_cast<float>(Dark * Weather);
	if (Out.Visibility <= 0.001f || NightHour < 21.0 || NightHour > 29.0)
	{
		return Out;
	}

	const uint32 NightKey = Mix(Seed ^ (static_cast<uint32>(Out.Night.EveningDay) * 0x5bd1e995u));
	if (Out.Night.Kind == EKind::Veil)
	{
		Out.VeilStrength = static_cast<float>(Out.Visibility
			* Smooth((NightHour - 22.0) / 1.0) * (1.0 - Smooth((NightHour - 27.0) / 1.0)));
	}

	if (Out.Night.Kind == EKind::Comet)
	{
		static constexpr double NightBrightness[4] = {0.55, 0.82, 1.0, 0.68};
		const int32 Index = FMath::Clamp(Out.Night.CometDayIndex, 0, 3);
		const uint32 Key = Mix(Seed ^ (static_cast<uint32>(Out.Night.CometStartDay) * 0x41c64e6du));
		const double Azimuth = 2.0 * UE_DOUBLE_PI * Unit(Key ^ 0xe393u)
			+ FMath::DegreesToRadians(5.0 * Index);
		const double Elevation = 38.0 + 18.0 * Unit(Key ^ 0x651du) + 1.5 * Index;
		Out.CometHead = SkyDirection(Azimuth, Elevation);
		Out.CometTail = SkyDirection(Azimuth - FMath::DegreesToRadians(12.0), Elevation + 4.0);
		Out.CometStrength = static_cast<float>(Out.Visibility * NightBrightness[Index]
			* Smooth((NightHour - 21.0) / 1.0) * (1.0 - Smooth((NightHour - 28.0) / 1.0)));
	}

	for (int32 I = 0; I < Out.Night.MeteorCount; ++I)
	{
		const uint32 Key = Mix(NightKey ^ (static_cast<uint32>(I + 1) * 0x27d4eb2du));
		const double Centre = 22.2 + (I + 0.25 + 0.5 * Unit(Key ^ 0x13a5u))
			* (5.8 / FMath::Max(1, Out.Night.MeteorCount));
		const double DurationHours = 0.52; // ~2 real seconds at normal 90-second days: time to notice and turn.
		const double Phase = (NightHour - (Centre - DurationHours * 0.5)) / DurationHours;
		if (Phase < 0.0 || Phase > 1.0)
		{
			continue;
		}
		const double Azimuth = 2.0 * UE_DOUBLE_PI * Unit(Key ^ 0xe393u);
		const double Elevation = 24.0 + 42.0 * Unit(Key ^ 0x651du);
		const double Sweep = FMath::DegreesToRadians(12.0 + 12.0 * Unit(Key ^ 0x88a1u));
		Out.MeteorStart = SkyDirection(Azimuth, Elevation);
		Out.MeteorEnd = SkyDirection(Azimuth + Sweep, Elevation - 6.0 - 11.0 * Unit(Key ^ 0x923du));
		Out.MeteorPhase = static_cast<float>(Phase);
		Out.MeteorStrength = static_cast<float>(Out.Visibility
			* Smooth(Phase / 0.14) * (1.0 - Smooth((Phase - 0.78) / 0.22)));
		break;
	}
	return Out;
}
}
