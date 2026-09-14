#pragma once

#include "WorldView/AnastasisTerrainSurface.h"

/**
 * TERRAIN_FORGE — morphologie de présentation.
 *
 * Ne possède pas la vérité de simulation (AnastasisWorld::Alt), ni la palette
 * de sol / rive (AnastasisTerrainSurface::TileColor), ni les arbres.
 *
 * Prend la surface tuilée déjà bâtie et produit le relief réellement rendu :
 * tessellation, amplification des masses, terrasses, ravines, escarpements,
 * bassin habitable. SampleActive suit EXACTEMENT ce maillage.
 */
namespace AnastasisTerrainForge
{
inline constexpr int32 DefaultSubdiv = 4;

struct FMesh
{
	AnastasisTerrainSurface::FGeometry Geometry;
	int32 CoarseW = 0;
	int32 CoarseH = 0;
	int32 Subdiv = 0;
	int32 FineW = 0;
	int32 FineH = 0;
	int32 OriginX = 0;
	int32 OriginY = 0;
	double MinZ = 0.0;
	double MaxZ = 0.0;
	double BasinX = 0.0;
	double BasinY = 0.0;
	double BasinZ = 0.0;
	double LandmarkX = 0.0;
	double LandmarkY = 0.0;
	double LandmarkZ = 0.0;
	bool bBasinFound = false;
	bool bLandmarkFound = false;
};

/** Remplace InOut par le maillage forgé. False = laisser la surface tuilée telle quelle. */
bool Apply(const AnastasisWorldView::FWorldVisualSnapshot& Crop, AnastasisTerrainSurface::FGeometry& InOut, FMesh& OutMeta);

bool SampleHeight(const FMesh& Mesh, double WorldX, double WorldY, double& OutZ);

void SetActive(const FMesh& Mesh);
void ClearActive();
/** Échantillon du sol réellement rendu si un Apply est actif, sinon false. */
bool SampleActive(double WorldX, double WorldY, double& OutZ);

inline int32 FineVerticesFor(int32 CoarseW, int32 CoarseH, int32 Subdiv)
{
	return ((CoarseW - 1) * Subdiv + 1) * ((CoarseH - 1) * Subdiv + 1);
}
}
