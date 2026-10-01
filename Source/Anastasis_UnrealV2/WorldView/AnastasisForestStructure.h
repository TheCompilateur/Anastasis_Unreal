#pragma once

#include "WorldView/AnastasisEcologicalDressing.h"

/**
 * Macro and meso structure laid over a forest plan that already exists.
 *
 * AnastasisEcologicalDressing::Build decides where a trunk is allowed.
 * This pass decides how those trunks form a forest: age, clumps, clearings,
 * crests, soft corridors. It never adds a tree and it never touches the
 * terrain, the hydrology, the weather, or the villagers.
 *
 * Call once, on the fresh plan, before the HISM loop in
 * AAnastasisWorldEmbodiment. That call site is intentionally not edited
 * while other agents still have AnastasisWorldEmbodiment.cpp and
 * AnastasisEcologicalDressing.cpp open.
 */
namespace AnastasisForestStructure
{
enum class EStand : uint8
{
	Young = 0,
	Mature = 1,
	Old = 2,
	Disturbed = 3,
	Clearing = 4,
};

struct FNote
{
	EStand Stand = EStand::Mature;
	bool bPacked = false;
	bool bClearingInterior = false;
	bool bClearingBorder = false;
	bool bCrest = false;
	bool bCorridor = false;
	double Cluster = 0.0;
};

struct FReport
{
	int32 Before = 0;
	int32 After = 0;
	int32 Moved = 0;
	int32 SmallClearings = 0;
	int32 MediumClearings = 0;
	int32 LargeClearings = 0;
	int32 ByStand[5] = {};
	int32 ByLayer[3] = {};
};

/**
 * bMacroHeights must match the plan: true only when Build ran with a rendered
 * habitat and therefore already multiplied scales by HeightMultiplier.
 * OutNotes is parallel to the surviving instances.
 */
bool Shape(
	const AnastasisWorldView::FWorldVisualSnapshot& Source,
	const FAnastasisForestDressingSettings& Settings,
	bool bMacroHeights,
	AnastasisEcologicalDressing::FPlan& InOut,
	TArray<FNote>& OutNotes,
	FReport& OutReport,
	FString& OutError);
}
