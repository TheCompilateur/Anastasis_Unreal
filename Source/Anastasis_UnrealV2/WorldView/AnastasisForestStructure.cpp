#include "WorldView/AnastasisForestStructure.h"
#include "WorldView/AnastasisHumanGeography.h"

namespace AnastasisForestStructure
{
namespace
{
using namespace AnastasisWorldView;
using AnastasisEcologicalDressing::ELayer;
using AnastasisEcologicalDressing::FPlacement;
using AnastasisWorld::ETileType;

uint32 Hash(uint32 Seed, int32 X, int32 Y, uint32 Salt)
{
	uint32 H = Seed ^ 0x9E3779B9u;
	H = (H ^ static_cast<uint32>(X)) * 0x85EBCA6Bu;
	H = (H ^ static_cast<uint32>(Y)) * 0xC2B2AE35u;
	H = (H ^ Salt) * 0x27D4EB2Fu;
	return H ^ (H >> 15);
}

double Unit(uint32 H) { return static_cast<double>(H) / 4294967296.0; }
double Smooth(double V) { V = FMath::Clamp(V, 0.0, 1.0); return V * V * (3.0 - 2.0 * V); }

/** Large masses. Same value-noise family as the dressing, different salt. */
double Field(uint32 Seed, double X, double Y)
{
	const int32 IX = FMath::FloorToInt(X), IY = FMath::FloorToInt(Y);
	const double U = Smooth(X - IX), V = Smooth(Y - IY);
	return FMath::Lerp(
		FMath::Lerp(Unit(Hash(Seed, IX, IY, 91)), Unit(Hash(Seed, IX + 1, IY, 91)), U),
		FMath::Lerp(Unit(Hash(Seed, IX, IY + 1, 91)), Unit(Hash(Seed, IX + 1, IY + 1, 91)), U), V);
}

bool Plantable(ETileType Type, bool bMacro)
{
	return Type == ETileType::Forest || Type == ETileType::Grass || Type == ETileType::Scrub
		|| (bMacro && Type == ETileType::Stone);
}

struct FOpening
{
	FVector2D Center = FVector2D::ZeroVector;
	double Radius = 0.0;
	int32 Kind = 0;
	uint32 Rank = 0;
};

double RadiusFor(int32 Kind, double Jitter)
{
	if (Kind == 2) return 4500.0 + 3000.0 * Jitter;
	if (Kind == 1) return 1600.0 + 1600.0 * Jitter;
	return 350.0 + 280.0 * Jitter;
}

/** Irregular outline. The angle samples the field so the edge is not a circle. */
double Outline(uint32 Seed, const FOpening& Opening, const FVector2D& Point, double TileUU)
{
	const double Angle = FMath::Atan2(Point.Y - Opening.Center.Y, Point.X - Opening.Center.X);
	const double Around = (Angle + PI) / (2.0 * PI);
	const double Site = (Opening.Center.X + Opening.Center.Y) / FMath::Max(TileUU, 1.0);
	const double Noise = Field(Seed ^ 0x51A7u, Around * 5.0 + Opening.Kind, Site * 0.017);
	return 0.58 + 0.62 * Noise;
}

void MakeOpenings(const FWorldVisualSnapshot& S, double TileUU, TArray<FOpening>& Out,
	int32& Small, int32& Medium, int32& Large)
{
	Out.Reset();
	Small = Medium = Large = 0;
	constexpr int32 Cell = 22;
	const int32 NX = FMath::Max((S.W + Cell - 1) / Cell, 1);
	const int32 NY = FMath::Max((S.H + Cell - 1) / Cell, 1);
	for (int32 GY = 0; GY < NY; ++GY)
		for (int32 GX = 0; GX < NX; ++GX)
		{
			const double Roll = Unit(Hash(S.Seed, GX, GY, 703));
			int32 Kind = -1;
			if (Roll < 0.18) Kind = 2;
			else if (Roll < 0.40) Kind = 1;
			else if (Roll < 0.72) Kind = 0;
			if (Kind < 0) continue;
			const double Jx = Unit(Hash(S.Seed, GX, GY, 701));
			const double Jy = Unit(Hash(S.Seed, GX, GY, 702));
			FOpening Opening;
			Opening.Center = FVector2D(
				(GX * Cell + (0.18 + 0.64 * Jx) * Cell) * TileUU,
				(GY * Cell + (0.18 + 0.64 * Jy) * Cell) * TileUU);
			Opening.Radius = RadiusFor(Kind, Unit(Hash(S.Seed, GX, GY, 704)));
			Opening.Kind = Kind;
			Opening.Rank = Hash(S.Seed, GX, GY, 705);
			Out.Add(Opening);
		}
	Out.Sort([](const FOpening& A, const FOpening& B) { return A.Rank < B.Rank; });
	int32 KeptLarge = 0;
	for (FOpening& Opening : Out)
	{
		if (Opening.Kind != 2) continue;
		if (KeptLarge < 2) ++KeptLarge;
		else Opening.Kind = 1;
	}
	if (KeptLarge == 0 && Out.Num() > 0)
	{
		Out[0].Kind = 2;
		Out[0].Radius = RadiusFor(2, 0.4);
		KeptLarge = 1;
	}
	for (const FOpening& Opening : Out)
	{
		if (Opening.Kind == 2) ++Large;
		else if (Opening.Kind == 1) ++Medium;
		else ++Small;
	}
}

struct FWork
{
	FPlacement Placement;
	FNote Note;
	bool bDrop = false;
	int32 Opening = INDEX_NONE;
	bool bInterior = false;
	double KeepRoll = 0.0;
};

FVector2D Envelope(ELayer Layer, const FAnastasisForestDressingSettings& C)
{
	if (Layer == ELayer::Canopy) return C.CanopyScale;
	if (Layer == ELayer::Secondary) return C.SecondaryScale;
	return C.YoungScale;
}

double ScaleOf(ELayer Layer, double Bias, const FAnastasisForestDressingSettings& C, double HeightMul)
{
	const FVector2D Span = Envelope(Layer, C);
	return FMath::Lerp(Span.X, Span.Y, FMath::Clamp(Bias, 0.0, 1.0)) * HeightMul;
}

bool CrestOf(const FWorldVisualSnapshot& S, const FPlacement& P, float HillsideMax)
{
	if (!S.Tiles.IsValidIndex(P.SourceIndex)) return false;
	const auto& Home = S.Tiles[P.SourceIndex];
	double Sum = 0.0;
	int32 Count = 0;
	for (int32 DY = -2; DY <= 2; ++DY)
		for (int32 DX = -2; DX <= 2; ++DX)
		{
			if (DX == 0 && DY == 0) continue;
			const FVisualTile* Neighbor = FindTile(S, Home.X + DX, Home.Y + DY);
			if (!Neighbor) continue;
			Sum += Neighbor->Alt;
			++Count;
		}
	if (Count == 0) return false;
	const double Mean = Sum / static_cast<double>(Count);
	return Home.Alt > Mean + 0.04
		&& P.SlopeDegrees > 8.0
		&& P.SlopeDegrees < HillsideMax;
}

bool SiteAllows(const FWorldVisualSnapshot& S, double X, double Y, bool bMacro, double TileUU, int32& OutSource)
{
	const FVisualTile* Tile = FindTile(S, FMath::FloorToInt(X / TileUU), FMath::FloorToInt(Y / TileUU));
	if (!Tile || !Plantable(Tile->Type, bMacro)) return false;
	if (S.bHumanGeography && S.Seed == ReferenceSeed)
	{
		const auto Geo = AnastasisHumanGeography::Evaluate(X / TileUU, Y / TileUU, 0.0);
		if (Geo.ValleyWeight > 0.55 || Geo.RiverWeight > 0.35 || Geo.RoadWeight > 0.30) return false;
	}
	OutSource = Tile->SourceIndex;
	return true;
}
}

bool Shape(
	const FWorldVisualSnapshot& S,
	const FAnastasisForestDressingSettings& C,
	bool bMacroHeights,
	AnastasisEcologicalDressing::FPlan& InOut,
	TArray<FNote>& OutNotes,
	FReport& OutReport,
	FString& Error,
	bool bNaturalHistory)
{
	OutNotes.Reset();
	OutReport = FReport{};
	Error.Reset();
	OutReport.Before = InOut.Instances.Num();
	if (!FMath::IsFinite(S.SpatialScale) || S.SpatialScale <= 0.0)
	{
		Error = TEXT("ForestStructure: invalid SpatialScale");
		return false;
	}
	const double TileUU = TileWorldSize * S.SpatialScale;
	const double HeightMul = bMacroHeights ? C.HeightMultiplier : 1.0;
	const double BaseSpacing = bMacroHeights ? C.TrunkSpacingUU : C.MinimumSpacing * TileUU;
	const double PackedSpacing = BaseSpacing * 0.64;
	if (InOut.Instances.Num() == 0)
	{
		return true;
	}

	TArray<FOpening> Openings;
	MakeOpenings(S, TileUU, Openings, OutReport.SmallClearings, OutReport.MediumClearings, OutReport.LargeClearings);

	TArray<FWork> Work;
	Work.Reserve(InOut.Instances.Num());
	for (const FPlacement& SourcePlacement : InOut.Instances)
	{
		FWork Item;
		Item.Placement = SourcePlacement;
		Item.KeepRoll = Unit(Hash(SourcePlacement.VisualSeed, 0, 0, 440));
		const double TX = SourcePlacement.Ground.X / TileUU;
		const double TY = SourcePlacement.Ground.Y / TileUU;

		// Age of the stand. Wide field first, a shorter octave so the boundary wanders.
		const double AgeWide = Field(S.Seed ^ 0xA6E1u, TX / 26.0, TY / 26.0);
		const double AgeLocal = Field(S.Seed ^ 0xA6E2u, TX / 11.0, TY / 13.0);
		const double Age = FMath::Clamp(AgeWide * 0.75 + AgeLocal * 0.25, 0.0, 1.0);
		const double Disturb = Field(S.Seed ^ 0xA6E3u, TX / 7.0, TY / 7.0);

		// Local clump field. High values are groups, low values are the gaps between them.
		const double Clump = Field(S.Seed ^ 0xC100u, TX / 4.2, TY / 4.2);
		Item.Note.Cluster = Clump;

		// Soft corridors: a thin band of an elongated field, never a ruled line.
		const double Along = Field(S.Seed ^ 0xC0A1u, TX / 36.0, TY / 9.0);
		const double Across = Field(S.Seed ^ 0xC0A2u, TX / 9.0, TY / 36.0);
		Item.Note.bCorridor = FMath::Min(FMath::Abs(Along - 0.5), FMath::Abs(Across - 0.5)) < 0.035;
		Item.Note.bCrest = CrestOf(S, SourcePlacement, C.HillsideMaxSlope);

		double Best = 99.0;
		for (int32 Index = 0; Index < Openings.Num(); ++Index)
		{
			const double Factor = Outline(S.Seed, Openings[Index], FVector2D(SourcePlacement.Ground.X, SourcePlacement.Ground.Y), TileUU);
			const double Reach = Openings[Index].Radius * Factor;
			if (Reach <= 1.0) continue;
			const double Depth = FVector2D::Distance(
				FVector2D(SourcePlacement.Ground.X, SourcePlacement.Ground.Y), Openings[Index].Center) / Reach;
			if (Depth < Best)
			{
				Best = Depth;
				Item.Opening = Index;
			}
		}
		Item.bInterior = Item.Opening != INDEX_NONE && Best < 1.0;
		Item.Note.bClearingBorder = Item.Opening != INDEX_NONE && !Item.bInterior && Best < 1.42;
		if (Item.bInterior || Item.Note.bClearingBorder)
		{
			Item.Note.Stand = EStand::Clearing;
			Item.Note.bCorridor = false;
		}
		else if (Disturb > 0.86 && (!bNaturalHistory || Item.Note.bCrest || SourcePlacement.SlopeDegrees > 24.0))
			Item.Note.Stand = EStand::Disturbed;
		else if (Age < 0.38)
			Item.Note.Stand = EStand::Young;
		else if (Age > 0.52 && (!bNaturalHistory || (!Item.Note.bCrest && SourcePlacement.SlopeDegrees < 24.0)))
			Item.Note.Stand = EStand::Old;
		else
			Item.Note.Stand = EStand::Mature;

		// P2 already opened the mass (stand, glade, fringe). This pass does not
		// clear it again. It keeps almost every trunk, then retargets age.
		// Real removals are a drawn clearing, a crest, or a soft corridor.
		const double Pack = Smooth((Clump - 0.60) / 0.18);
		double Keep = 0.98;
		switch (Item.Note.Stand)
		{
		case EStand::Old: Keep = 0.94; break;
		case EStand::Disturbed: Keep = 0.72; break;
		case EStand::Clearing: Keep = Item.Note.bClearingBorder ? 0.90 : 0.0; break;
		default: break;
		}
		if (Item.Note.bCrest) Keep = FMath::Min(Keep, 0.62);
		if (Item.Note.bCorridor && Item.Note.Stand != EStand::Clearing) Keep = FMath::Min(Keep, 0.70);
		if (SourcePlacement.SlopeDegrees > 40.0) Keep = FMath::Min(Keep, 0.80);
		// Gaps between clumps. Young stands stay even; the openings belong to
		// mature and old forest, which is where a group has to be readable.
		if (Clump < 0.38 && !SourcePlacement.bLone
			&& Item.Note.Stand != EStand::Clearing && Item.Note.Stand != EStand::Young)
			Keep = FMath::Min(Keep, 0.48);
		Item.Note.bPacked = Pack > 0.45 && !Item.bInterior && !Item.Note.bClearingBorder && !SourcePlacement.bLone;
		Item.bDrop = !SourcePlacement.bLone && !Item.bInterior && Item.KeepRoll >= Keep;
		Work.Add(Item);
	}

	// A clearing keeps a few isolated trunks. The rest of the interior goes.
	for (int32 Index = 0; Index < Openings.Num(); ++Index)
	{
		TArray<int32> Inside;
		for (int32 Tree = 0; Tree < Work.Num(); ++Tree)
			if (Work[Tree].bInterior && Work[Tree].Opening == Index) Inside.Add(Tree);
		const int32 Allowance = Openings[Index].Kind == 2 ? 3 : Openings[Index].Kind == 1 ? 2 : 1;
		Inside.Sort([&](int32 A, int32 B)
		{
			return Work[A].Placement.VisualSeed < Work[B].Placement.VisualSeed;
		});
		for (int32 N = 0; N < Inside.Num(); ++N)
		{
			FWork& Tree = Work[Inside[N]];
			if (N < Allowance || Tree.Placement.bLone)
			{
				Tree.bDrop = false;
				Tree.Note.bClearingInterior = true;
			}
			else Tree.bDrop = true;
		}
	}

	for (FWork& Tree : Work)
	{
		if (Tree.bDrop || Tree.Placement.bLone) continue;
		const double Roll = Unit(Hash(Tree.Placement.VisualSeed, 0, 0, 441));
		ELayer Layer = ELayer::Canopy;
		double Bias = Roll;
		switch (Tree.Note.Stand)
		{
		case EStand::Young:
			Layer = Roll < 0.72 ? ELayer::Young : Roll < 0.94 ? ELayer::Secondary : ELayer::Canopy;
			Bias = Roll * 0.85;
			break;
		case EStand::Old:
			Layer = Roll < 0.06 ? ELayer::Young : Roll < 0.24 ? ELayer::Secondary : ELayer::Canopy;
			Bias = Layer == ELayer::Canopy ? 0.45 + 0.55 * Roll : Roll;
			break;
		case EStand::Disturbed:
			Layer = Roll < 0.50 ? ELayer::Young : Roll < 0.75 ? ELayer::Secondary : ELayer::Canopy;
			Bias = Roll * 0.9;
			break;
		case EStand::Clearing:
			if (Tree.Note.bClearingInterior)
			{
				Layer = Roll < 0.55 ? ELayer::Secondary : ELayer::Canopy;
				Bias = 0.35 + 0.45 * Roll;
			}
			else
			{
				Layer = Roll < 0.78 ? ELayer::Young : ELayer::Secondary;
				Bias = Roll * 0.7;
			}
			break;
		default:
			Layer = Roll < 0.12 ? ELayer::Young : Roll < 0.42 ? ELayer::Secondary : ELayer::Canopy;
			Bias = 0.2 + 0.8 * Roll;
			break;
		}
		// Existing forest margin and clearing borders carry younger cohorts; no extra trunks.
		if (bNaturalHistory && Tree.Note.bCorridor && Layer == ELayer::Canopy)
		{
			Layer = Roll < 0.55 ? ELayer::Young : ELayer::Secondary;
			Bias = Roll * 0.8;
		}
		if (Tree.Note.bCrest && Layer == ELayer::Canopy)
		{
			Layer = Roll < 0.45 ? ELayer::Secondary : ELayer::Canopy;
			Bias = Roll * 0.55;
		}
		Tree.Placement.Layer = Layer;
		double Scale = ScaleOf(Layer, Bias, C, HeightMul);
		if (Tree.Placement.SlopeDegrees > 36.0)
		{
			const double Floor = Envelope(Layer, C).X * HeightMul;
			Scale = FMath::Lerp(Scale, Floor, 0.35);
		}
		Tree.Placement.ScaleMultiplier = Scale;
		// P1 multiplies the species' real height by Maturity. ScaleMultiplier
		// only remains for a mesh that has no height range.
		Tree.Placement.Maturity = FMath::Clamp(Scale / (C.CanopyScale.Y * HeightMul), 0.20, 1.0);

		// Walk a short way up the clump gradient. One step is not enough to
		// gather a group; three capped steps stay on ground Build already accepted.
		if (Tree.Note.bPacked && !Tree.Note.bCrest)
		{
			const double StepScale = Tree.Note.Stand == EStand::Young ? 0.65
				: Tree.Note.Stand == EStand::Old ? 0.45 : 1.0;
			FVector2D XY(Tree.Placement.Ground.X, Tree.Placement.Ground.Y);
			const FVector2D Origin = XY;
			for (int32 Step = 0; Step < 4; ++Step)
			{
				const double SX = XY.X / TileUU, SY = XY.Y / TileUU;
				constexpr double Eps = 0.35;
				const double Dx = Field(S.Seed ^ 0xC100u, (SX + Eps) / 4.2, SY / 4.2)
					- Field(S.Seed ^ 0xC100u, (SX - Eps) / 4.2, SY / 4.2);
				const double Dy = Field(S.Seed ^ 0xC100u, SX / 4.2, (SY + Eps) / 4.2)
					- Field(S.Seed ^ 0xC100u, SX / 4.2, (SY - Eps) / 4.2);
				FVector2D Gradient(Dx, Dy);
				if (Gradient.SizeSquared() < 1.0e-8) break;
				Gradient.Normalize();
				const FVector2D Next = XY + Gradient * (220.0 * StepScale);
				if (FVector2D::Distance(Next, Origin) > 640.0) break;
				int32 SourceIndex = Tree.Placement.SourceIndex;
				if (!SiteAllows(S, Next.X, Next.Y, bMacroHeights, TileUU, SourceIndex)) break;
				XY = Next;
				Tree.Placement.SourceIndex = SourceIndex;
			}
			if (!XY.Equals(Origin, 1.0))
			{
				Tree.Placement.Ground.X = XY.X;
				Tree.Placement.Ground.Y = XY.Y;
				++OutReport.Moved;
			}
		}
	}

	TArray<FWork> Kept;
	Kept.Reserve(Work.Num());
	for (FWork& Tree : Work)
		if (!Tree.bDrop) Kept.Add(MoveTemp(Tree));

	const double Cell = FMath::Max(PackedSpacing, 1.0);
	TMap<FIntPoint, TArray<int32>> Grid;
	for (int32 Index = 0; Index < Kept.Num(); ++Index)
	{
		const FVector& G = Kept[Index].Placement.Ground;
		Grid.FindOrAdd(FIntPoint(FMath::FloorToInt(G.X / Cell), FMath::FloorToInt(G.Y / Cell))).Add(Index);
	}
	TArray<bool> Drop;
	Drop.Init(false, Kept.Num());
	for (int32 Index = 0; Index < Kept.Num(); ++Index)
	{
		if (Drop[Index]) continue;
		const FWork& A = Kept[Index];
		const FIntPoint Home(
			FMath::FloorToInt(A.Placement.Ground.X / Cell),
			FMath::FloorToInt(A.Placement.Ground.Y / Cell));
			const double NeedA = A.Placement.bLone ? PackedSpacing
				: A.Note.bClearingInterior ? BaseSpacing * 2.2
				: A.Note.Stand == EStand::Old ? BaseSpacing * 1.15
				: A.Note.bPacked ? PackedSpacing
				: A.Note.bCorridor ? BaseSpacing * 1.2
				: BaseSpacing;
		for (int32 DY = -2; DY <= 2; ++DY)
			for (int32 DX = -2; DX <= 2; ++DX)
			{
				const TArray<int32>* Neighbors = Grid.Find(Home + FIntPoint(DX, DY));
				if (!Neighbors) continue;
				for (int32 Other : *Neighbors)
				{
					if (Other <= Index || Drop[Other]) continue;
					const FWork& B = Kept[Other];
					const double NeedB = B.Placement.bLone ? PackedSpacing
						: B.Note.bClearingInterior ? BaseSpacing * 2.2
						: B.Note.Stand == EStand::Old ? BaseSpacing * 1.15
						: B.Note.bPacked ? PackedSpacing
						: B.Note.bCorridor ? BaseSpacing * 1.2
						: BaseSpacing;
					const double Limit = FMath::Max3(PackedSpacing, NeedA, NeedB);
					if (FVector::DistSquared2D(A.Placement.Ground, B.Placement.Ground) >= Limit * Limit) continue;
					const double PriA = A.Placement.ScaleMultiplier + (A.Placement.bLone ? 200.0 : 0.0) + (A.Note.bClearingInterior ? 100.0 : 0.0);
					const double PriB = B.Placement.ScaleMultiplier + (B.Placement.bLone ? 200.0 : 0.0) + (B.Note.bClearingInterior ? 100.0 : 0.0);
					Drop[PriA < PriB ? Index : Other] = true;
					if (Drop[Index]) break;
				}
				if (Drop[Index]) break;
			}
	}

	InOut.Instances.Reset();
	OutNotes.Reserve(Kept.Num());
	for (int32 Index = 0; Index < Kept.Num(); ++Index)
	{
		if (Drop[Index]) continue;
		const int32 Layer = static_cast<int32>(Kept[Index].Placement.Layer);
		const int32 Stand = static_cast<int32>(Kept[Index].Note.Stand);
		if (Layer >= 0 && Layer < 3) ++OutReport.ByLayer[Layer];
		if (Stand >= 0 && Stand < 5) ++OutReport.ByStand[Stand];
		InOut.Instances.Add(Kept[Index].Placement);
		OutNotes.Add(Kept[Index].Note);
	}
	OutReport.After = InOut.Instances.Num();
	return true;
}
}
