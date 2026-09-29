#include "WorldView/AnastasisHumanGeography.h"

namespace
{
double Smooth(double A, double B, double X)
{
    const double T=FMath::Clamp((X-A)/(B-A),0.0,1.0);
    return T*T*(3.0-2.0*T);
}
double Ellipse(double X,double Y,double CX,double CY,double RX,double RY)
{
    return FMath::Sqrt(FMath::Square((X-CX)/RX)+FMath::Square((Y-CY)/RY));
}
// Catmull-Rom XY avoids straight ramp joins; Z remains monotone between knots.
TArray<FVector> Curve(std::initializer_list<FVector> Knots)
{
    TArray<FVector> K; for (const FVector& P:Knots) K.Add(P);
    TArray<FVector> C;
    for(int32 I=0;I<K.Num()-1;++I)
    {
        const FVector P0=K[FMath::Max(0,I-1)],P1=K[I],P2=K[I+1],P3=K[FMath::Min(K.Num()-1,I+2)];
        for(int32 J=0;J<12;++J)
        {
            const double T=J/12.0;
            FVector P=0.5*((2*P1)+(-P0+P2)*T+(2*P0-5*P1+4*P2-P3)*T*T+(-P0+3*P1-3*P2+P3)*T*T*T);
            P.Z=FMath::Lerp(P1.Z,P2.Z,T);
            C.Add(P);
        }
    }
    C.Add(K.Last()); return C;
}
struct FNearest { double Distance=1.e10; double Height=0; };
FNearest Nearest(const TArray<FVector>& C,double X,double Y)
{
    FNearest Out;
    for(int32 I=0;I<C.Num()-1;++I)
    {
        const FVector2D A(C[I].X,C[I].Y),B(C[I+1].X,C[I+1].Y),P(X,Y),D=B-A;
        const double T=FMath::Clamp(FVector2D::DotProduct(P-A,D)/FMath::Max(D.SizeSquared(),1.e-9),0.0,1.0);
        const double Dist=(P-(A+T*D)).Size();
        if(Dist<Out.Distance) { Out.Distance=Dist;Out.Height=FMath::Lerp(C[I].Z,C[I+1].Z,T); }
    }
    return Out;
}
const TArray<FVector>& MainRiver()
{
    static const auto C=Curve({{30,76,5.6},{34,68,3.55},{40,59,3.30},{46,57,3.14},{52,58,3.02},{58,61,2.88},{63,65,2.75}});return C;
}
const TArray<FVector>& LakeOutlet()
{
    static const auto C=Curve({{69,70,2.75},{74,70,2.75},{80,69,2.68},{86,72,2.61},{91,75,2.54},{97,76,2.46}});return C;
}
const TArray<FVector>& SecondaryBrook()
{
    static const auto C=Curve({{33,24,4.15},{28,19,3.72},{23,13,3.23},{17,6,2.75}});return C;
}
const TArray<FVector>& Pass()
{
    static const auto C=Curve({{34,26,4.85},{42,33,5.4},{46,40,5.2},{48,47,4.5},{56,49,4.1}});return C;
}
}

AnastasisHumanGeography::FSample AnastasisHumanGeography::Evaluate(double X,double Y,double OriginalHeight)
{
    FSample Out;Out.Height=OriginalHeight;
    // Coalescent alluvial valley: wide interior, 100-200m transition at physical scale.
    const double A1=1-Smooth(0.60,1.25,Ellipse(X,Y,48,58,18,13));
    const double A2=1-Smooth(0.60,1.25,Ellipse(X,Y,62,47,15,12));
    const double KeepNE=1-Smooth(0.9,1.3,Ellipse(X,Y,69,70,9,9));
    const double KeepSE=1-Smooth(0.9,1.25,Ellipse(X,Y,71,26,11,15));
    const double A=(1-(1-A1)*(1-A2))*(1-KeepNE)*(1-KeepSE)*(1-Smooth(72,76,Y));
    const double B=(1-Smooth(0.60,1.3,Ellipse(X,Y,33,24,13,11)))*(1-KeepSE);
    const double Undulation=0.055*FMath::Sin(X*0.39)*FMath::Sin(Y*0.31);
    const double FloorA=4.0+0.010*(X-53)-0.012*(Y-53)+Undulation;
    const double FloorB=4.8+0.008*(X-33)+0.025*(Y-24)+Undulation;
    Out.Height=FMath::Lerp(Out.Height,FloorA,A);
    Out.Height=FMath::Lerp(Out.Height,FloorB,B);
    Out.ValleyWeight=FMath::Max(A,B);
    // A low sinuous saddle links the two territories without removing their divide.
    if(X>29 && X<61 && Y>21 && Y<54)
    {
        const auto Road=Nearest(Pass(),X,Y);
        const double W=1-Smooth(0.5,3.2,Road.Distance);
        Out.Height=FMath::Lerp(Out.Height,Road.Height+0.035*Road.Distance*Road.Distance,W);
        Out.ValleyWeight=FMath::Max(Out.ValleyWeight,W);
    }
    FNearest River;
    const auto Try=[&](const TArray<FVector>& C)
    { const auto R=Nearest(C,X,Y); if(R.Distance<River.Distance) River=R; };
    if(X>26 && X<67 && Y>53 && Y<80) Try(MainRiver());
    if(X>65 && Y>64 && Y<81) Try(LakeOutlet());
    if(X>12 && X<38 && Y<29) Try(SecondaryBrook());
    if(River.Distance<3.0)
    {
        // Bed below the water profile, then natural banks. No erosion noise is added.
        const double W=1-Smooth(0.45,3.0,River.Distance);
        const double Bed=River.Height-0.10+0.25*River.Distance*River.Distance;
        Out.Height=FMath::Lerp(Out.Height,FMath::Min(Out.Height,Bed),W);
        if(River.Distance<1.2) Out.WaterHeight=River.Height;
        Out.RiverWeight=1-Smooth(0.35,1.2,River.Distance);
    }
    return Out;
}

void AnastasisHumanGeography::Apply(const AnastasisWorldView::FWorldVisualSnapshot& S,
    AnastasisTerrainSurface::FGeometry& G,int32 W,int32 H)
{
    const double Scale=S.SpatialScale;
    const double Sea=AnastasisTerrainSurface::WaterPlaneZ;
    G.RiverFlow.SetNumZeroed(G.Vertices.Num());
    for(int32 I=0;I<G.Vertices.Num();++I)
    {
        FVector& P=G.Vertices[I];
        const double Original=(Sea+(P.Z-Sea)/Scale)/100.0;
        const FSample V=Evaluate(P.X/(100.0*Scale),P.Y/(100.0*Scale),Original);
        P.Z=Sea+(V.Height*100.0-Sea)*Scale;
        G.WaterVertices[I].Z=Sea+(V.WaterHeight*100.0-Sea)*Scale;
        G.RiverFlow[I]=static_cast<float>(V.RiverWeight);
        // New dry alluvium is presentation, not a mutation of source tile fertility/type.
        if(V.ValleyWeight>0 && V.Height>V.WaterHeight)
        {
            const float Blend=static_cast<float>(V.ValleyWeight);
            G.Colors[I]=FMath::Lerp(G.Colors[I],FLinearColor(0.31f,0.40f,0.19f,0.0f),Blend);
            G.UV0[I]*=(1.0-Blend);
            G.UV1[I].X=FMath::Lerp(G.UV1[I].X,0.15,static_cast<double>(Blend));
        }
    }
    // Water occupancy follows the edited geometry, including rivers and the open outlet.
    // Off-layer original water triangles are regenerated from actual submerged vertices.
    G.WaterTriangles.Reset();
    for(int32 Y=0;Y<H-1;++Y) for(int32 X=0;X<W-1;++X)
    {
        const int32 A=Y*W+X,B=A+1,C=A+W,D=C+1;
        if(G.Vertices[A].Z<G.WaterVertices[A].Z || G.Vertices[B].Z<G.WaterVertices[B].Z ||
            G.Vertices[C].Z<G.WaterVertices[C].Z || G.Vertices[D].Z<G.WaterVertices[D].Z)
            G.WaterTriangles.Append({A,C,B,B,C,D});
    }
}
