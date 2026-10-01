#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class AActor;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;

/**
 * MICRO_ECOLOGY_001 — microstructure des berges, de la prairie, de la lisiere et du sous-bois.
 *
 * Le projet n'a pas de graphe PCG, de Landscape, de RVT, ni de Foliage Type. Le dressing
 * existant (AnastasisEcologicalDressing, AnastasisGroundCover, AnastasisPlaces) reste le
 * cadre : cette passe s'y ajoute, elle ne le remplace pas.
 *
 * Elle ne decide pas la topographie. Elle lit le sol, l'eau et les couronnes deja poses,
 * puis distribue des poches : une berge n'est pas un ruban, une prairie n'est pas un tapis,
 * une lisiere n'est pas un cercle, un sous-bois n'est pas une grille.
 *
 * Build est pur. Embody pose des HISM (meshes Ecotone et Rock deja forges). ApplySoil
 * module la couleur de sommet deja ecrite, sans remplacer le materiau de sol.
 */
namespace AnastasisMicroEcology
{
enum class ERole : uint8
{
	BankPebble, BankReed, BankTuft, BankDrift, BankBranch,
	MeadowStone, MeadowBush,
	EdgeBush, EdgeSapling,
	UnderLog, UnderStump, UnderBranch, UnderRoots, UnderSapling,
	Count
};

/** Etat de poche. None = la couche n'a rien a ajouter (l'herbe et les arbres existants restent). */
enum class EPocket : uint8
{
	None = 0,
	Clean, Rocky, Muddy, Vegetated, Drift,
	MeadowBare, MeadowDry, MeadowStone, MeadowWet,
	Count
};

inline constexpr int32 RoleCount = static_cast<int32>(ERole::Count);
inline constexpr int32 PocketCount = static_cast<int32>(EPocket::Count);

struct FSettings
{
	/** Pas des candidats de berge (uu). Un candidat par cellule, jamais un tapis. */
	double BankCellUU = 200.0;
	/** Hauteur au-dessus de la nappe ou la berge existe (uu). Au-dela, seulement si l'humidite de rive est forte. */
	double BankMinAboveUU = 8.0;
	double BankMaxAboveUU = 90.0;
	double BankWetExtendUU = 220.0;
	double BankWetness = 0.45;
	/** Taille des poches de berge (uu). Le bruit large fait des etats, le bruit court les casse dans la largeur. */
	double BankPocketSpanUU = 1600.0;
	double MeadowCellUU = 800.0;
	double MeadowPocketSpanUU = 2400.0;
	double MeadowMinMask = 0.35;
	/** Prairie au-dela de ce multiple du rayon de couronne. En deca, lisiere ou sous-bois. */
	double MeadowCanopyClear = 2.55;
	double EdgePocketSpanUU = 900.0;
	double UnderPocketSpanUU = 750.0;
	double ProbeUU = 60.0;
	/** Assez fin pour qu'une berge de quelques metres tombe dans sa propre cellule. */
	double SoilCellUU = 400.0;
	double MaxBankSlope = 36.0;
	double MaxVegSlope = 22.0;
	double MaxMeadowSlope = 18.0;
	double MaxForestSlope = 32.0;
	int32 MaxInstances = 20000;
};

struct FClearing
{
	FVector2D Center = FVector2D::ZeroVector;
	double Radius = 0.0;
};

struct FInputs
{
	TFunction<bool(double, double, double&)> SampleHeight;
	/** Nappe rendue. Sans elle, pas de berge (on ne devine pas ou est l'eau). */
	TFunction<bool(double, double, double&)> SampleWaterHeight;
	/** Humidite de rive [0,1]. Optionnel : etend la berge la ou le drainage dit que c'est mouille. */
	TFunction<bool(double, double, double&)> SampleWetness;
	/** Espace ouvert [0,1]. Obligatoire pour la prairie. */
	TFunction<double(double, double)> Mask;
	FBox2D Bounds = FBox2D(ForceInit);
	/** Couronnes posees : X, Y, rayon (uu). */
	TArray<FVector> Canopy;
	TArray<FClearing> Clearings;
	uint32 Seed = 0;
};

struct FPlacement
{
	FVector Ground = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	double Yaw = 0.0;
	double Scale = 1.0;
	double SlopeDegrees = 0.0;
	double AboveWater = 0.0;
	ERole Role = ERole::BankPebble;
	EPocket Pocket = EPocket::None;
};

struct FSoilField
{
	FVector2D Origin = FVector2D::ZeroVector;
	double CellUU = 1600.0;
	int32 W = 0;
	int32 H = 0;
	TArray<uint8> Cells;

	bool IsValid() const { return W > 0 && H > 0 && Cells.Num() == W * H && CellUU > 0.0; }
	EPocket Sample(double X, double Y) const;
};

struct FPlan
{
	TArray<FPlacement> Instances;
	int32 Counts[RoleCount] = {};
	int32 PocketCells[PocketCount] = {};
	FSoilField Soil;
	int32 Candidates = 0;
	int32 RejectedGround = 0;
	int32 RejectedWater = 0;
	int32 RejectedSlope = 0;
	int32 RejectedMask = 0;
	int32 RejectedClearing = 0;
	int32 RejectedDensity = 0;
	bool bTruncated = false;
};

const TCHAR* RoleName(ERole Role);
const TCHAR* PocketName(EPocket Pocket);

/** Etat de berge au point, independant de la pente. None si le bruit ne s'applique pas : le bruit, lui, rend toujours un etat. */
EPocket BankPocket(uint32 Seed, double X, double Y, const FSettings& Settings);
/** None sur la majeure partie de la prairie ; les autres etats sont des taches. */
EPocket MeadowPocket(uint32 Seed, double X, double Y, const FSettings& Settings);

bool Build(const FInputs& In, const FSettings& Settings, FPlan& Out, FString& OutError);

/**
 * Module une couleur de sol deja resolue. None, Vegetated et Drift ne changent rien :
 * les plantes portent ces poches. Clean asseche le ruban humide uniforme.
 */
FLinearColor TintSoil(const FLinearColor& Base, EPocket Pocket, double Strength);

struct FEmbodyResult
{
	int32 Instances = 0;
	int32 MissingMeshes = 0;
	int32 Components = 0;
};

/** HISM transitoires, reutilises par nom. Vide, ne detruit pas. */
FEmbodyResult Embody(AActor& Owner, const FPlan& Plan, UMaterialInterface* ShapeMaterial,
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& InOutComponents);

/** Recolorie la section 0 du sol forge. Les positions ne bougent pas. */
int32 ApplySoil(UProceduralMeshComponent* Surface, const FSoilField& Field, double Strength);
}
