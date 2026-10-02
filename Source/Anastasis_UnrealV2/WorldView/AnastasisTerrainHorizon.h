#pragma once

#include "WorldView/AnastasisTectonics.h"
#include "WorldView/AnastasisTerrainForge.h"

/**
 * HORIZON_RING_001 -- terrain lointain autour de la carte.
 *
 * Le maillage forge s'arrete net au bord des 96 tuiles. Au-dela, rien : sous l'horizon,
 * la camera voit le sol de planete par defaut du SkyAtmosphere, un disque presque noir.
 * C'est le "vide noir" des captures.
 *
 * L'anneau est un maillage de PRESENTATION, pas de simulation. Il ne lit ni n'ecrit
 * AnastasisWorld ; il ne lit que le maillage deja forge, dont il reprend le bord a
 * l'identique (memes positions, memes normales) : aucune couture n'est possible.
 * Puis il s'eloigne par anneaux carres de plus en plus espaces, redescend en collines
 * et se releve en montagnes lointaines assez hautes pour fermer l'horizon depuis
 * n'importe quel point de la carte.
 *
 * Tout est exprime en tuiles et en unites d'altitude, multiplies par SpatialScale :
 * anastasis.WorldView.Scale agrandit l'anneau avec la carte, angles conserves.
 */
namespace AnastasisTerrainHorizon
{
/**
 * Anneaux bornes a ce nombre de tuiles au-dela du bord (60 km a l'echelle 5).
 * CONTINENTAL_001 : 20 km fermaient l'horizon sur un plateau ; 60 km portent la seconde
 * chaine (AnastasisTectonics) derriere la premiere.
 */
inline constexpr double OuterTiles = 3000.0;
/** Raison de la suite geometrique des anneaux : le premier a le pas fin de la forge. */
inline constexpr double RingGrowth = 1.15;
/**
 * CONTINENTAL_001 : le pas radial ne depasse jamais cette fraction de la distance au centre de
 * la carte, soit une resolution ANGULAIRE constante (0,86 degre vu du centre). Sans ce plafond
 * les anneaux lointains avaient des cellules de 1,5 x 3 km a 10 km : une montagne n'y pouvait
 * pas etre autre chose qu'un plateau.
 */
inline constexpr double FarAngularStep = 0.015;
/**
 * CONTINENTAL_001 : talus d'eboulis. Le relief de AnastasisTectonics est une fonction ; le
 * maillage qui le porte ne peut pas avoir de falaise de 75 degres entre deux sommets de 150 m. Apres
 * la pose des hauteurs, des passes de relaxation (erosion thermique par triangle) ramenent chaque triangle a la pente
 * limite : TalusNearDeg jusqu'a TalusNearKm du bord de la carte, puis TalusFarDeg a partir de
 * TalusFarKm (kilometres a l'echelle de reference 5). Les anneaux de la riviere sortante
 * (RiverEndTiles) ne bougent pas : leur lit doit rester sous la nappe.
 */
inline constexpr double TalusNearDeg = 42.0;
inline constexpr double TalusFarDeg = 58.0;
inline constexpr double TalusNearKm = 4.0;
inline constexpr double TalusFarKm = 8.0;
inline constexpr int32 RelaxIterations = 96;
/**
 * Au-dela de cette distance du bord (km a l'echelle 5), la surface est celle de la montagne lointaine
 * (M_AnastasisFarTerrain : la couleur de sommet EST l'albedo) et non celle du sol de la carte, dont le
 * materiau applique ses propres couleurs de famille et ne laisse lire ni foret, ni roche, ni neige.
 */
inline constexpr double FarMaterialKm = 8.0;
/** Distance (tuiles) sur laquelle le bord forge se fond dans le relief lointain. */
inline constexpr double BlendTiles = 30.0;
/**
 * L'eau qui touche le bord (rivière sortante) continue dans l'anneau, puis son lit
 * remonte entre ces deux distances (tuiles) et elle s'achève en rive naturelle.
 */
inline constexpr double RiverStartTiles = 40.0;
inline constexpr double RiverEndTiles = 90.0;
/** Lissage tangentiel maximal du profil de bord, en colonnes de chaque côté (480 m). */
inline constexpr int32 MaxSmoothColumns = 96;
/** Au plus 2^N fois moins de colonnes qu'au bord sur les anneaux lointains. */
inline constexpr int32 MaxDecimation = 4;
/** Distance (tuiles) sur laquelle les anneaux passent du decalage perpendiculaire a l'homothetie. */
inline constexpr double PerpendicularTiles = 10.0;
/**
 * La fenetre de lissage gagne une colonne tous les N pas fins de distance au bord.
 * 2 : quand la fenetre s'elargit d'une colonne, le haut et le pied d'une berge du bord
 * bougent d'au plus un tiers de sa hauteur sur deux pas fins -- la pente radiale reste
 * sous la pente tangentielle qu'elle efface.
 */
inline constexpr double SmoothFineStepsPerColumn = 2.0;
/**
 * Pres du raccord, l'anneau ne peut pas etre moins raide que le bord forge qu'il
 * prolonge : une surface qui contient un segment de bord a 48 degres a au moins
 * 48 degres. Et lisser une berge en rampe reguliere n'en change pas la pente tant que la
 * fenetre est plus courte que la rampe. Une berge raide du bord (celle de la rivière
 * Human_Geography_V2 qui sort de la carte) se prolonge donc sur cette distance (en pas
 * fins) ; au-dela, plus rien ne depasse 45 degres.
 */
inline constexpr double SeamFineSteps = 12.0;

struct FRing
{
	AnastasisTerrainSurface::FGeometry Geometry;
	/** Sommets de l'anneau 0 : le pourtour fin du maillage forge, 2(FineW-1)+2(FineH-1). */
	int32 Perimeter = 0;
	/** Nombre d'anneaux, bord forge compris (anneau 0). */
	int32 Rings = 0;
	/** Distance au bord (uu) de chaque anneau ; Distances[0] = 0. */
	TArray<double> Distances;
	/** Palier de chaque anneau : Perimeter >> RingLevel[K] colonnes. */
	TArray<int32> RingLevel;
	/** Premier sommet de chaque anneau dans Geometry.Vertices. */
	TArray<int32> RingStart;
	/** Colonne de bord (indice dans le pourtour) dont chaque sommet est issu. */
	TArray<int32> VertexColumn;
	/** Anneau d'un sommet. Le premier sommet d'un triangle est toujours sur son anneau interieur. */
	int32 RingOf(int32 Vertex) const;
	double MinZ = 0.0;
	double MaxZ = 0.0;
	/** Sommets du bord forge sous leur nappe : l'eau qui sort de la carte. */
	int32 EdgeWater = 0;
	/** Sommets de prairie de la carte dont l'anneau tire ses teintes lointaines. */
	int32 PaletteDonors = 0;
	/** Prairie verte (moitie sombre) et prairie seche (moitie claire) des donneurs. */
	FLinearColor LushColor = FLinearColor::Black;
	FLinearColor DryColor = FLinearColor::Black;
	/** CONTINENTAL_001 : cadre tectonique (sens de l'eau, graine) et ce qu'il a produit. */
	AnastasisTectonics::FTectonicFrame Tectonics;
	/** Altitude maximale de l'anneau au-dessus de la nappe, en metres a l'echelle de reference. */
	double MaxHeightM = 0.0;
	/** Sommets d'anneau classes neige / roche / foret d'altitude (poids > 0,5). */
	int32 SnowVertices = 0;
	int32 RockVertices = 0;
	int32 ForestVertices = 0;
	/** Premier triangle (indice dans Geometry.Triangles / 3) des anneaux au-dela de FarMaterialKm. */
	int32 FarTriangleStart = 0;
};

/**
 * Pourtour fin du maillage forge, dans l'ordre de l'anneau : bas (Y=0) en X croissant,
 * droite, haut en X decroissant, gauche. Indices dans Forge.Geometry.Vertices.
 */
TArray<int32> PerimeterIndices(int32 FineW, int32 FineH);

/**
 * Batit l'anneau autour de Forge. False si Forge n'est pas un maillage fin complet.
 * Anneau 0 = copie exacte du bord forge (positions, normales, couleur, UV).
 *
 * Nappe d'eau : Out.Geometry.Water* couvre chaque triangle dont un sommet passe sous sa
 * nappe -- celle du bord forge prolongee (rivière sortante), la mer ailleurs. Rendered (optionnel) est la geometrie rendue
 * APRES FillShorelineChannels : l'anneau 0 en reprend alors les canaux de rive, pour que
 * la nappe ne change pas de profondeur ni de courant a la couture.
 */
bool Build(const AnastasisTerrainForge::FMesh& Forge, uint32 Seed, FRing& Out,
	const AnastasisTerrainSurface::FGeometry* Rendered = nullptr);
}
