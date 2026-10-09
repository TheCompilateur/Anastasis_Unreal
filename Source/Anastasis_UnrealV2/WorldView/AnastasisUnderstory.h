#pragma once

#include "CoreMinimal.h"
#include "WorldView/AnastasisWorldView.h"

/**
 * FOREST_TERRAIN_P3 -- strate arbustive et mineraliere de la carte.
 *
 *   maquis   lentisque (bas, sec), chene kermes (pentes rocheuses), genet (lisieres, ouvert)
 *   ronces   lisieres des couronnes posees et bandes riveraines
 *   rochers  epars sur le pre, plus nombreux et plus gros avec la pente, la roche et l'altitude
 *
 * Pure et deterministe, sans UObject : l'incarnation fournit le sol, l'eau et la rive REELLEMENT
 * rendus, les couronnes deja posees et le bassin ; Build rend un plan. Les regles sont des
 * aptitudes douces tirees par cellule (une candidate par CellUU), jamais des seuils par tuile :
 * aucune limite de la grille de simulation ne se lit dans le resultat. Jamais sur un champ, une
 * ruine ou l'eau ; hors du bassin du village, du lit des rivieres ecrites et de la route du col.
 */
namespace AnastasisUnderstory
{
enum class EKind : uint8
{
	Lentisk, KermesOak, Broom, Bramble, Rock,
	// GPT_FLORA_001 -- les six petits sujets de la planche GPT (create-gpt-flora.py), apres Rock pour ne rien
	// renumeroter. Poses seulement dans une cellule que le maquis, les ronces et les rochers laissent vide.
	Juniper, Hazel, BerryShrub, Rhododendron, Fern, Meadow,
	Count
};
inline constexpr int32 KindCount = static_cast<int32>(EKind::Count);
inline constexpr int32 ShrubShapes = 3;

/** Familles de rochers reprises de Content/Anastasis/Rock (celles des lieux composes). */
enum class ERock : uint8 { Boulder, Low, Split, Massive, Cluster, CliffFragment, Vertical, Count };
inline constexpr int32 RockCount = static_cast<int32>(ERock::Count);

/** Un des six sujets GPT : un seul maillage, pas de forme 1..3. */
inline bool IsGptKind(EKind Kind) { return static_cast<int32>(Kind) >= static_cast<int32>(EKind::Juniper) && Kind != EKind::Count; }
const TCHAR* KindName(EKind Kind);
const TCHAR* RockName(ERock Rock);
int32 RockVariants(ERock Rock);
/** Assets de tools/unreal/create_tree_asset.py (arbustes) et des lieux composes (rochers). */
FString ShrubMeshPath(EKind Kind, int32 Variant);
FString RockMeshPath(ERock Rock, int32 Variant);

struct FSettings
{
	/** Une candidate par cellule, jittee dans la cellule. */
	double CellUU = 400.0;
	/** Demi-ecart des sondes de pente. */
	double ProbeUU = 150.0;
	double WaterClearanceUU = 15.0;
	double MaxShrubSlopeDegrees = 38.0;
	double MaxRockSlopeDegrees = 60.0;
	/** Maquis par cellule, a aptitude pleine. */
	double MaquisDensity = 0.30;
	/** Taches de maquis, en tuiles : le maquis vient par plaques, pas en semis regulier. */
	double MaquisPatchTiles = 3.0;
	/** Le sim dit Scrub : la garrigue y est chez elle. */
	double ScrubBoost = 1.5;
	/** Fonds de vallee pature : il y reste cette part du maquis. */
	double ValleyKeep = 0.15;
	/** Sous les couronnes : cette part (ombre). */
	double CanopyKeep = 0.2;
	double BrambleDensity = 0.35;
	/** GPT_FLORA_001 : multiplicateur de la densite des six sujets GPT (0 = aucun). */
	double GptFloraDensity = 1.0;
	/** Rochers : eparpilles sur le pre, avec la pente, sur la roche du sim, en altitude. */
	double RockMeadowDensity = 0.006;
	double RockSlopeDensity = 0.22;
	double RockStoneDensity = 0.15;
	double RockAltitudeDensity = 0.05;
	double RockClusterTiles = 1.5;
	double BasinClearRadiusTiles = 8.0;
	int32 MaxInstances = 400000;
};

struct FInputs
{
	/** Le monde canonique entier : types, humidite. Obligatoire. */
	const AnastasisWorldView::FWorldVisualSnapshot* Source = nullptr;
	/** Sol rendu (uu). Obligatoire. */
	TFunction<bool(double, double, double&)> SampleHeight;
	/** Nappe rendue (uu). Optionnel : WaterPlaneZ sinon. */
	TFunction<bool(double, double, double&)> SampleWaterHeight;
	/** Humidite de rive rendue [0,1]. Optionnel. */
	TFunction<bool(double, double, double&)> SampleRiparian;
	/** Couronnes posees : X, Y, rayon (uu). */
	TArray<FVector> Canopy;
	FVector Basin = FVector::ZeroVector;
	bool bHasBasin = false;
	double WaterPlaneZ = 0.0;
	/** Hauteur du relief rendu au-dessus de l'eau : l'altitude relative, comme pour les arbres. */
	double AltitudeSpanUU = 1.0;
};

struct FInstance
{
	EKind Kind = EKind::Lentisk;
	/** Forme d'arbuste (0..ShrubShapes-1) ou variante de rocher. */
	int32 Variant = 0;
	ERock Rock = ERock::Boulder;
	FVector Ground = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	/** Hauteur visee, metres. */
	double HeightM = 1.0;
	double Yaw = 0.0;
	double SlopeDegrees = 0.0;
	/** Secheresse du site [0,1], meme sens que la teinte des arbres. */
	double Dryness = 0.0;
	/** Tirage individuel [0,1] : ecart de teinte, enfoncement d'un rocher. */
	double Jitter = 0.0;
};

struct FPlan
{
	TArray<FInstance> Instances;
	int32 Counts[KindCount] = {};
	int32 Cells = 0;
	int32 RejectedWater = 0;
	int32 RejectedReserved = 0;
	bool bTruncated = false;
};

bool Build(const FInputs& In, const FSettings& Settings, FPlan& Out, FString& OutError);
}
