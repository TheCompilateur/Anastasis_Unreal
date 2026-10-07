#include "WorldTheatre/AnastasisWorldTheatre.h"

namespace AnastasisWorldTheatre
{
void FMeshData::Append(const FMeshData& Other)
{
	const int32 Base = Vertices.Num();
	Vertices.Append(Other.Vertices);
	Normals.Append(Other.Normals);
	Colours.Append(Other.Colours);
	Triangles.Reserve(Triangles.Num() + Other.Triangles.Num());
	for (const int32 T : Other.Triangles) Triangles.Add(Base + T);
}

bool PointInPolygon(const TArray<FVector2D>& Poly, const FVector2D& P)
{
	bool bIn = false;
	for (int32 I = 0, J = Poly.Num() - 1; I < Poly.Num(); J = I++)
	{
		const FVector2D& A = Poly[I];
		const FVector2D& B = Poly[J];
		if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X) bIn = !bIn;
	}
	return bIn;
}

double DistanceToOutline(const TArray<FVector2D>& Poly, const FVector2D& P)
{
	double Best = TNumericLimits<double>::Max();
	for (int32 I = 0, J = Poly.Num() - 1; I < Poly.Num(); J = I++)
	{
		const FVector2D A = Poly[J], B = Poly[I];
		const FVector2D AB = B - A;
		const double L2 = AB.SizeSquared();
		const double T = L2 > 0 ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
		Best = FMath::Min(Best, FVector2D::Distance(P, A + AB * T));
	}
	return Best;
}

namespace
{
/** Grain de couronnes : un multiplicateur stable par noeud de grille, 0,72 a 1,16. Pas une forme. */
double CrownGrain(int32 I, int32 J)
{
	uint32 H = static_cast<uint32>(I) * 73856093u ^ static_cast<uint32>(J) * 19349663u;
	H ^= H >> 13;
	H *= 0x5bd1e995u;
	H ^= H >> 15;
	return 0.72 + 0.44 * (H & 0xFFFF) / 65535.0;
}

void RecomputeNormals(FMeshData& M, int32 FirstVertex)
{
	for (int32 V = FirstVertex; V < M.Vertices.Num(); ++V) M.Normals[V] = FVector::ZeroVector;
	for (int32 T = 0; T + 2 < M.Triangles.Num(); T += 3)
	{
		const int32 A = M.Triangles[T], B = M.Triangles[T + 1], C = M.Triangles[T + 2];
		if (A < FirstVertex) continue;
		const FVector N = FVector::CrossProduct(M.Vertices[C] - M.Vertices[A], M.Vertices[B] - M.Vertices[A]);
		M.Normals[A] += N;
		M.Normals[B] += N;
		M.Normals[C] += N;
	}
	for (int32 V = FirstVertex; V < M.Vertices.Num(); ++V) M.Normals[V] = M.Normals[V].GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector);
}

/** Face plane a sommets propres (aretes nettes : une silhouette se lit par ses aretes). Sens : normale vers l'exterieur. */
void AddQuad(FMeshData& M, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Colour)
{
	const FVector N = FVector::CrossProduct(D - A, B - A).GetSafeNormal();
	const int32 Base = M.Vertices.Num();
	for (const FVector& P : { A, B, C, D })
	{
		M.Vertices.Add(P);
		M.Normals.Add(N);
		M.Colours.Add(Colour);
	}
	M.Triangles.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
}

void AddTriangle(FMeshData& M, const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Colour)
{
	const FVector N = FVector::CrossProduct(C - A, B - A).GetSafeNormal();
	const int32 Base = M.Vertices.Num();
	for (const FVector& P : { A, B, C })
	{
		M.Vertices.Add(P);
		M.Normals.Add(N);
		M.Colours.Add(Colour);
	}
	M.Triangles.Append({ Base, Base + 1, Base + 2 });
}

/**
 * Volume a toit a deux pans (ou plat si RoofRise = 0) dans un repere local : X long, Y large, Z haut.
 * Pignons sur les faces X. Le volume descend sous le sol (Sink) pour qu'aucune pente ne le decolle.
 */
void AddGabledBox(FMeshData& M, const FTransform& T, double Len, double Wid, double Wall, double RoofRise, double Sink,
	const FLinearColor& WallColour, const FLinearColor& RoofColour)
{
	const double X = Len * 0.5, Y = Wid * 0.5;
	auto P = [&T](double PX, double PY, double PZ) { return T.TransformPosition(FVector(PX, PY, PZ)); };
	const FVector B0 = P(-X, -Y, -Sink), B1 = P(X, -Y, -Sink), B2 = P(X, Y, -Sink), B3 = P(-X, Y, -Sink);
	const FVector T0 = P(-X, -Y, Wall), T1 = P(X, -Y, Wall), T2 = P(X, Y, Wall), T3 = P(-X, Y, Wall);
	AddQuad(M, B0, T0, T1, B1, WallColour);
	AddQuad(M, B1, T1, T2, B2, WallColour);
	AddQuad(M, B2, T2, T3, B3, WallColour);
	AddQuad(M, B3, T3, T0, B0, WallColour);
	if (RoofRise <= 0)
	{
		AddQuad(M, T0, T3, T2, T1, RoofColour);
		return;
	}
	const FVector R0 = P(-X, 0, Wall + RoofRise), R1 = P(X, 0, Wall + RoofRise);
	AddQuad(M, T0, R0, R1, T1, RoofColour);
	AddQuad(M, T2, R1, R0, T3, RoofColour);
	AddTriangle(M, T1, R1, T2, WallColour);
	AddTriangle(M, T3, R0, T0, WallColour);
}

/** Tour : fut carre et toit en pyramide basse. */
void AddTower(FMeshData& M, const FTransform& T, double Side, double Height, double Roof, double Sink,
	const FLinearColor& WallColour, const FLinearColor& RoofColour)
{
	AddGabledBox(M, T, Side, Side, Height, 0.0, Sink, WallColour, WallColour);
	const double S = Side * 0.5 + 40.0;
	auto P = [&T](double PX, double PY, double PZ) { return T.TransformPosition(FVector(PX, PY, PZ)); };
	const FVector C0 = P(-S, -S, Height), C1 = P(S, -S, Height), C2 = P(S, S, Height), C3 = P(-S, S, Height);
	const FVector Apex = P(0, 0, Height + Roof);
	AddTriangle(M, C0, Apex, C1, RoofColour);
	AddTriangle(M, C1, Apex, C2, RoofColour);
	AddTriangle(M, C2, Apex, C3, RoofColour);
	AddTriangle(M, C3, Apex, C0, RoofColour);
}
}

bool BuildMass(const FMass& Mass, const FGroundSampler& Ground, FMeshData& Out, double& OutHectares, FString& Why)
{
	OutHectares = 0.0;
	if (Mass.Outline.Num() < 3) { Why = TEXT("contour de moins de 3 points"); return false; }
	FBox2D Box(ForceInit);
	for (const FVector2D& P : Mass.Outline) Box += P;
	Box = Box.ExpandBy(MassCell);
	const int32 W = FMath::CeilToInt32(Box.GetSize().X / MassCell) + 1;
	const int32 H = FMath::CeilToInt32(Box.GetSize().Y / MassCell) + 1;
	if (W * H > 400000) { Why = FString::Printf(TEXT("masse trop grande (%d x %d cellules)"), W, H); return false; }
	// Indices de grille absolus : le grain d'une couronne ne depend que de sa position monde.
	const int32 GI0 = FMath::FloorToInt32(Box.Min.X / MassCell), GJ0 = FMath::FloorToInt32(Box.Min.Y / MassCell);
	TArray<int32> Node;
	Node.Init(INDEX_NONE, W * H);
	TArray<double> Weight;
	Weight.Init(0.0, W * H);
	TArray<double> Z;
	Z.Init(0.0, W * H);
	int32 Inside = 0, Wet = 0;
	for (int32 J = 0; J < H; ++J)
	{
		for (int32 I = 0; I < W; ++I)
		{
			const FVector2D P((GI0 + I) * MassCell, (GJ0 + J) * MassCell);
			double G = 0.0, Wz = 0.0;
			if (!Ground(P.X, P.Y, G, Wz)) continue;
			const int32 K = J * W + I;
			Z[K] = G;
			Node[K] = 0;
			if (!PointInPolygon(Mass.Outline, P)) continue;
			if (Wz > G - 50.0) { ++Wet; continue; }
			++Inside;
			Weight[K] = FMath::SmoothStep(0.0, 1.0, DistanceToOutline(Mass.Outline, P) / FMath::Max(Mass.EdgeRamp, 1.0));
		}
	}
	if (Inside < 6) { Why = FString::Printf(TEXT("%d noeud(s) de sol sec dans le contour (%d sous l'eau)"), Inside, Wet); return false; }
	const int32 First = Out.Vertices.Num();
	constexpr double Sink = 300.0;
	for (int32 J = 0; J < H; ++J)
	{
		for (int32 I = 0; I < W; ++I)
		{
			const int32 K = J * W + I;
			if (Node[K] == INDEX_NONE) continue;
			// Seuls les noeuds d'un quad qui touche la masse servent ; les autres sont ecartes plus bas.
			bool bUsed = false;
			for (int32 DJ = -1; DJ <= 1 && !bUsed; ++DJ)
				for (int32 DI = -1; DI <= 1 && !bUsed; ++DI)
				{
					const int32 II = I + DI, JJ = J + DJ;
					bUsed = II >= 0 && JJ >= 0 && II < W && JJ < H && Weight[JJ * W + II] > 0.0;
				}
			if (!bUsed) { Node[K] = INDEX_NONE; continue; }
			const double Grain = CrownGrain(GI0 + I, GJ0 + J);
			const double Wt = Weight[K];
			const double Top = Z[K] + Mass.CanopyHeight * Wt * Grain - Sink * (1.0 - Wt);
			Node[K] = Out.Vertices.Num();
			Out.Vertices.Add(FVector((GI0 + I) * MassCell, (GJ0 + J) * MassCell, Top));
			Out.Normals.Add(FVector::UpVector);
			// Valeur par couronne (+-18 %) : une canopee n'est pas un aplat ; la lisiere, eclairee de cote, un peu plus claire.
			const float V = static_cast<float>((0.82 + 0.4 * (Grain - 0.72)) * (1.0 + 0.15 * (1.0 - Wt)));
			Out.Colours.Add(FLinearColor(Mass.Colour.R * V, Mass.Colour.G * V, Mass.Colour.B * V, 1.0f));
		}
	}
	int32 Quads = 0;
	for (int32 J = 0; J + 1 < H; ++J)
	{
		for (int32 I = 0; I + 1 < W; ++I)
		{
			const int32 A = Node[J * W + I], B = Node[J * W + I + 1], C = Node[(J + 1) * W + I + 1], D = Node[(J + 1) * W + I];
			if (A == INDEX_NONE || B == INDEX_NONE || C == INDEX_NONE || D == INDEX_NONE) continue;
			if (Weight[J * W + I] + Weight[J * W + I + 1] + Weight[(J + 1) * W + I + 1] + Weight[(J + 1) * W + I] <= 0.0) continue;
			// Sens UE (main gauche, Z haut) : face visible du dessus.
			Out.Triangles.Append({ A, C, B, A, D, C });
			++Quads;
		}
	}
	RecomputeNormals(Out, First);
	OutHectares = Inside * MassCell * MassCell / 1.0e8;
	return Quads > 0;
}

bool BuildSilhouette(const FSilhouetteSpec& Spec, const FGroundSampler& Ground, FMeshData& Out, FString& Why)
{
	// Emprise de reference (uu) : tour 5 m, chapelle 13 x 7 m, hameau 40 x 30 m.
	const double Half = (Spec.Kind == ESilhouette::Tower ? 300.0 : Spec.Kind == ESilhouette::Chapel ? 700.0 : 2200.0) * Spec.Scale;
	const FVector2D Fwd(FMath::Cos(FMath::DegreesToRadians(Spec.Yaw)), FMath::Sin(FMath::DegreesToRadians(Spec.Yaw)));
	const FVector2D Right(-Fwd.Y, Fwd.X);
	double MinZ = TNumericLimits<double>::Max(), MaxZ = -TNumericLimits<double>::Max();
	for (int32 K = 0; K < 9; ++K)
	{
		const FVector2D P = Spec.Location + Fwd * Half * ((K % 3) - 1) + Right * Half * ((K / 3) - 1);
		double G = 0.0, Wz = 0.0;
		if (!Ground(P.X, P.Y, G, Wz)) { Why = TEXT("emprise hors du sol releve"); return false; }
		if (Wz > G - 50.0) { Why = TEXT("emprise dans l'eau"); return false; }
		MinZ = FMath::Min(MinZ, G);
		MaxZ = FMath::Max(MaxZ, G);
	}
	const double SlopeDeg = FMath::RadiansToDegrees(FMath::Atan((MaxZ - MinZ) / (2.0 * Half)));
	if (SlopeDeg > Spec.MaxSlopeDeg) { Why = FString::Printf(TEXT("pente %.1f deg > %.1f"), SlopeDeg, Spec.MaxSlopeDeg); return false; }
	const double Sink = (MaxZ - MinZ) + 100.0;
	const FLinearColor Stone(0.36f, 0.33f, 0.28f), StoneDark(0.24f, 0.22f, 0.19f), Tile(0.28f, 0.13f, 0.08f);
	const double S = Spec.Scale;
	const FTransform Base(FRotator(0, Spec.Yaw, 0), FVector(Spec.Location.X, Spec.Location.Y, MinZ));
	const int32 First = Out.Vertices.Num();
	switch (Spec.Kind)
	{
	case ESilhouette::Tower:
		AddTower(Out, Base, 500.0 * S, 1400.0 * S, 300.0 * S, Sink, Stone, StoneDark);
		break;
	case ESilhouette::Chapel:
		AddGabledBox(Out, Base, 1200.0 * S, 650.0 * S, 600.0 * S, 300.0 * S, Sink, Stone, Tile);
		// Clocher-mur sur le pignon ouest : la marque qui distingue une chapelle d'une grange.
		AddGabledBox(Out, FTransform(FRotator(0, Spec.Yaw, 0), Base.TransformPosition(FVector(-650.0 * S, 0, 0))),
			120.0 * S, 300.0 * S, 1250.0 * S, 120.0 * S, Sink, Stone, StoneDark);
		break;
	case ESilhouette::Hamlet:
	{
		// Cinq maisons groupees, pignons alignes sur la pente (cap du plan), pas un semis.
		const FVector Offsets[5] = { {0, 0, 0}, {1100, 650, 0}, {-1200, 500, 0}, {300, -1300, 0}, {-600, 1500, 0} };
		const double Yaws[5] = { 0, 8, -6, 90, 4 };
		for (int32 K = 0; K < 5; ++K)
		{
			const FTransform House(FRotator(0, Spec.Yaw + Yaws[K], 0), Base.TransformPosition(Offsets[K] * S));
			AddGabledBox(Out, House, 850.0 * S, 520.0 * S, 380.0 * S, 230.0 * S, Sink, Stone, Tile);
		}
		break;
	}
	}
	(void)First;
	return true;
}

bool BuildTrace(const FTrace& Trace, const FGroundSampler& Ground, FMeshData& Out, FString& Why)
{
	if (Trace.Points.Num() < 2) { Why = TEXT("moins de 2 points"); return false; }
	constexpr double Step = 1000.0;
	constexpr double Lift = 25.0;
	TArray<FVector2D> P;
	for (int32 K = 0; K + 1 < Trace.Points.Num(); ++K)
	{
		const FVector2D A = Trace.Points[K], B = Trace.Points[K + 1];
		const int32 N = FMath::Max(1, FMath::CeilToInt32(FVector2D::Distance(A, B) / Step));
		for (int32 S = 0; S < N; ++S) P.Add(FMath::Lerp(A, B, double(S) / N));
	}
	P.Add(Trace.Points.Last());
	int32 Segments = 0;
	int32 Prev = INDEX_NONE;
	const double Half = Trace.Width * 0.5;
	for (int32 K = 0; K < P.Num(); ++K)
	{
		const FVector2D Dir = (P[FMath::Min(K + 1, P.Num() - 1)] - P[FMath::Max(K - 1, 0)]).GetSafeNormal();
		const FVector2D N(-Dir.Y, Dir.X);
		const FVector2D L = P[K] + N * Half, R = P[K] - N * Half;
		double GL = 0, WL = 0, GR = 0, WR = 0;
		// Un gue : le ruban s'interrompt sur l'eau et reprend sur l'autre rive.
		if (!Ground(L.X, L.Y, GL, WL) || !Ground(R.X, R.Y, GR, WR) || WL > GL - 30.0 || WR > GR - 30.0) { Prev = INDEX_NONE; continue; }
		const int32 Base = Out.Vertices.Num();
		Out.Vertices.Add(FVector(L.X, L.Y, GL + Lift));
		Out.Vertices.Add(FVector(R.X, R.Y, GR + Lift));
		Out.Normals.Add(FVector::UpVector);
		Out.Normals.Add(FVector::UpVector);
		Out.Colours.Add(Trace.Colour);
		Out.Colours.Add(Trace.Colour);
		if (Prev != INDEX_NONE)
		{
			Out.Triangles.Append({ Prev, Base + 1, Prev + 1, Prev, Base, Base + 1 });
			++Segments;
		}
		Prev = Base;
	}
	if (Segments == 0) { Why = TEXT("aucun segment sur sol sec"); return false; }
	return true;
}

FBuilt Build(const FPlan& Plan, const FGroundSampler& Ground)
{
	FBuilt B;
	for (const FMass& M : Plan.Masses)
	{
		FString Why;
		double Ha = 0.0;
		if (BuildMass(M, Ground, B.Masses, Ha, Why)) { ++B.Report.MassesBuilt; B.Report.MassHectares += Ha; }
		else B.Report.Rejected.Add(FString::Printf(TEXT("%s: %s"), M.Id, *Why));
	}
	for (const FSilhouetteSpec& S : Plan.Silhouettes)
	{
		FString Why;
		if (BuildSilhouette(S, Ground, B.Silhouettes, Why)) ++B.Report.SilhouettesBuilt;
		else B.Report.Rejected.Add(FString::Printf(TEXT("%s: %s"), S.Id, *Why));
	}
	for (const FTrace& T : Plan.Traces)
	{
		FString Why;
		if (BuildTrace(T, Ground, B.Traces, Why)) ++B.Report.TracesBuilt;
		else B.Report.Rejected.Add(FString::Printf(TEXT("%s: %s"), T.Id, *Why));
	}
	return B;
}

const FPlan& CanonicalPlan()
{
	static const FPlan Plan = []
	{
		FPlan P;
#include "WorldTheatre/AnastasisWorldTheatrePlan.inl"
		return P;
	}();
	return Plan;
}
}
