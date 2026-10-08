#include "WorldView/AnastasisDrainage.h"

#include "WorldView/AnastasisHumanGeography.h"
#include "World/AnastasisWorldNoise.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"
#include "Algo/Reverse.h"
#include "Containers/SortedMap.h"

static TAutoConsoleVariable<int32> CVarDrainage(
	TEXT("anastasis.Terrain.Drainage"),
	1,
	TEXT("HYDRO_NETWORK_001: 1 = graded drainage network on the forged relief (default), 0 = previous water (sea-level trenches). Applied on embodiment."),
	ECVF_Default);

static TAutoConsoleVariable<FString> CVarDrainageDump(
	TEXT("anastasis.Drainage.Dump"),
	TEXT(""),
	TEXT("If set, path of a JSON file where each embodiment writes the drainage network (rivers, points, lakes, wetlands)."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarDrainageDebug(
	TEXT("anastasis.Drainage.Debug"),
	0,
	TEXT("Drainage network debug lines: 0 off, 1 width, 2 depth, 3 velocity, 4 Strahler order. Applied on embodiment."),
	ECVF_Default);

// Espace nomme, pas anonyme : en build unity, plusieurs .cpp partagent une unite de
// traduction, et un SmoothStep anonyme de meme signature que celui de TERRAIN_FORGE
// entrerait en collision le jour ou les deux fichiers tombent dans le meme lot. Imbrique
// dans AnastasisDrainage : visible des seules definitions de ce module, sans using global.
namespace AnastasisDrainage::Detail
{
using AnastasisWorldView::FWorldVisualSnapshot;

AnastasisDrainage::FNetwork GActiveNetwork;

constexpr int32 Off4X[4] = {1, -1, 0, 0};
constexpr int32 Off4Y[4] = {0, 0, 1, -1};
constexpr int32 Off8X[8] = {1, -1, 0, 0, 1, 1, -1, -1};
constexpr int32 Off8Y[8] = {0, 0, 1, -1, 1, -1, 1, -1};

double SmoothStep(double A, double B, double X)
{
	const double T = FMath::Clamp((X - A) / FMath::Max(B - A, 1.e-9), 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T);
}

/** Tas binaire (priorite, index) ; egalite departagee par l'index : routage deterministe. */
struct FHeap
{
	TArray<TPair<double, int32>> A;
	static bool Less(const TPair<double, int32>& L, const TPair<double, int32>& R)
	{
		return L.Key < R.Key || (L.Key == R.Key && L.Value < R.Value);
	}
	void Push(int32 I, double P)
	{
		A.Emplace(P, I);
		int32 K = A.Num() - 1;
		while (K > 0)
		{
			const int32 Q = (K - 1) >> 1;
			if (!Less(A[K], A[Q])) break;
			A.Swap(K, Q);
			K = Q;
		}
	}
	int32 Pop()
	{
		const int32 Top = A[0].Value;
		A[0] = A.Last();
		A.Pop(EAllowShrinking::No);
		int32 K = 0;
		for (;;)
		{
			const int32 L = 2 * K + 1, R = L + 1;
			int32 M = K;
			if (L < A.Num() && Less(A[L], A[M])) M = L;
			if (R < A.Num() && Less(A[R], A[M])) M = R;
			if (M == K) break;
			A.Swap(K, M);
			K = M;
		}
		return Top;
	}
	int32 Num() const { return A.Num(); }
};

struct FGrid
{
	int32 W = 0, H = 0, N = 0;
	double S = 0.0;          // uu entre deux sommets
	double X0 = 0.0, Y0 = 0.0;
	bool In(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < W && Y < H; }
	bool Border(int32 I) const { const int32 X = I % W, Y = I / W; return X == 0 || Y == 0 || X == W - 1 || Y == H - 1; }
	int32 EdgeDist(int32 I) const { const int32 X = I % W, Y = I / W; return FMath::Min(FMath::Min(X, Y), FMath::Min(W - 1 - X, H - 1 - Y)); }
	double WX(int32 I) const { return X0 + (I % W) * S; }
	double WY(int32 I) const { return Y0 + (I / W) * S; }
	/** Bilineaire sur un champ de sommets, XY monde ; hors grille = bord. */
	double Sample(const TArray<double>& F, double X, double Y) const
	{
		const double U = FMath::Clamp((X - X0) / S, 0.0, W - 1.0), V = FMath::Clamp((Y - Y0) / S, 0.0, H - 1.0);
		const int32 IX = FMath::Min(static_cast<int32>(U), W - 2), IY = FMath::Min(static_cast<int32>(V), H - 2);
		const double FX = U - IX, FY = V - IY;
		const int32 A = IY * W + IX;
		return FMath::Lerp(FMath::Lerp(F[A], F[A + 1], FX), FMath::Lerp(F[A + W], F[A + W + 1], FX), FY);
	}
};

/** Plus proche point d'une polyligne (XY), Z interpole. */
struct FNearest { double Dist = TNumericLimits<double>::Max(); double Z = 0.0; FVector2D P = FVector2D::ZeroVector; };
FNearest NearestOn(const TArray<FVector>& Poly, double X, double Y)
{
	FNearest Out;
	for (int32 K = 0; K + 1 < Poly.Num(); ++K)
	{
		const FVector2D A(Poly[K].X, Poly[K].Y), B(Poly[K + 1].X, Poly[K + 1].Y), D = B - A, P(X, Y);
		const double T = FMath::Clamp(FVector2D::DotProduct(P - A, D) / FMath::Max(D.SizeSquared(), 1.e-9), 0.0, 1.0);
		const FVector2D Q = A + T * D;
		const double Dist = FVector2D::Distance(P, Q);
		if (Dist < Out.Dist) { Out.Dist = Dist; Out.Z = FMath::Lerp(Poly[K].Z, Poly[K + 1].Z, T); Out.P = Q; }
	}
	return Out;
}

/**
 * Remplissage harmonique des cellules de Mask depuis leurs voisines hors masque :
 * initialisation par front BFS (moyenne des voisins deja connus), puis SOR.
 * Ordre de parcours fixe : deterministe.
 */
void Harmonic(const FGrid& G, TArray<double>& F, const TArray<uint8>& Mask, int32 Iterations)
{
	TArray<uint8> Known, Queued;
	Known.SetNumZeroed(G.N);
	Queued.SetNumZeroed(G.N);
	TArray<int32> Queue, Cells;
	for (int32 I = 0; I < G.N; ++I)
	{
		if (!Mask[I]) { Known[I] = 1; continue; }
		Cells.Add(I);
		for (int32 K = 0; K < 4; ++K)
		{
			const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
			if (G.In(X, Y) && !Mask[Y * G.W + X]) { Queue.Add(I); Queued[I] = 1; break; }
		}
	}
	for (int32 Q = 0; Q < Queue.Num(); ++Q)
	{
		const int32 I = Queue[Q];
		double Sum = 0.0; int32 Count = 0;
		for (int32 K = 0; K < 4; ++K)
		{
			const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
			if (!G.In(X, Y)) continue;
			const int32 J = Y * G.W + X;
			if (Known[J]) { Sum += F[J]; ++Count; }
			else if (!Queued[J]) { Queued[J] = 1; Queue.Add(J); }
		}
		if (Count > 0) F[I] = Sum / Count;
		Known[I] = 1;
	}
	for (int32 It = 0; It < Iterations; ++It)
	{
		for (const int32 I : Cells)
		{
			double Sum = 0.0; int32 Count = 0;
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
				if (G.In(X, Y)) { Sum += F[Y * G.W + X]; ++Count; }
			}
			F[I] += 1.85 * (Sum / Count - F[I]);
		}
	}
}

/** Priority-flood (Barnes) depuis le bord : surface sans depression, pente epsilon sur les plats. */
TArray<double> PriorityFlood(const FGrid& G, const TArray<double>& Surface)
{
	TArray<double> F;
	F.SetNumUninitialized(G.N);
	TArray<uint8> Closed;
	Closed.SetNumZeroed(G.N);
	FHeap Heap;
	Heap.A.Reserve(G.N);
	for (int32 I = 0; I < G.N; ++I)
	{
		if (G.Border(I)) { F[I] = Surface[I]; Closed[I] = 1; Heap.Push(I, F[I]); }
	}
	while (Heap.Num() > 0)
	{
		const int32 C = Heap.Pop();
		for (int32 K = 0; K < 8; ++K)
		{
			const int32 X = C % G.W + Off8X[K], Y = C / G.W + Off8Y[K];
			if (!G.In(X, Y)) continue;
			const int32 J = Y * G.W + X;
			if (Closed[J]) continue;
			Closed[J] = 1;
			F[J] = FMath::Max(Surface[J], F[C] + 1.e-3);
			Heap.Push(J, F[J]);
		}
	}
	return F;
}

struct FLakeWork
{
	TArray<int32> Cells;
	double Level = 0.0;
	bool bBorder = false;
	/** Touche une riviere ecrite de Human_Geography_V2 : niveau et rive d'origine gardes. */
	bool bAuthored = false;
	int32 Spill = INDEX_NONE;
	/** Lac dans lequel celui-ci deborde directement, sans chenal entre les deux. */
	int32 SpillsInto = INDEX_NONE;
};

struct FRiverWork
{
	TArray<int32> Cells;
	int32 Parent = INDEX_NONE;
	int32 Join = INDEX_NONE;   // cellule du parent ou se fait la confluence
	EMouth Mouth = EMouth::Border;
	int32 MouthLake = INDEX_NONE;
	int32 SourceLake = INDEX_NONE;
	bool bDead = false;
};

/** Point de polyligne en cours de construction : attributs de cellule. */
struct FPt
{
	FVector2D P;
	double Ws = 0.0, Area = 0.0, Hill = 0.0;
	int32 Order = 1;
	bool bAuthored = false;
};

TArray<FPt> Resample(const TArray<FPt>& In, double Step)
{
	TArray<FPt> Out;
	if (In.Num() < 2) return In;
	Out.Add(In[0]);
	double Carry = 0.0;
	for (int32 K = 0; K + 1 < In.Num(); ++K)
	{
		const double Len = FVector2D::Distance(In[K].P, In[K + 1].P);
		double T = Step - Carry;
		while (T < Len)
		{
			const double A = T / Len;
			FPt Q = In[K];
			Q.P = FMath::Lerp(In[K].P, In[K + 1].P, A);
			Q.Ws = FMath::Lerp(In[K].Ws, In[K + 1].Ws, A);
			Q.Area = FMath::Lerp(In[K].Area, In[K + 1].Area, A);
			Q.Hill = FMath::Lerp(In[K].Hill, In[K + 1].Hill, A);
			Q.Order = A < 0.5 ? In[K].Order : In[K + 1].Order;
			Q.bAuthored = In[K].bAuthored && In[K + 1].bAuthored;
			Out.Add(Q);
			T += Step;
		}
		Carry = Len - (T - Step);
	}
	if (FVector2D::Distance(Out.Last().P, In.Last().P) > Step * 0.3) Out.Add(In.Last());
	else Out.Last() = In.Last();
	return Out;
}

void SmoothXY(TArray<FPt>& Pts, int32 Passes, bool bPinLast)
{
	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		TArray<FPt> Copy = Pts;
		// Les points ecrits sont lisses aussi : sans cela, chaque noeud de la courbe de HG
		// restait un coude visible.
		for (int32 K = 1; K + 1 < Pts.Num(); ++K)
		{
			Pts[K].P = 0.25 * Copy[K - 1].P + 0.5 * Copy[K].P + 0.25 * Copy[K + 1].P;
		}
		if (!bPinLast && Pts.Num() >= 2) Pts.Last().P = 0.5 * (Copy.Last().P + Copy[Pts.Num() - 2].P);
	}
}

/**
 * Garde la plus grande composante 4-connexe de Cells ; renvoie les autres sommets. Un lac est
 * un seul plan d'eau : les creux voisins qui passent sous son niveau sans le rejoindre ne sont
 * pas le lac, ce seraient des flaques isolees.
 */
TArray<int32> KeepLargestComponent(const FGrid& G, TArray<int32>& Cells)
{
	TArray<int32> Dropped;
	if (Cells.Num() < 2) return Dropped;
	TMap<int32, int32> Label;
	for (const int32 I : Cells) Label.Add(I, INDEX_NONE);
	int32 Best = INDEX_NONE, BestSize = 0, Next = 0;
	for (const int32 Start : Cells)
	{
		if (Label[Start] != INDEX_NONE) continue;
		TArray<int32> Queue = {Start};
		Label[Start] = Next;
		for (int32 Q = 0; Q < Queue.Num(); ++Q)
		{
			const int32 I = Queue[Q];
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
				if (!G.In(X, Y)) continue;
				int32* L = Label.Find(Y * G.W + X);
				if (L && *L == INDEX_NONE) { *L = Next; Queue.Add(Y * G.W + X); }
			}
		}
		if (Queue.Num() > BestSize) { BestSize = Queue.Num(); Best = Next; }
		++Next;
	}
	TArray<int32> Kept;
	for (const int32 I : Cells) (Label[I] == Best ? Kept : Dropped).Add(I);
	Cells = MoveTemp(Kept);
	return Dropped;
}

/** Segment de riviere indexe pour le tampon de creusement. */
struct FSeg
{
	int32 River = 0;
	int32 K = 0; // point de depart
};
}
namespace AnastasisDrainage
{
using namespace Detail;
}

bool AnastasisDrainage::IsEnabled()
{
	return CVarDrainage.GetValueOnGameThread() != 0;
}

int32 AnastasisDrainage::DebugMode()
{
	return CVarDrainageDebug.GetValueOnGameThread();
}

void AnastasisDrainage::SetActive(const FNetwork& Network)
{
	GActiveNetwork = Network;
}

bool AnastasisDrainage::RiparianAt(double X, double Y, double& Out)
{
	const FNetwork& Net = GActiveNetwork;
	Out = 0.0;
	if (Net.GridW < 2 || Net.GridH < 2 || Net.Riparian.Num() != Net.GridW * Net.GridH || Net.GridStep <= 0.0) return false;
	const double U = (X - Net.GridX0) / Net.GridStep, V = (Y - Net.GridY0) / Net.GridStep;
	if (!FMath::IsFinite(U) || !FMath::IsFinite(V) || U < 0.0 || V < 0.0 || U > Net.GridW - 1 || V > Net.GridH - 1) return false;
	const int32 IX = FMath::Min(static_cast<int32>(U), Net.GridW - 2), IY = FMath::Min(static_cast<int32>(V), Net.GridH - 2);
	const double FX = U - IX, FY = V - IY;
	const int32 A = IY * Net.GridW + IX;
	Out = FMath::Lerp(FMath::Lerp(Net.Riparian[A], Net.Riparian[A + 1], FX), FMath::Lerp(Net.Riparian[A + Net.GridW], Net.Riparian[A + Net.GridW + 1], FX), FY);
	return true;
}

const AnastasisDrainage::FNetwork& AnastasisDrainage::GetActive()
{
	return GActiveNetwork;
}

bool AnastasisDrainage::Apply(
	const FWorldVisualSnapshot& Crop,
	AnastasisTerrainForge::FMesh& Mesh,
	FNetwork& Out,
	const FParams& Params)
{
	Out = FNetwork{};
	const double StartTime = FPlatformTime::Seconds();
	AnastasisTerrainSurface::FGeometry& Geo = Mesh.Geometry;
	FGrid G;
	G.W = Mesh.FineW;
	G.H = Mesh.FineH;
	G.N = G.W * G.H;
	if (G.W < 8 || G.H < 8 || Mesh.Subdiv < 1 || Geo.Vertices.Num() != G.N || Geo.WaterVertices.Num() != G.N
		|| Crop.W != Mesh.CoarseW || Crop.H != Mesh.CoarseH)
	{
		return false;
	}
	const double Unit = AnastasisWorldView::TileWorldSize * Mesh.SpatialScale; // uu par tuile
	G.S = Unit / Mesh.Subdiv;
	G.X0 = Geo.Vertices[0].X;
	G.Y0 = Geo.Vertices[0].Y;
	const double SeaZ = AnastasisTerrainSurface::WaterPlaneZ;
	const double CellM2 = FMath::Square(G.S / 100.0);
	const int32 N = G.N;

	TArray<double> Z, Wz0;
	Z.SetNumUninitialized(N);
	Wz0.SetNumUninitialized(N);
	for (int32 I = 0; I < N; ++I) { Z[I] = Geo.Vertices[I].Z; Wz0[I] = Geo.WaterVertices[I].Z; }

	// --- Rivieres ecrites (Human_Geography_V2) : distance et hauteur d'eau par sommet.
	TArray<TArray<FVector>> Authored;
	if (Mesh.bHumanGeography)
	{
		for (const TArray<FVector>& Poly : AnastasisHumanGeography::AuthoredRivers())
		{
			TArray<FVector>& WorldPoly = Authored.AddDefaulted_GetRef();
			for (const FVector& P : Poly)
			{
				WorldPoly.Add(FVector(P.X * Unit, P.Y * Unit, SeaZ + (P.Z * 100.0 - SeaZ) * Mesh.SpatialScale));
			}
		}
	}
	TArray<double> AuthDist, AuthZ;
	AuthDist.Init(TNumericLimits<double>::Max(), N);
	AuthZ.Init(0.0, N);
	for (const TArray<FVector>& Poly : Authored)
	{
		FBox2D Box(ForceInit);
		for (const FVector& P : Poly) Box += FVector2D(P.X, P.Y);
		Box = Box.ExpandBy(3.0 * Unit);
		for (int32 I = 0; I < N; ++I)
		{
			const FVector2D P(G.WX(I), G.WY(I));
			if (!Box.IsInside(P)) continue;
			const FNearest Nr = NearestOn(Poly, P.X, P.Y);
			if (Nr.Dist < AuthDist[I]) { AuthDist[I] = Nr.Dist; AuthZ[I] = Nr.Z; }
		}
	}

	// --- 1. Eau existante : sommets immerges, distance au rivage, plans d'eau.
	TArray<uint8> Wet, AuthWet;
	Wet.SetNumZeroed(N);
	AuthWet.SetNumZeroed(N);
	for (int32 I = 0; I < N; ++I)
	{
		Wet[I] = Z[I] < Wz0[I] - 1.0 ? 1 : 0;
		AuthWet[I] = Wet[I] && AuthDist[I] < 1.6 * Unit && FMath::Abs(Wz0[I] - AuthZ[I]) < 30.0 ? 1 : 0;
	}
	TArray<int32> Inscribed;
	Inscribed.SetNumZeroed(N);
	{
		TArray<int32> Queue;
		for (int32 I = 0; I < N; ++I)
		{
			if (!Wet[I]) continue;
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
				if (!G.In(X, Y) || !Wet[Y * G.W + X]) { Inscribed[I] = 1; Queue.Add(I); break; }
			}
		}
		for (int32 Q = 0; Q < Queue.Num(); ++Q)
		{
			const int32 I = Queue[Q];
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
				if (!G.In(X, Y)) continue;
				const int32 J = Y * G.W + X;
				if (Wet[J] && !Inscribed[J]) { Inscribed[J] = Inscribed[I] + 1; Queue.Add(J); }
			}
		}
	}

	TArray<FLakeWork> Lakes;
	TArray<int32> LakeOf;
	LakeOf.Init(INDEX_NONE, N);
	{
		TArray<int32> Comp;
		Comp.Init(INDEX_NONE, N);
		int32 NextComp = 0;
		const int32 MinRadius = FMath::Max(2, FMath::RoundToInt(Params.LakeMinRadiusM * 100.0 / G.S));
		const int32 Open = FMath::Max(2, MinRadius / 2);
		// Une riviere ecrite a l'etiage d'un plan d'eau (son embouchure, son exutoire) fait
		// partie de ce plan d'eau : reparer puis recreuser ce bout de lit dans le lac le
		// dessinerait en rayure sous la nappe.
		// Compare au niveau de la graine, pas du voisin : pas a pas, un lit en pente douce
		// passerait tout entier sous le seuil.
		auto Joins = [&](int32 Seed, int32 To) { return !AuthWet[To] || FMath::Abs(Wz0[To] - Wz0[Seed]) < 10.0; };
		for (int32 Start = 0; Start < N; ++Start)
		{
			if (!Wet[Start] || AuthWet[Start] || Comp[Start] != INDEX_NONE) continue;
			TArray<int32> Cells = {Start};
			Comp[Start] = NextComp;
			for (int32 Q = 0; Q < Cells.Num(); ++Q)
			{
				const int32 I = Cells[Q];
				for (int32 K = 0; K < 4; ++K)
				{
					const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
					if (!G.In(X, Y)) continue;
					const int32 J = Y * G.W + X;
					if (Wet[J] && Joins(Start, J) && Comp[J] == INDEX_NONE) { Comp[J] = NextComp; Cells.Add(J); }
				}
			}
			++NextComp;
			bool bBorder = false, bAuthored = false;
			int32 MaxIn = 0;
			for (const int32 I : Cells)
			{
				bBorder |= G.Border(I);
				MaxIn = FMath::Max(MaxIn, Inscribed[I]);
				for (int32 K = 0; K < 4; ++K)
				{
					const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
					if (G.In(X, Y) && AuthWet[Y * G.W + X]) bAuthored = true;
					bAuthored |= AuthWet[I] != 0;
				}
			}
			FLakeWork Lake;
			Lake.bBorder = bBorder;
			Lake.bAuthored = bAuthored && !bBorder;
			if (bBorder)
			{
				Lake.Cells = MoveTemp(Cells);
			}
			else
			{
				if (MaxIn < MinRadius || Cells.Num() * CellM2 < Params.LakeMinAreaM2) continue;
				// Ouverture morphologique : le coeur du plan d'eau, sans les bras de tranchee.
				TSet<int32> InComp(Cells), Keep;
				TArray<int32> Front;
				for (const int32 I : Cells) if (Inscribed[I] >= Open) { Front.Add(I); Keep.Add(I); }
				for (int32 Step = 0; Step < Open; ++Step)
				{
					TArray<int32> Next;
					for (const int32 I : Front)
					{
						for (int32 K = 0; K < 4; ++K)
						{
							const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
							if (!G.In(X, Y)) continue;
							const int32 J = Y * G.W + X;
							if (InComp.Contains(J) && !Keep.Contains(J)) { Keep.Add(J); Next.Add(J); }
						}
					}
					Front = MoveTemp(Next);
				}
				for (const int32 I : Cells) if (Keep.Contains(I)) Lake.Cells.Add(I);
			}
			if (Lake.Cells.Num() == 0) continue;
			for (const int32 I : Lake.Cells) LakeOf[I] = Lakes.Num();
			Lakes.Add(MoveTemp(Lake));
		}
	}
	// Contours arrondis : un lac herite ses bords des tuiles de simulation (marches de 20 m,
	// angles droits). Le masque est floute (trois passes de boite, ~gaussienne de 15 m) puis
	// seuille a 0.5 : les coins s'arrondissent, les bras plus etroits que ~30 m tombent.
	// Les mers de bord aussi : le flou est normalise par les echantillons DANS la grille, donc
	// l'eau ne recule pas du bord de carte. Un sommet qui quitte un lac est memorise pour
	// etre rendu a la terre au-dessus du niveau.
	TArray<int32> LeftLake;
	LeftLake.Init(INDEX_NONE, N);
	// RIVERBANK_LIFE_001 (WaterLook) : distance signee a la rive (uu, negative dans le lac), tiree
	// du masque floute AVANT seuillage -- (0.5 - M) / |grad M|. Le seuil rend des cellules, donc
	// une rive en escalier a angles droits, vue a hauteur d'homme ; la distance, elle, est
	// continue : un talus qui la suit fait passer la ligne d'eau entre les sommets.
	TArray<float> LakeSigned;
	LakeSigned.Init(TNumericLimits<float>::Max(), N);
	{
		const int32 Radius = FMath::Max(1, FMath::RoundToInt(1500.0 / G.S));
		for (int32 L = 0; L < Lakes.Num(); ++L)
		{
			FLakeWork& Lake = Lakes[L];
			int32 X0 = G.W, Y0 = G.H, X1 = 0, Y1 = 0;
			for (const int32 I : Lake.Cells)
			{
				X0 = FMath::Min(X0, I % G.W); X1 = FMath::Max(X1, I % G.W);
				Y0 = FMath::Min(Y0, I / G.W); Y1 = FMath::Max(Y1, I / G.W);
			}
			X0 = FMath::Max(0, X0 - 3 * Radius); Y0 = FMath::Max(0, Y0 - 3 * Radius);
			X1 = FMath::Min(G.W - 1, X1 + 3 * Radius); Y1 = FMath::Min(G.H - 1, Y1 + 3 * Radius);
			const int32 BW = X1 - X0 + 1, BH = Y1 - Y0 + 1;
			TArray<float> M, T;
			M.Init(0.0f, BW * BH);
			T.Init(0.0f, BW * BH);
			for (const int32 I : Lake.Cells) M[(I / G.W - Y0) * BW + (I % G.W - X0)] = 1.0f;
			for (int32 Pass = 0; Pass < 3; ++Pass)
			{
				for (int32 Y = 0; Y < BH; ++Y)
				{
					for (int32 X = 0; X < BW; ++X)
					{
						float Sum = 0.0f;
						int32 Count = 0;
						for (int32 D = -Radius; D <= Radius; ++D) { const int32 XX = X + D; if (XX >= 0 && XX < BW) { Sum += M[Y * BW + XX]; ++Count; } }
						T[Y * BW + X] = Sum / Count;
					}
				}
				for (int32 Y = 0; Y < BH; ++Y)
				{
					for (int32 X = 0; X < BW; ++X)
					{
						float Sum = 0.0f;
						int32 Count = 0;
						for (int32 D = -Radius; D <= Radius; ++D) { const int32 YY = Y + D; if (YY >= 0 && YY < BH) { Sum += T[YY * BW + X]; ++Count; } }
						M[Y * BW + X] = Sum / Count;
					}
				}
			}
			TArray<int32> Cells;
			for (int32 Y = 0; Y < BH; ++Y)
			{
				for (int32 X = 0; X < BW; ++X)
				{
					const int32 I = (Y0 + Y) * G.W + (X0 + X);
					if (M[Y * BW + X] >= 0.5f && (LakeOf[I] == L || LakeOf[I] == INDEX_NONE)) Cells.Add(I);
				}
			}
			// Un lac que le flou efface presque entierement garde son masque : mieux anguleux qu'absent.
			KeepLargestComponent(G, Cells);
			if (Cells.Num() < Lake.Cells.Num() / 4) continue;
			for (const int32 I : Lake.Cells) { LakeOf[I] = INDEX_NONE; LeftLake[I] = L; }
			for (const int32 I : Cells) { LakeOf[I] = L; LeftLake[I] = INDEX_NONE; }
			Lake.Cells = MoveTemp(Cells);
			for (int32 Y = 1; Y + 1 < BH; ++Y)
			{
				for (int32 X = 1; X + 1 < BW; ++X)
				{
					const float Mv = M[Y * BW + X];
					if (Mv <= 0.02f || Mv >= 0.98f) continue;
					const int32 I = (Y0 + Y) * G.W + (X0 + X);
					if (LakeOf[I] != L && LakeOf[I] != INDEX_NONE) continue;
					const double GX = (M[Y * BW + X + 1] - M[Y * BW + X - 1]) / (2.0 * G.S);
					const double GY = (M[(Y + 1) * BW + X] - M[(Y - 1) * BW + X]) / (2.0 * G.S);
					const double Grad = FMath::Sqrt(GX * GX + GY * GY);
					if (Grad < 1.e-6) continue;
					const float D = static_cast<float>(FMath::Clamp((0.5 - Mv) / Grad, -2000.0, 2000.0));
					if (FMath::Abs(D) < FMath::Abs(LakeSigned[I])) LakeSigned[I] = D;
				}
			}
		}
	}
	auto KeepsShore = [&Lakes](int32 L) { return L >= 0 && (Lakes[L].bBorder || Lakes[L].bAuthored); };

	// --- 2. Reparation. (a) La forge ecrase le relief pres de l'eau de simulation
	// (LocalExag x lerp(1, 0.42, ShoreBlend)) : autour d'une eau qui n'est pas gardee,
	// c'est un halo de tranchee. On le defait, pondere par la vallee ecrite de HG.
	TArray<int32> NearLake; // lac du plan d'eau le plus proche, -1 = eau reparee, -2 = aucune
	NearLake.Init(-2, N);
	{
		TArray<int32> Queue;
		for (int32 I = 0; I < N; ++I) if (Wet[I]) { NearLake[I] = LakeOf[I]; Queue.Add(I); }
		for (int32 Q = 0; Q < Queue.Num(); ++Q)
		{
			const int32 I = Queue[Q];
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
				if (!G.In(X, Y)) continue;
				const int32 J = Y * G.W + X;
				if (NearLake[J] == -2) { NearLake[J] = NearLake[I]; Queue.Add(J); }
			}
		}
	}
	TArray<double> R = Z;
	for (int32 I = 0; I < N; ++I)
	{
		if (Wet[I] || Z[I] <= SeaZ || NearLake[I] == -2 || KeepsShore(NearLake[I])) continue;
		const double U = static_cast<double>(I % G.W) / Mesh.Subdiv, V = static_cast<double>(I / G.W) / Mesh.Subdiv;
		const int32 TX = FMath::Clamp(static_cast<int32>(U), 0, Crop.W - 2), TY = FMath::Clamp(static_cast<int32>(V), 0, Crop.H - 2);
		const double FX = FMath::Clamp(U - TX, 0.0, 1.0), FY = FMath::Clamp(V - TY, 0.0, 1.0);
		auto Sh = [&Crop](int32 X, int32 Y) { return Crop.Tiles[Y * Crop.W + X].Shore; };
		const double Shore = FMath::Lerp(FMath::Lerp(Sh(TX, TY), Sh(TX + 1, TY), FX), FMath::Lerp(Sh(TX, TY + 1), Sh(TX + 1, TY + 1), FX), FY);
		const double Squash = FMath::Lerp(1.0, 0.42, SmoothStep(0.15, 0.85, Shore));
		if (Squash >= 1.0) continue;
		const double Valley = Mesh.bHumanGeography
			? AnastasisHumanGeography::Evaluate(G.WX(I) / Unit, G.WY(I) / Unit, 0.0).ValleyWeight : 0.0;
		const double Unsquashed = SeaZ + (Z[I] - SeaZ) / Squash;
		R[I] = Z[I] + (1.0 - Valley) * (Unsquashed - Z[I]);
	}
	// (b) Remplissage harmonique de toute eau non gardee (et des lacs interieurs, qui
	// retrouvent un niveau de berge), elargi de 25 m : la pente qui descendait vers la
	// tranchee appartient a la tranchee.
	TArray<uint8> Repair;
	Repair.SetNumZeroed(N);
	for (int32 I = 0; I < N; ++I) if (Wet[I] && !KeepsShore(LakeOf[I])) Repair[I] = 1;
	{
		const int32 Dilate = FMath::Max(1, FMath::RoundToInt(2500.0 / G.S));
		TArray<int32> Front;
		for (int32 I = 0; I < N; ++I) if (Repair[I]) Front.Add(I);
		for (int32 Step = 0; Step < Dilate; ++Step)
		{
			TArray<int32> Next;
			for (const int32 I : Front)
			{
				for (int32 K = 0; K < 4; ++K)
				{
					const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
					if (!G.In(X, Y)) continue;
					const int32 J = Y * G.W + X;
					if (!Repair[J] && !Wet[J] && !KeepsShore(NearLake[J])) { Repair[J] = 1; Next.Add(J); }
				}
			}
			Front = MoveTemp(Next);
		}
	}
	Harmonic(G, R, Repair, 300);

	// --- Niveaux de lac. Interieur : sous la berge reparee (p10 de l'anneau), fond en
	// cuvette. Bord de monde et lac ecrit : leur eau d'origine.
	for (FLakeWork& Lake : Lakes)
	{
		if (Lake.bBorder || Lake.bAuthored)
		{
			TArray<double> Levels;
			for (const int32 I : Lake.Cells) Levels.Add(Wz0[I]);
			Levels.Sort();
			Lake.Level = Levels[Levels.Num() / 2];
			continue;
		}
		TSet<int32> InLake(Lake.Cells);
		TArray<double> Ring;
		for (const int32 I : Lake.Cells)
		{
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
				if (G.In(X, Y) && !InLake.Contains(Y * G.W + X)) Ring.Add(R[Y * G.W + X]);
			}
		}
		Ring.Sort();
		Lake.Level = Ring.Num() ? Ring[Ring.Num() / 10] - 50.0 : SeaZ;
	}
	// Ce que le contour arrondi rend a la terre sort de l'eau : au moins 40 cm au-dessus du lac.
	for (int32 I = 0; I < N; ++I)
	{
		if (LeftLake[I] != INDEX_NONE && LakeOf[I] == INDEX_NONE) R[I] = FMath::Max(R[I], Lakes[LeftLake[I]].Level + 40.0);
	}

	// Cuvettes closes du relief repare (forge, erosion, halos de tranchee) : un trou sans
	// exutoire rend une tache sombre et casse l'ecoulement. Mesurees sur la surface de
	// routage (rivieres ecrites comprises : la plaine centrale se draine par elles).
	// Grande et profonde (>= LakeMinAreaM2, >= 3 m, <= 15 ha) : lac de bassin ferme, a son
	// niveau de debordement moins 50 cm, contour naturel. Sinon : comblee jusqu'au col.
	// Plus grande encore : laissee aux rivieres, qui franchissent le seuil en s'encaissant.
	{
		TArray<double> Surf = R;
		for (int32 I = 0; I < N; ++I)
		{
			if (LakeOf[I] >= 0) Surf[I] = Lakes[LakeOf[I]].Level;
			if (AuthDist[I] < G.S * 1.2) Surf[I] = FMath::Min(Surf[I], AuthZ[I] - 300.0);
		}
		const TArray<double> Fill = PriorityFlood(G, Surf);
		TArray<uint8> Seen;
		Seen.SetNumZeroed(N);
		for (int32 Start = 0; Start < N; ++Start)
		{
			if (Seen[Start] || LakeOf[Start] >= 0 || Fill[Start] - Surf[Start] <= 1.0) continue;
			TArray<int32> Cells = {Start};
			Seen[Start] = 1;
			double Depth = 0.0, Spill = TNumericLimits<double>::Max();
			bool bAuthoredIn = false, bNearEdge = false;
			for (int32 Q = 0; Q < Cells.Num(); ++Q)
			{
				const int32 I = Cells[Q];
				Depth = FMath::Max(Depth, Fill[I] - Surf[I]);
				Spill = FMath::Min(Spill, Fill[I]);
				bAuthoredIn |= AuthDist[I] < G.S * 2.0;
				bNearEdge |= G.EdgeDist(I) < FMath::RoundToInt(10000.0 / G.S);
				for (int32 K = 0; K < 4; ++K)
				{
					const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
					if (!G.In(X, Y)) continue;
					const int32 J = Y * G.W + X;
					if (!Seen[J] && LakeOf[J] < 0 && Fill[J] - Surf[J] > 1.0) { Seen[J] = 1; Cells.Add(J); }
				}
			}
			if (Depth < 50.0) continue;
			const double Area = Cells.Num() * CellM2;
			if (Area > 150000.0) continue;
			// Pres du bord, une cuvette est la douve que laisse la transition de bord de la forge
			// entre le rempart et la limite de carte : on la comble, on n'y met pas de lac.
			if (!bAuthoredIn && !bNearEdge && Depth >= 300.0 && Area >= Params.LakeMinAreaM2)
			{
				FLakeWork Lake;
				Lake.Level = Spill - 50.0;
				for (const int32 I : Cells) if (Surf[I] < Lake.Level) Lake.Cells.Add(I);
				for (const int32 I : KeepLargestComponent(G, Lake.Cells)) R[I] = FMath::Max(R[I], Fill[I]);
				if (Lake.Cells.Num() == 0) continue;
				for (const int32 I : Lake.Cells) LakeOf[I] = Lakes.Num();
				Lakes.Add(MoveTemp(Lake));
				++Out.BasinLakes;
			}
			else
			{
				for (const int32 I : Cells) R[I] = FMath::Max(R[I], Fill[I]);
				++Out.FilledPits;
			}
		}
	}

	// --- 3. Routage. Surface : relief repare, lacs a niveau, rivieres ecrites brulees.
	TArray<double> Routing = R;
	for (int32 I = 0; I < N; ++I)
	{
		if (LakeOf[I] >= 0) Routing[I] = Lakes[LakeOf[I]].Level;
		if (AuthDist[I] < G.S * 1.2) Routing[I] = FMath::Min(Routing[I], AuthZ[I] - 300.0);
	}
	const TArray<double> F = PriorityFlood(G, Routing);
	TArray<int32> Down;
	Down.Init(INDEX_NONE, N);
	for (int32 I = 0; I < N; ++I)
	{
		double Best = 0.0;
		for (int32 K = 0; K < 8; ++K)
		{
			const int32 X = I % G.W + Off8X[K], Y = I / G.W + Off8Y[K];
			if (!G.In(X, Y)) continue;
			const int32 J = Y * G.W + X;
			const double Slope = (F[I] - F[J]) / (K < 4 ? 1.0 : UE_DOUBLE_SQRT_2);
			if (Slope > Best) { Best = Slope; Down[I] = J; }
		}
	}
	TArray<int32> Order;
	Order.SetNumUninitialized(N);
	for (int32 I = 0; I < N; ++I) Order[I] = I;
	Order.Sort([&F](int32 A, int32 B) { return F[A] > F[B] || (F[A] == F[B] && A < B); });
	TArray<double> Acc;
	Acc.Init(1.0, N);
	for (const int32 I : Order) if (Down[I] >= 0) Acc[Down[I]] += Acc[I];

	// Lacs en chaine : un lac ne peut pas etre plus haut que le lac qui s'y deverse.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 L = 0; L < Lakes.Num(); ++L)
		{
			int32 Spill = Lakes[L].Cells[0];
			for (const int32 I : Lakes[L].Cells) if (F[I] < F[Spill]) Spill = I;
			Lakes[L].Spill = Spill;
			int32 C = Spill, Guard = 0;
			while (C >= 0 && Guard++ < N && (LakeOf[C] == L || LakeOf[C] < 0)) C = Down[C];
			if (C >= 0 && LakeOf[C] >= 0 && !Lakes[LakeOf[C]].bBorder && !Lakes[LakeOf[C]].bAuthored)
			{
				Lakes[LakeOf[C]].Level = FMath::Min(Lakes[LakeOf[C]].Level, Lakes[L].Level - 50.0);
			}
		}
	}

	// Champs de routage pour le diagnostic (anastasis.Drainage.Dump) : <chemin>.fields.json.
	{
		const FString DumpPath = CVarDrainageDump.GetValueOnGameThread();
		if (!DumpPath.IsEmpty())
		{
			FString J = FString::Printf(TEXT("{\"w\":%d,\"h\":%d"), G.W, G.H);
			auto Arr = [&J, N](const TCHAR* Name, TFunctionRef<FString(int32)> V)
			{
				J += FString::Printf(TEXT(",\"%s\":["), Name);
				for (int32 I = 0; I < N; ++I) { if (I) J += TEXT(","); J += V(I); }
				J += TEXT("]");
			};
			Arr(TEXT("r"), [&](int32 I) { return FString::Printf(TEXT("%.0f"), R[I]); });
			Arr(TEXT("routing"), [&](int32 I) { return FString::Printf(TEXT("%.0f"), Routing[I]); });
			Arr(TEXT("f"), [&](int32 I) { return FString::Printf(TEXT("%.3f"), F[I]); });
			Arr(TEXT("acc"), [&](int32 I) { return FString::Printf(TEXT("%.0f"), Acc[I]); });
			Arr(TEXT("down"), [&](int32 I) { return FString::Printf(TEXT("%d"), Down[I]); });
			Arr(TEXT("lake"), [&](int32 I) { return FString::Printf(TEXT("%d"), LakeOf[I]); });
			Arr(TEXT("authdist"), [&](int32 I) { return FString::Printf(TEXT("%.0f"), FMath::Min(AuthDist[I], 99999.0)); });
			J += TEXT("}");
			FFileHelper::SaveStringToFile(J, *(DumpPath + TEXT(".fields.json")));
		}
	}

	// --- 4. Chenaux : critere aire x pente^2 (Montgomery & Dietrich). Un versant raide fait
	// naitre un ruisseau avec peu d'aire drainee ; un plat en demande beaucoup -- sinon la
	// plaine se couvre de lignes paralleles, droites, qui ne suivent aucune pente. Aire
	// plancher ChannelAreaM2 ; au-dela de ChannelBigAreaM2 un cours d'eau existe quelle que
	// soit la pente. Le seuil aire x pente^2 est releve jusqu'a MaxHeads tetes.
	TArray<double> LocalSlope;
	LocalSlope.SetNumUninitialized(N);
	for (int32 I = 0; I < N; ++I)
	{
		const int32 X = I % G.W, Y = I / G.W, D = 3;
		const double DX = R[Y * G.W + FMath::Min(X + D, G.W - 1)] - R[Y * G.W + FMath::Max(X - D, 0)];
		const double DY = R[FMath::Min(Y + D, G.H - 1) * G.W + X] - R[FMath::Max(Y - D, 0) * G.W + X];
		LocalSlope[I] = FMath::Sqrt(DX * DX + DY * DY) / (2.0 * D * G.S);
	}
	const double A0 = Params.ChannelAreaM2 / CellM2;
	const double ABig = Params.ChannelBigAreaM2 / CellM2;
	double AreaSlope = Params.ChannelAreaSlopeM2;
	// Les tetes naissent a l'interieur : le versant exterieur du rempart, entre sa crete et
	// le bord du monde, donnerait des ruisseaux paralleles au bord qui ne drainent rien.
	const int32 EdgeMargin = FMath::RoundToInt(10000.0 / G.S);
	TArray<uint8> Channel;
	Channel.SetNumZeroed(N);
	for (int32 Iter = 0; Iter < 60; ++Iter)
	{
		TArray<uint8> Up;
		Up.SetNumZeroed(N);
		for (int32 I = 0; I < N; ++I)
		{
			const bool bInitiates = Acc[I] >= ABig || Acc[I] * CellM2 * FMath::Square(LocalSlope[I]) >= AreaSlope;
			Channel[I] = Acc[I] >= A0 && bInitiates && LakeOf[I] < 0 && G.EdgeDist(I) >= EdgeMargin ? 1 : 0;
		}
		for (int32 I = 0; I < N; ++I)
		{
			if (Down[I] >= 0 && (Channel[I] || (LakeOf[I] >= 0 && Acc[I] >= A0))) Up[Down[I]] = 1;
		}
		int32 Heads = 0;
		for (int32 I = 0; I < N; ++I) if (Channel[I] && !Up[I]) ++Heads;
		if (Heads <= Params.MaxHeads) break;
		AreaSlope *= 1.15;
	}
	// Reference de la geometrie hydraulique (largeur, profondeur), fixe : elle ne suit pas le
	// seuil d'initiation, sinon relever celui-ci elargirait toutes les rivieres.
	Out.ChannelAreaM2 = Params.WidthRefAreaM2;
	Out.ChannelAreaSlopeM2 = AreaSlope;
	// Une riviere ecrite reste une riviere sur tout son trace, meme la ou l'aire drainee
	// n'atteint pas le seuil : on ne supprime pas un cours d'eau existant.
	for (int32 I = 0; I < N; ++I)
	{
		if (AuthDist[I] < G.S * 0.75 && LakeOf[I] < 0) Channel[I] = 1;
	}
	// Un chenal ne s'arrete pas a la marge : il continue jusqu'a son lac ou son bord.
	for (const int32 I : Order)
	{
		if (Channel[I] && Down[I] >= 0 && LakeOf[Down[I]] < 0) Channel[Down[I]] = 1;
	}
	// Un lac interieur deborde : son exutoire suit l'ecoulement depuis le seuil jusqu'au
	// reseau, meme sous le seuil d'aire. Sinon ce serait une mare posee sans role.
	for (int32 L = 0; L < Lakes.Num(); ++L)
	{
		FLakeWork& Lake = Lakes[L];
		if (Lake.bBorder || Lake.bAuthored || Lake.Spill == INDEX_NONE) continue;
		int32 C = Down[Lake.Spill], Guard = 0;
		const int32 First = C;
		while (C >= 0 && Guard++ < N && !Channel[C] && LakeOf[C] < 0) { Channel[C] = 1; C = Down[C]; }
		// Chapelet de lacs : le seuil donne directement dans le lac suivant. C'est un exutoire.
		if (C == First && C >= 0 && LakeOf[C] >= 0) Lake.SpillsInto = LakeOf[C];
	}

	// --- 5. Decomposition en rivieres : tronc = branche de plus grande aire a chaque confluence.
	TArray<TArray<int32>> Ups;
	Ups.SetNum(N);
	for (int32 I = 0; I < N; ++I)
	{
		if (Channel[I] && Down[I] >= 0 && Channel[Down[I]]) Ups[Down[I]].Add(I);
	}
	for (TArray<int32>& U : Ups)
	{
		U.Sort([&Acc](int32 A, int32 B) { return Acc[A] > Acc[B] || (Acc[A] == Acc[B] && A < B); });
	}
	TArray<int32> Terminals;
	for (int32 I = 0; I < N; ++I)
	{
		if (Channel[I] && (Down[I] < 0 || !Channel[Down[I]])) Terminals.Add(I);
	}
	Terminals.Sort([&Acc](int32 A, int32 B) { return Acc[A] > Acc[B] || (Acc[A] == Acc[B] && A < B); });
	TArray<FRiverWork> Work;
	TArray<int32> RiverOf;
	RiverOf.Init(INDEX_NONE, N);
	{
		TArray<TPair<int32, int32>> Pending; // (cellule aval, riviere parente)
		for (const int32 T : Terminals) Pending.Emplace(T, INDEX_NONE);
		for (int32 Q = 0; Q < Pending.Num(); ++Q)
		{
			FRiverWork River;
			River.Parent = Pending[Q].Value;
			const int32 Id = Work.Num();
			for (int32 C = Pending[Q].Key; C >= 0;)
			{
				River.Cells.Add(C);
				RiverOf[C] = Id;
				const TArray<int32>& U = Ups[C];
				for (int32 K = 1; K < U.Num(); ++K) Pending.Emplace(U[K], Id);
				C = U.Num() ? U[0] : INDEX_NONE;
			}
			Algo::Reverse(River.Cells);
			const int32 D = Down[River.Cells.Last()];
			if (River.Parent != INDEX_NONE) { River.Mouth = EMouth::River; River.Join = D; }
			else if (D < 0) River.Mouth = EMouth::Border;
			else if (LakeOf[D] >= 0) { River.Mouth = EMouth::Lake; River.MouthLake = LakeOf[D]; }
			else River.Mouth = EMouth::Border; // impossible : un chenal a toujours un aval chenal, lac ou bord
			Work.Add(MoveTemp(River));
		}
	}
	// Source lacustre : un voisin amont de la premiere cellule appartient a un lac.
	for (FRiverWork& River : Work)
	{
		const int32 First = River.Cells[0];
		for (int32 K = 0; K < 8; ++K)
		{
			const int32 X = First % G.W + Off8X[K], Y = First / G.W + Off8Y[K];
			if (!G.In(X, Y)) continue;
			const int32 J = Y * G.W + X;
			if (Down[J] == First && LakeOf[J] >= 0) { River.SourceLake = LakeOf[J]; break; }
		}
	}
	// Elagage : affluents courts sans affluent propre ; rivieres collees au bord du monde.
	{
		const double CellM = G.S / 100.0;
		const int32 Margin = EdgeMargin;
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			TArray<int32> Children;
			Children.Init(0, Work.Num());
			for (const FRiverWork& River : Work) if (!River.bDead && River.Parent != INDEX_NONE) ++Children[River.Parent];
			for (int32 Id = 0; Id < Work.Num(); ++Id)
			{
				FRiverWork& River = Work[Id];
				if (River.bDead || Children[Id] > 0) continue;
				// Une mer ne donne pas naissance a une riviere.
				if (River.SourceLake != INDEX_NONE && Lakes[River.SourceLake].bBorder) { River.bDead = true; continue; }
				// Un lac n'a qu'un exutoire : le plus grand debit ; les autres sont des cellules de plat.
				if (River.SourceLake != INDEX_NONE)
				{
					bool bLesser = false;
					for (int32 Other = 0; Other < Work.Num(); ++Other)
					{
						if (Other == Id || Work[Other].bDead || Work[Other].SourceLake != River.SourceLake) continue;
						const double A = Acc[River.Cells.Last()], B = Acc[Work[Other].Cells.Last()];
						if (B > A || (B == A && Other < Id)) bLesser = true;
					}
					if (bLesser) { River.bDead = true; continue; }
				}
				int32 NearEdge = 0;
				for (const int32 C : River.Cells) if (G.EdgeDist(C) < Margin) ++NearEdge;
				// L'exutoire d'un lac reste, meme le long du bord : sans lui le lac n'a plus de role.
				const bool bEdgeHugger = River.SourceLake == INDEX_NONE && NearEdge * 2 > River.Cells.Num();
				const bool bShort = River.Parent != INDEX_NONE && River.SourceLake == INDEX_NONE && River.Cells.Num() * CellM < Params.MinTributaryM;
				// Moignon : quelques cellules entre un lit ecrit et un lac, ou sur la rive d'une mer.
				const bool bStub = River.SourceLake == INDEX_NONE && River.Cells.Num() * CellM < 60.0;
				if (bEdgeHugger || bShort || bStub) River.bDead = true;
			}
		}
		for (int32 I = 0; I < N; ++I) if (RiverOf[I] >= 0 && Work[RiverOf[I]].bDead) { Channel[I] = 0; RiverOf[I] = INDEX_NONE; }
	}

	// --- Strahler, pente de vallee, surface d'eau par cellule.
	TArray<int32> Strahler;
	Strahler.Init(0, N);
	for (const int32 I : Order)
	{
		if (!Channel[I]) continue;
		int32 Max = 0, Count = 0;
		for (int32 K = 0; K < 8; ++K)
		{
			const int32 X = I % G.W + Off8X[K], Y = I / G.W + Off8Y[K];
			if (!G.In(X, Y)) continue;
			const int32 J = Y * G.W + X;
			if (Down[J] != I || !Channel[J]) continue;
			if (Strahler[J] > Max) { Max = Strahler[J]; Count = 1; }
			else if (Strahler[J] == Max) ++Count;
		}
		Strahler[I] = Max == 0 ? 1 : (Count >= 2 ? Max + 1 : Max);
	}
	// Ordre porte a travers un lac : son exutoire herite de l'ordre maximal qui y entre.
	TArray<int32> LakeOrder;
	LakeOrder.Init(0, Lakes.Num());
	for (const FRiverWork& River : Work)
	{
		if (!River.bDead && River.MouthLake >= 0) LakeOrder[River.MouthLake] = FMath::Max(LakeOrder[River.MouthLake], Strahler[River.Cells.Last()]);
	}

	// Fond de vallee = le relief REPARE lui-meme, pas une surface remplie. Dans une cuvette
	// close (la plaine centrale sans les rivieres ecrites de HG), une surface remplie est
	// plate jusqu'au col : l'eau y serait perchee au-dessus de la plaine et deborderait.
	// Avec le relief reel et le plafond amont ci-dessous (la surface ne remonte jamais vers
	// l'aval), la riviere suit le fond et franchit le seuil en s'y encaissant : une gorge
	// d'exutoire, pas un lac suspendu.
	const TArray<double>& Valley = R;
	// Pente de versant lue sur le relief repare (la brulure y ferait des falaises de 3 m).
	auto HillAt = [&](int32 I) -> double
	{
		const int32 X = I % G.W, Y = I / G.W, D = 3;
		const double DX = R[Y * G.W + FMath::Min(X + D, G.W - 1)] - R[Y * G.W + FMath::Max(X - D, 0)];
		const double DY = R[FMath::Min(Y + D, G.H - 1) * G.W + X] - R[FMath::Max(Y - D, 0) * G.W + X];
		const double Grad = FMath::Sqrt(DX * DX + DY * DY) / (2.0 * D * G.S);
		return SmoothStep(0.03, 0.14, Grad);
	};
	TArray<double> Ws;
	Ws.Init(0.0, N);
	for (const int32 I : Order)
	{
		if (!Channel[I]) continue;
		double Raw;
		if (AuthDist[I] < G.S * 2.0) Raw = AuthZ[I];
		else
		{
			const double Hill = HillAt(I);
			// Incision : berge basse en plaine, chenal encaisse en colline.
			// Berge marquee (WaterLook) : en plaine la riviere coule 1 m sous la prairie, pas a
			// fleur d'herbe -- sinon elle se lit comme un trait de peinture pose sur le sol.
			const double Freeboard = FMath::Lerp(Params.bWaterLook ? 100.0 : 60.0, 240.0, Hill) + 15.0 * (Strahler[I] - 1);
			Raw = Valley[I] - Freeboard;
		}
		double Cap = TNumericLimits<double>::Max();
		for (int32 K = 0; K < 8; ++K)
		{
			const int32 X = I % G.W + Off8X[K], Y = I / G.W + Off8Y[K];
			if (!G.In(X, Y)) continue;
			const int32 J = Y * G.W + X;
			if (Down[J] != I) continue;
			if (Channel[J]) Cap = FMath::Min(Cap, Ws[J]);
			else if (LakeOf[J] >= 0) Cap = FMath::Min(Cap, Lakes[LakeOf[J]].Level);
		}
		Ws[I] = FMath::Min(Raw, Cap);
	}
	for (int32 K = N - 1; K >= 0; --K)
	{
		const int32 I = Order[K];
		if (!Channel[I]) continue;
		const int32 D = Down[I];
		if (D < 0) continue;
		if (Channel[D]) Ws[I] = FMath::Max(Ws[I], Ws[D]);
		else if (LakeOf[D] >= 0) Ws[I] = FMath::Max(Ws[I], Lakes[LakeOf[D]].Level);
	}

	// --- 6. Polylignes : lissage, raccord tangent aux confluences, meandres de plaine.
	TArray<int32> WorkToOut;
	WorkToOut.Init(INDEX_NONE, Work.Num());
	for (int32 Id = 0; Id < Work.Num(); ++Id) if (!Work[Id].bDead) WorkToOut[Id] = Out.Rivers.AddDefaulted();
	FRandomStream Rng(static_cast<int32>(Crop.Seed * 7919u + 17u));
	const double MinW = Params.MinWidthM * 100.0, MaxW = Params.MaxWidthM * 100.0;
	const double MinD = Params.MinDepthM * 100.0, MaxD = Params.MaxDepthM * 100.0;
	for (int32 Id = 0; Id < Work.Num(); ++Id)
	{
		const FRiverWork& Wr = Work[Id];
		if (Wr.bDead) continue;
		FRiver& River = Out.Rivers[WorkToOut[Id]];
		River.Mouth = Wr.Mouth;
		River.Parent = Wr.Parent != INDEX_NONE ? WorkToOut[Wr.Parent] : INDEX_NONE;
		River.MouthLake = Wr.MouthLake;
		River.SourceLake = Wr.SourceLake;
		const int32 InflowOrder = Wr.SourceLake >= 0 ? LakeOrder[Wr.SourceLake] : 0;

		auto MakePt = [&](int32 C, double WsValue) -> FPt
		{
			FPt Pt;
			Pt.P = FVector2D(G.WX(C), G.WY(C));
			Pt.Ws = WsValue;
			Pt.Area = Acc[C] * CellM2;
			Pt.Hill = HillAt(C);
			Pt.Order = FMath::Max(Strahler[C], InflowOrder);
			Pt.bAuthored = AuthDist[C] < G.S * 2.0;
			return Pt;
		};
		TArray<FPt> Pts;
		if (Wr.SourceLake >= 0)
		{
			const int32 First = Wr.Cells[0];
			for (int32 K = 0; K < 8; ++K)
			{
				const int32 X = First % G.W + Off8X[K], Y = First / G.W + Off8Y[K];
				if (!G.In(X, Y)) continue;
				const int32 J = Y * G.W + X;
				if (Down[J] == First && LakeOf[J] == Wr.SourceLake) { Pts.Add(MakePt(J, Lakes[Wr.SourceLake].Level)); Pts.Last().Area = Acc[First] * CellM2; break; }
			}
		}
		for (const int32 C : Wr.Cells) Pts.Add(MakePt(C, Ws[C]));
		int32 Extension = 0;
		const int32 Last = Wr.Cells.Last();
		const int32 D = Down[Last];
		if (Wr.Mouth == EMouth::River && D >= 0)
		{
			// Le parent continue la polyligne le temps du lissage : l'affluent arrive tangent.
			int32 C = D;
			for (int32 K = 0; K < 4 && C >= 0 && Channel[C]; ++K, C = Down[C])
			{
				FPt Pt = MakePt(C, Ws[C]);
				Pt.Area = Acc[Last] * CellM2;
				Pt.Order = Pts.Last().Order;
				Pts.Add(Pt);
				++Extension;
			}
		}
		else if (Wr.Mouth == EMouth::Lake && D >= 0)
		{
			Pts.Add(MakePt(D, Lakes[Wr.MouthLake].Level));
			Pts.Last().Area = Acc[Last] * CellM2;
			Pts.Last().Order = Pts[Pts.Num() - 2].Order;
		}
		else if (Wr.Mouth == EMouth::Border && Pts.Num() >= 2)
		{
			// L'exutoire sort franchement du monde.
			FPt Pt = Pts.Last();
			Pt.P += (Pts.Last().P - Pts[Pts.Num() - 2].P).GetSafeNormal() * G.S * 2.0;
			Pts.Add(Pt);
		}
		// Les points ecrits sont colles a la ligne de HG.
		for (FPt& Pt : Pts)
		{
			if (!Pt.bAuthored) continue;
			// Mesure depuis la position d'origine : deplacer le point avant d'avoir compare
			// toutes les lignes le collerait a la premiere, fut-elle a deux kilometres.
			const FVector2D From = Pt.P;
			FNearest Best;
			for (const TArray<FVector>& Poly : Authored)
			{
				const FNearest Nr = NearestOn(Poly, From.X, From.Y);
				if (Nr.Dist < Best.Dist) Best = Nr;
			}
			if (Best.Dist < G.S * 2.0) { Pt.P = Best.P; Pt.Ws = Best.Z; }
		}
		SmoothXY(Pts, 6, Wr.Mouth != EMouth::River);
		if (Extension > 0)
		{
			// On garde le premier point du parent : la confluence tombe dans son lit.
			Pts.SetNum(Pts.Num() - Extension + 1);
			Pts.Last().Ws = Ws[D];
		}
		Pts = Resample(Pts, G.S);

		// Largeur / profondeur hydrauliques avant le meandre : l'amplitude en depend.
		auto WidthOf = [&](const FPt& Pt) { return FMath::Clamp(MinW * FMath::Sqrt(Pt.Area / Out.ChannelAreaM2), MinW, MaxW); };
		const double PhaseA = Rng.FRandRange(0.0, UE_DOUBLE_TWO_PI), PhaseB = Rng.FRandRange(0.0, UE_DOUBLE_TWO_PI);
		const double Skew = Rng.FRandRange(0.35, 0.6);
		{
			TArray<double> Arc;
			Arc.Init(0.0, Pts.Num());
			for (int32 K = 1; K < Pts.Num(); ++K) Arc[K] = Arc[K - 1] + FVector2D::Distance(Pts[K - 1].P, Pts[K].P);
			const double Total = Arc.Last();
			TArray<FPt> Moved = Pts;
			const double NoiseSeed = static_cast<double>(Crop.Seed) + 4271.0 + 97.0 * Id;
			for (int32 K = 1; K + 1 < Pts.Num(); ++K)
			{
				const double Width = WidthOf(Pts[K]);
				const double Flat = 1.0 - Pts[K].Hill;
				// Plus court en versant : un torrent zigzague serre, une riviere de plaine ample.
				const double Lambda = FMath::Max(FMath::Lerp(7.0, 12.0, Flat) * Width, 6000.0);
				const double Taper = SmoothStep(0.0, Lambda * 0.6, FMath::Min(Arc[K], Total - Arc[K]));
				// Plancher de sinuosite : meme sur versant raide, aucun cours d'eau n'est trace a la
				// regle. Une riviere ecrite (Human_Geography_V2) meandre autour de sa courbe, plus
				// sagement : son trace reste lisible, son profil d'eau est garde.
				const double Amp = (0.35 + 0.65 * Flat * Flat) * Width * Taper * (Pts[K].bAuthored ? 0.7 : 1.0);
				if (Amp < 1.0) continue;
				const double Phase = UE_DOUBLE_TWO_PI * Arc[K] / Lambda;
				// Deux harmoniques de phases independantes : des boucles asymetriques, pas une sinusoide ;
				// un bruit fBm le long du cours casse ce qui resterait de regulier.
				const double Noise = 2.0 * (AnastasisWorldNoise::Fbm(Arc[K] / (0.7 * Lambda), 0.5, NoiseSeed) - 0.5);
				const double Wave = FMath::Sin(Phase + PhaseA) + Skew * FMath::Sin(2.13 * Phase + PhaseB) + 0.8 * Noise;
				const FVector2D Tangent = (Pts[K + 1].P - Pts[K - 1].P).GetSafeNormal();
				const FVector2D Normal(-Tangent.Y, Tangent.X);
				double Offset = Amp * Wave / (1.0 + Skew + 0.4);
				// Le meandre reste dans la vallee : pas de boucle qui grimpe sur le versant.
				for (int32 Try = 0; Try < 3; ++Try)
				{
					const FVector2D Q = Pts[K].P + Normal * Offset;
					if (G.Sample(R, Q.X, Q.Y) <= Pts[K].Ws + 200.0) break;
					Offset *= 0.5;
				}
				Moved[K].P = Pts[K].P + Normal * Offset;
			}
			Pts = MoveTemp(Moved);
			SmoothXY(Pts, 2, true);
		}

		// Attributs definitifs, contraints vers l'aval : l'eau ne remonte pas, le lit ne retrecit pas.
		River.Points.SetNum(Pts.Num());
		double WsMin = TNumericLimits<double>::Max(), WMax = 0.0, DMax = 0.0;
		for (int32 K = 0; K < Pts.Num(); ++K)
		{
			FRiverPoint& P = River.Points[K];
			WsMin = FMath::Min(WsMin, Pts[K].Ws);
			WMax = FMath::Max(WMax, WidthOf(Pts[K]));
			DMax = FMath::Max(DMax, FMath::Clamp(MinD * FMath::Pow(Pts[K].Area / Out.ChannelAreaM2, 0.35), MinD, MaxD));
			P.Location = FVector(Pts[K].P.X, Pts[K].P.Y, WsMin);
			P.Width = WMax;
			P.Depth = DMax;
			P.AreaM2 = Pts[K].Area;
			P.Order = Pts[K].Order;
			P.Hill = Pts[K].Hill;
			// Berge large et douce en plaine, courte en colline.
			const double PlainBank = Params.bWaterLook ? FMath::Max(1.5 * WMax, 1000.0) : FMath::Max(3.0 * WMax, 2500.0);
			P.BankFalloff = FMath::Clamp(FMath::Lerp(PlainBank, FMath::Max(1.0 * WMax, 800.0), Pts[K].Hill), 800.0, 6000.0);
			River.bAuthored |= Pts[K].bAuthored;
		}
		// Source de versant : le chenal nait, il n'est pas coupe a l'emporte-piece. Largeur et
		// profondeur montent sur les 80 premiers metres ; une source lacustre part pleine.
		if (Wr.SourceLake == INDEX_NONE)
		{
			double Arc = 0.0;
			for (int32 K = 0; K < River.Points.Num(); ++K)
			{
				if (K > 0) Arc += FVector::Dist2D(River.Points[K - 1].Location, River.Points[K].Location);
				const double T = SmoothStep(0.0, 8000.0, Arc);
				if (T >= 1.0) break;
				// Pas sous une maille (5 m) : un filet plus etroit que la grille s'y egrene en flaques.
				River.Points[K].Width *= FMath::Lerp(0.75, 1.0, T);
				River.Points[K].Depth *= FMath::Lerp(0.3, 1.0, T);
			}
		}
		River.Order = River.Points.Last().Order;
		double Length = 0.0;
		for (int32 K = 1; K < River.Points.Num(); ++K) Length += FVector::Dist2D(River.Points[K - 1].Location, River.Points[K].Location);
		River.LengthM = Length / 100.0;
	}
	// Profil en long, recepteurs d'abord (les rivieres sont rangees parent avant affluent).
	// 1. Un affluent n'est jamais sous l'eau de son recepteur a la confluence.
	// 2. Pas de marche : en remontant depuis l'embouchure, la surface ne peut pas etre plus
	//    raide que ~1.5 % en plaine, ~10 % sur versant raide. Une chute (affluent qui arrive
	//    au-dessus de son recepteur, riviere calculee qui rejoint un lit ecrit plus bas)
	//    devient un bief encaisse et regulier ; le lit est creuse en consequence. Minimum
	//    avec une borne decroissante vers l'aval : la surface reste decroissante.
	for (int32 RI = 0; RI < Out.Rivers.Num(); ++RI)
	{
		FRiver& River = Out.Rivers[RI];
		if (River.Mouth == EMouth::River && River.Parent != INDEX_NONE)
		{
			if (!ensure(River.Parent < RI)) continue;
			const FRiver& Parent = Out.Rivers[River.Parent];
			const FVector2D End(River.Points.Last().Location);
			double Best = TNumericLimits<double>::Max(), ParentWs = River.Points.Last().Location.Z;
			for (const FRiverPoint& P : Parent.Points)
			{
				const double Dist = FVector2D::Distance(End, FVector2D(P.Location));
				if (Dist < Best) { Best = Dist; ParentWs = P.Location.Z; }
			}
			for (FRiverPoint& P : River.Points) P.Location.Z = FMath::Max(P.Location.Z, ParentWs);
			River.Points.Last().Location.Z = ParentWs;
			++Out.Confluences;
		}
		for (int32 K = River.Points.Num() - 2; K >= 0; --K)
		{
			const FRiverPoint& Below = River.Points[K + 1];
			const double MaxSlope = FMath::Lerp(0.015, 0.10, River.Points[K].Hill);
			const double Dist = FVector::Dist2D(River.Points[K].Location, Below.Location);
			River.Points[K].Location.Z = FMath::Min(River.Points[K].Location.Z, Below.Location.Z + MaxSlope * Dist);
		}
	}
	// Vitesse : Manning, n = 0.035, rayon hydraulique ~ profondeur. La pente rendue est
	// exageree par TERRAIN_FORGE (anastasis.Terrain.Forge.Exaggerate, x3.6) : la vitesse lit
	// la pente physique, sinon toute riviere de plaine sortirait en torrent.
	double Exaggeration = 1.0;
	if (Params.ForgeExaggeration > 0.0)
	{
		Exaggeration = FMath::Max(1.0, Params.ForgeExaggeration); // WATER_NETWORK_001 : recette canonique
	}
	else if (const IConsoleVariable* Exag = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Terrain.Forge.Exaggerate")))
	{
		Exaggeration = FMath::Max(1.0, static_cast<double>(Exag->GetFloat()));
	}
	for (FRiver& River : Out.Rivers)
	{
		const int32 Num = River.Points.Num();
		for (int32 K = 0; K < Num; ++K)
		{
			const int32 A = FMath::Max(0, K - 4), B = FMath::Min(Num - 1, K + 4);
			double Dist = 0.0;
			for (int32 J = A + 1; J <= B; ++J) Dist += FVector::Dist2D(River.Points[J - 1].Location, River.Points[J].Location);
			const double Slope = Dist > 1.0 ? FMath::Max((River.Points[A].Location.Z - River.Points[B].Location.Z) / Dist, 1.e-4) : 1.e-4;
			FRiverPoint& P = River.Points[K];
			P.Slope = Slope;
			P.Velocity = FMath::Clamp(FMath::Pow(P.Depth / 100.0, 2.0 / 3.0) * FMath::Sqrt(Slope / Exaggeration) / 0.035, 0.1, 3.5);
		}
	}
	for (const FRiver& River : Out.Rivers)
	{
		if (River.SourceLake == INDEX_NONE) ++Out.Heads;
	}

	// --- 7. Plaines d'inondation : quelques biefs lents, larges, bas.
	struct FZone { int32 River; int32 K0, K1; };
	struct FPond { FVector2D C; FVector2D Axis; double A, B, Ws; };
	TArray<FZone> Zones;
	TArray<FPond> Ponds;
	{
		struct FCand { double Score; int32 River; int32 K; };
		TArray<FCand> Cands;
		for (int32 RI = 0; RI < Out.Rivers.Num(); ++RI)
		{
			const FRiver& River = Out.Rivers[RI];
			for (int32 K = 4; K + 4 < River.Points.Num(); K += 3)
			{
				const FRiverPoint& P = River.Points[K];
				if (P.Slope > 0.004 || P.AreaM2 < 6.0 * Out.ChannelAreaM2) continue;
				const FVector2D T = (FVector2D(River.Points[K + 1].Location) - FVector2D(River.Points[K - 1].Location)).GetSafeNormal();
				const FVector2D Nm(-T.Y, T.X);
				int32 Open = 0;
				for (const double Off : {3000.0, 6000.0})
				{
					for (const double Side : {-1.0, 1.0})
					{
						const FVector2D Q = FVector2D(P.Location) + Nm * Side * (0.5 * P.Width + Off);
						if (G.Sample(R, Q.X, Q.Y) < P.Location.Z + 250.0) ++Open;
					}
				}
				if (Open < 3) continue;
				bool bProtected = false;
				for (const FVector2D& Pr : Params.Protected)
				{
					if (FVector2D::Distance(Pr, FVector2D(P.Location)) < Params.ProtectedRadiusM * 100.0) bProtected = true;
				}
				if (bProtected) continue;
				Cands.Add({P.AreaM2 * Open, RI, K});
			}
		}
		Cands.Sort([](const FCand& A, const FCand& B) { return A.Score > B.Score || (A.Score == B.Score && (A.River < B.River || (A.River == B.River && A.K < B.K))); });
		for (const FCand& C : Cands)
		{
			if (Zones.Num() >= Params.MaxWetlands) break;
			const FVector2D At(Out.Rivers[C.River].Points[C.K].Location);
			bool bFar = true;
			for (const FWetland& Wl : Out.Wetlands) if (FVector2D::Distance(At, FVector2D(Wl.Center)) < 40000.0) bFar = false;
			if (!bFar) continue;
			const FRiver& River = Out.Rivers[C.River];
			const int32 Half = FMath::RoundToInt(10000.0 / G.S);
			FZone Zone{C.River, FMath::Max(1, C.K - Half), FMath::Min(River.Points.Num() - 2, C.K + Half)};
			Zones.Add(Zone);
			FWetland& Wl = Out.Wetlands.AddDefaulted_GetRef();
			Wl.Center = River.Points[C.K].Location;
			Wl.River = C.River;
			Wl.LengthM = (Zone.K1 - Zone.K0) * G.S / 100.0;
			// Deux mares laterales, en quinconce : bras mort et cuvette de decantation.
			for (int32 PondIndex = 0; PondIndex < 2; ++PondIndex)
			{
				const int32 K = FMath::Clamp(Zone.K0 + (PondIndex + 1) * (Zone.K1 - Zone.K0) / 3, 1, River.Points.Num() - 2);
				const FRiverPoint& P = River.Points[K];
				const FVector2D T = (FVector2D(River.Points[K + 1].Location) - FVector2D(River.Points[K - 1].Location)).GetSafeNormal();
				const FVector2D Nm(-T.Y, T.X);
				const double Side = PondIndex == 0 ? 1.0 : -1.0;
				const double Fp = FMath::Clamp(4.0 * P.Width, 3000.0, 7000.0);
				FPond Pond;
				Pond.C = FVector2D(P.Location) + Nm * Side * (0.5 * P.Width + 0.5 * Fp);
				Pond.Axis = T;
				Pond.A = Rng.FRandRange(3000.0, 4500.0);
				Pond.B = Rng.FRandRange(1200.0, 1800.0);
				Pond.Ws = P.Location.Z;
				Ponds.Add(Pond);
				++Wl.Ponds;
			}
		}
	}

	// --- 8. Tampon : lit, berges, plaines, mares, surfaces d'eau.
	TArray<FSeg> Segs;
	const double Bucket = 64.0 * G.S;
	const int32 BW = FMath::CeilToInt((G.W * G.S) / Bucket) + 1, BH = FMath::CeilToInt((G.H * G.S) / Bucket) + 1;
	TArray<TArray<int32>> Buckets;
	Buckets.SetNum(BW * BH);
	for (int32 RI = 0; RI < Out.Rivers.Num(); ++RI)
	{
		const FRiver& River = Out.Rivers[RI];
		for (int32 K = 0; K + 1 < River.Points.Num(); ++K)
		{
			const FRiverPoint& A = River.Points[K];
			const FRiverPoint& B = River.Points[K + 1];
			double Reach = FMath::Max(0.5 * A.Width + A.BankFalloff, 0.5 * B.Width + B.BankFalloff);
			for (const FZone& Zn : Zones) if (Zn.River == RI && K >= Zn.K0 && K <= Zn.K1) Reach = FMath::Max(Reach, 0.5 * A.Width + 7000.0);
			const int32 Seg = Segs.Add({RI, K});
			const int32 BX0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(A.Location.X, B.Location.X) - Reach - G.X0) / Bucket));
			const int32 BX1 = FMath::Min(BW - 1, FMath::FloorToInt((FMath::Max(A.Location.X, B.Location.X) + Reach - G.X0) / Bucket));
			const int32 BY0 = FMath::Max(0, FMath::FloorToInt((FMath::Min(A.Location.Y, B.Location.Y) - Reach - G.Y0) / Bucket));
			const int32 BY1 = FMath::Min(BH - 1, FMath::FloorToInt((FMath::Max(A.Location.Y, B.Location.Y) + Reach - G.Y0) / Bucket));
			for (int32 BY = BY0; BY <= BY1; ++BY) for (int32 BX = BX0; BX <= BX1; ++BX) Buckets[BY * BW + BX].Add(Seg);
		}
	}

	// Profondeur a l'interieur des lacs interieurs : cuvette, pas un puits de tranchee.
	TArray<int32> LakeDepthCells;
	LakeDepthCells.Init(0, N);
	{
		TArray<int32> Queue;
		for (int32 I = 0; I < N; ++I)
		{
			if (LakeOf[I] < 0) continue;
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
				if (!G.In(X, Y) || LakeOf[Y * G.W + X] != LakeOf[I]) { LakeDepthCells[I] = 1; Queue.Add(I); break; }
			}
		}
		for (int32 Q = 0; Q < Queue.Num(); ++Q)
		{
			const int32 I = Queue[Q];
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
				if (!G.In(X, Y)) continue;
				const int32 J = Y * G.W + X;
				if (LakeOf[J] == LakeOf[I] && !LakeDepthCells[J]) { LakeDepthCells[J] = LakeDepthCells[I] + 1; Queue.Add(J); }
			}
		}
	}
	// Rive d'un lac : deux sommets de marge portent son niveau, pour que la nappe atteigne la berge.
	TArray<int32> LakeShore;
	LakeShore.Init(INDEX_NONE, N);
	{
		TArray<int32> Front, Dist;
		Dist.Init(0, N);
		for (int32 I = 0; I < N; ++I) if (LakeOf[I] >= 0) { LakeShore[I] = LakeOf[I]; Front.Add(I); }
		for (int32 Q = 0; Q < Front.Num(); ++Q)
		{
			const int32 I = Front[Q];
			if (Dist[I] >= 2) continue;
			for (int32 K = 0; K < 8; ++K)
			{
				const int32 X = I % G.W + Off8X[K], Y = I / G.W + Off8Y[K];
				if (!G.In(X, Y)) continue;
				const int32 J = Y * G.W + X;
				if (LakeShore[J] == INDEX_NONE) { LakeShore[J] = LakeShore[I]; Dist[J] = Dist[I] + 1; Front.Add(J); }
			}
		}
	}

	TSortedMap<int32, TPair<double, int32>> NearestPerRiver;
	TArray<uint8> RiverWater;
	RiverWater.SetNumZeroed(N);
	TArray<double> Ground = R, Water, Flow, Wetness;
	Water.Init(TNumericLimits<double>::Lowest(), N);
	Flow.Init(0.0, N);
	Wetness.Init(0.0, N);
	for (int32 I = 0; I < N; ++I)
	{
		const int32 L = LakeOf[I];
		if (L >= 0)
		{
			const FLakeWork& Lake = Lakes[L];
			// Lac interieur : cuvette sous son niveau. Pas le fond de tranchee d'origine, creuse
			// sous la mer : sous un niveau releve de 20 m, il ferait des falaises.
			const double Bowl = Lake.Level - FMath::Min(450.0, 45.0 * LakeDepthCells[I]);
			// Mer et lac ecrit gardent leur fond d'origine, plus profond ; ce que l'arrondi leur
			// ajoute est creuse en cuvette. Lac interieur : cuvette partout.
			Ground[I] = (Lake.bBorder || Lake.bAuthored) ? FMath::Min(Z[I], Bowl) : Bowl;
			// WaterLook : le fond remonte vers la rive sur le talus droit (pente 0.15), jusqu'a
			// la ligne d'eau -- plus de marche entre la derniere cellule du lac et la greve.
			if (Params.bWaterLook && LakeSigned[I] < 0.0f && LakeSigned[I] > -800.0f)
			{
				Ground[I] = FMath::Max(Ground[I], Lake.Level + 0.15 * LakeSigned[I]);
			}
			Water[I] = Lake.Level;
		}
		const FVector2D P(G.WX(I), G.WY(I));
		const int32 BX = FMath::Clamp(FMath::FloorToInt((P.X - G.X0) / Bucket), 0, BW - 1);
		const int32 BY = FMath::Clamp(FMath::FloorToInt((P.Y - G.Y0) / Bucket), 0, BH - 1);
		double Target = TNumericLimits<double>::Max(), BestRel = TNumericLimits<double>::Max();
		double BestWs = 0.0, BestFlow = 0.0, BestWet = 0.0;
		// Dans un lit (et sa levee), l'eau est celle du plus bas des chenaux qui le couvrent :
		// a une confluence c'est le recepteur qui commande, sinon l'eau d'un affluent plus haut
		// noierait la berge que le recepteur a abaissee.
		double ChannelWs = TNumericLimits<double>::Max();
		bool bHit = false;
		// WaterLook : talus droit qui traverse la ligne d'eau (pente BankSlope, en cm par cm).
		// Sans lui le lit s'arrete a 10 cm sous l'eau et la berge repart 12 cm au-dessus dans la
		// meme maille de 5 m : la rive suivait la grille, en escalier sous le ruban d'eau.
		constexpr double BankSlope = 0.15;
		// Une seule cible par riviere : celle de son segment le plus proche. Prendre le minimum
		// sur tous les segments laisserait la levee d'un segment aval, plus basse, entamer la
		// berge du segment d'en face -- l'eau fuirait sur la pente.
		NearestPerRiver.Reset();
		for (const int32 SI : Buckets[BY * BW + BX])
		{
			const FSeg& Sg = Segs[SI];
			const FRiver& River = Out.Rivers[Sg.River];
			const FVector2D PA(River.Points[Sg.K].Location), PB(River.Points[Sg.K + 1].Location), D = PB - PA;
			const double T = FMath::Clamp(FVector2D::DotProduct(P - PA, D) / FMath::Max(D.SizeSquared(), 1.e-9), 0.0, 1.0);
			const double Dist = FVector2D::Distance(P, PA + T * D);
			TPair<double, int32>* Found = NearestPerRiver.Find(Sg.River);
			if (!Found) NearestPerRiver.Add(Sg.River, TPair<double, int32>(Dist, SI));
			else if (Dist < Found->Key || (Dist == Found->Key && SI < Found->Value)) *Found = TPair<double, int32>(Dist, SI);
		}
		for (const TPair<int32, TPair<double, int32>>& Entry : NearestPerRiver)
		{
			const int32 SI = Entry.Value.Value;
			const FSeg& Sg = Segs[SI];
			const FRiver& River = Out.Rivers[Sg.River];
			const FRiverPoint& A = River.Points[Sg.K];
			const FRiverPoint& B = River.Points[Sg.K + 1];
			const FVector2D PA(A.Location), PB(B.Location), D = PB - PA;
			const double T = FMath::Clamp(FVector2D::DotProduct(P - PA, D) / FMath::Max(D.SizeSquared(), 1.e-9), 0.0, 1.0);
			const double Dist = FVector2D::Distance(P, PA + T * D);
			const double WsHere = FMath::Lerp(A.Location.Z, B.Location.Z, T);
			const double HalfW = 0.5 * FMath::Lerp(A.Width, B.Width, T);
			const double Depth = FMath::Lerp(A.Depth, B.Depth, T);
			const double Fall = FMath::Lerp(A.BankFalloff, B.BankFalloff, T);
			bool bZone = false;
			for (const FZone& Zn : Zones) if (Zn.River == Sg.River && Sg.K >= Zn.K0 && Sg.K <= Zn.K1) bZone = true;
			const double Fp = bZone ? FMath::Clamp(4.0 * 2.0 * HalfW, 3000.0, 7000.0) : 0.0;
			if (Dist > HalfW + FMath::Max(Fall, Fp)) continue;
			bHit = true;
			double Tg;
			if (Dist <= HalfW)
			{
				// Lit parabolique : bord mouille a 10 cm sous la surface, fond a Depth.
				const double U = Dist / FMath::Max(HalfW, 1.0);
				Tg = WsHere - 10.0 - (Depth - 10.0) * FMath::Pow(FMath::Max(1.0 - U * U, 0.0), 0.7);
				if (Params.bWaterLook) Tg = FMath::Max(Tg, WsHere + BankSlope * (Dist - HalfW));
			}
			else
			{
				const double Tb = (Dist - HalfW) / FMath::Max(Fall, 1.0);
				const double Natural = R[I];
				const double Edge = WsHere + 12.0;
				Tg = Tb >= 1.0 ? Natural : Edge + (Natural - Edge) * FMath::Pow(SmoothStep(0.0, 1.0, Tb), 0.8);
				// Plancher de levee : en WaterLook il part de la ligne d'eau sur le talus droit.
				const double Lin = WsHere + BankSlope * (Dist - HalfW);
				const double EdgeFloor = Params.bWaterLook ? FMath::Min(Edge, Lin) : Edge;
				if (Params.bWaterLook && Tb < 1.0)
				{
					const double Curve = WsHere + (Natural - WsHere) * FMath::Pow(SmoothStep(0.0, 1.0, Tb), 0.8);
					Tg = Natural >= WsHere ? FMath::Min(FMath::Max(Lin, Curve), Natural) : Curve;
				}
				// Levee minimale, bornee a 60 cm au-dessus du terrain : la riviere ne deborde pas
				// sur une plaine un peu plus basse qu'elle, mais on n'eleve pas de digue.
				if (Tb < 1.0) Tg = FMath::Max(Tg, (Dist - HalfW) < 2.0 * G.S ? EdgeFloor : FMath::Min(EdgeFloor, Natural + 60.0));
				if (bZone)
				{
					// Plaine d'inondation : terrain ramene juste au-dessus de l'eau, jamais releve.
					const double Tf = (Dist - HalfW) / Fp;
					if (Tf < 1.0)
					{
						const double Plain = WsHere + 35.0 + 25.0 * Tf * Tf;
						Tg = FMath::Min(Tg, FMath::Lerp(Plain, Natural, SmoothStep(0.7, 1.0, Tf)));
						Tg = FMath::Max(Tg, FMath::Min(EdgeFloor, Natural + 60.0));
					}
				}
			}
			Target = FMath::Min(Target, Tg);
			if (Dist <= HalfW + 2.0 * G.S) ChannelWs = FMath::Min(ChannelWs, WsHere);
			const double Rel = Dist / FMath::Max(HalfW, 1.0);
			if (Rel < BestRel)
			{
				BestRel = Rel;
				BestWs = WsHere;
				const double Vel = FMath::Lerp(A.Velocity, B.Velocity, T);
				BestFlow = Dist <= HalfW + G.S ? FMath::Clamp(Vel / 3.5, 0.0, 1.0) : 0.0;
				BestWet = 1.0 - SmoothStep(HalfW, HalfW + FMath::Max(Fall, 1500.0), Dist);
			}
		}
		if (bHit)
		{
			// Dans un lac, le fond reste celui du lac : un lit de riviere creuse sous la nappe
			// se verrait comme une rayure a travers l'eau.
			if (L < 0) { Ground[I] = Target; Water[I] = ChannelWs < TNumericLimits<double>::Max() ? ChannelWs : BestWs; RiverWater[I] = 1; }
			Flow[I] = BestFlow;
			Wetness[I] = BestWet;
		}
		for (const FPond& Pd : Ponds)
		{
			const FVector2D Q = P - Pd.C;
			const double E = FMath::Sqrt(FMath::Square(FVector2D::DotProduct(Q, Pd.Axis) / Pd.A)
				+ FMath::Square(FVector2D::DotProduct(Q, FVector2D(-Pd.Axis.Y, Pd.Axis.X)) / Pd.B));
			if (E >= 1.4) continue;
			if (E < 1.0) Ground[I] = FMath::Min(Ground[I], Pd.Ws - 60.0 * (1.0 - E * E));
			if (Water[I] == TNumericLimits<double>::Lowest()) Water[I] = Pd.Ws;
			Wetness[I] = FMath::Max(Wetness[I], 1.0 - SmoothStep(1.0, 1.4, E));
		}
		if (Water[I] == TNumericLimits<double>::Lowest() && LakeShore[I] >= 0) Water[I] = Lakes[LakeShore[I]].Level;
		if (Water[I] == TNumericLimits<double>::Lowest()) Water[I] = FMath::Min(SeaZ, Ground[I] - 100.0);
	}

	// Rive de lac interieur : une greve de 20 m monte du niveau vers le terrain, jamais au-dessus
	// de lui. Un lac creuse dans un versant n'a pas de paroi.
	{
		TArray<int32> Dist, Owner;
		Dist.Init(MAX_int32, N);
		Owner.Init(INDEX_NONE, N);
		TArray<int32> Queue;
		for (int32 I = 0; I < N; ++I)
		{
			const int32 L = LakeOf[I];
			if (L >= 0 && !Lakes[L].bBorder && !Lakes[L].bAuthored) { Dist[I] = 0; Owner[I] = L; Queue.Add(I); }
		}
		for (int32 Q = 0; Q < Queue.Num(); ++Q)
		{
			const int32 I = Queue[Q];
			if (Dist[I] >= 4) continue;
			for (int32 K = 0; K < 8; ++K)
			{
				const int32 X = I % G.W + Off8X[K], Y = I / G.W + Off8Y[K];
				if (!G.In(X, Y)) continue;
				const int32 J = Y * G.W + X;
				if (Dist[J] == MAX_int32) { Dist[J] = Dist[I] + 1; Owner[J] = Owner[I]; Queue.Add(J); }
			}
		}
		for (int32 I = 0; I < N; ++I)
		{
			if (Dist[I] < 1 || Dist[I] > 4 || LakeOf[I] >= 0 || Flow[I] > 0.0) continue;
			const double Level = Lakes[Owner[I]].Level;
			// WaterLook : la greve suit la distance continue a la rive (talus droit qui part de la
			// ligne d'eau), pas le rang de cellule -- un palier de 30 cm a bord droit, sinon. Une
			// cellule que le seuil a laissee a terre du "mauvais" cote reste au moins 5 cm au sec.
			const float D = LakeSigned[I];
			if (Params.bWaterLook && D < 1999.0f)
			{
				Ground[I] = FMath::Min(Ground[I], Level + 0.15 * FMath::Max(static_cast<double>(D), 30.0));
				continue;
			}
			Ground[I] = FMath::Min(Ground[I], Level + 30.0 + 110.0 * Dist[I]);
		}
	}

	// Couture : le remplissage harmonique raccorde la hauteur mais pas la pente ; une bande
	// de trois sommets de part et d'autre du bord de reparation est lissee (Jacobi), hors
	// eau et hors lit, pour effacer le pli.
	{
		TArray<int32> Dist;
		Dist.Init(MAX_int32, N);
		TArray<int32> Queue;
		for (int32 I = 0; I < N; ++I)
		{
			if (!Repair[I]) continue;
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
				if (G.In(X, Y) && !Repair[Y * G.W + X]) { Dist[I] = 0; Queue.Add(I); break; }
			}
		}
		for (int32 Q = 0; Q < Queue.Num(); ++Q)
		{
			const int32 I = Queue[Q];
			if (Dist[I] >= 3) continue;
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
				if (!G.In(X, Y)) continue;
				const int32 J = Y * G.W + X;
				if (Dist[J] == MAX_int32) { Dist[J] = Dist[I] + 1; Queue.Add(J); }
			}
		}
		TArray<int32> Band;
		for (int32 I = 0; I < N; ++I)
		{
			if (Dist[I] <= 3 && !G.Border(I) && Ground[I] >= Water[I] && Flow[I] == 0.0 && Wetness[I] < 0.5 && LakeOf[I] < 0) Band.Add(I);
		}
		for (int32 It = 0; It < 6; ++It)
		{
			TArray<double> Next = Ground;
			for (const int32 I : Band)
			{
				double Sum = 0.0;
				for (int32 K = 0; K < 4; ++K) Sum += Ground[(I / G.W + Off4Y[K]) * G.W + I % G.W + Off4X[K]];
				Next[I] = FMath::Max(0.5 * Ground[I] + 0.125 * Sum, Water[I] + 12.0);
			}
			Ground = MoveTemp(Next);
		}
	}

	// L'eau n'existe que la ou le reseau la met : un creux isole sous le niveau d'un lac
	// voisin (quelques sommets de rive) est asseche plutot que laisse en flaque.
	{
		TArray<int32> Comp;
		Comp.Init(INDEX_NONE, N);
		for (int32 Start = 0; Start < N; ++Start)
		{
			if (Ground[Start] >= Water[Start] || Comp[Start] != INDEX_NONE) continue;
			TArray<int32> Cells = {Start};
			Comp[Start] = Start;
			bool bAnchored = false;
			for (int32 Q = 0; Q < Cells.Num(); ++Q)
			{
				const int32 I = Cells[Q];
				bAnchored |= LakeOf[I] >= 0 || Flow[I] > 0.0 || Wetness[I] >= 1.0 || G.Border(I);
				for (int32 K = 0; K < 4; ++K)
				{
					const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
					if (!G.In(X, Y)) continue;
					const int32 J = Y * G.W + X;
					if (Ground[J] < Water[J] && Comp[J] == INDEX_NONE) { Comp[J] = Start; Cells.Add(J); }
				}
			}
			if (!bAnchored) for (const int32 I : Cells) Water[I] = Ground[I] - 100.0;
		}
	}

	// Couleurs et canaux du sol sur la terre reparee : relus depuis la terre voisine.
	{
		TArray<uint8> Recolor;
		Recolor.SetNumZeroed(N);
		for (int32 I = 0; I < N; ++I) Recolor[I] = Repair[I] && LakeOf[I] < 0 ? 1 : 0;
		// WaterLook : toute terre seche qui porte encore le bleu d'une tuile d'eau reprend la
		// couleur de ses voisines -- sinon un halo cyan, de la peinture d'eau sans eau, borde les
		// rives et les anciens bras. Critere B > R : aucune teinte de terre (TileColor) n'a plus
		// de bleu que de rouge, alors que meme un melange a moitie herbe d'une eau peu profonde
		// en a ; tester B > G manquerait ces melanges, ou le vert de l'herbe l'emporte.
		if (Params.bWaterLook)
		{
			for (int32 I = 0; I < N; ++I)
			{
				const FLinearColor& C = Geo.Colors[I];
				if (Ground[I] >= Water[I] && C.B > C.R + 0.04f) Recolor[I] = 1;
			}
		}
		// WaterLook : 120 iterations de relaxation ne traversent pas un ancien bras de 30 mailles,
		// dont le coeur gardait son bleu d'origine (Anastasis.Terrain.Drainage.WaterLook en comptait
		// 2 800 sommets). On part donc de la terre seche la plus proche, puis on lisse.
		TArray<int32> NearestLand;
		if (Params.bWaterLook)
		{
			NearestLand.Init(INDEX_NONE, N);
			TArray<int32> Queue;
			for (int32 I = 0; I < N; ++I)
			{
				if (!Recolor[I] && Ground[I] >= Water[I]) { NearestLand[I] = I; Queue.Add(I); }
			}
			for (int32 Q = 0; Q < Queue.Num(); ++Q)
			{
				const int32 I = Queue[Q];
				for (int32 K = 0; K < 4; ++K)
				{
					const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
					if (!G.In(X, Y)) continue;
					const int32 J = Y * G.W + X;
					if (Recolor[J] && NearestLand[J] == INDEX_NONE) { NearestLand[J] = NearestLand[I]; Queue.Add(J); }
				}
			}
		}
		TArray<double> Ch;
		Ch.SetNumUninitialized(N);
		auto FillChannel = [&](TFunctionRef<double(int32)> Get, TFunctionRef<void(int32, double)> Set)
		{
			for (int32 I = 0; I < N; ++I) Ch[I] = Get(I);
			if (NearestLand.Num() == N)
			{
				for (int32 I = 0; I < N; ++I) if (Recolor[I] && NearestLand[I] != INDEX_NONE) Ch[I] = Get(NearestLand[I]);
			}
			Harmonic(G, Ch, Recolor, 120);
			for (int32 I = 0; I < N; ++I) if (Recolor[I]) Set(I, Ch[I]);
		};
		FillChannel([&](int32 I) { return Geo.Colors[I].R; }, [&](int32 I, double V) { Geo.Colors[I].R = static_cast<float>(V); });
		FillChannel([&](int32 I) { return Geo.Colors[I].G; }, [&](int32 I, double V) { Geo.Colors[I].G = static_cast<float>(V); });
		FillChannel([&](int32 I) { return Geo.Colors[I].B; }, [&](int32 I, double V) { Geo.Colors[I].B = static_cast<float>(V); });
		if (NearestLand.Num() == N)
		{
			// Le lissage tire aussi vers les sommets immerges, encore bleus a ce stade (leur fond
			// est peint a l'ecriture) : au bord de l'eau, une terre seche peut en rester bleutee.
			// Elle prend alors franchement la teinte de la terre la plus proche.
			for (int32 I = 0; I < N; ++I)
			{
				FLinearColor& C = Geo.Colors[I];
				if (!Recolor[I] || Ground[I] < Water[I] || C.B <= C.R + 0.04f) continue;
				if (NearestLand[I] != INDEX_NONE)
				{
					const FLinearColor& L = Geo.Colors[NearestLand[I]];
					C = FLinearColor(L.R, L.G, L.B, C.A);
				}
				else C.B = C.R;
			}
		}
		if (Geo.UV0.Num() == N)
		{
			FillChannel([&](int32 I) { return Geo.UV0[I].X; }, [&](int32 I, double V) { Geo.UV0[I].X = V; });
			FillChannel([&](int32 I) { return Geo.UV0[I].Y; }, [&](int32 I, double V) { Geo.UV0[I].Y = V; });
		}
		if (Geo.UV1.Num() == N)
		{
			FillChannel([&](int32 I) { return Geo.UV1[I].X; }, [&](int32 I, double V) { Geo.UV1[I].X = V; });
			FillChannel([&](int32 I) { return Geo.UV1[I].Y; }, [&](int32 I, double V) { Geo.UV1[I].Y = V; });
		}
	}

	// WaterLook : l'humidite que lit le sol (UV1.y) ne vaut que pres d'une eau reellement
	// rendue. Celle de la simulation couvre aussi les tuiles d'eau que ce reseau a assechees ;
	// M_AnastasisGround y abaisse la rugosite, et le sol mouille reflete le ciel : une aureole
	// cyan, de l'eau peinte sans eau. Plafond : 1 au contact, 0 a 8 mailles (40 m).
	TArray<int32> WaterSteps;
	if (Params.bWaterLook && Geo.UV1.Num() == N)
	{
		WaterSteps.Init(MAX_int32, N);
		TArray<int32> Queue;
		for (int32 I = 0; I < N; ++I) if (Ground[I] < Water[I]) { WaterSteps[I] = 0; Queue.Add(I); }
		for (int32 Q = 0; Q < Queue.Num(); ++Q)
		{
			const int32 I = Queue[Q];
			if (WaterSteps[I] >= 8) continue;
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 X = I % G.W + Off4X[K], Y = I / G.W + Off4Y[K];
				if (!G.In(X, Y)) continue;
				const int32 J = Y * G.W + X;
				if (WaterSteps[J] == MAX_int32) { WaterSteps[J] = WaterSteps[I] + 1; Queue.Add(J); }
			}
		}
	}

	// Ecriture.
	Geo.RiverFlow.SetNumZeroed(N);
	for (int32 I = 0; I < N; ++I)
	{
		Geo.Vertices[I].Z = Ground[I];
		Geo.WaterVertices[I].Z = Water[I];
		Geo.RiverFlow[I] = static_cast<float>(Flow[I]);
		const bool bUnder = Ground[I] < Water[I];
		if (Geo.UV1.Num() == N) Geo.UV1[I].Y = FMath::Max(Geo.UV1[I].Y, Wetness[I]);
		if (WaterSteps.Num() == N)
		{
			const double Cap = WaterSteps[I] == MAX_int32 ? 0.0 : 1.0 - SmoothStep(0.0, 8.0, WaterSteps[I]);
			Geo.UV1[I].Y = static_cast<float>(FMath::Min(static_cast<double>(Geo.UV1[I].Y), Cap));
		}
		Geo.Colors[I].A = bUnder ? 1.0f : 0.0f;
		if (Params.bWaterLook && bUnder)
		{
			// Fond immerge. Le degrade profondeur (sable / gravier pres de la rive, vase au
			// large) est module par deux taches de ~16 m, pour que le lit ne soit pas un
			// aplat. Toutes ces teintes gardent B <= R - 0.04 : le test scelle refuse un
			// fond peint en bleu de tuile. Le grain fin reste celui du materiau de sol.
			auto Hash01 = [](int32 A, int32 B) -> float
			{
				uint32 H = static_cast<uint32>(A) * 374761393u ^ static_cast<uint32>(B) * 668265263u;
				H = (H ^ (H >> 13)) * 1274126177u;
				return static_cast<float>((H ^ (H >> 16)) & 0xFFFFu) / 65535.0f;
			};
			const int32 X0 = FMath::FloorToInt(Geo.Vertices[I].X / 1600.0);
			const int32 Y0 = FMath::FloorToInt(Geo.Vertices[I].Y / 1600.0);
			const float Coarse = Hash01(X0, Y0);
			const float Fine = Hash01(X0 * 2 + 5, Y0 * 2 - 3);
			const double Deep = SmoothStep(0.0, 180.0, Water[I] - Ground[I]);
			const FLinearColor Sand(0.40f, 0.34f, 0.22f);
			const FLinearColor Gravel(0.29f, 0.26f, 0.19f);
			const FLinearColor Silt(0.15f, 0.13f, 0.09f);
			const FLinearColor Stone(0.22f, 0.20f, 0.15f);
			FLinearColor Bed = FMath::Lerp(FMath::Lerp(Sand, Gravel, Coarse), FMath::Lerp(Silt, Stone, Fine * Fine), static_cast<float>(Deep));
			if (Fine > 0.86f) Bed = FMath::Lerp(Bed, Stone, 0.45f * static_cast<float>(1.0 - Deep));
			Geo.Colors[I] = FLinearColor(Bed.R, Bed.G, Bed.B, 1.0f);
		}
		if (Wet[I] && LakeOf[I] < 0) (bUnder ? Out.KeptWaterVertices : Out.RepairedWaterVertices) += 1;
		else if (Wet[I]) Out.KeptWaterVertices += 1;
	}
	Geo.WaterTriangles.Reset();
	for (int32 Y = 0; Y + 1 < G.H; ++Y)
	{
		for (int32 X = 0; X + 1 < G.W; ++X)
		{
			const int32 A = Y * G.W + X, B = A + 1, C = A + G.W, D = C + 1;
			if (Ground[A] < Water[A] || Ground[B] < Water[B] || Ground[C] < Water[C] || Ground[D] < Water[D])
			{
				Geo.WaterTriangles.Append({A, C, B, B, C, D});
				// Les rivieres sont dessinees par leurs rubans : la grille ne garde que les quads
				// ou un sommet mouille n'est pas de riviere (lac, mer, mare, embouchure).
				const int32 Q[4] = {A, B, C, D};
				bool bStill = false;
				for (const int32 V : Q) bStill |= Ground[V] < Water[V] && !RiverWater[V];
				if (bStill) Out.LakeWaterTriangles.Append({A, C, B, B, C, D});
			}
		}
	}
	AnastasisTerrainForge::RecomputeNormals(Geo);
	Mesh.MinZ = TNumericLimits<double>::Max();
	Mesh.MaxZ = TNumericLimits<double>::Lowest();
	for (const FVector& P : Geo.Vertices) { Mesh.MinZ = FMath::Min(Mesh.MinZ, P.Z); Mesh.MaxZ = FMath::Max(Mesh.MaxZ, P.Z); }
	if (Mesh.bBasinFound || Mesh.bHumanGeography) AnastasisTerrainForge::SampleHeight(Mesh, Mesh.BasinX, Mesh.BasinY, Mesh.BasinZ);
	if (Mesh.bLandmarkFound) AnastasisTerrainForge::SampleHeight(Mesh, Mesh.LandmarkX, Mesh.LandmarkY, Mesh.LandmarkZ);

	// Lacs publies.
	for (int32 L = 0; L < Lakes.Num(); ++L)
	{
		FLake& Lake = Out.Lakes.AddDefaulted_GetRef();
		Lake.SurfaceZ = Lakes[L].Level;
		Lake.Cells = Lakes[L].Cells.Num();
		Lake.bBorder = Lakes[L].bBorder;
		FVector Sum = FVector::ZeroVector;
		for (const int32 I : Lakes[L].Cells) Sum += FVector(G.WX(I), G.WY(I), Lakes[L].Level);
		Lake.Centroid = Sum / FMath::Max(1, Lakes[L].Cells.Num());
		for (int32 K = 0; K < Lakes[L].Cells.Num(); K += 16) Lake.Anchors.Add(FVector2D(G.WX(Lakes[L].Cells[K]), G.WY(Lakes[L].Cells[K])));
	}
	for (const FRiver& River : Out.Rivers)
	{
		if (River.MouthLake >= 0) ++Out.Lakes[River.MouthLake].Inflows;
		if (River.SourceLake >= 0) ++Out.Lakes[River.SourceLake].Outflows;
	}
	for (int32 L = 0; L < Lakes.Num(); ++L)
	{
		if (Lakes[L].SpillsInto == INDEX_NONE) continue;
		++Out.Lakes[L].Outflows;
		++Out.Lakes[Lakes[L].SpillsInto].Inflows;
	}
	Out.Riparian.SetNumUninitialized(N);
	for (int32 I = 0; I < N; ++I) Out.Riparian[I] = static_cast<float>(Wetness[I]);
	Out.GridW = G.W;
	Out.GridH = G.H;
	Out.GridX0 = G.X0;
	Out.GridY0 = G.Y0;
	Out.GridStep = G.S;
	Out.MilliSeconds = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	return true;
}

AnastasisDrainage::FCheck AnastasisDrainage::Check(const FNetwork& Network, const AnastasisTerrainForge::FMesh& Mesh)
{
	FCheck C;
	const auto& Geo = Mesh.Geometry;
	const int32 W = Mesh.FineW, H = Mesh.FineH;
	if (W < 2 || H < 2 || Geo.Vertices.Num() != W * H) return C;
	const double S = Geo.Vertices[1].X - Geo.Vertices[0].X;
	const double X0 = Geo.Vertices[0].X, Y0 = Geo.Vertices[0].Y;
	auto Index = [&](const FVector2D& P) -> int32
	{
		const int32 X = FMath::Clamp(FMath::RoundToInt((P.X - X0) / S), 0, W - 1);
		const int32 Y = FMath::Clamp(FMath::RoundToInt((P.Y - Y0) / S), 0, H - 1);
		return Y * W + X;
	};
	auto Under = [&](int32 I) { return Geo.Vertices[I].Z < Geo.WaterVertices[I].Z; };
	int32 Contained = 0, Probed = 0;
	for (const FRiver& River : Network.Rivers)
	{
		for (int32 K = 1; K < River.Points.Num(); ++K)
		{
			if (River.Points[K].Location.Z > River.Points[K - 1].Location.Z + 1.0) ++C.UphillSteps;
			if (River.Points[K].Width < River.Points[K - 1].Width - 1.0) ++C.NarrowingSteps;
		}
		const FVector2D End(River.Points.Last().Location);
		switch (River.Mouth)
		{
		case EMouth::River:
		{
			if (!Network.Rivers.IsValidIndex(River.Parent)) { ++C.DanglingMouths; break; }
			const FRiver& Parent = Network.Rivers[River.Parent];
			int32 Best = 0;
			double BestD = TNumericLimits<double>::Max();
			for (int32 K = 0; K < Parent.Points.Num(); ++K)
			{
				const double D = FVector2D::Distance(End, FVector2D(Parent.Points[K].Location));
				if (D < BestD) { BestD = D; Best = K; }
			}
			if (BestD > 0.5 * Parent.Points[Best].Width + 2.0 * S) ++C.DanglingMouths;
			const int32 After = FMath::Min(Best + 2, Parent.Points.Num() - 1);
			if (Parent.Points[After].Width < River.Points.Last().Width - 1.0) ++C.ConfluenceNarrower;
			break;
		}
		case EMouth::Lake:
			if (!Under(Index(End))) ++C.DanglingMouths;
			break;
		case EMouth::Border:
		{
			const int32 I = Index(End);
			const int32 X = I % W, Y = I / W;
			if (FMath::Min(FMath::Min(X, Y), FMath::Min(W - 1 - X, H - 1 - Y)) > 3) ++C.DanglingMouths;
			break;
		}
		}
		for (int32 K = 2; K + 2 < River.Points.Num(); K += 2)
		{
			const FRiverPoint& P = River.Points[K];
			const FVector2D T = (FVector2D(River.Points[K + 1].Location) - FVector2D(River.Points[K - 1].Location)).GetSafeNormal();
			const FVector2D Nm(-T.Y, T.X);
			bool bOk = true, bInside = true;
			for (const double Side : {-1.0, 1.0})
			{
				const FVector2D Q = FVector2D(P.Location) + Nm * Side * (0.5 * P.Width + 0.5 * P.BankFalloff);
				if (Q.X < X0 || Q.Y < Y0 || Q.X > X0 + (W - 1) * S || Q.Y > Y0 + (H - 1) * S) { bInside = false; break; }
				double Zq = 0.0;
				if (!AnastasisTerrainForge::SampleHeight(Mesh, Q.X, Q.Y, Zq)) { bInside = false; break; }
				// Un point de berge immerge dans un lac ou une autre riviere n'est pas une fuite.
				const int32 Qi = Index(Q);
				if (Zq < P.Location.Z - 5.0 && !(Under(Qi) && Geo.WaterVertices[Qi].Z <= P.Location.Z + 5.0)) bOk = false;
			}
			if (!bInside) continue;
			++Probed;
			Contained += bOk ? 1 : 0;
		}
	}
	C.BankContainment = Probed > 0 ? static_cast<double>(Contained) / Probed : 1.0;

	// Plans d'eau rendus : chacun porte une riviere, un lac retenu, un bord, ou une mare de plaine.
	TArray<int32> Comp;
	Comp.Init(INDEX_NONE, W * H);
	TArray<uint8> Anchored;
	int32 NumComp = 0;
	for (int32 Start = 0; Start < W * H; ++Start)
	{
		if (!Under(Start) || Comp[Start] != INDEX_NONE) continue;
		TArray<int32> Cells = {Start};
		Comp[Start] = NumComp;
		bool bBorder = false;
		for (int32 Q = 0; Q < Cells.Num(); ++Q)
		{
			const int32 I = Cells[Q];
			const int32 X = I % W, Y = I / W;
			bBorder |= X == 0 || Y == 0 || X == W - 1 || Y == H - 1;
			for (int32 K = 0; K < 4; ++K)
			{
				const int32 NX = X + Off4X[K], NY = Y + Off4Y[K];
				if (NX < 0 || NY < 0 || NX >= W || NY >= H) continue;
				const int32 J = NY * W + NX;
				if (Under(J) && Comp[J] == INDEX_NONE) { Comp[J] = NumComp; Cells.Add(J); }
			}
		}
		Anchored.Add(bBorder ? uint8(1) : uint8(0));
		++NumComp;
	}
	auto Anchor = [&](const FVector2D& P, double Radius)
	{
		const int32 R = FMath::CeilToInt(Radius / S);
		const int32 C0 = Index(P);
		for (int32 DY = -R; DY <= R; ++DY)
		{
			for (int32 DX = -R; DX <= R; ++DX)
			{
				const int32 X = C0 % W + DX, Y = C0 / W + DY;
				if (X < 0 || Y < 0 || X >= W || Y >= H) continue;
				const int32 I = Y * W + X;
				if (Comp[I] != INDEX_NONE) Anchored[Comp[I]] = 1;
			}
		}
	};
	for (const FRiver& River : Network.Rivers) for (const FRiverPoint& P : River.Points) Anchor(FVector2D(P.Location), S);
	for (const FLake& Lake : Network.Lakes) for (const FVector2D& A : Lake.Anchors) Anchor(A, S);
	for (const FWetland& Wl : Network.Wetlands) Anchor(FVector2D(Wl.Center), 15000.0);
	for (const uint8 A : Anchored) if (!A) ++C.IsolatedWaterBodies;
	for (const FLake& Lake : Network.Lakes) if (!Lake.bBorder && Lake.Inflows + Lake.Outflows == 0) ++C.LakesWithoutRole;
	return C;
}

FString AnastasisDrainage::Describe(const FNetwork& Network)
{
	int32 MaxOrder = 0;
	double MaxWidth = 0.0, MinWidth = TNumericLimits<double>::Max(), Length = 0.0;
	int32 ToBorder = 0, ToLake = 0, ToRiver = 0;
	for (const FRiver& River : Network.Rivers)
	{
		MaxOrder = FMath::Max(MaxOrder, River.Order);
		Length += River.LengthM;
		for (const FRiverPoint& P : River.Points) { MaxWidth = FMath::Max(MaxWidth, P.Width); MinWidth = FMath::Min(MinWidth, P.Width); }
		(River.Mouth == EMouth::Border ? ToBorder : River.Mouth == EMouth::Lake ? ToLake : ToRiver) += 1;
	}
	int32 Interior = 0;
	for (const FLake& Lake : Network.Lakes) Interior += Lake.bBorder ? 0 : 1;
	return FString::Printf(
		TEXT("rivers=%d heads=%d confluences=%d max_order=%d length_m=%.0f width_m=[%.1f,%.1f] mouths(river/lake/border)=%d/%d/%d lakes=%d interior_lakes=%d wetlands=%d area_slope_m2=%.0f basin_lakes=%d filled_pits=%d repaired_water_vertices=%d kept_water_vertices=%d ms=%.0f"),
		Network.Rivers.Num(), Network.Heads, Network.Confluences, MaxOrder, Length,
		Network.Rivers.Num() ? MinWidth / 100.0 : 0.0, MaxWidth / 100.0, ToRiver, ToLake, ToBorder,
		Network.Lakes.Num(), Interior, Network.Wetlands.Num(), Network.ChannelAreaSlopeM2, Network.BasinLakes, Network.FilledPits,
		Network.RepairedWaterVertices, Network.KeptWaterVertices, Network.MilliSeconds);
}

void AnastasisDrainage::DrawDebug(UWorld* World, const FNetwork& Network, int32 Mode)
{
	if (!World) return;
	FlushPersistentDebugLines(World);
	if (Mode <= 0) return;
	double Lo = TNumericLimits<double>::Max(), Hi = TNumericLimits<double>::Lowest();
	auto Value = [Mode](const FRiverPoint& P) -> double
	{
		switch (Mode)
		{
		case 1: return P.Width / 100.0;
		case 2: return P.Depth / 100.0;
		case 3: return P.Velocity;
		default: return P.Order;
		}
	};
	for (const FRiver& River : Network.Rivers) for (const FRiverPoint& P : River.Points) { Lo = FMath::Min(Lo, Value(P)); Hi = FMath::Max(Hi, Value(P)); }
	for (const FRiver& River : Network.Rivers)
	{
		for (int32 K = 0; K + 1 < River.Points.Num(); ++K)
		{
			const FRiverPoint& A = River.Points[K];
			const double T = Hi > Lo ? (Value(A) - Lo) / (Hi - Lo) : 0.0;
			// Bleu (petit) -> rouge (grand).
			const FColor Color = FLinearColor::LerpUsingHSV(FLinearColor(0.1f, 0.3f, 1.0f), FLinearColor(1.0f, 0.15f, 0.05f), static_cast<float>(T)).ToFColor(true);
			const float Thickness = Mode == 1 ? static_cast<float>(FMath::Max(A.Width * 0.5, 150.0)) : 400.0f;
			DrawDebugLine(World, A.Location + FVector(0, 0, 150), River.Points[K + 1].Location + FVector(0, 0, 150), Color, true, -1.0f, 0, Thickness);
		}
	}
}

bool AnastasisDrainage::DumpIfRequested(const FNetwork& Network)
{
	const FString Path = CVarDrainageDump.GetValueOnGameThread();
	if (Path.IsEmpty()) return false;
	FString Json = TEXT("{\"rivers\":[");
	for (int32 RI = 0; RI < Network.Rivers.Num(); ++RI)
	{
		const FRiver& River = Network.Rivers[RI];
		Json += FString::Printf(TEXT("%s{\"order\":%d,\"mouth\":%d,\"parent\":%d,\"source_lake\":%d,\"mouth_lake\":%d,\"authored\":%d,\"length_m\":%.1f,\"points\":["),
			RI ? TEXT(",") : TEXT(""), River.Order, static_cast<int32>(River.Mouth), River.Parent, River.SourceLake, River.MouthLake, River.bAuthored ? 1 : 0, River.LengthM);
		for (int32 K = 0; K < River.Points.Num(); ++K)
		{
			const FRiverPoint& P = River.Points[K];
			Json += FString::Printf(TEXT("%s[%.0f,%.0f,%.1f,%.0f,%.0f,%.2f,%.0f,%.5f,%d]"), K ? TEXT(",") : TEXT(""),
				P.Location.X, P.Location.Y, P.Location.Z, P.Width, P.Depth, P.Velocity, P.BankFalloff, P.Slope, P.Order);
		}
		Json += TEXT("]}");
	}
	Json += TEXT("],\"lakes\":[");
	for (int32 L = 0; L < Network.Lakes.Num(); ++L)
	{
		const FLake& Lake = Network.Lakes[L];
		Json += FString::Printf(TEXT("%s{\"z\":%.0f,\"cells\":%d,\"border\":%d,\"inflows\":%d,\"outflows\":%d,\"at\":[%.0f,%.0f]}"),
			L ? TEXT(",") : TEXT(""), Lake.SurfaceZ, Lake.Cells, Lake.bBorder ? 1 : 0, Lake.Inflows, Lake.Outflows, Lake.Centroid.X, Lake.Centroid.Y);
	}
	Json += TEXT("],\"wetlands\":[");
	for (int32 K = 0; K < Network.Wetlands.Num(); ++K)
	{
		const FWetland& Wl = Network.Wetlands[K];
		Json += FString::Printf(TEXT("%s{\"at\":[%.0f,%.0f,%.0f],\"river\":%d,\"ponds\":%d,\"length_m\":%.0f}"),
			K ? TEXT(",") : TEXT(""), Wl.Center.X, Wl.Center.Y, Wl.Center.Z, Wl.River, Wl.Ponds, Wl.LengthM);
	}
	Json += FString::Printf(TEXT("],\"summary\":\"%s\"}"), *Describe(Network));
	return FFileHelper::SaveStringToFile(Json, *Path);
}

void AnastasisDrainage::BuildRiverRibbons(const FNetwork& Network, FWaterRibbons& Out)
{
	Out = FWaterRibbons{};
	for (int32 RI = 0; RI < Network.Rivers.Num(); ++RI)
	{
		const FRiver& River = Network.Rivers[RI];
		const int32 Num = River.Points.Num();
		if (Num < 2) continue;
		// Profondeur dans l'arbre : chaque affluent 1.5 cm sous son recepteur ; toute riviere
		// 2 cm sous les lacs (ses embouchures plongent sous la nappe du lac, qui les couvre).
		int32 Tier = 0;
		for (int32 P = River.Parent, Guard = 0; P != INDEX_NONE && Guard < 64; P = Network.Rivers[P].Parent, ++Guard) ++Tier;
		const double Lower = 2.0 + 1.5 * Tier;
		const int32 Base = Out.Vertices.Num();
		double Arc = 0.0;
		for (int32 K = 0; K < Num; ++K)
		{
			const FRiverPoint& P = River.Points[K];
			if (K > 0) Arc += FVector::Dist2D(River.Points[K - 1].Location, P.Location);
			const FVector2D Tangent = (FVector2D(River.Points[FMath::Min(K + 1, Num - 1)].Location)
				- FVector2D(River.Points[FMath::Max(K - 1, 0)].Location)).GetSafeNormal();
			const FVector2D Normal(-Tangent.Y, Tangent.X);
			// 3.5 m sous chaque berge (le talus y est ~50 cm au-dessus de l eau) : la rive visible
			// est l intersection avec le talus, meme dans les coudes ou le ruban se deforme.
			const double Half = 0.5 * P.Width + 350.0;
			const double Z = P.Location.Z - Lower;
			const FVector2D C(P.Location);
			const FVector2D L = C + Normal * Half, R = C - Normal * Half;
			const FVector2D Flow = Tangent * FMath::Clamp(P.Velocity / 3.0, 0.0, 1.0);
			// Courbure signee : produit vectoriel des tangentes amont/aval, ramene a [-1,1].
			// Un coude d'environ 25 deg sur 25 m vaut ~0.5. Positif = virage a gauche.
			double Curv = 0.0;
			if (K > 0 && K + 1 < Num)
			{
				const FVector2D Prev(River.Points[K - 1].Location);
				const FVector2D Next(River.Points[K + 1].Location);
				const FVector2D Here(P.Location);
				const FVector2D InDir = (Here - Prev).GetSafeNormal();
				const FVector2D OutDir = (Next - Here).GetSafeNormal();
				const double Cross = InDir.X * OutDir.Y - InDir.Y * OutDir.X;
				const double Chord = FVector2D::Distance(Prev, Next);
				Curv = FMath::Clamp(Cross * 2800.0 / FMath::Max(Chord, 200.0), -1.0, 1.0);
			}
			const FVector2D Curve(Curv, 0.0);
			const FVector2D Body(P.Depth / 100.0, P.Slope);
			Out.Vertices.Add(FVector(L.X, L.Y, Z));
			Out.Vertices.Add(FVector(R.X, R.Y, Z));
			Out.UV0.Add(FVector2D(Arc / 1000.0, -1.0));
			Out.UV0.Add(FVector2D(Arc / 1000.0, 1.0));
			for (int32 Side = 0; Side < 2; ++Side)
			{
				Out.Normals.Add(FVector::UpVector);
				Out.UV1.Add(Curve);
				Out.UV2.Add(Flow);
				Out.UV3.Add(Body);
				Out.Colors.Add(FLinearColor(0.043f, 0.176f, 0.290f, 1.0f));
			}
		}
		for (int32 K = 0; K + 1 < Num; ++K)
		{
			const int32 L0 = Base + 2 * K, R0 = L0 + 1, L1 = L0 + 2, R1 = L0 + 3;
			// Meme sens d'enroulement que la grille (face visible vers le haut), teste triangle par
			// triangle : dans un coude serre, le quad se replie et ses deux moities n'ont plus le
			// meme signe (12 triangles retournes sur 2 654 avant ce test).
			const auto AddUp = [&Out](int32 A, int32 B, int32 C)
			{
				const FVector& PA = Out.Vertices[A];
				const FVector& PB = Out.Vertices[B];
				const FVector& PC = Out.Vertices[C];
				const double Cross = (PB.X - PA.X) * (PC.Y - PA.Y) - (PB.Y - PA.Y) * (PC.X - PA.X);
				if (Cross < 0.0) Out.Triangles.Append({A, B, C});
				else Out.Triangles.Append({A, C, B});
			};
			AddUp(L0, R0, L1);
			AddUp(R0, R1, L1);
		}
	}
}
