#include "WorldView/AnastasisTerrainSurface.h"

namespace
{
using AnastasisWorld::ETileType;

/** Palette de presentation de la tranche. Distincte de la palette DEBUG des cubes. */
FLinearColor SurfaceTypeColor(ETileType Type)
{
    switch (Type)
    {
    case ETileType::Grass:  return FLinearColor(0.243f, 0.412f, 0.169f);
    case ETileType::Forest: return FLinearColor(0.102f, 0.243f, 0.114f);
    case ETileType::Scrub:  return FLinearColor(0.392f, 0.396f, 0.212f);
    case ETileType::Field:  return FLinearColor(0.557f, 0.482f, 0.176f);
    case ETileType::Stone:  return FLinearColor(0.400f, 0.396f, 0.380f);
    case ETileType::Ruin:   return FLinearColor(0.353f, 0.302f, 0.318f);
    case ETileType::Water:  return FLinearColor(0.055f, 0.220f, 0.353f);
    }
    return FLinearColor::White;
}

const FLinearColor ShoreSand(0.694f, 0.612f, 0.435f);
const FLinearColor HighlandRock(0.518f, 0.490f, 0.463f);
const FLinearColor ShallowWater(0.102f, 0.361f, 0.427f);
const FLinearColor DeepWater(0.016f, 0.063f, 0.204f);

/** Projette une tuile en couleur lisible. Aucune donnee nouvelle : Type, Shade, Shore, Alt. */
FLinearColor TileColor(const AnastasisWorldView::FVisualTile& T, double MinAlt, double MaxAlt)
{
    if (T.Type == ETileType::Water)
    {
        // Shade = Lerp(0.6, -1.0, Depth) cote AnastasisSim : on inverse pour retrouver Depth.
        const double Depth = FMath::Clamp((0.6 - T.Shade) / 1.6, 0.0, 1.0);
        FLinearColor Water = FMath::Lerp(ShallowWater, DeepWater, static_cast<float>(Depth));
        Water.A = 1.0f;
        return Water;
    }

    FLinearColor Color = SurfaceTypeColor(T.Type);

    // Altitude : au-dessus du niveau de la mer, le sol se mineralise avec la hauteur.
    const double HighSpan = FMath::Max(MaxAlt - AnastasisWorld::SeaLevel, 1.e-6);
    const double Height = FMath::Clamp((T.Alt - AnastasisWorld::SeaLevel) / HighSpan, 0.0, 1.0);
    Color = FMath::Lerp(Color, HighlandRock, static_cast<float>(FMath::SmoothStep(0.45, 1.0, Height)));

    // Rive : bande cotiere sableuse, directement pilotee par Shore.
    Color = FMath::Lerp(Color, ShoreSand, static_cast<float>(FMath::Pow(T.Shore, 1.6) * 0.85));

    // Basses terres : leger assombrissement pour que le fond de vallee se distingue.
    const double LowSpan = FMath::Max(AnastasisWorld::SeaLevel - MinAlt, 1.e-6);
    const double Low = FMath::Clamp((AnastasisWorld::SeaLevel - T.Alt) / LowSpan, 0.0, 1.0);
    Color *= static_cast<float>(FMath::Lerp(1.0, 0.82, Low));

    Color.A = 0.0f;
    return Color;
}
}

bool AnastasisTerrainSurface::Build(const AnastasisWorldView::FWorldVisualSnapshot& Crop, FGeometry& Out)
{
    Out = FGeometry{};
    if (Crop.W != CropW || Crop.H != CropH || Crop.SourceW != SourceW || Crop.SourceH != SourceH
        || Crop.OriginX < 0 || Crop.OriginY < 0
        || Crop.OriginX + CropW > SourceW || Crop.OriginY + CropH > SourceH
        || Crop.Tiles.Num() != VertexCount) return false;
    FGeometry Result;
    TArray<bool> IsWater;
    IsWater.Reserve(VertexCount);
    for (int32 I = 0; I < Crop.Tiles.Num(); ++I)
    {
        const auto& T = Crop.Tiles[I];
        const int32 X = Crop.OriginX + I % CropW, Y = Crop.OriginY + I / CropW;
        if (T.X != X || T.Y != Y || T.SourceIndex != Y * SourceW + X || !FMath::IsFinite(T.Alt)) return false;
        if (!FMath::IsFinite(T.Shore) || !FMath::IsFinite(T.Shade)) return false;
        const FVector P = AnastasisWorldView::TileToUnreal(X, Y, T.Alt);
        if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y) || !FMath::IsFinite(P.Z)) return false;
        Result.Vertices.Add(P);
        Result.SourceIndices.Add(T.SourceIndex);
        Result.Colors.Add(TileColor(T, Crop.MinAlt, Crop.MaxAlt));
        Result.WaterVertices.Add(FVector(P.X, P.Y, WaterPlaneZ));
        IsWater.Add(T.Type == AnastasisWorld::ETileType::Water);
    }
    Result.Normals.Init(FVector::ZeroVector, VertexCount);
    Result.WaterNormals.Init(FVector::UpVector, VertexCount);
    for (int32 Y = 0; Y < CropH - 1; ++Y)
        for (int32 X = 0; X < CropW - 1; ++X)
        {
            const int32 A = Y * CropW + X, B = A + 1, C = A + CropW, D = C + 1;
            // Unreal front faces use clockwise winding viewed from above.
            Result.Triangles.Append({A, C, B, B, C, D});
            // Une cellule porte de l'eau des qu'une de ses quatre tuiles source est de l'eau :
            // le plan plat s'encastre alors naturellement dans le relief et dessine le trait de cote.
            if (IsWater[A] || IsWater[B] || IsWater[C] || IsWater[D])
            {
                Result.WaterTriangles.Append({A, C, B, B, C, D});
            }
        }
    for (int32 I = 0; I < Result.Triangles.Num(); I += 3)
    {
        const int32 A = Result.Triangles[I], B = Result.Triangles[I+1], C = Result.Triangles[I+2];
        const FVector N = FVector::CrossProduct(Result.Vertices[C]-Result.Vertices[A], Result.Vertices[B]-Result.Vertices[A]);
        Result.Normals[A] += N; Result.Normals[B] += N; Result.Normals[C] += N;
    }
    for (FVector& N : Result.Normals) N = N.GetSafeNormal();
    Out = MoveTemp(Result);
    return true;
}
