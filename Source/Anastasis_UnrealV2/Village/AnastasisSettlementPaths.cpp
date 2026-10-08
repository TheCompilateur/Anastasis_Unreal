#include "Village/AnastasisSettlementPaths.h"

#include "Anastasis_UnrealV2.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Village/AnastasisVillage.h"
#include "Village/AnastasisVillagePresentation.h"
#include "World/AnastasisTraffic.h"
#include "World/AnastasisWorld.h"

namespace
{
	const TCHAR* PathMaterial = TEXT("/Game/Anastasis/VillageArchitecture/M_AnastasisArchitecture.M_AnastasisArchitecture");
	/** Terre battue : classe EARTH du materiau du bati (alpha .25), un peu plus sombre que la cour. */
	const FLinearColor Earth(0.155f, 0.122f, 0.085f, 0.25f);

	/** Bruit lisse et deterministe le long du sentier : la largeur respire, sans tirage. */
	double Breath(const FVector& P)
	{
		const double A = FMath::Sin(P.X * 0.0031 + P.Y * 0.0017) * 0.6 + FMath::Sin(P.X * 0.0007 - P.Y * 0.0043) * 0.4;
		return 0.5 + 0.5 * A;
	}

	double DistToSegment2D(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double L2 = AB.SizeSquared();
		const double T = L2 > 1.0 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
		return FVector2D::Distance(P, A + AB * T);
	}
}

void FAnastasisSettlementPaths::Clear()
{
	for (const FHiddenInstance& H : Hidden)
	{
		if (UHierarchicalInstancedStaticMeshComponent* C = H.Component.Get())
		{
			if (H.Index >= 0 && H.Index < C->GetInstanceCount())
			{
				C->UpdateInstanceTransform(H.Index, H.Original, true, false, true);
			}
		}
	}
	TSet<UHierarchicalInstancedStaticMeshComponent*> Dirty;
	for (const FHiddenInstance& H : Hidden)
	{
		if (UHierarchicalInstancedStaticMeshComponent* C = H.Component.Get()) Dirty.Add(C);
	}
	for (UHierarchicalInstancedStaticMeshComponent* C : Dirty) C->MarkRenderStateDirty();
	Hidden.Reset();
	if (AActor* A = Actor.Get())
	{
		A->Destroy();
	}
	Actor.Reset();
	LastSignature = 0;
	LastRoadCount = -1;
	Segments = 0;
}

int32 FAnastasisSettlementPaths::Sync(const AnastasisVillage::FVillage& Village, const AnastasisWorld::FWorld& World,
	UWorld* PresentationWorld, const TArray<FDoorLink>& Doors)
{
	if (!PresentationWorld || World.W <= 0)
	{
		return 0;
	}
	const TMap<int32, AnastasisTraffic::FRoadTile>& Roads = Village.GetRoads();
	uint32 Signature = ::GetTypeHash(Roads.Num());
	TArray<int32> Keys;
	Roads.GetKeys(Keys);
	Keys.Sort();
	for (const int32 K : Keys) Signature = HashCombine(Signature, ::GetTypeHash(K));
	for (const FDoorLink& D : Doors)
	{
		Signature = HashCombine(Signature, ::GetTypeHash(FMath::RoundToInt32(D.Entry.X / 50.0)));
		Signature = HashCombine(Signature, ::GetTypeHash(FMath::RoundToInt32(D.Entry.Y / 50.0)));
		Signature = HashCombine(Signature, ::GetTypeHash(D.AccessTileX * 4096 + D.AccessTileY));
	}
	if (Signature == LastSignature && Actor.IsValid())
	{
		return 0;
	}
	Clear();
	LastSignature = Signature;
	LastRoadCount = Roads.Num();
	if (Roads.Num() == 0)
	{
		return 0;
	}

	auto Centre = [&](int32 X, int32 Y)
	{
		return FAnastasisVillagePresentation::SimToUnreal(World, X + 0.5, Y + 0.5, PresentationWorld);
	};
	auto IsRoad = [&](int32 X, int32 Y)
	{
		return X >= 0 && Y >= 0 && X < World.W && Y < World.H && Roads.Contains(Y * World.W + X);
	};

	// Les segments : cases foulees voisines (diagonale seulement si aucun detour orthogonal), puis portes.
	TArray<TPair<FVector, FVector>> Lines;
	for (const int32 Index : Keys)
	{
		const int32 X = Index % World.W;
		const int32 Y = Index / World.W;
		const FIntPoint Next[4] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
		for (const FIntPoint& N : Next)
		{
			const int32 NX = X + N.X;
			const int32 NY = Y + N.Y;
			if (!IsRoad(NX, NY)) continue;
			const bool bDiagonal = N.X != 0 && N.Y != 0;
			if (bDiagonal && (IsRoad(X + N.X, Y) || IsRoad(X, Y + N.Y))) continue;
			Lines.Add({Centre(X, Y), Centre(NX, NY)});
		}
	}
	for (const FDoorLink& D : Doors)
	{
		// La porte rejoint le sentier qui dessert sa case d'acces (la case elle-meme, ou une voisine foulee).
		int32 BestX = INDEX_NONE;
		int32 BestY = INDEX_NONE;
		double BestD = TNumericLimits<double>::Max();
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				const int32 X = D.AccessTileX + DX;
				const int32 Y = D.AccessTileY + DY;
				if (!IsRoad(X, Y)) continue;
				const double Dist = FVector::Dist2D(Centre(X, Y), D.Entry);
				if (Dist < BestD) { BestD = Dist; BestX = X; BestY = Y; }
			}
		}
		if (BestX != INDEX_NONE) Lines.Add({D.Entry, Centre(BestX, BestY)});
	}
	if (Lines.Num() == 0)
	{
		return 0;
	}

	// Le ruban drape : un point tous les 80 cm, la largeur qui respire, le sol releve de part et d'autre.
	TArray<FVector> V;
	TArray<int32> T;
	TArray<FVector> N;
	TArray<FVector2D> UV;
	TArray<FLinearColor> C;
	TArray<FProcMeshTangent> Tan;
	TArray<TPair<FVector2D, FVector2D>> Flat;
	TArray<double> Half;
	for (int32 L = 0; L < Lines.Num(); ++L)
	{
		const FVector A = Lines[L].Key;
		const FVector B = Lines[L].Value;
		const FVector2D Dir2 = FVector2D(B - A).GetSafeNormal();
		if (Dir2.IsNearlyZero()) continue;
		const FVector Side(-Dir2.Y, Dir2.X, 0.0);
		const double Len = FVector::Dist2D(A, B);
		const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Len / 80.0));
		const int32 Base = V.Num();
		double MaxHalf = 0.0;
		for (int32 S = 0; S <= Steps; ++S)
		{
			const double U = static_cast<double>(S) / Steps;
			FVector P = FMath::Lerp(A, B, U);
			const double W = 60.0 + 25.0 * Breath(P);
			MaxHalf = FMath::Max(MaxHalf, W);
			for (int32 K = -1; K <= 1; K += 2)
			{
				FVector Q = P + Side * (K * W);
				double Z = P.Z;
				if (!FAnastasisVillagePresentation::TraceGround(PresentationWorld, Q.X, Q.Y, Z))
				{
					FAnastasisVillagePresentation::TraceGround(PresentationWorld, P.X, P.Y, Z);
				}
				Q.Z = Z + 4.0 + 0.15 * L;  // un leger etagement : deux bandes qui se croisent ne scintillent pas
				V.Add(Q);
				N.Add(FVector::UpVector);
				UV.Add(FVector2D(U * Len / 100.0, (K * W) / 100.0));
				C.Add(Earth * (0.92f + 0.08f * static_cast<float>(Breath(Q * 3.0))));
				Tan.Add(FProcMeshTangent(FVector(Dir2.X, Dir2.Y, 0.0), false));
			}
			if (S > 0)
			{
				const int32 I0 = Base + (S - 1) * 2;
				T.Append({I0, I0 + 2, I0 + 1, I0 + 1, I0 + 2, I0 + 3});
			}
		}
		Flat.Add({FVector2D(A), FVector2D(B)});
		Half.Add(MaxHalf);
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Host = PresentationWorld->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Host)
	{
		return 0;
	}
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Host, TEXT("SettlementPaths"), RF_Transient);
	Host->SetRootComponent(Mesh);
	Mesh->RegisterComponent();
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->CreateMeshSection_LinearColor(0, V, T, N, UV, C, Tan, false);
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, PathMaterial))
	{
		Mesh->SetMaterial(0, Mat);
	}
#if WITH_EDITOR
	Host->SetActorLabel(TEXT("SettlementPaths"));
#endif
	Actor = Host;
	Segments = Flat.Num();

	// L'herbe sous la bande : echelle zero, l'original garde pour Clear.
	FBox2D Reach(ForceInit);
	for (int32 I = 0; I < Flat.Num(); ++I)
	{
		Reach += Flat[I].Key;
		Reach += Flat[I].Value;
	}
	Reach = Reach.ExpandBy(150.0);
	for (TActorIterator<AActor> It(PresentationWorld); It; ++It)
	{
		TArray<UHierarchicalInstancedStaticMeshComponent*> Hisms;
		It->GetComponents(Hisms);
		for (UHierarchicalInstancedStaticMeshComponent* Hism : Hisms)
		{
			if (!Hism || !Hism->GetName().StartsWith(TEXT("GroundCover_")) || Hism->GetInstanceCount() == 0) continue;
			const FBox B = Hism->Bounds.GetBox();
			if (!Reach.Intersect(FBox2D(FVector2D(B.Min), FVector2D(B.Max)))) continue;
			bool bAny = false;
			for (int32 I = 0; I < Hism->GetInstanceCount(); ++I)
			{
				FTransform Tr;
				if (!Hism->GetInstanceTransform(I, Tr, true) || Tr.GetScale3D().IsNearlyZero(1.e-4)) continue;
				const FVector2D P(Tr.GetLocation());
				if (!Reach.IsInside(P)) continue;
				for (int32 S = 0; S < Flat.Num(); ++S)
				{
					if (DistToSegment2D(P, Flat[S].Key, Flat[S].Value) < Half[S] + 15.0)
					{
						Hidden.Add({Hism, I, Tr});
						Tr.SetScale3D(FVector::ZeroVector);
						Hism->UpdateInstanceTransform(I, Tr, true, false, true);
						bAny = true;
						break;
					}
				}
			}
			if (bAny) Hism->MarkRenderStateDirty();
		}
	}
	UE_LOG(LogAnastasis_UnrealV2, Display,
		TEXT("ANASTASIS_SETTLEMENT paths road_tiles=%d segments=%d doors=%d vertices=%d grass_hidden=%d"),
		Roads.Num(), Segments, Doors.Num(), V.Num(), Hidden.Num());
	return Segments;
}
