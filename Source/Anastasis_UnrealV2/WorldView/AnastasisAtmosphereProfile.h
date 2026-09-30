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

	/** Cool, slightly desaturated white — water fog, not a warm dust haze. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|Mist")
	FLinearColor MistAlbedo = FLinearColor(0.86f, 0.90f, 0.94f);

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

	/** Sun elevation (deg) at and above which the day exposure (ExposureEV100) holds. */
	UPROPERTY(EditAnywhere, Category = "Atmosphere|SkyClock")
	float DayElevationDegrees = 10.0f;

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
