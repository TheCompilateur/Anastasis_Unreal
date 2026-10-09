#include "WorldView/AnastasisEcologicalDressing.h"
#include "WorldView/AnastasisTerrainSurface.h"
#include "WorldView/AnastasisHumanGeography.h"
#include "HAL/IConsoleManager.h"

namespace AnastasisEcologicalDressing
{
static TAutoConsoleVariable<int32> CVarPonticSlopeEcology(
    TEXT("anastasis.Dressing.PonticSlopeEcology"), 1,
    TEXT("1: wet, drained hillsides may hold more woodland; 0: previous uniform wetness penalty."),
    ECVF_Default);
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

double Cluster(uint32 Seed, double X, double Y, uint32 Salt = 91)
{
    const int32 IX = FMath::FloorToInt(X), IY = FMath::FloorToInt(Y);
    const double U = Smooth(X - IX), V = Smooth(Y - IY);
    return FMath::Lerp(
        FMath::Lerp(Unit(Hash(Seed, IX, IY, Salt)), Unit(Hash(Seed, IX+1, IY, Salt)), U),
        FMath::Lerp(Unit(Hash(Seed, IX, IY+1, Salt)), Unit(Hash(Seed, IX+1, IY+1, Salt)), U), V);
}

/**
 * FOREST_TERRAIN_P2 -- a tile property read as a CONTINUOUS field.
 *
 * Bilinear between tile centres (X + 0.5), so a value that steps from one tile to the next
 * becomes a ramp across the twenty metres between them. Reading the candidate's own tile was
 * what cut the forest along the simulation grid: every per-tile test drew a straight,
 * axis-aligned edge, at exactly the tile size.
 */
double TileField(const FWorldVisualSnapshot& S, double X, double Y, TFunctionRef<double(const FVisualTile&)> Value)
{
    const double U = X - 0.5, V = Y - 0.5;
    const int32 IX = FMath::FloorToInt(U), IY = FMath::FloorToInt(V);
    const double FX = U - IX, FY = V - IY;
    const auto At = [&](int32 TX, int32 TY)
    {
        const FVisualTile* T = FindTile(S, FMath::Clamp(TX, 0, S.W - 1), FMath::Clamp(TY, 0, S.H - 1));
        return T ? Value(*T) : 0.0;
    };
    return FMath::Lerp(FMath::Lerp(At(IX, IY), At(IX + 1, IY), FX), FMath::Lerp(At(IX, IY + 1), At(IX + 1, IY + 1), FX), FY);
}

/**
 * FOREST_TERRAIN_P2 -- share of forest habitat around (X, Y), cone-weighted over Radius tiles.
 * 0.5 on a straight habitat border, rising to 1 about Radius inside it: the forest edge becomes a
 * band whose width is set here, not a line drawn along the tile grid.
 */
double HabitatShare(const FWorldVisualSnapshot& S, double X, double Y, double Radius)
{
    const int32 CX = FMath::FloorToInt(X), CY = FMath::FloorToInt(Y), R = FMath::CeilToInt(Radius);
    double Weight = 0.0, Habitable = 0.0;
    for (int32 DY = -R; DY <= R; ++DY)
        for (int32 DX = -R; DX <= R; ++DX)
        {
            const FVisualTile* N = FindTile(S, CX + DX, CY + DY);
            if (!N) continue;
            const double W = FMath::Max(0.0, 1.0 - FVector2D(X - (CX + DX + 0.5), Y - (CY + DY + 0.5)).Size() / Radius);
            Weight += W;
            Habitable += (N->Type == ETileType::Forest || N->Type == ETileType::Grass || N->Type == ETileType::Scrub
                || N->Type == ETileType::Stone) ? W : 0.0;
        }
    return Weight > 0.0 ? Habitable / Weight : 0.0;
}

/** FOREST_TERRAIN_P2 -- distribution constants of the macro forest, in simulation tiles. */
namespace P2
{
    /** Domain warp of the habitat field: the edge meanders by up to this much. */
    constexpr double EdgeWarp = 0.45;
    constexpr double EdgeWarpSpan = 2.2;
    /** Habitat share where the forest stops, the band over which it thins out to it, and the
     *  radius of the share. Measured on a straight forest/field border at scale 5 (replica of
     *  this file): 7 %, 35 %, 79 % of interior density over the first three half-tiles -- a
     *  thirty-metre fringe, where the grid cut stood at 106 % on the border itself. */
    constexpr double HabitatEdge = 0.58;
    constexpr double HabitatFringe = 0.40;
    constexpr double HabitatRadius = 3.0;
    /** Stand density: open woodland to closed stand, at this span. */
    constexpr double StandSpan = 5.0;
    constexpr double StandFloor = 0.55;
    /** Glades: small openings inside the mass. */
    constexpr double GladeSpan = 3.5;
    constexpr double GladeThreshold = 0.72;
    constexpr double GladeRamp = 0.14;
    constexpr double GladeDepth = 0.92;
    /** Lone trees and groves in open ground, per candidate, before the grove noise. */
    constexpr double LoneDensity = 0.018;
    constexpr double GroveSpan = 2.0;
    /** Gallery along the rendered rivers (plane trees by species), per candidate. */
    constexpr double GalleryDensity = 0.07;
    /** Candidates per (TileUU / TrunkSpacing)^2 -- enough to express the density field
     *  instead of saturating every tile against the trunk spacing. */
    constexpr double CandidatesPerSpacingCell = 2.0;
    /** Graines propres a chaque bruit. Hash() ne xore le sel qu'au dernier pas : deux sels voisins
     *  (91 masse, 95 clairiere) donneraient deux champs presque identiques. La graine, elle, passe
     *  les deux multiplications. */
    constexpr uint32 WarpXSeed = 0x1B873593u;
    constexpr uint32 WarpYSeed = 0xCC9E2D51u;
    constexpr uint32 StandSeed = 0x85EBCA77u;
    constexpr uint32 GladeSeed = 0xC2B2AE3Du;
    constexpr uint32 GroveSeed = 0x27D4EB2Fu;
}

bool Habitat(ETileType Type, bool bMacro = false)
{
    return Type == ETileType::Forest || Type == ETileType::Grass || Type == ETileType::Scrub
        || (bMacro && Type == ETileType::Stone);
}

bool ValidScale(const FVector2D& V)
{
    return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && V.X > 0.0 && V.Y >= V.X && V.Y <= 3.0;
}

bool GroundAt(const FWorldVisualSnapshot& S, double X, double Y,
    const FAnastasisForestDressingSettings& C, double& Z, double& Slope,
    const FRenderedHabitat* Rendered)
{
    const auto Sample = [&](double PX, double PY, double& H)
    {
        return (Rendered ? Rendered->SampleHeight(PX, PY, H)
            : AnastasisTerrainSurface::SampleHeight(S, PX, PY, H)) && FMath::IsFinite(H);
    };
    const double Radius = Rendered ? 60.0 : C.RootRadius * TileWorldSize;
    const FVector2D Offsets[] = {{0,0}, {Radius,0}, {-Radius,0}, {0,Radius}, {0,-Radius}};
    for (const auto& O : Offsets)
    {
        const double PX = X + O.X, PY = Y + O.Y;
        const FVisualTile* Tile = FindTile(S, FMath::FloorToInt(PX / (TileWorldSize * S.SpatialScale)), FMath::FloorToInt(PY / (TileWorldSize * S.SpatialScale)));
        double H, Water = AnastasisTerrainSurface::WaterPlaneZ;
        if (Rendered && Rendered->SampleWaterHeight && !Rendered->SampleWaterHeight(PX,PY,Water)) return false;
        if (!Tile || !Habitat(Tile->Type, Rendered != nullptr) || !Sample(PX, PY, H)
            || !FMath::IsFinite(Water) || H <= Water + C.WaterClearanceUU) return false;
    }
    if (!Sample(X, Y, Z)) return false;
    if (Rendered)
    {
        // Four one-sided gradients avoid averaging a narrow cliff into a plantable slope.
        double East, West, North, South;
        if (!Sample(X + Radius,Y,East) || !Sample(X - Radius,Y,West)
            || !Sample(X,Y + Radius,North) || !Sample(X,Y - Radius,South)) return false;
        const double DX = FMath::Max(FMath::Abs(East-Z), FMath::Abs(West-Z)) / Radius;
        const double DY = FMath::Max(FMath::Abs(North-Z), FMath::Abs(South-Z)) / Radius;
        Slope = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX*DX + DY*DY)));
        return true;
    }
    // Exact triangle gradient, same B-C diagonal as TerrainSurface::Build/SampleHeight.
    const double U = X / (TileWorldSize * S.SpatialScale) - 0.5, V = Y / (TileWorldSize * S.SpatialScale) - 0.5;
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

bool Build(const FWorldVisualSnapshot& S, const FAnastasisForestDressingSettings& C, FPlan& Out, FString& Error,
    const FRenderedHabitat* Rendered)
{
    Out = FPlan{};
    Error.Reset();
    if (S.SourceW != ReferenceWidth || S.SourceH != ReferenceHeight || S.OriginX != 0 || S.OriginY != 0
        || S.W != ReferenceWidth || S.H != ReferenceHeight || S.Tiles.Num() != ReferenceWidth * ReferenceHeight)
    {
        Error = TEXT("Source: expected full canonical 96x96 snapshot"); return false;
    }
    const float Values[] = {C.EdgeRadius,C.Density,C.ClusterSpan,C.ClearingThreshold,C.MaxSlopeDegrees,
        C.WetnessPenalty,C.WaterClearanceUU,C.MinimumSpacing,C.RootRadius,
        C.MassSpan,C.TrunkSpacingUU,C.HeightMultiplier,C.BasinClearRadius,C.HillsideMaxSlope};
    for (float Value : Values)
        if (!FMath::IsFinite(Value)) { Error = TEXT("Settings: non-finite value"); return false; }
    if (C.CandidatesPerTile < 1 || C.CandidatesPerTile > 4 || C.EdgeRadius < 1 || C.EdgeRadius > 5
        || C.Density < 0 || C.Density > 1 || C.ClusterSpan < 1 || C.ClusterSpan > 16
        || C.ClearingThreshold < 0 || C.ClearingThreshold > 0.8 || C.MaxSlopeDegrees < 1 || C.MaxSlopeDegrees > 60
        || C.WetnessPenalty < 0 || C.WetnessPenalty > 1 || C.WaterClearanceUU < 0 || C.WaterClearanceUU > 50
        || C.MinimumSpacing < 0.1 || C.MinimumSpacing > 0.9 || C.RootRadius < 0 || C.RootRadius > 0.25
        || !ValidScale(C.YoungScale) || !ValidScale(C.SecondaryScale) || !ValidScale(C.CanopyScale)
        || C.MassSpan < 4 || C.MassSpan > 32 || C.TrunkSpacingUU < 300 || C.TrunkSpacingUU > 1600
        || C.HeightMultiplier < 1 || C.HeightMultiplier > 4 || C.BasinClearRadius < 0 || C.BasinClearRadius > 20
        || C.HillsideMaxSlope < 1 || C.HillsideMaxSlope > 60)
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
    if (!C.bMacroForest) Rendered = nullptr;
    if (Rendered && (!Rendered->SampleHeight || (Rendered->bHasBasin && Rendered->Basin.ContainsNaN())))
    {
        Error = TEXT("Rendered: missing height sampler or invalid basin"); return false;
    }
    const bool bMacro = Rendered != nullptr;
    const bool bPonticSlopeEcology = bMacro && CVarPonticSlopeEcology.GetValueOnAnyThread() != 0;
    FPlan Result;
    const double TileUU = TileWorldSize * S.SpatialScale;
    if (!FMath::IsFinite(TileUU) || TileUU <= 0.0) { Error=TEXT("Source.SpatialScale: invalid scale"); return false; }
    const double Spacing = bMacro ? C.TrunkSpacingUU : C.MinimumSpacing * TileUU;
    // Physical sampling density stays useful when a simulation tile spans twenty metres.
    // Bounded work: at most 128 candidates per canonical tile; never one actor per tree.
    // FOREST_TERRAIN_P2 : 14 candidats par cellule d'espacement (128 par tuile a l'echelle 5)
    // saturaient chaque tuile contre l'espacement des troncs ; la probabilite n'y decidait plus
    // que du oui ou du non, d'ou une densite uniforme. Deux par cellule laissent la probabilite
    // s'exprimer en densite.
    const int32 CandidateCount = bMacro
        ? FMath::Clamp(FMath::CeilToInt(FMath::Square(TileUU/Spacing)*P2::CandidatesPerSpacingCell),4,128)
        : C.CandidatesPerTile;
    TMap<FIntPoint, TArray<FVector2D>> Occupied;
    const int32 Radius = FMath::CeilToInt(C.EdgeRadius);
    for (const auto& T : S.Tiles)
    {
        if (!Habitat(T.Type, bMacro)) continue;
        for (int32 Candidate=0; Candidate<CandidateCount; ++Candidate)
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
            double Support = Weight > 0 ? Forest / Weight : 0.0;
            double Z = 0.0, Slope = 0.0;
            double Opening = 1.0;
            // FOREST_TERRAIN_P2 (macro seulement) : lisiere, densite, clairieres, arbres isoles.
            double Fringe = 1.0, Stand = 1.0, Glade = 0.0, LoneProbability = 0.0, Wetness = T.Wetness;
            if (bMacro)
            {
                if (!GroundAt(S, X * TileUU, Y * TileUU, C, Z, Slope, Rendered))
                { ++Result.RejectedWaterOrFootprint; continue; }
                if (Slope > C.HillsideMaxSlope)
                { ++Result.RejectedSlope; continue; }
                // Habitat continu et deforme : champs, ruines et eau d'un cote, tout le reste de
                // l'autre. La lisiere suit une courbe, plus la grille, et s'eclaircit en degrade.
                const double WX = X + P2::EdgeWarp * (2.0 * Cluster(S.Seed ^ P2::WarpXSeed, X / P2::EdgeWarpSpan, Y / P2::EdgeWarpSpan, 92) - 1.0);
                const double WY = Y + P2::EdgeWarp * (2.0 * Cluster(S.Seed ^ P2::WarpYSeed, X / P2::EdgeWarpSpan + 17.3, Y / P2::EdgeWarpSpan + 5.1, 93) - 1.0);
                // Hors de la bande, plus de foret close ; un arbre isole reste possible (le candidat
                // est deja sur une tuile d'habitat : jamais sur un champ ni une ruine).
                Fringe = Smooth((HabitatShare(S, WX, WY, P2::HabitatRadius) - P2::HabitatEdge) / P2::HabitatFringe);
                Stand = P2::StandFloor + (1.0 - P2::StandFloor) * Cluster(S.Seed ^ P2::StandSeed, X / P2::StandSpan, Y / P2::StandSpan, 94);
                Glade = Smooth((Cluster(S.Seed ^ P2::GladeSeed, X / P2::GladeSpan, Y / P2::GladeSpan, 95) - P2::GladeThreshold) / P2::GladeRamp);
                Wetness = TileField(S, X, Y, [](const FVisualTile& N) { return N.Wetness; });
                // Upland shoulders and mountain shelves can carry a forest even where the
                // simulation labels a rock resource. Flat low grass remains an open valley.
                const double UnscaledHeight = (Z - AnastasisTerrainSurface::WaterPlaneZ) / S.SpatialScale;
                const double Upland = Smooth((UnscaledHeight - 160.0) / 500.0);
                const double Hillside = Smooth((Slope - 5.0) / 18.0);
                Support = FMath::Max(Support, Upland * 0.95 + Hillside * 0.35);
                Support = FMath::Clamp(Support, 0.0, 1.0);
                // Prairie : bilineaire, plus le type de la seule tuile du candidat.
                const double GrassShare = TileField(S, X, Y, [](const FVisualTile& N) { return N.Type == ETileType::Grass ? 1.0 : 0.0; });
                Opening *= FMath::Lerp(1.0, FMath::Max(Upland, Smooth((Slope - 5.0) / 10.0)), GrassShare);
                // Reserves humaines, communes a la foret et aux arbres isoles : bassin du village,
                // lit de la riviere ecrite, route du col.
                double Reserve = 1.0;
                if (S.bHumanGeography && S.Seed == ReferenceSeed)
                {
                    // Consume the EXISTING valley/pass/river weights; never author another
                    // terrain or duplicate its geographic masks in the vegetation layer.
                    const auto Geo = AnastasisHumanGeography::Evaluate(X,Y,
                        (AnastasisTerrainSurface::WaterPlaneZ + UnscaledHeight)/100.0);
                    Opening *= 1.0-Smooth((Geo.ValleyWeight-0.20)/0.60);
                    Reserve *= 1.0-Smooth(Geo.RiverWeight/0.50);
                    Reserve *= 1.0-Smooth(Geo.RoadWeight/0.30);
                }
                double Riparian = 0.0;
                if (Rendered->SampleRiparian && Rendered->SampleRiparian(X * TileUU, Y * TileUU, Riparian))
                {
                    // Berges et plaines d'inondation : ripisylve claire, pas une foret close.
                    Riparian = FMath::Clamp(Riparian, 0.0, 1.0);
                    Opening *= 1.0 - 0.8 * Riparian;
                }
                if (Rendered->bHasBasin && C.BasinClearRadius > 0.0f)
                {
                    const double Distance = FVector2D(X - Rendered->Basin.X / TileUU,
                        Y - Rendered->Basin.Y / TileUU).Size();
                    // Wide, gradual edge, keeping the village basin entirely free of trunks.
                    Reserve *= Smooth((Distance - C.BasinClearRadius) / 4.0);
                }
                Opening *= Reserve;
                // Arbres isoles et bosquets dans l'ouvert, et galerie le long des rivieres rendues :
                // la ou la foret close n'a pas sa place, un arbre seul l'a. Memes reserves.
                const double Grove = Smooth((Cluster(S.Seed ^ P2::GroveSeed, X / P2::GroveSpan, Y / P2::GroveSpan, 96) - 0.55) / 0.35);
                const double Gallery = Smooth((Riparian - 0.20) / 0.25) * (1.0 - Smooth((Riparian - 0.80) / 0.15));
                LoneProbability = (P2::LoneDensity * Grove + P2::GalleryDensity * Gallery) * Reserve;
                if (Opening <= 0.0 && LoneProbability <= 0.0) { ++Result.RejectedOpenGround; continue; }
            }
            if (Support <= 0 && LoneProbability <= 0) continue;
            const double Span = bMacro ? C.MassSpan : C.ClusterSpan;
            const double Patch = Smooth((Cluster(S.Seed, X / Span, Y / Span)
                - C.ClearingThreshold) / (1.0 - C.ClearingThreshold));
            const double CanopyCover = bMacro ? FMath::Sqrt(Patch) : Patch;
            // Wetness on a drained hillside is not the same habitat as saturated
            // valley ground. Keep the old penalty below 8 degrees; relax it only
            // across the rendered 8-24 degree shoulder, without changing water,
            // human reserves, trunk spacing, or the tile-mode reference.
            const double DrainedShoulder = bPonticSlopeEcology ? Smooth((Slope - 8.0) / 16.0) : 0.0;
            const double EffectiveWetnessPenalty = C.WetnessPenalty * (1.0 - 0.60 * DrainedShoulder);
            double Probability = Support > 0 ? C.Density * FMath::Sqrt(Support) * CanopyCover * Opening
                * (1.0 - EffectiveWetnessPenalty * Wetness) : 0.0;
            // Lisiere en degrade, peuplement plus ou moins serre, clairieres.
            if (bMacro) Probability *= Fringe * Stand * (1.0 - P2::GladeDepth * Glade);
            // Sans arbre isole (hors macro, ou hors de toute prairie), le tirage reste bit a bit
            // celui d'avant.
            const double Combined = LoneProbability > 0.0
                ? 1.0 - (1.0 - FMath::Max(Probability, 0.0)) * (1.0 - LoneProbability) : Probability;
            if (Unit(Hash(Seed,T.X,T.Y,3)) >= Combined) continue;
            const bool bLone = bMacro && LoneProbability > Probability;
            FPlacement P;
            if (!bMacro && !GroundAt(S, X * TileWorldSize * S.SpatialScale, Y * TileWorldSize * S.SpatialScale, C, Z, Slope, nullptr))
            { ++Result.RejectedWaterOrFootprint; continue; }
            if (!bMacro && Slope > C.MaxSlopeDegrees)
            { ++Result.RejectedSlope; continue; }
            const FVector2D XY(X * TileWorldSize * S.SpatialScale, Y * TileWorldSize * S.SpatialScale);
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
            // Macro : une lisiere, une clairiere ou un peuplement clair recrutent des arbres plus
            // jeunes -- la hauteur descend vers le bord au lieu de s'arreter net. Un arbre isole
            // a eu la place de grandir : canopee.
            const double Interior = Fringe * Smooth((Patch - 0.05) / 0.5) * (1.0 - Glade) * (0.5 + 0.5 * Mature);
            P.bLone = bLone;
            P.Layer = bMacro ? (bLone || Choice < 0.5 + 0.42 * Interior ? ELayer::Canopy : ELayer::Secondary)
                : Choice < Mature * 0.60 ? ELayer::Canopy
                : Choice < 0.25 + Mature * 0.65 ? ELayer::Secondary : ELayer::Young;
            const FVector2D Envelope = P.Layer == ELayer::Canopy ? C.CanopyScale
                : P.Layer == ELayer::Secondary ? C.SecondaryScale : C.YoungScale;
            const double Growth = FMath::Lerp(Envelope.X, Envelope.Y, Unit(Hash(Seed,T.X,T.Y,5)));
            P.ScaleMultiplier = Growth * (bMacro ? C.HeightMultiplier : 1.0);
            P.Maturity = FMath::Clamp(Growth / C.CanopyScale.Y, 0.05, 1.0);
            P.SourceIndex = T.SourceIndex;
            P.VisualSeed = Seed;
            P.Ground = FVector(XY.X,XY.Y,Z);
            P.SlopeDegrees = Slope;
            Result.LoneTrees += bLone ? 1 : 0;
            Result.Instances.Add(P);
        }
    }
    Out = MoveTemp(Result);
    return true;
}
}
