#pragma once

#include "CoreMinimal.h"

class UAnastasisAtmosphereProfile;

/**
 * ATMOSPHERE RESOLUTION.
 *
 * Reads UAnastasisAtmosphereProfile and turns it into the two things an actor needs: a
 * profile that is never null, and a sun rotation. Pure functions, no Actor/UObject state
 * beyond the cached profile -- the same split AnastasisPresentation uses between its
 * registry (data) and its resolver (decisions).
 */
namespace AnastasisAtmosphere
{
	/** Fixed path + code fallback, the convention AnastasisPresentation::RegistryAssetPath already established. */
	inline constexpr const TCHAR* ProfileAssetPath = TEXT("/Game/Anastasis/Presentation/DA_AnastasisAtmosphere.DA_AnastasisAtmosphere");

	/** The live profile: the data asset when it loads, the code defaults otherwise. Never null. */
	const UAnastasisAtmosphereProfile& GetProfile();

	/** True while GetProfile() is serving the data asset rather than the code fallback. */
	bool IsProfileDataDriven();

	/** Drops the cache so the next GetProfile() re-resolves. For tests and for re-editing the asset in-editor. */
	void InvalidateProfileCache();

	/**
	 * Solar geometry -> DirectionalLight rotation.
	 *
	 * Hours is local solar time ([0,24), 12 = solar noon), Latitude and Declination are
	 * degrees. Standard hour-angle model; no calendar, no equation of time, no refraction --
	 * a declination parameter is the whole seasonal input. Documented as an approximation
	 * rather than dressed up as an ephemeris.
	 *
	 * Convention: UE world X = north, Y = east, Z = up. The returned rotation is the light's
	 * own, i.e. it points ALONG the sun's rays (away from the sun), so a high sun gives a
	 * steeply negative pitch. Deterministic: same inputs, same rotation, always.
	 */
	FRotator SunRotationForTimeOfDay(double Hours, double LatitudeDegrees, double DeclinationDegrees);

	/** The profile's explicit angles, or SunRotationForTimeOfDay when it asks for the derived sun. */
	FRotator ResolveSunRotation(const UAnastasisAtmosphereProfile& Profile);
}
