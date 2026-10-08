#pragma once

// Port de `sim.traffic` (src/sim/simulation.js : `recordPassage`, `updateRoadEvolutionDaily`,
// `evolveDesirePathTileDaily`, `setPlannedRoad`, `roadClassForTraffic`, `roadClearEffortDays`,
// `isRoadReclaimableTile`) et de `src/sim/trafficDecay.js` (`decayFootTraffic`).
//
// Le passage des habitants s'accumule par case ; il decroit chaque nuit ; une case foulee assez
// longtemps devient un sentier durable, dont le cout de marche plus bas attire le passage suivant.
// La route n'existe que parce que des gens ont marche la.
//
// Fidele : le compteur (stockage f32, plafond 180, un passage par 0,85 s de marche), la decroissance
// et l'effort de defrichage. Reduit (ecart n°42) : le sentier ne paie pas son bois (le marche n'est pas
// porte), ne rase pas une case porteuse d'une ressource, et les classes formelles (ruelle, axe, pavage)
// ne montent pas. Fonctions pures : FVillage les applique a son etat.

#include "CoreMinimal.h"
#include "World/AnastasisWorld.h"

namespace AnastasisTraffic
{
	/** `TRAFFIC_DECAY` (trafficDecay.js, version traffic-decay-v2-phase6-time-signature-2026-08-02) : la part foulee. */
	inline constexpr double FootMul = 0.88;
	inline constexpr double FootSub = 0.55;
	inline constexpr double FootEpsilon = 0.35;

	/** `recordPassage` : plafond du compteur, et le pas de temps de marche entre deux passages. */
	inline constexpr double PassageCap = 180.0;
	inline constexpr double PassageInterval = 0.85;

	/** `ROAD_EVOLUTION` (simulation.js) : seuls les champs du sentier de desir. */
	inline constexpr double DesireTraffic = 14.0;
	inline constexpr int32 PathDays = 18;
	inline constexpr int32 FieldClearExtraDays = 6;
	inline constexpr int32 ForestBaseExtraDays = 4;
	inline constexpr int32 WoodClearPerDay = 10;
	inline constexpr double PathEffortDecay = 0.72;

	/** `ROAD_PROFILES` : les trois classes que le passage peut demander. */
	enum class ERoadClass : uint8
	{
		Path = 0,
		Lane = 1,
		Main = 2,
	};

	struct FRoadProfile
	{
		const TCHAR* Id;
		int32 Rank;
		double PathCost;
		double Traffic;
	};

	ANASTASISSIM_API const FRoadProfile& ProfileOf(ERoadClass Class);

	/** `roadClassForTraffic`. */
	ANASTASISSIM_API ERoadClass RoadClassForTraffic(double Value);

	/** `strongerRoadClass`. */
	ANASTASISSIM_API ERoadClass StrongerRoadClass(ERoadClass Current, ERoadClass Next);

	/** Une case devenue route par le passage : sa classe et le jour ou elle s'est fixee (`roadBuiltDay`). */
	struct FRoadTile
	{
		ERoadClass Class = ERoadClass::Path;
		int32 BuiltDay = 0;
		/** Passage au moment ou le sentier s'est fixe : la trace de ce qui l'a cause. */
		double TrafficAtBirth = 0.0;
	};

	/**
	 * `isRoadReclaimableTile` : prairie, lande, champ ou foret, ni eau ni pierre ni ruine, pas un batiment.
	 * Reduit (ecart n°42) : une case porteuse d'une ressource n'est pas rasee.
	 */
	ANASTASISSIM_API bool IsRoadReclaimable(const AnastasisWorld::FTile& Tile, bool bBuilding);

	/** `roadClearEffortDays` : 18 jours, + 6 pour un champ, + 4 pour une foret, + ceil(bois / 10). */
	ANASTASISSIM_API int32 RoadClearEffortDays(const AnastasisWorld::FTile& Tile);

	/** `decayFootTraffic` sur le tableau f32 ; rend le nombre de cases touchees. */
	ANASTASISSIM_API int32 DecayFootTraffic(TArray<float>& Traffic);

	/** Un passage de plus sur une case (`min(180, previous + 1)`, stocke en f32). */
	ANASTASISSIM_API float AddPassage(float Previous);

	/**
	 * L'effort d'une nuit de `evolveDesirePathTileDaily` pour une case non routiere reclamable.
	 * `InOutEffort` < 0 = absent (`delete tile.roadBuildEffort`). Rend vrai si le sentier doit se fixer.
	 */
	ANASTASISSIM_API bool AdvanceDesireEffort(double Traffic, int32 ClearDays, double& InOutEffort);
}
