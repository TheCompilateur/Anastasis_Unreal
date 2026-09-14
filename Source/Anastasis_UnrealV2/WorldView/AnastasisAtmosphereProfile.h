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
