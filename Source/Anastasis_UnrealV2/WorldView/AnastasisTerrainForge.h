#pragma once

#include "WorldView/AnastasisTerrainSurface.h"

/**
 * TERRAIN_FORGE — morphologie de présentation.
 *
 * Ne possède pas la vérité de simulation (AnastasisWorld::Alt), ni la palette
 * de sol / rive (AnastasisTerrainSurface::TileColor), ni les arbres.
 *
 * Prend la surface tuilée déjà bâtie et produit le relief réellement rendu :
 * tessellation bicubique, ravines, bassin habitable, puis erosion thermique sur la
 * hauteur rendue. Amplification des masses, terrasses et escarpements existent encore
 * derriere des CVars, coupees par defaut (TERRAIN_RELIEF_001). SampleActive suit
 * EXACTEMENT ce maillage, erosion comprise.
 */
namespace AnastasisTerrainForge
{
inline constexpr int32 DefaultSubdiv = 4;

/**
 * Marge de tuiles qu'un HaloCrop doit porter autour de Crop pour qu'un chunk raccorde
 * EXACTEMENT au monde entier. 2 et pas 1 : l'altitude fine est un Catmull-Rom sur 4x4
 * tuiles (anastasis.Terrain.Forge.Bicubic), et le voisin fin juste hors du chunk, lu par
 * la pente et le Laplacien, tombe dans une cellule dont le stencil deborde de deux tuiles.
 */
inline constexpr int32 HaloTiles = 2;

struct FMesh
{
	double SpatialScale = 1.0;
	bool bHumanGeography = false;
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

/**
 * Remplace InOut par le maillage forgé. False = laisser la surface tuilée telle quelle.
 *
 * HaloCrop (optionnel) : un instantané couvrant Crop plus HaloTiles tuiles voisines,
 * dans le même repère absolu (OriginX/OriginY). L'altitude fine de tout Crop y est
 * alors lue, pas seulement celle des voisins hors emprise. Pente et Laplacien en tirent alors de
 * vrais voisins au bord fin de Crop au lieu de dupliquer le sommet du bord sur lui-même
 * (l'artefact clamp-sur-soi qui pouvait faire gicler l'escarpement pile aux bords de
 * chunk, sur les pentes raides). Sans HaloCrop, le comportement est inchangé : c'est le
 * seul cas correct quand Crop touche déjà le bord du MONDE, où il n'existe aucune tuile
 * voisine à lire.
 *
 * OutLaplacian (optionnel, tests uniquement) : copie du Laplacien fin par sommet utilisé
 * pour l'escarpement/la macro-crête. Le bassin habitable et le point haut cherchent leur
 * candidat sur TOUTE l'emprise de Crop, donc leur placement change legitimement avec la
 * taille de Crop — comparer la hauteur finale entre deux emprises differentes mesurerait
 * ca, pas l'artefact de bord. Le Laplacien, lui, ne depend que des 4 voisins immediats :
 * c'est la seule quantite qu'un test peut comparer directement a une autre emprise pour
 * juger HaloCrop sans ce bruit.
 */
bool Apply(
	const AnastasisWorldView::FWorldVisualSnapshot& Crop,
	AnastasisTerrainSurface::FGeometry& InOut,
	FMesh& OutMeta,
	const AnastasisWorldView::FWorldVisualSnapshot* HaloCrop = nullptr,
	TArray<double>* OutLaplacian = nullptr);

bool SampleHeight(const FMesh& Mesh, double WorldX, double WorldY, double& OutZ);
bool SampleActiveWater(double WorldX, double WorldY, double& OutZ);

/** Normales par sommet recalculees depuis les triangles, comme a la fin d'Apply. */
void RecomputeNormals(AnastasisTerrainSurface::FGeometry& Geometry);

void SetActive(const FMesh& Mesh);
void ClearActive();
/** Échantillon du sol réellement rendu si un Apply est actif, sinon false. */
bool SampleActive(double WorldX, double WorldY, double& OutZ);

inline int32 FineVerticesFor(int32 CoarseW, int32 CoarseH, int32 Subdiv)
{
	return ((CoarseW - 1) * Subdiv + 1) * ((CoarseH - 1) * Subdiv + 1);
}
}
