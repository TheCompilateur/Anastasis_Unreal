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

	double NeglectForDays(const int32 VacantDays)
	{
		struct FKey { double Days; double Value; };
		static constexpr FKey Keys[] = {{0.0, 0.0}, {6.0, 0.2}, {18.0, 0.55}, {45.0, 1.0}};
		const double D = FMath::Max(0, VacantDays);
		if (D >= Keys[3].Days)
		{
			return 1.0;
		}
		for (int32 I = 1; I < 4; ++I)
		{
			if (D <= Keys[I].Days)
			{
				const double T = (D - Keys[I - 1].Days) / (Keys[I].Days - Keys[I - 1].Days);
				return Keys[I - 1].Value + (Keys[I].Value - Keys[I - 1].Value) * T;
			}
		}
		return 1.0;
	}

	FState Derive(const FInput& Input, const EMode Mode)
	{
		FState State;
		State.Occupancy = OccupancyOf(Input);
		if (Mode == EMode::Truth && State.Occupancy == EOccupancy::Vacant)
		{
			State.Neglect = NeglectForDays(Input.VacantDays);
		}
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
