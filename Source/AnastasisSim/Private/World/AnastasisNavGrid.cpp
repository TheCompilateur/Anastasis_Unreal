#include "World/AnastasisNavGrid.h"

#include "Core/AnastasisJsNumeric.h"

#include <limits>

namespace AnastasisNav
{
	using AnastasisWorld::ETileType;
	using AnastasisWorld::EResource;
	using AnastasisWorld::FTile;
	using AnastasisWorld::FWorld;

	bool IsStandingTreeTile(const FTile& Tile)
	{
		return Tile.Type == ETileType::Forest
			&& Tile.Resource == EResource::Wood
			&& Tile.Amount > 0;
	}

	double TerrainMoveCostOf(const FTile* Tile, const double* RoadPathCost)
	{
		if (Tile == nullptr)
		{
			return Infinity;
		}
		if (Tile->Type == ETileType::Water)
		{
			return Infinity;
		}

		// `Number.isFinite` cote JS: un cout de route non fini retombe sur le
		// terrain, il ne contamine pas la grille.
		if (RoadPathCost != nullptr && FMath::IsFinite(*RoadPathCost))
		{
			return FMath::Max(0.35, *RoadPathCost);
		}

		switch (Tile->Type)
		{
		case ETileType::Road: return TerrainMoveCost::Road;
		case ETileType::Forest: return TerrainMoveCost::Forest;
		case ETileType::Scrub: return TerrainMoveCost::Scrub;
		case ETileType::Field: return TerrainMoveCost::Field;
		case ETileType::Stone: return TerrainMoveCost::Stone;
		case ETileType::Ruin: return TerrainMoveCost::Ruin;
		default: break;
		}

		if (Tile->Type == ETileType::Grass && Tile->Wetness >= MarshWetness)
		{
			return TerrainMoveCost::Marsh;
		}
		return TerrainMoveCost::Grass;
	}

	void InitFromWorld(FNavGrid& Grid, const FWorld& World)
	{
		Grid.W = World.W;
		Grid.H = World.H;
		const int32 Size = World.W * World.H;
		Grid.Blocked.Reset();
		Grid.Blocked.AddZeroed(Size);
		for (int32 Index = 0; Index < Size; ++Index)
		{
			if (World.Tiles[Index].Type == ETileType::Water)
			{
				Grid.Blocked[Index] = 1;
			}
		}
		RebuildMoveCosts(Grid, World);
	}

	void RebuildMoveCosts(FNavGrid& Grid, const FWorld& World)
	{
		const int32 Size = Grid.W * Grid.H;
		if (Grid.MoveCost.Num() != Size)
		{
			Grid.MoveCost.SetNumZeroed(Size);
		}
		for (int32 Index = 0; Index < Size; ++Index)
		{
			if (Grid.Blocked.IsValidIndex(Index) && Grid.Blocked[Index] == 1)
			{
				// `Float32Array` garde l'infini tel quel: pas de valeur sentinelle
				// a inventer, donc pas de seuil a se rappeler plus tard.
				Grid.MoveCost[Index] = AnastasisJs::StoreF32(Infinity);
				continue;
			}

			// Le profil de route (`sim.roadProfileAt`) appartient au chantier
			// urbanisme: tant qu'il n'existe pas, une case de route retombe sur
			// le cout de route par defaut, exactement comme le JS quand
			// `roadProfileAt` est absent.
			const FTile& Tile = World.Tiles[Index];
			if (Tile.Type == ETileType::Road)
			{
				const double RoadCost = TerrainMoveCost::Road;
				Grid.MoveCost[Index] = AnastasisJs::StoreF32(TerrainMoveCostOf(&Tile, &RoadCost));
				continue;
			}
			Grid.MoveCost[Index] = AnastasisJs::StoreF32(TerrainMoveCostOf(&Tile, nullptr));
		}
	}

	bool BlockedAt(const FNavGrid& Grid, int32 X, int32 Y)
	{
		if (!Grid.IsInBounds(X, Y))
		{
			return true;
		}
		return Grid.Blocked[Y * Grid.W + X] == 1;
	}

	bool FootBlockedAt(const FNavGrid& Grid, const FWorld& World, int32 X, int32 Y)
	{
		if (!Grid.IsInBounds(X, Y))
		{
			return true;
		}
		if (BlockedAt(Grid, X, Y))
		{
			return true;
		}
		return IsStandingTreeTile(World.Tiles[Y * Grid.W + X]);
	}

	double MoveCostAt(const FNavGrid& Grid, int32 X, int32 Y)
	{
		if (!Grid.IsInBounds(X, Y))
		{
			return Infinity;
		}
		return AnastasisJs::LoadF32(Grid.MoveCost[Y * Grid.W + X]);
	}

	double TileTraversalCost(const FNavGrid& Grid, int32 X, int32 Y, double BaseCost)
	{
		const double Mult = MoveCostAt(Grid, X, Y);
		if (!FMath::IsFinite(Mult))
		{
			return Infinity;
		}
		return FMath::Max(MinTraversalCost, BaseCost * Mult);
	}
}
