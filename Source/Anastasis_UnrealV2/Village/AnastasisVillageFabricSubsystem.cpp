#include "Village/AnastasisVillageFabricSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Village/AnastasisVillage.h"
#include "Village/AnastasisVillageFabricActor.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisWorldView.h"


static TAutoConsoleVariable<int32> CVarVillageFabric(
	TEXT("anastasis.Village.Fabric"),
	1,
	TEXT("VILLAGE_FABRIC_001: 1 = paved lanes, steps, well plaza and dry-stone terrace walls between the village buildings (default). 0 = none, grass given back."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageFabricClear(
	TEXT("anastasis.Village.FabricClear"),
	1,
	TEXT("VILLAGE_FABRIC_001: 1 = grass and understory instances standing on the paving are scaled to zero (given back when the fabric changes). 0 = left in place (A/B)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVillageFabricTree(
	TEXT("anastasis.Village.FabricTree"),
	1,
	TEXT("VILLAGE_FABRIC_001: 1 = a plane tree shades the well plaza (default). 0 = bare plaza."),
	ECVF_Default);

static FAutoConsoleCommandWithWorld CmdVillageFabricReport(
	TEXT("Anastasis.Village.FabricReport"),
	TEXT("VILLAGE_FABRIC_001: log the current village fabric (ANASTASIS_FABRIC report + JSON)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisVillageFabricSubsystem* Fabric = World ? World->GetSubsystem<UAnastasisVillageFabricSubsystem>() : nullptr)
		{
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_FABRIC status %s"), *Fabric->StatusJson());
		}
	}));

static FAutoConsoleCommandWithWorld CmdVillageFabricRebuild(
	TEXT("Anastasis.Village.FabricRebuild"),
	TEXT("VILLAGE_FABRIC_001: rebuild the fabric on the next tick, same village (determinism check)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisVillageFabricSubsystem* Fabric = World ? World->GetSubsystem<UAnastasisVillageFabricSubsystem>() : nullptr)
		{
			Fabric->Invalidate();
		}
	}));

namespace AnastasisVillageFabric
{
namespace
{
	/** Le sol rendu de ce monde : maillages proceduraux visibles et collisionnables du terrain. */
	struct FGround
	{
		TArray<UProceduralMeshComponent*> Surfaces;
		double Scale = 1.0;

		explicit FGround(UWorld* World)
		{
			for (TActorIterator<AAnastasisWorldEmbodiment> It(World); It; ++It)
			{
				Scale = It->GetSnapshot().SpatialScale;
				TArray<UProceduralMeshComponent*> All;
				It->GetComponents(All);
				for (UProceduralMeshComponent* Surface : All)
				{
					if (Surface->IsVisible() && Surface->IsCollisionEnabled())
					{
						Surfaces.Add(Surface);
					}
				}
				break;
			}
		}

		bool Trace(double X, double Y, double& OutZ) const
		{
			for (UProceduralMeshComponent* Surface : Surfaces)
			{
				const FBox Bounds = Surface->Bounds.GetBox();
				if (X < Bounds.Min.X || X > Bounds.Max.X || Y < Bounds.Min.Y || Y > Bounds.Max.Y)
				{
					continue;
				}
				FHitResult Hit;
				if (Surface->LineTraceComponent(Hit, FVector(X, Y, Bounds.Max.Z + 1000.0), FVector(X, Y, Bounds.Min.Z - 1000.0),
					FCollisionQueryParams(SCENE_QUERY_STAT(VillageFabricGround), true)))
				{
					OutZ = Hit.ImpactPoint.Z;
					return true;
				}
			}
			return false;
		}
	};

	TArray<FPlot> PlotsOf(const AnastasisVillage::FVillage& Village)
	{
		TArray<FPlot> Plots;
		for (const AnastasisVillage::FBuilding& B : Village.GetBuildings())
		{
			FPlot Plot;
			Plot.Id = B.Id;
			Plot.Type = B.Type;
			Plot.Cell = FIntPoint(FMath::FloorToInt32(B.X), FMath::FloorToInt32(B.Y));
			if (B.AccessPoints.Num() > 0)
			{
				Plot.Access = FVector2D(B.AccessPoints[0].X, B.AccessPoints[0].Y);
				Plot.bHasAccess = true;
			}
			Plots.Add(Plot);
		}
		return Plots;
	}

	/** Ce qui fait changer le tissu : les batiments (id, type, case, acces) et les reglages. */
	uint32 KeyOf(const TArray<FPlot>& Plots)
	{
		TArray<FString> Lines;
		for (const FPlot& P : Plots)
		{
			Lines.Add(FString::Printf(TEXT("%s|%s|%d|%d|%.2f|%.2f"), *P.Id, *P.Type, P.Cell.X, P.Cell.Y, P.Access.X, P.Access.Y));
		}
		Lines.Sort();
		Lines.Add(FString::Printf(TEXT("clear=%d tree=%d"), CVarVillageFabricClear.GetValueOnGameThread(), CVarVillageFabricTree.GetValueOnGameThread()));
		return StableHash(FString::Join(Lines, TEXT(";"))) | 1u; // jamais 0 : 0 = a refaire
	}
}
}

bool UAnastasisVillageFabricSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UAnastasisVillageFabricSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAnastasisVillageFabricSubsystem, STATGROUP_Tickables);
}

void UAnastasisVillageFabricSubsystem::Deinitialize()
{
	TearDown();
	Super::Deinitialize();
}

void UAnastasisVillageFabricSubsystem::TearDown()
{
	using namespace AnastasisVillageFabric;
	if (AAnastasisVillageFabric* Existing = Actor.Get())
	{
		const int32 Restored = Existing->RestoreCleared();
		Existing->Destroy();
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_FABRIC teardown restored=%d"), Restored);
	}
	Actor.Reset();
	Last = FFabric();
	LastKey = 0;
}

void UAnastasisVillageFabricSubsystem::Tick(float DeltaTime)
{
	using namespace AnastasisVillageFabric;
	Wait -= DeltaTime;
	if (Wait > 0.0f)
	{
		return;
	}
	Wait = 0.5f;

	if (CVarVillageFabric.GetValueOnGameThread() == 0)
	{
		if (Actor.IsValid())
		{
			TearDown();
		}
		return;
	}
	UWorld* World = GetWorld();
	UAnastasisSimulationSubsystem* Sim = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Sim || !Sim->GetSimulation().IsRunning())
	{
		return;
	}
	const TArray<FPlot> Plots = PlotsOf(Sim->GetSimulation().GetVillage());
	if (Plots.Num() == 0)
	{
		if (Actor.IsValid())
		{
			TearDown();
		}
		return;
	}
	const uint32 Key = KeyOf(Plots);
	if (Key != LastKey)
	{
		Rebuild(Key);
	}
}

void UAnastasisVillageFabricSubsystem::Rebuild(const uint32 Key)
{
	using namespace AnastasisVillageFabric;
	UWorld* World = GetWorld();
	UAnastasisSimulationSubsystem* Sim = World->GetSubsystem<UAnastasisSimulationSubsystem>();
	const FGround Ground(World);
	const TArray<FPlot> Plots = PlotsOf(Sim->GetSimulation().GetVillage());
	const double CellCm = AnastasisWorldView::TileWorldSize * Ground.Scale;

	// Le terrain peut ne pas etre encore incarne : on reessaie au prochain passage, sans memoriser la cle.
	double Probe = 0.0;
	const FPlot& First = Plots[0];
	if (Ground.Surfaces.Num() == 0 || !Ground.Trace((First.Cell.X + 0.5) * CellCm, (First.Cell.Y + 0.5) * CellCm, Probe))
	{
		return;
	}

	const double Start = FPlatformTime::Seconds();
	const auto Height = [&Ground](double X, double Y, double& OutZ) { return Ground.Trace(X, Y, OutZ); };
	Last = Build(Plots, CellCm, Height);
	BuildMs = (FPlatformTime::Seconds() - Start) * 1000.0;

	AAnastasisVillageFabric* Fabric = Actor.Get();
	if (!Fabric)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Fabric = World->SpawnActor<AAnastasisVillageFabric>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		Actor = Fabric;
#if WITH_EDITOR
		if (Fabric)
		{
			Fabric->SetActorLabel(TEXT("VillageFabric"));
		}
#endif
	}
	if (!Fabric)
	{
		return;
	}

	// Pierre de la cote : la pierre patinee des lieux composes, a defaut la pierre du projet.
	UMaterialInterface* StoneMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Anastasis/WorldDressing01/MI_WeatheredStone.MI_WeatheredStone"));
	if (!StoneMaterial)
	{
		StoneMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Anastasis/Materials/M_AnastasisStone.M_AnastasisStone"));
	}
	UStaticMesh* Plane = nullptr;
	if (CVarVillageFabricTree.GetValueOnGameThread() != 0 && Last.Plaza.bHasTree)
	{
		const int32 Variant = 1 + static_cast<int32>(StableHash(Last.Plaza.WellId) % 3);
		const FString Path = FString::Printf(TEXT("/Game/Anastasis/Vegetation/SM_Tree_PlaneTree_0%d.SM_Tree_PlaneTree_0%d"), Variant, Variant);
		Plane = LoadObject<UStaticMesh>(nullptr, *Path);
	}

	const double ApplyStart = FPlatformTime::Seconds();
	ClearedLast = Fabric->Apply(Last, Height, StoneMaterial, StoneMaterial, Plane, CVarVillageFabricClear.GetValueOnGameThread() != 0);
	const double ApplyMs = (FPlatformTime::Seconds() - ApplyStart) * 1000.0;
	LastKey = Key;
	++Rebuilds;
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("%s"), *ToLogLine(Last.Report));
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_FABRIC built rebuild=%d grammarMs=%.1f applyMs=%.1f stones=%d triangles=%d cleared=%d tree=%s material=%s"),
		Rebuilds, BuildMs, ApplyMs, Fabric->GetStoneCount(), Fabric->GetTriangleCount(), ClearedLast,
		Plane ? *Plane->GetName() : TEXT("-"), StoneMaterial ? *StoneMaterial->GetName() : TEXT("-"));
}

FString UAnastasisVillageFabricSubsystem::StatusJson() const
{
	using namespace AnastasisVillageFabric;
	const AAnastasisVillageFabric* Fabric = Actor.Get();
	return FString::Printf(
		TEXT("{\"enabled\":%d,\"actor\":%s,\"rebuilds\":%d,\"key\":\"%08x\",\"grammarMs\":%.1f,\"stones\":%d,\"triangles\":%d,\"cleared\":%d,\"fabric\":%s}"),
		CVarVillageFabric.GetValueOnGameThread(),
		Fabric ? TEXT("true") : TEXT("false"),
		Rebuilds,
		LastKey,
		BuildMs,
		Fabric ? Fabric->GetStoneCount() : 0,
		Fabric ? Fabric->GetTriangleCount() : 0,
		Fabric ? Fabric->GetClearedCount() : 0,
		*ToJson(Last));
}

FString UAnastasisVillageFabricLibrary::GetVillageFabricStatus(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UAnastasisVillageFabricSubsystem* Fabric = World ? World->GetSubsystem<UAnastasisVillageFabricSubsystem>() : nullptr;
	return Fabric ? Fabric->StatusJson() : FString(TEXT("{}"));
}
