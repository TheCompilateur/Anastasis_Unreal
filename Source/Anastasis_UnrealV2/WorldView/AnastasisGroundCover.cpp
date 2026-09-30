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
		&& Finite(C.TallSlopeDegrees) && Finite(C.MaxSlopeDegrees)
		&& C.TallSlopeDegrees > 0.0 && C.TallSlopeDegrees < C.MaxSlopeDegrees && C.MaxSlopeDegrees < 90.0
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

const TCHAR* AnastasisGroundCover::FamilyName(EFamily Family)
{
	switch (Family)
	{
	case EFamily::MeadowTall: return TEXT("MeadowTall");
	case EFamily::MeadowShort: return TEXT("MeadowShort");
	case EFamily::Sedge: return TEXT("Sedge");
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
					if (!(Mask >= C.MinMask)) { ++Row.RejectedMask; continue; }

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
					if (Slope > C.MaxSlopeDegrees) { ++Row.RejectedSlope; continue; }

					const double CrownDistance = Canopy.Nearest(X, Y);
					if (CrownDistance < C.CanopyExclusion) { ++Row.RejectedCanopy; continue; }
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

					const double Patchy = Patch(In.Seed, X, Y, C.PatchSpanUU, 21);
					const double Density = C.Density
						* Smooth((Mask - C.MinMask) / 0.3)
						* FMath::Lerp(C.PatchFloor, 1.0, Smooth((Patchy - 0.25) / 0.5))
						* (1.0 - 0.45 * Smooth((Slope - 12.0) / 8.0))
						* (1.0 - 0.75 * Shade)
						* Trampled;
					if (Unit(Hash(In.Seed, GX, GY, 13)) >= Density) { ++Row.RejectedDensity; continue; }

					// Famille. Humidite d'abord (laiches), puis la pente decide haute ou basse ;
					// une seconde tache melange les hauteurs sur le plat : "hauteurs melees", pas un gazon.
					EFamily Family;
					const double WetDraw = C.SedgeWetness + 0.12 * (Unit(Hash(In.Seed, GX, GY, 14)) - 0.5);
					if (Wet >= WetDraw)
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
						* (Family == EFamily::MeadowTall ? FMath::Lerp(0.85, 1.0, Mask) : 1.0);
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
		for (const FPlacement& P : Row.Instances)
		{
			if (Out.Instances.Num() >= C.MaxInstances) { Out.bTruncated = true; break; }
			Out.Instances.Add(P);
			++Out.Counts[static_cast<int32>(P.Family)];
		}
		Row.Instances.Empty();
	}
	return true;
}
