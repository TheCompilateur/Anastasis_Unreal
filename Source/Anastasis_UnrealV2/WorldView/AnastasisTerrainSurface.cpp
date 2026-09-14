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

/** Zone inondable / vase : sol sature pontique, pas une plage seche. */
const FLinearColor WetMud(0.204f, 0.184f, 0.137f);
/** Rive saturee : berge humide au trait de cote. */
const FLinearColor SaturatedBank(0.247f, 0.220f, 0.165f);
const FLinearColor HighlandRock(0.518f, 0.490f, 0.463f);
const FLinearColor ShallowWater(0.102f, 0.361f, 0.427f);
const FLinearColor DeepWater(0.016f, 0.063f, 0.204f);
/** Ecoulement : toujours bleu-dominant, un peu plus trouble que l'eau stagnante. */
const FLinearColor ChannelWater(0.078f, 0.286f, 0.345f);
}

double AnastasisTerrainSurface::WaterDepthFromShade(double Shade)
{
    return FMath::Clamp((0.6 - Shade) / 1.6, 0.0, 1.0);
}

FLinearColor AnastasisTerrainSurface::TileColor(const AnastasisWorldView::FVisualTile& T, double MinAlt, double MaxAlt)
{
    if (T.Type == AnastasisWorld::ETileType::Water)
    {
        const double Depth = WaterDepthFromShade(T.Shade);
        FLinearColor Water = FMath::Lerp(ShallowWater, DeepWater, static_cast<float>(Depth));
        const float Flow = static_cast<float>(FMath::Clamp(T.FlowAmt, 0.0, 1.0));
        Water = FMath::Lerp(Water, ChannelWater, FMath::Pow(Flow, 0.85f) * 0.55f);
        Water.A = 1.0f;
        return Water;
    }

    FLinearColor Color = SurfaceTypeColor(T.Type);

    const double HighSpan = FMath::Max(MaxAlt - AnastasisWorld::SeaLevel, 1.e-6);
    const double Height = FMath::Clamp((T.Alt - AnastasisWorld::SeaLevel) / HighSpan, 0.0, 1.0);
    Color = FMath::Lerp(Color, HighlandRock, static_cast<float>(FMath::SmoothStep(0.45, 1.0, Height)));

    // Wetness est plus large que Shore (wd/6.5 contre ~5 tuiles) : c'est le deuxieme
    // degre de liberte, la crue / la vase en arriere de la berge.
    Color = FMath::Lerp(Color, WetMud, static_cast<float>(FMath::Pow(T.Wetness, 1.35) * 0.55));

    // Shore : berge saturee au contact de l'eau, pas le sable mediterraneen.
    Color = FMath::Lerp(Color, SaturatedBank, static_cast<float>(FMath::Pow(T.Shore, 1.4) * 0.70));

    const double LowSpan = FMath::Max(AnastasisWorld::SeaLevel - MinAlt, 1.e-6);
    const double Low = FMath::Clamp((AnastasisWorld::SeaLevel - T.Alt) / LowSpan, 0.0, 1.0);
    Color *= static_cast<float>(FMath::Lerp(1.0, 0.82, Low));

    // Shade terre = pente sim (pas la profondeur, reservee a l'eau). 0 = identite.
    const float SlopeLit = static_cast<float>(1.0 + FMath::Clamp(T.Shade, -1.0, 1.0) * 0.18);
    Color.R = FMath::Clamp(Color.R * SlopeLit, 0.0f, 1.0f);
    Color.G = FMath::Clamp(Color.G * SlopeLit, 0.0f, 1.0f);
    Color.B = FMath::Clamp(Color.B * SlopeLit, 0.0f, 1.0f);

    Color.A = 0.0f;
    return Color;
}

bool AnastasisTerrainSurface::SampleHeight(
    const AnastasisWorldView::FWorldVisualSnapshot& Crop, double WorldX, double WorldY, double& OutZ)
{
    OutZ = 0.0;
    const int32 W = Crop.W, H = Crop.H;
    if (W < 2 || H < 2 || Crop.Tiles.Num() != VerticesFor(W, H)) return false;
    if (!FMath::IsFinite(WorldX) || !FMath::IsFinite(WorldY)) return false;

    // Les sommets sont au CENTRE des tuiles (TileToUnreal ajoute 0.5) : le champ de
    // hauteur ne commence donc qu'a un demi-tuile du bord de l'emprise. Au-dela il n'y
    // a pas de face rendue -- Build n'extrapole deliberement aucune bande exterieure.
    const double U = WorldX / AnastasisWorldView::TileWorldSize - 0.5 - static_cast<double>(Crop.OriginX);
    const double V = WorldY / AnastasisWorldView::TileWorldSize - 0.5 - static_cast<double>(Crop.OriginY);
    if (U < 0.0 || V < 0.0 || U > static_cast<double>(W - 1) || V > static_cast<double>(H - 1)) return false;

    const int32 X = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(U)), 0, W - 2);
    const int32 Y = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(V)), 0, H - 2);
    const double Fx = U - static_cast<double>(X), Fy = V - static_cast<double>(Y);

    const int32 A = Y * W + X, B = A + 1, C = A + W, D = C + 1;
    const double ZA = Crop.Tiles[A].Alt * AnastasisWorldView::AltitudeScale;
    const double ZB = Crop.Tiles[B].Alt * AnastasisWorldView::AltitudeScale;
    const double ZC = Crop.Tiles[C].Alt * AnastasisWorldView::AltitudeScale;
    const double ZD = Crop.Tiles[D].Alt * AnastasisWorldView::AltitudeScale;
    if (!FMath::IsFinite(ZA) || !FMath::IsFinite(ZB) || !FMath::IsFinite(ZC) || !FMath::IsFinite(ZD)) return false;

    // Build emet {A,C,B} puis {B,C,D} : la diagonale est B-C. Fx+Fy <= 1 tombe dans le
    // premier triangle, le reste dans le second. Chaque branche evalue le plan de sa face.
    OutZ = (Fx + Fy <= 1.0)
        ? ZA + Fx * (ZB - ZA) + Fy * (ZC - ZA)
        : ZD + (1.0 - Fx) * (ZC - ZD) + (1.0 - Fy) * (ZB - ZD);
    return FMath::IsFinite(OutZ);
}

bool AnastasisTerrainSurface::Build(const AnastasisWorldView::FWorldVisualSnapshot& Crop, FGeometry& Out)
{
    Out = FGeometry{};
    // L'emprise est libre, le monde ne l'est pas : une surface batie sur autre chose
    // que le 96x96 canonique ne serait plus adossee a la verite de simulation.
    // Il faut deux sommets par axe pour former une seule cellule.
    const int32 W = Crop.W, H = Crop.H;
    if (W < 2 || H < 2 || Crop.SourceW != SourceW || Crop.SourceH != SourceH
        || Crop.OriginX < 0 || Crop.OriginY < 0
        || Crop.OriginX + W > SourceW || Crop.OriginY + H > SourceH
        || Crop.Tiles.Num() != VerticesFor(W, H)) return false;
    const int32 Vertices = VerticesFor(W, H);
    FGeometry Result;
    TArray<bool> IsWater;
    IsWater.Reserve(Vertices);
    for (int32 I = 0; I < Crop.Tiles.Num(); ++I)
    {
        const auto& T = Crop.Tiles[I];
        const int32 X = Crop.OriginX + I % W, Y = Crop.OriginY + I / W;
        if (T.X != X || T.Y != Y || T.SourceIndex != Y * Crop.SourceW + X || !FMath::IsFinite(T.Alt)) return false;
        if (!FMath::IsFinite(T.Shore) || !FMath::IsFinite(T.Shade)
            || !FMath::IsFinite(T.Wetness) || !FMath::IsFinite(T.FlowAmt)
            || !FMath::IsFinite(T.FlowX) || !FMath::IsFinite(T.FlowZ)) return false;
        const FVector P = AnastasisWorldView::TileToUnreal(X, Y, T.Alt);
        if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y) || !FMath::IsFinite(P.Z)) return false;
        Result.Vertices.Add(P);
        Result.SourceIndices.Add(T.SourceIndex);
        Result.Colors.Add(TileColor(T, Crop.MinAlt, Crop.MaxAlt));
        Result.WaterVertices.Add(FVector(P.X, P.Y, WaterPlaneZ));
        IsWater.Add(T.Type == AnastasisWorld::ETileType::Water);
    }
    Result.Normals.Init(FVector::ZeroVector, Vertices);
    Result.WaterNormals.Init(FVector::UpVector, Vertices);
    for (int32 Y = 0; Y < H - 1; ++Y)
        for (int32 X = 0; X < W - 1; ++X)
        {
            const int32 A = Y * W + X, B = A + 1, C = A + W, D = C + 1;
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
