#pragma once

#include "CoreMinimal.h"

/**
 * A few specimen trees, and a far canopy envelope over the forest that already
 * exists. The mass stays on the production meshes. Heroes are only the four
 * species that are read at arm's length (Aleppo pine, cypress, holm oak,
 * olive): the caller places them on meshes authored at real stature, so the
 * instance scale stays near 1. Shells are one soft crown per stand, drawn only
 * beyond the distance where separate trees stop reading as a forest.
 *
 * Candidates are the trunks already placed. Index is the caller's index.
 * This pass never adds a trunk and never moves one.
 */
namespace AnastasisHeroCanopy
{
struct FCandidate
{
	int32 Index = INDEX_NONE;
	FVector2D Ground = FVector2D::ZeroVector;
	/** EAnastasisTreeSpecies as a byte. 1..4 are the hero species. */
	uint8 Species = 0;
	/** Metres. Zero is not a hero: the tree has no real stature to match. */
	double Score = 0.0;
};

struct FHero
{
	int32 Index = INDEX_NONE;
	uint8 Species = 0;
};

struct FShell
{
	FVector2D Center = FVector2D::ZeroVector;
	double RadiusCm = 0.0;
	int32 Trees = 0;
};

struct FReport
{
	int32 Candidates = 0;
	int32 Heroes = 0;
	int32 Shells = 0;
};

bool Build(const TArray<FCandidate>& Candidates, TArray<FHero>& Heroes, TArray<FShell>& Shells, FReport& Report, FString& Error);
}
