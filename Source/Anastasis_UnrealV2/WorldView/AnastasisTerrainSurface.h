#pragma once
#include "WorldView/AnastasisWorldView.h"

// Presentation only. Vertices are source tile centers; outer half-tile strips
// are deliberately NOT extrapolated. No border halo or new altitude is needed.
//
// Colors and the water plane are PROJECTIONS of the snapshot, never new simulation:
//   terre/eau  <- FVisualTile::Type
//   profondeur <- FVisualTile::Shade   (Lerp(0.6, -1.0, Depth) cote AnastasisSim)
//   rive       <- FVisualTile::Shore   (0 sur l'eau, decroit sur ~5 tuiles)
//   altitude   <- FVisualTile::Alt normalisee sur le crop
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
}
