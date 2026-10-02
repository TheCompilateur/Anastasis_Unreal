#pragma once

#include "CoreMinimal.h"

/**
 * CONTINENTAL_001 -- la structure tectonique du continent autour de la carte.
 *
 * Jusqu'ici l'anneau d'horizon n'avait pour tout continent qu'un bruit de collines et une
 * chaine de 100 a 300 m : au-dela de 1,9 km, un plateau vert sans ossature, des cretes
 * qui ne descendent d'aucune faille. Ce module donne a ce lointain une CAUSE.
 *
 * C'est de la PRESENTATION, pas de la simulation : fonction pure de (x, y, graine, direction
 * de l'exutoire), sans etat, sans lecture de AnastasisWorld. Meme graine, meme continent.
 *
 * Le recit geologique (inspire de la marge pontique -- Alpes pontiques, faille nord-
 * anatolienne -- sans pretendre la restituer) :
 *
 *   - la carte est un BASSIN INTRAMONTAGNEUX. L'eau y sort d'un cote (l'exutoire) : c'est
 *     le cote bas, les basses terres. Tout l'espace se range le long de l'axe OPPOSE a
 *     cette sortie, que ce module appelle "interieur" (u croissant) ;
 *   - une CHAINE PRINCIPALE (orogene) a 10-17 km de l'interieur, dissymetrique : front
 *     raide tourne vers la carte, revers long vers le plateau ;
 *   - entre la carte et la chaine, une CEINTURE DE PLIS ET DE CHEVAUCHEMENTS : des
 *     crêtes paralleles (hogbacks, cuestas) a front raide et revers doux, coupees de
 *     seuils ou passent les rivieres ;
 *   - une FAILLE DECROCHANTE rectiligne (tranchee et crêtes de blocage) qui recoupe cette
 *     ceinture a 8 km ;
 *   - un ESCARPEMENT de faille normale a 7 km cote basses terres : la carte est une terrasse ;
 *   - un PLATEAU CONTINENTAL qui monte vers l'interieur, que borde une SECONDE chaine, a
 *     40 km, plus haute, enneigee ;
 *   - une chaine EXTERIEURE basse cote basses terres, et une cuvette generale, pour que
 *     l'horizon se ferme dans toutes les directions.
 *
 * Unites : kilometres et metres a l'echelle de reference (anastasis.WorldView.Scale 5).
 * L'appelant convertit pour une autre echelle (angles conserves).
 */
namespace AnastasisTectonics
{
/** Echelle spatiale a laquelle les kilometres et metres de ce module sont exprimes. */
inline constexpr double ReferenceScale = 5.0;

struct FTectonicFrame
{
	/** Direction monde (X, Y), unitaire, de l'interieur vers les basses terres : sens de l'eau. */
	FVector2D Down = FVector2D(-1.0, 0.0);
	uint32 Seed = 12345;

	/** Axe de la chaine (perpendiculaire a Down). */
	FVector2D Strike() const { return FVector2D(-Down.Y, Down.X); }
};

/**
 * Cadre d'un monde. Outlet = somme, sur le bord de la carte, des normales sortantes pondérées
 * par l'eau qui y sort : l'eau sort la ou le continent descend. Sans eau au bord (ou somme
 * nulle), la direction vient de la graine.
 */
FTectonicFrame MakeFrame(const FVector2D& Outlet, uint32 Seed);

/** Contributions separees, en metres au-dessus du plan de reference (hors collines locales). */
struct FBreakdown
{
	double Ramp = 0.0;      // piedmont et plateau continental
	double Folds = 0.0;     // ceinture de plis et de chevauchements
	double Scarp = 0.0;     // escarpement de faille normale (la terrasse de la carte)
	double Fault = 0.0;     // tranchee decrochante (negatif) et crêtes de blocage
	double Range = 0.0;     // chaine principale
	double Backdrop = 0.0;  // seconde chaine, au fond du plateau
	double Outer = 0.0;     // chaine exterieure cote basses terres
	double Bowl = 0.0;      // cuvette generale
	double Total = 0.0;     // somme, apres saturation douce a MaxHeightM
	/** Coordonnees tectoniques (km) : u vers l'interieur, v le long de la chaine. */
	double U = 0.0;
	double V = 0.0;
};

/** Hauteur au-dessus du plan de reference, en metres ; saturation douce a MaxHeightM. */
inline constexpr double MaxHeightM = 4600.0;

void Evaluate(const FTectonicFrame& Frame, double XKm, double YKm, FBreakdown& Out);
double HeightM(const FTectonicFrame& Frame, double XKm, double YKm);

/**
 * Familles de surface d'un sommet lointain, poids dans [0,1] (pas une partition : l'appelant les
 * applique dans l'ordre foret, alpage, roche, neige, chacune recouvrant la precedente).
 * Entrees : altitude au-dessus du plan de reference (m), pente de la surface rendue (degres),
 * position (km, pour le bruit de lisiere : la limite des neiges et des arbres ne suit pas une
 * isohypse).
 */
struct FSurface
{
	double Forest = 0.0;
	double Alpine = 0.0;
	double Rock = 0.0;
	double Snow = 0.0;
	/** Variation de valeur [0.86, 1.14], a multiplier a la teinte : une montagne n'est pas un aplat de plâtre. */
	double Tone = 1.0;
};
FSurface SurfaceAt(const FTectonicFrame& Frame, double XKm, double YKm, double HeightM, double SlopeDeg);

/** Altitude moyenne de la limite des neiges (m). */
inline constexpr double SnowLineM = 3150.0;
/**
 * Sous cette altitude (m) la surface reste celle de la vallee habitee -- les prairies de la
 * carte, donc la palette de l'anneau ; au-dessus, sur 450 m, commence la foret montagnarde.
 */
inline constexpr double TreeLineLowM = 300.0;
/** Limite superieure de la foret (m). */
inline constexpr double TreeLineHighM = 1950.0;
}
