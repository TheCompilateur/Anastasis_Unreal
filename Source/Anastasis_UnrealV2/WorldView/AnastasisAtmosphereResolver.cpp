#include "WorldView/AnastasisAtmosphereResolver.h"

#include "Anastasis_UnrealV2.h"
#include "UObject/StrongObjectPtr.h"
#include "WorldView/AnastasisAtmosphereProfile.h"

namespace AnastasisAtmosphere
{
namespace
{
	// Kept alive explicitly: the profile is either an asset we did not create or a transient
	// fallback object, and nothing else in the actor graph roots it.
	TStrongObjectPtr<UAnastasisAtmosphereProfile> GCachedProfile;
	bool GProfileIsDataDriven = false;
}

void InvalidateProfileCache()
{
	GCachedProfile.Reset();
	GProfileIsDataDriven = false;
}

const UAnastasisAtmosphereProfile& GetProfile()
{
	if (GCachedProfile.IsValid())
	{
		return *GCachedProfile.Get();
	}

	UAnastasisAtmosphereProfile* Loaded = LoadObject<UAnastasisAtmosphereProfile>(nullptr, ProfileAssetPath);
	if (Loaded)
	{
		GCachedProfile.Reset(Loaded);
		GProfileIsDataDriven = true;
		UE_LOG(LogAnastasis_UnrealV2, Display,
			TEXT("ANASTASIS_ATMOSPHERE_PROFILE source=asset path=%s"), ProfileAssetPath);
		return *GCachedProfile.Get();
	}

	// FAIL-CLOSED, LOUDLY: no asset degrades to the observe-slice.py rig values, which are
	// the only lighting this project has validated captures against -- never to a dark world,
	// never to a null dereference.
	UE_LOG(LogAnastasis_UnrealV2, Warning,
		TEXT("ANASTASIS_ATMOSPHERE_PROFILE source=code_defaults reason=asset_not_found path=%s"),
		ProfileAssetPath);
	GCachedProfile.Reset(UAnastasisAtmosphereProfile::CreateCodeDefaults(GetTransientPackage()));
	GProfileIsDataDriven = false;
	return *GCachedProfile.Get();
}

bool IsProfileDataDriven()
{
	GetProfile();
	return GProfileIsDataDriven;
}

FRotator SunRotationForTimeOfDay(double Hours, double LatitudeDegrees, double DeclinationDegrees)
{
	// Hour angle: 15 degrees per hour, zero at solar noon, negative in the morning.
	const double HourAngle = FMath::DegreesToRadians((Hours - 12.0) * 15.0);
	const double Latitude = FMath::DegreesToRadians(LatitudeDegrees);
	const double Declination = FMath::DegreesToRadians(DeclinationDegrees);

	// Unit vector TOWARDS the sun in the local horizontal frame, UE axes: X north, Y east,
	// Z up. Written in vector form rather than as an azimuth formula because the azimuth
	// conventions (from north / from south, east- / west-positive) are exactly where this
	// kind of code silently ends up mirrored.
	const double Up = FMath::Sin(Latitude) * FMath::Sin(Declination)
		+ FMath::Cos(Latitude) * FMath::Cos(Declination) * FMath::Cos(HourAngle);
	const double North = FMath::Sin(Declination) * FMath::Cos(Latitude)
		- FMath::Cos(Declination) * FMath::Sin(Latitude) * FMath::Cos(HourAngle);
	const double East = -FMath::Cos(Declination) * FMath::Sin(HourAngle);

	// The light points along the rays, i.e. away from the sun: a sun high in the south-east
	// becomes a light aimed down toward the north-west.
	const FVector Forward = -FVector(North, East, Up);
	FRotator Rotation = Forward.GetSafeNormal().Rotation();
	Rotation.Roll = 0.0;
	return Rotation;
}

FRotator ResolveSunRotation(const UAnastasisAtmosphereProfile& Profile)
{
	if (Profile.bDeriveSunFromTimeOfDay)
	{
		return SunRotationForTimeOfDay(
			Profile.TimeOfDayHours, Profile.LatitudeDegrees, Profile.SunDeclinationDegrees);
	}
	return FRotator(Profile.SunPitchDegrees, Profile.SunYawDegrees, 0.0f);
}

}
