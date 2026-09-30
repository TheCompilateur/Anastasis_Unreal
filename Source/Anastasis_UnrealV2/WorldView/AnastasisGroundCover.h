#pragma once

#include "CoreMinimal.h"

/**
 * GROUND_COVER_001 -- la strate herbacee des espaces ouverts.
 *
 * Trois familles, lues sur les planches Etat Zero (docs/visual/reference/) :
 *   H1 MeadowTall  prairie haute de clairiere naturelle (EZ5, EZ1)   : plat, a decouvert
 *   H2 MeadowShort prairie basse, terre seche ou tassee (EZ5, EZ2)    : pente moderee, sol pietine
 *   H3 Sedge       prairie humide, laiches et carex (EZ5, EZ4, EZ2)   : rive, creux mouilles
 * Les trois autres familles de la note de cadrage (joncs de rive, herbacees de sous-bois,
 * lande d'eboulis) sont des passes suivantes : au-dela de MaxSlopeDegrees et sous la couronne
 * d'un arbre, cette passe ne pose rien.
 *
 * Pure et deterministe : aucune dependance UObject, aucun etat global. L'incarnation fournit
 * le sol, l'eau et l'humidite REELLEMENT rendus, le masque des espaces ouverts et les
 * couronnes deja posees ; Build rend un plan. Meme graine, memes entrees = meme plan.
 */
namespace AnastasisGroundCover
{
enum class EFamily : uint8 { MeadowTall, MeadowShort, Sedge, Count };

inline constexpr int32 FamilyCount = static_cast<int32>(EFamily::Count);

const TCHAR* FamilyName(EFamily Family);
/** Chemin d'objet du mesh d'une famille (assets de tools/unreal/create-ground-cover.py). */
FString MeshPath(EFamily Family);

struct FSettings
{
	/** Une candidate par cellule, jittee dans la cellule. */
	double CellUU = 120.0;
	/** Rayon des sondes de pente : celui de la foret macro, pour que les deux lisent le meme sol. */
	double ProbeUU = 60.0;
	/** Bandes de pente du plan V2 : 0-10 prairie haute, 10-20 prairie basse, au-dela rien (lande = passe suivante). */
	double TallSlopeDegrees = 10.0;
	double MaxSlopeDegrees = 20.0;
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
	int32 RejectedDensity = 0;
	bool bTruncated = false;
};

bool Build(const FInputs& In, const FSettings& Settings, FPlan& Out, FString& OutError);
}
