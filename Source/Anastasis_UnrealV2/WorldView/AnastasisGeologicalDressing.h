#pragma once

#include "CoreMinimal.h"
#include "WorldView/AnastasisWorldView.h"
#include "AnastasisGeologicalDressing.generated.h"

/**
 * LITHOS_FORGE — grammaire géologique de présentation.
 *
 * Ne possède pas la vérité de simulation (AnastasisWorld::Alt, ETileType).
 * Ne refait pas le Landscape / TerrainForge. Lit le relief déjà décidé et
 * pose des formations qui l'épousent : parois sur les ruptures, éboulis
 * à leurs pieds, affleurements sur les pentes, crêtes aux sommets.
 *
 * Les chemins de mesh restent hors de ce fichier — comme l'écologie forestière.
 */
USTRUCT(BlueprintType)
struct FAnastasisLithosDressingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Lithos")
	bool bEnabled = true;

	/** Pente au-delà de laquelle une tuile est une paroi, pas une pente. */
	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "12", ClampMax = "70"))
	float CliffMinDegrees = 26.0f;

	/** Pente à partir de laquelle un affleurement / une paroi inclinée est licite. */
	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "6", ClampMax = "40"))
	float SlopeMinDegrees = 14.0f;

	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "0", ClampMax = "1"))
	float CliffDensity = 0.62f;

	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "0", ClampMax = "1"))
	float SlopeDensity = 0.28f;

	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "0", ClampMax = "1"))
	float TalusDensity = 0.48f;

	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "0", ClampMax = "1"))
	float SummitDensity = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "1.2", ClampMax = "8"))
	float CliffSpacing = 2.6f;

	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "1.2", ClampMax = "8"))
	float SlopeSpacing = 3.2f;

	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "0.8", ClampMax = "6"))
	float TalusSpacing = 1.35f;

	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "3", ClampMax = "16"))
	float SummitSpacing = 6.0f;

	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "0", ClampMax = "40"))
	float WaterClearanceUU = 14.0f;

	UPROPERTY(EditAnywhere, Category = "Lithos", meta = (ClampMin = "8", ClampMax = "80"))
	float EmbedUU = 28.0f;
};

namespace AnastasisGeologicalDressing
{
enum class EKind : uint8
{
	VerticalWall = 0,
	InclinedWall = 1,
	Stratum = 2,
	Cornice = 3,
	Outcrop = 4,
	Fractured = 5,
	DetachedBlock = 6,
	TalusCluster = 7,
	Transition = 8,
	Summit = 9,
	COUNT = 10
};

inline constexpr int32 KindCount = static_cast<int32>(EKind::COUNT);

struct FPlacement
{
	int32 SourceIndex = INDEX_NONE;
	uint32 VisualSeed = 0;
	FVector Ground = FVector::ZeroVector;
	double ScaleMultiplier = 1.0;
	double SlopeDegrees = 0.0;
	double AspectYaw = 0.0;
	double PitchDegrees = 0.0;
	double EmbedUU = 0.0;
	EKind Kind = EKind::Outcrop;
};

struct FPlan
{
	TArray<FPlacement> Instances;
	int32 CliffCount = 0;
	int32 SlopeCount = 0;
	int32 TalusCount = 0;
	int32 SummitCount = 0;
	int32 RejectedWater = 0;
	int32 RejectedFlat = 0;
	int32 RejectedSpacing = 0;
};

/**
 * Plan déterministe. Snapshot 96x96 canonique requis.
 * Aucun UObject, aucun chemin de mesh, aucun RNG de simulation.
 */
bool Build(const AnastasisWorldView::FWorldVisualSnapshot& Source,
	const FAnastasisLithosDressingSettings& Settings, FPlan& Out, FString& OutError);

const TCHAR* KindName(EKind Kind);
float DefaultScaleMin(EKind Kind);
float DefaultScaleMax(EKind Kind);
}
