#include "WorldView/AnastasisEcologicalDressing.h"
#include "WorldView/AnastasisTerrainSurface.h"

namespace AnastasisEcologicalDressing
{
namespace
{
using namespace AnastasisWorldView;
using AnastasisWorld::ETileType;

uint32 Hash(uint32 Seed, int32 X, int32 Y, uint32 Salt)
{
    uint32 H = Seed ^ 0x9E3779B9u;
    H = (H ^ static_cast<uint32>(X)) * 0x85EBCA6Bu;
    H = (H ^ static_cast<uint32>(Y)) * 0xC2B2AE35u;
    H = (H ^ Salt) * 0x27D4EB2Fu;
    return H ^ (H >> 15);
}
double Unit(uint32 H) { return static_cast<double>(H) / 4294967296.0; }
double Smooth(double V) { V = FMath::Clamp(V, 0.0, 1.0); return V * V * (3.0 - 2.0 * V); }

double Cluster(uint32 Seed, double X, double Y)
{
    const int32 IX = FMath::FloorToInt(X), IY = FMath::FloorToInt(Y);
    const double U = Smooth(X - IX), V = Smooth(Y - IY);
    return FMath::Lerp(
        FMath::Lerp(Unit(Hash(Seed, IX, IY, 91)), Unit(Hash(Seed, IX+1, IY, 91)), U),
        FMath::Lerp(Unit(Hash(Seed, IX, IY+1, 91)), Unit(Hash(Seed, IX+1, IY+1, 91)), U), V);
}

bool Habitat(ETileType Type)
{
    return Type == ETileType::Forest || Type == ETileType::Grass || Type == ETileType::Scrub;
}

bool ValidScale(const FVector2D& V)
{
    return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && V.X > 0.0 && V.Y >= V.X && V.Y <= 3.0;
}

bool GroundAt(const FWorldVisualSnapshot& S, double X, double Y,
    const FAnastasisForestDressingSettings& C, double& Z, double& Slope)
{
    const double Radius = C.RootRadius * TileWorldSize;
    const FVector2D Offsets[] = {{0,0}, {Radius,0}, {-Radius,0}, {0,Radius}, {0,-Radius}};
    for (const auto& O : Offsets)
    {
        const double PX = X + O.X, PY = Y + O.Y;
        const FVisualTile* Tile = FindTile(S, FMath::FloorToInt(PX / TileWorldSize), FMath::FloorToInt(PY / TileWorldSize));
        double H;
        if (!Tile || !Habitat(Tile->Type) || !AnastasisTerrainSurface::SampleHeight(S, PX, PY, H)
            || H <= AnastasisTerrainSurface::WaterPlaneZ + C.WaterClearanceUU) return false;
    }
    if (!AnastasisTerrainSurface::SampleHeight(S, X, Y, Z)) return false;
    // Exact triangle gradient, same B-C diagonal as TerrainSurface::Build/SampleHeight.
    const double U = X / TileWorldSize - 0.5, V = Y / TileWorldSize - 0.5;
    const int32 IX = FMath::Min(FMath::FloorToInt(U), S.W - 2);
    const int32 IY = FMath::Min(FMath::FloorToInt(V), S.H - 2);
    const int32 A = IY * S.W + IX, B = A + 1, CC = A + S.W, D = CC + 1;
    const bool First = (U - IX) + (V - IY) <= 1.0;
    const double DX = First ? S.Tiles[B].Alt - S.Tiles[A].Alt : S.Tiles[D].Alt - S.Tiles[CC].Alt;
    const double DY = First ? S.Tiles[CC].Alt - S.Tiles[A].Alt : S.Tiles[D].Alt - S.Tiles[B].Alt;
    Slope = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX*DX + DY*DY) * AltitudeScale / TileWorldSize));
    return true;
}
}

bool Build(const FWorldVisualSnapshot& S, const FAnastasisForestDressingSettings& C, FPlan& Out, FString& Error)
{
    Out = FPlan{};
    Error.Reset();
    if (S.SourceW != ReferenceWidth || S.SourceH != ReferenceHeight || S.OriginX != 0 || S.OriginY != 0
        || S.W != ReferenceWidth || S.H != ReferenceHeight || S.Tiles.Num() != ReferenceWidth * ReferenceHeight)
    {
        Error = TEXT("Source: expected full canonical 96x96 snapshot"); return false;
    }
    const float Values[] = {C.EdgeRadius,C.Density,C.ClusterSpan,C.ClearingThreshold,C.MaxSlopeDegrees,
        C.WetnessPenalty,C.WaterClearanceUU,C.MinimumSpacing,C.RootRadius};
    for (float Value : Values)
        if (!FMath::IsFinite(Value)) { Error = TEXT("Settings: non-finite value"); return false; }
    if (C.CandidatesPerTile < 1 || C.CandidatesPerTile > 4 || C.EdgeRadius < 1 || C.EdgeRadius > 5
        || C.Density < 0 || C.Density > 1 || C.ClusterSpan < 1 || C.ClusterSpan > 16
        || C.ClearingThreshold < 0 || C.ClearingThreshold > 0.8 || C.MaxSlopeDegrees < 1 || C.MaxSlopeDegrees > 60
        || C.WetnessPenalty < 0 || C.WetnessPenalty > 1 || C.WaterClearanceUU < 0 || C.WaterClearanceUU > 50
        || C.MinimumSpacing < 0.1 || C.MinimumSpacing > 0.9 || C.RootRadius < 0 || C.RootRadius > 0.25
        || !ValidScale(C.YoungScale) || !ValidScale(C.SecondaryScale) || !ValidScale(C.CanopyScale))
    {
        Error = TEXT("Settings: value outside supported range"); return false;
    }
    for (int32 I=0; I<S.Tiles.Num(); ++I)
    {
        const auto& T = S.Tiles[I];
        if (T.X != I % S.W || T.Y != I / S.W || T.SourceIndex != I
            || !FMath::IsFinite(T.Alt) || !FMath::IsFinite(T.Wetness) || T.Wetness < 0 || T.Wetness > 1
            || static_cast<uint8>(T.Type) >= AnastasisWorld::TileTypeCount)
        {
            Error = FString::Printf(TEXT("Source.Tiles[%d]: invalid coordinates, altitude, wetness or type"), I); return false;
        }
    }
    if (!C.bEnabled) return true;
    FPlan Result;
    const double Spacing = C.MinimumSpacing * TileWorldSize;
    TMap<FIntPoint, TArray<FVector2D>> Occupied;
    const int32 Radius = FMath::CeilToInt(C.EdgeRadius);
    for (const auto& T : S.Tiles)
    {
        if (!Habitat(T.Type)) continue;
        for (int32 Candidate=0; Candidate<C.CandidatesPerTile; ++Candidate)
        {
            const uint32 Seed = Hash(S.Seed, T.X, T.Y, 100 + Candidate);
            const double X = T.X + Unit(Hash(Seed,T.X,T.Y,1));
            const double Y = T.Y + Unit(Hash(Seed,T.X,T.Y,2));
            double Weight=0.0, Forest=0.0;
            for (int32 DY=-Radius; DY<=Radius; ++DY)
                for (int32 DX=-Radius; DX<=Radius; ++DX)
                {
                    const double Dist = FVector2D(X - (T.X+DX+0.5), Y - (T.Y+DY+0.5)).Size();
                    const double W = FMath::Max(0.0, 1.0 - Dist / C.EdgeRadius);
                    const auto* N = FindTile(S, T.X+DX, T.Y+DY);
                    if (!N) continue;
                    Weight += W;
                    if (N->Type == ETileType::Forest) Forest += W;
                }
            const double Support = Weight > 0 ? Forest / Weight : 0.0;
            if (Support <= 0) continue;
            const double Patch = Smooth((Cluster(S.Seed, X / C.ClusterSpan, Y / C.ClusterSpan)
                - C.ClearingThreshold) / (1.0 - C.ClearingThreshold));
            const double Probability = C.Density * FMath::Sqrt(Support) * Patch * (1.0 - C.WetnessPenalty * T.Wetness);
            if (Unit(Hash(Seed,T.X,T.Y,3)) >= Probability) continue;
            FPlacement P;
            double Z, Slope;
            if (!GroundAt(S, X * TileWorldSize, Y * TileWorldSize, C, Z, Slope))
            { ++Result.RejectedWaterOrFootprint; continue; }
            if (Slope > C.MaxSlopeDegrees)
            { ++Result.RejectedSlope; continue; }
            const FVector2D XY(X * TileWorldSize, Y * TileWorldSize);
            const FIntPoint Cell(FMath::FloorToInt(XY.X / Spacing), FMath::FloorToInt(XY.Y / Spacing));
            bool Near=false;
            for (int32 DY=-1; DY<=1; ++DY)
                for (int32 DX=-1; DX<=1; ++DX)
                    if (const auto* Neighbors = Occupied.Find(Cell + FIntPoint(DX,DY)))
                        for (const auto& N : *Neighbors)
                            Near |= FVector2D::DistSquared(N,XY) < Spacing * Spacing;
            if (Near) { ++Result.RejectedSpacing; continue; }
            Occupied.FindOrAdd(Cell).Add(XY);
            const double Mature = Smooth((Support - 0.25) / 0.60);
            const double Choice = Unit(Hash(Seed,T.X,T.Y,4));
            P.Layer = Choice < Mature * 0.60 ? ELayer::Canopy
                : Choice < 0.25 + Mature * 0.65 ? ELayer::Secondary : ELayer::Young;
            const FVector2D Envelope = P.Layer == ELayer::Canopy ? C.CanopyScale
                : P.Layer == ELayer::Secondary ? C.SecondaryScale : C.YoungScale;
            P.ScaleMultiplier = FMath::Lerp(Envelope.X, Envelope.Y, Unit(Hash(Seed,T.X,T.Y,5)));
            P.SourceIndex = T.SourceIndex;
            P.VisualSeed = Seed;
            P.Ground = FVector(XY.X,XY.Y,Z);
            P.SlopeDegrees = Slope;
            Result.Instances.Add(P);
        }
    }
    Out = MoveTemp(Result);
    return true;
}
}
