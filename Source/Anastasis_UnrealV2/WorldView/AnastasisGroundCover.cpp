#include "WorldView/AnastasisGroundCover.h"
#include "Async/ParallelFor.h"

// Espace nomme, pas anonyme : en build unity, un Smooth() anonyme d'un autre fichier du meme
// lot entre en collision (cf. AnastasisDrainage.cpp).
namespace AnastasisGroundCover::Detail
{
uint32 Hash(uint32 Seed, int32 X, int32 Y, uint32 Salt)
{
	uint32 H = Seed ^ 0x9E3779B9u;
	H = (H ^ static_cast<uint32>(X)) * 0x85EBCA6Bu;
	H = (H ^ static_cast<uint32>(Y)) * 0xC2B2AE35u;
	H = (H ^ Salt) * 0x27D4EB2Fu;
	return H ^ (H >> 15);
}

double Unit(uint32 H) { return static_cast<double>(H) / 4294967296.0; }

double Smooth(double V)
{
	V = FMath::Clamp(V, 0.0, 1.0);
	return V * V * (3.0 - 2.0 * V);
}

/** Bruit de valeur bilineaire lisse, [0,1]. X, Y en cellules du bruit. */
double Noise(uint32 Seed, double X, double Y, uint32 Salt)
{
	const int32 IX = FMath::FloorToInt(X), IY = FMath::FloorToInt(Y);
	const double U = Smooth(X - IX), V = Smooth(Y - IY);
	return FMath::Lerp(
		FMath::Lerp(Unit(Hash(Seed, IX, IY, Salt)), Unit(Hash(Seed, IX + 1, IY, Salt)), U),
		FMath::Lerp(Unit(Hash(Seed, IX, IY + 1, Salt)), Unit(Hash(Seed, IX + 1, IY + 1, Salt)), U), V);
}

/** Deux octaves : des taches franches, pas des bulles regulieres. */
double Patch(uint32 Seed, double X, double Y, double Span, uint32 Salt)
{
	const double A = Noise(Seed, X / Span, Y / Span, Salt);
	const double B = Noise(Seed, X / (Span * 0.37), Y / (Span * 0.37), Salt + 1);
	return FMath::Clamp(0.7 * A + 0.3 * B, 0.0, 1.0);
}

bool ValidSettings(const FSettings& C)
{
	const auto Finite = [](double V) { return FMath::IsFinite(V); };
	return Finite(C.CellUU) && C.CellUU >= 20.0 && C.CellUU <= 2000.0
		&& Finite(C.ProbeUU) && C.ProbeUU > 0.0 && C.ProbeUU <= 500.0
		&& Finite(C.TallSlopeDegrees) && Finite(C.MeadowSlopeDegrees) && Finite(C.MaxSlopeDegrees)
		&& C.TallSlopeDegrees > 0.0 && C.TallSlopeDegrees < C.MeadowSlopeDegrees
		&& C.MeadowSlopeDegrees < C.MaxSlopeDegrees && C.MaxSlopeDegrees < 90.0
		&& Finite(C.LandeBlendDegrees) && C.LandeBlendDegrees >= 0.0
		&& Finite(C.LandeDensity) && C.LandeDensity > 0.0 && C.LandeDensity <= 1.0
		&& Finite(C.LandeDensitySteep) && C.LandeDensitySteep >= 0.0 && C.LandeDensitySteep <= C.LandeDensity
		&& Finite(C.LandePatchFloor) && C.LandePatchFloor >= 0.0 && C.LandePatchFloor <= 1.0
		&& Finite(C.LandeShade) && C.LandeShade >= 0.0 && C.LandeShade <= 1.0
		&& Finite(C.LandeScale) && C.LandeScale > 0.0 && C.LandeScale <= 4.0
		&& Finite(C.HeatherAboveFloorUU) && Finite(C.HeatherRangeUU) && C.HeatherRangeUU > 0.0
		&& Finite(C.BlendDegrees) && C.BlendDegrees >= 0.0
		&& Finite(C.WaterClearanceUU) && C.WaterClearanceUU >= 0.0
		&& Finite(C.SedgeWetness) && C.SedgeWetness > 0.0 && C.SedgeWetness <= 1.0
		&& Finite(C.DampHeightUU) && C.DampHeightUU >= 0.0
		&& Finite(C.MinMask) && C.MinMask >= 0.0 && C.MinMask < 1.0
		&& Finite(C.PatchSpanUU) && C.PatchSpanUU > 0.0
		&& Finite(C.PatchFloor) && C.PatchFloor >= 0.0 && C.PatchFloor <= 1.0
		&& Finite(C.Density) && C.Density > 0.0 && C.Density <= 1.0
		&& Finite(C.CanopyExclusion) && Finite(C.CanopyShade)
		&& C.CanopyExclusion >= 0.0 && C.CanopyShade >= C.CanopyExclusion
		&& Finite(C.ScaleMin) && Finite(C.ScaleMax) && C.ScaleMin > 0.0 && C.ScaleMax >= C.ScaleMin && C.ScaleMax <= 4.0
		&& C.MaxInstances > 0;
}

/** Index spatial des couronnes : la question est "quelle couronne la plus proche, en rayons". */
struct FCanopyIndex
{
	double Cell = 1.0;
	TMap<FIntPoint, TArray<int32>> Cells;
	const TArray<FVector>* Crowns = nullptr;

	void Init(const TArray<FVector>& In, double Reach)
	{
		Crowns = &In;
		double MaxRadius = 0.0;
		for (const FVector& C : In) MaxRadius = FMath::Max(MaxRadius, C.Z);
		Cell = FMath::Max(100.0, MaxRadius * Reach);
		for (int32 I = 0; I < In.Num(); ++I)
		{
			Cells.FindOrAdd(FIntPoint(FMath::FloorToInt(In[I].X / Cell), FMath::FloorToInt(In[I].Y / Cell))).Add(I);
		}
	}

	/** Distance a la couronne la plus proche, en multiples de son rayon ; grand = a decouvert. */
	double Nearest(double X, double Y) const
	{
		double Best = TNumericLimits<double>::Max();
		const int32 CX = FMath::FloorToInt(X / Cell), CY = FMath::FloorToInt(Y / Cell);
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				const TArray<int32>* Found = Cells.Find(FIntPoint(CX + DX, CY + DY));
				if (!Found) continue;
				for (const int32 I : *Found)
				{
					const FVector& C = (*Crowns)[I];
					if (C.Z <= 0.0) continue;
					Best = FMath::Min(Best, FVector2D::Distance(FVector2D(X, Y), FVector2D(C.X, C.Y)) / C.Z);
				}
			}
		}
		return Best;
	}
};
}

FVector3f AnastasisGroundCover::FCoverField::Sample(double X, double Y) const
{
	if (!IsValid()) return FVector3f::ZeroVector;
	const double U = (X - Origin.X) / CellUU - 0.5, V = (Y - Origin.Y) / CellUU - 0.5;
	if (!FMath::IsFinite(U) || !FMath::IsFinite(V) || U < -0.5 || V < -0.5 || U > W - 0.5 || V > H - 0.5) return FVector3f::ZeroVector;
	const int32 X0 = FMath::Clamp(FMath::FloorToInt(U), 0, W - 2), Y0 = FMath::Clamp(FMath::FloorToInt(V), 0, H - 2);
	const float FX = static_cast<float>(FMath::Clamp(U - X0, 0.0, 1.0)), FY = static_cast<float>(FMath::Clamp(V - Y0, 0.0, 1.0));
	const int32 A = Y0 * W + X0;
	return FMath::Lerp(FMath::Lerp(Cover[A], Cover[A + 1], FX), FMath::Lerp(Cover[A + W], Cover[A + W + 1], FX), FY);
}

void AnastasisGroundCover::BuildCoverField(const FPlan& Plan, const FBox2D& Bounds, double CellUU, double CandidateCellUU, FCoverField& Out)
{
	Out = FCoverField();
	if (!Bounds.bIsValid || !(CellUU > 0.0) || !(CandidateCellUU > 0.0)) return;
	Out.Origin = Bounds.Min;
	Out.CellUU = CellUU;
	Out.W = FMath::Max(2, FMath::CeilToInt((Bounds.Max.X - Bounds.Min.X) / CellUU));
	Out.H = FMath::Max(2, FMath::CeilToInt((Bounds.Max.Y - Bounds.Min.Y) / CellUU));
	Out.Cover.SetNumZeroed(Out.W * Out.H);
	// Une cellule du champ contient au plus (CellUU / CandidateCellUU)^2 candidates : la
	// couverture est la part de candidates REELLEMENT posees, par groupe de familles.
	const float PerTuft = static_cast<float>(FMath::Square(CandidateCellUU / CellUU));
	for (const FPlacement& P : Plan.Instances)
	{
		const int32 X = FMath::FloorToInt((P.Ground.X - Out.Origin.X) / CellUU), Y = FMath::FloorToInt((P.Ground.Y - Out.Origin.Y) / CellUU);
		if (X < 0 || Y < 0 || X >= Out.W || Y >= Out.H) continue;
		FVector3f& C = Out.Cover[Y * Out.W + X];
		switch (P.Family)
		{
		case EFamily::MeadowTall: case EFamily::MeadowShort: C.X += PerTuft; break;
		case EFamily::Sedge: C.Y += PerTuft; break;
		default: C.Z += PerTuft; break;
		}
	}
	// Deux passes de flou 3x3 : ~3 cellules de portee, pas de marche a la frontiere d'une tache.
	TArray<FVector3f> Tmp;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		Tmp = Out.Cover;
		for (int32 Y = 0; Y < Out.H; ++Y)
		{
			for (int32 X = 0; X < Out.W; ++X)
			{
				FVector3f Sum = FVector3f::ZeroVector;
				int32 N = 0;
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 DX = -1; DX <= 1; ++DX)
					{
						const int32 SX = X + DX, SY = Y + DY;
						if (SX < 0 || SY < 0 || SX >= Out.W || SY >= Out.H) continue;
						Sum += Tmp[SY * Out.W + SX];
						++N;
					}
				}
				Out.Cover[Y * Out.W + X] = Sum / static_cast<float>(N);
			}
		}
	}
	for (FVector3f& C : Out.Cover)
	{
		C = FVector3f(FMath::Clamp(C.X, 0.f, 1.f), FMath::Clamp(C.Y, 0.f, 1.f), FMath::Clamp(C.Z, 0.f, 1.f));
	}
}

FLinearColor AnastasisGroundCover::TintSoil(const FLinearColor& Base, const FVector3f& Cover, const FSoilTint& T, double* Amount)
{
	using namespace AnastasisGroundCover::Detail;
	const double Total = static_cast<double>(Cover.X) + Cover.Y + Cover.Z;
	if (Amount) *Amount = 0.0;
	if (!(Total > 1e-4) || !(T.FullCover > 0.0)) return Base;
	const auto Target = [&](const FLinearColor& Factor, const FLinearColor& Absolute)
	{
		const FLinearColor Relative(Base.R * Factor.R, Base.G * Factor.G, Base.B * Factor.B, Base.A);
		return FMath::Lerp(Relative, FLinearColor(Absolute.R, Absolute.G, Absolute.B, Base.A), static_cast<float>(T.AbsoluteShare));
	};
	const FLinearColor Mixed = (Target(T.MeadowFactor, T.MeadowAbsolute) * Cover.X
		+ Target(T.SedgeFactor, T.SedgeAbsolute) * Cover.Y
		+ Target(T.LandeFactor, T.LandeAbsolute) * Cover.Z) / static_cast<float>(Total);
	const double Share = FMath::Clamp(T.Strength, 0.0, 1.0) * Smooth(FMath::Min(Total, 1.0) / T.FullCover);
	if (Amount) *Amount = Share;
	FLinearColor Out = FMath::Lerp(Base, Mixed, static_cast<float>(Share));
	Out.A = Base.A;
	return Out;
}

const TCHAR* AnastasisGroundCover::FamilyName(EFamily Family)
{
	switch (Family)
	{
	case EFamily::MeadowTall: return TEXT("MeadowTall");
	case EFamily::MeadowShort: return TEXT("MeadowShort");
	case EFamily::Sedge: return TEXT("Sedge");
	case EFamily::HeathTussock: return TEXT("HeathTussock");
	case EFamily::Heather: return TEXT("Heather");
	default: return TEXT("Unknown");
	}
}

FString AnastasisGroundCover::MeshPath(EFamily Family)
{
	const FString Leaf = FString::Printf(TEXT("SM_Grass_%s_01"), FamilyName(Family));
	return FString::Printf(TEXT("/Game/Anastasis/GroundCover/%s.%s"), *Leaf, *Leaf);
}

bool AnastasisGroundCover::Build(const FInputs& In, const FSettings& C, FPlan& Out, FString& OutError)
{
	using namespace AnastasisGroundCover::Detail;
	Out = FPlan();
	if (!ValidSettings(C)) { OutError = TEXT("ground cover: invalid settings"); return false; }
	if (!In.SampleHeight) { OutError = TEXT("ground cover: no ground sampler"); return false; }
	if (!In.Mask) { OutError = TEXT("ground cover: no open-ground mask"); return false; }
	if (In.bHasValleyFloor && !FMath::IsFinite(In.ValleyFloorZ)) { OutError = TEXT("ground cover: non-finite valley floor"); return false; }
	if (!In.Bounds.bIsValid || !FMath::IsFinite(In.Bounds.Min.X) || !FMath::IsFinite(In.Bounds.Max.X)
		|| !FMath::IsFinite(In.Bounds.Min.Y) || !FMath::IsFinite(In.Bounds.Max.Y)
		|| In.Bounds.Max.X <= In.Bounds.Min.X || In.Bounds.Max.Y <= In.Bounds.Min.Y)
	{
		OutError = TEXT("ground cover: empty or non-finite bounds");
		return false;
	}
	for (const FVector& Crown : In.Canopy)
	{
		if (!Crown.ContainsNaN() && FMath::IsFinite(Crown.X) && FMath::IsFinite(Crown.Y) && FMath::IsFinite(Crown.Z)) continue;
		OutError = TEXT("ground cover: non-finite crown");
		return false;
	}

	FCanopyIndex Canopy;
	Canopy.Init(In.Canopy, C.CanopyShade);
	const double R = C.ProbeUU;
	const int32 CellsX = FMath::FloorToInt((In.Bounds.Max.X - In.Bounds.Min.X) / C.CellUU);
	const int32 CellsY = FMath::FloorToInt((In.Bounds.Max.Y - In.Bounds.Min.Y) / C.CellUU);
	// Blocs d'environ 20 m : un bloc sans espace ouvert est saute sur 25 sondes du masque,
	// sans evaluer ses cellules une a une. La decision par cellule reste celle du masque.
	const int32 Block = FMath::Max(1, FMath::CeilToInt(2000.0 / C.CellUU));

	// Une rangee de blocs par tache : les echantillonneurs sont en lecture seule (forge, drainage,
	// geographie ecrite), et les rangees sont fusionnees DANS L'ORDRE -- le plan ne depend ni du
	// nombre de coeurs ni de l'ordonnancement. Toute la carte : ~2,5 M candidates.
	const int32 BlockRows = (CellsY + Block - 1) / Block;
	TArray<FPlan> Rows;
	Rows.SetNum(BlockRows);
	ParallelFor(BlockRows, [&](int32 RowIndex)
	{
		FPlan& Row = Rows[RowIndex];
		const int32 BY = RowIndex * Block;
		for (int32 BX = 0; BX < CellsX; BX += Block)
		{
			double BlockMask = 0.0;
			for (int32 SY = 0; SY <= 4; ++SY)
			{
				for (int32 SX = 0; SX <= 4; ++SX)
				{
					const double PX = In.Bounds.Min.X + (BX + Block * SX / 4.0) * C.CellUU;
					const double PY = In.Bounds.Min.Y + (BY + Block * SY / 4.0) * C.CellUU;
					BlockMask = FMath::Max(BlockMask, In.Mask(PX, PY));
					if (In.LandeMask) BlockMask = FMath::Max(BlockMask, In.LandeMask(PX, PY));
				}
			}
			// Marge : le masque est lisse, une cellule du bloc peut depasser les sondes.
			if (BlockMask < C.MinMask * 0.5) continue;

			for (int32 GY = BY; GY < FMath::Min(BY + Block, CellsY); ++GY)
			{
				for (int32 GX = BX; GX < FMath::Min(BX + Block, CellsX); ++GX)
				{
					++Row.Candidates;
					const double X = In.Bounds.Min.X + (GX + Unit(Hash(In.Seed, GX, GY, 11))) * C.CellUU;
					const double Y = In.Bounds.Min.Y + (GY + Unit(Hash(In.Seed, GX, GY, 12))) * C.CellUU;
					const double Mask = FMath::Clamp(In.Mask(X, Y), 0.0, 1.0);
					const double Lande = In.LandeMask ? FMath::Clamp(In.LandeMask(X, Y), 0.0, 1.0) : Mask;
					if (!(FMath::Max(Mask, Lande) >= C.MinMask)) { ++Row.RejectedMask; continue; }

					double Z, East, West, North, South;
					if (!In.SampleHeight(X, Y, Z) || !In.SampleHeight(X + R, Y, East) || !In.SampleHeight(X - R, Y, West)
						|| !In.SampleHeight(X, Y + R, North) || !In.SampleHeight(X, Y - R, South)
						|| !FMath::IsFinite(Z) || !FMath::IsFinite(East) || !FMath::IsFinite(West)
						|| !FMath::IsFinite(North) || !FMath::IsFinite(South))
					{
						++Row.RejectedGround;
						continue;
					}

					double AboveWater = TNumericLimits<double>::Max();
					if (In.SampleWaterHeight)
					{
						// La touffe entiere, pas son centre : une touffe a cheval sur la berge trempe.
						bool bDry = true;
						double Lowest = TNumericLimits<double>::Max();
						const FVector2D Probes[] = {{0.0, 0.0}, {R, 0.0}, {-R, 0.0}, {0.0, R}, {0.0, -R}};
						const double Heights[] = {Z, East, West, North, South};
						for (int32 K = 0; K < 5 && bDry; ++K)
						{
							double W;
							if (!In.SampleWaterHeight(X + Probes[K].X, Y + Probes[K].Y, W) || !FMath::IsFinite(W)) continue;
							Lowest = FMath::Min(Lowest, Heights[K] - W);
							bDry = Heights[K] > W + C.WaterClearanceUU;
						}
						if (!bDry) { ++Row.RejectedWater; continue; }
						AboveWater = Lowest;
					}

					// Meme lecture que la foret macro : quatre pentes unilaterales, une falaise
					// etroite n'est pas moyennee en pente douce.
					const double DX = FMath::Max(FMath::Abs(East - Z), FMath::Abs(West - Z)) / R;
					const double DY = FMath::Max(FMath::Abs(North - Z), FMath::Abs(South - Z)) / R;
					const double Slope = FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(DX * DX + DY * DY)));
					++Row.SlopeBins[Slope < 10.0 ? 0 : Slope < 20.0 ? 1 : Slope < 30.0 ? 2 : Slope < 45.0 ? 3 : Slope < 60.0 ? 4 : 5];
					if (Slope > C.MaxSlopeDegrees) { ++Row.RejectedSlope; continue; }

					const double CrownDistance = Canopy.Nearest(X, Y);
					if (CrownDistance < C.CanopyExclusion)
					{
						++Row.RejectedCanopy;
						Row.RejectedCanopySteep += Slope > C.MeadowSlopeDegrees ? 1 : 0;
						continue;
					}
					const double Shade = C.CanopyShade > C.CanopyExclusion
						? 1.0 - Smooth((CrownDistance - C.CanopyExclusion) / (C.CanopyShade - C.CanopyExclusion))
						: 0.0;

					double Wet = 0.0;
					if (In.SampleWetness)
					{
						double W;
						if (In.SampleWetness(X, Y, W) && FMath::IsFinite(W)) Wet = FMath::Clamp(W, 0.0, 1.0);
					}
					if (C.DampHeightUU > 0.0 && AboveWater < TNumericLimits<double>::Max())
					{
						Wet = FMath::Max(Wet, 1.0 - Smooth((AboveWater - C.WaterClearanceUU) / C.DampHeightUU));
					}

					double Trampled = 1.0;
					for (const FClearing& Cl : In.Clearings)
					{
						if (Cl.Radius <= 0.0) continue;
						const double D = FVector2D::Distance(FVector2D(X, Y), Cl.Center) / Cl.Radius;
						Trampled = FMath::Min(Trampled, FMath::Lerp(FMath::Clamp(Cl.Keep, 0.0, 1.0), 1.0, Smooth((D - 0.7) / 0.6)));
					}

					// Lande ou prairie : un tirage dans le fondu autour de MeadowSlopeDegrees, pas une
					// courbe de niveau. EZ5 : "aucune ligne fixe mais un gradient de vie".
					const double LandeChance = C.LandeBlendDegrees > 0.0
						? Smooth((Slope - (C.MeadowSlopeDegrees - C.LandeBlendDegrees)) / (2.0 * C.LandeBlendDegrees))
						: (Slope > C.MeadowSlopeDegrees ? 1.0 : 0.0);
					const bool bLande = Unit(Hash(In.Seed, GX, GY, 19)) < LandeChance;
					const double Habitat = bLande ? Lande : Mask;
					if (!(Habitat >= C.MinMask)) { ++Row.RejectedMask; continue; }

					double Density;
					if (bLande)
					{
						// La lande s'eclaircit en montant ; ses taches sont plus maigres que celles
						// de la prairie : la roche affleure (EZ1, "rochers, eboulis").
						const double Steep = Smooth((Slope - C.MeadowSlopeDegrees) / (C.MaxSlopeDegrees - C.MeadowSlopeDegrees));
						Density = FMath::Lerp(C.LandeDensity, C.LandeDensitySteep, Steep)
							* Smooth((Habitat - C.MinMask) / 0.3)
							* FMath::Lerp(C.LandePatchFloor, 1.0, Smooth((Patch(In.Seed, X, Y, C.PatchSpanUU, 41) - 0.25) / 0.5))
							* (1.0 - C.LandeShade * Shade);
					}
					else
					{
						const double Patchy = Patch(In.Seed, X, Y, C.PatchSpanUU, 21);
						Density = C.Density
							* Smooth((Mask - C.MinMask) / 0.3)
							* FMath::Lerp(C.PatchFloor, 1.0, Smooth((Patchy - 0.25) / 0.5))
							* (1.0 - 0.45 * Smooth((Slope - 12.0) / 8.0))
							* (1.0 - 0.75 * Shade)
							* Trampled;
					}
					if (Unit(Hash(In.Seed, GX, GY, 13)) >= Density) { ++Row.RejectedDensity; continue; }

					// Famille. Lande : la callune tient le haut des versants, loin du fond de vallee,
					// la touffe d'eboulis le reste. Prairie : humidite d'abord (laiches), puis la pente
					// decide haute ou basse ; une seconde tache melange les hauteurs sur le plat.
					EFamily Family;
					const double WetDraw = C.SedgeWetness + 0.12 * (Unit(Hash(In.Seed, GX, GY, 14)) - 0.5);
					if (bLande)
					{
						const double High = In.bHasValleyFloor
							? Smooth((Z - In.ValleyFloorZ - C.HeatherAboveFloorUU) / C.HeatherRangeUU) : 1.0;
						const double HeatherChance = 0.75 * High
							* FMath::Lerp(0.25, 1.0, Smooth((Patch(In.Seed, X, Y, C.PatchSpanUU * 0.8, 51) - 0.2) / 0.5));
						Family = Unit(Hash(In.Seed, GX, GY, 20)) < HeatherChance ? EFamily::Heather : EFamily::HeathTussock;
					}
					else if (Wet >= WetDraw)
					{
						Family = EFamily::Sedge;
					}
					else
					{
						const double Flat = C.BlendDegrees > 0.0
							? 1.0 - Smooth((Slope - (C.TallSlopeDegrees - C.BlendDegrees)) / (2.0 * C.BlendDegrees))
							: (Slope <= C.TallSlopeDegrees ? 1.0 : 0.0);
						const double Mix = FMath::Lerp(0.45, 1.0, Smooth((Patch(In.Seed, X, Y, C.PatchSpanUU * 0.6, 31) - 0.2) / 0.45));
						// Sol pietine et lisiere ombragee : l'herbe haute y cede la place a la rase.
						const double TallChance = Flat * Mix * Trampled * (1.0 - 0.6 * Shade);
						Family = Unit(Hash(In.Seed, GX, GY, 15)) < TallChance ? EFamily::MeadowTall : EFamily::MeadowShort;
					}

					FPlacement& P = Row.Instances.AddDefaulted_GetRef();
					P.Ground = FVector(X, Y, Z);
					P.Normal = FVector(-(East - West) / (2.0 * R), -(North - South) / (2.0 * R), 1.0).GetSafeNormal();
					P.Yaw = 360.0 * Unit(Hash(In.Seed, GX, GY, 16));
					P.Scale = FMath::Lerp(C.ScaleMin, C.ScaleMax, Unit(Hash(In.Seed, GX, GY, 17)))
						* (Family == EFamily::MeadowTall ? FMath::Lerp(0.85, 1.0, Mask) : 1.0)
						* (bLande ? C.LandeScale : 1.0);
					P.SlopeDegrees = Slope;
					P.Wetness = Wet;
					P.Family = Family;
					P.Thin = Unit(Hash(In.Seed, GX, GY, 18));
				}
			}
		}
	});
	for (FPlan& Row : Rows)
	{
		Out.Candidates += Row.Candidates;
		Out.RejectedMask += Row.RejectedMask;
		Out.RejectedGround += Row.RejectedGround;
		Out.RejectedWater += Row.RejectedWater;
		Out.RejectedSlope += Row.RejectedSlope;
		Out.RejectedCanopy += Row.RejectedCanopy;
		Out.RejectedDensity += Row.RejectedDensity;
		Out.RejectedCanopySteep += Row.RejectedCanopySteep;
		for (int32 B = 0; B < 6; ++B) Out.SlopeBins[B] += Row.SlopeBins[B];
		for (const FPlacement& P : Row.Instances)
		{
			if (Out.Instances.Num() >= C.MaxInstances) { Out.bTruncated = true; break; }
			Out.Instances.Add(P);
			++Out.Counts[static_cast<int32>(P.Family)];
		}
		Row.Instances.Empty();
	}
	if (In.bHasValleyFloor)
	{
		TArray<double> Above;
		for (const FPlacement& P : Out.Instances)
		{
			if (P.Family == EFamily::HeathTussock || P.Family == EFamily::Heather) Above.Add(P.Ground.Z - In.ValleyFloorZ);
		}
		Above.Sort();
		for (int32 Q = 0; Q < 3 && Above.Num() > 0; ++Q)
		{
			Out.LandeAboveFloor[Q] = Above[FMath::Clamp(FMath::FloorToInt((0.1 + 0.4 * Q) * (Above.Num() - 1)), 0, Above.Num() - 1)];
		}
	}
	return true;
}
