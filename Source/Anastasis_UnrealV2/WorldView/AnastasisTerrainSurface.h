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

// ---------------------------------------------------------------------------
// SHORELINE_FORGE_001 -- grammaire de rencontre eau / terre.
//
// La nappe d'eau est PLATE au niveau de la mer et le relief est SCELLE : aucune
// de ces deux geometries ne bouge ici, et le trait de cote reste exactement
// l'intersection que Build produisait deja. Ce qui manquait n'etait pas une
// forme, c'etait une LECTURE : la nappe etait peinte d'une seule couleur opaque,
// donc la seule chose que le bord d'eau pouvait montrer etait sa propre arete.
//
// Ces trois canaux rendent a la nappe ce que le relief sait deja d'elle.
// ---------------------------------------------------------------------------

/**
 * Profondeur (UU) au-dela de laquelle l'eau est franche : au-dessous se joue
 * toute la marge de rive ; au-dessus, la nappe est pleine et uniforme.
 *
 * MESUREE, PAS CHOISIE, ET RE-MESUREE DEPUIS TERRAIN_FORGE.
 *
 * Premiere version : 120 uu, par raisonnement -- le pas de tuile fait 100 uu et
 * AltitudeScale vaut 1000, donc 120 uu est la marche qu'une berge de pente 1:1
 * franchit en une tuile. Le raisonnement etait juste et la valeur etait fausse :
 * la surface tuilee ne creusait qu'a 91 uu, donc AUCUN sommet n'atteignait jamais
 * l'eau franche. Le test a echoue exactement la-dessus.
 *
 * Depuis, TERRAIN_FORGE tessele le relief et exagere le fond immerge (DepthExag
 * 1.7) : sur le maillage REELLEMENT rendu, l'eau descend a 614 uu. Une
 * justification en "deux tiers du maximum" n'a donc plus aucun sens -- elle
 * donnerait 400 uu et noierait toute la rive dans une seule teinte.
 *
 * 60 uu est cale sur la DISTRIBUTION, et sur son debut : c'est la ou est la rive.
 * Deciles de profondeur du maillage forge (TERRAIN_SHORELINE_FORGED_DEPTHS) :
 *
 *     p10=11.1  p20=21.7  p30=30.6  p40=35.5  p50=41.2
 *     p60=47.0  p70=54.0  p80=62.5  p90=78.6        max=614
 *
 * A 60 uu, la marge couvre les sept premiers deciles de l'eau du monde et le
 * dernier quart reste de l'eau franche -- un etat reellement occupe (3641 sommets
 * sur 16234 immerges), pas une limite jamais atteinte. La bande de limon, elle,
 * occupe 0.10 a 0.32 de ce span selon la platitude, soit 6 a 19 uu : elle se
 * ferme avant le p20. C'est un bord d'eau, pas un lac brun.
 *
 * Les deux marqueurs sortent a chaque execution. Changer cette constante sans les
 * relire, c'est refaire l'erreur de 120.
 */
inline constexpr double ShoreDepthSpan = 60.0;

/**
 * SHORELINE_GENOME -- un sommet de nappe d'eau, trois nombres, tous derives.
 * Aucune simulation nouvelle : la profondeur vient du relief deja bati et de
 * SeaLevel, la platitude de la normale deja calculee, le courant du champ
 * FlowAmt que le simulateur hydrologique pose sur les tuiles d'eau.
 */
struct FShorelineVertex
{
    /**
     * (WaterPlaneZ - Z_relief) / ShoreDepthSpan, borne [0,1].
     * 0 = le relief affleure ou emerge : c'est la ligne de rive.
     * 1 = eau franche.
     * C'est la FORME de la berge, lue par en dessous.
     */
    double Depth = 0.0;

    /**
     * Normale Z du relief au meme sommet, borne [0,1]. 1 = fond plat, 0 = paroi.
     * Une berge plate merite une marge etalee et laiteuse, une berge abrupte une
     * ligne nette : la largeur de la bande n'est donc pas une constante, elle
     * sort de la topographie. C'est ce qui fait varier la rive d'un point a
     * l'autre du monde sans qu'aucune variante ne soit ecrite a la main.
     */
    double Flatness = 1.0;

    /**
     * FlowAmt de la tuile source, borne [0,1]. Nul sur une eau dormante, non nul
     * seulement sur les tuiles que l'hydrologie a reconnues comme chenal
     * (WaterFlowAmtGate = 0.06). Une rive de courant se lave et se cailloute,
     * une rive de lac se colmate : deux familles de rive, une seule donnee, et
     * c'est le simulateur qui decide laquelle s'applique ou.
     */
    double Flow = 0.0;
};

/** Profondeur normalisee d'un sommet de nappe. Reproductible, bornee, finie. */
inline double ShorelineDepthAt(double TerrainZ)
{
    return FMath::Clamp((WaterPlaneZ - TerrainZ) / ShoreDepthSpan, 0.0, 1.0);
}
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
     * Canaux de rive, un par sommet, dans le meme ordre que WaterVertices.
     * Portes par la nappe d'eau (section 1), jamais par le relief (section 0) :
     * le sol appartient a GROUND_SURFACE_001 et n'est pas touche ici.
     *
     *   WaterUV0 = (Depth, Flatness)
     *   WaterUV1 = (Flow, 0)
     *
     * Pourquoi des UV et pas la couleur de sommet : un test scelle exige que la
     * couleur reste une couleur (alpha 1 sur l'eau, 0 sur la terre). Un canal
     * de morphologie n'y a pas sa place.
     */
    TArray<FVector2D> WaterUV0;
    TArray<FVector2D> WaterUV1;

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
 * Remplit WaterUV0 / WaterUV1 d'une geometrie DEJA BATIE, quelle que soit sa
 * resolution. Build l'appelle a la fin ; TERRAIN_FORGE doit l'appeler apres avoir
 * remplace la geometrie.
 *
 * POURQUOI CETTE FONCTION EXISTE. AnastasisTerrainForge::Apply remplace la
 * geometrie entiere -- il rebatit la nappe d'eau a la resolution fine et n'a
 * aucune raison de connaitre les canaux de rive. Sans ce point d'entree, les
 * canaux repartaient a zero des que la forge etait active (son defaut), la nappe
 * lisait Depth=0 partout, donc opacite nulle : l'eau disparaissait. Les canaux
 * sont un contrat de FGeometry, c'est donc ici qu'ils se remplissent, pas chez
 * celui qui produit le maillage.
 *
 * Ne lit que la geometrie et le snapshot : hauteur du relief, normale, et
 * FlowAmt de la tuile source sous chaque sommet. Aucune donnee nouvelle.
 */
void FillShorelineChannels(const AnastasisWorldView::FWorldVisualSnapshot& Crop, FGeometry& InOut);

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
