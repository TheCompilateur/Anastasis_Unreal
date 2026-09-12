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

// Cette surface ne traite QUE le crop canonique. Les dimensions ne sont pas
// redefinies ici : elles sont reprises de WorldView, seul proprietaire.
inline constexpr int32 SourceW = AnastasisWorldView::ReferenceWidth;
inline constexpr int32 SourceH = AnastasisWorldView::ReferenceHeight;
inline constexpr int32 CropW = AnastasisWorldView::CanonicalCropWidth;
inline constexpr int32 CropH = AnastasisWorldView::CanonicalCropHeight;
inline constexpr int32 VertexCount = CropW * CropH;

struct FGeometry
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<int32> SourceIndices;

    /** RGB = lecture semantique du sol ; A = 1 sur l'eau, 0 sur la terre. */
    TArray<FLinearColor> Colors;

    /** Nappe d'eau plate : memes 1024 sommets, Z fige a WaterPlaneZ. */
    TArray<FVector> WaterVertices;
    TArray<int32> WaterTriangles;
    TArray<FVector> WaterNormals;
};

bool Build(const AnastasisWorldView::FWorldVisualSnapshot& Crop, FGeometry& Out);
}
