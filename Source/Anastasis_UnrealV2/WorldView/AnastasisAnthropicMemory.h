#pragma once
#include "CoreMinimal.h"

/** Presentation-only memory of sampled outdoor displacement, not a simulation of soil or traffic. */
namespace AnastasisAnthropic
{
struct FObservation
{
    FString Id;
    FVector2D Position = FVector2D::ZeroVector; // rendered world centimetres
    bool bOutdoor = true;
};
struct FCell
{
    double Metres = 0.0;
    FVector2D Position = FVector2D::ZeroVector;
};
class FMemory
{
public:
    static constexpr double CellUU = 100.0;
    static constexpr int32 MaxCells = 4096;
    void Observe(double Time, const TArray<FObservation>& People);
    void Reset();
    double StrengthAt(const FVector2D& Point) const;
    const TMap<FIntPoint, FCell>& GetCells() const { return Cells; }
    int32 RejectedGaps = 0;
    int32 RejectedJumps = 0;
    int32 DroppedCells = 0;
private:
    TMap<FString, FObservation> Previous;
    TMap<FIntPoint, FCell> Cells;
    double LastTime = -1.0;
};
}
