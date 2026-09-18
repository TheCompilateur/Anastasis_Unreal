#include "WorldDressing/AnastasisWorldDressingPlacement.h"
#include "Misc/SecureHash.h"
#include "WorldView/AnastasisTerrainSurface.h"

namespace AnastasisWorldDressing
{
namespace
{
constexpr double TileSize = AnastasisWorldView::TileWorldSize;
FIntPoint Bucket(double X, double Y) { return FIntPoint(FMath::FloorToInt(X/TileSize), FMath::FloorToInt(Y/TileSize)); }
double Distance(FVector2D P, const FBox2D& B)
{
    const double X=FMath::Max(FMath::Max(B.Min.X-P.X, 0.0), P.X-B.Max.X);
    const double Y=FMath::Max(FMath::Max(B.Min.Y-P.Y, 0.0), P.Y-B.Max.Y);
    return FMath::Sqrt(X*X+Y*Y);
}
bool Overlaps(FVector2D P, double Radius, const TArray<FBox2D>& Boxes)
{
    for (const auto& B:Boxes) if (Distance(P,B)<=Radius) return true;
    return false;
}
bool ValidRule(const FAnastasisDressingRule& R)
{
    const float Values[]={R.AltitudeMin,R.AltitudeMax,R.SlopeMin,R.SlopeMax,R.DistanceToWaterMin,
        R.DistanceToWaterMax,R.Density,R.ClusterRadius,R.ScaleMin,R.ScaleMax,R.ClearanceRadius};
    for (float V:Values) if (!FMath::IsFinite(V)) return false;
    for (auto T:R.AllowedTerrainFamilies) if (static_cast<uint8>(T)>7) return false;
    return !R.AssetId.IsNone() && R.AltitudeMin<=R.AltitudeMax && R.SlopeMin>=0 && R.SlopeMax<90
        && R.SlopeMin<=R.SlopeMax && R.DistanceToWaterMin>=0
        && (R.DistanceToWaterMax==-1 || R.DistanceToWaterMax>=R.DistanceToWaterMin)
        && R.Density>=0 && R.Density<=16 && R.ClusterRadius>=0 && R.ClusterRadius<=10000
        && R.ScaleMin>0 && R.ScaleMin<=R.ScaleMax && R.ScaleMax<=1000
        && R.ClearanceRadius>=0 && R.ClearanceRadius<=10000;
}
}

bool FMap::Prepare(FString& Error)
{
    TriangleBuckets.Reset(); Water.Reset();
    const auto& S=Snapshot;
    if (S.W<1 || S.H<1 || S.W>96 || S.H>96 || S.Tiles.Num()!=S.W*S.H
        || S.OriginX<0 || S.OriginY<0 || S.OriginX+S.W>S.SourceW || S.OriginY+S.H>S.SourceH
        || SourceTransform.ContainsNaN() || !SourceTransform.GetRotation().Equals(FQuat::Identity)
        || !SourceTransform.GetScale3D().Equals(FVector::OneVector))
    { Error=TEXT("Invalid snapshot dimensions or source transform (V0 supports translation only)"); return false; }
    for (int32 I=0;I<S.Tiles.Num();++I)
    {
        const auto& T=S.Tiles[I];
        if (!FMath::IsFinite(T.Alt) || T.X!=S.OriginX+I%S.W || T.Y!=S.OriginY+I/S.W || static_cast<uint8>(T.Type)>7)
        { Error=FString::Printf(TEXT("Invalid snapshot tile[%d]"),I); return false; }
        const FBox2D B(FVector2D(T.X*TileSize,T.Y*TileSize),FVector2D((T.X+1)*TileSize,(T.Y+1)*TileSize));
        if (T.Type==AnastasisWorld::ETileType::Water) Water.Add(B);
        if (T.Type==AnastasisWorld::ETileType::Road) Roads.Add(B);
    }
    if (Triangles.IsEmpty()) { Error=TEXT("No rendered ground triangles; legacy/debug slabs are not supported"); return false; }
    for (int32 I=0;I<Triangles.Num();++I)
    {
        const auto& T=Triangles[I];
        if (T.A.ContainsNaN() || T.B.ContainsNaN() || T.C.ContainsNaN())
        { Error=FString::Printf(TEXT("Invalid surface triangle[%d]"),I); return false; }
        const auto Min=Bucket(FMath::Min3(T.A.X,T.B.X,T.C.X),FMath::Min3(T.A.Y,T.B.Y,T.C.Y));
        const auto Max=Bucket(FMath::Max3(T.A.X,T.B.X,T.C.X),FMath::Max3(T.A.Y,T.B.Y,T.C.Y));
        if (Max.X-Min.X>96 || Max.Y-Min.Y>96) { Error=TEXT("Surface triangle exceeds map bounds"); return false; }
        for (int32 Y=Min.Y;Y<=Max.Y;++Y) for (int32 X=Min.X;X<=Max.X;++X)
            TriangleBuckets.FindOrAdd(FIntPoint(X,Y)).Add(I);
    }
    return true;
}

const AnastasisWorldView::FVisualTile* FMap::TileAt(double X,double Y) const
{
    const auto B=Bucket(X,Y);
    return AnastasisWorldView::FindTile(Snapshot,B.X,B.Y);
}
bool FMap::Sample(double X,double Y,FVector& Ground,FVector& Normal) const
{
    const auto* Indices=TriangleBuckets.Find(Bucket(X,Y));
    if (!Indices || !TileAt(X,Y)) return false;
    bool Found=false;
    for (int32 I:*Indices)
    {
        const auto& T=Triangles[I];
        const double D=(T.B.Y-T.C.Y)*(T.A.X-T.C.X)+(T.C.X-T.B.X)*(T.A.Y-T.C.Y);
        if (FMath::Abs(D)<1.e-10) continue;
        const double A=((T.B.Y-T.C.Y)*(X-T.C.X)+(T.C.X-T.B.X)*(Y-T.C.Y))/D;
        const double B=((T.C.Y-T.A.Y)*(X-T.C.X)+(T.A.X-T.C.X)*(Y-T.C.Y))/D;
        const double C=1-A-B;
        if (A<-1.e-8 || B<-1.e-8 || C<-1.e-8) continue;
        const double Z=A*T.A.Z+B*T.B.Z+C*T.C.Z;
        if (Found && Z<=Ground.Z) continue;
        Ground=FVector(X,Y,Z);
        Normal=FVector::CrossProduct(T.B-T.A,T.C-T.A).GetSafeNormal();
        if (Normal.Z<0) Normal=-Normal;
        Found=true;
    }
    return Found;
}
double FMap::WaterDistance(FVector2D P) const
{
    double D=TNumericLimits<double>::Max();
    for (const auto& B:Water) D=FMath::Min(D,Distance(P,B));
    return D;
}
bool FMap::IsDry(FVector2D P) const
{
    const auto* T=TileAt(P.X,P.Y);
    FVector G=FVector::ZeroVector,N=FVector::UpVector;
    return T && T->Type!=AnastasisWorld::ETileType::Water && Sample(P.X,P.Y,G,N)
        && G.Z>AnastasisTerrainSurface::WaterPlaneZ+1;
}

bool Build(const FMap& Map,const TArray<FAnastasisDressingRule>& Rules,int32 Seed,
    int32 MaxInstances,FResult& Out,FString& Error)
{
    Out=FResult(); Error.Reset(); Out.Counts.SetNumZeroed(Rules.Num());
    if (Rules.Num()>128 || MaxInstances<1 || MaxInstances>100000)
    { Error=TEXT("V0 limits: 128 rules, 1..100000 instances"); return false; }
    TSet<FName> Ids;
    for (int32 I=0;I<Rules.Num();++I)
    {
        if (!ValidRule(Rules[I]) || Ids.Contains(Rules[I].AssetId))
        { Error=FString::Printf(TEXT("Invalid or duplicate profile rule[%d]"),I); return false; }
        Ids.Add(Rules[I].AssetId);
    }
    // Rule order, row-major tile order, and FRandomStream are fixed. No TMap iteration or global RNG.
    for (int32 RI=0;RI<Rules.Num();++RI)
    {
        const auto& R=Rules[RI];
        if (R.StaticMesh.IsNull() || R.Density==0) continue;
        FTCHARToUTF8 Id(*R.AssetId.ToString());
        uint32 RuleSeed=static_cast<uint32>(Seed);
        for (int32 K=0;K<Id.Length();++K) RuleSeed=(RuleSeed^static_cast<uint8>(Id.Get()[K]))*16777619u;
        FRandomStream Random(static_cast<int32>(RuleSeed));
        for (const auto& T:Map.Snapshot.Tiles)
        {
            const int32 Count=FMath::FloorToInt(R.Density)+(Random.FRand()<FMath::Frac(R.Density)?1:0);
            const FVector2D Center((T.X+Random.FRand())*TileSize,(T.Y+Random.FRand())*TileSize);
            for (int32 K=0;K<Count;++K)
            {
                FVector2D P;
                if (R.ClusterRadius>0)
                {
                    const double Angle=Random.FRand()*2*PI, Radius=FMath::Sqrt(Random.FRand())*R.ClusterRadius;
                    P=Center+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Radius;
                }
                else P=FVector2D((T.X+Random.FRand())*TileSize,(T.Y+Random.FRand())*TileSize);
                const double Scale=Random.FRandRange(R.ScaleMin,R.ScaleMax);
                const double Yaw=Random.FRand()*2*PI;
                const double Radius=R.ClearanceRadius*Scale;
                FVector G=FVector::ZeroVector,N=FVector::UpVector;
                const auto* At=Map.TileAt(P.X,P.Y);
                bool Valid=At && Map.IsDry(P) && Map.Sample(P.X,P.Y,G,N);
                if (Valid)
                {
                    const double Slope=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(N.Z,0.0,1.0)));
                    const double WaterDistance=Map.WaterDistance(P);
                    Valid=(R.AllowedTerrainFamilies.IsEmpty() || R.AllowedTerrainFamilies.Contains(static_cast<EAnastasisDressingTerrain>(At->Type)))
                        && G.Z>=R.AltitudeMin && G.Z<=R.AltitudeMax && Slope>=R.SlopeMin && Slope<=R.SlopeMax
                        && WaterDistance>=FMath::Max<double>(R.DistanceToWaterMin,Radius+(R.bAvoidWater?25:0))
                        && (R.DistanceToWaterMax<0 || WaterDistance<=R.DistanceToWaterMax)
                        && (!R.bAvoidRoads || !Overlaps(P,Radius,Map.Roads))
                        && (!R.bAvoidBuildings || !Overlaps(P,Radius,Map.Buildings));
                    // Check support ring too: a valid center alone can hang over a crop edge or water.
                    for (int32 J=0;Valid && J<8;++J)
                        Valid=Map.IsDry(P+FVector2D(FMath::Cos(J*PI/4),FMath::Sin(J*PI/4))*Radius);
                }
                if (!Valid) { ++Out.Rejected; continue; }
                if (Out.Placements.Num()>=MaxInstances) { Out.bCapped=true; Out.Hash=PlacementHash(Out,Rules); return true; }
                const FQuat Align=R.bAlignToSurfaceNormal?FQuat::FindBetweenNormals(FVector::UpVector,N):FQuat::Identity;
                const FQuat Rotation=Align*FQuat(FVector::UpVector,R.bRandomYaw?Yaw:0);
                Out.Placements.Add({RI,FTransform(Rotation,Map.SourceTransform.TransformPosition(G),FVector(Scale)),G});
                ++Out.Counts[RI];
            }
        }
    }
    Out.Hash=PlacementHash(Out,Rules);
    return true;
}

FString PlacementHash(const FResult& Result,const TArray<FAnastasisDressingRule>& Rules)
{
    FSHA1 Sha;
    // Versioned UTF-8 serialization, quantized at 1e-4 cm/scale and 1e-7 quaternion.
    auto Add=[&Sha](const FString& S) { FTCHARToUTF8 U(*S); Sha.Update(reinterpret_cast<const uint8*>(U.Get()),U.Length()); };
    Add(TEXT("WorldDressingV0:1\n"));
    for (const auto& P:Result.Placements)
    {
        const auto& R=Rules[P.RuleIndex]; const auto& T=P.Transform;
        const FVector L=T.GetLocation(), S=T.GetScale3D(); const FQuat Q=T.GetRotation();
        const FString Id=R.AssetId.ToString(), Path=R.StaticMesh.ToSoftObjectPath().ToString();
        Add(FString::Printf(TEXT("%d:%s|%d:%s|"),Id.Len(),*Id,Path.Len(),*Path));
        for (double V:{L.X,L.Y,L.Z,S.X,S.Y,S.Z}) Add(FString::Printf(TEXT("%lld,"),FMath::RoundToInt64(V*10000)));
        for (double V:{Q.X,Q.Y,Q.Z,Q.W}) Add(FString::Printf(TEXT("%lld,"),FMath::RoundToInt64(V*10000000)));
        Add(TEXT("\n"));
    }
    Sha.Final(); uint8 Bytes[FSHA1::DigestSize]; Sha.GetHash(Bytes);
    return BytesToHex(Bytes,UE_ARRAY_COUNT(Bytes));
}
}
