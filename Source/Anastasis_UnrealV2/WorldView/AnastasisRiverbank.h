#pragma once

#include "CoreMinimal.h"
#include "WorldView/AnastasisDrainage.h"

/**
 * RIVERBANK_LIFE_001 -- rives vivantes.
 *
 * Apres WATER_LOOK_001 l'eau se lit comme de l'eau, mais la prairie la touche directement :
 * aucune bande entre l'herbe et la ligne d'eau. Une vraie rive en a, et elles disent la
 * vitesse du courant :
 *   eau calme (plaine, lacs, mares)  vase sombre et luisante au trait de cote, massifs de
 *                                    roseaux les pieds dans l'eau ; derriere, les laiches de
 *                                    GROUND_COVER_001 (SM_Ecotone_ShoreTuft, essayee en v2, se
 *                                    lisait comme des bulbes verts a 1,7 m : retiree) ;
 *   eau vive (pente, sorties de lac)  gravier clair sur la berge, galets a la ligne d'eau,
 *                                    quelques blocs dans le courant, pas de roseaux.
 * La vitesse vient du reseau (FRiverPoint::Velocity, Manning) : un champ de vitesse est
 * estampe autour de chaque riviere ; ailleurs (lacs, mares, mers) l'eau est calme.
 *
 * Deux sorties, toutes deux deterministes et sans UObject :
 *   PaintBanks  bandes de couleur de sommet du sol (vase / gravier) et lustre humide, sur
 *               la geometrie forgee, avant la creation de la section de sol ;
 *   Build       instances de roseaux, galets et blocs, posees sur le sol
 *               et l'eau REELLEMENT rendus (forge + drainage).
 * Les maillages sont ceux qui existent deja (Ecotone, Rock) : rien n'est cree dans Content.
 */
namespace AnastasisRiverbank
{
enum class EFamily : uint8
{
	/** Tige de roseau (SM_Ecotone_Reed), en massifs, eau calme. */
	Reed,
	/** Galet (SM_Rock_Low), a la ligne d'eau vive. */
	Cobble,
	/** Bloc (SM_Rock_Boulder), rare, dans le courant vif. */
	Boulder,
	Count
};
constexpr int32 FamilyCount = static_cast<int32>(EFamily::Count);

const TCHAR* FamilyName(EFamily Family);

/** Vitesse d'ecoulement (m/s) sur la grille fine du drainage : 0 loin des rivieres. */
struct FSpeedField
{
	int32 W = 0, H = 0;
	double X0 = 0.0, Y0 = 0.0, Step = 0.0;
	TArray<float> Speed;
	/** Bilineaire ; 0 hors grille (eau calme). */
	double Sample(double X, double Y) const;
	bool IsValid() const { return W > 1 && H > 1 && Step > 0.0 && Speed.Num() == W * H; }
};

/**
 * Estampe la vitesse de chaque point de riviere sur la grille du reseau : pleine jusqu'a
 * 5 m au-dela du bord mouille, nulle a 15 m. Le maximum l'emporte (confluences).
 */
void BuildSpeedField(const AnastasisDrainage::FNetwork& Network, FSpeedField& Out);

struct FSettings
{
	/**
	 * m/s : en dessous, eau calme ; au-dessus de FastVelocity, eau vive. Fondu entre les deux.
	 * Relatifs au reseau, pas aux rivieres reelles : ses vitesses de Manning sont hautes (seed
	 * 12345 : mediane 1.7 m/s, la grande riviere de plaine 1.1-1.5). A 0.45 / 1.1, toutes les
	 * rivieres sortaient "vives" : des galets partout, des roseaux seulement aux lacs.
	 */
	double CalmVelocity = 1.3;
	double FastVelocity = 2.3;
	/** Grille des candidats (touffes, galets) et des massifs de roseaux (uu). */
	double CellUU = 150.0;
	double ClumpCellUU = 450.0;
	/** Hauteur du sol au-dessus de l'eau (uu) ; negatif = sous l'eau. */
	double ReedMinFreeboard = -30.0, ReedMaxFreeboard = 25.0;
	double CobbleMinFreeboard = -35.0, CobbleMaxFreeboard = 45.0;
	double BoulderMinFreeboard = -110.0, BoulderMaxFreeboard = -30.0;
	/** Pente maximale (dZ/dXY) : pas de roseau sur un talus raide. */
	double ReedMaxSlope = 0.35;
	/** Massifs : bruit de valeur a cette echelle, seuil au-dessus duquel un massif existe. */
	double PatchScaleUU = 2200.0;
	double ReedPatchThreshold = 0.45;
	/**
	 * Bandes de sol peintes au-dessus de l'eau (uu). Sous 100 : le drainage donne aux sommets secs
	 * loin de l'eau la sentinelle sol - 1 m, qui ne doit jamais passer pour une rive.
	 */
	double MudBandUU = 70.0;
	double GravelBandUU = 90.0;
	int32 MaxInstances = 400000;
};

struct FInputs
{
	/** Sol et eau rendus. Faux = hors du sol rendu. */
	TFunction<bool(double X, double Y, double& Z)> SampleHeight;
	TFunction<bool(double X, double Y, double& Z)> SampleWaterHeight;
	FBox2D Bounds = FBox2D(ForceInit);
	int32 Seed = 0;
};

struct FPlacement
{
	/** Pied de l'instance (uu) : sur le sol, ou sur le fond sous l'eau. */
	FVector Location = FVector::ZeroVector;
	double Yaw = 0.0;
	/** Inclinaison (degres) et sa direction (degres). */
	double Tilt = 0.0;
	double TiltYaw = 0.0;
	/**
	 * Roseaux : facteur d'echelle du maillage.
	 * Galets et blocs : diametre vise (uu) -- l'incarnation le convertit avec les bornes du maillage.
	 */
	double Size = 1.0;
	/** Part de la hauteur enfoncee dans le sol (galets, blocs). */
	double Sink = 0.0;
	int32 Variant = 0;
	EFamily Family = EFamily::Reed;
};

struct FPlan
{
	TArray<FPlacement> Instances;
	int32 Counts[FamilyCount] = {};
	int32 ReedClumps = 0;
	/** Candidats examines a la ligne d'eau, calmes / vifs (calme >= 0.5 ou non). */
	int32 CalmShore = 0;
	int32 FastShore = 0;
	bool bTruncated = false;
	double MilliSeconds = 0.0;
};

/** Calme [0,1] : 1 en eau calme, 0 en eau vive. */
double Calmness(double VelocityMs, const FSettings& Settings);

bool Build(const FInputs& In, const FSpeedField& Speed, const FSettings& Settings, FPlan& Out, FString& Error);

struct FPaintResult
{
	int32 MudVertices = 0;
	int32 GravelVertices = 0;
};

/**
 * Peint les bandes de rive dans la geometrie du sol : sommets SECS a moins de MudBandUU /
 * GravelBandUU au-dessus de leur eau. Vase : couleur assombrie vers une vase saturee, et
 * humidite (UV1.y) relevee -- M_AnastasisGround y abaisse la rugosite, la vase luit. Gravier :
 * couleur vers un gravier clair, et poids de roche (UV0.x) releve -- le grain de pierre du
 * materiau. Les sommets immerges (fond, deja peint par le drainage) ne sont pas touches.
 */
FPaintResult PaintBanks(const FSpeedField& Speed, const FSettings& Settings, AnastasisTerrainSurface::FGeometry& Geometry);
}
