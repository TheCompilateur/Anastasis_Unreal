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
 * MESUREE, PAS CHOISIE. Une premiere version valait 120 uu, par raisonnement :
 * le pas de tuile fait 100 uu et AltitudeScale vaut 1000, donc 120 uu est la
 * marche qu'une berge de pente 1:1 franchit en une tuile. Le raisonnement etait
 * juste et la valeur etait fausse. Le marqueur TERRAIN_SHORELINE du test
 * Anastasis.Terrain.Shoreline a mesure le monde canonique : sa fosse la plus
 * profonde fait 91 uu. Avec un span de 120, AUCUN sommet du monde n'atteignait
 * jamais l'eau franche -- tout le monde etait marge, donc plus rien n'etait une
 * marge. Le test a echoue exactement la-dessus ("l'eau franche existe aussi").
 *
 * 60 uu, soit les deux tiers de la profondeur maximale relevee : la nappe atteint
 * son etat plein bien avant le point le plus creux du monde, donc "eau franche"
 * est un etat reellement occupe et pas une limite jamais atteinte. La marge,
 * elle, occupe 0.16 a 0.55 de ce span selon la platitude de la berge, soit les
 * 10 a 33 premiers uu d'eau.
 *
 * Changer cette valeur sans relire TERRAIN_SHORELINE_DEPTHS, c'est refaire
 * l'erreur de 120.
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
}
