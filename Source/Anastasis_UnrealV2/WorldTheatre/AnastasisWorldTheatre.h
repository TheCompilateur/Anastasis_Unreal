#pragma once

#include "CoreMinimal.h"

/**
 * WORLD_THEATRE_001 -- la couche de mise en scene, partie pure (aucun UObject).
 *
 * Elle ne decide rien : la composition est un PLAN explicite (AnastasisWorldTheatrePlan.inl), ecrit par
 * l'analyse perceptuelle hors moteur (tools/unreal/world-theatre-analyze.py) a partir du monde mesure.
 * Ici, on drape ce plan sur le relief REELLEMENT rendu, et on refuse ce qui n'y tient plus (sol absent,
 * eau, pente) -- en le disant. Rien n'est tire au hasard : le seul "bruit" est le grain des couronnes
 * d'une masse forestiere, a l'echelle d'un arbre, invisible comme forme a la distance ou la masse joue.
 *
 * Ce que la couche porte, et ce qu'elle ne pretend jamais etre :
 *  - des MASSES (foret lointaine) : une enveloppe de canopee, pas des arbres ;
 *  - des SILHOUETTES (tour, chapelle, hameau) : des volumes lus a 1-8 km, sans collision ni interieur ;
 *  - des TRACES (chemin lointain) : un ruban pale sur le sol.
 * Aucun element n'a de collision, de navigation ni d'effet sur la simulation.
 */
namespace AnastasisWorldTheatre
{
enum class ESilhouette : uint8
{
	Tower,
	/** Tour de guet abandonnee : fut sans toit, sommet rompu (zone de depart exposee, sans garnison). */
	RuinedTower,
	Chapel,
	Hamlet,
};

/** Une masse forestiere : contour ferme en XY (uu), hauteur de canopee (uu). */
struct FMass
{
	const TCHAR* Id = TEXT("");
	TArray<FVector2D> Outline;
	/** Clairieres (cretes nues, combes ouvertes) : contours fermes a l'interieur du contour. Regle pair-impair. */
	TArray<TArray<FVector2D>> Holes;
	double CanopyHeight = 1600.0;
	/** Distance (uu) sur laquelle la lisiere monte du sol a la canopee : un bord de couronnes, pas un mur. */
	double EdgeRamp = 2500.0;
	FLinearColor Colour = FLinearColor(0.035f, 0.06f, 0.03f);
};

struct FSilhouetteSpec
{
	const TCHAR* Id = TEXT("");
	ESilhouette Kind = ESilhouette::Tower;
	FVector2D Location = FVector2D::ZeroVector;
	/** Cap (deg) de l'axe long du volume. */
	double Yaw = 0.0;
	/** Echelle sur les dimensions de reference du type (1 = tour de 14 m, chapelle de 12 m de long). */
	double Scale = 1.0;
	/** Pente maximale (deg) du sol sous l'emprise ; au-dela, refuse. */
	double MaxSlopeDeg = 14.0;
};

struct FTrace
{
	const TCHAR* Id = TEXT("");
	TArray<FVector2D> Points;
	double Width = 450.0;
	FLinearColor Colour = FLinearColor(0.42f, 0.36f, 0.26f);
};

struct FPlan
{
	TArray<FMass> Masses;
	TArray<FSilhouetteSpec> Silhouettes;
	TArray<FTrace> Traces;
};

/** Le plan versionne, ecrit par l'analyse. */
const FPlan& CanonicalPlan();

/** Sol rendu sous (X, Y) en uu ; false hors du releve. L'eau rendue, si presente, dans WaterZ (sinon -inf). */
using FGroundSampler = TFunction<bool(double X, double Y, double& GroundZ, double& WaterZ)>;

struct FMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FLinearColor> Colours;

	int32 TriangleCount() const { return Triangles.Num() / 3; }
	void Append(const FMeshData& Other);
};

struct FBuildReport
{
	int32 MassesBuilt = 0;
	int32 SilhouettesBuilt = 0;
	int32 TracesBuilt = 0;
	/** "id: raison" pour chaque element refuse. */
	TArray<FString> Rejected;
	/** Surface couverte par les masses (ha). */
	double MassHectares = 0.0;
};

struct FBuilt
{
	FMeshData Masses;
	FMeshData Silhouettes;
	FMeshData Traces;
	FBuildReport Report;
};

/** Cellule (uu) de la grille d'une masse : 40 m, la taille d'un groupe de couronnes. */
inline constexpr double MassCell = 4000.0;

bool PointInPolygon(const TArray<FVector2D>& Poly, const FVector2D& P);
/** Distance (uu) du point au contour du polygone. */
double DistanceToOutline(const TArray<FVector2D>& Poly, const FVector2D& P);

/** Drape le plan sur le sol echantillonne. */
FBuilt Build(const FPlan& Plan, const FGroundSampler& Ground);

/** Une masse seule (tests). False et Why si elle ne tient pas sur le sol. */
bool BuildMass(const FMass& Mass, const FGroundSampler& Ground, FMeshData& Out, double& OutHectares, FString& Why);
bool BuildSilhouette(const FSilhouetteSpec& Spec, const FGroundSampler& Ground, FMeshData& Out, FString& Why);
bool BuildTrace(const FTrace& Trace, const FGroundSampler& Ground, FMeshData& Out, FString& Why);
}
