#pragma once

#include "WorldView/AnastasisWorldView.h"

/**
 * HYDRA_FORGE — grammaire hydrologique de présentation.
 *
 * Ne calcule pas un second écoulement. Ne touche pas AnastasisSim.
 * Ne repeint pas TileColor (sol) et ne refait pas la nappe plate (section 1).
 *
 * Lit uniquement les champs déjà portés par le snapshot :
 *   Type / Alt / FlowAmt / FlowX / FlowZ / Shore / Wetness / Shade
 *
 * et en projette trois archétypes visuels, là où la géographie les autorise :
 *   torrent de montagne, rivière de vallée, entrée dans un lac.
 */
namespace AnastasisHydrologyDressing
{
enum class EArchetype : uint8
{
	None = 0,
	Still = 1,
	Torrent = 2,
	Valley = 3,
	Inflow = 4,
};

struct FMesh
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FLinearColor> Colors;
	/** 1 = ce sommet doit être recalé sur le sol réellement rendu (berge, roche de rive). */
	TArray<uint8> SnapToGround;
};

struct FSite
{
	EArchetype Archetype = EArchetype::None;
	FVector Centroid = FVector::ZeroVector;
	int32 TileCount = 0;
	double MeanFlowAmt = 0.0;
	double MeanBankRelief = 0.0;
	double MeanWidth = 0.0;
	double BestScore = -1.0;
};

struct FPlan
{
	FMesh Flow;
	FMesh Bank;
	FMesh Props;
	FSite Torrent;
	FSite Valley;
	FSite Inflow;
	int32 FlowingTiles = 0;
	int32 StillTiles = 0;
	int32 TorrentTiles = 0;
	int32 ValleyTiles = 0;
	int32 InflowTiles = 0;
	int32 RockCount = 0;
	int32 ReedCount = 0;
	int32 FoamCount = 0;
};

/**
 * Canonical 96x96 requis. Crop optionnel : la classification reste mondiale,
 * la géométrie n'est émise que sur l'emprise rendue. False + OutError, pas de
 * plan partiel, si l'entrée est invalide. Ne mute pas le snapshot.
 */
bool Build(
	const AnastasisWorldView::FWorldVisualSnapshot& Canonical,
	const AnastasisWorldView::FWorldVisualSnapshot* Crop,
	FPlan& Out,
	FString& OutError);
}
