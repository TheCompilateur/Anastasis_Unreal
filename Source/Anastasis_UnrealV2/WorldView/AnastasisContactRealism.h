#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

/**
 * AAA_CONTACT_REALISM_001 -- la peau de contact.
 *
 * Le monde a deja ses objets (arbres, rochers, roseaux, billes, souches) et ses rives
 * (AnastasisRiverbank, AnastasisMicroEcology, AnastasisTrunkContact). Ce qui leur manque, a
 * un a vingt metres de la camera, c'est la matiere qui les raccorde au sol : l'objet commence
 * a une intersection de polygones, la rive est une ligne. Cette couche ne pose pas de nouvel
 * objet de decor. Elle lit ce qui est pose (ancres) et le sol et l'eau rendus, puis planifie :
 *   - des decalques DBuffer (sol assombri, vase, depot, litiere, collerette de sediment),
 *   - quelques cailloux au pied des rochers et sur les rives de pierre.
 *
 * Build est pur et deterministe : pas d'UObject, pas de lecture du monde, tout par FInputs.
 * Le vide est voulu : chaque famille a une probabilite < 1 et un bruit de plaque, une rive
 * n'est jamais un anneau uniforme.
 */
namespace AnastasisContactRealism
{
/** Les cinq etats de rive. None = hors bande (la prairie touche, rien a ajouter). */
enum class EShore : uint8
{
	None = 0,
	/** Rive seche : lit de crue, ligne de depot pale, pas de vase. */
	Dry,
	/** Bande humide etroite : sol sombre qui s'eclaircit vers l'interieur. */
	Damp,
	/** Vase : eau calme, replat, sol noir et luisant. */
	Muddy,
	/** Roselière : sol organique sombre sous les roseaux reels. */
	Reed,
	/** Rive de pierre : eau vive ou talus, mineral gris mouille. */
	Stony,
	Count
};

/** Une famille de decalque = un materiau (MI_ACR_<nom>). */
enum class EDecal : uint8
{
	WetBand,
	Mud,
	StoneWet,
	ReedBed,
	Litter,
	ContactDark,
	RockDirt,
	Deposit,
	/** Creux de prairie, plus humide. */
	Depression,
	Streak,
	/** Halo humide d'une rive : large, discret, centre sur la nappe. */
	Halo,
	Count
};

/** Ce que la couche lit dans l'incarnation : un objet pose, pas un objet cree. */
enum class EAnchor : uint8
{
	Tree,
	Sapling,
	Stump,
	/** Billes, driftwood, tas de branches : objets couches, orientes par leur lacet. */
	Lying,
	Roots,
	Rock,
	Bush,
	Reed,
	Count
};

inline constexpr int32 ShoreCount = static_cast<int32>(EShore::Count);
inline constexpr int32 DecalCount = static_cast<int32>(EDecal::Count);
inline constexpr int32 AnchorCount = static_cast<int32>(EAnchor::Count);

const TCHAR* ShoreName(EShore Shore);
const TCHAR* DecalName(EDecal Decal);
const TCHAR* AnchorName(EAnchor Anchor);

struct FAnchor
{
	/** Pied de l'objet (uu), monde. */
	FVector Location = FVector::ZeroVector;
	/** Demi-emprise horizontale (uu) et hauteur (uu). */
	double Radius = 0.0;
	double Height = 0.0;
	double YawDegrees = 0.0;
	EAnchor Kind = EAnchor::Tree;
};

struct FSettings
{
	/** Pas de la grille de sol, d'eau et de distance a l'eau (uu). */
	double GridUU = 150.0;
	/** Pas des candidats de rive (uu) : un candidat par cellule, jamais un tapis. */
	double ShoreCellUU = 280.0;
	/** Largeur maximale de la bande etudiee autour de l'eau (uu). */
	double ShoreBandUU = 1800.0;
	/** Pentes au-dela desquelles un decalque se deforme : rien n'est pose (dZ/dXY). */
	double MaxDecalSlope = 0.75;
	/** Pas des candidats de prairie : depressions, depots et rigoles (uu). */
	double MeadowCellUU = 900.0;
	/** Part des arbres adultes qui portent un raccord de pied. */
	double TreeShare = 0.78;
	double RockShare = 1.0;
	/** Une plaque de roseaux = au moins ce nombre de tiges dans la cellule. */
	int32 ReedMinStems = 2;
	double ReedCellUU = 240.0;
	/** Taille d'un caillou (diametre, uu). */
	double PebbleMinUU = 5.0;
	double PebbleMaxUU = 15.0;
	/** Demi-profondeur de la boite de projection (uu) : doit egaler DEPTH de aaa-contact-realism.py. */
	double DecalDepthUU = 220.0;
	int32 MaxDecals = 3200;
	int32 MaxPebbles = 6000;
};

struct FInputs
{
	/** Sol et eau rendus. Faux = hors du sol rendu (hauteur) ; faux pour l'eau = pas d'eau. */
	TFunction<bool(double X, double Y, double& Z)> SampleHeight;
	TFunction<bool(double X, double Y, double& Z)> SampleWaterHeight;
	/** [0,1] : 1 eau calme (lac, plaine), 0 eau vive. Absent = tout est calme. */
	TFunction<double(double X, double Y)> SampleCalm;
	FBox2D Bounds = FBox2D(ForceInit);
	int32 Seed = 0;
	TArray<FAnchor> Anchors;
};

struct FDecal
{
	/** Centre sur le sol (uu). */
	FVector Location = FVector::ZeroVector;
	double YawDegrees = 0.0;
	/** Demi-longueur le long du lacet, demi-largeur, demi-profondeur de projection (uu). */
	double HalfLong = 100.0;
	double HalfWide = 100.0;
	double HalfDepth = 90.0;
	EDecal Kind = EDecal::WetBand;
	EShore Shore = EShore::None;
	/** Direction qui s'eloigne de l'eau (degres) pour une rive ; sans objet si bAway est faux. */
	double AwayDegrees = 0.0;
	bool bAway = false;
	/** EAnchor de l'objet qui a motive ce decalque, -1 pour une rive ou la prairie. */
	int32 Source = -1;
};

struct FPebble
{
	/** Pied du caillou (uu), sur le sol. */
	FVector Location = FVector::ZeroVector;
	double YawDegrees = 0.0;
	/** Diametre vise (uu) ; l'incarnation le convertit avec les bornes du maillage. */
	double Diameter = 8.0;
	/** Part de la hauteur enfoncee dans le sol : un caillou sort du sol, il n'y est pas pose. */
	double Sink = 0.4;
	int32 Variant = 0;
};

struct FPlan
{
	TArray<FDecal> Decals;
	TArray<FPebble> Pebbles;
	int32 DecalCounts[DecalCount] = {};
	/** Candidats examines par etat, et nombre de ceux qui ont recu un decalque. */
	int32 ShoreExamined[ShoreCount] = {};
	int32 ShoreDecaled[ShoreCount] = {};
	int32 AnchorCounts[AnchorCount] = {};
	int32 WetCells = 0;
	int32 BandCells = 0;
	bool bTruncated = false;
	double MilliSeconds = 0.0;
};

/**
 * Etat d'une cellule de rive. Pur, sert aussi aux tests.
 *   DistUU      distance a l'eau (uu), 0 = au trait de cote
 *   Freeboard   hauteur du sol au-dessus de la nappe (uu)
 *   Slope       dZ/dXY
 *   Calm        [0,1] 1 = eau calme
 *   Patch       bruit de plaque [0,1], ~25 m : change l'etat le long de la rive
 *   ReedStems   tiges de roseau reelles autour
 *   BandUU      largeur locale de la bande humide
 */
EShore ClassifyShore(double DistUU, double Freeboard, double Slope, double Calm, double Patch, int32 ReedStems, double BandUU);

bool Build(const FInputs& In, const FSettings& S, FPlan& Out, FString& Error);
}
