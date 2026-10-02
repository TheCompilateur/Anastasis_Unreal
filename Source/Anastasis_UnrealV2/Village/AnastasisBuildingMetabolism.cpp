#include "Village/AnastasisBuildingMetabolism.h"

namespace AnastasisMetabolism
{
	EOccupancy OccupancyOf(const FInput& Input)
	{
		if (!Input.bDwelling)
		{
			return EOccupancy::NotADwelling;
		}
		if (!Input.bCompleted)
		{
			return EOccupancy::Site;
		}
		if (Input.Inside > 0)
		{
			return EOccupancy::Inhabited;
		}
		return Input.Residents > 0 ? EOccupancy::Resident : EOccupancy::Vacant;
	}

	FState Derive(const FInput& Input, const EMode Mode)
	{
		FState State;
		State.Occupancy = OccupancyOf(Input);
		if (Mode == EMode::Off)
		{
			return State;
		}
		const double Dark = 1.0 - FMath::Clamp(Input.Daylight, 0.0, 1.0);
		if (Mode == EMode::WrongWitness)
		{
			if (State.Occupancy == EOccupancy::Vacant || State.Occupancy == EOccupancy::Resident
				|| State.Occupancy == EOccupancy::Inhabited)
			{
				State.Hearth = Dark;
			}
			return State;
		}
		switch (State.Occupancy)
		{
		case EOccupancy::Inhabited:
			State.Hearth = Dark;
			break;
		case EOccupancy::Resident:
			State.Hearth = Dark * ResidentHearth;
			break;
		default:
			break;
		}
		return State;
	}

	EMode ModeFromInt(const int32 Value)
	{
		return static_cast<EMode>(FMath::Clamp(Value, 0, 2));
	}
}
