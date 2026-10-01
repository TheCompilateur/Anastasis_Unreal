#pragma once

#include "CoreMinimal.h"

/**
 * Reads the authored relief and names a few places on it.
 * Does not move terrain, tiles, water, or the simulation.
 */
namespace AnastasisSiteDressing
{
struct FRead
{
	double Valley = 0;
	double River = 0;
	double Passage = 0;
	bool bMeadow = false;
	bool bRiparian = false;
	bool bPassage = false;
};

enum class ESeat : uint8
{
	Dry,
	Bank
};

struct FProp
{
	const TCHAR* Mesh = nullptr;
	double X = 0;
	double Y = 0;
	float Yaw = 0;
	float Scale = 1;
	float StepX = 0;
	float StepY = 0;
	ESeat Seat = ESeat::Dry;
	bool bStone = false;
	bool bBlock = true;
	bool bSink = false;
	bool bTree = false;
};

FRead Read(double TileX, double TileY);
bool ShouldOmitForest(const FRead& Site, uint32 VisualSeed);
bool IsFutureSettlement(double TileX, double TileY);
void AppendCompositions(TArray<FProp>& Out);
}
