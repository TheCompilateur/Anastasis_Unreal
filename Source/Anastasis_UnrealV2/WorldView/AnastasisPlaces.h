#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "WorldView/AnastasisWorldView.h"

class AActor;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * PLACES -- WORLD_DRESSING_01. Des lieux, pas une distribution.
 *
 * Lit la geographie deja etablie (tuiles de simulation, relief rendu, geographie humaine
 * de la graine 12345) et en tire une poignee d'endroits reconnaissables : la source et ses
 * pierres levees, le col, le guet du point haut, le chaos rocheux, l'ancien hameau, le
 * marais du delta, le vieux chene, la vieille foret. Chaque lieu est une COMPOSITION de
 * pieces existantes (Rock, Lithos, ruines, Ecotone, arbres), ancree sur le sol rendu.
 *
 * Ne modifie ni la simulation, ni le relief, ni l'eau, ni les autres couches de dressing.
 * Les vallees de la geographie humaine et le bassin habitable restent VIDES : c'est la
 * reserve du futur village et des champs.
 *
 * Deux etages : Compose() est pur (aucun UObject, aucun chemin charge, deterministe,
 * testable) ; Embody() charge les meshes et pose des HISM transitoires.
 */
namespace AnastasisPlaces
{
enum class EFamily : uint8
{
	RockVertical, RockBoulder, RockMassive, RockSplit, RockCliff, RockLow, RockCluster,
	LithosSummit, LithosOutcrop, LithosStratum, LithosVerticalWall, LithosInclinedWall,
	LithosCornice, LithosTalus, LithosDetached, LithosFractured,
	RuinSoubassement, RuinAngle, RuinMur, RuinFoyer, RuinEnclos, RuinReemploi,
	TreeBroadleafEmergent, TreeConiferEmergent, TreeBroadleafCanopy,
	FallenLog, Stump, BranchPile, ExposedRoots, BushLow, Sapling, Reed, ShoreTuft, Driftwood,
	Count
};

enum class EKind : uint8 { Spring, Pass, Lookout, Crags, Outcrop, Hamlet, Vestige, Marsh, OldTree, OldWood };

struct FPiece
{
	EFamily Family = EFamily::RockBoulder;
	uint8 Variant = 0;
	int32 Place = INDEX_NONE;
	/** Position monde (uu), deja a l'echelle spatiale du snapshot. */
	FVector2D XY = FVector2D::ZeroVector;
	double Yaw = 0.0;
	double Scale = 1.0;
	/** Part de la hauteur (apres rotation) enfouie sous le point le plus bas de l'empreinte. */
	double Sink = 0.1;
	/** Inclinaison hors verticale (degres) et azimut vers lequel la piece penche. */
	double Tilt = 0.0;
	double TiltToward = 0.0;
	/** Epouse la pente mesuree sous l'empreinte (fondations, troncs couches). */
	bool bFollowSlope = false;
};

struct FPlace
{
	EKind Kind = EKind::Crags;
	FString Id;
	FString Name;
	FVector2D Center = FVector2D::ZeroVector;
	double Radius = 0.0;
	int32 FirstPiece = 0;
	int32 NumPieces = 0;
};

struct FPlan
{
	TArray<FPlace> Places;
	TArray<FPiece> Pieces;
	/** Lieux cherches mais non trouves (geographie absente, graine differente...). */
	TArray<FString> Missing;
	/** Lieu proprietaire de chaque tuile du snapshot source (index de Places), INDEX_NONE sinon. */
	TArray<int32> TileOwner;
};

struct FInputs
{
	const AnastasisWorldView::FWorldVisualSnapshot* Source = nullptr;
	/** Sol reellement rendu en (X, Y) monde. Obligatoire. */
	TFunction<bool(double, double, double&)> Ground;
	/** Nappe d'eau en (X, Y) ; vide = plan d'eau plat WaterPlaneZ. */
	TFunction<bool(double, double, double&)> Water;
	bool bLandmark = false;
	FVector Landmark = FVector::ZeroVector;
	bool bBasin = false;
	FVector Basin = FVector::ZeroVector;
};

/** Seuil de la reserve : au-dela de ce poids de vallee, aucun rocher, aucune ruine. */
inline constexpr double ValleyReserve = 0.3;
/** Rayon de reserve autour du bassin habitable, en tuiles. */
inline constexpr double BasinReserveTiles = 10.0;

/** Plan complet. Donnees invalides : false, OutError renseigne, aucun plan partiel. */
bool Compose(const FInputs& In, FPlan& Out, FString& OutError);

/**
 * Vrai si un lieu compose REMPLACE la presentation par tuile de cette tuile source : les
 * ruines composees (hameau, vestiges) remplacent le moignon generique pose tuile par tuile,
 * au lieu de s'y superposer.
 */
bool SupersedesTile(const FPlan& Plan, int32 SourceIndex);

/** Poids de vallee (0..1) au point monde ; 0 hors geographie humaine. */
double ValleyWeightAt(const AnastasisWorldView::FWorldVisualSnapshot& Source, double X, double Y);

/** Chemin d'objet du mesh d'une piece, par ex. /Game/Anastasis/Rock/SM_Rock_Split_02.SM_Rock_Split_02. */
FString MeshPath(EFamily Family, int32 Variant);
int32 VariantCount(EFamily Family);
const TCHAR* FamilyName(EFamily Family);

struct FEmbodyResult
{
	int32 Instances = 0;
	int32 Ungrounded = 0;
	int32 MissingMeshes = 0;
	TArray<FVector> PlaceLocations;
};

/**
 * Pose le plan : un HISM par (famille, variante), transitoires, attaches a Root.
 * InOutComponents est vide et recree a chaque appel. Pierre et ruine n'ont pas de couleur de
 * sommet : elles passent par ShapeMaterial teinte, comme Ruin dans le registre.
 */
FEmbodyResult Embody(AActor& Owner, const FPlan& Plan, const FInputs& In, UMaterialInterface* ShapeMaterial,
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& InOutComponents);
}
