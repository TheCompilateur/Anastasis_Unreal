#include "WorldView/AnastasisTerrainSurface.h"

bool AnastasisTerrainSurface::Build(const AnastasisWorldView::FWorldVisualSnapshot& Crop, FGeometry& Out)
{
    Out = FGeometry{};
    if (Crop.W != 32 || Crop.H != 32 || Crop.SourceW != 96 || Crop.SourceH != 96
        || Crop.OriginX < 0 || Crop.OriginY < 0 || Crop.OriginX + 32 > 96 || Crop.OriginY + 32 > 96
        || Crop.Tiles.Num() != 1024) return false;
    FGeometry Result;
    for (int32 I = 0; I < Crop.Tiles.Num(); ++I)
    {
        const auto& T = Crop.Tiles[I];
        const int32 X = Crop.OriginX + I % 32, Y = Crop.OriginY + I / 32;
        if (T.X != X || T.Y != Y || T.SourceIndex != Y * 96 + X || !FMath::IsFinite(T.Alt)) return false;
        const FVector P = AnastasisWorldView::TileToUnreal(X, Y, T.Alt);
        if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y) || !FMath::IsFinite(P.Z)) return false;
        Result.Vertices.Add(P);
        Result.SourceIndices.Add(T.SourceIndex);
    }
    Result.Normals.Init(FVector::ZeroVector, 1024);
    for (int32 Y = 0; Y < 31; ++Y)
        for (int32 X = 0; X < 31; ++X)
        {
            const int32 A = Y * 32 + X, B = A + 1, C = A + 32, D = C + 1;
            // Unreal front faces use clockwise winding viewed from above.
            Result.Triangles.Append({A, C, B, B, C, D});
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
