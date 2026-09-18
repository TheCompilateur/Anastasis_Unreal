#include "WorldDressing/AnastasisWorldDressingManager.h"
#include "WorldDressing/AnastasisWorldDressingPlacement.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "Village/AnastasisVillageBuilding.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnastasisWorldDressing, Log, All);

namespace
{
using namespace AnastasisWorldDressing;
bool ReadMap(const AAnastasisWorldDressingManager& Manager,FMap& Out,FString& Error)
{
    const auto* Source=Manager.WorldSource.Get();
    if (!IsValid(Source) || Source->GetWorld()!=Manager.GetWorld())
    { Error=TEXT("Assign WorldSource in this level; source must already embody a surface"); return false; }
    // Sole semantic integration point: copy the existing snapshot. Never GenerateWorld/Embody here.
    Out.Snapshot=Source->GetSnapshot(); Out.SourceTransform=Source->GetActorTransform();
    // Sole geometric integration point: read section 0 of ExperimentalTerrain, including forge relief.
    // No line trace: collisions might be stale or hit another prop; section 1 is WATER, never ground.
    TArray<UProceduralMeshComponent*> Surfaces;
    Source->GetComponents(Surfaces);
    bool Found=false;
    for (auto* Surface:Surfaces)
    {
        if (Surface->GetFName()!=TEXT("ExperimentalTerrain") || !Surface->IsVisible()) continue;
        const auto* Section=Surface->GetProcMeshSection(0);
        if (!Section || !Section->bSectionVisible) continue;
        Found=true;
        for (int32 I=0;I+2<Section->ProcIndexBuffer.Num();I+=3)
        {
            FVector V[3];
            for (int32 K=0;K<3;++K)
            {
                const uint32 Index=Section->ProcIndexBuffer[I+K];
                if (!Section->ProcVertexBuffer.IsValidIndex(Index)) { Error=TEXT("Invalid rendered surface index"); return false; }
                V[K]=Out.SourceTransform.InverseTransformPosition(Surface->GetComponentTransform().TransformPosition(Section->ProcVertexBuffer[Index].Position));
            }
            Out.Triangles.Add({V[0],V[1],V[2]});
        }
    }
    if (!Found) { Error=TEXT("WorldSource has no visible ExperimentalTerrain section 0; generate its surface first"); return false; }
    if (!FMath::IsFinite(Manager.ExclusionPadding) || Manager.ExclusionPadding<0)
    { Error=TEXT("Invalid ExclusionPadding"); return false; }
    auto Bounds=[&](AActor* Actor)
    {
        FBox B=Actor->GetComponentsBoundingBox(true);
        if (!B.IsValid) B=FBox(Actor->GetActorLocation(),Actor->GetActorLocation());
        B=B.TransformBy(Out.SourceTransform.ToInverseMatrixWithScale());
        return FBox2D(FVector2D(B.Min.X,B.Min.Y)-FVector2D(Manager.ExclusionPadding),
            FVector2D(B.Max.X,B.Max.Y)+FVector2D(Manager.ExclusionPadding));
    };
    // Logical village actors and explicit tags/arrays are read-only exclusion sources.
    // No road/building generators or navigation systems are called. Unrepresented footprints
    // must be supplied by the designer; the semantic snapshot itself contains no building mask.
    for (TActorIterator<AActor> It(Manager.GetWorld());It;++It)
    {
        AActor* A=*It;
        if (A==&Manager || A==Source) continue;
        if (A->IsA<AAnastasisVillageBuilding>() || A->ActorHasTag(TEXT("WorldDressingBuilding")) || Manager.BuildingExclusions.Contains(A))
            Out.Buildings.Add(Bounds(A));
        if (A->ActorHasTag(TEXT("WorldDressingRoad")) || Manager.RoadExclusions.Contains(A)) Out.Roads.Add(Bounds(A));
    }
    return Out.Prepare(Error);
}
}

AAnastasisWorldDressingManager::AAnastasisWorldDressingManager()
{
    PrimaryActorTick.bCanEverTick=false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("WorldDressingRoot")));
}
#if WITH_EDITOR
void AAnastasisWorldDressingManager::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // Any details edit or manager move invalidates the explicit preview; never implicit regeneration.
    ClearPreview();
}
#endif
void AAnastasisWorldDressingManager::ClearPreview()
{
    for (UHierarchicalInstancedStaticMeshComponent* C:GeneratedComponents) if (IsValid(C)) C->DestroyComponent();
    GeneratedComponents.Reset(); GeneratedInstanceCount=0; PlacementHash.Reset();
    PlacementReport=TEXT("EMPTY: preview cleared; 0 generated components, 0 instances");
}
void AAnastasisWorldDressingManager::RebuildFromSeed() { GeneratePreview(); }
void AAnastasisWorldDressingManager::GeneratePreview()
{
    ClearPreview();
    auto Fail=[this](const FString& E)
    {
        ClearPreview(); PlacementReport=TEXT("FAIL: ")+E;
        UE_LOG(LogAnastasisWorldDressing,Warning,TEXT("%s"),*PlacementReport);
    };
    if (!GetWorld() || GetWorld()->IsGameWorld()) { Fail(TEXT("Preview is available only in the editor world")); return; }
    if (!IsValid(Profile)) { Fail(TEXT("Assign a World Dressing Profile")); return; }
    FMap Map; FString Error;
    if (!ReadMap(*this,Map,Error)) { Fail(Error); return; }
    FResult Result;
    if (!Build(Map,Profile->Rules,Seed,MaxInstances,Result,Error)) { Fail(Error); return; }
    // Load all assigned assets before creating components; a broken reference is an explicit failure.
    TArray<UStaticMesh*> Meshes; Meshes.SetNumZeroed(Profile->Rules.Num());
    int32 Unassigned=0;
    for (int32 I=0;I<Profile->Rules.Num();++I)
    {
        const auto& R=Profile->Rules[I];
        if (R.StaticMesh.IsNull()) { ++Unassigned; continue; }
        Meshes[I]=R.StaticMesh.LoadSynchronous();
        if (!Meshes[I]) { Fail(FString::Printf(TEXT("Cannot load rule[%d] mesh %s"),I,*R.StaticMesh.ToString())); return; }
    }
    // Support arbitrary mesh pivots: place its lowest local Z at the sampled root, along aligned up.
    for (auto& P:Result.Placements)
    {
        const double Bottom=Meshes[P.RuleIndex]->GetBoundingBox().Min.Z;
        const FVector Offset=P.Transform.GetRotation().RotateVector(FVector(0,0,-Bottom*P.Transform.GetScale3D().Z));
        P.Transform.AddToTranslation(Offset);
    }
    Result.Hash=AnastasisWorldDressing::PlacementHash(Result,Profile->Rules);
    GeneratedComponents.SetNumZeroed(Profile->Rules.Num());
    for (int32 I=0;I<Profile->Rules.Num();++I)
    {
        if (Result.Counts[I]==0) continue;
        auto* C=NewObject<UHierarchicalInstancedStaticMeshComponent>(this,NAME_None,RF_Transient|RF_DuplicateTransient);
        C->SetupAttachment(GetRootComponent());
        C->SetMobility(EComponentMobility::Movable);
        C->SetStaticMesh(Meshes[I]);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetGenerateOverlapEvents(false);
        C->SetCanEverAffectNavigation(false);
        C->bAutoRebuildTreeOnInstanceChanges=false;
        C->RegisterComponent();
        GeneratedComponents[I]=C;
    }
    int32 InvalidWater=0, InvalidBounds=0;
    for (const auto& P:Result.Placements)
    {
        if (!Map.TileAt(P.Ground.X,P.Ground.Y)) ++InvalidBounds;
        if (!Map.IsDry(FVector2D(P.Ground.X,P.Ground.Y))) ++InvalidWater;
        GeneratedComponents[P.RuleIndex]->AddInstance(P.Transform,true);
    }
    int32 Components=0;
    FString Counts;
    for (int32 I=0;I<Profile->Rules.Num();++I)
    {
        const auto* C=GeneratedComponents[I].Get();
        const int32 Count=C?C->GetInstanceCount():0;
        GeneratedInstanceCount+=Count;
        Counts+=FString::Printf(TEXT("\n  %s=%d%s"),*Profile->Rules[I].AssetId.ToString(),Count,
            Profile->Rules[I].StaticMesh.IsNull()?TEXT(" (mesh unassigned)"):TEXT(""));
    }
    for (UHierarchicalInstancedStaticMeshComponent* C:GeneratedComponents) if (C)
    {
        ++Components; C->bAutoRebuildTreeOnInstanceChanges=true; C->BuildTreeIfOutdated(false,true);
    }
    const bool Valid=InvalidWater==0 && InvalidBounds==0 && GeneratedInstanceCount==Result.Placements.Num();
    PlacementHash=Result.Hash;
    PlacementReport=FString::Printf(TEXT("%s seed=%d instances=%d HISM=%d propActors=0 water=%d outOfBounds=%d rejected=%d capped=%s unassigned=%d hash=%s%s\nValidation: roots/support ring only; road/building coverage = snapshot + known actors + supplied exclusions. Rebuild after world edits."),
        Valid?TEXT("PASS"):TEXT("FAIL"),Seed,GeneratedInstanceCount,Components,InvalidWater,InvalidBounds,
        Result.Rejected,Result.bCapped?TEXT("yes"):TEXT("no"),Unassigned,*PlacementHash,*Counts);
    PrintPlacementReport();
}
void AAnastasisWorldDressingManager::PrintPlacementReport()
{
    int32 Live=0;
    for (UHierarchicalInstancedStaticMeshComponent* C:GeneratedComponents) if (IsValid(C)) Live+=C->GetInstanceCount();
    UE_LOG(LogAnastasisWorldDressing,Display,TEXT("WORLD_DRESSING %s\nLive instances=%d (report describes last generation, not later world edits)"),*PlacementReport,Live);
}
