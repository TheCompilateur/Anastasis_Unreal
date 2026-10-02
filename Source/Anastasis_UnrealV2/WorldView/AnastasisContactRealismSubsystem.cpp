#include "WorldView/AnastasisContactRealismSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Components/DecalComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Materials/MaterialInterface.h"
#include "WorldView/AnastasisDrainage.h"
#include "WorldView/AnastasisPlaces.h"
#include "WorldView/AnastasisRiverbank.h"
#include "WorldView/AnastasisTerrainForge.h"
#include "WorldView/AnastasisWorldEmbodiment.h"

namespace
{
// AAA_CONTACT_REALISM_001 : la peau de contact. Coupable pour l'A/B : meme monde, sans la couche.
TAutoConsoleVariable<int32> CVarContactRealism(
	TEXT("anastasis.Contact.Realism"), 1,
	TEXT("1=raccord objets / sol et rives : decalques DBuffer et cailloux au pied des rochers (defaut), 0=aucun."), ECVF_Default);
TAutoConsoleVariable<int32> CVarContactPebbles(
	TEXT("anastasis.Contact.Pebbles"), 1,
	TEXT("1=cailloux au pied des rochers et sur les rives de pierre (defaut), 0=decalques seuls ; applique a la reconstruction."), ECVF_Default);
TAutoConsoleVariable<float> CVarContactFade(
	TEXT("anastasis.Contact.FadeScreenSize"), 0.012f,
	TEXT("Taille ecran (fraction) en dessous de laquelle un decalque s'efface. Plus petit = visible plus loin ; applique a la reconstruction."), ECVF_Default);
TAutoConsoleVariable<int32> CVarContactMaxDecals(
	TEXT("anastasis.Contact.MaxDecals"), 3200,
	TEXT("Plafond de decalques VIVANTS (composants) autour du point de vue ; les plus proches l'emportent."), ECVF_Default);
TAutoConsoleVariable<float> CVarContactRadius(
	TEXT("anastasis.Contact.Radius"), 9000.0f,
	TEXT("Rayon (uu) autour du point de vue ou les decalques de contact existent ; au-dela, le plan les garde sans composant."), ECVF_Default);
TAutoConsoleVariable<int32> CVarContactPlanDecals(
	TEXT("anastasis.Contact.PlanDecals"), 40000,
	TEXT("Plafond du PLAN (tout le monde) : bien au-dessus de ce qu'un point de vue en fait vivre ; applique a la reconstruction."), ECVF_Default);
TAutoConsoleVariable<int32> CVarContactInAutomation(
	TEXT("anastasis.Contact.InAutomation"), 0,
	TEXT("0=la couche ne se reconstruit pas seule pendant un test d'automatisation (defaut), 1=elle le fait."), ECVF_Default);

FAutoConsoleCommandWithWorld CmdContactRebuild(
	TEXT("anastasis.Contact.Rebuild"),
	TEXT("Rebatit la couche de contact depuis l'incarnation courante (monde d'editeur : a lancer apres EmbodyCanonical)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (World)
		{
			if (UAnastasisContactRealismSubsystem* Sub = World->GetSubsystem<UAnastasisContactRealismSubsystem>()) Sub->Rebuild();
		}
	}));

FAutoConsoleCommandWithWorld CmdContactStatus(
	TEXT("anastasis.Contact.Status"),
	TEXT("Journalise ce que la couche de contact a pose, famille par famille."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (World)
		{
			if (UAnastasisContactRealismSubsystem* Sub = World->GetSubsystem<UAnastasisContactRealismSubsystem>()) Sub->LogStatus();
		}
	}));

FAutoConsoleCommandWithWorldAndArgs CmdContactFocus(
	TEXT("anastasis.Contact.Focus"),
	TEXT("anastasis.Contact.Focus <x> <y> | off : force le point de vue de la diffusion des decalques (editeur, preuves)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 1) return;
		UAnastasisContactRealismSubsystem* Sub = World->GetSubsystem<UAnastasisContactRealismSubsystem>();
		if (!Sub) return;
		if (Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase)) Sub->SetFocusOverride(false, FVector2D::ZeroVector);
		else if (Args.Num() >= 2) Sub->SetFocusOverride(true, FVector2D(FCString::Atod(*Args[0]), FCString::Atod(*Args[1])));
	}));

FAutoConsoleCommandWithWorldAndArgs CmdContactDump(
	TEXT("anastasis.Contact.Dump"),
	TEXT("anastasis.Contact.Dump <fichier.json> : ecrit le plan de contact (decalques, cailloux) pour viser les cameras de preuve."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 1) return;
		if (UAnastasisContactRealismSubsystem* Sub = World->GetSubsystem<UAnastasisContactRealismSubsystem>()) Sub->DumpPlan(Args[0]);
	}));

constexpr const TCHAR* DecalFolder = TEXT("/Game/Anastasis/AAAContactRealism");
}

bool UAnastasisContactRealismSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->IsGameWorld() || World->WorldType == EWorldType::Editor) && Super::ShouldCreateSubsystem(Outer);
}

TStatId UAnastasisContactRealismSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAnastasisContactRealismSubsystem, STATGROUP_Tickables);
}

void UAnastasisContactRealismSubsystem::Deinitialize()
{
	Clear();
	if (IsValid(HostActor)) HostActor->Destroy();
	HostActor = nullptr;
	PebbleMeshes.Reset();
	Super::Deinitialize();
}

AAnastasisWorldEmbodiment* UAnastasisContactRealismSubsystem::FindEmbodiment() const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;
	for (TActorIterator<AAnastasisWorldEmbodiment> It(World); It; ++It)
	{
		if (IsValid(*It)) return *It;
	}
	return nullptr;
}

uint64 UAnastasisContactRealismSubsystem::EmbodimentSignature(const AAnastasisWorldEmbodiment& Embodiment) const
{
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

void UAnastasisContactRealismSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (GIsAutomationTesting && CVarContactInAutomation.GetValueOnGameThread() == 0) return;
	PollClock += DeltaTime;
	if (PollClock < 0.5f) return;
	PollClock = 0.0f;
	const int32 Enabled = CVarContactRealism.GetValueOnGameThread() != 0 ? 1 : 0;
	if (Enabled != BuiltEnabled)
	{
		if (Enabled == 0)
		{
			Clear();
			BuiltEnabled = 0;
			return;
		}
		BuiltSignature = 0;
		BuiltEnabled = 1;
	}
	if (!Enabled) return;
	const AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();
	if (!Embodiment) return;
	const uint64 Signature = EmbodimentSignature(*Embodiment);
	if (Signature != 0 && Signature == BuiltSignature) UpdateLive(false);
	if (Signature == 0 || Signature == BuiltSignature) { PendingSignature = 0; PendingPolls = 0; return; }
	// L'incarnation pose ses HISM par phases : on attend que l'empreinte tienne deux relevÃ©s d'affilee.
	if (Signature != PendingSignature) { PendingSignature = Signature; PendingPolls = 1; return; }
	if (++PendingPolls < 3) return;
	Rebuild();
}

int32 UAnastasisContactRealismSubsystem::GetPebbleCount() const
{
	int32 N = 0;
	for (const UHierarchicalInstancedStaticMeshComponent* M : PebbleMeshes)
	{
		if (IsValid(M)) N += M->GetInstanceCount();
	}
	return N;
}

AActor* UAnastasisContactRealismSubsystem::EnsureActor()
{
	if (IsValid(HostActor)) return HostActor;
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.Name = TEXT("AnastasisContactRealism");
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Host = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Host) return nullptr;
	USceneComponent* Root = NewObject<USceneComponent>(Host, TEXT("ContactRoot"), RF_Transient);
	Host->SetRootComponent(Root);
	Root->RegisterComponent();
#if WITH_EDITOR
	Host->SetActorLabel(TEXT("AnastasisContactRealism (transitoire)"));
#endif
	HostActor = Host;
	return HostActor;
}

UMaterialInterface* UAnastasisContactRealismSubsystem::DecalMaterial(AnastasisContactRealism::EDecal Kind)
{
	const int32 Key = static_cast<int32>(Kind);
	if (const TObjectPtr<UMaterialInterface>* Found = Materials.Find(Key)) return Found->Get();
	const FString Name = FString::Printf(TEXT("MI_ACR_%s"), AnastasisContactRealism::DecalName(Kind));
	UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), DecalFolder, *Name, *Name));
	Materials.Add(Key, Loaded);
	if (!Loaded)
	{
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_CONTACT missing_material=%s (tools/unreal/aaa-contact-realism.ps1)"), *Name);
	}
	return Loaded;
}

void UAnastasisContactRealismSubsystem::GatherAnchors(const AAnastasisWorldEmbodiment& Embodiment, TArray<AnastasisContactRealism::FAnchor>& Out) const
{
	using namespace AnastasisContactRealism;
	TArray<UHierarchicalInstancedStaticMeshComponent*> Comps;
	Embodiment.GetComponents(Comps);
	TSet<int64> Seen;
	for (const UHierarchicalInstancedStaticMeshComponent* C : Comps)
	{
		if (!C || C->GetOwner() != &Embodiment) continue;
		const UStaticMesh* Mesh = C->GetStaticMesh();
		if (!Mesh) continue;
		const FString Name = Mesh->GetName();
		bool bKnown = true;
		EAnchor Kind = EAnchor::Tree;
		if (Name.StartsWith(TEXT("SM_Tree_"))) Kind = EAnchor::Tree;
		else if (Name.StartsWith(TEXT("SM_Ecotone_Sapling"))) Kind = EAnchor::Sapling;
		else if (Name.StartsWith(TEXT("SM_Ecotone_Stump"))) Kind = EAnchor::Stump;
		else if (Name.StartsWith(TEXT("SM_Ecotone_FallenLog")) || Name.StartsWith(TEXT("SM_Ecotone_BranchPile")) || Name.StartsWith(TEXT("SM_Ecotone_Driftwood"))) Kind = EAnchor::Lying;
		else if (Name.StartsWith(TEXT("SM_Ecotone_ExposedRoots"))) Kind = EAnchor::Roots;
		else if (Name.StartsWith(TEXT("SM_Ecotone_Bush")) || Name.StartsWith(TEXT("SM_Shrub_"))) Kind = EAnchor::Bush;
		else if (Name.StartsWith(TEXT("SM_Ecotone_Reed"))) Kind = EAnchor::Reed;
		else if (Name.StartsWith(TEXT("SM_Rock_"))) Kind = EAnchor::Rock;
		else bKnown = false;
		if (!bKnown) continue;
		const FBox Box = Mesh->GetBoundingBox();
		const FVector Size = Box.GetSize();
		const int32 Count = C->GetInstanceCount();
		Out.Reserve(Out.Num() + Count);
		for (int32 I = 0; I < Count; ++I)
		{
			FTransform T;
			if (!C->GetInstanceTransform(I, T, true)) continue;
			const FVector Loc = T.GetLocation();
			const FVector Scale = T.GetScale3D().GetAbs();
			const double SizeX = Size.X * Scale.X, SizeY = Size.Y * Scale.Y;
			if (Kind != EAnchor::Reed)
			{
				// Un seul ancrage par place : les HISM de couronne et de bille se recouvrent parfois.
				const int64 Key = (static_cast<int64>(FMath::RoundToInt(Loc.X / 90.0)) << 32) ^ static_cast<uint32>(FMath::RoundToInt(Loc.Y / 90.0)) ^ (static_cast<int64>(Kind) << 56);
				bool bAlready = false;
				Seen.Add(Key, &bAlready);
				if (bAlready) continue;
			}
			FAnchor A;
			A.Kind = Kind;
			A.Location = Loc;
			A.Height = Size.Z * Scale.Z;
			A.Radius = 0.5 * FMath::Max(SizeX, SizeY);
			const double Yaw = T.Rotator().Yaw;
			A.YawDegrees = SizeY > SizeX ? Yaw + 90.0 : Yaw;
			Out.Add(A);
		}
	}
}

void UAnastasisContactRealismSubsystem::SuppressVegetationReceivers(const AAnastasisWorldEmbodiment& Embodiment)
{
	// Un decalque DBuffer se compose sur TOUT recepteur de sa boite : sans cela, il noircit aussi les
	// roseaux, l'herbe, les buissons et les troncs (capture first, 2026-10-02). La matiere de contact
	// est celle du SOL ; les objets posees dessus ne la recoivent pas. Etat d'origine restitue par Clear().
	TArray<UHierarchicalInstancedStaticMeshComponent*> Comps;
	Embodiment.GetComponents(Comps);
	for (UHierarchicalInstancedStaticMeshComponent* C : Comps)
	{
		if (C && C->bReceivesDecals)
		{
			C->SetReceivesDecals(false);
			ReceiveSwitched.Add(C);
		}
	}
}

void UAnastasisContactRealismSubsystem::Clear()
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& Weak : ReceiveSwitched)
	{
		if (UPrimitiveComponent* C = Weak.Get()) C->SetReceivesDecals(true);
	}
	ReceiveSwitched.Reset();
	for (TPair<int32, TObjectPtr<UDecalComponent>>& Pair : Live)
	{
		if (IsValid(Pair.Value)) Pair.Value->DestroyComponent();
	}
	Live.Reset();
	bLiveBuilt = false;
	for (UHierarchicalInstancedStaticMeshComponent* M : PebbleMeshes)
	{
		if (IsValid(M)) M->ClearInstances();
	}
}

bool UAnastasisContactRealismSubsystem::Rebuild()
{
	namespace CR = AnastasisContactRealism;
	Clear();
	if (CVarContactRealism.GetValueOnGameThread() == 0)
	{
		BuiltEnabled = 0;
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_CONTACT enabled=0 reason=cvar"));
		return false;
	}
	const AAnastasisWorldEmbodiment* Embodiment = FindEmbodiment();
	if (!Embodiment)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_CONTACT enabled=0 reason=no_embodiment"));
		return false;
	}
	const FBox& Footprint = Embodiment->GetActiveFootprintBounds();
	double Probe;
	if (!Footprint.IsValid || !AnastasisTerrainForge::SampleActive(Footprint.GetCenter().X, Footprint.GetCenter().Y, Probe))
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_CONTACT enabled=0 reason=no_rendered_ground footprint=%d"), Footprint.IsValid);
		return false;
	}
	const double Start = FPlatformTime::Seconds();
	namespace RB = AnastasisRiverbank;
	RB::FSpeedField Speed;
	const AnastasisDrainage::FNetwork& Network = AnastasisDrainage::GetActive();
	if (Network.GridW > 0) RB::BuildSpeedField(Network, Speed);
	const RB::FSettings BankSettings;

	CR::FInputs In;
	In.SampleHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActive(X, Y, Z); };
	In.SampleWaterHeight = [](double X, double Y, double& Z) { return AnastasisTerrainForge::SampleActiveWater(X, Y, Z); };
	if (Speed.IsValid())
	{
		In.SampleCalm = [&Speed, &BankSettings](double X, double Y) { return RB::Calmness(Speed.Sample(X, Y), BankSettings); };
	}
	In.Bounds = FBox2D(FVector2D(Footprint.Min.X, Footprint.Min.Y), FVector2D(Footprint.Max.X, Footprint.Max.Y));
	In.Seed = static_cast<int32>(Embodiment->GetSnapshot().Seed);
	GatherAnchors(*Embodiment, In.Anchors);

	CR::FSettings Settings;
	Settings.MaxDecals = FMath::Max(1, CVarContactPlanDecals.GetValueOnGameThread());
	if (CVarContactPebbles.GetValueOnGameThread() == 0) Settings.MaxPebbles = 0;
	FString Error;
	if (!CR::Build(In, Settings, LastPlan, Error))
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_CONTACT rejected=%s"), *Error);
		return false;
	}
	AActor* Host = EnsureActor();
	if (!Host)
	{
		UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_CONTACT rejected=no_host_actor"));
		return false;
	}
	SuppressVegetationReceivers(*Embodiment);

	// ---- Decalques : diffuses autour du point de vue par UpdateLive (projetes vers le bas : X local = -Z monde).
	int32 MissingMaterial = 0;
	for (int32 K = 0; K < CR::DecalCount; ++K)
	{
		if (LastPlan.DecalCounts[K] > 0 && !DecalMaterial(static_cast<CR::EDecal>(K))) ++MissingMaterial;
	}

	// ---- Cailloux : un HISM par variante, reutilise d'une reconstruction a l'autre (jamais detruit).
	int32 PebblesPlaced = 0;
	if (LastPlan.Pebbles.Num() > 0)
	{
		constexpr int32 Variants = 3;
		UStaticMesh* Meshes[Variants] = {};
		for (int32 V = 0; V < Variants; ++V)
		{
			Meshes[V] = LoadObject<UStaticMesh>(nullptr, *AnastasisPlaces::MeshPath(AnastasisPlaces::EFamily::RockLow, V));
		}
		TArray<FTransform> Batches[Variants];
		for (const CR::FPebble& P : LastPlan.Pebbles)
		{
			const int32 V = FMath::Clamp(P.Variant, 0, Variants - 1);
			const UStaticMesh* Mesh = Meshes[V];
			if (!Mesh) continue;
			const FBox MB = Mesh->GetBoundingBox();
			const double Diameter = 2.0 * FMath::Max(MB.GetExtent().X, MB.GetExtent().Y);
			const double Scale = Diameter > 1.0 ? P.Diameter / Diameter : 1.0;
			FVector At = P.Location;
			At.Z = P.Location.Z - MB.Min.Z * Scale - P.Sink * MB.GetSize().Z * Scale;
			Batches[V].Add(FTransform(FRotator(0.0, P.YawDegrees, 0.0), At, FVector(Scale)));
		}
		for (int32 V = 0; V < Variants; ++V)
		{
			if (!Meshes[V]) continue;
			const FName Name(*FString::Printf(TEXT("ACR_Pebble_%d"), V));
			UHierarchicalInstancedStaticMeshComponent* Made = nullptr;
			for (UHierarchicalInstancedStaticMeshComponent* M : PebbleMeshes)
			{
				if (IsValid(M) && M->GetFName() == Name) { Made = M; break; }
			}
			if (!Made)
			{
				Made = NewObject<UHierarchicalInstancedStaticMeshComponent>(Host, Name, RF_Transient);
				Made->SetupAttachment(Host->GetRootComponent());
				Made->SetStaticMesh(Meshes[V]);
				Made->SetMobility(EComponentMobility::Movable);
				Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Made->SetGenerateOverlapEvents(false);
				Made->SetCanEverAffectNavigation(false);
				Made->SetCastShadow(false);
				Made->SetReceivesDecals(false);
				Made->SetCullDistances(0, 6000);
				Made->RegisterComponent();
				PebbleMeshes.Add(Made);
			}
			if (Batches[V].Num() > 0)
			{
				Made->AddInstances(Batches[V], false, true);
				PebblesPlaced += Batches[V].Num();
			}
		}
	}

	BuiltSignature = EmbodimentSignature(*Embodiment);
	BuiltEnabled = 1;
	UpdateLive(true);
	const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_CONTACT enabled=1 planned=%d live=%d pebbles=%d anchors=%d wet_cells=%d band_cells=%d truncated=%d missing_material=%d plan_ms=%.1f total_ms=%.1f"),
		LastPlan.Decals.Num(), Live.Num(), PebblesPlaced, In.Anchors.Num(), LastPlan.WetCells, LastPlan.BandCells, LastPlan.bTruncated ? 1 : 0, MissingMaterial, LastPlan.MilliSeconds, Ms);
	LogStatus();
	return true;
}

void UAnastasisContactRealismSubsystem::DumpPlan(const FString& Path) const
{
	namespace CR = AnastasisContactRealism;
	FString Json = TEXT("{\"decals\":[");
	for (int32 I = 0; I < LastPlan.Decals.Num(); ++I)
	{
		const CR::FDecal& D = LastPlan.Decals[I];
		Json += FString::Printf(TEXT("%s[\"%s\",\"%s\",%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%d,%d]"), I ? TEXT(",") : TEXT(""),
			CR::DecalName(D.Kind), CR::ShoreName(D.Shore), D.Location.X, D.Location.Y, D.Location.Z, D.HalfLong, D.HalfWide,
			D.AwayDegrees, D.bAway ? 1 : 0, D.Source);
	}
	Json += TEXT("],\"pebbles\":[");
	for (int32 I = 0; I < LastPlan.Pebbles.Num(); ++I)
	{
		const CR::FPebble& P = LastPlan.Pebbles[I];
		Json += FString::Printf(TEXT("%s[%.1f,%.1f,%.1f]"), I ? TEXT(",") : TEXT(""), P.Location.X, P.Location.Y, P.Location.Z);
	}
	Json += FString::Printf(TEXT("],\"truncated\":%d,\"ms\":%.1f}"), LastPlan.bTruncated ? 1 : 0, LastPlan.MilliSeconds);
	const bool bOk = FFileHelper::SaveStringToFile(Json, *Path);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_CONTACT_DUMP %s decals=%d pebbles=%d ok=%d"), *Path, LastPlan.Decals.Num(), LastPlan.Pebbles.Num(), bOk);
}

void UAnastasisContactRealismSubsystem::SetFocusOverride(bool bOn, const FVector2D& XY)
{
	bFocusOverride = bOn;
	FocusOverride = XY;
	UpdateLive(true);
}

void UAnastasisContactRealismSubsystem::UpdateLive(bool bForce)
{
	namespace CR = AnastasisContactRealism;
	if (LastPlan.Decals.Num() == 0 || !IsValid(HostActor) || CVarContactRealism.GetValueOnGameThread() == 0) return;
	UWorld* World = GetWorld();
	FVector2D Focus = FVector2D::ZeroVector;
	bool bHaveFocus = false;
	if (bFocusOverride)
	{
		Focus = FocusOverride;
		bHaveFocus = true;
	}
	else if (World && World->IsGameWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			FVector Loc;
			FRotator Rot;
			PC->GetPlayerViewPoint(Loc, Rot);
			Focus = FVector2D(Loc.X, Loc.Y);
			bHaveFocus = true;
		}
	}
	if (!bHaveFocus)
	{
		// Aucun point de vue (editeur sans Focus) : le centre du sol rendu, les plus proches l'emportent.
		if (const AAnastasisWorldEmbodiment* E = FindEmbodiment())
		{
			const FVector C = E->GetActiveFootprintBounds().GetCenter();
			Focus = FVector2D(C.X, C.Y);
		}
	}
	if (!bForce && bLiveBuilt && FVector2D::Distance(Focus, LiveFocus) < 1500.0) return;
	const double Radius = FMath::Max(1000.0, static_cast<double>(CVarContactRadius.GetValueOnGameThread()));
	const int32 MaxLive = FMath::Max(1, CVarContactMaxDecals.GetValueOnGameThread());

	// Voulus : a moins de Radius, les MaxLive plus proches. Gardes : a moins de 1,15 Radius (pas de clignotement
	// a la limite quand le point de vue oscille).
	TArray<TPair<double, int32>> Candidates;
	for (int32 I = 0; I < LastPlan.Decals.Num(); ++I)
	{
		const CR::FDecal& D = LastPlan.Decals[I];
		const double Dist = FVector2D::Distance(FVector2D(D.Location.X, D.Location.Y), Focus);
		if (Dist <= Radius) Candidates.Emplace(Dist, I);
	}
	Candidates.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
	if (Candidates.Num() > MaxLive) Candidates.SetNum(MaxLive);
	TSet<int32> Wanted;
	for (const TPair<double, int32>& C : Candidates) Wanted.Add(C.Value);

	int32 Destroyed = 0, Spawned = 0;
	for (auto It = Live.CreateIterator(); It; ++It)
	{
		const CR::FDecal& D = LastPlan.Decals[It.Key()];
		const double Dist = FVector2D::Distance(FVector2D(D.Location.X, D.Location.Y), Focus);
		if (!Wanted.Contains(It.Key()) && Dist > Radius * 1.15)
		{
			if (IsValid(It.Value())) It.Value()->DestroyComponent();
			It.RemoveCurrent();
			++Destroyed;
		}
	}
	const float Fade = CVarContactFade.GetValueOnGameThread();
	for (const int32 I : Wanted)
	{
		if (Live.Contains(I)) continue;
		const CR::FDecal& D = LastPlan.Decals[I];
		UMaterialInterface* Material = DecalMaterial(D.Kind);
		if (!Material) continue;
		UDecalComponent* C = NewObject<UDecalComponent>(HostActor, NAME_None, RF_Transient);
		C->SetupAttachment(HostActor->GetRootComponent());
		C->SetRelativeLocationAndRotation(D.Location, FRotator(-90.0, D.YawDegrees, 0.0));
		C->DecalSize = FVector(D.HalfDepth, D.HalfWide, D.HalfLong);
		C->SetDecalMaterial(Material);
		C->SetFadeScreenSize(Fade);
		C->SortOrder = static_cast<int32>(D.Kind);
		C->RegisterComponent();
		Live.Add(I, C);
		++Spawned;
	}
	LiveFocus = Focus;
	bLiveBuilt = true;
	if (Spawned > 0 || Destroyed > 0)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_CONTACT_LIVE focus=(%.0f,%.0f) radius=%.0f live=%d spawned=%d destroyed=%d planned=%d"),
			Focus.X, Focus.Y, Radius, Live.Num(), Spawned, Destroyed, LastPlan.Decals.Num());
	}
}

void UAnastasisContactRealismSubsystem::LogStatus() const
{
	namespace CR = AnastasisContactRealism;
	FString Families, Shores, Anchors;
	for (int32 I = 0; I < CR::DecalCount; ++I)
		Families += FString::Printf(TEXT("%s=%d "), CR::DecalName(static_cast<CR::EDecal>(I)), LastPlan.DecalCounts[I]);
	for (int32 I = 1; I < CR::ShoreCount; ++I)
		Shores += FString::Printf(TEXT("%s=%d/%d "), CR::ShoreName(static_cast<CR::EShore>(I)), LastPlan.ShoreDecaled[I], LastPlan.ShoreExamined[I]);
	for (int32 I = 0; I < CR::AnchorCount; ++I)
		Anchors += FString::Printf(TEXT("%s=%d "), CR::AnchorName(static_cast<CR::EAnchor>(I)), LastPlan.AnchorCounts[I]);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_CONTACT_DECALS %s"), *Families);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_CONTACT_SHORE decaled/examined %s"), *Shores);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_CONTACT_ANCHORS %s"), *Anchors);
}
