#include "Village/AnastasisFieldGrainVisual.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Village/AnastasisVillage.h"
#include "Village/AnastasisVillagePresentation.h"
#include "World/AnastasisWorld.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisWorldView.h"
#include "EngineUtils.h"

namespace
{
	TAutoConsoleVariable<int32> CVarGrainFields(TEXT("anastasis.Village.GrainFields"), 1,
		TEXT("Show grain clumps on live grain field tiles."), ECVF_Default);
	TAutoConsoleVariable<int32> CVarGrainFieldsAutomation(TEXT("anastasis.Village.GrainFields.InAutomation"), 0,
		TEXT("Allow grain field presentation during automation tests."), ECVF_Default);
	constexpr int32 ChunkSide = 8;
	constexpr int32 SlotsPerAxis = 12;
	constexpr int32 SlotsPerTile = SlotsPerAxis * SlotsPerAxis;
	constexpr int32 MaximumAmount = 37; // World generation's field range is 11..37.
	constexpr TCHAR GrainMeshPath[] = TEXT("/Game/Anastasis/FieldGrain/SM_Field_GrainClump_01.SM_Field_GrainClump_01");

	int32 Density(const AnastasisWorld::FTile& Tile)
	{
		if (Tile.Type != AnastasisWorld::ETileType::Field || Tile.Resource != AnastasisWorld::EResource::Food
			|| Tile.CropId != AnastasisWorld::ECropId::Grain || Tile.Amount <= 0) return 0;
		return FMath::Clamp(FMath::RoundToInt(static_cast<double>(Tile.Amount) * SlotsPerTile / MaximumAmount), 1, SlotsPerTile);
	}
}

AAnastasisFieldGrainVisual::AAnastasisFieldGrainVisual()
{
	PrimaryActorTick.bCanEverTick = true;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FieldGrainRoot"));
	RootComponent = SceneRoot;
	SetActorEnableCollision(false);
}

void AAnastasisFieldGrainVisual::Reset()
{
	for (auto& Pair : Chunks) if (IsValid(Pair.Value)) Pair.Value->DestroyComponent();
	Chunks.Reset();
	Densities.Reset();
	DirtyChunks.Reset();
	Width = Height = ChunkColumns = 0;
	PollAccumulator = 0.0f;
}

int32 AAnastasisFieldGrainVisual::GetTileClumps(int32 TileX, int32 TileY) const
{
	const int32 Index = TileY * Width + TileX;
	return TileX >= 0 && TileY >= 0 && TileX < Width && TileY < Height && Densities.IsValidIndex(Index)
		? Densities[Index] : 0;
}

FVector AAnastasisFieldGrainVisual::GetFullestField() const
{
	int32 Best = INDEX_NONE;
	for (int32 I = 0; I < Densities.Num(); ++I)
		if (Best == INDEX_NONE || Densities[I] > Densities[Best]) Best = I;
	return Best != INDEX_NONE && Densities[Best] > 0
		? FVector(Best % Width, Best / Width, Densities[Best]) : FVector(-1, -1, 0);
}

int32 AAnastasisFieldGrainVisual::GetTotalClumps() const
{
	int32 Total = 0;
	for (int32 D : Densities) Total += D;
	return Total;
}

int32 AAnastasisFieldGrainVisual::GetRenderedClumps() const
{
	int32 Total = 0;
	for (const auto& Pair : Chunks) if (IsValid(Pair.Value)) Total += Pair.Value->GetInstanceCount();
	return Total;
}

void AAnastasisFieldGrainVisual::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const bool bEnabled = CVarGrainFields.GetValueOnGameThread() != 0
		&& (!GIsAutomationTesting || CVarGrainFieldsAutomation.GetValueOnGameThread() != 0);
	SetActorHiddenInGame(!bEnabled);
	if (!bEnabled) return;

	UAnastasisSimulationSubsystem* Host = GetWorld()->GetSubsystem<UAnastasisSimulationSubsystem>();
	if (!Host || !Host->GetSimulation().IsRunning()) return;
	// The simulation begins before terrain construction. Wait for a rendered footprint.
	bool bTerrainReady = false;
	for (TActorIterator<AAnastasisWorldEmbodiment> It(GetWorld()); It; ++It)
		if (It->GetSnapshot().SpatialScale > 0.0 && It->GetActiveFootprintBounds().IsValid) { bTerrainReady = true; break; }
	if (!bTerrainReady) return;

	PollAccumulator += DeltaSeconds;
	if (Densities.IsEmpty() || PollAccumulator >= 0.5f)
	{
		PollAccumulator = 0.0f;
		Poll(Host->GetSimulation().GetVillage(), Host->GetSimulation().GetWorld());
	}
	if (!DirtyChunks.IsEmpty())
	{
		// Spread initial mesh/ground work across frames. A harvest dirties only its own 8x8-tile chunk.
		const int32 Chunk = *DirtyChunks.CreateConstIterator();
		DirtyChunks.Remove(Chunk);
		RebuildChunk(Chunk, Host->GetSimulation().GetWorld());
	}
}

void AAnastasisFieldGrainVisual::Poll(const AnastasisVillage::FVillage& Village, const AnastasisWorld::FWorld& SimWorld)
{
	const AAnastasisWorldEmbodiment* Terrain = nullptr;
	for (TActorIterator<AAnastasisWorldEmbodiment> It(GetWorld()); It; ++It) { Terrain = *It; break; }
	if (!Terrain) return;
	const auto& Crop = Terrain->GetSnapshot();
	if (Width != SimWorld.W || Height != SimWorld.H || Densities.Num() != SimWorld.Tiles.Num())
	{
		Reset();
		Width = SimWorld.W;
		Height = SimWorld.H;
		ChunkColumns = FMath::DivideAndRoundUp(Width, ChunkSide);
		Densities.Init(0, SimWorld.Tiles.Num());
	}
	for (int32 I = 0; I < Densities.Num(); ++I)
	{
		// Most non-field tiles can be skipped without asking the village for its live override.
		const bool bWasGrain = Densities[I] > 0;
		const auto& Base = SimWorld.Tiles[I];
		if (!bWasGrain && Base.Type != AnastasisWorld::ETileType::Field) continue;
		const int32 X = I % Width, Y = I / Width;
		const bool bRendered = X >= Crop.OriginX && Y >= Crop.OriginY && X < Crop.OriginX + Crop.W && Y < Crop.OriginY + Crop.H;
		const int32 NewDensity = bRendered ? Density(Village.LiveTileAt(X, Y)) : 0;
		if (NewDensity == Densities[I]) continue;
		Densities[I] = NewDensity;
		DirtyChunks.Add((Y / ChunkSide) * ChunkColumns + X / ChunkSide);
	}
}

void AAnastasisFieldGrainVisual::RebuildChunk(int32 Chunk, const AnastasisWorld::FWorld& SimWorld)
{
	UHierarchicalInstancedStaticMeshComponent* Component = Chunks.FindRef(Chunk);
	if (!Component)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, GrainMeshPath);
		if (!Mesh)
		{
			if (!bMissingMeshLogged) UE_LOG(LogTemp, Error, TEXT("FIELD_GRAIN missing mesh %s"), GrainMeshPath);
			bMissingMeshLogged = true;
			return;
		}
		Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient);
		Component->SetupAttachment(RootComponent);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCullDistances(10000, 14000);
		Component->InstancingRandomSeed = Chunk * 179 + 17;
		Component->RegisterComponent();
		Chunks.Add(Chunk, Component);
	}
	Component->ClearInstances();
	Component->bAutoRebuildTreeOnInstanceChanges = false;
	const int32 ChunkX = (Chunk % ChunkColumns) * ChunkSide;
	const int32 ChunkY = (Chunk / ChunkColumns) * ChunkSide;
	for (int32 Y = ChunkY; Y < FMath::Min(ChunkY + ChunkSide, Height); ++Y)
	for (int32 X = ChunkX; X < FMath::Min(ChunkX + ChunkSide, Width); ++X)
	{
		const int32 Count = Densities[Y * Width + X];
		for (int32 I = 0; I < Count; ++I)
		{
			const int32 Slot = (I * 17 + (X * 11 + Y * 7) % SlotsPerTile) % SlotsPerTile;
			const double U = (Slot % SlotsPerAxis + 0.5) / SlotsPerAxis;
			const double V = (Slot / SlotsPerAxis + 0.5) / SlotsPerAxis;
			const FVector Root = FAnastasisVillagePresentation::SimToUnreal(SimWorld, X + U, Y + V, GetWorld());
			const float Yaw = static_cast<float>((X * 73 + Y * 37 + Slot * 59) % 360);
			const float Scale = 0.88f + static_cast<float>((X * 3 + Y * 5 + Slot * 7) % 9) * 0.03f;
			Component->AddInstance(FTransform(FRotator(0, Yaw, 0), Root, FVector(Scale)), true);
		}
	}
	Component->bAutoRebuildTreeOnInstanceChanges = true;
	Component->BuildTreeIfOutdated(false, true);
}
