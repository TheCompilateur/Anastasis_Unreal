#pragma once

#include "CoreMinimal.h"
#include "WorldView/AnastasisMicroEcology.h"

class AActor;
class UHierarchicalInstancedStaticMeshComponent;

/** Sparse, visual-only micro life selected from the already measured bank plan. */
namespace AnastasisPonticWaterMicro
{
enum class EKind : uint8 { Horsetail, Coltsfoot, Frog, Count };
inline constexpr int32 KindCount = static_cast<int32>(EKind::Count);

struct FPlacement
{
	FVector Ground = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	double Yaw = 0.0;
	double Scale = 1.0;
	double AboveWater = 0.0;
	EKind Kind = EKind::Horsetail;
};

struct FPlan
{
	TArray<FPlacement> Instances;
	int32 Counts[KindCount] = {};
};

/** Does not modify the source ecology plan, ground, water, or NPC state. */
void Build(const AnastasisMicroEcology::FPlan& Bank,
	const AnastasisMicroEcology::FInputs& Habitat, uint32 Seed, FPlan& Out);

struct FEmbodyResult
{
	int32 Instances = 0;
	int32 Components = 0;
	int32 MissingMeshes = 0;
};

/** Clears and reuses transient HISM components across incarnations. */
FEmbodyResult Embody(AActor& Owner, const FPlan* Plan,
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& InOutComponents);
}
