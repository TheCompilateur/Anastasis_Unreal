#pragma once

#include "CoreMinimal.h"

/**
 * GROUND_COVER_001 -- la strate herbacee des espaces ouverts.
 *
 * Cinq familles, lues sur les planches Etat Zero (docs/visual/reference/) :
 *   H1  MeadowTall   prairie haute de clairiere naturelle (EZ5, EZ1)   : plat, a decouvert
 *   H2  MeadowShort  prairie basse, terre seche ou tassee (EZ5, EZ2)    : pente moderee, sol pietine
 *   H3  Sedge        prairie humide, laiches et carex (EZ5, EZ4, EZ2)   : rive, creux mouilles
 *   H6a HeathTussock touffe d'eboulis, graminee dure (EZ1 "rochers, eboulis") : versant 20-45 deg
 *   H6b Heather      callune de lande, epis mauves (EZ1 "pente subalpine : landes") : haut de versant
 *   H5a Fern         fougere en volant, frondes divisees (EZ3 "fougeres & herbacees") : trouees, bord de couronne
 *   H5b HartsTongue  scolopendre, lanieres entieres luisantes (EZ3, palette) : ombre humide
 *   H5c WoodHerb     herbacee d'ombre, luzule / anemone (EZ3 "strate herbacee") : ombre profonde
 * Reste a faire : joncs de rive (H4). Au-dela de MaxSlopeDegrees (falaise) et au pied des troncs,
 * rien n'est pose.
 *
 * Pure et deterministe : aucune dependance UObject, aucun etat global. L'incarnation fournit
 * le sol, l'eau et l'humidite REELLEMENT rendus, le masque des espaces ouverts et les
 * couronnes deja posees ; Build rend un plan. Meme graine, memes entrees = meme plan.
 */
namespace AnastasisGroundCover
{
enum class EFamily : uint8 { MeadowTall, MeadowShort, Sedge, HeathTussock, Heather, Fern, HartsTongue, WoodHerb, Count };

inline constexpr int32 FamilyCount = static_cast<int32>(EFamily::Count);

const TCHAR* FamilyName(EFamily Family);
/** Chemin d'objet du mesh d'une famille (assets de tools/unreal/create-ground-cover.py). */
FString MeshPath(EFamily Family);

struct FSettings
{
	/** Natural-history-001: presentation habitat rules; false retains the reference A/B. */
	bool bNaturalHistory = false;
	/** Une candidate par cellule, jittee dans la cellule. */
	double CellUU = 120.0;
	/** Rayon des sondes de pente : celui de la foret macro, pour que les deux lisent le meme sol. */
	double ProbeUU = 60.0;
	/** Bandes de pente du plan V2 : 0-10 prairie haute, 10-20 prairie basse, 20-45 lande, au-dela falaise nue. */
	double TallSlopeDegrees = 10.0;
	double MeadowSlopeDegrees = 20.0;
	double MaxSlopeDegrees = 45.0;
	/** Demi-largeur du fondu prairie -> lande autour de MeadowSlopeDegrees : pas de ligne de partage. */
	double LandeBlendDegrees = 2.0;
	/**
	 * Densite de la lande au pied du versant, puis a MaxSlopeDegrees : elle s'eclaircit en montant.
	 * v1 a 0.6 / 0.15, plancher 0.25, ombre pleine : ~5 % de couverture, invisible a 10 m.
	 */
	double LandeDensity = 0.9;
	double LandeDensitySteep = 0.3;
	/** Plancher de densite d'une tache de lande : plus clairsemee que la prairie, la roche affleure. */
	double LandePatchFloor = 0.35;
	/** Part de l'ombre des couronnes qui eteint la lande (prairie : 0.75) : un versant boise clair en garde. */
	double LandeShade = 0.5;
	/** Touffes de lande agrandies : petites et serrees, elles doivent encore couvrir le versant. */
	double LandeScale = 1.3;
	/**
	 * Callune au-dela de cette hauteur au-dessus du fond de vallee, pleinement a + HeatherRangeUU.
	 * Mesure sur la carte de reference (ANASTASIS_GROUND_SLOPES) : la lande est posee entre 0,6 et
	 * 34,8 m au-dessus du fond (p10 / p90, mediane 14,9 m). Une bande 15-50 m la manquait presque.
	 */
	double HeatherAboveFloorUU = 800.0;
	double HeatherRangeUU = 2000.0;
	/** Largeur du fondu entre prairie haute et basse autour de TallSlopeDegrees. */
	double BlendDegrees = 3.0;
	/** Sol au moins a cette hauteur au-dessus de la nappe rendue. */
	double WaterClearanceUU = 6.0;
	/** Humidite a partir de laquelle la prairie devient humide (laiches). */
	double SedgeWetness = 0.4;
	/** Sol a moins de cette hauteur au-dessus de la nappe : sature, meme sans riviere proche. */
	double DampHeightUU = 45.0;
	/** Masque minimal : en dessous, pas d'espace ouvert. */
	double MinMask = 0.05;
	/** Echelle des taches : une prairie n'est jamais un tapis (EZ5 : "un gradient de vie"). */
	double PatchSpanUU = 1600.0;
	/** Densite relative au creux d'une tache. v1 a 0.2 : des touffes isolees, pas une prairie. */
	double PatchFloor = 0.4;
	double Density = 1.0;
	/** Ombre de lisiere : la densite baisse jusqu'a ce multiple du rayon de couronne. */
	double CanopyShade = 1.6;
	/** Sous ce multiple du rayon de couronne : sous-bois, pas de prairie. */
	double CanopyExclusion = 0.8;
	/** Sous-bois (H5) : rien sous ce multiple du rayon (pied du tronc, racines, ombre la plus dense). */
	double TrunkClearance = 0.15;
	/** Densite du sous-bois et plancher de ses taches : il colonise par plaques, sol de litiere entre elles. */
	double UnderstoryDensity = 0.6;
	double UnderstoryPatchFloor = 0.2;
	/** Humidite a partir de laquelle la scolopendre prend le pas. */
	double HartsTongueWetness = 0.3;
	double ScaleMin = 0.8;
	double ScaleMax = 1.2;
	/** Garde-fou de cout : au-dela, la passe s'arrete et le dit. Toute la carte : ~1 M attendu. */
	int32 MaxInstances = 1500000;
};

/** Zone pietinee (hameau) : densite multipliee par Keep, herbe rase seulement. */
struct FClearing
{
	FVector2D Center = FVector2D::ZeroVector;
	double Radius = 0.0;
	double Keep = 0.3;
};

struct FInputs
{
	/** Sol rendu (uu). Obligatoire. */
	TFunction<bool(double, double, double&)> SampleHeight;
	/** Nappe rendue (uu). Optionnel : sans elle, aucun refus d'eau ni saturation par la hauteur. */
	TFunction<bool(double, double, double&)> SampleWaterHeight;
	/** Humidite de rive [0,1] (AnastasisDrainage::RiparianAt). Optionnel. */
	TFunction<bool(double, double, double&)> SampleWetness;
	/** Espace ouvert [0,1] : ou une prairie a le droit d'exister. Obligatoire. */
	TFunction<double(double, double)> Mask;
	/** Habitat de lande [0,1] (la roche y compte, au contraire de la prairie). Optionnel : Mask sinon. */
	TFunction<double(double, double)> LandeMask;
	/**
	 * Fond de vallee habitable (uu) : la callune tient le haut des versants, mesure depuis lui.
	 * Pas la nappe : hors rivieres et lacs, le drainage la pose a 1 m sous le sol partout (v1 :
	 * 6 140 callunes seulement). Sans fond connu, la callune ne suit que ses taches.
	 */
	bool bHasValleyFloor = false;
	double ValleyFloorZ = 0.0;
	/** Emprise parcourue, uu. */
	FBox2D Bounds = FBox2D(ForceInit);
	/** Couronnes deja posees : X, Y, rayon (uu). */
	TArray<FVector> Canopy;
	TArray<FClearing> Clearings;
	uint32 Seed = 0;
};

struct FPlacement
{
	FVector Ground = FVector::ZeroVector;
	/** Normale du sol lissee, pour incliner la touffe avec la pente. */
	FVector Normal = FVector::UpVector;
	double Yaw = 0.0;
	double Scale = 1.0;
	double SlopeDegrees = 0.0;
	double Wetness = 0.0;
	EFamily Family = EFamily::MeadowTall;
	/** Tirage [0,1) propre a la touffe : decide si elle reste visible au loin (eclaircie de distance). */
	double Thin = 0.0;
};

struct FPlan
{
	TArray<FPlacement> Instances;
	int32 Counts[FamilyCount] = {};
	int32 Candidates = 0;
	int32 RejectedMask = 0;
	int32 RejectedGround = 0;
	int32 RejectedWater = 0;
	int32 RejectedSlope = 0;
	int32 RejectedCanopy = 0;
	/** Candidates posees en sous-bois (sous une couronne). */
	int32 Understory = 0;
	int32 RejectedDensity = 0;
	/** Candidates sur sol sec par pente mesuree : 0-10, 10-20, 20-30, 30-45, 45-60, 60+ deg. */
	int32 SlopeBins[6] = {};
	/** Candidates de versant (> MeadowSlopeDegrees) refusees par une couronne. */
	int32 RejectedCanopySteep = 0;
	/** Hauteur au-dessus du fond de vallee des touffes de lande posees : p10, p50, p90 (uu). */
	double LandeAboveFloor[3] = {};
	bool bTruncated = false;
};

bool Build(const FInputs& In, const FSettings& Settings, FPlan& Out, FString& OutError);

/**
 * SOL SOUS L'HERBE. Part du sol couverte, par groupe de familles, lissee sur une grille de
 * quelques metres : X prairie (haute + basse), Y laiches, Z lande (touffes + callune), W sous-bois
 * (fougeres, scolopendre, herbacees d'ombre), [0,1].
 * C'est un champ BASSE FREQUENCE : il teinte la couleur de sommet du sol, qui porte la chromie
 * large (GROUND_HYDROLOGY_ARBITRATION.md) ; le materiau de sol garde le detail.
 */
struct FCoverField
{
	FVector2D Origin = FVector2D::ZeroVector;
	double CellUU = 400.0;
	int32 W = 0;
	int32 H = 0;
	TArray<FVector4f> Cover;

	bool IsValid() const { return W > 1 && H > 1 && Cover.Num() == W * H && CellUU > 0.0; }
	/** Bilineaire entre centres de cellules ; hors grille : zero. */
	FVector4f Sample(double X, double Y) const;
};

/** Teinte du sol sous chaque groupe : multiplicative sur la teinte de tuile, puis tiree vers un absolu. */
struct FSoilTint
{
	/** Facteurs sur la teinte de la tuile (garde sa semantique : sable, herbe, rive). */
	FLinearColor MeadowFactor = FLinearColor(0.62f, 0.70f, 0.50f);
	FLinearColor SedgeFactor = FLinearColor(0.50f, 0.58f, 0.50f);
	FLinearColor LandeFactor = FLinearColor(0.82f, 0.72f, 0.62f);
	/** Sous-bois : litiere de feuilles mortes et humus (EZ3, "litiere forestiere"). */
	FLinearColor UnderstoryFactor = FLinearColor(0.72f, 0.60f, 0.46f);
	/** Couleurs absolues vers lesquelles on tire a moitie : un sable sous prairie verdit. */
	FLinearColor MeadowAbsolute = FLinearColor(0.070f, 0.085f, 0.035f);
	FLinearColor SedgeAbsolute = FLinearColor(0.050f, 0.062f, 0.038f);
	FLinearColor LandeAbsolute = FLinearColor(0.095f, 0.082f, 0.060f);
	FLinearColor UnderstoryAbsolute = FLinearColor(0.055f, 0.042f, 0.026f);
	double AbsoluteShare = 0.5;
	/** Couverture a partir de laquelle la teinte est pleine. */
	double FullCover = 0.6;
	double Strength = 0.75;
};

/** Champ de couverture d'un plan : comptes par cellule normalises, puis deux passes de flou 3x3. */
void BuildCoverField(const FPlan& Plan, const FBox2D& Bounds, double CellUU, double CandidateCellUU, FCoverField& Out);
/** Teinte d'un sommet de sol ; Amount (optionnel) recoit la part de teinte appliquee [0,1]. Alpha intact. */
FLinearColor TintSoil(const FLinearColor& Base, const FVector4f& Cover, const FSoilTint& Tint, double* Amount = nullptr);
}
