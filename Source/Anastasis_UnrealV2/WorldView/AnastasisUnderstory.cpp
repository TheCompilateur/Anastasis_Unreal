#include "WorldView/AnastasisUnderstory.h"

#include "WorldView/AnastasisHumanGeography.h"

namespace AnastasisUnderstory
{
namespace
{
using AnastasisWorld::ETileType;
using AnastasisWorldView::FVisualTile;
using AnastasisWorldView::FWorldVisualSnapshot;

/**
 * Le sel entre AVANT les melanges et la sortie passe un finaliseur complet. Le hachage des autres
 * passes (sel xore au dernier pas) donne, pour deux sels voisins, deux tirages qui ne different
 * que d'une constante : le choix d'essence etait alors lie au tirage de presence.
 */
uint32 Hash(uint32 Seed, int32 X, int32 Y, uint32 Salt)
{
	uint32 H = Seed ^ 0x6C8E9CF5u ^ (Salt * 0x9E3779B9u);
	H = (H ^ static_cast<uint32>(X)) * 0x85EBCA6Bu;
	H = (H ^ static_cast<uint32>(Y)) * 0xC2B2AE35u;
	H ^= H >> 16; H *= 0x7FEB352Du;
	H ^= H >> 15; H *= 0x846CA68Bu;
	return H ^ (H >> 16);
}
double Unit(uint32 H) { return static_cast<double>(H) / 4294967296.0; }
double Smooth(double V) { V = FMath::Clamp(V, 0.0, 1.0); return V * V * (3.0 - 2.0 * V); }
double Rise(double A, double B, double X) { return Smooth((X - A) / (B - A)); }
double Fall(double A, double B, double X) { return 1.0 - Rise(A, B, X); }

/** Bruit de valeur lisse, en tuiles. */
double Noise(uint32 Seed, double X, double Y, uint32 Salt)
{
	const int32 IX = FMath::FloorToInt(X), IY = FMath::FloorToInt(Y);
	const double U = Smooth(X - IX), V = Smooth(Y - IY);
	return FMath::Lerp(
		FMath::Lerp(Unit(Hash(Seed, IX, IY, Salt)), Unit(Hash(Seed, IX + 1, IY, Salt)), U),
		FMath::Lerp(Unit(Hash(Seed, IX, IY + 1, Salt)), Unit(Hash(Seed, IX + 1, IY + 1, Salt)), U), V);
}

/** Part d'un type de tuile en (X, Y) tuiles, bilineaire entre centres : aucune marche de grille. */
double TypeShare(const FWorldVisualSnapshot& S, double X, double Y, ETileType Type)
{
	const double U = X - 0.5, V = Y - 0.5;
	const int32 IX = FMath::FloorToInt(U), IY = FMath::FloorToInt(V);
	const double FX = U - IX, FY = V - IY;
	const auto At = [&](int32 TX, int32 TY)
	{
		const FVisualTile* T = AnastasisWorldView::FindTile(S, FMath::Clamp(TX, 0, S.W - 1), FMath::Clamp(TY, 0, S.H - 1));
		return T && T->Type == Type ? 1.0 : 0.0;
	};
	return FMath::Lerp(FMath::Lerp(At(IX, IY), At(IX + 1, IY), FX), FMath::Lerp(At(IX, IY + 1), At(IX + 1, IY + 1), FX), FY);
}

/** Couronnes indexees par cases : distance au plus proche tronc, en rayons de sa couronne. */
struct FCrowns
{
	double Bucket = 2500.0;
	TMap<FIntPoint, TArray<FVector>> Cells;

	void Init(const TArray<FVector>& Canopy)
	{
		for (const FVector& C : Canopy)
		{
			if (!FMath::IsFinite(C.X) || !FMath::IsFinite(C.Y) || !(C.Z > 1.0)) continue;
			Cells.FindOrAdd(FIntPoint(FMath::FloorToInt(C.X / Bucket), FMath::FloorToInt(C.Y / Bucket))).Add(C);
		}
	}

	double Relative(double X, double Y) const
	{
		double Best = TNumericLimits<double>::Max();
		const FIntPoint Cell(FMath::FloorToInt(X / Bucket), FMath::FloorToInt(Y / Bucket));
		for (int32 DY = -1; DY <= 1; ++DY)
			for (int32 DX = -1; DX <= 1; ++DX)
				if (const TArray<FVector>* List = Cells.Find(Cell + FIntPoint(DX, DY)))
					for (const FVector& C : *List)
						Best = FMath::Min(Best, FVector2D::Distance(FVector2D(X, Y), FVector2D(C.X, C.Y)) / C.Z);
		return Best;
	}
};

struct FRockSpec { const TCHAR* Stem; int32 Variants; };
constexpr FRockSpec Rocks[] = {
	{TEXT("SM_Rock_Boulder"), 3}, {TEXT("SM_Rock_Low"), 3}, {TEXT("SM_Rock_Split"), 3}, {TEXT("SM_Rock_Massive"), 3},
	{TEXT("SM_Rock_Cluster"), 1}, {TEXT("SM_Rock_CliffFragment"), 3}, {TEXT("SM_Rock_Vertical"), 3},
};
static_assert(static_cast<int32>(UE_ARRAY_COUNT(Rocks)) == RockCount, "one spec per ERock");

constexpr const TCHAR* ShrubStems[] = {TEXT("SM_Shrub_Lentisk"), TEXT("SM_Shrub_KermesOak"), TEXT("SM_Shrub_Broom"), TEXT("SM_Shrub_Bramble")};
}

const TCHAR* KindName(EKind Kind)
{
	switch (Kind)
	{
	case EKind::Lentisk: return TEXT("Lentisk");
	case EKind::KermesOak: return TEXT("KermesOak");
	case EKind::Broom: return TEXT("Broom");
	case EKind::Bramble: return TEXT("Bramble");
	case EKind::Rock: return TEXT("Rock");
	default: return TEXT("Unknown");
	}
}

const TCHAR* RockName(ERock Rock)
{
	const int32 I = static_cast<int32>(Rock);
	return I >= 0 && I < RockCount ? Rocks[I].Stem : TEXT("Unknown");
}

int32 RockVariants(ERock Rock)
{
	const int32 I = static_cast<int32>(Rock);
	return I >= 0 && I < RockCount ? Rocks[I].Variants : 1;
}

FString ShrubMeshPath(EKind Kind, int32 Variant)
{
	const int32 I = FMath::Clamp(static_cast<int32>(Kind), 0, 3);
	const FString Name = FString::Printf(TEXT("%s_%02d"), ShrubStems[I], FMath::Clamp(Variant, 0, ShrubShapes - 1) + 1);
	return FString::Printf(TEXT("/Game/Anastasis/Vegetation/%s.%s"), *Name, *Name);
}

FString RockMeshPath(ERock Rock, int32 Variant)
{
	const int32 I = FMath::Clamp(static_cast<int32>(Rock), 0, RockCount - 1);
	const FString Name = FString::Printf(TEXT("%s_%02d"), Rocks[I].Stem, FMath::Clamp(Variant, 0, Rocks[I].Variants - 1) + 1);
	return FString::Printf(TEXT("/Game/Anastasis/Rock/%s.%s"), *Name, *Name);
}

bool Build(const FInputs& In, const FSettings& C, FPlan& Out, FString& Error)
{
	Out = FPlan{};
	Error.Reset();
	const FWorldVisualSnapshot* SourcePtr = In.Source;
	if (!SourcePtr || SourcePtr->W <= 0 || SourcePtr->H <= 0 || SourcePtr->Tiles.Num() != SourcePtr->W * SourcePtr->H)
	{
		Error = TEXT("Source: missing or inconsistent snapshot"); return false;
	}
	if (!In.SampleHeight) { Error = TEXT("SampleHeight: required"); return false; }
	const double Values[] = {C.CellUU, C.ProbeUU, C.WaterClearanceUU, C.MaxShrubSlopeDegrees, C.MaxRockSlopeDegrees,
		C.MaquisDensity, C.MaquisPatchTiles, C.ScrubBoost, C.ValleyKeep, C.CanopyKeep, C.BrambleDensity,
		C.RockMeadowDensity, C.RockSlopeDensity, C.RockStoneDensity, C.RockAltitudeDensity, C.RockClusterTiles,
		C.BasinClearRadiusTiles, In.WaterPlaneZ, In.AltitudeSpanUU};
	for (const double V : Values)
	{
		if (!FMath::IsFinite(V)) { Error = TEXT("Settings: non-finite value"); return false; }
	}
	if (C.CellUU < 50.0 || C.ProbeUU <= 0.0 || C.MaquisPatchTiles <= 0.0 || C.RockClusterTiles <= 0.0
		|| In.AltitudeSpanUU <= 0.0 || C.MaxInstances <= 0)
	{
		Error = TEXT("Settings: value outside supported range"); return false;
	}

	const FWorldVisualSnapshot& S = *SourcePtr;
	const double TileUU = AnastasisWorldView::TileWorldSize * S.SpatialScale;
	const bool bHG = S.bHumanGeography && S.Seed == AnastasisWorldView::ReferenceSeed;
	FCrowns Crowns;
	Crowns.Init(In.Canopy);

	const int32 NX = FMath::CeilToInt(S.W * TileUU / C.CellUU);
	const int32 NY = FMath::CeilToInt(S.H * TileUU / C.CellUU);
	FPlan Result;
	for (int32 GY = 0; GY < NY && !Result.bTruncated; ++GY)
	{
		for (int32 GX = 0; GX < NX; ++GX)
		{
			++Result.Cells;
			const double X = (GX + Unit(Hash(S.Seed, GX, GY, 1))) * C.CellUU;
			const double Y = (GY + Unit(Hash(S.Seed, GX, GY, 2))) * C.CellUU;
			const double TX = X / TileUU, TY = Y / TileUU;
			const FVisualTile* Tile = AnastasisWorldView::FindTile(S, FMath::FloorToInt(TX), FMath::FloorToInt(TY));
			if (!Tile) continue;
			// Jamais sur un champ, une ruine (les lieux composes s'en chargent) ni l'eau du sim.
			if (Tile->Type == ETileType::Field || Tile->Type == ETileType::Ruin || Tile->Type == ETileType::Water)
			{
				++Result.RejectedReserved; continue;
			}

			double Z = 0.0;
			if (!In.SampleHeight(X, Y, Z) || !FMath::IsFinite(Z)) continue;
			double Water = In.WaterPlaneZ;
			if (In.SampleWaterHeight && !In.SampleWaterHeight(X, Y, Water)) Water = In.WaterPlaneZ;
			if (!FMath::IsFinite(Water) || Z <= Water + C.WaterClearanceUU) { ++Result.RejectedWater; continue; }

			double East = Z, West = Z, North = Z, South = Z;
			In.SampleHeight(X + C.ProbeUU, Y, East);
			In.SampleHeight(X - C.ProbeUU, Y, West);
			In.SampleHeight(X, Y + C.ProbeUU, North);
			In.SampleHeight(X, Y - C.ProbeUU, South);
			const FVector Normal = FVector(-(East - West) / (2.0 * C.ProbeUU), -(North - South) / (2.0 * C.ProbeUU), 1.0).GetSafeNormal();
			const double Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Normal.Z, -1.0, 1.0)));
			if (!FMath::IsFinite(Slope)) continue;

			// Reserves humaines : bassin du village, lit de riviere ecrite, route du col.
			double Reserve = 1.0, Valley = 0.0;
			if (In.bHasBasin && C.BasinClearRadiusTiles > 0.0)
			{
				const double D = FVector2D(X - In.Basin.X, Y - In.Basin.Y).Size() / TileUU;
				Reserve *= Smooth((D - C.BasinClearRadiusTiles) / 4.0);
			}
			if (bHG)
			{
				const AnastasisHumanGeography::FSample Geo = AnastasisHumanGeography::Evaluate(TX, TY, 0.0);
				Reserve *= (1.0 - Smooth(Geo.RiverWeight / 0.5)) * (1.0 - Smooth(Geo.RoadWeight / 0.3));
				Valley = Geo.ValleyWeight;
			}
			if (Reserve <= 0.0) { ++Result.RejectedReserved; continue; }

			const double Alt = FMath::Clamp((Z - In.WaterPlaneZ) / In.AltitudeSpanUU, 0.0, 1.0);
			double Riparian = 0.0;
			if (In.SampleRiparian && In.SampleRiparian(X, Y, Riparian) && FMath::IsFinite(Riparian)) Riparian = FMath::Clamp(Riparian, 0.0, 1.0);
			else Riparian = 0.0;
			const double Damp = FMath::Max(Riparian, FMath::Clamp(Tile->Wetness, 0.0, 1.0));
			const double Crown = Crowns.Relative(X, Y);
			const bool bUnder = Crown < 0.75;
			// Anneau de lisiere : de 0.8 a 2.2 rayons de couronne, plein entre 1.2 et 1.4.
			const double EdgeRing = Rise(0.8, 1.2, Crown) * Fall(1.4, 3.0, Crown);
			const double Open = Rise(2.0, 4.0, Crown);
			const double Stone = TypeShare(S, TX, TY, ETileType::Stone);
			const double Scrub = TypeShare(S, TX, TY, ETileType::Scrub);

			// Rochers.
			const double RockCluster = 0.25 + 1.5 * Rise(0.45, 0.85, Noise(S.Seed, TX / C.RockClusterTiles, TY / C.RockClusterTiles, 31));
			const double RockP = Slope > C.MaxRockSlopeDegrees ? 0.0
				: (C.RockMeadowDensity + C.RockSlopeDensity * Rise(14.0, 36.0, Slope) + C.RockStoneDensity * Stone
					+ C.RockAltitudeDensity * Rise(0.5, 0.9, Alt)) * RockCluster * Reserve;

			// Ronces : lisieres des couronnes, bande riveraine.
			const double RiverBand = Rise(0.15, 0.35, Riparian) * Fall(0.75, 0.9, Riparian);
			const double BrambleP = Slope > 30.0 ? 0.0
				: C.BrambleDensity * FMath::Max(0.6 * EdgeRing, RiverBand) * Fall(0.65, 0.85, Alt) * Reserve;

			// Maquis.
			const double Patch = FMath::Lerp(0.15, 1.0, Rise(0.35, 0.75, Noise(S.Seed, TX / C.MaquisPatchTiles, TY / C.MaquisPatchTiles, 32)));
			const double ShrubP = Slope > C.MaxShrubSlopeDegrees ? 0.0
				: C.MaquisDensity
					* FMath::Lerp(1.0, 0.2, Rise(0.55, 0.8, Alt))
					* (1.0 - 0.7 * Rise(0.35, 0.75, Damp))
					* (0.5 + 0.5 * Rise(0.0, 6.0, Slope)) * Fall(30.0, 38.0, Slope)
					* Patch * (1.0 + C.ScrubBoost * Scrub)
					* (bUnder ? C.CanopyKeep : 1.0) * (1.0 + 0.8 * EdgeRing)
					* FMath::Lerp(1.0, C.ValleyKeep, Rise(0.3, 0.7, Valley))
					* Reserve;

			const double Draw = Unit(Hash(S.Seed, GX, GY, 3));
			const double Scale = FMath::Min(1.0, 0.95 / FMath::Max(RockP + BrambleP + ShrubP, 0.95));
			FInstance P;
			const double Size = Unit(Hash(S.Seed, GX, GY, 4));
			if (Draw < RockP * Scale)
			{
				P.Kind = EKind::Rock;
				const double Steep = Rise(25.0, 45.0, Slope);
				const double Big = FMath::Max(Steep, Stone);
				P.HeightM = FMath::Lerp(FMath::Lerp(0.35, 1.0, Size), FMath::Lerp(0.6, 2.6, Size * Size), Big);
				const double Pick = Unit(Hash(S.Seed, GX, GY, 5));
				if (Steep > 0.5) P.Rock = P.HeightM > 1.4 ? (Pick < 0.5 ? ERock::CliffFragment : ERock::Vertical) : ERock::Split;
				else if (P.HeightM < 0.7) P.Rock = Pick < 0.45 ? ERock::Low : (Pick < 0.75 ? ERock::Boulder : ERock::Cluster);
				else P.Rock = Pick < 0.4 ? ERock::Boulder : (Pick < 0.7 ? ERock::Split : ERock::Massive);
				P.Variant = static_cast<int32>(Hash(S.Seed, GX, GY, 6) % static_cast<uint32>(RockVariants(P.Rock)));
			}
			else if (Draw < (RockP + BrambleP) * Scale)
			{
				P.Kind = EKind::Bramble;
				P.HeightM = FMath::Lerp(0.6, 1.4, Size) * (1.0 + 0.2 * EdgeRing);
				P.Variant = static_cast<int32>(Hash(S.Seed, GX, GY, 6) % ShrubShapes);
			}
			else if (Draw < (RockP + BrambleP + ShrubP) * Scale)
			{
				// Essence du maquis : lentisque au bas et au sec, kermes sur la pente et la roche,
				// genet dans l'ouvert et en lisiere.
				const double WL = Fall(0.25, 0.5, Alt) * (1.0 - 0.6 * Rise(0.3, 0.6, Damp)) + 0.02;
				const double WK = (0.3 + 0.7 * Rise(8.0, 23.0, Slope)) * Fall(0.55, 0.8, Alt) * (0.5 + Stone);
				const double WB = 0.6 * (0.8 * EdgeRing + 0.4 * Open) * Fall(0.6, 0.85, Alt);
				const double Pick = Unit(Hash(S.Seed, GX, GY, 7)) * (WL + WK + WB);
				P.Kind = Pick < WL ? EKind::Lentisk : (Pick < WL + WK ? EKind::KermesOak : EKind::Broom);
				const double Low = P.Kind == EKind::Lentisk ? 1.0 : (P.Kind == EKind::KermesOak ? 0.6 : 1.2);
				const double High = P.Kind == EKind::Lentisk ? 2.5 : (P.Kind == EKind::KermesOak ? 1.8 : 2.8);
				// Plus bas sur la pente seche et sous le couvert.
				P.HeightM = FMath::Lerp(Low, High, Size) * (bUnder ? 0.8 : 1.0) * (1.0 - 0.2 * Rise(20.0, 35.0, Slope));
				P.Variant = static_cast<int32>(Hash(S.Seed, GX, GY, 6) % ShrubShapes);
			}
			else
			{
				continue;
			}
			P.Ground = FVector(X, Y, Z);
			P.Normal = Normal;
			P.Yaw = 360.0 * Unit(Hash(S.Seed, GX, GY, 8));
			P.SlopeDegrees = Slope;
			P.Dryness = FMath::Clamp(1.0 - 1.5 * Damp, 0.0, 1.0);
			P.Jitter = Unit(Hash(S.Seed, GX, GY, 9));
			++Result.Counts[static_cast<int32>(P.Kind)];
			Result.Instances.Add(P);
			if (Result.Instances.Num() >= C.MaxInstances) { Result.bTruncated = true; break; }
		}
	}
	Out = MoveTemp(Result);
	return true;
}
}
