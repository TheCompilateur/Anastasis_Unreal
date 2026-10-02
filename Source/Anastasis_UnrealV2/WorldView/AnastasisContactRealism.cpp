#include "WorldView/AnastasisContactRealism.h"

#include "HAL/PlatformTime.h"

// Helpers dans un espace nomme propre : pas d'espace anonyme, qui entrerait en collision avec
// ses homonymes dans un meme lot unity (cf. AnastasisRiverbank::Detail).
namespace AnastasisContactRealism::Detail
{
double ContactSmooth(double A, double B, double X)
{
	const double T = FMath::Clamp((X - A) / FMath::Max(B - A, 1.e-9), 0.0, 1.0);
	return T * T * (3.0 - 2.0 * T);
}

uint32 ContactHash(int32 X, int32 Y, int32 Seed, uint32 Salt)
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

double ContactUnit(int32 X, int32 Y, int32 Seed, uint32 Salt)
{
	return static_cast<double>(ContactHash(X, Y, Seed, Salt) & 0xFFFFFFu) / static_cast<double>(0xFFFFFF);
}

/** Bruit de valeur lisse a deux octaves, [0,1]. Echelle en uu. */
double ContactPatch(double X, double Y, double Scale, int32 Seed, uint32 Salt)
{
	double Sum = 0.0, Norm = 0.0, Amp = 1.0, S = Scale;
	for (int32 Octave = 0; Octave < 2; ++Octave)
	{
		const double U = X / S, V = Y / S;
		const int32 IX = FMath::FloorToInt(U), IY = FMath::FloorToInt(V);
		const double FX = ContactSmooth(0.0, 1.0, U - IX), FY = ContactSmooth(0.0, 1.0, V - IY);
		const uint32 OSalt = Salt + 977u * Octave;
		const double A = FMath::Lerp(ContactUnit(IX, IY, Seed, OSalt), ContactUnit(IX + 1, IY, Seed, OSalt), FX);
		const double B = FMath::Lerp(ContactUnit(IX, IY + 1, Seed, OSalt), ContactUnit(IX + 1, IY + 1, Seed, OSalt), FX);
		Sum += Amp * FMath::Lerp(A, B, FY);
		Norm += Amp;
		Amp *= 0.5;
		S *= 0.5;
	}
	return Sum / Norm;
}

struct FContactGrid
{
	int32 W = 0, H = 0;
	double X0 = 0.0, Y0 = 0.0, Step = 1.0;
	TArray<float> Ground;
	/** Niveau de la nappe la plus proche (propage avec la distance). */
	TArray<float> NearWater;
	/** Distance au plus proche sol mouille (uu). */
	TArray<float> Dist;
	TArray<uint8> Valid;
	TArray<uint8> Wet;

	int32 Index(int32 I, int32 J) const { return J * W + I; }
	bool InRange(int32 I, int32 J) const { return I >= 0 && J >= 0 && I < W && J < H; }
	bool Locate(double X, double Y, int32& I, int32& J) const
	{
		I = FMath::RoundToInt((X - X0) / Step);
		J = FMath::RoundToInt((Y - Y0) / Step);
		return InRange(I, J);
	}
	bool GroundAt(int32 I, int32 J, double& Z) const
	{
		if (!InRange(I, J) || !Valid[Index(I, J)]) return false;
		Z = Ground[Index(I, J)];
		return true;
	}
	/** dZ/dXY et direction de la pente descendante (radians, repere monde). */
	double SlopeAt(int32 I, int32 J, double& DownRad) const
	{
		double E, Wv, N, S, C;
		if (!GroundAt(I, J, C)) { DownRad = 0.0; return TNumericLimits<double>::Max(); }
		if (!GroundAt(I + 1, J, E)) E = C;
		if (!GroundAt(I - 1, J, Wv)) Wv = C;
		if (!GroundAt(I, J + 1, N)) N = C;
		if (!GroundAt(I, J - 1, S)) S = C;
		const double GX = (E - Wv) / (2.0 * Step), GY = (N - S) / (2.0 * Step);
		DownRad = FMath::Atan2(-GY, -GX);
		return FMath::Sqrt(GX * GX + GY * GY);
	}
	/** Direction qui s'eloigne de l'eau (gradient de la distance), radians ; faux = plat. */
	bool AwayFromWater(int32 I, int32 J, double& AwayRad) const
	{
		auto D = [this, I, J](int32 DI, int32 DJ)
		{
			const int32 A = FMath::Clamp(I + DI, 0, W - 1), B = FMath::Clamp(J + DJ, 0, H - 1);
			return static_cast<double>(Dist[Index(A, B)]);
		};
		const double GX = D(1, 0) - D(-1, 0), GY = D(0, 1) - D(0, -1);
		if (FMath::Abs(GX) + FMath::Abs(GY) < 1.0e-3) return false;
		AwayRad = FMath::Atan2(GY, GX);
		return true;
	}
};

struct FReedBin
{
	int32 Count = 0;
	double SumX = 0.0, SumY = 0.0;
};

int64 BinKey(int32 X, int32 Y) { return (static_cast<int64>(X) << 32) ^ static_cast<uint32>(Y); }
}

const TCHAR* AnastasisContactRealism::ShoreName(EShore Shore)
{
	switch (Shore)
	{
	case EShore::Dry: return TEXT("SHORE_DRY");
	case EShore::Damp: return TEXT("SHORE_DAMP");
	case EShore::Muddy: return TEXT("SHORE_MUDDY");
	case EShore::Reed: return TEXT("SHORE_REED");
	case EShore::Stony: return TEXT("SHORE_STONY");
	default: return TEXT("SHORE_NONE");
	}
}

const TCHAR* AnastasisContactRealism::DecalName(EDecal Decal)
{
	switch (Decal)
	{
	case EDecal::WetBand: return TEXT("WetBand");
	case EDecal::Mud: return TEXT("Mud");
	case EDecal::StoneWet: return TEXT("StoneWet");
	case EDecal::ReedBed: return TEXT("ReedBed");
	case EDecal::Litter: return TEXT("Litter");
	case EDecal::ContactDark: return TEXT("ContactDark");
	case EDecal::RockDirt: return TEXT("RockDirt");
	case EDecal::Deposit: return TEXT("Deposit");
	case EDecal::Depression: return TEXT("Depression");
	case EDecal::Streak: return TEXT("Streak");
	case EDecal::Halo: return TEXT("Halo");
	default: return TEXT("Unknown");
	}
}

const TCHAR* AnastasisContactRealism::AnchorName(EAnchor Anchor)
{
	switch (Anchor)
	{
	case EAnchor::Tree: return TEXT("Tree");
	case EAnchor::Sapling: return TEXT("Sapling");
	case EAnchor::Stump: return TEXT("Stump");
	case EAnchor::Lying: return TEXT("Lying");
	case EAnchor::Roots: return TEXT("Roots");
	case EAnchor::Rock: return TEXT("Rock");
	case EAnchor::Bush: return TEXT("Bush");
	case EAnchor::Reed: return TEXT("Reed");
	default: return TEXT("Unknown");
	}
}

AnastasisContactRealism::EShore AnastasisContactRealism::ClassifyShore(double DistUU, double Freeboard, double Slope,
	double Calm, double Patch, int32 ReedStems, double BandUU)
{
	if (DistUU > BandUU * 3.5) return EShore::None;
	if (DistUU > BandUU * 1.3) return EShore::Dry;
	// Les roseaux sont un fait : ils existent ou non. Le sol sous eux est organique et sombre.
	if (ReedStems >= 2 && Calm > 0.3) return EShore::Reed;
	// Eau vive, talus raide ou banc de gravier : mineral.
	if (Slope > 0.28 || Calm < 0.4 || (Patch > 0.78 && Slope > 0.08)) return EShore::Stony;
	// Replat d'eau calme, bas sur l'eau : vase, par plaques.
	if (Calm > 0.6 && Slope < 0.14 && Freeboard < 130.0 && Patch > 0.32) return EShore::Muddy;
	// Tronçons de berge dure : pas de bande humide, la terre touche l'eau nette.
	if (Patch < 0.2) return EShore::Dry;
	return EShore::Damp;
}

bool AnastasisContactRealism::Build(const FInputs& In, const FSettings& S, FPlan& Out, FString& Error)
{
	using namespace Detail;
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
	if (!(S.GridUU > 10.0) || !(S.ShoreCellUU > 20.0) || !(S.ShoreBandUU > 100.0) || !(S.MeadowCellUU > 100.0)
		|| S.MaxDecals <= 0 || S.MaxPebbles < 0 || !(S.PebbleMaxUU >= S.PebbleMinUU))
	{
		Error = TEXT("reglages invalides");
		return false;
	}
	for (const FAnchor& A : In.Anchors)
	{
		if (!FMath::IsFinite(A.Location.X) || !FMath::IsFinite(A.Location.Y) || !FMath::IsFinite(A.Location.Z))
		{
			Error = TEXT("ancre invalide : position non finie");
			return false;
		}
		if (!FMath::IsFinite(A.Radius) || !FMath::IsFinite(A.Height) || !FMath::IsFinite(A.YawDegrees) || A.Radius < 0.0 || A.Height < 0.0)
		{
			Error = TEXT("ancre invalide : taille ou lacet non finis");
			return false;
		}
	}
	const double Start = FPlatformTime::Seconds();
	const int32 Seed = In.Seed;

	// ---- Grille : sol, eau, distance a l'eau (chamfer 8 voisins), niveau de l'eau voisine.
	FContactGrid G;
	G.Step = S.GridUU;
	G.X0 = In.Bounds.Min.X;
	G.Y0 = In.Bounds.Min.Y;
	G.W = FMath::Max(2, FMath::CeilToInt(In.Bounds.GetSize().X / G.Step) + 1);
	G.H = FMath::Max(2, FMath::CeilToInt(In.Bounds.GetSize().Y / G.Step) + 1);
	if (static_cast<int64>(G.W) * G.H > 6000000)
	{
		Error = TEXT("grille trop grande");
		return false;
	}
	const int32 N = G.W * G.H;
	G.Ground.Init(0.f, N);
	G.NearWater.Init(-1.0e9f, N);
	G.Dist.Init(1.0e9f, N);
	G.Valid.Init(0, N);
	G.Wet.Init(0, N);
	for (int32 J = 0; J < G.H; ++J)
	{
		for (int32 I = 0; I < G.W; ++I)
		{
			const double X = G.X0 + I * G.Step, Y = G.Y0 + J * G.Step;
			double Z = 0.0, Wz = 0.0;
			if (!In.SampleHeight(X, Y, Z) || !FMath::IsFinite(Z)) continue;
			const int32 K = G.Index(I, J);
			G.Valid[K] = 1;
			G.Ground[K] = static_cast<float>(Z);
			if (In.SampleWaterHeight(X, Y, Wz) && FMath::IsFinite(Wz) && Z <= Wz + 3.0)
			{
				G.Wet[K] = 1;
				G.Dist[K] = 0.f;
				G.NearWater[K] = static_cast<float>(Wz);
				++Out.WetCells;
			}
		}
	}
	auto Relax = [&G](int32 I, int32 J, int32 DI, int32 DJ, float Cost)
	{
		const int32 A = I + DI, B = J + DJ;
		if (!G.InRange(A, B)) return;
		const int32 K = G.Index(I, J), L = G.Index(A, B);
		if (G.Dist[L] + Cost < G.Dist[K])
		{
			G.Dist[K] = G.Dist[L] + Cost;
			G.NearWater[K] = G.NearWater[L];
		}
	};
	const float Orth = static_cast<float>(G.Step), Diag = static_cast<float>(G.Step * UE_SQRT_2);
	for (int32 J = 0; J < G.H; ++J)
		for (int32 I = 0; I < G.W; ++I)
		{
			Relax(I, J, -1, 0, Orth); Relax(I, J, 0, -1, Orth); Relax(I, J, -1, -1, Diag); Relax(I, J, 1, -1, Diag);
		}
	for (int32 J = G.H - 1; J >= 0; --J)
		for (int32 I = G.W - 1; I >= 0; --I)
		{
			Relax(I, J, 1, 0, Orth); Relax(I, J, 0, 1, Orth); Relax(I, J, 1, 1, Diag); Relax(I, J, -1, 1, Diag);
		}

	// ---- Roseaux reels, en cases : l'etat REED vient des tiges posees, pas d'un bruit.
	TMap<int64, FReedBin> Reeds;
	for (const FAnchor& A : In.Anchors)
	{
		if (A.Kind != EAnchor::Reed) continue;
		FReedBin& Bin = Reeds.FindOrAdd(BinKey(FMath::FloorToInt(A.Location.X / S.ReedCellUU), FMath::FloorToInt(A.Location.Y / S.ReedCellUU)));
		++Bin.Count;
		Bin.SumX += A.Location.X;
		Bin.SumY += A.Location.Y;
		++Out.AnchorCounts[static_cast<int32>(EAnchor::Reed)];
	}
	auto ReedStemsAround = [&](double X, double Y)
	{
		const int32 BX = FMath::FloorToInt(X / S.ReedCellUU), BY = FMath::FloorToInt(Y / S.ReedCellUU);
		int32 Sum = 0;
		for (int32 DY = -1; DY <= 1; ++DY)
			for (int32 DX = -1; DX <= 1; ++DX)
				if (const FReedBin* Bin = Reeds.Find(BinKey(BX + DX, BY + DY))) Sum += Bin->Count;
		return Sum;
	};

	// Quatre classes, chacune son plafond : sans cela, la passe la plus gourmande (83 000 ancres au
	// premier essai) mange le budget et les roseaux n'en recoivent plus.
	enum : int32 { ClassShore = 0, ClassAnchor = 1, ClassReed = 2, ClassMeadow = 3, ClassCount = 4 };
	const double ClassShare[ClassCount] = {0.40, 0.36, 0.12, 0.12};
	int32 ClassCap[ClassCount];
	int32 ClassUsed[ClassCount] = {};
	for (int32 C = 0; C < ClassCount; ++C) ClassCap[C] = FMath::Max(1, FMath::RoundToInt(S.MaxDecals * ClassShare[C]));
	auto Emit = [&](const FVector& At, double Yaw, double Long, double Wide, EDecal Kind, EShore Shore, int32 Class)
	{
		if (ClassUsed[Class] >= ClassCap[Class] || Out.Decals.Num() >= S.MaxDecals)
		{
			Out.bTruncated = true;
			return false;
		}
		++ClassUsed[Class];
		FDecal D;
		D.Location = At;
		D.YawDegrees = FMath::Fmod(Yaw, 360.0);
		D.HalfLong = FMath::Clamp(Long, 20.0, 1500.0);
		D.HalfWide = FMath::Clamp(Wide, 20.0, 1500.0);
		D.Kind = Kind;
		D.Shore = Shore;
		Out.Decals.Add(D);
		++Out.DecalCounts[static_cast<int32>(Kind)];
		return true;
	};
	auto SnapToGround = [&](FVector& At, double& Slope, double& DownRad)
	{
		double Z;
		if (!In.SampleHeight(At.X, At.Y, Z) || !FMath::IsFinite(Z)) return false;
		At.Z = Z;
		int32 I, J;
		if (!G.Locate(At.X, At.Y, I, J)) { Slope = 0.0; DownRad = 0.0; return true; }
		Slope = G.SlopeAt(I, J, DownRad);
		return true;
	};
	auto AddPebble = [&](const FVector& At, double Yaw, double Diameter, double Sink, int32 Variant)
	{
		if (Out.Pebbles.Num() >= S.MaxPebbles) return;
		FPebble P;
		P.Location = At;
		P.YawDegrees = Yaw;
		P.Diameter = Diameter;
		P.Sink = Sink;
		P.Variant = Variant;
		Out.Pebbles.Add(P);
	};

	// ---- 1. Rives : un candidat par cellule, l'etat decide, le bruit de plaque laisse du vide.
	const int32 CX = FMath::CeilToInt(In.Bounds.GetSize().X / S.ShoreCellUU);
	const int32 CY = FMath::CeilToInt(In.Bounds.GetSize().Y / S.ShoreCellUU);
	for (int32 CJ = 0; CJ < CY; ++CJ)
	{
		for (int32 CI = 0; CI < CX; ++CI)
		{
			auto R = [&](uint32 K) { return ContactUnit(CI, CJ, Seed, 0x5100u + K); };
			const double PX = In.Bounds.Min.X + (CI + 0.5 + (R(1) - 0.5) * 0.9) * S.ShoreCellUU;
			const double PY = In.Bounds.Min.Y + (CJ + 0.5 + (R(2) - 0.5) * 0.9) * S.ShoreCellUU;
			int32 I, J;
			if (!G.Locate(PX, PY, I, J)) continue;
			const int32 K = G.Index(I, J);
			if (!G.Valid[K] || G.Wet[K]) continue;
			const double D = G.Dist[K];
			if (D > S.ShoreBandUU) continue;
			double DownRad;
			const double Slope = G.SlopeAt(I, J, DownRad);
			if (Slope > S.MaxDecalSlope) continue;
			const double Free = static_cast<double>(G.Ground[K]) - static_cast<double>(G.NearWater[K]);
			const double Calm = In.SampleCalm ? FMath::Clamp(In.SampleCalm(PX, PY), 0.0, 1.0) : 1.0;
			const double Patch = ContactPatch(PX, PY, 2500.0, Seed, 0x5200u);
			const double Detail = ContactPatch(PX, PY, 700.0, Seed, 0x5300u);
			const double Flat = 1.0 - ContactSmooth(0.0, 0.25, Slope);
			const double Band = FMath::Lerp(100.0, 300.0, Patch) * (1.0 + 0.5 * Flat);
			const EShore State = ClassifyShore(D, Free, Slope, Calm, Patch, ReedStemsAround(PX, PY), Band);
			if (State == EShore::None) continue;
			++Out.BandCells;
			++Out.ShoreExamined[static_cast<int32>(State)];
			double AwayRad;
			const bool bAway = G.AwayFromWater(I, J, AwayRad);
			const double Tangent = bAway ? FMath::RadiansToDegrees(AwayRad) + 90.0 : R(3) * 360.0;
			const double Yaw = Tangent + (R(4) - 0.5) * 50.0;
			auto Place = [&](double Long, double Wide, EDecal Kind, double CenterFrac, bool bPull)
			{
				// Le centre est ramene vers l'eau : la bande touche la ligne d'eau, son bord exterieur
				// reste irregulier (largeur, probabilite et bruit de plaque varient le long de la rive).
				FVector At(PX, PY, 0.0);
				if (bPull && bAway)
				{
					const double Target = FMath::Min(D, Wide * CenterFrac);
					At.X -= FMath::Cos(AwayRad) * (D - Target);
					At.Y -= FMath::Sin(AwayRad) * (D - Target);
				}
				double SlopeHere, Down;
				if (!SnapToGround(At, SlopeHere, Down) || SlopeHere > S.MaxDecalSlope) return false;
				// Une rive est centree sur le NIVEAU DE L'EAU, pas sur le sol : le materiau retranche tout ce qui
				// est sous ce niveau (fond immerge, vu par transparence : ovales cyan de la capture first).
				if (bPull) At.Z = FMath::Clamp(static_cast<double>(G.NearWater[K]), At.Z - 120.0, At.Z);
				if (!Emit(At, Yaw, Long, Wide, Kind, State, ClassShore)) return false;
				Out.Decals.Last().bAway = bAway;
				Out.Decals.Last().AwayDegrees = bAway ? FMath::RadiansToDegrees(AwayRad) : 0.0;
				++Out.ShoreDecaled[static_cast<int32>(State)];
				return true;
			};
			switch (State)
			{
			case EShore::Damp:
				if (D < Band * 1.1 && R(5) < 0.9 * (1.0 - D / (Band * 1.3)))
				{
					Place(190.0 + 170.0 * R(6), Band * (0.55 + 0.6 * R(7)), EDecal::WetBand, 0.15 + 0.4 * R(8), true);
					if (R(9) < 0.45) Place(380.0 + 200.0 * R(10), Band * (1.2 + 0.7 * R(11)), EDecal::Halo, 0.35, true);
				}
				break;
			case EShore::Muddy:
				if (D < Band * 1.4 && R(5) < 0.92 * (1.0 - D / (Band * 1.6)))
				{
					Place(240.0 + 220.0 * R(6), Band * (0.6 + 0.6 * R(7)), EDecal::Mud, 0.15 + 0.35 * R(8), true);
					if (R(9) < 0.5) Place(420.0 + 200.0 * R(10), Band * (1.3 + 0.7 * R(11)), EDecal::Halo, 0.35, true);
				}
				break;
			case EShore::Reed:
				if (D < Band * 1.4 && R(5) < 0.8)
				{
					Place(220.0 + 160.0 * R(6), Band * (0.6 + 0.5 * R(7)), EDecal::WetBand, 0.2 + 0.35 * R(8), true);
				}
				break;
			case EShore::Stony:
				if (D < Band * 1.3 && R(5) < 0.8 * (1.0 - D / (Band * 1.5)))
				{
					if (Place(220.0 + 200.0 * R(6), Band * (0.5 + 0.55 * R(7)), EDecal::StoneWet, 0.2 + 0.4 * R(8), true))
					{
						const int32 Pebbles = 1 + FMath::FloorToInt(R(12) * 4.0 * (0.4 + Detail));
						for (int32 P = 0; P < Pebbles; ++P)
						{
							auto Q = [&](uint32 Salt) { return ContactUnit(CI * 7 + P, CJ * 13 + P, Seed, 0x5400u + Salt); };
							FVector At(PX + (Q(1) - 0.5) * Band * 0.9, PY + (Q(2) - 0.5) * Band * 0.9, 0.0);
							double Sl, Dn;
							if (!SnapToGround(At, Sl, Dn) || Sl > 0.5) continue;
							AddPebble(At, Q(3) * 360.0, FMath::Lerp(S.PebbleMinUU, S.PebbleMaxUU, Q(4) * Q(4)), 0.35 + 0.25 * Q(5), FMath::FloorToInt(Q(6) * 2.99));
						}
					}
				}
				break;
			case EShore::Dry:
				// Ligne de depot : un lit de crue pale, bas et allonge, rare, par plaques seulement.
				if (D > Band * 1.2 && D < Band * 3.5 && Patch > 0.6 && R(5) < 0.18)
				{
					Place(260.0 + 220.0 * R(6), 45.0 + 45.0 * R(7), EDecal::Deposit, 1.0, false);
				}
				break;
			default:
				break;
			}
		}
	}

	// ---- 2. Ancres : on pose la matiere sous ce qui est deja la, jamais un objet de plus.
	auto DownhillShift = [&](FVector& At, double Slope, double DownRad, double Amount)
	{
		const double Lean = ContactSmooth(0.02, 0.25, Slope);
		At.X += FMath::Cos(DownRad) * Amount * Lean;
		At.Y += FMath::Sin(DownRad) * Amount * Lean;
	};
	// Ordre melange mais deterministe : si le plafond tombe, il tronque au hasard et non par
	// morceaux entiers de carte (l'ordre des HISM suit les tuiles).
	TArray<int32> Order;
	Order.Reserve(In.Anchors.Num());
	TArray<uint32> Keys;
	Keys.Reserve(In.Anchors.Num());
	for (int32 Index = 0; Index < In.Anchors.Num(); ++Index)
	{
		Order.Add(Index);
		Keys.Add(ContactHash(FMath::RoundToInt(In.Anchors[Index].Location.X * 0.37), FMath::RoundToInt(In.Anchors[Index].Location.Y * 0.37), Seed, 0x99u));
	}
	Order.Sort([&Keys](int32 A, int32 B) { return Keys[A] != Keys[B] ? Keys[A] < Keys[B] : A < B; });
	for (const int32 Index : Order)
	{
		const FAnchor& A = In.Anchors[Index];
		if (A.Kind == EAnchor::Reed) continue;
		if (ClassUsed[ClassAnchor] >= ClassCap[ClassAnchor]) { Out.bTruncated = true; break; }
		auto R = [&](uint32 K)
		{
			return ContactUnit(FMath::RoundToInt(A.Location.X * 0.37), FMath::RoundToInt(A.Location.Y * 0.37), Seed, 0x6100u + K);
		};
		FVector Base = A.Location;
		double Slope, DownRad;
		if (!SnapToGround(Base, Slope, DownRad) || Slope > S.MaxDecalSlope) continue;
		int32 GI, GJ;
		const bool bGrid = G.Locate(Base.X, Base.Y, GI, GJ);
		// Un objet immerge ne recoit pas de matiere de sol : on ne salit pas un fond vu par transparence.
		if (bGrid && G.Wet[G.Index(GI, GJ)]) continue;
		const double DistToWater = bGrid ? G.Dist[G.Index(GI, GJ)] : 1.0e9;
		const double DownDeg = FMath::RadiansToDegrees(DownRad);
		const double Lean = ContactSmooth(0.02, 0.25, Slope);
		const double Facing = Lean > 0.2 ? DownDeg : R(20) * 360.0;
		++Out.AnchorCounts[static_cast<int32>(A.Kind)];
		const int32 DecalsBefore = Out.Decals.Num();
		switch (A.Kind)
		{
		case EAnchor::Tree:
		{
			if (A.Height < 450.0 || R(1) >= S.TreeShare) break;
			const double R0 = FMath::Clamp(0.028 * A.Height + 12.0, 20.0, 85.0);
			FVector At = Base;
			DownhillShift(At, Slope, DownRad, R0 * 0.5);
			Emit(At, Facing + (R(2) - 0.5) * 60.0, R0 * (3.4 + 1.4 * R(3)), R0 * (2.8 + 1.0 * R(4)), EDecal::ContactDark, EShore::None, ClassAnchor);
			// La litiere s'accumule en aval, et moins pres de l'eau, ou le courant l'emporte.
			const double LitterChance = DistToWater < 900.0 ? 0.45 : 0.85;
			if (R(5) < LitterChance)
			{
				FVector Lit = Base;
				DownhillShift(Lit, Slope, DownRad, R0 * 1.4);
				Lit.X += (R(6) - 0.5) * R0 * 0.8;
				Lit.Y += (R(7) - 0.5) * R0 * 0.8;
				Emit(Lit, Facing + (R(8) - 0.5) * 70.0, R0 * (5.4 + 2.4 * R(9)), R0 * (4.2 + 1.8 * R(10)), EDecal::Litter, EShore::None, ClassAnchor);
			}
			break;
		}
		case EAnchor::Sapling:
			if (R(1) < 0.4)
			{
				const double R0 = FMath::Clamp(0.03 * A.Height + 8.0, 12.0, 40.0);
				Emit(Base, R(2) * 360.0, R0 * (2.2 + R(3)), R0 * (1.8 + 0.8 * R(4)), EDecal::ContactDark, EShore::None, ClassAnchor);
			}
			break;
		case EAnchor::Stump:
		{
			const double R0 = FMath::Max(A.Radius, 20.0);
			Emit(Base, R(2) * 360.0, R0 * (2.4 + R(3)), R0 * (2.0 + 0.8 * R(4)), EDecal::ContactDark, EShore::None, ClassAnchor);
			if (R(5) < 0.6) Emit(Base, R(6) * 360.0, R0 * (3.8 + 1.5 * R(7)), R0 * (3.0 + R(8)), EDecal::Litter, EShore::None, ClassAnchor);
			break;
		}
		case EAnchor::Lying:
			// Un objet couche s'allonge : le sol est sombre sous toute sa longueur, pas autour d'un point.
			Emit(Base, A.YawDegrees + (R(2) - 0.5) * 20.0, A.Radius + 35.0 + 25.0 * R(3), A.Radius * 0.42 + 40.0 + 30.0 * R(4), EDecal::ContactDark, EShore::None, ClassAnchor);
			break;
		case EAnchor::Roots:
			Emit(Base, R(2) * 360.0, A.Radius * (1.5 + 0.4 * R(3)) + 25.0, A.Radius * (1.3 + 0.4 * R(4)) + 25.0, EDecal::ContactDark, EShore::None, ClassAnchor);
			break;
		case EAnchor::Bush:
			if (R(1) < 0.55 && A.Radius > 20.0)
			{
				Emit(Base, R(2) * 360.0, A.Radius * (1.1 + 0.3 * R(3)), A.Radius * (0.9 + 0.3 * R(4)), EDecal::ContactDark, EShore::None, ClassAnchor);
			}
			break;
		case EAnchor::Rock:
		{
			if (A.Radius < 18.0 || R(1) >= S.RockShare) break;
			FVector At = Base;
			DownhillShift(At, Slope, DownRad, A.Radius * 0.3);
			Emit(At, Facing + (R(2) - 0.5) * 40.0, A.Radius * (1.45 + 0.35 * R(3)), A.Radius * (1.2 + 0.3 * R(4)), EDecal::RockDirt, EShore::None, ClassAnchor);
			// Cailloux : plus nombreux en aval, une collerette qui se brise, pas un anneau.
			const int32 Count = FMath::Clamp(FMath::RoundToInt(A.Radius / 22.0 * (0.6 + R(5))), 0, 9);
			for (int32 P = 0; P < Count; ++P)
			{
				auto Q = [&](uint32 Salt) { return ContactUnit(Index * 31 + P, P + 7, Seed, 0x6500u + Salt); };
				const bool bDown = Lean > 0.2 && Q(1) < 0.65;
				const double Angle = bDown ? DownRad + (Q(2) - 0.5) * 2.4 : Q(2) * UE_TWO_PI;
				const double Ring = A.Radius * (0.95 + 0.95 * Q(3));
				FVector Pb(Base.X + FMath::Cos(Angle) * Ring, Base.Y + FMath::Sin(Angle) * Ring, 0.0);
				double Sl, Dn;
				if (!SnapToGround(Pb, Sl, Dn) || Sl > 0.6) continue;
				AddPebble(Pb, Q(4) * 360.0, FMath::Lerp(S.PebbleMinUU, S.PebbleMaxUU, Q(5) * Q(5)) * (0.7 + 0.6 * Q(6)), 0.35 + 0.25 * Q(7), FMath::FloorToInt(Q(8) * 2.99));
			}
			break;
		}
		default:
			break;
		}
		for (int32 I = DecalsBefore; I < Out.Decals.Num(); ++I) Out.Decals[I].Source = static_cast<int32>(A.Kind);
	}

	// ---- 3. Plaques de roseaux : la matiere sous les roseaux reels (centroide de chaque case).
	for (const TPair<int64, FReedBin>& Pair : Reeds)
	{
		const FReedBin& Bin = Pair.Value;
		if (Bin.Count < S.ReedMinStems) continue;
		const int32 BX = static_cast<int32>(Pair.Key >> 32), BY = static_cast<int32>(Pair.Key & 0xFFFFFFFF);
		auto R = [&](uint32 K) { return ContactUnit(BX, BY, Seed, 0x6700u + K); };
		if (R(1) >= 0.9) continue;
		FVector At(Bin.SumX / Bin.Count, Bin.SumY / Bin.Count, 0.0);
		double Slope, DownRad;
		if (!SnapToGround(At, Slope, DownRad) || Slope > S.MaxDecalSlope) continue;
		double ReedWater;
		if (In.SampleWaterHeight(At.X, At.Y, ReedWater) && FMath::IsFinite(ReedWater)) At.Z = FMath::Clamp(ReedWater, At.Z - 120.0, At.Z);
		if (!Emit(At, R(2) * 360.0, S.ReedCellUU * (0.8 + 0.5 * R(3)), S.ReedCellUU * (0.6 + 0.4 * R(4)), EDecal::ReedBed, EShore::Reed, ClassReed))
		{
			break;
		}
	}

	// ---- 4. Prairie : depressions plus humides, lits de depot, rigoles d'erosion. Tres rare.
	const int32 MX = FMath::CeilToInt(In.Bounds.GetSize().X / S.MeadowCellUU);
	const int32 MY = FMath::CeilToInt(In.Bounds.GetSize().Y / S.MeadowCellUU);
	for (int32 MJ = 0; MJ < MY; ++MJ)
	{
		for (int32 MI = 0; MI < MX; ++MI)
		{
			auto R = [&](uint32 K) { return ContactUnit(MI, MJ, Seed, 0x7100u + K); };
			const double PX = In.Bounds.Min.X + (MI + 0.5 + (R(1) - 0.5) * 0.9) * S.MeadowCellUU;
			const double PY = In.Bounds.Min.Y + (MJ + 0.5 + (R(2) - 0.5) * 0.9) * S.MeadowCellUU;
			int32 I, J;
			if (!G.Locate(PX, PY, I, J)) continue;
			const int32 K = G.Index(I, J);
			if (!G.Valid[K] || G.Wet[K] || G.Dist[K] < S.ShoreBandUU * 0.5) continue;
			double DownRad;
			const double Slope = G.SlopeAt(I, J, DownRad);
			if (Slope > S.MaxDecalSlope) continue;
			const double Macro = ContactPatch(PX, PY, 4000.0, Seed, 0x7200u);
			// Creux : le sol est plus bas que la moyenne de son voisinage a trois cases.
			double Around = 0.0;
			int32 Taps = 0;
			for (int32 Dir = 0; Dir < 4; ++Dir)
			{
				double Z;
				const int32 DI = (Dir == 0) - (Dir == 1) , DJ = (Dir == 2) - (Dir == 3);
				if (G.GroundAt(I + 3 * DI, J + 3 * DJ, Z)) { Around += Z; ++Taps; }
			}
			const double Hollow = Taps == 4 ? Around / 4.0 - G.Ground[K] : 0.0;
			FVector At(PX, PY, 0.0);
			double SlopeHere, Down;
			if (!SnapToGround(At, SlopeHere, Down)) continue;
			if (Hollow > 6.0 && Slope < 0.2 && R(3) < 0.55)
			{
				Emit(At, R(4) * 360.0, 260.0 + 260.0 * R(5), 200.0 + 200.0 * R(6), EDecal::Depression, EShore::None, ClassMeadow);
			}
			else if (Slope > 0.22 && Slope < 0.6 && R(3) < 0.3)
			{
				Emit(At, FMath::RadiansToDegrees(Down) + (R(7) - 0.5) * 20.0, 300.0 + 300.0 * R(5), 40.0 + 40.0 * R(6), EDecal::Streak, EShore::None, ClassMeadow);
			}
			else if (Slope < 0.08 && Macro > 0.62 && R(3) < 0.22)
			{
				Emit(At, R(4) * 360.0, 220.0 + 200.0 * R(5), 110.0 + 120.0 * R(6), EDecal::Deposit, EShore::None, ClassMeadow);
			}
		}
	}

	// Profondeur de projection : la meme pour tous, lue aussi par le materiau (Surface.w, DEPTH de
	// aaa-contact-realism.py) qui fond le decalque sur cette epaisseur : plus de bord droit sur un talus.
	for (FDecal& D : Out.Decals) D.HalfDepth = S.DecalDepthUU;
	Out.MilliSeconds = (FPlatformTime::Seconds() - Start) * 1000.0;
	return true;
}
