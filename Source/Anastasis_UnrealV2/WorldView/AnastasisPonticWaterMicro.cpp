#include "WorldView/AnastasisPonticWaterMicro.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

namespace AnastasisPonticWaterMicro
{
namespace
{
static TAutoConsoleVariable<int32> CVarHorsetailCandidate(
	TEXT("anastasis.Dressing.PonticHorsetailCandidate"), 0,
	TEXT("0: original horsetail mesh; 1: V2 candidate at identical placements."));

uint32 Mix(uint32 Seed, int32 X, int32 Y, uint32 Salt)
{
	uint32 H = Seed ^ Salt ^ 0x9e3779b9u;
	H = (H ^ static_cast<uint32>(X)) * 0x85ebca6bu;
	H = (H ^ static_cast<uint32>(Y)) * 0xc2b2ae35u;
	return H ^ (H >> 16);
}

double Unit(uint32 H) { return static_cast<double>(H) / 4294967296.0; }

const TCHAR* Name(EKind Kind)
{
	switch (Kind)
	{
	case EKind::Horsetail: return TEXT("Horsetail");
	case EKind::Coltsfoot: return TEXT("Coltsfoot");
	case EKind::Frog: return TEXT("Frog");
	default: return TEXT("Unknown");
	}
}

const TCHAR* MeshPath(EKind Kind)
{
	switch (Kind)
	{
	case EKind::Horsetail: return CVarHorsetailCandidate.GetValueOnGameThread() != 0
		? TEXT("/Game/Anastasis/PonticMicro/SM_Pontic_Horsetail_02.SM_Pontic_Horsetail_02")
		: TEXT("/Game/Anastasis/PonticMicro/SM_Pontic_Horsetail_01.SM_Pontic_Horsetail_01");
	case EKind::Coltsfoot: return TEXT("/Game/Anastasis/PonticMicro/SM_Pontic_Coltsfoot_01.SM_Pontic_Coltsfoot_01");
	case EKind::Frog: return TEXT("/Game/Anastasis/PonticMicro/SM_Pontic_Frog_01.SM_Pontic_Frog_01");
	default: return TEXT("");
	}
}
}

void Build(const AnastasisMicroEcology::FPlan& Bank,
	const AnastasisMicroEcology::FInputs& Habitat, uint32 Seed, FPlan& Out)
{
	Out = FPlan();
	if (!Habitat.SampleHeight || !Habitat.SampleWaterHeight) return;
	using AnastasisMicroEcology::ERole;
	using AnastasisMicroEcology::EPocket;
	constexpr double CellUU = 200.0;
	TMap<FIntPoint, TArray<FVector2D>> ExistingBankProps;
	for (const AnastasisMicroEcology::FPlacement& Source : Bank.Instances)
	{
		if (Source.Role > ERole::BankBranch) continue;
		const FVector2D XY(Source.Ground.X, Source.Ground.Y);
		ExistingBankProps.FindOrAdd(FIntPoint(FMath::FloorToInt(XY.X / CellUU),
			FMath::FloorToInt(XY.Y / CellUU))).Add(XY);
	}
	for (const AnastasisMicroEcology::FPlacement& Source : Bank.Instances)
	{
		// Every candidate is already on sampled, rendered ground and above sampled water.
		if (!(Source.AboveWater >= 8.0 && Source.AboveWater <= 220.0)
			|| !FMath::IsFinite(Source.Ground.X) || !FMath::IsFinite(Source.Ground.Y)) continue;
		const int32 X = FMath::FloorToInt(Source.Ground.X / 100.0);
		const int32 Y = FMath::FloorToInt(Source.Ground.Y / 100.0);
		EKind Kind;
		if (Source.Role == ERole::BankReed && Source.Pocket == EPocket::Vegetated
			&& Source.SlopeDegrees <= 20.0 && Unit(Mix(Seed, X, Y, 0x1101u)) < 0.55)
		{
			Kind = EKind::Horsetail;
		}
		else if (Source.Role == ERole::BankPebble && Source.Pocket == EPocket::Rocky
			&& Source.SlopeDegrees <= 22.0 && Unit(Mix(Seed, X, Y, 0x1102u)) < 0.12)
		{
			// Small raw-earth pockets between stones; this is an ecological analogy.
			Kind = EKind::Coltsfoot;
		}
		else if (Source.Role == ERole::BankTuft
			&& (Source.Pocket == EPocket::Muddy || Source.Pocket == EPocket::Vegetated)
			&& Source.SlopeDegrees <= 16.0 && Source.AboveWater <= 90.0
			&& Unit(Mix(Seed, X, Y, 0x1103u)) < 0.10)
		{
			Kind = EKind::Frog;
		}
		else continue;
		FPlacement P;
		// The selected bank prop itself occupies Source.Ground. Offset to a nearby
		// measured patch so a reed, rock, or shore tuft does not cover this asset.
		bool bFound = false;
		for (int32 Attempt = 0; Attempt < 4; ++Attempt)
		{
			const double Angle = (Unit(Mix(Seed, X, Y, 0x1112u)) + Attempt * 0.25) * 2.0 * PI;
			const FVector2D XY(Source.Ground.X + 135.0 * FMath::Cos(Angle),
				Source.Ground.Y + 135.0 * FMath::Sin(Angle));
			const FIntPoint Cell(FMath::FloorToInt(XY.X / CellUU), FMath::FloorToInt(XY.Y / CellUU));
			bool bOccupied = false;
			for (int32 DX = -1; DX <= 1 && !bOccupied; ++DX)
			{
				for (int32 DY = -1; DY <= 1 && !bOccupied; ++DY)
				{
					if (const TArray<FVector2D>* Props = ExistingBankProps.Find(Cell + FIntPoint(DX, DY)))
					{
						for (const FVector2D& Prop : *Props)
						{
							if (FVector2D::DistSquared(XY, Prop) < 85.0 * 85.0) { bOccupied = true; break; }
						}
					}
				}
			}
			if (bOccupied) continue;
			double GroundZ = 0.0, WaterZ = 0.0, Zx = 0.0, Zy = 0.0;
			if (!Habitat.SampleHeight(XY.X, XY.Y, GroundZ)
				|| !Habitat.SampleWaterHeight(XY.X, XY.Y, WaterZ)
				|| !Habitat.SampleHeight(XY.X + 40.0, XY.Y, Zx)
				|| !Habitat.SampleHeight(XY.X, XY.Y + 40.0, Zy)) continue;
			const double Above = GroundZ - WaterZ;
			if (Above < 8.0 || Above > (Kind == EKind::Frog ? 90.0 : 220.0)) continue;
			const FVector RawNormal = FVector::CrossProduct(FVector(0, 40, Zy - GroundZ),
				FVector(40, 0, Zx - GroundZ)).GetSafeNormal();
			const FVector Normal = RawNormal.Z < 0.0 ? -RawNormal : RawNormal;
			const double Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Normal.Z, -1.0, 1.0)));
			if (Slope > (Kind == EKind::Frog ? 16.0 : Kind == EKind::Horsetail ? 20.0 : 22.0)) continue;
			P.Ground = FVector(XY.X, XY.Y, GroundZ);
			P.Normal = Normal;
			P.AboveWater = Above;
			bFound = true;
			break;
		}
		if (!bFound) continue;
		P.Yaw = Unit(Mix(Seed, X, Y, 0x1110u)) * 360.0;
		P.Scale = 0.82 + 0.35 * Unit(Mix(Seed, X, Y, 0x1111u));
		P.Kind = Kind;
		Out.Instances.Add(P);
		++Out.Counts[static_cast<int32>(Kind)];
	}
	// A resting frog needs a readable bare patch, rather than a new horsetail
	// growing through its silhouette. Keep the animal and reject nearby new flora.
	TArray<FVector2D> Frogs;
	for (const FPlacement& P : Out.Instances)
	{
		if (P.Kind == EKind::Frog) Frogs.Add(FVector2D(P.Ground.X, P.Ground.Y));
	}
	Out.Instances.RemoveAll([&Frogs](const FPlacement& P)
	{
		if (P.Kind == EKind::Frog) return false;
		const FVector2D XY(P.Ground.X, P.Ground.Y);
		for (const FVector2D& Frog : Frogs)
		{
			if (FVector2D::DistSquared(XY, Frog) < 150.0 * 150.0) return true;
		}
		return false;
	});
	for (int32 I = 0; I < KindCount; ++I) Out.Counts[I] = 0;
	for (const FPlacement& P : Out.Instances) ++Out.Counts[static_cast<int32>(P.Kind)];
}

FEmbodyResult Embody(AActor& Owner, const FPlan* Plan,
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& InOutComponents)
{
	TMap<FName, UHierarchicalInstancedStaticMeshComponent*> Existing;
	for (UHierarchicalInstancedStaticMeshComponent* Component : InOutComponents)
	{
		if (!IsValid(Component)) continue;
		Component->ClearInstances();
		Component->MarkRenderStateDirty();
		// Unreal may suffix the UObject name. The logical tile key is stable.
		for (const FName& Tag : Component->ComponentTags)
		{
			if (Tag.ToString().StartsWith(TEXT("MicroEco_bank_Pontic_"))) Existing.Add(Tag, Component);
		}
	}
	InOutComponents.RemoveAll([](const TObjectPtr<UHierarchicalInstancedStaticMeshComponent>& C) { return !IsValid(C); });
	FEmbodyResult Result;
	if (!Plan) return Result;

	UStaticMesh* Meshes[KindCount] = {};
	for (int32 I = 0; I < KindCount; ++I)
	{
		Meshes[I] = LoadObject<UStaticMesh>(nullptr, MeshPath(static_cast<EKind>(I)));
		if (!Meshes[I]) ++Result.MissingMeshes;
	}
	constexpr double ChunkUU = 8000.0;
	TMap<FName, TArray<FTransform>> Batches;
	TMap<FName, int32> Kinds;
	for (const FPlacement& P : Plan->Instances)
	{
		const int32 Kind = static_cast<int32>(P.Kind);
		if (!Meshes[Kind]) continue;
		const int32 CX = FMath::FloorToInt(P.Ground.X / ChunkUU);
		const int32 CY = FMath::FloorToInt(P.Ground.Y / ChunkUU);
		const FName ComponentName(*FString::Printf(TEXT("MicroEco_bank_Pontic_%s_%d_%d"), Name(P.Kind), CX, CY));
		const FVector Up = FMath::Lerp(FVector::UpVector, P.Normal, P.Kind == EKind::Frog ? 0.8 : 0.45).GetSafeNormal();
		const FQuat Rotation = FQuat::FindBetweenNormals(FVector::UpVector, Up)
			* FQuat(FVector::UpVector, FMath::DegreesToRadians(P.Yaw));
		const double Sink = P.Kind == EKind::Frog ? 0.8 : 2.0;
		Batches.FindOrAdd(ComponentName).Add(FTransform(Rotation, P.Ground - FVector(0, 0, Sink), FVector(P.Scale)));
		Kinds.FindOrAdd(ComponentName) = Kind;
	}
	for (TPair<FName, TArray<FTransform>>& Batch : Batches)
	{
		const int32 Kind = Kinds.FindChecked(Batch.Key);
		UHierarchicalInstancedStaticMeshComponent* Component = Existing.FindRef(Batch.Key);
		if (!Component)
		{
			Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(&Owner,
				MakeUniqueObjectName(&Owner, UHierarchicalInstancedStaticMeshComponent::StaticClass(), Batch.Key));
			Component->SetFlags(RF_Transient);
			Component->SetupAttachment(Owner.GetRootComponent());
			Component->SetMobility(EComponentMobility::Movable);
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetGenerateOverlapEvents(false);
			Component->SetCanEverAffectNavigation(false);
			Component->ComponentTags.AddUnique(Batch.Key);
			Component->RegisterComponent();
			InOutComponents.Add(Component);
		}
		Component->SetStaticMesh(Meshes[Kind]);
		Component->SetCastShadow(false);
		const int32 CullEnd = Kind == static_cast<int32>(EKind::Frog) ? 2000 : 3500;
		Component->SetCullDistances(CullEnd * 3 / 4, CullEnd);
		Component->LDMaxDrawDistance = static_cast<float>(CullEnd + ChunkUU * 0.71);
		Component->SetCachedMaxDrawDistance(Component->LDMaxDrawDistance);
		Component->AddInstances(Batch.Value, false, false, false);
		Component->MarkRenderStateDirty();
		Result.Instances += Batch.Value.Num();
		++Result.Components;
	}
	return Result;
}
}
