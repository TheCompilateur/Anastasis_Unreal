#include "WorldView/AnastasisRiverbank.h"

#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"

// Helpers dans un espace nomme propre : pas d'espace anonyme, qui entrerait en collision avec
// ses homonymes (SmoothStep...) dans un meme lot unity.
namespace AnastasisRiverbank::Detail
{
double BankSmoothStep(double A, double B, double X)
{
	const double T = FMath::Clamp((X - A) / FMath::Max(B - A, 1.e-9), 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T);
}

uint32 BankHash(int32 X, int32 Y, int32 Seed, uint32 Salt)
{
	uint32 H = static_cast<uint32>(X) * 0x8DA6B343u ^ static_cast<uint32>(Y) * 0xD8163841u
		^ static_cast<uint32>(Seed) * 0xCB1AB31Fu ^ Salt * 0x165667B1u;
	H ^= H >> 15;
	H *= 0x2C1B3C6Du;
	H ^= H >> 12;
	H *= 0x297A2D39u;
	H ^= H >> 15;
	return H;
}

double Lattice(int32 X, int32 Y, int32 Seed, uint32 Salt)
{
	return static_cast<double>(BankHash(X, Y, Seed, Salt) & 0xFFFFFFu) / static_cast<double>(0xFFFFFF);
}

/** Bruit de valeur lisse, deux octaves, [0,1]. */
double Patch(double X, double Y, double Scale, int32 Seed, uint32 Salt)
{
	double Sum = 0.0, Norm = 0.0, Amp = 1.0, S = Scale;
	for (int32 Octave = 0; Octave < 2; ++Octave)
	{
		const double U = X / S, V = Y / S;
		const int32 IX = FMath::FloorToInt(U), IY = FMath::FloorToInt(V);
		const double FX = BankSmoothStep(0.0, 1.0, U - IX), FY = BankSmoothStep(0.0, 1.0, V - IY);
		const uint32 OSalt = Salt + 977u * Octave;
		const double A = FMath::Lerp(Lattice(IX, IY, Seed, OSalt), Lattice(IX + 1, IY, Seed, OSalt), FX);
		const double B = FMath::Lerp(Lattice(IX, IY + 1, Seed, OSalt), Lattice(IX + 1, IY + 1, Seed, OSalt), FX);
		Sum += Amp * FMath::Lerp(A, B, FY);
		Norm += Amp;
		Amp *= 0.5;
		S *= 0.5;
	}
	return Sum / Norm;
}
}

const TCHAR* AnastasisRiverbank::FamilyName(EFamily Family)
{
	switch (Family)
	{
	case EFamily::Reed: return TEXT("Reed");
	case EFamily::Cobble: return TEXT("Cobble");
	case EFamily::Boulder: return TEXT("Boulder");
	default: return TEXT("Unknown");
	}
}

double AnastasisRiverbank::FSpeedField::Sample(double X, double Y) const
{
	if (!IsValid()) return 0.0;
	const double U = (X - X0) / Step, V = (Y - Y0) / Step;
	if (U < 0.0 || V < 0.0 || U > W - 1 || V > H - 1) return 0.0;
	const int32 IX = FMath::Min(FMath::FloorToInt(U), W - 2), IY = FMath::Min(FMath::FloorToInt(V), H - 2);
	const double FX = U - IX, FY = V - IY;
	const int32 I = IY * W + IX;
	const double A = FMath::Lerp(static_cast<double>(Speed[I]), static_cast<double>(Speed[I + 1]), FX);
	const double B = FMath::Lerp(static_cast<double>(Speed[I + W]), static_cast<double>(Speed[I + W + 1]), FX);
	return FMath::Lerp(A, B, FY);
}

void AnastasisRiverbank::BuildSpeedField(const AnastasisDrainage::FNetwork& Network, FSpeedField& Out)
{
	Out = FSpeedField();
	if (Network.GridW < 2 || Network.GridH < 2 || Network.GridStep <= 0.0) return;
	Out.W = Network.GridW;
	Out.H = Network.GridH;
	Out.X0 = Network.GridX0;
	Out.Y0 = Network.GridY0;
	Out.Step = Network.GridStep;
	Out.Speed.Init(0.0f, Out.W * Out.H);
	// Echantillons tous les demi-pas le long de chaque segment : le disque estampe est continu
	// meme quand les points de la polyligne sont espaces de 20 m.
	for (const AnastasisDrainage::FRiver& River : Network.Rivers)
	{
		for (int32 K = 0; K + 1 < River.Points.Num(); ++K)
		{
			const AnastasisDrainage::FRiverPoint& A = River.Points[K];
			const AnastasisDrainage::FRiverPoint& B = River.Points[K + 1];
			const double Len = FVector2D::Distance(FVector2D(A.Location), FVector2D(B.Location));
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(Len / (0.5 * Out.Step)));
			for (int32 S = 0; S <= Steps; ++S)
			{
				const double T = static_cast<double>(S) / Steps;
				const FVector2D P = FMath::Lerp(FVector2D(A.Location), FVector2D(B.Location), T);
				const double Half = 0.5 * FMath::Lerp(A.Width, B.Width, T);
				const double Vel = FMath::Lerp(A.Velocity, B.Velocity, T);
				const double Full = Half + 500.0, Reach = Half + 1500.0;
				const int32 CX = FMath::RoundToInt((P.X - Out.X0) / Out.Step), CY = FMath::RoundToInt((P.Y - Out.Y0) / Out.Step);
				const int32 RC = FMath::CeilToInt(Reach / Out.Step);
				for (int32 Y = FMath::Max(0, CY - RC); Y <= FMath::Min(Out.H - 1, CY + RC); ++Y)
				{
					for (int32 X = FMath::Max(0, CX - RC); X <= FMath::Min(Out.W - 1, CX + RC); ++X)
					{
						const double D = FVector2D::Distance(P, FVector2D(Out.X0 + X * Out.Step, Out.Y0 + Y * Out.Step));
						if (D > Reach) continue;
						const double Fall = 1.0 - Detail::BankSmoothStep(Full, Reach, D);
						float& Cell = Out.Speed[Y * Out.W + X];
						Cell = FMath::Max(Cell, static_cast<float>(Vel * Fall));
					}
				}
			}
		}
	}
}

double AnastasisRiverbank::Calmness(double VelocityMs, const FSettings& Settings)
{
	return 1.0 - Detail::BankSmoothStep(Settings.CalmVelocity, Settings.FastVelocity, VelocityMs);
}

bool AnastasisRiverbank::Build(const FInputs& In, const FSpeedField& Speed, const FSettings& S, FPlan& Out, FString& Error)
{
	Out = FPlan();
	if (!In.SampleHeight || !In.SampleWaterHeight)
	{
		Error = TEXT("echantillonneurs de sol ou d'eau absents");
		return false;
	}
	if (!In.Bounds.bIsValid || In.Bounds.GetArea() <= 0.0)
	{
		Error = TEXT("emprise invalide");
		return false;
	}
	if (!(S.CellUU > 0.0) || !(S.ClumpCellUU > 0.0) || !(S.FastVelocity > S.CalmVelocity) || S.MaxInstances <= 0)
	{
		Error = TEXT("reglages invalides");
		return false;
	}
	const double Start = FPlatformTime::Seconds();
	using namespace AnastasisRiverbank::Detail;

	auto Freeboard = [&In](double X, double Y, double& Ground, double& Free)
	{
		double Water;
		if (!In.SampleHeight(X, Y, Ground) || !In.SampleWaterHeight(X, Y, Water)) return false;
		if (!FMath::IsFinite(Ground) || !FMath::IsFinite(Water)) return false;
		Free = Ground - Water;
		return true;
	};
	auto SlopeAt = [&In](double X, double Y)
	{
		constexpr double H = 100.0;
		double E, Wv, N, Sv;
		if (!In.SampleHeight(X + H, Y, E) || !In.SampleHeight(X - H, Y, Wv) || !In.SampleHeight(X, Y + H, N) || !In.SampleHeight(X, Y - H, Sv))
		{
			return TNumericLimits<double>::Max();
		}
		return FMath::Sqrt(FMath::Square((E - Wv) / (2.0 * H)) + FMath::Square((N - Sv) / (2.0 * H)));
	};
	auto Add = [&Out, &S](EFamily Family, const FVector& At, double Yaw, double Tilt, double TiltYaw, double Size, double Sink, int32 Variant)
	{
		if (Out.Instances.Num() >= S.MaxInstances)
		{
			Out.bTruncated = true;
			return;
		}
		FPlacement& P = Out.Instances.AddDefaulted_GetRef();
		P.Family = Family;
		P.Location = At;
		P.Yaw = Yaw;
		P.Tilt = Tilt;
		P.TiltYaw = TiltYaw;
		P.Size = Size;
		P.Sink = Sink;
		P.Variant = Variant;
		++Out.Counts[static_cast<int32>(Family)];
	};
	const FBox2D& B = In.Bounds;

	// 1. MASSIFS DE ROSEAUX. Une decision par maille de 4,5 m : un point tire au hasard, a la
	// ligne d'eau calme, dans une tache du bruit de massif. Les roselieres reelles sont des
	// taches, pas un lisere continu le long de la rive.
	{
		const double C = S.ClumpCellUU;
		const int32 X0 = FMath::FloorToInt(B.Min.X / C), X1 = FMath::FloorToInt(B.Max.X / C);
		const int32 Y0 = FMath::FloorToInt(B.Min.Y / C), Y1 = FMath::FloorToInt(B.Max.Y / C);
		for (int32 CY = Y0; CY <= Y1 && !Out.bTruncated; ++CY)
		{
			for (int32 CX = X0; CX <= X1 && !Out.bTruncated; ++CX)
			{
				FRandomStream R(static_cast<int32>(BankHash(CX, CY, In.Seed, 11u)));
				const double X = (CX + R.FRand()) * C, Y = (CY + R.FRand()) * C;
				if (!B.IsInside(FVector2D(X, Y))) continue;
				double G, F;
				if (!Freeboard(X, Y, G, F) || F < S.ReedMinFreeboard || F > S.ReedMaxFreeboard) continue;
				const double Calm = Calmness(Speed.Sample(X, Y), S);
				if (Calm < 0.6) continue;
				if (Patch(X, Y, S.PatchScaleUU, In.Seed, 23u) < S.ReedPatchThreshold) continue;
				if (SlopeAt(X, Y) > S.ReedMaxSlope) continue;
				++Out.ReedClumps;
				const int32 Stems = FMath::RoundToInt(FMath::Lerp(6.0, 13.0, Calm) * R.FRandRange(0.8, 1.2));
				for (int32 K = 0; K < Stems; ++K)
				{
					const double A = R.FRandRange(0.0, 2.0 * PI), D = 110.0 * FMath::Sqrt(R.FRand());
					const double PX = X + FMath::Cos(A) * D, PY = Y + FMath::Sin(A) * D;
					double PG, PF;
					// Le pied d'une tige est sur le fond ou sur la vase, jamais au sec ni au large.
					if (!Freeboard(PX, PY, PG, PF) || PF > S.ReedMaxFreeboard + 15.0 || PF < S.ReedMinFreeboard - 30.0) continue;
					Add(EFamily::Reed, FVector(PX, PY, PG), R.FRandRange(0.0, 360.0), R.FRandRange(0.0, 9.0), R.FRandRange(0.0, 360.0),
						R.FRandRange(1.1, 1.9) * FMath::Lerp(0.8, 1.0, Calm), 0.0, 0);
				}
			}
		}
	}

	// 2. CANDIDATS FINS (1,5 m) : galets, blocs.
	{
		const double C = S.CellUU;
		const int32 X0 = FMath::FloorToInt(B.Min.X / C), X1 = FMath::FloorToInt(B.Max.X / C);
		const int32 Y0 = FMath::FloorToInt(B.Min.Y / C), Y1 = FMath::FloorToInt(B.Max.Y / C);
		for (int32 CY = Y0; CY <= Y1 && !Out.bTruncated; ++CY)
		{
			for (int32 CX = X0; CX <= X1 && !Out.bTruncated; ++CX)
			{
				FRandomStream R(static_cast<int32>(BankHash(CX, CY, In.Seed, 31u)));
				const double X = (CX + R.FRand()) * C, Y = (CY + R.FRand()) * C;
				if (!B.IsInside(FVector2D(X, Y))) continue;
				double G, F;
				if (!Freeboard(X, Y, G, F) || F < S.BoulderMinFreeboard || F > S.CobbleMaxFreeboard) continue;
				const double Calm = Calmness(Speed.Sample(X, Y), S), Fast = 1.0 - Calm;
				if (F > S.CobbleMinFreeboard && F < S.CobbleMaxFreeboard) (Calm >= 0.5 ? Out.CalmShore : Out.FastShore) += 1;
				const double Roll = R.FRand();
				if (F > S.CobbleMinFreeboard && F < S.CobbleMaxFreeboard && Fast >= 0.35)
				{
					// Galets : le courant a emporte le fin, il reste la pierre a la ligne d'eau. Par
					// groupes de 3 a 7, de 18 a 55 cm : isoles, clairsemes et petits (v1, v2), ils
					// ne se lisaient pas a 1,7 m.
					if (Roll < 0.6 * Fast)
					{
						const int32 Stones = R.RandRange(3, 7);
						for (int32 K = 0; K < Stones; ++K)
						{
							const double A = R.FRandRange(0.0, 2.0 * PI), D = 70.0 * FMath::Sqrt(R.FRand());
							const double PX = X + FMath::Cos(A) * D, PY = Y + FMath::Sin(A) * D;
							double PG, PF;
							if (!Freeboard(PX, PY, PG, PF) || PF <= S.CobbleMinFreeboard || PF >= S.CobbleMaxFreeboard) continue;
							Add(EFamily::Cobble, FVector(PX, PY, PG), R.FRandRange(0.0, 360.0), R.FRandRange(0.0, 25.0), R.FRandRange(0.0, 360.0),
								R.FRandRange(18.0, 25.0 + 30.0 * Fast), 0.35, R.RandRange(0, 2));
						}
					}
				}
				else if (F >= S.BoulderMinFreeboard && F <= S.BoulderMaxFreeboard && Fast >= 0.6)
				{
					// Blocs : rares, dans le courant vif ; l'eau doit s'y briser, pas les noyer.
					if (Roll < 0.06 * Fast)
					{
						Add(EFamily::Boulder, FVector(X, Y, G), R.FRandRange(0.0, 360.0), R.FRandRange(0.0, 15.0), R.FRandRange(0.0, 360.0),
							R.FRandRange(60.0, 130.0), 0.45, R.RandRange(0, 2));
					}
				}
			}
		}
	}
	Out.MilliSeconds = (FPlatformTime::Seconds() - Start) * 1000.0;
	return true;
}

AnastasisRiverbank::FPaintResult AnastasisRiverbank::PaintBanks(const FSpeedField& Speed, const FSettings& S, AnastasisTerrainSurface::FGeometry& Geo)
{
	FPaintResult Result;
	const int32 N = Geo.Vertices.Num();
	if (N == 0 || Geo.WaterVertices.Num() != N || Geo.Colors.Num() != N) return Result;
	using namespace AnastasisRiverbank::Detail;
	// Vase saturee, plus sombre que WetMud (le sol detrempe du trait de cote) ; gravier de
	// riviere, gris froid : un gravier beige se confondait avec le sol sableux des berges (v1).
	// Lineaires, comme toute la couleur de sommet du sol.
	const FLinearColor Mud(0.060f, 0.052f, 0.040f), Gravel(0.150f, 0.148f, 0.140f);
	const bool bUV0 = Geo.UV0.Num() == N, bUV1 = Geo.UV1.Num() == N, bUV2 = Geo.UV2.Num() == N;
	const double Top = FMath::Max(S.MudBandUU, S.GravelBandUU);
	for (int32 I = 0; I < N; ++I)
	{
		const FVector& V = Geo.Vertices[I];
		const double Free = V.Z - Geo.WaterVertices[I].Z;
		// Sec seulement : le fond immerge est deja peint (gravier / vase) par le drainage.
		// Au-dela de Top, l'eau du sommet est une sentinelle (sol - 1 m) : pas une rive.
		if (Free < 0.0 || Free >= Top) continue;
		const double Calm = Calmness(Speed.Sample(V.X, V.Y), S);
		const double MudW = (1.0 - BankSmoothStep(0.0, S.MudBandUU, Free)) * Calm;
		const double GravelW = (1.0 - BankSmoothStep(0.0, S.GravelBandUU, Free)) * (1.0 - Calm);
		FLinearColor& C = Geo.Colors[I];
		const float A = C.A;
		if (MudW > 0.02)
		{
			C = FMath::Lerp(C, Mud, static_cast<float>(0.75 * MudW));
			// Lustre : M_AnastasisGround abaisse la rugosite au-dessus de DampStart (0.60).
			if (bUV1) Geo.UV1[I].Y = FMath::Max(Geo.UV1[I].Y, 0.55 + 0.42 * MudW);
			++Result.MudVertices;
		}
		if (GravelW > 0.02)
		{
			C = FMath::Lerp(C, Gravel, static_cast<float>(0.85 * GravelW));
			// Grain de pierre : poids de roche du materiau de sol.
			if (bUV0) Geo.UV0[I].X = FMath::Max(Geo.UV0[I].X, 0.9 * GravelW);
			if (bUV2) Geo.UV2[I].Y = FMath::Max(Geo.UV2[I].Y, GravelW);
			++Result.GravelVertices;
		}
		C.A = A;
	}
	return Result;
}
