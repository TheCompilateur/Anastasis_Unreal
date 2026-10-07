#pragma once
#include "CoreMinimal.h"
#include "World/AnastasisWorld.h"
class UHierarchicalInstancedStaticMeshComponent;

/** Presentation only: resource units are not individual historical trees. */
namespace AnastasisForestUse
{
double Depletion(const AnastasisWorld::FTile& Original, const AnastasisWorld::FTile* Live);
bool HideTree(double Fraction, uint32 StableSeed);
struct FInstance
{
    TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Mesh;
    int32 Index = INDEX_NONE;
    int32 SourceIndex = INDEX_NONE;
    uint32 StableSeed = 0;
    FTransform Original;
    bool bHidden = false;
};
class FBinding
{
public:
    void Reset();
    void Add(UHierarchicalInstancedStaticMeshComponent* Mesh, int32 Index, int32 SourceIndex, uint32 StableSeed);
    void AddShell(UHierarchicalInstancedStaticMeshComponent* Mesh, int32 Index, FVector2D Center, double Radius);
    void Apply(const AnastasisWorld::FWorld& World, const TMap<int32, AnastasisWorld::FTile>& Live, bool bEnabled);
    int32 Hidden = 0;
    int32 Changed = 0;
    int32 Conflicts = 0;
    int32 Count() const { return Trees.Num(); }
private:
    TArray<FInstance> Trees;
    struct FShell { FInstance Instance; FVector2D Center; double Radius = 0; };
    TArray<FShell> Shells;
};
}
