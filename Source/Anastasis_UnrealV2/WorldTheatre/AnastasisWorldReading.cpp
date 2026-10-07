#include "WorldTheatre/AnastasisWorldReading.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "WorldView/AnastasisWorldEmbodiment.h"

namespace AnastasisWorldReading
{
void FRaster::Init(const FVector2D& InOrigin, double InCell, int32 InW, int32 InH)
{
	Origin = InOrigin;
	Cell = InCell;
	W = InW;
	H = InH;
	Ground.Init(NoData, W * H);
	Water.Init(NoData, W * H);
}

int32 FRaster::FilledCount(const TArray<float>& Layer) const
{
	int32 N = 0;
	for (const float V : Layer) N += V > NoData ? 1 : 0;
	return N;
}

const TCHAR* FamilyName(EFamily Family)
{
	switch (Family)
	{
	case EFamily::Tree: return TEXT("tree");
	case EFamily::Shrub: return TEXT("shrub");
	case EFamily::Rock: return TEXT("rock");
	case EFamily::Building: return TEXT("building");
	case EFamily::Ruin: return TEXT("ruin");
	case EFamily::Place: return TEXT("place");
	case EFamily::Reed: return TEXT("reed");
	default: return TEXT("other");
	}
}

EFamily FamilyOfMesh(const FString& MeshPath, bool& bIgnore)
{
	bIgnore = MeshPath.Contains(TEXT("/GroundCover/")) || MeshPath.Contains(TEXT("SM_Grass_"))
		|| MeshPath.Contains(TEXT("/Weather/")) || MeshPath.Contains(TEXT("/Engine/"));
	if (MeshPath.Contains(TEXT("SM_Tree_")) || MeshPath.Contains(TEXT("/Vegetation/Hero/"))) return EFamily::Tree;
	if (MeshPath.Contains(TEXT("SM_Shrub_")) || MeshPath.Contains(TEXT("Bush")) || MeshPath.Contains(TEXT("Sapling"))) return EFamily::Shrub;
	if (MeshPath.Contains(TEXT("Reed"))) return EFamily::Reed;
	if (MeshPath.Contains(TEXT("SM_Ruin_"))) return EFamily::Ruin;
	if (MeshPath.Contains(TEXT("/VillageBuildings/")) || MeshPath.Contains(TEXT("/Architecture/"))
		|| MeshPath.Contains(TEXT("Shelter"))) return EFamily::Building;
	if (MeshPath.Contains(TEXT("/Rock/")) || MeshPath.Contains(TEXT("/Lithos/")) || MeshPath.Contains(TEXT("RockCluster"))
		|| MeshPath.Contains(TEXT("BuriedBlock"))) return EFamily::Rock;
	if (MeshPath.Contains(TEXT("/WorldDressing01/")) || MeshPath.Contains(TEXT("/RefugeeProps008/"))) return EFamily::Place;
	return EFamily::Other;
}

namespace
{
/** Rasterise un triangle (positions monde) : chaque cellule dont le centre est dedans garde le Z le plus haut. */
void RasterTriangle(FRaster& R, TArray<float>& Layer, const FVector& A, const FVector& B, const FVector& C)
{
	const double MinX = FMath::Min3(A.X, B.X, C.X), MaxX = FMath::Max3(A.X, B.X, C.X);
	const double MinY = FMath::Min3(A.Y, B.Y, C.Y), MaxY = FMath::Max3(A.Y, B.Y, C.Y);
	const int32 I0 = FMath::Max(0, FMath::CeilToInt32((MinX - R.Origin.X) / R.Cell - 0.5));
	const int32 I1 = FMath::Min(R.W - 1, FMath::FloorToInt32((MaxX - R.Origin.X) / R.Cell - 0.5));
	const int32 J0 = FMath::Max(0, FMath::CeilToInt32((MinY - R.Origin.Y) / R.Cell - 0.5));
	const int32 J1 = FMath::Min(R.H - 1, FMath::FloorToInt32((MaxY - R.Origin.Y) / R.Cell - 0.5));
	if (I0 > I1 || J0 > J1) return;
	const double D = (B.Y - C.Y) * (A.X - C.X) + (C.X - B.X) * (A.Y - C.Y);
	if (FMath::Abs(D) < 1e-9) return;
	constexpr double Eps = -1e-7;
	for (int32 J = J0; J <= J1; ++J)
	{
		const double Y = R.Origin.Y + (J + 0.5) * R.Cell;
		for (int32 I = I0; I <= I1; ++I)
		{
			const double X = R.Origin.X + (I + 0.5) * R.Cell;
			const double L0 = ((B.Y - C.Y) * (X - C.X) + (C.X - B.X) * (Y - C.Y)) / D;
			const double L1 = ((C.Y - A.Y) * (X - C.X) + (A.X - C.X) * (Y - C.Y)) / D;
			const double L2 = 1.0 - L0 - L1;
			if (L0 < Eps || L1 < Eps || L2 < Eps) continue;
			const float Z = static_cast<float>(L0 * A.Z + L1 * B.Z + L2 * C.Z);
			float& Cell = Layer[R.Index(I, J)];
			if (Z > Cell) Cell = Z;
		}
	}
}

/** Rasterise une section ; renvoie son nombre de triangles. */
int32 RasterSection(const UProceduralMeshComponent& Comp, int32 Section, FRaster& Near, FRaster& Far, bool bWater)
{
	const FProcMeshSection* S = const_cast<UProceduralMeshComponent&>(Comp).GetProcMeshSection(Section);
	if (!S || S->ProcIndexBuffer.Num() < 3) return 0;
	const FTransform& T = Comp.GetComponentTransform();
	TArray<FVector> P;
	P.Reserve(S->ProcVertexBuffer.Num());
	for (const FProcMeshVertex& V : S->ProcVertexBuffer) P.Add(T.TransformPosition(V.Position));
	const int32 Tris = S->ProcIndexBuffer.Num() / 3;
	for (int32 K = 0; K < Tris; ++K)
	{
		const FVector& A = P[S->ProcIndexBuffer[3 * K]];
		const FVector& B = P[S->ProcIndexBuffer[3 * K + 1]];
		const FVector& C = P[S->ProcIndexBuffer[3 * K + 2]];
		RasterTriangle(Near, bWater ? Near.Water : Near.Ground, A, B, C);
		RasterTriangle(Far, bWater ? Far.Water : Far.Ground, A, B, C);
	}
	return Tris;
}

FBox SectionBounds(const UProceduralMeshComponent& Comp, int32 Section)
{
	FBox B(ForceInit);
	const FProcMeshSection* S = const_cast<UProceduralMeshComponent&>(Comp).GetProcMeshSection(Section);
	if (!S) return B;
	const FTransform& T = Comp.GetComponentTransform();
	for (const FProcMeshVertex& V : S->ProcVertexBuffer) B += T.TransformPosition(V.Position);
	return B;
}

void AddPlaced(FReading& Out, const UStaticMesh* Mesh, const FTransform& T)
{
	if (!Mesh) return;
	bool bIgnore = false;
	const FString Path = Mesh->GetPathName();
	const EFamily Family = FamilyOfMesh(Path, bIgnore);
	if (bIgnore) { ++Out.IgnoredInstances; return; }
	const FBox Box = Mesh->GetBoundingBox();
	const FVector S = T.GetScale3D().GetAbs();
	FPlaced& P = Out.Placed.AddDefaulted_GetRef();
	P.Family = Family;
	P.Mesh = Mesh->GetFName();
	P.Location = T.GetLocation();
	P.Height = static_cast<float>(Box.GetSize().Z * S.Z);
	P.Radius = static_cast<float>(0.25 * (Box.GetSize().X * S.X + Box.GetSize().Y * S.Y));
}
}

bool Read(UWorld* World, double NearCellUu, double NearMarginUu, double FarCellUu, FReading& Out, FString& Why)
{
	Out = FReading();
	if (!World) { Why = TEXT("pas de monde"); return false; }
	AAnastasisWorldEmbodiment* Embodiment = nullptr;
	for (TActorIterator<AAnastasisWorldEmbodiment> It(World); It; ++It)
	{
		if (IsValid(*It)) { Embodiment = *It; break; }
	}
	if (!Embodiment) { Why = TEXT("aucune incarnation (AAnastasisWorldEmbodiment)"); return false; }

	// Le sol forge et l'anneau : les deux maillages proceduraux de l'incarnation, par nom de sous-objet.
	UProceduralMeshComponent* Ground = nullptr;
	UProceduralMeshComponent* Horizon = nullptr;
	TArray<UProceduralMeshComponent*> Procs;
	Embodiment->GetComponents(Procs);
	for (UProceduralMeshComponent* C : Procs)
	{
		if (!C || !C->IsVisible()) continue;
		if (C->GetFName() == TEXT("HorizonTerrain")) Horizon = C;
		else if (C->GetFName() == TEXT("ExperimentalTerrain")) Ground = C;
	}
	if (!Ground) { Why = TEXT("pas de sol procedural visible"); return false; }
	Out.MapFootprint = SectionBounds(*Ground, 0);
	if (!Out.MapFootprint.IsValid) { Why = TEXT("sol procedural vide"); return false; }
	FBox Whole = Out.MapFootprint;
	if (Horizon)
	{
		for (int32 S = 0; S < Horizon->GetNumSections(); ++S)
		{
			const FBox B = SectionBounds(*Horizon, S);
			if (B.IsValid) Whole += B;
		}
	}
	Out.Seed = Embodiment->GetSnapshot().Seed;
	Out.Basin = Embodiment->GetTerrainForgeBasin();
	Out.Landmark = Embodiment->GetTerrainForgeLandmark();

	const FVector2D MapMin(Out.MapFootprint.Min.X - NearMarginUu, Out.MapFootprint.Min.Y - NearMarginUu);
	const FVector2D MapMax(Out.MapFootprint.Max.X + NearMarginUu, Out.MapFootprint.Max.Y + NearMarginUu);
	Out.Near.Init(MapMin, NearCellUu,
		FMath::CeilToInt32((MapMax.X - MapMin.X) / NearCellUu), FMath::CeilToInt32((MapMax.Y - MapMin.Y) / NearCellUu));
	Out.Far.Init(FVector2D(Whole.Min.X, Whole.Min.Y), FarCellUu,
		FMath::CeilToInt32((Whole.Max.X - Whole.Min.X) / FarCellUu), FMath::CeilToInt32((Whole.Max.Y - Whole.Min.Y) / FarCellUu));

	// Sections (AnastasisWorldEmbodiment) : carte 0 = sol, 1 = nappe, 2 = rubans de riviere ;
	// anneau 0 = sol proche, 1 = eau, 2 = sol lointain (M_AnastasisFarTerrain).
	for (int32 S = 0; S < Ground->GetNumSections(); ++S)
	{
		const bool bWater = S != 0;
		const int32 N = RasterSection(*Ground, S, Out.Near, Out.Far, bWater);
		(bWater ? Out.WaterTriangles : Out.GroundTriangles) += N;
	}
	if (Horizon)
	{
		for (int32 S = 0; S < Horizon->GetNumSections(); ++S)
		{
			const bool bWater = S == 1;
			const int32 N = RasterSection(*Horizon, S, Out.Near, Out.Far, bWater);
			(bWater ? Out.WaterTriangles : Out.HorizonTriangles) += N;
		}
	}

	// Ce qui est pose : tous les acteurs du monde, instances comprises. L'herbe est comptee, pas listee.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UStaticMeshComponent*> Meshes;
		It->GetComponents(Meshes);
		for (UStaticMeshComponent* C : Meshes)
		{
			if (!C || !C->IsVisible() || !C->GetStaticMesh()) continue;
			++Out.MeshComponents;
			if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(C))
			{
				const int32 N = ISM->GetInstanceCount();
				for (int32 K = 0; K < N; ++K)
				{
					FTransform T;
					if (ISM->GetInstanceTransform(K, T, true)) AddPlaced(Out, ISM->GetStaticMesh(), T);
				}
			}
			else
			{
				AddPlaced(Out, C->GetStaticMesh(), C->GetComponentTransform());
			}
		}
	}
	return true;
}

namespace
{
bool SaveFloats(const TArray<float>& V, const FString& Path)
{
	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(V.Num() * sizeof(float));
	FMemory::Memcpy(Bytes.GetData(), V.GetData(), Bytes.Num());
	return FFileHelper::SaveArrayToFile(Bytes, *Path);
}

FString RasterJson(const FRaster& R, const TCHAR* Prefix)
{
	return FString::Printf(TEXT("{\"origin\":[%.3f,%.3f],\"cell\":%.3f,\"w\":%d,\"h\":%d,\"ground\":\"%s_ground.f32\",\"water\":\"%s_water.f32\","
		"\"ground_cells\":%d,\"water_cells\":%d}"),
		R.Origin.X, R.Origin.Y, R.Cell, R.W, R.H, Prefix, Prefix, R.FilledCount(R.Ground), R.FilledCount(R.Water));
}
}

bool Dump(const FReading& Reading, const FString& Dir, FString& Why)
{
	IFileManager::Get().MakeDirectory(*Dir, true);
	if (!SaveFloats(Reading.Near.Ground, Dir / TEXT("near_ground.f32")) || !SaveFloats(Reading.Near.Water, Dir / TEXT("near_water.f32"))
		|| !SaveFloats(Reading.Far.Ground, Dir / TEXT("far_ground.f32")) || !SaveFloats(Reading.Far.Water, Dir / TEXT("far_water.f32")))
	{
		Why = FString::Printf(TEXT("ecriture des grilles impossible dans %s"), *Dir);
		return false;
	}
	int32 Counts[static_cast<int32>(EFamily::Count)] = {};
	FString Csv = TEXT("family,mesh,x,y,z,height,radius\n");
	for (const FPlaced& P : Reading.Placed)
	{
		++Counts[static_cast<int32>(P.Family)];
		Csv += FString::Printf(TEXT("%s,%s,%.1f,%.1f,%.1f,%.1f,%.1f\n"), FamilyName(P.Family), *P.Mesh.ToString(),
			P.Location.X, P.Location.Y, P.Location.Z, P.Height, P.Radius);
	}
	FString Families;
	for (int32 F = 0; F < static_cast<int32>(EFamily::Count); ++F)
	{
		Families += FString::Printf(TEXT("%s\"%s\":%d"), F ? TEXT(",") : TEXT(""), FamilyName(static_cast<EFamily>(F)), Counts[F]);
	}
	const FBox& M = Reading.MapFootprint;
	const FString Json = FString::Printf(TEXT("{\"seed\":%u,\"map\":{\"min\":[%.1f,%.1f,%.1f],\"max\":[%.1f,%.1f,%.1f]},"
		"\"basin\":[%.1f,%.1f,%.1f],\"landmark\":[%.1f,%.1f,%.1f],\"near\":%s,\"far\":%s,\"families\":{%s},"
		"\"ignored_instances\":%d,\"mesh_components\":%d,\"ground_triangles\":%d,\"horizon_triangles\":%d,\"water_triangles\":%d}\n"),
		Reading.Seed, M.Min.X, M.Min.Y, M.Min.Z, M.Max.X, M.Max.Y, M.Max.Z,
		Reading.Basin.X, Reading.Basin.Y, Reading.Basin.Z, Reading.Landmark.X, Reading.Landmark.Y, Reading.Landmark.Z,
		*RasterJson(Reading.Near, TEXT("near")), *RasterJson(Reading.Far, TEXT("far")), *Families,
		Reading.IgnoredInstances, Reading.MeshComponents, Reading.GroundTriangles, Reading.HorizonTriangles, Reading.WaterTriangles);
	if (!FFileHelper::SaveStringToFile(Csv, *(Dir / TEXT("placed.csv"))) || !FFileHelper::SaveStringToFile(Json, *(Dir / TEXT("reading.json"))))
	{
		Why = FString::Printf(TEXT("ecriture du releve impossible dans %s"), *Dir);
		return false;
	}
	return true;
}
}
