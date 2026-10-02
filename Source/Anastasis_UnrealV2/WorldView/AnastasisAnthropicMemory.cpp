#include "WorldView/AnastasisAnthropicMemory.h"

void AnastasisAnthropic::FMemory::Reset()
{
    Previous.Reset(); Cells.Reset(); LastTime = -1.0;
    RejectedGaps = RejectedJumps = DroppedCells = 0;
}
void AnastasisAnthropic::FMemory::Observe(double Time, const TArray<FObservation>& People)
{
    if (!FMath::IsFinite(Time)) return;
    if (Time < LastTime) Reset();
    if (Time == LastTime) return; // repeated render frames are not footsteps
    const double Dt = LastTime < 0.0 ? 0.0 : Time - LastTime;
    // Artistic envelope: eight simulated days to halve a trace. Not historical calibration.
    const double Decay = FMath::Pow(0.5, Dt / (8.0 * 90.0));
    for (auto It = Cells.CreateIterator(); It; ++It)
    {
        It.Value().Metres *= Decay;
        if (It.Value().Metres < 0.005) It.RemoveCurrent();
    }
    // Only adjacent observations separated by at most one normal 1/60 simulation step.
    // Accelerated/missing samples must not turn a curved route into an invented chord.
    const bool bGap = Dt > 1.0 / 60.0 + 1.e-7;
    if (bGap) ++RejectedGaps;
    TMap<FString, FObservation> Next;
    for (const FObservation& Person : People)
    {
        if (!FMath::IsFinite(Person.Position.X) || !FMath::IsFinite(Person.Position.Y)) continue;
        const FObservation* Before = Previous.Find(Person.Id);
        if (Before && !bGap && Dt > 0.0 && Before->bOutdoor && Person.bOutdoor)
        {
            const double Length = FVector2D::Distance(Before->Position, Person.Position);
            // More than 2m per normal step is untrusted (reset/teleport/fast actor).
            if (Length > 200.0) ++RejectedJumps;
            else if (Length > 0.001)
            {
                const FVector2D Delta = Person.Position - Before->Position;
                // Exact segment/grid intersection: distance, not number of frames, adds wear.
                TArray<double> Cuts{0.0, 1.0};
                for (int32 Axis = 0; Axis < 2; ++Axis)
                {
                    const double A = Before->Position[Axis], B = Person.Position[Axis];
                    if (FMath::Abs(B - A) < 1.e-9) continue;
                    for (int32 K = FMath::FloorToInt32(FMath::Min(A, B) / CellUU) + 1;
                        K <= FMath::FloorToInt32(FMath::Max(A, B) / CellUU); ++K)
                    {
                        const double T = (K * CellUU - A) / (B - A);
                        if (T > 0.0 && T < 1.0) Cuts.Add(T);
                    }
                }
                Cuts.Sort();
                for (int32 I = 1; I < Cuts.Num(); ++I)
                {
                    const double Metres = Length * (Cuts[I] - Cuts[I - 1]) / 100.0;
                    if (Metres <= 0.0) continue;
                    const FVector2D P = Before->Position + Delta * ((Cuts[I] + Cuts[I - 1]) * 0.5);
                    const FIntPoint Key(FMath::FloorToInt32(P.X / CellUU), FMath::FloorToInt32(P.Y / CellUU));
                    FCell* Cell = Cells.Find(Key);
                    if (!Cell)
                    {
                        if (Cells.Num() >= MaxCells) { ++DroppedCells; continue; }
                        Cell = &Cells.Add(Key);
                        Cell->Position = P;
                    }
                    const double Sum = Cell->Metres + Metres;
                    Cell->Position = (Cell->Position * Cell->Metres + P * Metres) / Sum;
                    Cell->Metres = FMath::Min(16.0, Sum);
                }
            }
        }
        Next.Add(Person.Id, Person);
    }
    Previous = MoveTemp(Next); // removed NPCs cannot leave a connecting segment on reappearance
    LastTime = Time;
}

double AnastasisAnthropic::FMemory::StrengthAt(const FVector2D& Point) const
{
    double Strength = 0.0;
    const FIntPoint Key(FMath::FloorToInt32(Point.X / CellUU), FMath::FloorToInt32(Point.Y / CellUU));
    for (int32 Y = -1; Y <= 1; ++Y) for (int32 X = -1; X <= 1; ++X)
    {
        if (const FCell* C = Cells.Find(Key + FIntPoint(X, Y)))
        {
            const double Edge = FMath::Clamp(1.0 - FVector2D::Distance(Point, C->Position) / 75.0, 0.0, 1.0);
            // A single traverse is invisible; repeated observed distance opens a soft, narrow tread.
            // Blend adjoining observed cells so grid boundaries do not create a dotted trail.
            Strength += FMath::Clamp((C->Metres - 2.0) / 6.0, 0.0, 1.0) * Edge;
        }
    }
    return FMath::Clamp(Strength, 0.0, 1.0);
}
