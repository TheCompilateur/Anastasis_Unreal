#include "WorldView/AnastasisRuinDressing.h"

namespace AnastasisRuinDressing
{
namespace
{
	using AnastasisWorldView::FWorldVisualSnapshot;
	using AnastasisWorldView::FVisualTile;

	constexpr int32 ReferenceWidth = 96;
	constexpr int32 ReferenceHeight = 96;

	/** Same mix as AnastasisEcologicalDressing::Hash: one source of determinism, two users. */
	uint32 Hash(uint32 Seed, int32 X, int32 Y, uint32 Salt)
	{
		uint32 H = Seed ^ 0x9E3779B9u;
		H = (H ^ static_cast<uint32>(X)) * 0x85EBCA6Bu;
		H = (H ^ static_cast<uint32>(Y)) * 0xC2B2AE35u;
		H = (H ^ Salt) * 0x27D4EB2Fu;
		return H ^ (H >> 15);
	}

	double Unit(uint32 H) { return static_cast<double>(H) / 4294967296.0; }

	bool IsRuin(const FVisualTile& Tile)
	{
		return Tile.Type == AnastasisWorld::ETileType::Ruin;
	}

	/**
	 * Orientation of one site, from the SHAPE of its own tile cloud.
	 *
	 * A site's pieces must share an axis or the plan does not read, but a hashed axis
	 * would ignore the footprint the simulation already drew. The major axis of the tile
	 * cloud is free, deterministic, and makes the building lie along its own ruin patch
	 * instead of across it. Too round or too small a cloud has no axis to speak of, and
	 * only then do we fall back to the hash.
	 */
	double SiteYawDegrees(const TArray<int32>& Tiles, const FWorldVisualSnapshot& Source,
		double CentroidX, double CentroidY, uint32 Seed, int32 AnchorX, int32 AnchorY)
	{
		double Sxx = 0.0, Syy = 0.0, Sxy = 0.0;
		for (const int32 Index : Tiles)
		{
			const double DX = Source.Tiles[Index].X - CentroidX;
			const double DY = Source.Tiles[Index].Y - CentroidY;
			Sxx += DX * DX;
			Syy += DY * DY;
			Sxy += DX * DY;
		}
		// Elongation, 0 = a disc, 1 = a line. Below the threshold the cloud has no
		// meaningful direction and any axis we computed would be numerical noise.
		const double Trace = Sxx + Syy;
		const double Diff = FMath::Sqrt(FMath::Square(Sxx - Syy) + 4.0 * Sxy * Sxy);
		const double Elongation = Trace > KINDA_SMALL_NUMBER ? Diff / Trace : 0.0;
		if (Tiles.Num() < 3 || Elongation < 0.25)
		{
			return Unit(Hash(Seed, AnchorX, AnchorY, 0x51u)) * 360.0;
		}
		return FMath::RadiansToDegrees(0.5 * FMath::Atan2(2.0 * Sxy, Sxx - Syy));
	}
}

const TCHAR* PieceName(EPiece Piece)
{
	switch (Piece)
	{
	case EPiece::Soubassement: return TEXT("Soubassement");
	case EPiece::Angle:        return TEXT("Angle");
	case EPiece::Mur:          return TEXT("Mur");
	case EPiece::Foyer:        return TEXT("Foyer");
	case EPiece::Enclos:       return TEXT("Enclos");
	case EPiece::Reemploi:     return TEXT("Reemploi");
	default:                   return TEXT("?");
	}
}

bool Build(const FWorldVisualSnapshot& Source, FPlan& Out, FString& OutError)
{
	Out = FPlan{};
	OutError.Reset();

	// Same contract as AnastasisEcologicalDressing::Build: the full canonical snapshot,
	// so the plan is independent of whatever crop is being rendered.
	if (Source.SourceW != ReferenceWidth || Source.SourceH != ReferenceHeight
		|| Source.OriginX != 0 || Source.OriginY != 0
		|| Source.W != ReferenceWidth || Source.H != ReferenceHeight
		|| Source.Tiles.Num() != ReferenceWidth * ReferenceHeight)
	{
		OutError = TEXT("Source: expected full canonical 96x96 snapshot");
		return false;
	}
	for (int32 Index = 0; Index < Source.Tiles.Num(); ++Index)
	{
		const FVisualTile& Tile = Source.Tiles[Index];
		if (Tile.X != Index % Source.W || Tile.Y != Index / Source.W
			|| Tile.SourceIndex != Index || !FMath::IsFinite(Tile.Alt)
			|| static_cast<uint8>(Tile.Type) >= AnastasisWorld::TileTypeCount)
		{
			OutError = FString::Printf(
				TEXT("Source.Tiles[%d]: invalid coordinates, altitude or type"), Index);
			return false;
		}
	}

	const int32 W = Source.W;
	const int32 H = Source.H;
	TArray<int32> SiteOf;
	SiteOf.Init(INDEX_NONE, Source.Tiles.Num());

	TArray<TArray<int32>> Sites;
	TArray<int32> Stack;

	// Eight-connected: two ruin tiles touching at a corner belong to the same building,
	// and four-connectivity would split one footprint into two sites across a diagonal.
	constexpr int32 DX8[8] = {1, -1, 0, 0, 1, 1, -1, -1};
	constexpr int32 DY8[8] = {0, 0, 1, -1, 1, -1, 1, -1};

	for (int32 Start = 0; Start < Source.Tiles.Num(); ++Start)
	{
		if (SiteOf[Start] != INDEX_NONE || !IsRuin(Source.Tiles[Start]))
		{
			continue;
		}
		const int32 SiteId = Sites.Num();
		TArray<int32>& Site = Sites.AddDefaulted_GetRef();
		Stack.Reset();
		Stack.Push(Start);
		SiteOf[Start] = SiteId;
		while (Stack.Num() > 0)
		{
			const int32 Index = Stack.Pop();
			Site.Add(Index);
			const int32 X = Index % W;
			const int32 Y = Index / W;
			for (int32 N = 0; N < 8; ++N)
			{
				const int32 NX = X + DX8[N];
				const int32 NY = Y + DY8[N];
				if (NX < 0 || NY < 0 || NX >= W || NY >= H)
				{
					continue;
				}
				const int32 NIndex = NY * W + NX;
				if (SiteOf[NIndex] == INDEX_NONE && IsRuin(Source.Tiles[NIndex]))
				{
					SiteOf[NIndex] = SiteId;
					Stack.Push(NIndex);
				}
			}
		}
	}

	Out.SiteCount = Sites.Num();

	for (int32 SiteId = 0; SiteId < Sites.Num(); ++SiteId)
	{
		TArray<int32>& Site = Sites[SiteId];
		// Flood fill order depends on the stack; sort so the plan is reproducible.
		Site.Sort();
		const int32 N = Site.Num();
		Out.LargestSiteTiles = FMath::Max(Out.LargestSiteTiles, N);

		double CentroidX = 0.0, CentroidY = 0.0;
		for (const int32 Index : Site)
		{
			CentroidX += Source.Tiles[Index].X;
			CentroidY += Source.Tiles[Index].Y;
		}
		CentroidX /= N;
		CentroidY /= N;

		const int32 AnchorX = Source.Tiles[Site[0]].X;
		const int32 AnchorY = Source.Tiles[Site[0]].Y;
		const double Yaw = SiteYawDegrees(Site, Source, CentroidX, CentroidY,
			Source.Seed, AnchorX, AnchorY);

		// A lone tile is not a building. The simulation gives it Stone 8 -- so it is a
		// heap of reusable stone, and drawing a house plan on it would be a lie.
		if (N == 1)
		{
			++Out.IsolatedSites;
			FPlacement& P = Out.Instances.AddDefaulted_GetRef();
			P.SourceIndex = Site[0];
			P.Piece = EPiece::Reemploi;
			P.YawDegrees = Yaw;
			P.SiteId = SiteId;
			P.VisualSeed = Hash(Source.Seed, AnchorX, AnchorY, 0x11u);
			P.ScaleMultiplier = 0.85 + Unit(P.VisualSeed) * 0.30;
			continue;
		}

		// Rank tiles by distance to the centroid: the heart of the site carries the
		// base course, the rim carries the spill.
		TArray<int32> Ranked = Site;
		Ranked.Sort([&Source, CentroidX, CentroidY](int32 A, int32 B)
		{
			const double DA = FMath::Square(Source.Tiles[A].X - CentroidX)
				+ FMath::Square(Source.Tiles[A].Y - CentroidY);
			const double DB = FMath::Square(Source.Tiles[B].X - CentroidX)
				+ FMath::Square(Source.Tiles[B].Y - CentroidY);
			return DA < DB;
		});

		const int32 RimStart = FMath::Max(1, FMath::CeilToInt(N * 0.62));
		for (int32 Rank = 0; Rank < Ranked.Num(); ++Rank)
		{
			const int32 Index = Ranked[Rank];
			const FVisualTile& Tile = Source.Tiles[Index];
			const uint32 Seed = Hash(Source.Seed, Tile.X, Tile.Y, 0x21u + static_cast<uint32>(Rank));

			EPiece Piece;
			if (Rank == 0)
			{
				// Four tiles is the smallest patch that can carry a footprint; below
				// that only a corner survived.
				Piece = (N >= 4) ? EPiece::Soubassement : EPiece::Angle;
			}
			else if (Rank == 1 && N >= 5)
			{
				// "Foyer en pierre... coeur de la maison" -- it sits inside, next to the
				// core, never out on the rim.
				Piece = EPiece::Foyer;
			}
			else if (Rank >= RimStart)
			{
				Piece = EPiece::Reemploi;
			}
			else
			{
				// Enclosure walls belong to the bigger sites: you fence a holding, not
				// a two-room shell.
				Piece = (N >= 7 && Unit(Seed) < 0.35) ? EPiece::Enclos : EPiece::Mur;
			}

			FPlacement& P = Out.Instances.AddDefaulted_GetRef();
			P.SourceIndex = Index;
			P.Piece = Piece;
			// Every piece of the site shares the axis. This single line is the whole
			// difference between a plan and a scatter.
			P.YawDegrees = Yaw;
			P.SiteId = SiteId;
			P.VisualSeed = Seed;
			P.ScaleMultiplier = (Rank == 0) ? 1.0 : 0.88 + Unit(Seed) * 0.20;
		}
	}

	return true;
}
}
