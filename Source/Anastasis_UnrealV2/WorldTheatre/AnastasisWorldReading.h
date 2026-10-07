#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * WORLD_THEATRE_001 -- lecture du monde reellement rendu.
 *
 * Le monde d'ANASTASIS est transitoire : l'incarnation (AAnastasisWorldEmbodiment) le rebatit a chaque
 * chargement, sol forge, anneau d'horizon, eau, vegetation et villages. Cette lecture n'en change rien :
 * elle rasterise les maillages de sol et d'eau tels qu'ils sont rendus, et releve ce qui est pose dessus,
 * classe par famille de maillage. C'est la matiere premiere de la mise en scene (vistas, horizons,
 * vides) : on ne compose pas un monde qu'on n'a pas mesure.
 *
 * Deux grilles : PROCHE (la carte et l'avant-pays, pas fin) et LOINTAINE (tout l'anneau, pas large).
 * Une cellule sans triangle vaut NoData.
 */
namespace AnastasisWorldReading
{
inline constexpr float NoData = -1.0e30f;

struct FRaster
{
	/** Coin (X min, Y min) en uu ; la cellule (I, J) a son centre en Origin + (I + 0,5, J + 0,5) * Cell. */
	FVector2D Origin = FVector2D::ZeroVector;
	double Cell = 0.0;
	int32 W = 0;
	int32 H = 0;
	/** Altitude du sol rendu (uu), NoData hors maillage. */
	TArray<float> Ground;
	/** Altitude de la nappe d'eau rendue (uu), NoData sans eau. */
	TArray<float> Water;

	void Init(const FVector2D& InOrigin, double InCell, int32 InW, int32 InH);
	int32 Index(int32 I, int32 J) const { return J * W + I; }
	int32 FilledCount(const TArray<float>& Layer) const;
};

/** Ce qui est pose sur le sol, par famille. Une ligne par instance ou par composant isole. */
enum class EFamily : uint8
{
	Tree,
	Shrub,
	Rock,
	Building,
	Ruin,
	Place,
	Reed,
	Other,
	Count
};

const TCHAR* FamilyName(EFamily Family);
/** Famille d'un maillage d'apres son chemin d'asset ; Other si rien ne la dit. Herbe et couvre-sol : bIgnore. */
EFamily FamilyOfMesh(const FString& MeshPath, bool& bIgnore);

struct FPlaced
{
	EFamily Family = EFamily::Other;
	FName Mesh;
	FVector Location = FVector::ZeroVector;
	/** Hauteur de l'objet pose (uu) : boite du maillage a l'echelle de l'instance. */
	float Height = 0.0f;
	/** Rayon au sol (uu), meme source. */
	float Radius = 0.0f;
};

struct FReading
{
	FRaster Near;
	FRaster Far;
	/** Emprise rendue de la carte (sol forge), sans l'anneau. */
	FBox MapFootprint = FBox(ForceInit);
	FVector Basin = FVector::ZeroVector;
	FVector Landmark = FVector::ZeroVector;
	uint32 Seed = 0;
	TArray<FPlaced> Placed;
	/** Instances ignorees (herbe, couvre-sol) et composants releves, pour le journal. */
	int32 IgnoredInstances = 0;
	int32 MeshComponents = 0;
	int32 GroundTriangles = 0;
	int32 HorizonTriangles = 0;
	int32 WaterTriangles = 0;
};

/**
 * Releve le monde courant. False (et Why) si aucune incarnation complete n'est presente.
 * NearMarginUu : avant-pays couvert par la grille proche au-dela de la carte.
 */
bool Read(UWorld* World, double NearCellUu, double NearMarginUu, double FarCellUu, FReading& Out, FString& Why);

/**
 * Ecrit le releve dans Dir : reading.json (meta, familles), near_ground.f32, near_water.f32,
 * far_ground.f32, far_water.f32 (float32 petit-boutiste, ligne par ligne, J puis I), placed.csv.
 */
bool Dump(const FReading& Reading, const FString& Dir, FString& Why);
}
