#pragma once

#include "WorldView/AnastasisTerrainForge.h"

class UWorld;

/**
 * HYDRO_NETWORK_001 -- reseau de drainage de presentation.
 *
 * Pourquoi cette couche existe. L'hydrologie de simulation (AnastasisHydrology) creuse
 * chaque cellule de chenal SOUS SeaLevel, quelle que soit son altitude naturelle : toute
 * l'eau du monde est un seul plan, et une riviere de montagne est une tranchee au niveau
 * de la mer. Mesure seed 12345 : 944 des 1 192 tuiles d'eau etaient de la terre avant
 * l'hydrologie ; 27 plans d'eau disjoints ; chenaux rectilignes sur le fond rempli du
 * bassin interieur. La simulation est un contrat de parite JS : on ne la touche pas.
 *
 * Ce que fait la couche, sur le maillage REELLEMENT rendu (apres TERRAIN_FORGE et
 * Human_Geography_V2), en coordonnees de sommets fins :
 *   1. repare les tranchees de chenal (remplissage harmonique depuis les berges) ;
 *      lacs et eaux de bord de monde sont gardes, a leur niveau ;
 *   2. route l'ecoulement (priority-flood + D8) ; les rivieres ecrites a la main de
 *      Human_Geography_V2 sont "brulees" dans la surface de routage et gardent leur
 *      profil d'eau : le reseau calcule s'y raccorde au lieu de les remplacer ;
 *   3. extrait un reseau CLAIRSEME (seuil d'aire drainee adaptatif, affluents courts
 *      elagues), l'ordonne (Strahler), en fait des polylignes lissees qui meandrent
 *      davantage en plaine ;
 *   4. donne a chaque point une surface d'eau qui ne remonte jamais vers l'aval, une
 *      largeur et une profondeur qui croissent avec l'aire drainee, une vitesse issue de
 *      la pente (Manning) -- l'equivalent des Width / Depth / Velocity par point de
 *      spline d'un WaterBodyRiver ;
 *   5. creuse lit et berges : vallee large et berge douce en plaine, chenal etroit et
 *      incise en colline ; au plus quelques plaines d'inondation avec mares laterales.
 *
 * Ne modifie ni Alt, ni Type, ni FlowAmt : les tuiles d'eau de simulation restent la
 * verite de gameplay. anastasis.Terrain.Drainage 0 rend la geometrie d'avant, a l'octet.
 */
namespace AnastasisDrainage
{
enum class EMouth : uint8
{
	/** Se jette dans une autre riviere (confluence). */
	River,
	/** Se jette dans un lac ou une eau de bord de monde. */
	Lake,
	/** Sort du monde par un bord (exutoire). */
	Border,
};

struct FRiverPoint
{
	/** XY monde, Z = surface d'eau (uu). */
	FVector Location = FVector::ZeroVector;
	double Width = 0.0;      // uu, largeur mouillee
	double Depth = 0.0;      // uu, sous la surface au centre
	double Velocity = 0.0;   // m/s
	double AreaM2 = 0.0;     // aire drainee
	double Slope = 0.0;      // pente de la surface d'eau (sans dimension)
	double BankFalloff = 0.0; // uu, largeur de berge au-dela du bord mouille
	double Hill = 0.0;       // 0 plaine .. 1 versant raide (pente du relief autour du lit)
	int32 Order = 1;         // Strahler
};

struct FRiver
{
	TArray<FRiverPoint> Points; // de la source vers l'embouchure
	int32 Order = 1;
	EMouth Mouth = EMouth::Border;
	/** Riviere receptrice (Mouth == River), sinon INDEX_NONE. */
	int32 Parent = INDEX_NONE;
	/** Lac recepteur (Mouth == Lake), sinon INDEX_NONE. */
	int32 MouthLake = INDEX_NONE;
	/** Lac dont cette riviere est l'exutoire, sinon INDEX_NONE (source = tete de bassin). */
	int32 SourceLake = INDEX_NONE;
	/** Suit une riviere ecrite par Human_Geography_V2. */
	bool bAuthored = false;
	double LengthM = 0.0;
};

struct FLake
{
	double SurfaceZ = 0.0;
	int32 Cells = 0;
	FVector Centroid = FVector::ZeroVector;
	/** Touche le bord du monde : mer / exutoire terminal. */
	bool bBorder = false;
	int32 Inflows = 0;
	int32 Outflows = 0;
	/** Quelques sommets du lac (XY monde), pour les controles. */
	TArray<FVector2D> Anchors;
};

struct FWetland
{
	FVector Center = FVector::ZeroVector;
	int32 River = INDEX_NONE;
	int32 Ponds = 0;
	double LengthM = 0.0;
};

struct FNetwork
{
	TArray<FRiver> Rivers;
	TArray<FLake> Lakes;
	TArray<FWetland> Wetlands;
	int32 Heads = 0;
	int32 Confluences = 0;
	/** Humidite riveraine [0,1] par sommet fin (berges, plaines, mares), pour la vegetation. */
	TArray<float> Riparian;
	int32 GridW = 0, GridH = 0;
	double GridX0 = 0.0, GridY0 = 0.0, GridStep = 0.0;
	/** Seuil d'initiation retenu (aire drainee, m2). */
	double ChannelAreaM2 = 0.0;
	/** Sommets immerges avant, hors lacs, que la reparation a rendus a la terre. */
	int32 RepairedWaterVertices = 0;
	/** Sommets immerges avant qui sont encore sous l'eau apres (reseau ou lac). */
	int32 KeptWaterVertices = 0;
	double MilliSeconds = 0.0;
};

struct FParams
{
	/** Aire drainee d'initiation d'un chenal (m2), relevee jusqu'a MaxHeads. */
	double ChannelAreaM2 = 40000.0;
	int32 MaxHeads = 16;
	/** Affluents de premier ordre plus courts : elagues. */
	double MinTributaryM = 220.0;
	double MinWidthM = 6.0;
	double MaxWidthM = 42.0;
	double MinDepthM = 0.6;
	double MaxDepthM = 3.0;
	/** Lac : aire minimale (m2) et rayon inscrit minimal (m). */
	double LakeMinAreaM2 = 15000.0;
	double LakeMinRadiusM = 40.0;
	/** Plaines d'inondation retenues au plus. */
	int32 MaxWetlands = 2;
	/** Points (XY monde) dont les plaines d'inondation se tiennent a l'ecart : village, point haut. */
	TArray<FVector2D> Protected;
	double ProtectedRadiusM = 220.0;
};

/** anastasis.Terrain.Drainage != 0. */
bool IsEnabled();

/**
 * Remplace la geometrie de InOut par le relief draine, et remplit Out.
 * False = rien n'est modifie (grille invalide).
 */
bool Apply(
	const AnastasisWorldView::FWorldVisualSnapshot& Crop,
	AnastasisTerrainForge::FMesh& InOut,
	FNetwork& Out,
	const FParams& Params = FParams());

/**
 * Controles du reseau sur la geometrie rendue. Tous a zero = reseau coherent.
 */
struct FCheck
{
	int32 UphillSteps = 0;        // surface d'eau qui remonte vers l'aval (> 1 uu)
	int32 NarrowingSteps = 0;     // largeur qui diminue vers l'aval (> 1 uu)
	int32 ConfluenceNarrower = 0; // aval d'une confluence moins large qu'un de ses bras
	int32 DanglingMouths = 0;     // embouchure qui ne touche ni riviere, ni lac, ni bord
	int32 IsolatedWaterBodies = 0; // plan d'eau rendu qui ne touche ni riviere ni lac retenu
	int32 LakesWithoutRole = 0;   // lac interieur sans affluent ni exutoire
	double BankContainment = 1.0; // part des points dont les deux berges dominent l'eau
};
FCheck Check(const FNetwork& Network, const AnastasisTerrainForge::FMesh& Mesh);

FString Describe(const FNetwork& Network);

/** anastasis.Drainage.Dump non vide : ecrit le reseau en JSON a ce chemin. Preuve, pas un format stable. */
bool DumpIfRequested(const FNetwork& Network);

/** Humidite riveraine du reseau actif en (X, Y) monde ; false hors grille ou sans reseau. */
bool RiparianAt(double X, double Y, double& Out);

/** Dernier reseau applique a l'incarnation, pour le dessin de debug. */
void SetActive(const FNetwork& Network);
const FNetwork& GetActive();

/**
 * anastasis.Drainage.Debug : 0 rien, 1 largeur, 2 profondeur, 3 vitesse, 4 ordre.
 * Lignes persistantes au-dessus de chaque riviere ; equivalent des visualisations
 * River Width / Depth / Velocity du Water plugin.
 */
void DrawDebug(UWorld* World, const FNetwork& Network, int32 Mode);
int32 DebugMode();
}
