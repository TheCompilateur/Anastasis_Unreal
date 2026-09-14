#pragma once
#include "WorldView/AnastasisWorldView.h"

// Presentation only. Vertices are source tile centers; outer half-tile strips
// are deliberately NOT extrapolated. No border halo or new altitude is needed.
//
// Colors and the water plane are PROJECTIONS of the snapshot, never new simulation:
//   terre/eau     <- FVisualTile::Type
//   profondeur    <- FVisualTile::Shade    (Lerp(0.6, -1.0, Depth) cote AnastasisSim)
//   rive saturee  <- FVisualTile::Shore    (0 sur l'eau, decroit sur ~5 tuiles)
//   vase / crue   <- FVisualTile::Wetness  (1 au bord, decroit sur ~6.5 tuiles)
//   ecoulement    <- FVisualTile::FlowAmt  (eau seulement, seuil hydrologique 0.06)
//   pente (terre) <- FVisualTile::Shade    (tanh des diffs d'altitude, [-1, 1])
//   altitude      <- FVisualTile::Alt normalisee sur le crop
// Le plan d'eau est plat a AnastasisWorld::SeaLevel : c'est une constante du monde,
// pas une hauteur inventee.
namespace AnastasisTerrainSurface
{
/** Niveau de la mer en unites Unreal. */
inline constexpr double WaterPlaneZ = AnastasisWorld::SeaLevel * AnastasisWorldView::AltitudeScale;

// Le MONDE source reste le 96x96 canonique : Build refuse toute autre taille de
// monde. L'EMPRISE, elle, est libre -- d'une cellule unique (2x2 sommets) au monde
// entier -- pourvu qu'elle tienne dans ce monde. Les dimensions ne sont pas
// redefinies ici : elles sont reprises de WorldView, seul proprietaire.
inline constexpr int32 SourceW = AnastasisWorldView::ReferenceWidth;
inline constexpr int32 SourceH = AnastasisWorldView::ReferenceHeight;

// Tranche scellee WORLD_SLICE_006. Desormais une reference de non-regression
// (1024 sommets / 1922 triangles), plus une limite de ce que Build accepte.
inline constexpr int32 CropW = AnastasisWorldView::CanonicalCropWidth;
inline constexpr int32 CropH = AnastasisWorldView::CanonicalCropHeight;
inline constexpr int32 VertexCount = CropW * CropH;

/** Sommets et triangles produits par une emprise W x H. Une cellule = 2 triangles. */
inline constexpr int32 VerticesFor(int32 W, int32 H) { return W * H; }
inline constexpr int32 TrianglesFor(int32 W, int32 H) { return 2 * (W - 1) * (H - 1); }

struct FGeometry
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<int32> SourceIndices;

    /** RGB = lecture semantique du sol ; A = 1 sur l'eau, 0 sur la terre. */
    TArray<FLinearColor> Colors;

    /** Nappe d'eau plate : les memes sommets que le relief, Z fige a WaterPlaneZ. */
    TArray<FVector> WaterVertices;
    TArray<int32> WaterTriangles;
    TArray<FVector> WaterNormals;
};

bool Build(const AnastasisWorldView::FWorldVisualSnapshot& Crop, FGeometry& Out);

/**
 * Hauteur du sol REELLEMENT RENDU au point monde (X,Y), en unites Unreal.
 *
 * Pas l'altitude de la tuile : la surface est triangulee, donc entre deux sommets
 * le sol est un plan incline, pas une marche. Tout ce qu'on pose dessus -- arbres,
 * ruines, plus tard un pion -- doit lire CETTE hauteur, sinon l'objet flotte ou
 * s'enfonce d'autant que la pente locale.
 *
 * L'echantillon suit exactement la triangulation de Build : la cellule est coupee
 * sur la diagonale B-C, donc le point est evalue sur le plan du triangle qui le
 * contient, pas sur une bilineaire qui ne correspondrait a aucune face rendue.
 *
 * Renvoie false hors de l'emprise -- il n'y a alors pas de sol, et rien ne doit y
 * etre pose. C'est un refus, pas un zero.
 */
bool SampleHeight(const AnastasisWorldView::FWorldVisualSnapshot& Crop, double WorldX, double WorldY, double& OutZ);

/**
 * Inverse de Shade = Lerp(0.6, -1.0, Depth) cote sim. Aucune donnee nouvelle.
 */
double WaterDepthFromShade(double Shade);

/**
 * Projection sommet : les champs deja presents sur la tuile, rien d'invente.
 * Palette distincte des cubes DEBUG.
 */
FLinearColor TileColor(const AnastasisWorldView::FVisualTile& Tile, double MinAlt, double MaxAlt);
}
