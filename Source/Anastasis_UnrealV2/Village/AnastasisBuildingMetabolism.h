#pragma once

#include "CoreMinimal.h"

/**
 * ICEBERG_001 -- BUILDING METABOLISM.
 *
 * A house is a promise: "someone lives here". Until this file the promise was never kept
 * or broken on screen -- a house whose family had gone looked exactly like a house full of
 * people (the mesh carried only a construction scale). With the sim's population gone and
 * the buildings left standing, the realistic render would have lied more convincingly the
 * better it looked: a ghost settlement dressed as a village.
 *
 * This is the pure translation from what the simulation already knows about a building to
 * what the building shows. It owns no truth and writes no state:
 *   residents -- AnastasisVillage::FVillage::CountShelterOccupants(id): living inhabitants whose
 *                home or shelter this is (a removed/dead owner is gone from it);
 *   inside    -- FVillage::InsideOf(id).Num(): inhabitants physically in it right now;
 *   completed -- FBuilding::IsCompleted(): a building site shows nothing.
 *   daylight  -- AnastasisSkyClock::FSkyState::Daylight, the daylight the viewer actually sees.
 *
 * What it does NOT know, on purpose: since when a house is empty. The sim keeps no vacated
 * date, so weathering, moss and a roof that gives way are NOT derived here (frontier, see
 * the handoff). The only thing a vacant house can honestly show today is the absence of life.
 */
namespace AnastasisMetabolism
{
	/** anastasis.Village.Metabolism */
	enum class EMode : uint8
	{
		/** The previous look: buildings show nothing of their occupants. */
		Off = 0,
		/** The projection of the simulation. Default. */
		Truth = 1,
		/**
		 * CONTROL ONLY (WRONG_WITNESS). Every completed house lit at night whatever the sim says:
		 * the prettier, causally false village. It exists so a capture can prove that the truthful
		 * version carries information the lie does not. Never a default, never a feature.
		 */
		WrongWitness = 2,
	};

	enum class EOccupancy : uint8
	{
		/** Not a dwelling (well, granary, workshop): no hearth to show. */
		NotADwelling,
		/** Construction site: the foundations speak, not a household. */
		Site,
		/** Completed, nobody's home or shelter, nobody inside. */
		Vacant,
		/** A household exists but nobody is in at the moment. */
		Resident,
		/** Somebody is inside. */
		Inhabited,
	};

	struct FInput
	{
		bool bDwelling = false;
		bool bCompleted = false;
		int32 Residents = 0;
		int32 Inside = 0;
		/** [0,1]: 0 at night, 1 in full day. */
		double Daylight = 1.0;
	};

	struct FState
	{
		EOccupancy Occupancy = EOccupancy::NotADwelling;
		/** [0,1] strength of the hearth glow seen through the openings; 0 = dark. */
		double Hearth = 0.0;
	};

	/** Hearth share of a household that is registered but out of the house (banked embers). */
	inline constexpr double ResidentHearth = 0.25;

	/** Occupancy of a building from the sim's numbers. Pure. */
	EOccupancy OccupancyOf(const FInput& Input);

	/** The projection. Pure, deterministic, no hidden state. */
	FState Derive(const FInput& Input, EMode Mode);

	/** anastasis.Village.Metabolism, clamped to a known mode. */
	EMode ModeFromInt(int32 Value);
}
