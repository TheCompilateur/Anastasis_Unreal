#include "WorldView/AnastasisTerrainSurface.h"

namespace
{
using AnastasisWorld::ETileType;

/**
 * Palette de presentation de la tranche. Distincte de la palette DEBUG des cubes.
 *
 * CES VALEURS SONT DES ALBEDOS, pas des couleurs d'interface. Elles sont consommees
 * comme Base Color d'un materiau PBR, sous un soleil de 75 000 lux et une exposition
 * figee a EV100 = 14 (le rig de capture scelle). A cette exposition une surface
 * d'albedo 0.5 sort a ~2 stops au-dessus du gris moyen : elle est blanche. C'est
 * exactement ce que montrait docs/visual/terrain-extent/C_world_surface.png -- un
 * monde de platre pastel, dont les zones les plus claires etaient precisement les
 * deux teintes les plus hautes de cette palette (ShoreSand 0.69, HighlandRock 0.52),
 * et qui sont aussi les deux appliquees le plus largement.
 *
 * Les valeurs ci-dessous sont donc ramenees dans la plage physique des sols reels :
 * herbe humide 0.10-0.18, litiere forestiere 0.05-0.10, terre travaillee 0.10-0.20,
 * roche mouillee 0.12-0.20, sediment de rive 0.18-0.28. Le monde pontique est humide,
 * donc plutot le bas de chaque plage. La teinte (le rapport entre canaux) est conservee
 * la ou elle etait juste ; seul le niveau descend.
 */
FLinearColor SurfaceTypeColor(ETileType Type)
{
    switch (Type)
    {
    case ETileType::Grass:  return FLinearColor(0.118f, 0.171f, 0.078f);
    case ETileType::Forest: return FLinearColor(0.062f, 0.097f, 0.052f);
    case ETileType::Scrub:  return FLinearColor(0.152f, 0.158f, 0.092f);
    case ETileType::Field:  return FLinearColor(0.196f, 0.163f, 0.086f);
    case ETileType::Stone:  return FLinearColor(0.149f, 0.147f, 0.141f);
    case ETileType::Ruin:   return FLinearColor(0.146f, 0.131f, 0.120f);
    // L'eau n'est pas du ressort de cette mission : valeur inchangee.
    case ETileType::Water:  return FLinearColor(0.055f, 0.220f, 0.353f);
    }
    return FLinearColor::White;
}

/**
 * Zone inondable / vase : sol sature pontique, pas une plage seche.
 *
 * Descendue de (0.204, 0.184, 0.137) a (0.082, 0.072, 0.055) par la fusion des deux
 * missions, et ce n'est pas un desaccord de gout : c'est une consequence arithmetique.
 *
 * hydrology-surface avait calibre cette teinte contre l'HERBE D'ORIGINE, de luminance
 * 0.359. A 0.185 elle assombrissait donc bien la crue. GROUND_SURFACE_001 a ramene
 * l'herbe a 0.153 pour la sortir du blanc -- et a cette luminance, la meme vase devient
 * plus CLAIRE que le sol sec : la crue eclaircissait le terrain au lieu de le noircir.
 *
 * C'est le test Anastasis.Terrain.HydrologyGradient d'hydrology-surface qui l'a
 * attrape, « la crue assombrit le sol », et il avait raison. La vase saturee est une
 * des surfaces naturelles les plus sombres (albedo reel 0.05-0.10) : 0.073 de luminance
 * la remet sous l'herbe, ou elle doit etre.
 */
const FLinearColor WetMud(0.082f, 0.072f, 0.055f);
/**
 * Rive saturee : berge humide au trait de cote.
 *
 * Deux missions ont retone cette teinte separement, sans se voir : GROUND_SURFACE_001
 * l'appelait ShoreSand et l'a descendue a (0.251, 0.216, 0.159), hydrology-surface
 * l'appelle SaturatedBank et l'a descendue a (0.247, 0.220, 0.165). Le meme diagnostic,
 * a trois millemes pres, trouve deux fois independamment : l'original a 0.694 sortait
 * blanc a l'exposition du rig. C'est le nom d'hydrology qui est garde, parce que son
 * code le reference et qu'il decrit mieux la matiere.
 */
const FLinearColor SaturatedBank(0.247f, 0.220f, 0.165f);
/**
 * Roche d'altitude : elle mineralise le sol en hauteur sans l'eclaircir en neige.
 *
 * Valeur de GROUND_SURFACE_001. hydrology-surface gardait l'original (0.518), qui est
 * l'autre moitie du halo blanc de docs/visual/terrain-extent : sous 75 000 lux et
 * EV100 = 14, un albedo de 0.5 sort deux diaphragmes au-dessus du gris moyen. Les deux
 * teintes les plus hautes de la palette etaient aussi les deux appliquees le plus
 * largement, par la rive et par l'altitude ; corriger une seule des deux laissait les
 * sommets en neige.
 */
const FLinearColor HighlandRock(0.171f, 0.163f, 0.152f);
// Eau : hors perimetre de GROUND_SURFACE_001, tenue par hydrology-surface.
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

AnastasisTerrainSurface::FSurfaceMix AnastasisTerrainSurface::SurfaceMixFor(ETileType Type)
{
    // Le reste (1 - Rock - Litter - Worked) est l'herbe. Aucune ligne ici n'invente une
    // matiere : chaque poids dit de quoi la tuile du simulateur est faite.
    switch (Type)
    {
    case ETileType::Stone:  return {1.00, 0.00, 0.00};
    // Une ruine est de la pierre remaniee posee sur un sol remue, pas une septieme matiere.
    case ETileType::Ruin:   return {0.85, 0.00, 0.10};
    case ETileType::Forest: return {0.00, 1.00, 0.00};
    // Le maquis n'est pas une matiere a part : c'est de l'herbe sous une litiere partielle.
    case ETileType::Scrub:  return {0.00, 0.35, 0.00};
    case ETileType::Field:  return {0.00, 0.00, 1.00};
    case ETileType::Grass:  return {0.00, 0.00, 0.00};
    // L'eau porte sa propre section de maillage : la famille de sol n'a pas de sens ici.
    case ETileType::Water:  return {0.00, 0.00, 0.00};
    }
    return {0.00, 0.00, 0.00};
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
        // Canaux morphologiques. Wetness sort telle quelle du simulateur -- deja bornee
        // a [0,1] par AnastasisWorld -- et on la re-borne ici seulement parce qu'un
        // canal de sommet qui deborde donne un materiau qui deborde, silencieusement.
        const FSurfaceMix Mix = SurfaceMixFor(T.Type);
        Result.UV0.Add(FVector2D(Mix.Rock, Mix.Litter));
        Result.UV1.Add(FVector2D(Mix.Worked, FMath::Clamp(T.Wetness, 0.0, 1.0)));
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
