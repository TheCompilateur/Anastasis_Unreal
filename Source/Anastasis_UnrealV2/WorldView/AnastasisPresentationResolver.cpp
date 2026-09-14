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

	/**
	 * The species gradient, as four numbers, set from the MEASURED distribution of the
	 * canonical world (seed 12345) over its 438 dressed forest sites:
	 *
	 *     Shade    min -0.74   median -0.04   max 0.79
	 *     Wetness  min  0.00   median  0.08   max 0.85
	 *
	 * Wetness is strongly skewed towards dry: its median is 0.08, not the 0.5 a [0,1] field
	 * invites one to assume. A first cut pivoted at 0.55 therefore added a constant bonus to
	 * nearly every site and produced 366 conifers against 72 broadleaves, with
	 * p_conifer saturating at BOTH ends -- a threshold wearing the disguise of a gradient.
	 * The pivot is the measured median, so moisture is neutral at a typical site and only
	 * speaks where it is genuinely unusual.
	 *
	 * The weights are then sized against the range REAL SITES span, verified by the
	 * p_conifer field of ANASTASIS_TREE_SPECIES: no site clamps at 0 or 1, so every tree on
	 * the map sits somewhere on the gradient rather than on one of its walls. An earlier
	 * pass left the wettest, darkest sites pinned at exactly 0.00 -- a clamp is a threshold,
	 * even when only a corner of the map reaches it.
	 */
	constexpr double ConiferBase = 0.60;
	constexpr double ConiferShadeWeight = 0.37;
	constexpr double ConiferWetWeight = 0.34;
	constexpr double ConiferWetPivot = 0.08;

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
	EAnastasisStatureClass Wanted,
	EAnastasisFoliageFamily Family)
{
	// Only variants that actually name a mesh are eligible: a half-filled row in the data
	// asset must not produce an invisible "chosen" variant.
	//
	// Three nested pools, because the two axes do not degrade equally. Losing the species
	// is a smaller lie than losing the age: a beech stand drawn as spruce still reads as a
	// forest of the right shape, while a sapling drawn as a dominant breaks the stand's
	// whole vertical profile. So the family is what gets dropped first.
	TArray<int32, TInlineAllocator<12>> Eligible;
	TArray<int32, TInlineAllocator<12>> StatureOnly;
	TArray<int32, TInlineAllocator<12>> Both;
	for (int32 Index = 0; Index < Entry.Variants.Num(); ++Index)
	{
		const FAnastasisPresentationVariant& Variant = Entry.Variants[Index];
		if (Variant.Mesh.IsNull())
		{
			continue;
		}
		Eligible.Add(Index);

		// Any on either side means "no opinion": an untagged variant serves every request,
		// and an Any request takes whatever the data offers. That is what keeps a registry
		// written before these axes existed rendering exactly as it did.
		const bool bStatureFits = Wanted == EAnastasisStatureClass::Any
			|| Variant.Stature == EAnastasisStatureClass::Any
			|| Variant.Stature == Wanted;
		const bool bFamilyFits = Family == EAnastasisFoliageFamily::Any
			|| Variant.Family == EAnastasisFoliageFamily::Any
			|| Variant.Family == Family;
		if (bStatureFits)
		{
			StatureOnly.Add(Index);
			if (bFamilyFits)
			{
				Both.Add(Index);
			}
		}
	}
	if (Eligible.Num() == 0)
	{
		return INDEX_NONE;
	}

	// FAIL OPEN, in that order. A missing art asset must degrade the look, never the
	// presence of the tree: presence is simulation truth, stature and species are dress.
	const TArray<int32, TInlineAllocator<12>>& Pool =
		Both.Num() > 0 ? Both : (StatureOnly.Num() > 0 ? StatureOnly : Eligible);

	// Both axes participate in the hash: two different requests on the same tile must be
	// free to land on different variants, which a (Seed, X, Y) hash alone could not express.
	const uint32 Salt = 0x5u + static_cast<uint32>(Wanted) * 0x9E37u
		+ static_cast<uint32>(Family) * 0x85EBu;
	const uint32 H = HashTile(Seed, TileX, TileY, Salt);
	return Pool[H % static_cast<uint32>(Pool.Num())];
}

double Coniferousness(double Shade, double Wetness)
{
	if (!FMath::IsFinite(Shade) || !FMath::IsFinite(Wetness))
	{
		return ConiferBase;
	}
	// Shade rises with altitude and with facing the light; Wetness with moisture. Conifers
	// climb and take the exposed ground, broadleaves hold the damp and the sheltered.
	const double Value = ConiferBase
		+ ConiferShadeWeight * FMath::Clamp(Shade, -1.0, 1.0)
		- ConiferWetWeight * (FMath::Clamp(Wetness, 0.0, 1.0) - ConiferWetPivot);
	return FMath::Clamp(Value, 0.0, 1.0);
}

EAnastasisFoliageFamily SelectFoliageFamily(
	double Shade,
	double Wetness,
	uint32 Seed,
	int32 TileX,
	int32 TileY)
{
	const double P = Coniferousness(Shade, Wetness);
	return UnitFloat(HashTile(Seed, TileX, TileY, 0x8u)) < P
		? EAnastasisFoliageFamily::Conifer
		: EAnastasisFoliageFamily::Broadleaf;
}

bool ResolvePresentation(
	AnastasisWorld::ETileType Type,
	uint32 Seed,
	int32 TileX,
	int32 TileY,
	FResolvedPresentation& Out,
	EAnastasisStatureClass Wanted,
	EAnastasisFoliageFamily Family)
{
	Out = FResolvedPresentation{};

	const FAnastasisPresentationEntry* Entry = FindEntry(Type);
	if (!Entry)
	{
		return false;
	}

	const int32 VariantIndex = SelectVariantIndex(*Entry, Seed, TileX, TileY, Wanted, Family);
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
	Out.AdditionalMaterials.Reset(Variant.AdditionalMaterialOverrides.Num());
	for (const TSoftObjectPtr<UMaterialInterface>& Slot : Variant.AdditionalMaterialOverrides)
	{
		// A null entry is kept, not skipped: the index IS the slot number, so dropping one
		// would silently shift every material after it onto the wrong part of the mesh.
		Out.AdditionalMaterials.Add(Slot.IsNull() ? nullptr : Slot.LoadSynchronous());
	}
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
