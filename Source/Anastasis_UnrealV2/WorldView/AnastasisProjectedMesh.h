#pragma once
#include "CoreMinimal.h"

namespace AnastasisProjectedMesh
{
/**
 * XY bins over the triangles of a height surface (ground, lake sheet, river ribbons): the highest
 * surface under a point, without collision ray casts. Shared by the settlement survey (rendered
 * mesh sections) and the canonical geography (WATER_NETWORK_001, same geometry, no UWorld).
 */
class FProjectedMesh
{
public:
    double Cell = 2000.0;
    TArray<FVector> V;
    TArray<FIntVector> T;
    TMap<FIntPoint, TArray<int32>> Bins;

    /** Adds triangles (index triples into `Vertices`), already in world space. */
    void Add(const TArray<FVector>& Vertices, const TArray<int32>& Indices, const FTransform& Transform = FTransform::Identity)
    {
        const int32 Offset = V.Num();
        for (const FVector& P : Vertices) V.Add(Transform.TransformPosition(P));
        for (int32 I = 0; I + 2 < Indices.Num(); I += 3)
        {
            const int32 A = Offset + Indices[I], B = Offset + Indices[I + 1], C = Offset + Indices[I + 2];
            if (!V.IsValidIndex(A) || !V.IsValidIndex(B) || !V.IsValidIndex(C)) continue;
            const int32 K = T.Add(FIntVector(A, B, C));
            const int32 X0 = FMath::FloorToInt32(FMath::Min3(V[A].X, V[B].X, V[C].X) / Cell);
            const int32 X1 = FMath::FloorToInt32(FMath::Max3(V[A].X, V[B].X, V[C].X) / Cell);
            const int32 Y0 = FMath::FloorToInt32(FMath::Min3(V[A].Y, V[B].Y, V[C].Y) / Cell);
            const int32 Y1 = FMath::FloorToInt32(FMath::Max3(V[A].Y, V[B].Y, V[C].Y) / Cell);
            for (int32 Y = Y0; Y <= Y1; ++Y) for (int32 X = X0; X <= X1; ++X) Bins.FindOrAdd(FIntPoint(X, Y)).Add(K);
        }
    }

    /** Highest surface Z under (X, Y); false when no triangle covers the point. */
    bool Sample(double X, double Y, double& Z) const
    {
        const TArray<int32>* Bucket = Bins.Find(FIntPoint(FMath::FloorToInt32(X / Cell), FMath::FloorToInt32(Y / Cell)));
        if (!Bucket) return false;
        bool bFound = false;
        for (const int32 I : *Bucket)
        {
            const FIntVector& Tri = T[I];
            const FVector& A = V[Tri.X];
            const FVector& B = V[Tri.Y];
            const FVector& C = V[Tri.Z];
            const double D = (B.Y - C.Y) * (A.X - C.X) + (C.X - B.X) * (A.Y - C.Y);
            if (FMath::Abs(D) < 1.e-9) continue;
            const double U = ((B.Y - C.Y) * (X - C.X) + (C.X - B.X) * (Y - C.Y)) / D;
            const double W = ((C.Y - A.Y) * (X - C.X) + (A.X - C.X) * (Y - C.Y)) / D;
            const double Q = 1.0 - U - W;
            if (U < -1.e-7 || W < -1.e-7 || Q < -1.e-7) continue;
            const double H = U * A.Z + W * B.Z + Q * C.Z;
            if (!bFound || H > Z) Z = H;
            bFound = true;
        }
        return bFound;
    }

    bool IsEmpty() const { return T.IsEmpty(); }
};
}
