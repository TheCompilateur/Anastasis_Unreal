#include "WorldView/AnastasisTectonics.h"

namespace
{
// Noms propres a ce fichier : en build unity, il partage l'unite de compilation avec
// d'autres fichiers de WorldView dont le namespace anonyme definit deja SmoothStep & co.
double TectSmooth(double Edge0, double Edge1, double X)
{
	const double T = FMath::Clamp((X - Edge0) / FMath::Max(Edge1 - Edge0, 1.e-9), 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T);
}

double TectSq(double X) { return X * X; }

uint32 TectMix(uint32 H)
{
	H ^= H >> 16;
	H *= 0x7feb352dU;
	H ^= H >> 15;
	H *= 0x846ca68bU;
	H ^= H >> 16;
	return H;
}

/** Valeur aleatoire dans [0,1) attachee a un sommet de grille, entiere donc portable. */
double TectCorner(int32 X, int32 Y, uint32 Seed)
{
	const uint32 H = TectMix(static_cast<uint32>(X) * 0x9E3779B1U ^ TectMix(static_cast<uint32>(Y) * 0x85EBCA6BU + Seed));
	return static_cast<double>(H >> 8) * (1.0 / 16777216.0);
}

struct FTectNoise
{
	double V = 0.0;   // [-1, 1]
	double Dx = 0.0;  // derivee par rapport a X
	double Dy = 0.0;
};

/** Bruit de valeur a interpolation quintique, avec ses derivees analytiques. */
FTectNoise TectNoise(double X, double Y, uint32 Seed)
{
	const double FX = FMath::FloorToDouble(X);
	const double FY = FMath::FloorToDouble(Y);
	const int32 IX = static_cast<int32>(FX), IY = static_cast<int32>(FY);
	const double TX = X - FX, TY = Y - FY;
	const double UX = TX * TX * TX * (TX * (TX * 6.0 - 15.0) + 10.0);
	const double UY = TY * TY * TY * (TY * (TY * 6.0 - 15.0) + 10.0);
	const double DUX = 30.0 * TX * TX * (TX - 1.0) * (TX - 1.0);
	const double DUY = 30.0 * TY * TY * (TY - 1.0) * (TY - 1.0);
	const double A = TectCorner(IX, IY, Seed), B = TectCorner(IX + 1, IY, Seed);
	const double C = TectCorner(IX, IY + 1, Seed), D = TectCorner(IX + 1, IY + 1, Seed);
	const double K0 = A, K1 = B - A, K2 = C - A, K3 = A - B - C + D;
	FTectNoise Out;
	Out.V = 2.0 * (K0 + K1 * UX + K2 * UY + K3 * UX * UY) - 1.0;
	Out.Dx = 2.0 * DUX * (K1 + K3 * UY);
	Out.Dy = 2.0 * DUY * (K2 + K3 * UX);
	return Out;
}

/** Bruit doux, somme de peu d'octaves, normalise dans [0,1] autour de 0,5. */
double TectSoft(double X, double Y, uint32 Seed, int32 Octaves)
{
	double Sum = 0.0, Amp = 0.5, Norm = 0.0;
	for (int32 I = 0; I < Octaves; ++I)
	{
		Sum += Amp * (0.5 + 0.5 * TectNoise(X, Y, Seed + static_cast<uint32>(I) * 977U).V);
		Norm += Amp;
		X = X * 2.03 + 17.1;
		Y = Y * 2.03 - 9.7;
		Amp *= 0.5;
	}
	return Sum / Norm;
}

/**
 * Multifractale a cretes ("ridged") : 1 - |bruit|, au carre, chaque octave ponderee par la
 * precedente -- les cretes se prolongent, les vallees restent lisses. Dans [0,1].
 */
double TectRidged(double X, double Y, uint32 Seed, int32 Octaves)
{
	double Sum = 0.0, Amp = 0.5, Norm = 0.0, Weight = 1.0;
	for (int32 I = 0; I < Octaves; ++I)
	{
		const double N = TectNoise(X, Y, Seed + static_cast<uint32>(I) * 1291U).V;
		double R = 1.0 - FMath::Abs(N);
		R = R * R * Weight;
		Weight = FMath::Clamp(R * 2.0, 0.0, 1.0);
		Sum += R * Amp;
		Norm += Amp;
		const double NX = 0.8 * X - 0.6 * Y, NY = 0.6 * X + 0.8 * Y;
		X = NX * 2.0 + 3.7;
		Y = NY * 2.0 - 5.3;
		Amp *= 0.5;
	}
	return Sum / Norm;
}

/**
 * Bruit "erode" : chaque octave est attenuee par la pente cumulee des precedentes, comme si
 * l'eau avait deja lissé les flancs raides et laisse les cretes. Dans [-1,1], centre sur 0.
 */
double TectEroded(double X, double Y, uint32 Seed, int32 Octaves)
{
	double Sum = 0.0, Amp = 0.5, Norm = 0.0, Dx = 0.0, Dy = 0.0;
	for (int32 I = 0; I < Octaves; ++I)
	{
		const FTectNoise N = TectNoise(X, Y, Seed + static_cast<uint32>(I) * 1637U);
		Dx += N.Dx;
		Dy += N.Dy;
		Sum += Amp * N.V / (1.0 + Dx * Dx + Dy * Dy);
		Norm += Amp;
		const double NX = 0.8 * X - 0.6 * Y, NY = 0.6 * X + 0.8 * Y;
		X = NX * 2.0 + 11.3;
		Y = NY * 2.0 - 7.9;
		Amp *= 0.5;
	}
	return Sum / Norm;
}
}

AnastasisTectonics::FTectonicFrame AnastasisTectonics::MakeFrame(const FVector2D& Outlet, uint32 Seed)
{
	FTectonicFrame Frame;
	Frame.Seed = Seed;
	if (Outlet.SizeSquared() > 1.e-6)
	{
		Frame.Down = Outlet.GetSafeNormal();
	}
	else
	{
		// Pas d'eau au bord : la graine choisit le cote des basses terres, par pas de 45 degres
		// pour que la chaine ne soit jamais exactement parallele aux bords de la carte.
		const double Angle = (static_cast<double>(TectMix(Seed ^ 0xC0FFEE11U) & 0xFFFFU) / 65536.0) * UE_DOUBLE_TWO_PI;
		Frame.Down = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
	}
	return Frame;
}

void AnastasisTectonics::Evaluate(const FTectonicFrame& F, double XKm, double YKm, FBreakdown& Out, double MountainSaddles)
{
	Out = FBreakdown{};
	const uint32 S = F.Seed;
	const FVector2D P(XKm, YKm);

	// Gauchissement de grande longueur d'onde : aucun element n'est une droite parfaite, et les
	// chaines serpentent de quelques kilometres autour de leur axe.
	const FVector2D Warp(
		(TectSoft(XKm / 24.0, YKm / 24.0, S + 11U, 3) - 0.5) * 6.0,
		(TectSoft(XKm / 24.0 + 31.7, YKm / 24.0 - 12.3, S + 13U, 3) - 0.5) * 6.0);
	const FVector2D Pw = P + Warp;
	const FVector2D Strike = F.Strike();
	const double U = -FVector2D::DotProduct(Pw, F.Down);
	const double V = FVector2D::DotProduct(Pw, Strike);
	Out.U = U;
	Out.V = V;

	// 1. Piedmont puis plateau continental : monte vers l'interieur.
	Out.Ramp = 220.0 * TectSmooth(2.0, 14.0, U) + 640.0 * TectSmooth(16.0, 46.0, U);

	// 2. Escarpement de faille normale : l'interieur est plus haut que les basses terres.
	const double ScarpU = -7.0 + 0.8 * (TectSoft(V / 18.0, 0.4, S + 21U, 2) - 0.5);
	const double ScarpGate = 0.55 + 0.45 * TectSoft(V / 7.0, 2.1, S + 23U, 2);
	Out.Scarp = 85.0 * ScarpGate * TectSmooth(ScarpU - 0.3, ScarpU + 0.3, U);

	// 3. Ceinture de plis : crêtes paralleles a la chaine, front raide (0.78 -> 1.0 de la phase),
	//    revers doux ; chaque crete est coupee de seuils (les cluses ou passent les rivieres).
	{
		constexpr double Spacing = 2.3;
		const double Phase = U / Spacing + 1.3 * (TectSoft(V / 7.0, U / 9.0, S + 31U, 2) - 0.5);
		const double Index = FMath::FloorToDouble(Phase);
		const double Fr = Phase - Index;
		const double Profile = TectSmooth(0.0, 0.78, Fr) * (1.0 - TectSmooth(0.78, 1.0, Fr));
		const double Gap = TectSmooth(0.28, 0.52, 0.5 + 0.5 * TectNoise(V / 3.5 + Index * 7.13, Index * 3.7, S + 37U).V);
		const double Amp = 380.0 * TectSmooth(2.5, 8.0, U) * (1.0 - TectSmooth(13.0, 18.0, U))
			* (0.45 + 0.55 * TectSoft(V / 9.0, Index * 1.7, S + 41U, 2));
		Out.Folds = Amp * Profile * Gap;
		// Texture de pli : nervures courtes le long de la chaine, sous la ceinture.
		Out.Folds += 38.0 * TectSmooth(2.0, 7.0, U) * (1.0 - TectSmooth(14.0, 19.0, U))
			* TectRidged(V / 1.6, U / 0.55, S + 47U, 3);
	}

	// 4. Faille decrochante rectiligne : tranchee etroite et crêtes de blocage decalees.
	{
		const double FaultU = 8.0 + 0.7 * (TectSoft(V / 30.0, 0.9, S + 51U, 2) - 0.5);
		const double Trench = 100.0 * FMath::Exp(-TectSq((U - FaultU) / 0.32));
		const double Gate = TectSmooth(0.45, 0.62, 0.5 + 0.5 * TectNoise(V / 2.6, 3.3, S + 53U).V);
		const double Shutter = 48.0 * Gate * FMath::Exp(-TectSq((U - FaultU - 0.55) / 0.19));
		Out.Fault = -Trench + Shutter;
	}

	// 5. Chaine principale : axe qui serpente, front raide cote carte, revers long. Le massif est
	//    sculpte par le drainage (bruit erode : cretes, eperons, vallees en V), traverse de cols
	//    tous les 7 a 13 km, et double d'un contrefort parallele moitie moins haut.
	{
		const double U0 = 12.5 + 3.0 * (TectSoft(V / 26.0, 0.3, S + 61U, 3) - 0.5) * 2.0;
		const double T = (U - U0) / 7.0;
		const double Envelope = FMath::Exp(-TectSq(T / (T < 0.0 ? 0.55 : 1.1)));
		const double Crest = 2000.0 + 1400.0 * TectSoft(V / 11.0, 1.7, S + 63U, 3);
		const double Ridged = TectRidged(V / 6.0, U / 2.2, S + 67U, 6);
		const double Drainage = TectEroded(Pw.X / 2.6, Pw.Y / 2.6, S + 65U, 6);
		const double Shape = FMath::Max(0.12, 0.30 + 0.55 * Ridged + 0.9 * (Drainage + 0.1));
		const double Peaks = TectRidged(Pw.X / 3.2, Pw.Y / 3.2, S + 69U, 5);
		// Cols : la ou le bruit transverse s'annule, la crete s'abaisse jusqu'a 45 %.
		const double Pass = TectNoise(V / 6.5, U / 14.0, S + 57U).V;
		const double Carve = 1.0 - 0.55 * (1.0 - TectSmooth(0.0, 0.24, FMath::Abs(Pass)));
		double Main = Crest * Envelope * Shape * Carve + 540.0 * FMath::Pow(Envelope, 1.3) * FMath::Pow(Peaks, 2.2) * Carve;
		const double Spur = FMath::Exp(-TectSq((U - U0 - 6.5) / 2.8));
		Main += 0.55 * Crest * Spur * Shape * (0.7 + 0.3 * TectSoft(V / 9.0, 4.4, S + 59U, 2));
		// Aretes vives : des cretes courtes de 1,4 km qui dechirent le haut du massif sans toucher
		// son pied (un massif n'est pas une colline a la loupe).
		Main += 130.0 * FMath::Pow(Envelope, 0.8) * TectSmooth(900.0, 1900.0, Main) * TectRidged(Pw.X / 1.4, Pw.Y / 1.4, S + 55U, 4);
		// Selles larges le long de la chaine : les massifs se distinguent sans ajouter de pics.
		// La seconde chaine demeure visible entre eux et la vallee habitee ne change pas.
		const double SaddleField = TectNoise(V / 8.0, 6.3, S + 93U).V;
		const double Massif = TectSmooth(-0.25, 0.25, SaddleField);
		Main *= 1.0 - 0.58 * FMath::Clamp(MountainSaddles, 0.0, 1.0) * (1.0 - Massif);
		Out.Range = Main;
	}

	// 6. Seconde chaine, au fond du plateau : plus haute, plus large, symetrique.
	{
		const double U1 = 40.0 + 5.0 * (TectSoft(V / 40.0, 0.7, S + 71U, 2) - 0.5) * 2.0;
		const double T = (U - U1) / 9.0;
		const double Envelope = FMath::Exp(-TectSq(T));
		const double Crest = 3000.0 + 1000.0 * TectSoft(V / 20.0, 2.9, S + 73U, 3);
		const double Ridged = TectRidged(V / 8.0, U / 3.6, S + 77U, 6);
		const double Peaks = TectRidged(Pw.X / 5.0, Pw.Y / 5.0, S + 79U, 5);
		Out.Backdrop = Crest * Envelope * (0.5 + 0.85 * Ridged) + 780.0 * FMath::Pow(Envelope, 1.3) * FMath::Pow(Peaks, 2.2);
	}

	// 7. Cote basses terres : une chaine exterieure basse a 16-34 km qui ferme l'horizon de ce
	//    cote, et un moutonnement de collines et de cretes de 3 a 16 km qui donne une ligne de
	//    crete au premier plan. Allonges le long de la chaine et eroded, pas un champ de bosses.
	{
		const double Outward = TectSmooth(14.0, 32.0, -U);
		const double Shape = FMath::Max(0.15, 0.35 + 0.6 * TectRidged(V / 12.0, U / 4.5, S + 83U, 5) + 0.6 * (TectEroded(Pw.X / 4.0, Pw.Y / 4.0, S + 85U, 5) + 0.1));
		Out.Outer = 1700.0 * Outward * Shape;
		const double Rolling = TectSmooth(2.5, 9.0, P.Size()) * (1.0 - 0.5 * TectSmooth(20.0, 40.0, P.Size()));
		Out.Outer += 320.0 * Rolling * (0.25 + 0.9 * FMath::Max(0.0, TectEroded(Pw.X / 3.0, Pw.Y / 3.0, S + 87U, 5) + 0.12) * 2.0);
	}

	// 8. Cuvette generale : aucune direction ne s'ouvre sur le vide.
	Out.Bowl = 650.0 * TectSmooth(24.0, 64.0, P.Size()) * (0.6 + 0.8 * TectSoft(XKm / 15.0, YKm / 15.0, S + 89U, 3));

	const double Sum = Out.Ramp + Out.Folds + Out.Scarp + Out.Fault + Out.Range + Out.Backdrop + Out.Outer + Out.Bowl;
	// Saturation douce : les sommets les plus hauts s'aplatissent vers MaxHeightM sans plafond net.
	Out.Total = Sum > 0.0 ? MaxHeightM * FMath::Tanh(Sum / MaxHeightM) : Sum;
}

double AnastasisTectonics::HeightM(const FTectonicFrame& Frame, double XKm, double YKm, double MountainSaddles)
{
	FBreakdown B;
	Evaluate(Frame, XKm, YKm, B, MountainSaddles);
	return B.Total;
}

AnastasisTectonics::FSurface AnastasisTectonics::SurfaceAt(const FTectonicFrame& F, double XKm, double YKm, double H, double SlopeDeg)
{
	// Deux bruits independants : la limite des neiges et celle des arbres ne suivent pas une
	// isohypse, elles montent sur les versants au soleil et descendent dans les couloirs.
	const double NSnow = TectSoft(XKm / 2.2, YKm / 2.2, F.Seed + 101U, 3);
	const double NTree = TectSoft(XKm / 3.1 + 40.0, YKm / 3.1 - 40.0, F.Seed + 103U, 3);
	const double NRock = TectSoft(XKm / 1.1, YKm / 1.1, F.Seed + 107U, 3);

	FSurface Out;
	const double Cliff = TectSmooth(33.0, 52.0, SlopeDeg + 9.0 * (NRock - 0.5));
	const double AltRock = TectSmooth(2500.0, 3100.0, H + 340.0 * (NSnow - 0.5));
	Out.Rock = FMath::Max(Cliff * TectSmooth(TreeLineLowM - 40.0, TreeLineLowM + 400.0, H), AltRock);

	const double SnowLine = SnowLineM + 360.0 * (NSnow - 0.5);
	const double SnowAlt = TectSmooth(SnowLine - 300.0, SnowLine + 450.0, H);
	const double SnowSlope = 1.0 - TectSmooth(46.0, 62.0, SlopeDeg);
	// Une face verticale ne retient pas la neige, mais les couloirs d'altitude la gardent.
	Out.Snow = SnowAlt * FMath::Max(SnowSlope, 0.35 * TectSmooth(SnowLine + 200.0, SnowLine + 600.0, H));

	const double TreeHigh = TreeLineHighM + 260.0 * (NTree - 0.5);
	Out.Forest = TectSmooth(TreeLineLowM, TreeLineLowM + 450.0, H) * (1.0 - TectSmooth(TreeHigh - 330.0, TreeHigh, H))
		* (1.0 - Cliff);
	Out.Alpine = TectSmooth(TreeHigh - 330.0, TreeHigh, H) * (1.0 - TectSmooth(2500.0, 3000.0, H));
	// Variation de valeur : bruit large (1,1 km) et fin (0,35 km), pour que ni la neige ni la roche ne soient des aplats.
	Out.Tone = 0.86 + 0.28 * (0.6 * NRock + 0.4 * TectSoft(XKm / 0.35 + 7.0, YKm / 0.35 - 3.0, F.Seed + 109U, 2));
	return Out;
}
