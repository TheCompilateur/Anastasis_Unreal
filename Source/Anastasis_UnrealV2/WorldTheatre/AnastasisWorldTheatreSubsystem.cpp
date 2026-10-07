#include "WorldTheatre/AnastasisWorldTheatreSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "WorldTheatre/AnastasisWorldReading.h"
#include "WorldTheatre/AnastasisWorldTheatreThreat.h"
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

// v2.1 -- la lumiere : ce qui est loin perd valeur, saturation et chaleur. Parametres lus a chaque sondage (0,5 s).
TAutoConsoleVariable<int32> CVarTheatreLight(
	TEXT("anastasis.Theatre.Light"), 0,
	TEXT("WORLD_THEATRE v2.1 : 1=post-traitement de distance (le lointain s'assombrit, se desature, se refroidit), 0=aucun (defaut). A chaud."), ECVF_Default);
TAutoConsoleVariable<float> CVarTheatreLightStart(
	TEXT("anastasis.Theatre.Light.Start"), 1.5f, TEXT("Distance (km) ou l'effet commence."), ECVF_Default);
TAutoConsoleVariable<float> CVarTheatreLightFull(
	TEXT("anastasis.Theatre.Light.Full"), 9.0f, TEXT("Distance (km) ou l'effet est plein."), ECVF_Default);
TAutoConsoleVariable<float> CVarTheatreLightDarken(
	TEXT("anastasis.Theatre.Light.Darken"), 0.45f, TEXT("Perte de valeur au plein (0..1)."), ECVF_Default);
TAutoConsoleVariable<float> CVarTheatreLightDesaturate(
	TEXT("anastasis.Theatre.Light.Desaturate"), 0.45f, TEXT("Perte de saturation au plein (0..1)."), ECVF_Default);
TAutoConsoleVariable<float> CVarTheatreLightCool(
	TEXT("anastasis.Theatre.Light.Cool"), 1.0f, TEXT("Part de la teinte froide (0,86 0,94 1,06) au plein (0..1)."), ECVF_Default);
TAutoConsoleVariable<float> CVarTheatreLightSky(
	TEXT("anastasis.Theatre.Light.Sky"), 0.7f,
	TEXT("Part de l'effet appliquee a la bande basse du ciel (0 a HorizonHigh deg) ; 0 = ciel intact (run 19 h : la chaine se decoupait sur la brume)."), ECVF_Default);
TAutoConsoleVariable<float> CVarTheatreLightHorizon(
	TEXT("anastasis.Theatre.Light.Horizon"), 12.0f, TEXT("Hauteur (deg) de la bande de ciel qui suit le lointain."), ECVF_Default);
TAutoConsoleVariable<int32> CVarTheatreDepthProbe(
	TEXT("anastasis.Theatre.DepthProbe"), 0,
	TEXT("1=l'image affiche la profondeur en gris (log2 des metres, 0,5 m -> 0, 131 km -> 1) : instrument de mesure, jamais en jeu."), ECVF_Default);

constexpr const TCHAR* DistanceMaterialPath = TEXT("/Game/WorldTheatre/M_WorldTheatreDistance.M_WorldTheatreDistance");
constexpr const TCHAR* ProbeMaterialPath = TEXT("/Game/WorldTheatre/M_WorldTheatreDepthProbe.M_WorldTheatreDepthProbe");

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
// v2.2 -- la menace lue dans la simulation.
TAutoConsoleVariable<int32> CVarTheatreThreat(
	TEXT("anastasis.Theatre.Threat"), 0,
	TEXT("WORLD_THEATRE v2.2 : 1=fumees et feux de signaux pilotes par le monde exterieur simule (Anastasis.Geo.Load requis), 0=aucun (defaut). A chaud."), ECVF_Default);
TAutoConsoleVariable<float> CVarTheatreThreatDread(
	TEXT("anastasis.Theatre.Threat.Dread"), 0.25f,
	TEXT("Assombrissement du lointain ajoute au plein de l'effroi du village (avec anastasis.Theatre.Light 1)."), ECVF_Default);

FAutoConsoleCommandWithWorld CmdTheatreThreatStatus(
	TEXT("anastasis.Theatre.Threat.Status"),
	TEXT("Journalise la carte de menace (monde exterieur simule) et l'intensite de chaque fumee et feu de signaux."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisWorldTheatreSubsystem* Sub = TheatreOf(World)) Sub->LogThreat();
	}));

constexpr const TCHAR* SmokeMaterialPath = TEXT("/Game/WorldTheatre/M_WorldTheatreSmoke.M_WorldTheatreSmoke");
constexpr const TCHAR* FireMaterialPath = TEXT("/Game/WorldTheatre/M_WorldTheatreFire.M_WorldTheatreFire");

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
	UpdateThreat();
	UpdateLight();
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

void UAnastasisWorldTheatreSubsystem::UpdateLight()
{
	const bool bLight = CVarTheatreLight.GetValueOnGameThread() != 0;
	const bool bProbe = CVarTheatreDepthProbe.GetValueOnGameThread() != 0;
	if (!bLight && !bProbe && !IsValid(LightVolume)) return;
	if (!EnsureActor()) return;
	if (!IsValid(LightVolume))
	{
		LightVolume = NewObject<UPostProcessComponent>(HostActor, TEXT("TheatreLight"), RF_Transient);
		LightVolume->SetupAttachment(HostActor->GetRootComponent());
		// Non borne, priorite basse : n'ajoute que ses deux materiaux, ne prend la main sur aucun reglage d'autrui.
		LightVolume->bUnbound = true;
		LightVolume->Priority = -100.0f;
		LightVolume->BlendWeight = 1.0f;
		LightVolume->RegisterComponent();
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, DistanceMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			DistanceMaterial = UMaterialInstanceDynamic::Create(Base, HostActor);
		}
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, ProbeMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			ProbeMaterial = UMaterialInstanceDynamic::Create(Base, HostActor);
		}
		if (DistanceMaterial) LightVolume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(0.0f, DistanceMaterial));
		if (ProbeMaterial) LightVolume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(0.0f, ProbeMaterial));
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("WORLD_THEATRE light volume distance=%s probe=%s"),
			DistanceMaterial ? TEXT("ok") : TEXT("ABSENT (world-theatre-light-material.ps1)"), ProbeMaterial ? TEXT("ok") : TEXT("ABSENT"));
	}
	for (FWeightedBlendable& B : LightVolume->Settings.WeightedBlendables.Array)
	{
		if (B.Object == DistanceMaterial) B.Weight = bLight ? 1.0f : 0.0f;
		else if (B.Object == ProbeMaterial) B.Weight = bProbe ? 1.0f : 0.0f;
	}
	if (DistanceMaterial)
	{
		const float Cool = FMath::Clamp(CVarTheatreLightCool.GetValueOnGameThread(), 0.0f, 1.0f);
		DistanceMaterial->SetScalarParameterValue(TEXT("Start"), CVarTheatreLightStart.GetValueOnGameThread());
		DistanceMaterial->SetScalarParameterValue(TEXT("Full"), FMath::Max(CVarTheatreLightFull.GetValueOnGameThread(), CVarTheatreLightStart.GetValueOnGameThread() + 0.1f));
		// v2.2 : l'effroi du village (exposition simulee) assombrit davantage ce qui est loin.
		const float DreadDarken = CVarTheatreThreat.GetValueOnGameThread() != 0 ? Dread * CVarTheatreThreatDread.GetValueOnGameThread() : 0.0f;
		DistanceMaterial->SetScalarParameterValue(TEXT("Darken"), FMath::Clamp(CVarTheatreLightDarken.GetValueOnGameThread() + DreadDarken, 0.0f, 0.95f));
		DistanceMaterial->SetScalarParameterValue(TEXT("Desaturate"), FMath::Clamp(CVarTheatreLightDesaturate.GetValueOnGameThread(), 0.0f, 1.0f));
		DistanceMaterial->SetScalarParameterValue(TEXT("SkyCut"), 2.0e7f);
		DistanceMaterial->SetScalarParameterValue(TEXT("SkyAmount"), FMath::Clamp(CVarTheatreLightSky.GetValueOnGameThread(), 0.0f, 1.0f));
		DistanceMaterial->SetScalarParameterValue(TEXT("HorizonHigh"), FMath::Max(CVarTheatreLightHorizon.GetValueOnGameThread(), 0.5f));
		DistanceMaterial->SetVectorParameterValue(TEXT("Tint"), FMath::Lerp(FLinearColor::White, FLinearColor(0.86f, 0.94f, 1.06f), Cool));
	}
	const int32 State = (bLight ? 1 : 0) | (bProbe ? 2 : 0);
	if (State != LoggedLight)
	{
		LoggedLight = State;
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("WORLD_THEATRE light=%d probe=%d start=%.1fkm full=%.1fkm darken=%.2f desat=%.2f cool=%.2f sky=%.2f"),
			bLight ? 1 : 0, bProbe ? 1 : 0, CVarTheatreLightStart.GetValueOnGameThread(), CVarTheatreLightFull.GetValueOnGameThread(),
			CVarTheatreLightDarken.GetValueOnGameThread(), CVarTheatreLightDesaturate.GetValueOnGameThread(),
			CVarTheatreLightCool.GetValueOnGameThread(), CVarTheatreLightSky.GetValueOnGameThread());
	}
}

float UAnastasisWorldTheatreSubsystem::ReadSunElevationDeg() const
{
	// Le soleil est la lumiere directionnelle la plus forte (l'atmosphere du jeu la tourne avec l'heure du ciel).
	const UDirectionalLightComponent* Sun = nullptr;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			const UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent());
			if (L && L->IsVisible() && (!Sun || L->Intensity > Sun->Intensity)) Sun = L;
		}
	}
	if (!Sun) return 45.0f;
	return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-Sun->GetForwardVector().Z, -1.0, 1.0)));
}

bool UAnastasisWorldTheatreSubsystem::BuildThreatSigns()
{
	const TArray<AnastasisWorldTheatre::FThreatSite>& Sites = AnastasisWorldTheatre::CanonicalPlan().ThreatSites;
	if (Sites.Num() == 0 || !EnsureActor()) return false;
	AnastasisWorldReading::FReading Reading;
	FString Why;
	AnastasisWorldReading::FReadOptions Options = ReadOptions();
	Options.bPlaced = false;
	if (!AnastasisWorldReading::Read(GetWorld(), Options, Reading, Why))
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("WORLD_THREAT build skipped: %s"), *Why);
		return false;
	}
	UMaterialInterface* Smoke = LoadObject<UMaterialInterface>(nullptr, SmokeMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* Fire = LoadObject<UMaterialInterface>(nullptr, FireMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	// Vent dominant de nord-ouest (hypothese de conception) : les colonnes se couchent vers le sud-est (X = nord, Y = est).
	const FVector2D Lean = FVector2D(-0.7071, 0.7071) * 0.35;
	int32 Placed = 0;
	for (int32 K = 0; K < Sites.Num(); ++K)
	{
		const AnastasisWorldTheatre::FThreatSite& S = Sites[K];
		double G = 0.0;
		if (!SampleRaster(Reading.Near, Reading.Near.Ground, S.Location.X, S.Location.Y, G)
			&& !SampleRaster(Reading.Far, Reading.Far.Ground, S.Location.X, S.Location.Y, G))
		{
			ThreatMeshes.Add(nullptr);
			ThreatMain.Add(nullptr);
			ThreatDay.Add(nullptr);
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("WORLD_THREAT reject %s: hors du sol releve"), S.Id);
			continue;
		}
		UProceduralMeshComponent* Proc = NewObject<UProceduralMeshComponent>(HostActor, *FString::Printf(TEXT("Threat_%s"), S.Id), RF_Transient);
		Proc->SetupAttachment(HostActor->GetRootComponent());
		Proc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Proc->SetCanEverAffectNavigation(false);
		Proc->SetCastShadow(false);
		Proc->bUseAsyncCooking = false;
		Proc->SetWorldLocation(FVector(S.Location.X, S.Location.Y, G));
		Proc->RegisterComponent();
		UMaterialInstanceDynamic* Main = nullptr;
		UMaterialInstanceDynamic* Day = nullptr;
		if (S.Sign == AnastasisWorldTheatre::EThreatSign::Smoke)
		{
			const AnastasisWorldTheatre::FMeshData M = AnastasisWorldTheatreThreat::BuildSmokeColumn(S.Height, Lean, S.Height * 0.03, S.Height * 0.28);
			Proc->CreateMeshSection_LinearColor(0, M.Vertices, M.Triangles, M.Normals, M.UVs, M.Colours, TArray<FProcMeshTangent>(), false);
			if (Smoke)
			{
				Main = UMaterialInstanceDynamic::Create(Smoke, HostActor);
				// Fumee d'incendie : brun-gris sombre.
				Main->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.10f, 0.09f, 0.08f));
				Main->SetScalarParameterValue(TEXT("Seed"), float(K) * 7.31f);
				Proc->SetMaterial(0, Main);
			}
		}
		else
		{
			const AnastasisWorldTheatre::FMeshData F = AnastasisWorldTheatreThreat::BuildFire(S.Height * 1.4);
			Proc->CreateMeshSection_LinearColor(0, F.Vertices, F.Triangles, F.Normals, F.UVs, F.Colours, TArray<FProcMeshTangent>(), false);
			// Le jour, un feu de veille se lit a sa fumee claire, mince et haute.
			const AnastasisWorldTheatre::FMeshData D = AnastasisWorldTheatreThreat::BuildSmokeColumn(16000.0, Lean, 400.0, 3500.0);
			Proc->CreateMeshSection_LinearColor(1, D.Vertices, D.Triangles, D.Normals, D.UVs, D.Colours, TArray<FProcMeshTangent>(), false);
			if (Fire)
			{
				Main = UMaterialInstanceDynamic::Create(Fire, HostActor);
				Main->SetScalarParameterValue(TEXT("Seed"), float(K) * 3.17f);
				Proc->SetMaterial(0, Main);
			}
			if (Smoke)
			{
				Day = UMaterialInstanceDynamic::Create(Smoke, HostActor);
				Day->SetVectorParameterValue(TEXT("Tint"), FLinearColor(0.55f, 0.55f, 0.53f));
				Day->SetScalarParameterValue(TEXT("Seed"), float(K) * 5.03f);
				Proc->SetMaterial(1, Day);
			}
		}
		Proc->SetVisibility(false);
		ThreatMeshes.Add(Proc);
		ThreatMain.Add(Main);
		ThreatDay.Add(Day);
		++Placed;
	}
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("WORLD_THREAT signs placed=%d of %d smoke_material=%s fire_material=%s"), Placed, Sites.Num(),
		Smoke ? TEXT("ok") : TEXT("ABSENT"), Fire ? TEXT("ok") : TEXT("ABSENT"));
	return Placed > 0;
}

void UAnastasisWorldTheatreSubsystem::UpdateThreat()
{
	const bool bOn = CVarTheatreThreat.GetValueOnGameThread() != 0;
	if (!bOn)
	{
		Dread = 0.0f;
		for (UProceduralMeshComponent* P : ThreatMeshes) if (IsValid(P)) P->SetVisibility(false);
		return;
	}
	if (ThreatMeshes.Num() == 0 && !BuildThreatSigns()) return;
	const UAnastasisSimulationSubsystem* Sim = GetWorld() ? GetWorld()->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	AnastasisWorldTheatreThreat::FThreatMap Map;
	if (Sim)
	{
		const FAnastasisSimulation& S = Sim->GetSimulation();
		AnastasisWorldTheatreThreat::ReadThreatMap(S.GetGeo(), 1.0 + S.GetTime() / FAnastasisSimulation::DayLength, Map);
	}
	const TArray<AnastasisWorldTheatre::FThreatSite>& Sites = AnastasisWorldTheatre::CanonicalPlan().ThreatSites;
	const AnastasisWorldTheatreThreat::FSignals Signals = AnastasisWorldTheatreThreat::Evaluate(Map, Sites);
	bThreatLoaded = Map.bLoaded;
	ThreatDay_ = Map.DayFloat;
	ThreatVillageExcess = Map.VillageExcess;
	Dread = Signals.Dread;
	ThreatIntensity = Signals.Site;
	SunElevationDeg = ReadSunElevationDeg();
	// Nuit : le feu ; jour : la fumee claire. Entre les deux (crepuscule, soleil entre -6 et +4 deg), les deux se melent.
	const float Night = FMath::Clamp((4.0f - SunElevationDeg) / 10.0f, 0.0f, 1.0f);
	for (int32 K = 0; K < Sites.Num() && K < ThreatMeshes.Num(); ++K)
	{
		UProceduralMeshComponent* P = ThreatMeshes[K];
		if (!IsValid(P)) continue;
		const float I = ThreatIntensity.IsValidIndex(K) ? ThreatIntensity[K] : 0.0f;
		P->SetVisibility(I > 0.01f);
		if (Sites[K].Sign == AnastasisWorldTheatre::EThreatSign::Smoke)
		{
			if (ThreatMain[K]) ThreatMain[K]->SetScalarParameterValue(TEXT("Opacity"), 0.85f * I);
		}
		else
		{
			if (ThreatMain[K]) ThreatMain[K]->SetScalarParameterValue(TEXT("Intensity"), I * Night);
			if (ThreatDay[K]) ThreatDay[K]->SetScalarParameterValue(TEXT("Opacity"), 0.6f * I * (1.0f - Night));
		}
	}
	int32 Lit = 0;
	for (const float I : ThreatIntensity) Lit += I > 0.01f ? 1 : 0;
	const int32 State = (bThreatLoaded ? 1000 : 0) + Lit;
	if (State != LoggedThreat)
	{
		LoggedThreat = State;
		LogThreat();
	}
}

void UAnastasisWorldTheatreSubsystem::LogThreat() const
{
	const TArray<AnastasisWorldTheatre::FThreatSite>& Sites = AnastasisWorldTheatre::CanonicalPlan().ThreatSites;
	int32 Lit = 0;
	for (const float I : ThreatIntensity) Lit += I > 0.01f ? 1 : 0;
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("WORLD_THREAT status on=%d loaded=%d day=%.3f village_excess=%.3f dread=%.3f sun=%.1f lit=%d/%d"),
		CVarTheatreThreat.GetValueOnGameThread(), bThreatLoaded ? 1 : 0, ThreatDay_, ThreatVillageExcess, Dread, SunElevationDeg, Lit, Sites.Num());
	for (int32 K = 0; K < Sites.Num(); ++K)
	{
		const float I = ThreatIntensity.IsValidIndex(K) ? ThreatIntensity[K] : 0.0f;
		const bool bVisible = ThreatMeshes.IsValidIndex(K) && IsValid(ThreatMeshes[K]) && ThreatMeshes[K]->IsVisible();
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("WORLD_THREAT site %s node=%s sign=%s stage=%d intensity=%.3f visible=%d"),
			Sites[K].Id, Sites[K].NodeId, Sites[K].Sign == AnastasisWorldTheatre::EThreatSign::Smoke ? TEXT("smoke") : TEXT("beacon"),
			Sites[K].Stage, I, bVisible ? 1 : 0);
	}
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
