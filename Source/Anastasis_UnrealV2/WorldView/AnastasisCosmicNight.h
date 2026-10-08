#pragma once

#include "CoreMinimal.h"

/** Presentation-only calendar. Nothing here changes simulation time, weather or decisions. */
namespace AnastasisCosmicNight
{
	enum class EKind : uint8 { Ordinary, Meteors, Veil, Comet };

	struct FNight
	{
		int32 EveningDay = 1;
		EKind Kind = EKind::Ordinary;
		int32 MeteorCount = 0;
		int32 CometStartDay = 0;
		int32 CometDayIndex = -1;
	};

	struct FInstant
	{
		FNight Night;
		float Visibility = 0.0f;
		float MeteorStrength = 0.0f;
		float MeteorPhase = 0.0f;
		FVector MeteorStart = FVector::UpVector;
		FVector MeteorEnd = FVector::UpVector;
		float VeilStrength = 0.0f;
		float CometStrength = 0.0f;
		FVector CometHead = FVector::UpVector;
		FVector CometTail = FVector::UpVector;
	};

	/** The date of the evening that owns this night; 00:00-11:59 belongs to yesterday. */
	int32 EveningDayFor(int32 Day, double Hours);

	/** ForcedKind: -1 natural calendar, 0 ordinary, 1 meteor night, 2 strange veil, 3 comet preview. */
	FNight Plan(uint32 Seed, int32 EveningDay, int32 ForcedKind = -1);

	/** Sample at the sky clock's day/hour. Sun and weather can conceal a scheduled event. */
	FInstant Evaluate(uint32 Seed, int32 Day, double Hours, double SunElevationDegrees,
		double Cover, double Rain, int32 ForcedKind = -1);
}
