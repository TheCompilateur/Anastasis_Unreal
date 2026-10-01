#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AnastasisAtmosphereProfile.generated.h"

/**
 * ATMOSPHERE DATA OWNER.
 *
 * Until this mission the only light ANASTASIS ever had was spawned by hand inside
 * tools/unreal/observe-slice.py, into one editor-built level. The playable level
 * (/Game/FirstPerson/Lvl_FirstPerson, the PIE target) carried the template's own lighting,
 * and no C++ owned sun, sky, fog or exposure anywhere -- see the Atmosphere identity pillar
 * in docs/visual/P1_6_PONTIC_BYZANTINE_ART_DIRECTION.md, which had no implementation.
 *
 * This asset is that missing owner, in the same shape the presentation layer already
 * established (UAnastasisPresentationRegistry): a UDataAsset at ProfileAssetPath, read by
 * one resolver, with CreateCodeDefaults() as the fail-closed fallback.
 *
 * CODE DEFAULTS ARE NOT ARBITRARY: they reproduce the observe-slice.py rig value for value
 * (sun pitch -38 / yaw -55, 75000 lux, atmosphere sun light on, real-time-capture sky light,
 * fixed EV100 14). That rig is the only lighting this project has ever validated captures
 * against, so adopting it verbatim means the code path starts at the known-good image and
 * every existing A/B capture stays comparable. Anastasis.Atmosphere.RigParity locks it.
 *
 * The one element the rig never had is fog: bFogEnabled below is the first implementation
 * of the "humid Pontic mist depth" the art direction asks for.
 *
 * Nothing here can reach AnastasisSim. An atmosphere profile decides how the world is lit,
 * never what the world is.
 */
UCLASS(BlueprintType)
class UAnastasisAtmosphereProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** False leaves the level's own lighting entirely alone -- an explicit data-side off switch, not a missing-asset accident. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere")
	bool bEnabled = true;

	// --- Sun -------------------------------------------------------------------------

	/**
	 * Explicit sun orientation, in the same convention as the DirectionalLight actor's own
	 * rotator. Used unless bDeriveSunFromTimeOfDay is set.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sun")
	float SunPitchDegrees = -38.0f;

	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sun")
	float SunYawDegrees = -55.0f;

	/**
	 * Derive the sun's orientation from TimeOfDayHours/LatitudeDegrees/SunDeclinationDegrees
	 * instead of the two angles above.
	 *
	 * DEFAULT FALSE, DELIBERATELY: nothing in ANASTASIS drives a time of day yet
	 * (AnastasisSimClock exists but no world clock feeds presentation), and turning this on
	 * by default would silently move the sun away from the rig angle every existing capture
	 * was taken at. The derivation itself is implemented and tested
	 * (Anastasis.Atmosphere.SunFromTime) so that whoever lands a world clock flips one flag
	 * rather than writing solar geometry under deadline.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sun")
	bool bDeriveSunFromTimeOfDay = false;

	/** Local solar time, hours in [0, 24). 12 = solar noon. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sun", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float TimeOfDayHours = 12.0f;

	/** 41 N: the Pontic coast (Trebizond), the latitude the art direction's light is describing. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sun", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float LatitudeDegrees = 41.0f;

	/** Solar declination. 0 = equinox; +-23.44 are the solstices. No calendar exists yet, so 0. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sun", meta = (ClampMin = "-23.44", ClampMax = "23.44"))
	float SunDeclinationDegrees = 0.0f;

	/** Lux. The project builds with ExtendDefaultLuminanceRange, so this is a real photometric value. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sun")
	float SunIntensityLux = 75000.0f;

	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sun")
	FLinearColor SunColor = FLinearColor::White;

	/** Without this the SkyAtmosphere is unlit: black sky, and smooth water reflecting it. The rig's comment, kept as code. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sun")
	bool bSunIsAtmosphereLight = true;

	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sun")
	bool bSunCastsShadows = true;

	// --- Sky -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sky")
	bool bSkyAtmosphereEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sky")
	bool bSkyLightEnabled = true;

	/** Real-time capture: the sky light follows the sun instead of baking a stale cubemap. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sky")
	bool bSkyLightRealTimeCapture = true;

	UPROPERTY(EditAnywhere, Category = "Atmosphere|Sky")
	float SkyLightIntensity = 1.0f;

	// --- Fog -------------------------------------------------------------------------

	/**
	 * The mission's one genuinely new visual element. Distance and scale in this world are
	 * currently unreadable: vertex-coloured ground and instanced meshes all arrive at the eye
	 * at the same contrast whether they are 3 m or 90 m away. Height fog is what makes a
	 * valley read as deep and a ridge as far.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Fog")
	bool bFogEnabled = true;

	/** Restrained on purpose: atmosphere that reveals scale, not a white-out that hides weak assets. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Fog")
	float FogDensity = 0.012f;

	UPROPERTY(EditAnywhere, Category = "Atmosphere|Fog")
	float FogHeightFalloff = 0.2f;

	/** Centimetres. Keeps the near field clear so material and silhouette stay legible. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Fog")
	float FogStartDistance = 1500.0f;

	/** Below 1 the horizon never turns into flat paint. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Fog")
	float FogMaxOpacity = 0.85f;

	/** Humid temperate air, faintly cool -- not the warm haze of a Mediterranean set. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Fog")
	FLinearColor FogInscatteringColor = FLinearColor(0.42f, 0.50f, 0.56f);

	/** Z of the fog actor, centimetres. Fog density is measured from here. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Fog")
	float FogHeightZ = 0.0f;

	// --- Mist (local fog volumes driven by simulation wetness) -------------------------

	/**
	 * ATMOSPHERE_002. The global height fog above is uniform: it says "air has depth", not
	 * "this valley is wet". These pockets are the part of the atmosphere the simulation
	 * actually earns — one ALocalFogVolume per wet cell, placed from AnastasisWorld's
	 * Wetness field, which is tile distance to water (see AnastasisMistField.h).
	 *
	 * False leaves the global fog untouched and places nothing.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Mist")
	bool bMistEnabled = true;

	/** Side of one mist cell, in tiles. Mist is an area phenomenon; one volume per tile would be thousands of proxies. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Mist", meta = (ClampMin = "1", ClampMax = "48"))
	int32 MistCellTiles = 8;

	/** Mean cell wetness required. Raise it and only the river bottoms keep fog; lower it and the shore band fills in. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Mist", meta = (ClampMin = "0.0", ClampMax = "0.999"))
	float MistWetnessThreshold = 0.35f;

	/** Pocket radius as a fraction of the cell's world size. Above ~0.8 neighbouring pockets visibly merge. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Mist", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MistVolumeRadiusFraction = 0.75f;

	/** Hard ceiling. Reaching it truncates by wetness AND logs it — a silently shortened mist field reads as a data bug later. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Mist", meta = (ClampMin = "0", ClampMax = "1024"))
	int32 MistMaxVolumes = 192;

	/** Centimetres above the rendered ground. Mist sits ON the surface; 0 buries half the volume in the terrain. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Mist")
	float MistGroundOffsetUU = 60.0f;

	/** Extinction of the thickest pocket (Density01 = 1). Thinner pockets scale down from here, they are not all identical. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Mist", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MistMaxExtinction = 0.65f;

	/** Vertical falloff inside a pocket, centimetres. Low values keep the fog lying on the ground instead of ballooning. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Mist", meta = (ClampMin = "1.0"))
	float MistHeightFalloff = 220.0f;

	/** Forward scattering. Mist lit from behind by a low sun is what makes a valley read as deep. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Mist", meta = (ClampMin = "0.0", ClampMax = "0.999"))
	float MistPhaseG = 0.35f;

	/**
	 * Cool, slightly desaturated -- water fog, not a warm dust haze.
	 *
	 * ATMOSPHERE_COHERENCE_001: was (0.86, 0.90, 0.94). A pocket is lit without shadows by the
	 * sky light and the low sun, over ground that is shadowed and occluded (grass albedo ~0.25):
	 * at that albedo a bank came out ~1.5 stops above the land it sits on (day-night-weather-001
	 * contact sheet, 19h30 overview: mist p99 luma 83 against a median 30, max 124) and read as a
	 * light source. ~0.6 halves the gap (-0.6 stop) and keeps it whiter than the land. A
	 * judgement to recalibrate with tools/unreal/atmosphere-metrics.py, stated as one.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Mist")
	FLinearColor MistAlbedo = FLinearColor(0.60f, 0.64f, 0.68f);

	// --- Realism (ENV_REALISM_001) -----------------------------------------------------
	//
	// Everything above describes a lit world. What it did not describe, and what made the
	// slice read as a set rather than a place, is measured in docs/unreal/ENV_REALISM_001.md:
	// a SkyAtmosphere left at engine defaults (air tuned for a 100 km horizon, in a 1.9 km
	// world, so aerial perspective contributes almost nothing), no cloud, no moon, one uniform
	// fog layer and no volumetric scattering.
	//
	// Every field below is OWNED by the realism layer and nothing else. With the layer off
	// (bRealismEnabled=false or anastasis.Atmosphere.Realism 0) the atmosphere writes the
	// ENGINE COMPONENT DEFAULTS back into each property it owns and removes the moon and the
	// clouds it placed -- which is exactly the pre-realism state, because neither the
	// observation rig nor ATMOSPHERE_001/002 ever wrote those properties. The A/B is therefore
	// one CVar, one session, one binary. Anastasis.Atmosphere.Realism.Reversible locks it.

	/** Data-side switch for the whole realism layer. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism")
	bool bRealismEnabled = true;

	/**
	 * Top-of-atmosphere solar colour temperature. The SkyAtmosphere already reddens the sun by
	 * transmittance as it drops, so this is deliberately NOT a "warm afternoon" tint: baking a
	 * sunset colour into the light would redden it twice. 5900 K is the extraterrestrial sun,
	 * a hair warmer than the D65 white the rig used.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Sun", meta = (ClampMin = "1700.0", ClampMax = "12000.0"))
	float SunTemperatureKelvin = 5900.0f;

	/** Clouds shadow the ground. Below 1: a real cumulus shadow is soft-edged and still lit by the sky. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Sun", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SunCloudShadowStrength = 0.6f;

	/**
	 * Humid maritime air carries more aerosol than the engine's standard atmosphere. Mie
	 * scattering is what turns a far ridge pale and a near one dark; the engine default,
	 * 0.003996, is a clear continental day.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Sky", meta = (ClampMin = "0.0", ClampMax = "0.05"))
	float SkyMieScatteringScale = 0.0065f;

	/**
	 * Aerial perspective distance multiplier. The world is 1.9 km across; at the engine's 1.0
	 * a ridge 1.5 km away receives almost no in-scattering and the whole relief sits at one
	 * contrast. Kept modest: it gives this footprint the depth of a humid valley system, it
	 * does not paint a white-out.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Sky", meta = (ClampMin = "0.0", ClampMax = "20.0"))
	float SkyAerialPerspectiveDistanceScale = 3.0f;

	/** Vegetated ground under the sky (sRGB). The engine's neutral grey over-brightens the lower dome. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Sky")
	FColor SkyGroundAlbedo = FColor(92, 104, 84);

	/** Clouds occlude the sky light too, else a forest under an overcast patch stays as bright as under clear sky. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Sky")
	bool bSkyLightCloudOcclusion = true;

	// Clouds.

	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Clouds")
	bool bCloudsEnabled = true;

	/** The engine's own simple volumetric cloud material. No project asset is invented for this. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Clouds")
	FSoftObjectPath CloudMaterial = FSoftObjectPath(TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"));

	/** Kilometres. Temperate fair-weather cumulus sits at 1-2 km; the engine default of 5 km is an alto layer. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Clouds", meta = (ClampMin = "0.2", ClampMax = "20.0"))
	float CloudLayerBottomKm = 1.5f;

	/** Kilometres of layer thickness. The default 10 km is a thunderstorm-tower budget. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Clouds", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float CloudLayerHeightKm = 3.0f;

	// Moon.

	/**
	 * A second directional light on AtmosphereSunLightIndex 1. With the sun up it contributes
	 * nothing visible (0.3 lux against 75000) and casts no shadow; it exists so that the day a
	 * clock drives the sun under the horizon, the night has a source instead of a black sky.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Moon")
	bool bMoonEnabled = true;

	/** Full moon near zenith: ~0.1-0.3 lux. The upper bound, not a "readable night" fudge. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Moon", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MoonIlluminanceLux = 0.3f;

	/**
	 * Moonlight is reflected sunlight, slightly WARMER than the sun (~4100 K). The blue night of
	 * films is a grade, not a light; baking it into the source is exactly the "blue day" failure.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Moon", meta = (ClampMin = "1700.0", ClampMax = "12000.0"))
	float MoonTemperatureKelvin = 4100.0f;

	/** Explicit moon orientation, used unless the sun is derived from time of day (then: full moon, opposite hour). */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Moon")
	float MoonPitchDegrees = -24.0f;

	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Moon")
	float MoonYawDegrees = 125.0f;

	// Fog.

	/**
	 * Second height-fog layer anchored on the valley floor with a steep falloff: fog that pools
	 * in the bottoms and thins on the slopes, instead of one global layer at the same density
	 * over ridge and river alike.
	 */
	/**
	 * First A/B (2026-09-30) at 0.02: a camera on a valley floor was whited out and the haze
	 * turned beige. Half the global layer's density, pooled much lower, is what "a light
	 * accumulation in the bottoms" means.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Fog", meta = (ClampMin = "0.0", ClampMax = "0.2"))
	float ValleyFogDensity = 0.006f;

	/** Much steeper than the global layer: e-folding over ~5 m, gone a few tens of metres above its floor. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Fog", meta = (ClampMin = "0.001", ClampMax = "10.0"))
	float ValleyFogHeightFalloff = 2.0f;

	/** Centimetres above the fog actor (FogHeightZ) where the valley layer is densest. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Fog")
	float ValleyFogHeightOffsetUU = 0.0f;

	/**
	 * Volumetric fog: sun, sky light and the mist volumes scatter in a froxel grid, so forest
	 * shade and wet hollows get light with depth. Distance-limited on purpose -- a near-field
	 * effect and a real GPU cost.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Fog")
	bool bVolumetricFog = true;

	/** Centimetres. The forest the camera stands in, not the far ridge (the height fog does that). */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Fog", meta = (ClampMin = "1000.0", ClampMax = "20000.0"))
	float VolumetricFogDistanceUU = 6000.0f;

	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Fog", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float VolumetricFogExtinctionScale = 1.0f;

	/** Mild forward scattering. High values are what make "universal god rays", which the art direction forbids. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Realism|Fog", meta = (ClampMin = "-0.9", ClampMax = "0.9"))
	float VolumetricFogScatteringDistribution = 0.3f;

	// --- Sky clock and weather (DAY_NIGHT_WEATHER_001) ---------------------------------
	//
	// See AnastasisSkyClock.h. The sky follows the SIMULATION's clock and weather instead of
	// the fixed angles above; those angles remain the anastasis.Sky.Clock 0 fallback, i.e.
	// the observation rig every capture before this mission was taken under.

	/** Data-side switch. False = the fixed sun above, whatever the simulation's hour. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock")
	bool bSkyFollowsSimulation = true;

	/** Seasonal swing of the sun. 23.44 is Earth's; 0 freezes the sun at the equinox path. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "0.0", ClampMax = "23.44"))
	float MaxDeclinationDegrees = 23.44f;

	/**
	 * Night exposure. A moonlit meadow "correctly" exposed is EV100 ~ -3 -- which renders
	 * night as a dim day, the exact failure the art direction forbids. -1 keeps two stops
	 * of darkness: the land reads by moonlight, the scene still reads as night.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock")
	float NightExposureEV100 = -1.0f;

	/** Sun elevation (deg) at and below which the night exposure holds: astronomical twilight is -18, most of the light is gone by -12. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock")
	float NightElevationDegrees = -12.0f;

	/**
	 * Sun elevation (deg) at and above which night vision is fully off (saturation 1, 6500 K).
	 * Since SKY_TRANSITIONS_001 it no longer shapes the exposure: ExposureStopsBelowDay does.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock")
	float DayElevationDegrees = 10.0f;

	/**
	 * Pinned exposure by sun elevation, as stops BELOW the day exposure: X = elevation (deg),
	 * Y = stops; linear between keys, held flat beyond them, floored at NightExposureEV100.
	 *
	 * SKY_TRANSITIONS_001. The first curve (a smoothstep from -12 to +10 deg) brightened the
	 * image AT sunset -- measured on Lvl_AnastasisSlice, valley view: mean luma 141 at +2.3 deg,
	 * 183 at -1.1 deg -- then crashed to 24 by -9 deg. At Sim.Speed 10 that is three seconds:
	 * Alexandre's "flash". These keys are calibrated on a second sweep (capture-sky
	 * sunset_after; scene log-light = log2(mean luma) + EV per elevation, valley AND overview
	 * views, the brighter one governing) for a displayed brightness falling ~0.15 stop per
	 * degree of sun: ~147 luma at +4.5 deg, ~93 at sunset, ~37 by -9 deg. Empirical, scene-
	 * and tonemapper-dependent, stated as such; Anastasis.Sky.Clock.SunsetOnlyDarkens replays
	 * the measurement and locks it.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock")
	TArray<FVector2D> ExposureStopsBelowDay = {
		FVector2D(-9.0, 15.0),
		FVector2D(-7.91, 13.10),
		FVector2D(-6.78, 11.79),
		FVector2D(-5.65, 10.44),
		FVector2D(-4.52, 8.69),
		FVector2D(-3.39, 7.63),
		FVector2D(-2.26, 6.72),
		FVector2D(-1.13, 5.88),
		FVector2D(0.0, 5.01),
		FVector2D(1.13, 4.26),
		FVector2D(2.26, 3.53),
		FVector2D(3.39, 2.70),
		FVector2D(4.52, 1.76),
		FVector2D(10.0, 0.95),
		FVector2D(20.0, 0.0),
	};

	/**
	 * Eye adaptation: the most the pinned exposure may move per REAL second while the clock
	 * runs. Captures (Apply) snap to the hour's value; a running sky never jumps, whatever
	 * anastasis.Sim.Speed does to the length of a dusk.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "0.1", ClampMax = "100.0"))
	float MaxExposureChangePerSecond = 3.0f;

	/**
	 * Night vision, not night grading. Under moonlight the eye is scotopic: colour fades and
	 * the white point adapts to the light there is. Second day/night capture (2026-09-30): a
	 * physically warm 4100 K moon, shown at full saturation, read as a dim ochre DAY, and the
	 * twilight fog as a magenta wash. Saturation and white balance follow the same elevation
	 * curve as exposure; at day both are exactly the engine's neutral (1, 6500 K).
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NightColorSaturation = 0.45f;

	/**
	 * White-balance temperature at night. UE renders a light white when WhiteTemp matches its
	 * temperature: at the moon's own 4100 K the moonlight comes out NEUTRAL, the adapted eye's
	 * view -- neither the ochre of an uncorrected warm source nor a blue night, which would need
	 * a white point well below the moon's.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "1500.0", ClampMax = "15000.0"))
	float NightWhiteTemp = 4100.0f;

	/** Weather drives clouds and fog. False = the realism layer's fixed fair-weather sky. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock")
	bool bWeatherDrivesSky = true;

	/** Cloud material coverage (m_SimpleVolumetricCloud "Cloud_GlobalCoverage") under a clear sky, weather cover 0. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float CloudCoverageClear = -0.55f;

	/** ... and under an overcast one, weather cover 1. The engine instance's own value, -0.2, sits between. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float CloudCoverageOvercast = 0.45f;

	/** Global fog density multiplier = 1 + gain * humidity (weatherHumidityAt): wet air after rain is thicker air. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float FogHumidityGain = 1.5f;

	/**
	 * Mist pockets with a high sun, as a fraction of their night/dawn strength. First
	 * day/night capture: at 1 (the ATMOSPHERE_002 constant), a clear summer noon still stood
	 * the valley camera in a white-out. Radiation fog burns off; a trace lingers.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MistMiddayFactor = 0.15f;

	/** Sun elevation (deg) at which the mist has burnt off down to MistMiddayFactor. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float MistBurnOffElevationDegrees = 25.0f;

	/**
	 * ATMOSPHERE_COHERENCE_001. Sun elevation (deg) at which the fog scatters the sun fully
	 * (its VolumetricScatteringIntensity reaches 1). Under the horizon it scatters nothing;
	 * smoothstep between. Low: the golden hour keeps its backlit haze.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "0.1", ClampMax = "30.0"))
	float SunFogScatterFullElevationDegrees = 4.0f;

	/**
	 * ATMOSPHERE_COHERENCE_001. Hours around midnight over which the sky cross-fades from one
	 * simulation day's weather to the next (AnastasisSkyClock::SkyWeatherAt). The simulation
	 * rolls a new humidity and wind each day; without this the fog density jumped at 00:00.
	 * 0 = the raw daily step.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float WeatherBlendHours = 4.0f;

	/**
	 * ATMOSPHERE_COHERENCE_001. Local exposure highlight contrast once the sun is gone. In full
	 * day the project's own r.DefaultFeature.LocalExposure.HighlightContrastScale applies
	 * unchanged; through twilight it eases to this value, so a bright dusk sky stops crushing a
	 * ground lit only by that sky. Highlights only: the night stays as dark as the pinned EV.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TwilightHighlightContrastScale = 0.6f;

	/**
	 * Vertical scale of a mist pocket relative to its radius. A sphere 240 m wide reads as a
	 * cotton ball from any height; valley mist is a bank, far wider than it is thick.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float MistVerticalScale = 0.3f;

	// --- Exposure --------------------------------------------------------------------

	/**
	 * Fixed exposure, in EV100. Auto-exposure makes two captures incomparable, which is the
	 * exact reason the observation rig pinned min = max = 14; the same reasoning applies to
	 * every level the world is embodied into, so it lives here rather than in one .umap.
	 */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Exposure")
	bool bFixedExposure = true;

	UPROPERTY(EditAnywhere, Category = "Atmosphere|Exposure")
	float ExposureEV100 = 14.0f;

	/** The observe-slice.py rig, as data. The fallback whenever the asset cannot be used. */
	static UAnastasisAtmosphereProfile* CreateCodeDefaults(UObject* Outer);
};
