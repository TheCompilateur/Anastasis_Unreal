#include "Village/AnastasisVillageFabricActor.h"

#include "Anastasis_UnrealV2.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Village/AnastasisVillageBuilding.h"
#include "Village/AnastasisVillagerVisual.h"


namespace AnastasisVillageFabric
{
namespace
{
	/** Assise de la calade au-dessus du sol rendu, et retombee de sa bordure. */
	constexpr double PaveLiftCm = 5.0;
	constexpr double KerbDropCm = 35.0;
	/** Hauteur d'une marche : au plus 18 cm, comme les escaliers de village. */
	constexpr double RiserCm = 18.0;
	/** Pierre seche : epaisseur, fruit (retrait du parement par cm de hauteur), joint. */
	constexpr double WallThickCm = 55.0;
	constexpr double WallBatter = 0.10;
	constexpr double JointCm = 2.5;

	uint32 Hash2(uint32 A, uint32 B)
	{
		uint32 H = A * 0x9E3779B1u ^ (B + 0x7F4A7C15u + (A << 6) + (A >> 2));
		H ^= H >> 15;
		H *= 0x2C1B3C6Du;
		return H ^ (H >> 13);
	}

	double Unit(uint32 H)
	{
		return (H & 0xFFFFu) / 65535.0;
	}

	struct FMeshBuffer
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UV0;
		TArray<FLinearColor> Colors;

		/** Triangle tourne vers `Facing` (convention du projet : CrossProduct(C - A, B - A) sort de la face). */
		void Tri(const FVector& A, const FVector& B, const FVector& C, const FVector& Facing, const FLinearColor& Color)
		{
			const FVector Cross = FVector::CrossProduct(C - A, B - A);
			if (Cross.SizeSquared() < 1.0e-6)
			{
				return;
			}
			const bool bSwap = FVector::DotProduct(Cross, Facing) < 0.0;
			const FVector N = (bSwap ? -Cross : Cross).GetSafeNormal();
			const FVector P[3] = { A, bSwap ? C : B, bSwap ? B : C };
			for (const FVector& V : P)
			{
				Triangles.Add(Vertices.Add(V));
				Normals.Add(N);
				// Projection planaire en metres : dessus en XY, parements en (X + Y, Z).
				UV0.Add(FMath::Abs(N.Z) > 0.6 ? FVector2D(V.X, V.Y) / 100.0 : FVector2D(V.X + V.Y, V.Z) / 100.0);
				Colors.Add(Color);
			}
		}

		void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Facing, const FLinearColor& Color)
		{
			Tri(A, B, C, Facing, Color);
			Tri(A, C, D, Facing, Color);
		}

		/** Pave oriente, cinq faces (le dessous ne se voit jamais). */
		void Block(const FVector& Origin, const FVector& Along, const FVector& Out, double Length, double Depth,
			double Height, double TopInset, const FLinearColor& Color)
		{
			const FVector Up = FVector::UpVector;
			const FVector B0 = Origin;
			const FVector B1 = Origin + Along * Length;
			const FVector B2 = B1 - Out * Depth;
			const FVector B3 = B0 - Out * Depth;
			const FVector Lift = Up * Height - Out * TopInset;
			const FVector T0 = B0 + Lift;
			const FVector T1 = B1 + Lift;
			const FVector T2 = B2 + Up * Height;
			const FVector T3 = B3 + Up * Height;
			Quad(T0, T1, T2, T3, Up, Color);          // dessus
			Quad(B0, B1, T1, T0, Out, Color);         // parement
			Quad(B3, B2, T2, T3, -Out, Color);        // queue
			Quad(B0, B3, T3, T0, -Along, Color);      // joint gauche
			Quad(B1, B2, T2, T1, Along, Color);       // joint droit
		}
	};

	FLinearColor StoneTint(uint32 H)
	{
		// Calcaire et gres de la cote : variation de valeur, a peine de teinte.
		const double V = 0.82 + 0.18 * Unit(H);
		const double Warm = 0.03 * Unit(H >> 16);
		return FLinearColor(V + Warm, V + Warm * 0.5, V, 1.0);
	}
}
}

AAnastasisVillageFabric::AAnastasisVillageFabric()
{
	PrimaryActorTick.bCanEverTick = false;
	Paving = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Paving"));
	RootComponent = Paving;
	Paving->bUseAsyncCooking = true;
	Paving->SetCastShadow(true);
	Walls = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Walls"));
	Walls->SetupAttachment(Paving);
	Walls->bUseAsyncCooking = true;
	Tree = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlazaTree"));
	Tree->SetupAttachment(Paving);
	Tree->SetMobility(EComponentMobility::Movable);
	Tree->SetVisibility(false);
	SetCanBeDamaged(false);
}

int32 AAnastasisVillageFabric::Apply(const AnastasisVillageFabric::FFabric& Fabric, AnastasisVillageFabric::FHeightFn Height, UMaterialInterface* PavingMaterial,
	UMaterialInterface* WallMaterial, UStaticMesh* PlazaTree, const bool bClear)
{
	using namespace AnastasisVillageFabric;
	RestoreCleared();
	Paving->ClearAllMeshSections();
	Walls->ClearAllMeshSections();
	Stones = 0;
	Triangles = 0;

	const FVector Up = FVector::UpVector;
	const auto Ground = [&](const FVector2D& P, double Fallback)
	{
		double Z = Fallback;
		return Height(P.X, P.Y, Z) ? Z : Fallback;
	};

	// --- Calades : ruban bombe, bordure qui retombe, marches au-dela de StepGrade -------------------
	FMeshBuffer Pave;
	for (const FLane& Lane : Fabric.Lanes)
	{
		const int32 Count = Lane.Points.Num();
		if (Count < 2)
		{
			continue;
		}
		const double Half = Lane.WidthCm * 0.5;
		TArray<FVector2D> Normals;
		for (int32 K = 0; K < Count; ++K)
		{
			const FVector2D Prev(Lane.Points[FMath::Max(0, K - 1)].Position);
			const FVector2D Next(Lane.Points[FMath::Min(Count - 1, K + 1)].Position);
			const FVector2D T = (Next - Prev).GetSafeNormal();
			Normals.Add(FVector2D(-T.Y, T.X));
		}
		const uint32 LaneSeed = StableHash(Lane.FromId);
		double Z = Lane.Points[0].Position.Z + PaveLiftCm;
		for (int32 K = 1; K < Count; ++K)
		{
			const FLanePoint& A = Lane.Points[K - 1];
			const FLanePoint& B = Lane.Points[K];
			const FVector2D A2(A.Position);
			const FVector2D B2(B.Position);
			const FLinearColor Tint = StoneTint(Hash2(LaneSeed, K));
			const double ZA = Z;
			const double ZB = B.Position.Z + PaveLiftCm;
			const FVector2D NA = Normals[K - 1] * Half;
			const FVector2D NB = Normals[K] * Half;
			if (!B.bStep)
			{
				// Rampe : deux bandes avec un faite au milieu (l'eau file vers les bords, comme une calade).
				const FVector LA(A2 - NA, ZA), MA(A2, ZA + 3.0), RA(A2 + NA, ZA);
				const FVector LB(B2 - NB, ZB), MB(B2, ZB + 3.0), RB(B2 + NB, ZB);
				Pave.Quad(LA, LB, MB, MA, Up, Tint);
				Pave.Quad(MA, MB, RB, RA, Up, Tint);
				// Bordures : elles descendent jusqu'au sol aval, et se perdent sous le talus amont.
				Pave.Quad(LA, LB, LB - Up * KerbDropCm, LA - Up * KerbDropCm, FVector(-NA, 0.0), Tint * 0.85f);
				Pave.Quad(RA, RB, RB - Up * KerbDropCm, RA - Up * KerbDropCm, FVector(NA, 0.0), Tint * 0.85f);
				Z = ZB;
				continue;
			}
			// Escalier : marches de RiserCm au plus, girons egaux sur le troncon.
			const double Rise = ZB - ZA;
			const int32 Steps = FMath::Clamp(FMath::CeilToInt32(FMath::Abs(Rise) / RiserCm), 1, 6);
			for (int32 S = 0; S < Steps; ++S)
			{
				const double T0 = static_cast<double>(S) / Steps;
				const double T1 = static_cast<double>(S + 1) / Steps;
				const FVector2D C0 = FMath::Lerp(A2, B2, T0);
				const FVector2D C1 = FMath::Lerp(A2, B2, T1);
				const FVector2D N0 = FMath::Lerp(NA, NB, T0);
				const FVector2D N1 = FMath::Lerp(NA, NB, T1);
				const double ZPrev = ZA + Rise * T0;
				const double ZTread = ZA + Rise * T1;
				// Contremarche (verticale) puis giron (horizontal) ; la contremarche regarde l'aval.
				const FVector Dir(FVector2D(B2 - A2).GetSafeNormal(), 0.0);
				const FVector Facing = Rise > 0.0 ? -Dir : Dir;
				const FLinearColor StepTint = StoneTint(Hash2(LaneSeed, K * 16 + S));
				Pave.Quad(FVector(C0 - N0, ZPrev), FVector(C0 + N0, ZPrev), FVector(C0 + N0, ZTread), FVector(C0 - N0, ZTread), Facing, StepTint * 0.9f);
				Pave.Quad(FVector(C0 - N0, ZTread), FVector(C1 - N1, ZTread), FVector(C1 + N1, ZTread), FVector(C0 + N0, ZTread), Up, StepTint);
				Pave.Quad(FVector(C0 - N0, ZTread), FVector(C1 - N1, ZTread), FVector(C1 - N1, ZTread - KerbDropCm), FVector(C0 - N0, ZTread - KerbDropCm), FVector(-N0, 0.0), StepTint * 0.8f);
				Pave.Quad(FVector(C0 + N0, ZTread), FVector(C1 + N1, ZTread), FVector(C1 + N1, ZTread - KerbDropCm), FVector(C0 + N0, ZTread - KerbDropCm), FVector(N0, 0.0), StepTint * 0.8f);
			}
			Z = ZB;
		}
	}

	// --- Placette : disque pave qui epouse le sol, rangs concentriques autour du puits ---------------
	if (Fabric.Plaza.bValid)
	{
		const int32 Sectors = 40;
		const int32 Rings = FMath::Max(2, FMath::CeilToInt32(Fabric.Plaza.RadiusCm / 100.0));
		const FVector2D C(Fabric.Plaza.Centre);
		TArray<FVector> Grid;
		for (int32 R = 0; R <= Rings; ++R)
		{
			const double Radius = Fabric.Plaza.RadiusCm * R / Rings;
			for (int32 S = 0; S < Sectors; ++S)
			{
				const double Angle = 2.0 * PI * S / Sectors;
				const FVector2D P = C + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
				Grid.Add(FVector(P, Ground(P, Fabric.Plaza.LevelCm) + PaveLiftCm));
			}
		}
		for (int32 R = 0; R < Rings; ++R)
		{
			for (int32 S = 0; S < Sectors; ++S)
			{
				const int32 S1 = (S + 1) % Sectors;
				const FLinearColor Tint = StoneTint(Hash2(StableHash(Fabric.Plaza.WellId), R * Sectors + S));
				Pave.Quad(Grid[R * Sectors + S], Grid[(R + 1) * Sectors + S], Grid[(R + 1) * Sectors + S1], Grid[R * Sectors + S1], Up, Tint);
			}
		}
		for (int32 S = 0; S < Sectors; ++S)
		{
			const FVector A = Grid[Rings * Sectors + S];
			const FVector B = Grid[Rings * Sectors + (S + 1) % Sectors];
			const FVector Out = FVector(FVector2D((A + B) * 0.5) - C, 0.0).GetSafeNormal();
			Pave.Quad(A, B, B - Up * KerbDropCm, A - Up * KerbDropCm, Out, StoneTint(Hash2(S, 77)) * 0.85f);
		}
	}

	// --- Murets de pierre seche : assises de 20 a 30 cm, pierres de 35 a 75 cm, fruit, couronnement --
	FMeshBuffer Stone;
	for (const FWallRun& Wall : Fabric.Walls)
	{
		const uint32 WallSeed = StableHash(Wall.PlotId) ^ (Wall.bRetaining ? 0x51u : 0xC3u);
		const FVector Out(Wall.Face, 0.0);
		for (int32 K = 1; K < Wall.Bottom.Num(); ++K)
		{
			const FVector A = Wall.Bottom[K - 1];
			const FVector B = Wall.Bottom[K];
			const FVector Along = FVector(FVector2D(B - A), 0.0).GetSafeNormal();
			const double SegLen = FVector2D::Distance(FVector2D(A), FVector2D(B));
			const double BaseZ = FMath::Min(A.Z, B.Z) - 10.0; // le pied s'enterre
			const double TopZ = 0.5 * (Wall.Top[K - 1].Z + Wall.Top[K].Z);
			// Parement aligne sur le bord de la terrasse ; la queue rentre dans le remblai.
			const FVector Base(FVector2D(A), BaseZ);
			double CourseZ = BaseZ;
			int32 Course = 0;
			while (CourseZ < TopZ - 4.0 && Course < 16)
			{
				const uint32 HC = Hash2(WallSeed, K * 32 + Course);
				const bool bCoping = TopZ - CourseZ <= 34.0;
				const double CourseH = bCoping ? TopZ - CourseZ : 20.0 + 10.0 * Unit(HC);
				// Joints croises : chaque assise commence decalee.
				double X = -Unit(HC >> 8) * 30.0;
				int32 StoneIndex = 0;
				while (X < SegLen)
				{
					const uint32 HS = Hash2(HC, StoneIndex++);
					const double Len = bCoping ? 45.0 + 25.0 * Unit(HS) : 35.0 + 40.0 * Unit(HS);
					const double X0 = FMath::Max(0.0, X);
					const double X1 = FMath::Min(SegLen, X + Len);
					if (X1 - X0 > 8.0)
					{
						const double Rel = CourseZ - BaseZ;
						const double Setback = Rel * WallBatter + (Unit(HS >> 16) - 0.5) * 4.0;
						const FVector Origin = Base + Along * (X0 + JointCm * 0.5) - Out * Setback + Up * Rel;
						const double StoneH = FMath::Max(6.0, CourseH - JointCm);
						const double Depth = bCoping ? WallThickCm + 8.0 : WallThickCm;
						Stone.Block(Origin + (bCoping ? Out * 4.0 : FVector::ZeroVector), Along, Out, X1 - X0 - JointCm, Depth,
							StoneH, bCoping ? 0.0 : StoneH * WallBatter, StoneTint(HS) * (bCoping ? 1.0f : 0.92f));
						++Stones;
					}
					X += Len;
				}
				CourseZ += CourseH;
				++Course;
			}
		}
	}

	if (Pave.Vertices.Num() > 0)
	{
		Paving->CreateMeshSection_LinearColor(0, Pave.Vertices, Pave.Triangles, Pave.Normals, Pave.UV0, Pave.Colors, {}, true);
		Paving->SetMaterial(0, PavingMaterial);
	}
	if (Stone.Vertices.Num() > 0)
	{
		Walls->CreateMeshSection_LinearColor(0, Stone.Vertices, Stone.Triangles, Stone.Normals, Stone.UV0, Stone.Colors, {}, true);
		Walls->SetMaterial(0, WallMaterial);
	}
	Triangles = (Pave.Triangles.Num() + Stone.Triangles.Num()) / 3;

	// --- Le platane de la placette ------------------------------------------------------------------
	const bool bTree = Fabric.Plaza.bHasTree && PlazaTree;
	Tree->SetStaticMesh(bTree ? PlazaTree : nullptr);
	Tree->SetVisibility(bTree);
	if (bTree)
	{
		const double Yaw = (StableHash(Fabric.Plaza.WellId) % 360);
		Tree->SetWorldLocationAndRotation(Fabric.Plaza.TreeSpot, FRotator(0.0, Yaw, 0.0));
	}

	return bClear ? ClearUnder(Fabric) : 0;
}

int32 AAnastasisVillageFabric::ClearUnder(const AnastasisVillageFabric::FFabric& Fabric)
{
	using namespace AnastasisVillageFabric;
	UWorld* World = GetWorld();
	if (!World || Fabric.GridW <= 0)
	{
		return 0;
	}
	const FBox Area(
		FVector(Fabric.GridOrigin, -1.0e7),
		FVector(Fabric.GridOrigin + FVector2D(Fabric.GridW, Fabric.GridH) * Fabric.GridCm, 1.0e7));
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this || Actor->IsA<AAnastasisVillageBuilding>() || Actor->IsA<AAnastasisVillagerVisual>())
		{
			continue;
		}
		TArray<UInstancedStaticMeshComponent*> Components;
		Actor->GetComponents(Components);
		for (UInstancedStaticMeshComponent* Component : Components)
		{
			if (!Component || !Component->Bounds.GetBox().Intersect(Area))
			{
				continue;
			}
			bool bTouched = false;
			const int32 Count = Component->GetInstanceCount();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FTransform Instance;
				if (!Component->GetInstanceTransform(Index, Instance, true))
				{
					continue;
				}
				const FVector Location = Instance.GetLocation();
				if (Instance.GetScale3D().IsNearlyZero() || !Fabric.IsPavedAt(Location.X, Location.Y))
				{
					continue;
				}
				Cleared.Add({ Component, Index, Instance });
				FTransform Hidden = Instance;
				Hidden.SetScale3D(FVector::ZeroVector);
				Component->UpdateInstanceTransform(Index, Hidden, true, false, true);
				bTouched = true;
			}
			if (bTouched)
			{
				Component->MarkRenderStateDirty();
			}
		}
	}
	return Cleared.Num();
}

int32 AAnastasisVillageFabric::RestoreCleared()
{
	int32 Restored = 0;
	TSet<UInstancedStaticMeshComponent*> Touched;
	for (const FClearedInstance& Entry : Cleared)
	{
		UInstancedStaticMeshComponent* Component = Entry.Component.Get();
		if (!Component || Entry.Index >= Component->GetInstanceCount())
		{
			continue;
		}
		FTransform Current;
		// Rendre seulement ce qui est encore tel qu'on l'a laisse : un autre systeme a pu reecrire l'instance.
		if (Component->GetInstanceTransform(Entry.Index, Current, true) && Current.GetScale3D().IsNearlyZero())
		{
			Component->UpdateInstanceTransform(Entry.Index, Entry.Original, true, false, true);
			Touched.Add(Component);
			++Restored;
		}
	}
	for (UInstancedStaticMeshComponent* Component : Touched)
	{
		Component->MarkRenderStateDirty();
	}
	Cleared.Reset();
	return Restored;
}

void AAnastasisVillageFabric::EndPlay(const EEndPlayReason::Type Reason)
{
	RestoreCleared();
	Super::EndPlay(Reason);
}
