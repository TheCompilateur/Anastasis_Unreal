#pragma once

#include "CoreMinimal.h"
#include "WorldView/AnastasisWorldView.h"

/**
 * RUIN SITE GRAMMAR.
 *
 * Turns scattered Ruin tiles into SITES. One instance per Ruin tile at a random yaw is
 * what the world did before, and at 448 instances it read as a field of headstones
 * rather than as anything built: a building has ONE axis, and a random yaw per piece
 * destroys exactly that.
 *
 * So this pass groups connected Ruin tiles, gives each group a SINGLE orientation, and
 * assigns a ROLE to each tile in it — a base course at the heart, walls along the axis,
 * a hearth inside, reusable stone at the edges.
 *
 * Reads the same canon as tools/unreal/create_ruin_grammar.py:
 * docs/visual/reference/pontique-grammaire-architecturale-batiment.png. Post-1204
 * rhomaioi rebuild; what survives of a modest house is its stone SOUBASSEMENT, and the
 * simulation agrees — a Ruin tile carries Resource=Stone, Amount=8, which is reuse
 * material.
 *
 * PRESENCE STAYS SIMULATION TRUTH. At most one instance per Ruin tile, and never on a
 * tile the simulation did not mark. This pass only decides WHICH piece a tile gets and
 * WHICH WAY it faces; it never invents or removes a ruin.
 *
 * Read-only, no UObject creation, no simulation RNG, no mesh paths.
 */
namespace AnastasisRuinDressing
{
/** The roles a tile of a site can take. Mirrors the archetypes of RUIN_GRAMMAR_V1. */
enum class EPiece : uint8
{
	Soubassement = 0,  // the base course: the footprint of the vanished core
	Angle,             // two walls meeting; corners outlive spans
	Mur,               // a lone wall segment, broken at both ends
	Foyer,             // the hearth, the most durable interior element
	Enclos,            // a low dry-stone field wall
	Reemploi,          // sorted stone waiting to be taken up again
	Count
};

const TCHAR* PieceName(EPiece Piece);

struct FPlacement
{
	/** Index into the canonical snapshot. The tile this piece stands on. */
	int32 SourceIndex = INDEX_NONE;
	EPiece Piece = EPiece::Reemploi;
	/** Shared by every piece of the same site. This is what makes a plan read as a plan. */
	double YawDegrees = 0.0;
	double ScaleMultiplier = 1.0;
	uint32 VisualSeed = 0;
	/** Which site this belongs to, for diagnostics. */
	int32 SiteId = INDEX_NONE;
};

struct FPlan
{
	TArray<FPlacement> Instances;
	int32 SiteCount = 0;
	/** Sites of a single tile: a heap of stone, never a building. */
	int32 IsolatedSites = 0;
	int32 LargestSiteTiles = 0;
};

/**
 * Full canonical snapshot in, deterministic plan out. Same (Seed, tiles) always yields
 * the same sites, the same roles and the same orientations.
 *
 * Invalid data returns a reason in OutError and no partial result.
 */
bool Build(const AnastasisWorldView::FWorldVisualSnapshot& Source, FPlan& Out, FString& OutError);
}
