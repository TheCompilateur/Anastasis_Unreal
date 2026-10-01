#pragma once

#include "CoreMinimal.h"
#include "WorldView/AnastasisPresentationRegistry.h"

/**
 * Which portrait a simulated villager wears (VILLAGER_PNG_001).
 *
 * PRESENTATION ONLY. The simulation decides nothing here and is told nothing: the choice is
 * a pure function of the villager's identifier (`npc-N`) and of the registry. It consumes no
 * simulation RNG and writes no simulation field, so the village digest cannot depend on it.
 *
 * The simulation's villagers are all adults (deviation 8 of AnastasisVillage.h): only adult
 * and elder portraits are handed out. Children exist in the registry and on the lineup board;
 * they are drawn in the village the day the simulation has children.
 */
namespace AnastasisVillagerLooks
{
	/** Card canvas, shared with SourceArt/Characters/villager-population.json ("canvas"). */
	inline constexpr double CanvasWidthCm = 100.0;
	inline constexpr double CanvasHeightCm = 200.0;
	/** Feet sit 16 px above the bottom of a 1024 px canvas. */
	inline constexpr double FootMarginCm = 16.0 * CanvasHeightCm / 1024.0;

	/** May a portrait of this category be handed to a simulated villager today. */
	bool IsAssignableInVillage(EAnastasisVillagerCategory Category);

	/**
	 * Indices into `Looks` of the portraits a villager may wear. Each category is ordered by CRC
	 * of LookId (independent of the asset order); the categories are then interleaved in
	 * proportion to their size, so any first N villagers mirror the population's make-up
	 * (8/8/4/4: man, woman, old man, old woman, man, woman...). Entries without a portrait are
	 * left out.
	 */
	TArray<int32> VillagePool(const TArray<FAnastasisVillagerLook>& Looks);

	/**
	 * Index into `Looks` for this villager, INDEX_NONE if the pool is empty.
	 * `npc-N` takes the pool's N-th portrait modulo its size: the first Pool.Num() villagers
	 * are all different. Any other identifier falls back to its CRC.
	 */
	int32 PickLook(const TArray<int32>& Pool, const FString& NpcId);
}
