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

/**
 * Poids de familles de surface pour UN sommet. PARTITION DE L'UNITE :
 * Rock + Litter + Worked + Herbe = 1, l'herbe etant le reste implicite.
 *
 * Pourquoi des poids et pas l'index de ETileType : un index est categoriel. Interpole
 * entre deux sommets il produit des valeurs qui ne designent aucune tuile -- entre
 * Grass(0) et Forest(4) le milieu vaut 2, c'est-a-dire Stone. Une partition, elle,
 * reste une partition apres interpolation lineaire : le triangle entre une tuile de
 * foret et une tuile d'herbe porte un melange des deux, ce qui est exactement la
 * transition que le sol doit montrer au lieu d'une frontiere de tuile.
 *
 * Quatre familles, pas sept : c'est ce que les sept ETileType portent reellement
 * comme matiere distincte. Scrub n'est pas une matiere a part, c'est de l'herbe avec
 * une part de litiere ; Ruin n'est pas une matiere a part, c'est de la pierre remaniee.
 */
struct FSurfaceMix
{
    double Rock = 0.0;    // Stone, Ruin
    double Litter = 0.0;  // Forest, et partiellement Scrub
    double Worked = 0.0;  // Field, et marginalement Ruin
};

/** Projection de ETileType en familles. Aucune donnee nouvelle : c'est une relecture du Type. */
FSurfaceMix SurfaceMixFor(AnastasisWorld::ETileType Type);

struct FGeometry
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<int32> SourceIndices;

    /** RGB = lecture semantique du sol ; A = 1 sur l'eau, 0 sur la terre. */
    TArray<FLinearColor> Colors;

    /**
     * Canaux morphologiques lus par le materiau de sol. La couleur de sommet ne peut
     * pas les porter : elle est deja une couleur, et un test scelle exige qu'elle en
     * reste une (bleue sur l'eau, jamais bleue sur la terre).
     *
     *   UV0 = (Rock, Litter)      familles, cf. FSurfaceMix
     *   UV1 = (Worked, Wetness)   famille + humidite [0,1] = proximite d'eau du simulateur
     *
     * Ce qui n'est PAS exporte, et pourquoi : la PENTE se lit dans la normale du sommet,
     * l'ALTITUDE dans la position monde, les COORDONNEES DE TUILE dans la position monde
     * aussi (un sommet est au centre de sa tuile). Les exporter serait dupliquer une
     * verite que le materiau tient deja. Shore n'est pas exporte non plus : c'est le meme
     * champ de distance a l'eau que Wetness a un rayon plus court, et il est deja peint
     * dans la couleur de sommet.
     */
    TArray<FVector2D> UV0;
    TArray<FVector2D> UV1;

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
}
