#include "WorldView/AnastasisSettlementSite.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

AnastasisSettlementSite::FReport AnastasisSettlementSite::Choose(const FInputs& In, const FSettings& S)
{
    FReport R;
    if (In.W < 3 || In.H < 3 || In.Cells.Num() != In.W * In.H || !FMath::IsFinite(In.TileMetres) || In.TileMetres <= 0.0)
    { R.Error = TEXT("invalid_survey"); return R; }
    R.bValidInput = true;
    const int32 Count = In.Cells.Num();
    TArray<bool> Open; Open.Init(false, Count);
    for (int32 I = 0; I < Count; ++I)
    {
        const auto& C = In.Cells[I];
        if (C.bSurveyed) ++R.Surveyed;
        Open[I] = C.bSurveyed && C.bWalkable && C.bDry && FMath::IsFinite(C.Height)
            && FMath::IsFinite(C.Slope) && C.Slope <= S.RouteSlope;
    }
    auto Neighbours = [&](int32 I, auto Visit)
    {
        const int32 X = I % In.W, Y = I / In.W;
        if (X > 0) Visit(I - 1);
        if (X + 1 < In.W) Visit(I + 1);
        if (Y > 0) Visit(I - In.W);
        if (Y + 1 < In.H) Visit(I + In.W);
    };
    auto Edge = [&](int32 A, int32 B)
    {
        return Open[A] && Open[B] && FMath::Abs(In.Cells[A].Height - In.Cells[B].Height)
            <= In.TileMetres * 100.0 * FMath::Tan(FMath::DegreesToRadians(S.RouteSlope));
    };
    TArray<int32> Dist[3], Target[3];
    for (int32 Kind = 0; Kind < 3; ++Kind)
    {
        Dist[Kind].Init(INDEX_NONE, Count); Target[Kind].Init(INDEX_NONE, Count);
        TArray<int32> Queue;
        for (int32 I = 0; I < Count; ++I)
        {
            if (!Open[I]) continue;
            bool bSource = Kind == 1 && In.Cells[I].bFood;
            Neighbours(I, [&](int32 J)
            {
                if (Kind == 0 && In.Cells[J].bWater) bSource = true;
                if (Kind == 2 && In.Cells[J].bWood) bSource = true;
            });
            if (bSource) { Dist[Kind][I] = 0; Target[Kind][I] = I; Queue.Add(I); }
        }
        for (int32 Q = 0; Q < Queue.Num(); ++Q)
        {
            const int32 I = Queue[Q];
            Neighbours(I, [&](int32 J)
            {
                if (Dist[Kind][J] < 0 && Edge(I, J))
                { Dist[Kind][J] = Dist[Kind][I] + 1; Target[Kind][J] = Target[Kind][I]; Queue.Add(J); }
            });
        }
    }
    // Exact legacy search order: first free tile around map centre with >=3 free neighbours.
    int32 Legacy = INDEX_NONE;
    for (int32 Radius = 0; Radius <= 24 && Legacy < 0; ++Radius)
        for (int32 DY = -Radius; DY <= Radius && Legacy < 0; ++DY)
            for (int32 DX = -Radius; DX <= Radius && Legacy < 0; ++DX)
            {
                if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Radius) continue;
                const int32 X = In.W/2 + DX, Y = In.H/2 + DY;
                if (X < 2 || Y < 2 || X > In.W-3 || Y > In.H-3 || !In.Cells[Y*In.W+X].bWalkable) continue;
                int32 Free = 0;
                for (int32 V = -1; V <= 1; ++V) for (int32 U = -1; U <= 1; ++U)
                    if ((U || V) && In.Cells[(Y+V)*In.W+X+U].bWalkable) ++Free;
                if (Free >= 3) Legacy = Y*In.W+X;
            }
    auto Evaluate = [&](int32 I)
    {
        FCandidate C; C.Index = I; C.Slope = In.Cells[I].Slope;
        C.WaterM = Dist[0][I] < 0 ? -1.0 : Dist[0][I]*In.TileMetres;
        C.FoodM = Dist[1][I] < 0 ? -1.0 : Dist[1][I]*In.TileMetres;
        C.WoodM = Dist[2][I] < 0 ? -1.0 : Dist[2][I]*In.TileMetres;
        C.WaterAccess = Target[0][I]; C.FoodAccess = Target[1][I]; C.WoodAccess = Target[2][I];
        int32 Free = 0;
        Neighbours(I, [&](int32 J) { if (Edge(I,J)) ++Free; });
        // SeedFirstWell requires a two-cell map margin; keep selection inside its placement contract.
        const int32 SiteX = I % In.W, SiteY = I / In.W;
        const bool bMargin = SiteX >= 2 && SiteY >= 2 && SiteX <= In.W-3 && SiteY <= In.H-3;
        const bool bBase = bMargin && Open[I] && In.Cells[I].bCenterAllowed && C.Slope <= S.SiteSlope && Free >= 3;
        if (bBase)
        {
            TSet<int32> Seen; TArray<int32> Queue; Seen.Add(I); Queue.Add(I);
            const int32 X = I % In.W, Y = I / In.W;
            for (int32 Q = 0; Q < Queue.Num(); ++Q)
                Neighbours(Queue[Q], [&](int32 J)
                {
                    if (!Seen.Contains(J) && FMath::Abs(J%In.W-X) <= S.AreaRadius && FMath::Abs(J/In.W-Y) <= S.AreaRadius
                        && In.Cells[J].Slope <= S.ExpansionSlope && Edge(Queue[Q],J))
                    { Seen.Add(J); Queue.Add(J); }
                });
            C.AreaM2 = Queue.Num() * In.TileMetres * In.TileMetres;
        }
        auto Reach = [](double D, double Limit) { return D < 0 ? 0.0 : FMath::Clamp(1.0-D/Limit,0.0,1.0); };
        C.Score = 40.0 * FMath::Min(C.AreaM2 / 10000.0,1.0)
            + 25.0*Reach(C.WaterM,S.WaterReach) + 20.0*Reach(C.FoodM,S.FoodReach) + 15.0*Reach(C.WoodM,S.WoodReach);
        C.bEligible = bBase && C.AreaM2 >= S.MinAreaCells*In.TileMetres*In.TileMetres
            && C.WaterM >= 0 && C.WaterM <= S.WaterReach && C.FoodM >= 0 && C.FoodM <= S.FoodReach
            && C.WoodM >= 0 && C.WoodM <= S.WoodReach;
        return C;
    };
    if (Legacy >= 0) R.Legacy = Evaluate(Legacy);
    for (int32 I = 0; I < Count; ++I)
    {
        if (!Open[I]) continue;
        FCandidate C = Evaluate(I);
        if (!C.bEligible) continue;
        ++R.Eligible; R.Top.Add(C);
    }
    R.Top.Sort([](const FCandidate& A, const FCandidate& B) { return A.Score != B.Score ? A.Score > B.Score : A.Index < B.Index; });
    if (!R.Top.IsEmpty()) R.Best = R.Top[0];
    else
    {
        int32 Reached[3] = {};
        for (int32 Kind = 0; Kind < 3; ++Kind) for (int32 D : Dist[Kind]) if (D >= 0) ++Reached[Kind];
        R.Error = FString::Printf(TEXT("no_site_meets_constraints reachable_water=%d food=%d wood=%d"), Reached[0], Reached[1], Reached[2]);
    }
    if (R.Top.Num() > 5) R.Top.SetNum(5);
    return R;
}

FString AnastasisSettlementSite::ToJson(const FReport& R, const FInputs& In)
{
    auto Encode = [&](const FCandidate& C)
    {
        auto O = MakeShared<FJsonObject>();
        O->SetNumberField(TEXT("index"), C.Index);
        O->SetNumberField(TEXT("x"), C.Index < 0 ? -1 : C.Index % In.W);
        O->SetNumberField(TEXT("y"), C.Index < 0 ? -1 : C.Index / In.W);
        O->SetNumberField(TEXT("ground_z"), In.Cells.IsValidIndex(C.Index) ? In.Cells[C.Index].Height : 0.0);
        O->SetBoolField(TEXT("eligible"), C.bEligible); O->SetNumberField(TEXT("score"), C.Score);
        O->SetNumberField(TEXT("slope_deg"), C.Slope); O->SetNumberField(TEXT("area_m2"), C.AreaM2);
        O->SetNumberField(TEXT("water_m"), C.WaterM); O->SetNumberField(TEXT("food_m"), C.FoodM); O->SetNumberField(TEXT("wood_m"), C.WoodM);
        O->SetNumberField(TEXT("water_access"), C.WaterAccess); O->SetNumberField(TEXT("food_access"), C.FoodAccess); O->SetNumberField(TEXT("wood_access"), C.WoodAccess);
        return O;
    };
    auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("status"), R.Best.bEligible ? TEXT("selected") : TEXT("unavailable"));
    Root->SetStringField(TEXT("error"),R.Error);
    Root->SetNumberField(TEXT("surveyed"),R.Surveyed); Root->SetNumberField(TEXT("eligible_count"),R.Eligible);
    Root->SetNumberField(TEXT("width"),In.W); Root->SetNumberField(TEXT("tile_m"),In.TileMetres);
    Root->SetObjectField(TEXT("selected"),Encode(R.Best)); Root->SetObjectField(TEXT("legacy"),Encode(R.Legacy));
    TArray<TSharedPtr<FJsonValue>> Top;
    for (const auto& C : R.Top) Top.Add(MakeShared<FJsonValueObject>(Encode(C)));
    Root->SetArrayField(TEXT("top"),Top);
    FString Json; const auto Writer = TJsonWriterFactory<>::Create(&Json); FJsonSerializer::Serialize(Root, Writer); return Json;
}
