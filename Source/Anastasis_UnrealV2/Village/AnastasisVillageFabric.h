#pragma once

#include "CoreMinimal.h"

/**
 * VILLAGE_FABRIC_001 -- le tissu entre les maisons : ruelles caladees (kaldirim), marches, placette du
 * puits, murets de terrasse en pierre seche. Grammaire de PRESENTATION : elle lit les batiments de la
 * simulation (case, type, point d'acces) et le relief rendu, et ne rend rien a la simulation. Aucun
 * tirage : la seule variation vient d'un hachage stable de l'identifiant. Deux appels sur la meme
 * entree, dans n'importe quel ordre de batiments, rendent le meme tissu.
 *
 * Ce que la grammaire tient du village pontique (vallees de Trebizonde, apres 1204) :
 * - les ruelles suivent les courbes de niveau et ne montent franchement qu'en escalier ;
 * - le reseau est un arbre qui pousse depuis la placette : chaque seuil rejoint le chemin deja trace le
 *   plus proche, comme les sentiers qui se greffent les uns sur les autres ;
 * - une maisonnee assise sur la pente tient sa terrasse par un mur de pierre seche cote aval, et se
 *   protege du talus cote amont.
 *
 * Toutes les longueurs sont en cm, dans le repere monde Unreal (X, Y horizontaux, Z altitude).
 */
namespace AnastasisVillageFabric
{
	/** Un batiment tel que la simulation le donne. */
	struct FPlot
	{
		FString Id;
		FString Type;
		/** Case de simulation (coin bas). Le centre monde est (Cell + 0.5) * CellCm. */
		FIntPoint Cell = FIntPoint::ZeroValue;
		/** Premier point d'acces de la simulation, en coordonnees de simulation (cases). */
		FVector2D Access = FVector2D::ZeroVector;
		bool bHasAccess = false;
	};

	struct FParams
	{
		/** Pas de la grille de routage. */
		double GridCm = 100.0;
		/** Demi-cote du carre reserve au corps du batiment (ARCH-09 : toute emprise tient dans +-900). */
		double BodyHalfCm = 900.0;
		/** Marge entre corps et chaussee. */
		double KeepOutCm = 40.0;
		/** Demi-cote du carre ou l'architecture defriche deja (emprise + 120) : l'herbe y est a elle. */
		double ArchitectureClearHalfCm = 1020.0;
		/** Rayon du disque reserve au puits au milieu de la placette. */
		double WellHalfCm = 300.0;
		double PlazaRadiusCm = 800.0;
		/** Ruelle d'une seule maisonnee, et ruelle maitresse (celle que tout le monde prend). */
		double LaneWidthCm = 150.0;
		double MainLaneWidthCm = 240.0;
		/** Au-dela de cette pente, la calade devient escalier. */
		double StepGrade = 0.16;
		/**
		 * Poids de la pente dans le cout : cout = longueur * (1 + SlopeWeight * pente^2). Pour gagner une
		 * hauteur donnee, la pente la moins couteuse vaut 1 / sqrt(SlopeWeight) : 50 donne 14 %, la pente
		 * d'un sentier muletier, juste sous StepGrade (une rampe en lacets ne bascule pas en marches a
		 * chaque metre). Plus raide que cela, la ruelle prend des lacets.
		 */
		double SlopeWeight = 50.0;
		/** Au-dela, le pas coute VeryStepCost fois plus : on ne grimpe droit que faute de mieux. */
		double MaxGrade = 0.5;
		double VeryStepCost = 12.0;
		/** Portee de la calade : au-dela (en cases, distance de Tchebychev a la racine), une maison est un ecart. */
		int32 MaxReachCells = 8;
		/** Cases de marge autour du hameau pour laisser les ruelles contourner. */
		int32 MarginCells = 1;
		/** Denivele minimal qui merite un mur de terrasse, et hauteur maximale d'un mur. */
		double WallMinDropCm = 45.0;
		double WallMaxCm = 320.0;
		/** Distance du mur au centre de la parcelle (bord de la plate-forme). */
		double TerraceHalfCm = 960.0;
		/** Echantillonnage du mur et ouverture laissee au seuil. */
		double WallStepCm = 100.0;
		double DoorGapCm = 260.0;
	};

	/** Un point de ruelle : sol pose, pente du troncon qui y arrive. */
	struct FLanePoint
	{
		FVector Position = FVector::ZeroVector;
		double Grade = 0.0;
		bool bStep = false;
	};

	struct FLane
	{
		/** Maisonnee dont le seuil ouvre cette ruelle. */
		FString FromId;
		/** Du seuil vers la jonction (placette ou ruelle deja tracee). */
		TArray<FLanePoint> Points;
		double WidthCm = 150.0;
		/** Nombre de seuils servis par le troncon le plus charge (1 = desserte). */
		int32 Usage = 1;
		double LengthCm = 0.0;
		/** Pente moyenne de la ruelle, et pente propre du terrain (ligne de plus grande pente) sous elle. */
		double MeanGrade = 0.0;
		double FallGrade = 0.0;
	};

	struct FWallRun
	{
		FString PlotId;
		/** Vrai : soutenement aval (la terrasse au-dessus du sol). Faux : mur de deblai amont. */
		bool bRetaining = true;
		/** Unitaire, horizontal : vers ou regarde le parement (l'aval pour un soutenement, la maison pour un deblai). */
		FVector2D Face = FVector2D(0.0, 1.0);
		/** Pied et crete de chaque echantillon, de proche en proche le long du bord. */
		TArray<FVector> Bottom;
		TArray<FVector> Top;
		double LengthCm = 0.0;
		double MaxHeightCm = 0.0;
	};

	struct FPlaza
	{
		bool bValid = false;
		FString WellId;
		FVector Centre = FVector::ZeroVector;
		double RadiusCm = 0.0;
		/** Niveau de la placette : mediane du sol sous le disque. */
		double LevelCm = 0.0;
		/** Ombre de la placette (platane), hors chaussee et hors parcelle. */
		bool bHasTree = false;
		FVector TreeSpot = FVector::ZeroVector;
	};

	struct FReport
	{
		int32 Plots = 0;
		/** Batiments hors de portee de la placette : ni ruelle ni mur. */
		int32 Remote = 0;
		int32 Doors = 0;
		int32 ConnectedDoors = 0;
		int32 Lanes = 0;
		double LaneLengthCm = 0.0;
		double StepLengthCm = 0.0;
		/** Moyennes ponderees par la longueur. Le tissu suit les courbes si LaneMeanGrade < FallMeanGrade. */
		double LaneMeanGrade = 0.0;
		double FallMeanGrade = 0.0;
		double LaneMaxGrade = 0.0;
		int32 Walls = 0;
		double WallLengthCm = 0.0;
		double WallMaxHeightCm = 0.0;
		/** Cellules de chaussee qui empietent sur un corps de batiment : doit rester 0. */
		int32 BodyIntrusions = 0;
		/** Hachage du tissu rendu : meme entree, meme hachage. */
		uint32 Signature = 0;
	};

	struct FFabric
	{
		FPlaza Plaza;
		TArray<FLane> Lanes;
		TArray<FWallRun> Walls;
		FReport Report;
		/** Grille de chaussee (ruelles + placette), pour defricher : origine, pas, dimensions, cases pavees. */
		FVector2D GridOrigin = FVector2D::ZeroVector;
		double GridCm = 100.0;
		int32 GridW = 0;
		int32 GridH = 0;
		TBitArray<> Paved;

		bool IsPavedAt(double X, double Y) const;
	};

	/** Hauteur du sol rendu (Z, cm) sous un point monde (X, Y). Faux si rien sous le point. */
	using FHeightFn = TFunctionRef<bool(double X, double Y, double& OutZ)>;

	/** Le tissu. `CellCm` = taille d'une case de simulation a l'ecran (TileWorldSize * echelle). */
	FFabric Build(TArray<FPlot> Plots, double CellCm, FHeightFn Height, const FParams& Params = FParams());

	/** Hachage stable d'une chaine (FNV-1a), independant de la plate-forme et de la session. */
	uint32 StableHash(const FString& Text);

	/** Une ligne de journal `ANASTASIS_FABRIC report ...`, et le meme rapport en JSON pour les preuves. */
	FString ToLogLine(const FReport& Report);
	FString ToJson(const FFabric& Fabric);
}
