#include "WorldTheatre/AnastasisWorldTheatreSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "WorldTheatre/AnastasisWorldReading.h"
#include "WorldView/AnastasisWorldEmbodiment.h"

namespace
{
TAutoConsoleVariable<int32> CVarTheatre(
	TEXT("anastasis.Theatre"), 0,
	TEXT("WORLD_THEATRE_001 : 1=couche de mise en scene (masses lointaines, silhouettes) drapee sur le sol rendu, ")
	TEXT("0=aucune (defaut tant que le verdict visuel n'est pas rendu). A chaud."), ECVF_Default);
TAutoConsoleVariable<int32> CVarTheatreInAutomation(
	TEXT("anastasis.Theatre.InAutomation"), 0,
	TEXT("0=la couche ne se reconstruit pas seule pendant un test d'automatisation (defaut), 1=elle le fait."), ECVF_Default);
TAutoConsoleVariable<float> CVarTheatreNearCell(
	TEXT("anastasis.Theatre.ReadNearCell"), 2000.0f,
	TEXT("Pas (uu) de la grille proche du releve (carte + avant-pays)."), ECVF_Default);
TAutoConsoleVariable<float> CVarTheatreNearMargin(
	TEXT("anastasis.Theatre.ReadNearMargin"), 1200000.0f,
	TEXT("Avant-pays (uu) couvert par la grille proche au-dela de la carte."), ECVF_Default);
TAutoConsoleVariable<float> CVarTheatreFarCell(
	TEXT("anastasis.Theatre.ReadFarCell"), 20000.0f,
	TEXT("Pas (uu) de la grille lointaine du releve (tout l'anneau)."), ECVF_Default);

UAnastasisWorldTheatreSubsystem* TheatreOf(UWorld* World)
{
	return World ? World->GetSubsystem<UAnastasisWorldTheatreSubsystem>() : nullptr;
}

FAutoConsoleCommandWithWorldAndArgs CmdTheatreRead(
	TEXT("anastasis.Theatre.Read"),
	TEXT("anastasis.Theatre.Read <dossier> : releve du monde rendu (sol, eau, objets poses) pour l'analyse de mise en scene."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1) return;
		if (UAnastasisWorldTheatreSubsystem* Sub = TheatreOf(World)) Sub->ReadAndDump(Args[0]);
	}));
FAutoConsoleCommandWithWorld CmdTheatreRebuild(
	TEXT("anastasis.Theatre.Rebuild"),
	TEXT("Rebatit la couche de mise en scene depuis l'incarnation courante (monde d'editeur : apres EmbodyCanonical)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisWorldTheatreSubsystem* Sub = TheatreOf(World)) Sub->Rebuild();
	}));
FAutoConsoleCommandWithWorld CmdTheatreStatus(
	TEXT("anastasis.Theatre.Status"),
	TEXT("Journalise ce que la couche de mise en scene a pose et ce qu'elle a refuse."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisWorldTheatreSubsystem* Sub = TheatreOf(World)) Sub->LogStatus();
	}));

constexpr const TCHAR* MaterialPath = TEXT("/Game/WorldTheatre/M_WorldTheatreMass.M_WorldTheatreMass");
/** Repli : le materiau des montagnes lointaines (couleur de sommet = albedo), lu sans etre modifie. */
constexpr const TCHAR* FallbackMaterialPath = TEXT("/Game/Anastasis/Materials/M_AnastasisFarTerrain.M_AnastasisFarTerrain");

AnastasisWorldReading::FReadOptions ReadOptions()
{
	AnastasisWorldReading::FReadOptions O;
	O.NearCellUu = CVarTheatreNearCell.GetValueOnGameThread();
	O.NearMarginUu = CVarTheatreNearMargin.GetValueOnGameThread();
	O.FarCellUu = CVarTheatreFarCell.GetValueOnGameThread();
	return O;
}

/** Echantillonneur bilineaire sur le releve : grille proche, sinon lointaine. */
bool SampleRaster(const AnastasisWorldReading::FRaster& R, const TArray<float>& Layer, double X, double Y, double& Out)
{
	if (R.W < 2 || R.H < 2) return false;
	const double FI = (X - R.Origin.X) / R.Cell - 0.5, FJ = (Y - R.Origin.Y) / R.Cell - 0.5;
	if (FI < 0 || FJ < 0 || FI >= R.W - 1 || FJ >= R.H - 1) return false;
	const int32 I = FMath::FloorToInt32(FI), J = FMath::FloorToInt32(FJ);
	const double TI = FI - I, TJ = FJ - J;
	const float A = Layer[R.Index(I, J)], B = Layer[R.Index(I + 1, J)], C = Layer[R.Index(I, J + 1)], D = Layer[R.Index(I + 1, J + 1)];
	const float No = AnastasisWorldReading::NoData;
	if (A <= No || B <= No || C <= No || D <= No) return false;
	Out = (A * (1 - TI) + B * TI) * (1 - TJ) + (C * (1 - TI) + D * TI) * TJ;
	return true;
}
}

bool UAnastasisWorldTheatreSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->IsGameWorld() || World->WorldType == EWorldType::Editor) && Super::ShouldCreateSubsystem(Outer);
}

TStatId UAnastasisWorldTheatreSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAnastasisWorldTheatreSubsystem, STATGROUP_Tickables);
}

void UAnastasisWorldTheatreSubsystem::Deinitialize()
{
	Clear();
	if (IsValid(HostActor)) HostActor->Destroy();
	HostActor = nullptr;
	Mesh = nullptr;
	Super::Deinitialize();
}

AAnastasisWorldEmbodiment* UAnastasisWorldTheatreSubsystem::FindEmbodiment() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;
	for (TActorIterator<AAnastasisWorldEmbodiment> It(World); It; ++It)
	{
		if (IsValid(*It)) return *It;
	}
	return nullptr;
}

uint64 UAnastasisWorldTheatreSubsystem::EmbodimentSignature(const AAnastasisWorldEmbodiment& Embodiment) const
{
	// Meme empreinte que la couche de contact : instances posees + emprise + graine. L'incarnation pose par
	// phases ; on rebatit quand l'empreinte tient trois releves d'affilee.
	TArray<UHierarchicalInstancedStaticMeshComponent*> Comps;
	Embodiment.GetComponents(Comps);
	uint64 Instances = 0;
	for (const UHierarchicalInstancedStaticMeshComponent* C : Comps) Instances += static_cast<uint64>(C->GetInstanceCount());
	const FBox& Footprint = Embodiment.GetActiveFootprintBounds();
	if (Instances == 0 || !Footprint.IsValid) return 0;
	uint64 H = Instances * 0x9E3779B97F4A7C15ull;
	H ^= static_cast<uint64>(GetTypeHash(Footprint.Min)) << 1;
	H ^= static_cast<uint64>(GetTypeHash(Footprint.Max)) << 17;
	H ^= static_cast<uint64>(Embodiment.GetSnapshot().Seed) * 0xC2B2AE3D27D4EB4Full;
	return H ? H : 1;
}

void UAnastasisWorldTheatreSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (GIsAutomationTesting && CVarTheatreInAutomation.GetValueOnGameThread() == 0) return;
	PollClock += DeltaTime;
	if (PollClock < 0.5f) return;
	PollClock = 0.0f;
	const int32 Enabled = CVarTheatre.GetValueOnGameThread() != 0 ? 1 : 0;
	if (Enabled != BuiltEnabled)
	{
		BuiltEnabled = Enabled;
		BuiltSignature = 0;
		if (!Enabled)
		{
			Clear();
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("WORLD_THEATRE off"));
			return;
		}
	}
	if (!Enabled) return;
	const AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();
	if (!Embodiment) return;
	const uint64 Signature = EmbodimentSignature(*Embodiment);
	if (Signature == 0 || Signature == BuiltSignature) { PendingSignature = 0; PendingPolls = 0; return; }
	if (Signature != PendingSignature) { PendingSignature = Signature; PendingPolls = 1; return; }
	if (++PendingPolls < 3) return;
	BuiltSignature = Signature;
	Rebuild();
}

AActor* UAnastasisWorldTheatreSubsystem::EnsureActor()
{
	if (IsValid(HostActor) && IsValid(Mesh)) return HostActor;
	UWorld* World = GetWorld();
	if (!World) return nullptr;
	FActorSpawnParameters Params;
	Params.Name = TEXT("AnastasisWorldTheatre");
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Host) return nullptr;
	USceneComponent* Root = NewObject<USceneComponent>(Host, TEXT("TheatreRoot"), RF_Transient);
	Host->SetRootComponent(Root);
	Root->RegisterComponent();
	UProceduralMeshComponent* Proc = NewObject<UProceduralMeshComponent>(Host, TEXT("TheatreMesh"), RF_Transient);
	Proc->SetupAttachment(Root);
	Proc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Proc->SetCanEverAffectNavigation(false);
	Proc->SetCastShadow(true);
	Proc->bUseAsyncCooking = false;
	Proc->RegisterComponent();
	Host->Tags.Add(TEXT("DL_WORLD_THEATRE"));
#if WITH_EDITOR
	Host->Layers.AddUnique(TEXT("DL_WORLD_THEATRE"));
	Host->SetActorLabel(TEXT("AnastasisWorldTheatre (transitoire)"));
#endif
	HostActor = Host;
	Mesh = Proc;
	return HostActor;
}

UMaterialInterface* UAnastasisWorldTheatreSubsystem::ResolveMaterial()
{
	if (Material) return Material;
	Material = LoadObject<UMaterialInterface>(nullptr, MaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Material) Material = LoadObject<UMaterialInterface>(nullptr, FallbackMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("WORLD_THEATRE material=%s"), Material ? *Material->GetPathName() : TEXT("(aucun : materiau par defaut)"));
	return Material;
}

void UAnastasisWorldTheatreSubsystem::Clear()
{
	if (IsValid(Mesh)) Mesh->ClearAllMeshSections();
	bBuilt = false;
	LastReport = AnastasisWorldTheatre::FBuildReport();
}

bool UAnastasisWorldTheatreSubsystem::Rebuild()
{
	const double T0 = FPlatformTime::Seconds();
	Clear();
	AnastasisWorldReading::FReading Reading;
	FString Why;
	AnastasisWorldReading::FReadOptions Options = ReadOptions();
	Options.bPlaced = false;
	if (!AnastasisWorldReading::Read(GetWorld(), Options, Reading, Why))
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("WORLD_THEATRE rebuild skipped: %s"), *Why);
		return false;
	}
	// La lecture voit aussi la couche elle-meme si elle existait : on l'a videe juste avant (Clear).
	const AnastasisWorldReading::FRaster& Near = Reading.Near;
	const AnastasisWorldReading::FRaster& Far = Reading.Far;
	const AnastasisWorldTheatre::FGroundSampler Ground = [&Near, &Far](double X, double Y, double& G, double& W)
	{
		W = -TNumericLimits<double>::Max();
		const bool bNear = SampleRaster(Near, Near.Ground, X, Y, G);
		if (!bNear && !SampleRaster(Far, Far.Ground, X, Y, G)) return false;
		double Wz = 0.0;
		if (bNear ? SampleRaster(Near, Near.Water, X, Y, Wz) : SampleRaster(Far, Far.Water, X, Y, Wz)) W = Wz;
		return true;
	};
	const AnastasisWorldTheatre::FBuilt Built = AnastasisWorldTheatre::Build(AnastasisWorldTheatre::CanonicalPlan(), Ground);
	LastReport = Built.Report;
	if (!EnsureActor()) { UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("WORLD_THEATRE no host actor")); return false; }
	UMaterialInterface* Mat = ResolveMaterial();
	const AnastasisWorldTheatre::FMeshData* Sections[2] = { &Built.Masses, &Built.Silhouettes };
	int32 Tris = 0;
	for (int32 S = 0; S < 2; ++S)
	{
		const AnastasisWorldTheatre::FMeshData& D = *Sections[S];
		if (D.Triangles.Num() == 0) continue;
		Mesh->CreateMeshSection_LinearColor(S, D.Vertices, D.Triangles, D.Normals, TArray<FVector2D>(), D.Colours,
			TArray<FProcMeshTangent>(), false);
		if (Mat) Mesh->SetMaterial(S, Mat);
		Tris += D.TriangleCount();
	}
	bBuilt = Tris > 0;
	for (const FString& R : LastReport.Rejected) UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("WORLD_THEATRE reject %s"), *R);
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("WORLD_THEATRE built masses=%d (%.0f ha) silhouettes=%d rejected=%d triangles=%d %.2fs"),
		LastReport.MassesBuilt, LastReport.MassHectares, LastReport.SilhouettesBuilt,
		LastReport.Rejected.Num(), Tris, FPlatformTime::Seconds() - T0);
	return bBuilt;
}

void UAnastasisWorldTheatreSubsystem::LogStatus() const
{
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("WORLD_THEATRE status enabled=%d built=%d masses=%d (%.0f ha) silhouettes=%d rejected=%d"),
		CVarTheatre.GetValueOnGameThread(), bBuilt ? 1 : 0, LastReport.MassesBuilt, LastReport.MassHectares,
		LastReport.SilhouettesBuilt, LastReport.Rejected.Num());
	for (const FString& R : LastReport.Rejected) UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("WORLD_THEATRE reject %s"), *R);
}

bool UAnastasisWorldTheatreSubsystem::ReadAndDump(const FString& Dir)
{
	const double T0 = FPlatformTime::Seconds();
	AnastasisWorldReading::FReading Reading;
	FString Why;
	const bool bRead = AnastasisWorldReading::Read(GetWorld(), ReadOptions(), Reading, Why);
	if (!bRead || !AnastasisWorldReading::Dump(Reading, Dir, Why))
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("WORLD_THEATRE_READ FAIL %s"), *Why);
		return false;
	}
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("WORLD_THEATRE_READ OK dir=%s near=%dx%d@%.0f far=%dx%d@%.0f placed=%d ignored=%d tris(ground/horizon/water)=%d/%d/%d %.1fs"),
		*Dir, Reading.Near.W, Reading.Near.H, Reading.Near.Cell, Reading.Far.W, Reading.Far.H, Reading.Far.Cell,
		Reading.Placed.Num(), Reading.IgnoredInstances, Reading.GroundTriangles, Reading.HorizonTriangles, Reading.WaterTriangles,
		FPlatformTime::Seconds() - T0);
	return true;
}
