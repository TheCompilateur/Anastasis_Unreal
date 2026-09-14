#include "WorldView/AnastasisPresentationResolver.h"

#include "Anastasis_UnrealV2.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/StrongObjectPtr.h"
#include "WorldView/AnastasisPresentationRegistry.h"
#include "WorldView/AnastasisWorldView.h"

namespace AnastasisPresentation
{
namespace
{
	/**
	 * Small stable integer hash, local to presentation. Deliberately NOT AnastasisRng /
	 * AnastasisWorldNoise (those are AnastasisSim-owned); the presentation layer must not
	 * reach into simulation internals for jitter that has no bearing on simulation truth.
	 */
	uint32 HashTile(uint32 Seed, int32 TileX, int32 TileY, uint32 Salt)
	{
		uint32 H = Seed ^ 0x9E3779B9u;
		H = (H ^ static_cast<uint32>(TileX)) * 0x85EBCA6Bu;
		H = (H ^ static_cast<uint32>(TileY)) * 0xC2B2AE35u;
		H = (H ^ Salt) * 0x27D4EB2Fu;
		H ^= H >> 15;
		return H;
	}

	double UnitFloat(uint32 H)
	{
		return static_cast<double>(H) / static_cast<double>(MAX_uint32);
	}

	// Kept alive explicitly: the registry is either an asset we did not create or a
	// transient fallback object, and nothing else in the actor graph roots it.
	TStrongObjectPtr<UAnastasisPresentationRegistry> GCachedRegistry;
	bool GRegistryIsDataDriven = false;
}

void InvalidateRegistryCache()
{
	GCachedRegistry.Reset();
	GRegistryIsDataDriven = false;
}

const UAnastasisPresentationRegistry& GetRegistry()
{
	if (GCachedRegistry.IsValid())
	{
		return *GCachedRegistry.Get();
	}

	UAnastasisPresentationRegistry* Loaded = LoadObject<UAnastasisPresentationRegistry>(nullptr, RegistryAssetPath);
	if (Loaded && Loaded->Entries.Num() > 0)
	{
		GCachedRegistry.Reset(Loaded);
		GRegistryIsDataDriven = true;
		UE_LOG(LogAnastasis_UnrealV2, Display,
			TEXT("ANASTASIS_PRESENTATION_REGISTRY source=asset path=%s entries=%d"),
			RegistryAssetPath, Loaded->Entries.Num());
		return *GCachedRegistry.Get();
	}

	// FAIL-CLOSED, LOUDLY: no asset, or an empty one, degrades to the known-good
	// VISUAL_BUILD_001 look instead of an empty world or a null dereference.
	UE_LOG(LogAnastasis_UnrealV2, Warning,
		TEXT("ANASTASIS_PRESENTATION_REGISTRY source=code_defaults reason=%s path=%s"),
		Loaded ? TEXT("asset_has_no_entries") : TEXT("asset_not_found"), RegistryAssetPath);
	GCachedRegistry.Reset(UAnastasisPresentationRegistry::CreateCodeDefaults(GetTransientPackage()));
	GRegistryIsDataDriven = false;
	return *GCachedRegistry.Get();
}

bool IsRegistryDataDriven()
{
	GetRegistry();
	return GRegistryIsDataDriven;
}

const FAnastasisPresentationEntry* FindEntry(AnastasisWorld::ETileType Type)
{
	return GetRegistry().FindEntry(Type);
}

int32 SelectVariantIndex(
	const FAnastasisPresentationEntry& Entry,
	uint32 Seed,
	int32 TileX,
	int32 TileY,
	EAnastasisStatureClass Wanted)
{
	// Only variants that actually name a mesh are eligible: a half-filled row in the data
	// asset must not produce an invisible "chosen" variant.
	TArray<int32, TInlineAllocator<8>> Eligible;
	TArray<int32, TInlineAllocator<8>> Matching;
	for (int32 Index = 0; Index < Entry.Variants.Num(); ++Index)
	{
		if (Entry.Variants[Index].Mesh.IsNull())
		{
			continue;
		}
		Eligible.Add(Index);

		// Any on either side means "no opinion": an untagged variant serves every request,
		// and an Any request takes whatever the data offers. That is what keeps a registry
		// written before this axis existed rendering exactly as it did.
		const EAnastasisStatureClass Tag = Entry.Variants[Index].Stature;
		if (Wanted == EAnastasisStatureClass::Any || Tag == EAnastasisStatureClass::Any || Tag == Wanted)
		{
			Matching.Add(Index);
		}
	}
	if (Eligible.Num() == 0)
	{
		return INDEX_NONE;
	}

	// FAIL OPEN. If the art has no look for this stature yet, draw the archetype anyway with
	// whatever exists. Presence is simulation truth; stature is only how it is dressed, and a
	// missing dress must not delete the tree.
	const TArray<int32, TInlineAllocator<8>>& Pool = Matching.Num() > 0 ? Matching : Eligible;

	// The stature participates in the hash: two statures on the same tile must be free to
	// land on different variants, which a (Seed, X, Y) hash alone could not express.
	const uint32 H = HashTile(Seed, TileX, TileY, 0x5u + static_cast<uint32>(Wanted) * 0x9E37u);
	return Pool[H % static_cast<uint32>(Pool.Num())];
}

bool ResolvePresentation(
	AnastasisWorld::ETileType Type,
	uint32 Seed,
	int32 TileX,
	int32 TileY,
	FResolvedPresentation& Out,
	EAnastasisStatureClass Wanted)
{
	Out = FResolvedPresentation{};

	const FAnastasisPresentationEntry* Entry = FindEntry(Type);
	if (!Entry)
	{
		return false;
	}

	const int32 VariantIndex = SelectVariantIndex(*Entry, Seed, TileX, TileY, Wanted);
	if (VariantIndex == INDEX_NONE)
	{
		return false;
	}

	const FAnastasisPresentationVariant& Variant = Entry->Variants[VariantIndex];
	UStaticMesh* Mesh = Variant.Mesh.LoadSynchronous();
	if (!Mesh)
	{
		// The row names a mesh that will not load. Omit this archetype rather than render a
		// hole or crash — and say which one, once per resolve attempt.
		UE_LOG(LogAnastasis_UnrealV2, Warning,
			TEXT("ANASTASIS_PRESENTATION_MISSING_MESH archetype=%s variant=%d path=%s"),
			*Entry->ArchetypeId.ToString(), VariantIndex, *Variant.Mesh.ToString());
		return false;
	}

	Out.Entry = Entry;
	Out.VariantIndex = VariantIndex;
	Out.Mesh = Mesh;
	Out.MaterialOverride = Variant.MaterialOverride.IsNull() ? nullptr : Variant.MaterialOverride.LoadSynchronous();
	Out.ScaleBias = FMath::IsFinite(Variant.ScaleBias) && Variant.ScaleBias > 0.0f ? Variant.ScaleBias : 1.0f;
	return true;
}

FTransform ResolveInstanceTransform(
	const FAnastasisPresentationEntry& Entry,
	uint32 Seed,
	int32 TileX,
	int32 TileY,
	double Alt,
	float ScaleBias)
{
	const uint32 HX = HashTile(Seed, TileX, TileY, 0x1u);
	const uint32 HY = HashTile(Seed, TileX, TileY, 0x2u);
	const uint32 HYaw = HashTile(Seed, TileX, TileY, 0x3u);
	const uint32 HScale = HashTile(Seed, TileX, TileY, 0x4u);
	const uint32 HLean = HashTile(Seed, TileX, TileY, 0x6u);
	const uint32 HLeanDir = HashTile(Seed, TileX, TileY, 0x7u);

	const double MinScale = static_cast<double>(Entry.MinUniformScale);
	const double MaxScale = static_cast<double>(Entry.MaxUniformScale);
	const double JitterRadius = static_cast<double>(Entry.JitterRadiusFraction) * AnastasisWorldView::TileWorldSize;
	const double Bias = FMath::IsFinite(ScaleBias) && ScaleBias > 0.0f ? static_cast<double>(ScaleBias) : 1.0;
	const double Scale = FMath::Lerp(MinScale, MaxScale, UnitFloat(HScale)) * Bias;

	FVector Location = AnastasisWorldView::TileToUnreal(TileX, TileY, Alt);
	Location.X += (UnitFloat(HX) * 2.0 - 1.0) * JitterRadius;
	Location.Y += (UnitFloat(HY) * 2.0 - 1.0) * JitterRadius;
	// Engine BasicShapes are centre-pivoted: lift by half the scaled bounding height so the
	// instance's base sits at Alt instead of clipping half-buried into the ground.
	Location.Z += 0.5 * EngineBasicShapeSize * Scale;

	const double Yaw = Entry.bRandomYaw ? UnitFloat(HYaw) * 360.0 : 0.0;

	// The tilt is squared before it is applied: most instances stay near plumb and only a
	// few lean far. A uniform draw would give a whole stand the same drunken average, which
	// reads as noise rather than as individuals.
	const double LeanUnit = UnitFloat(HLean);
	const double Lean = static_cast<double>(FMath::Max(0.0f, Entry.MaxLeanDegrees)) * LeanUnit * LeanUnit;
	const double LeanDir = UnitFloat(HLeanDir) * 360.0;
	const FQuat Tilt(FVector(FMath::Cos(FMath::DegreesToRadians(LeanDir)),
			FMath::Sin(FMath::DegreesToRadians(LeanDir)), 0.0),
		FMath::DegreesToRadians(Lean));
	const FQuat Turn = FRotator(0.0, Yaw, 0.0).Quaternion();

	return FTransform(Lean > 0.0 ? FRotator(Tilt * Turn) : FRotator(0.0, Yaw, 0.0), Location, FVector(Scale));
}
}
