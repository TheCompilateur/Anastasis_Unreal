#pragma once

#include "CoreMinimal.h"

/**
 * Contact at the foot of a trunk. Meadow grass stops under the crown, so the
 * ground there is only the litter tint of the soil shader. These patches are
 * the skirt a standing camera actually sees: litter and a low moss pad,
 * within about a metre of the trunk, drawn only for the nearest metres.
 *
 * Trunks are the crowns already placed: X, Y, crown radius in centimetres.
 * The plan never adds a tree and never moves one.
 */
namespace AnastasisTrunkContact
{
struct FPatch
{
	FVector2D Position = FVector2D::ZeroVector;
	double YawDegrees = 0.0;
	double ScaleXY = 1.0;
	double ScaleZ = 1.0;
	/** 0 = flattened litter (short meadow grass), 1 = low moss pad (sedge). */
	int32 Kind = 0;
	int32 Trunk = INDEX_NONE;
};

struct FReport
{
	int32 Trunks = 0;
	int32 Patches = 0;
	int32 Litter = 0;
	int32 Moss = 0;
};

bool Build(const TArray<FVector>& Trunks, uint32 Seed, TArray<FPatch>& Out, FReport& OutReport, FString& OutError);
}
