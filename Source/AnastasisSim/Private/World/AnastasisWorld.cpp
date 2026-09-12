#include "World/AnastasisWorld.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"
#include "World/AnastasisHydrology.h"
#include "World/AnastasisWorldArchetype.h"
#include "World/AnastasisWorldNoise.h"

namespace AnastasisWorld
{
	namespace
	{
		constexpr double ScrubMoistSpan = 0.11;
		constexpr double StoneTileKeepFrac = 0.85;
		constexpr int32 MoistSeed = 1013;
		constexpr int32 RockSeed = 2027;
		constexpr int32 RuinSeed = 3041;
		constexpr int32 FieldPatchSeed = 4057;
		constexpr int32 ScrubPatchSeed = 5083;
		constexpr double LightStrength = 9.0;
		constexpr double AltitudeWeight = 0.25;

		struct FMountainProfile
		{
			double Knee = SeaLevel + 0.14;
			double GentlePlain = 0.48;
			double GentleRim = 0.85;
			double FootReach = 26.0;
			double FootPow = 2.6;
			double RimRise = 0.34;
			double RimSeaMargin = 0.03;
		};

		bool ShouldKeepStoneTile(int32 X, int32 Y, uint32 Seed)
		{
			return AnastasisWorldNoise::ValueNoise(
				static_cast<double>(X),
				static_cast<double>(Y),
				static_cast<double>(Seed) + RockSeed * 3.0) < StoneTileKeepFrac;
		}

		double TerrainAltitude(int32 X, int32 Y, uint32 Seed, const AnastasisWorldArchetype::FKnobs& Arch)
		{
			const double WarpX = (AnastasisWorldNoise::Fbm(X / 28.0, Y / 28.0, static_cast<double>(Seed) + 7.0) - 0.5) * 7.0;
			const double WarpY = (AnastasisWorldNoise::Fbm(X / 31.0, Y / 31.0, static_cast<double>(Seed) + 19.0) - 0.5) * 7.0;
			const double WX = X + WarpX;
			const double WY = Y + WarpY;
			const double Continent = AnastasisWorldNoise::Fbm(WX / 44.0, WY / 44.0, static_cast<double>(Seed)) * 0.5;
			const double Hills = AnastasisWorldNoise::Fbm(WX / 17.0, WY / 17.0, static_cast<double>(Seed) + 41.0) * 0.28;
			const double Detail = AnastasisWorldNoise::Fbm(WX / 7.5, WY / 7.5, static_cast<double>(Seed) + 83.0) * 0.11;
			const double RidgeNoise = AnastasisWorldNoise::Fbm(WX / 23.0, WY / 23.0, static_cast<double>(Seed) + 127.0);
			const double Ridge = 1.0 - FMath::Abs(RidgeNoise * 2.0 - 1.0);
			const double RidgeSoft = AnastasisJs::Pow(FMath::Max(0.0, Ridge), 1.45);
			const double HighDamp = 1.0 - RidgeSoft * 0.42;
			double Height = Continent + Hills + Detail * HighDamp + RidgeSoft * Arch.RidgeBoost * 0.88 - 0.04;

			const double PlateauMask = AnastasisWorldNoise::Fbm(WX / 34.0, WY / 34.0, static_cast<double>(Seed) + 211.0);
			if (Arch.PlateauFlat > 0.05 && PlateauMask > 0.48 && PlateauMask < 0.88)
			{
				const double Flatness = 1.0 - FMath::Abs(PlateauMask - 0.68) / 0.2;
				const double Target = 0.38 + (PlateauMask - 0.48) * 0.22;
				Height = AnastasisMath::Lerp(Height, Target, AnastasisMath::Clamp(Flatness, 0.0, 1.0) * Arch.PlateauFlat);
				Height += Detail * 0.08 * Flatness;
			}
			return Height;
		}

		void ShapeMountainProfile(TArray<float>& Height, int32 W, int32 H, uint32 Seed)
		{
			const FMountainProfile P;
			for (int32 Y = 0; Y < H; ++Y)
			{
				for (int32 X = 0; X < W; ++X)
				{
					const int32 I = Y * W + X;
					double Alt = AnastasisJs::LoadF32(Height[I]);
					const double Edge = FMath::Min(FMath::Min(static_cast<double>(X), static_cast<double>(Y)), FMath::Min(static_cast<double>(W - 1 - X), static_cast<double>(H - 1 - Y)));
					const double Foot = AnastasisMath::Clamp(1.0 - Edge / P.FootReach, 0.0, 1.0);
					const double Gentle = AnastasisMath::Lerp(P.GentlePlain, P.GentleRim, Foot * Foot);
					if (Alt > P.Knee)
					{
						Alt = P.Knee + (Alt - P.Knee) * Gentle;
					}
					if (Alt > SeaLevel + P.RimSeaMargin)
					{
						const double Jag = 0.72 + AnastasisWorldNoise::Fbm(X / 6.0, Y / 6.0, static_cast<double>(Seed) + 301.0) * 0.55;
						Alt += P.RimRise * AnastasisJs::Pow(Foot, P.FootPow) * Jag;
					}
					Height[I] = AnastasisJs::StoreF32(FMath::Min(Alt, 1.0));
				}
			}
		}

		void SwapFloat(TArray<float>& Values, int32 A, int32 B)
		{
			const float Tmp = Values[A];
			Values[A] = Values[B];
			Values[B] = Tmp;
		}

		int32 PartitionFloat(TArray<float>& Values, int32 Left, int32 Right, int32 PivotIndex)
		{
			const float PivotValue = Values[PivotIndex];
			SwapFloat(Values, PivotIndex, Right);
			int32 StoreIndex = Left;
			for (int32 I = Left; I < Right; ++I)
			{
				if (Values[I] < PivotValue)
				{
					SwapFloat(Values, StoreIndex, I);
					StoreIndex += 1;
				}
			}
			SwapFloat(Values, Right, StoreIndex);
			return StoreIndex;
		}

		float SelectKthFloat(TArray<float>& Values, int32 K)
		{
			int32 Left = 0;
			int32 Right = Values.Num() - 1;
			while (Left < Right)
			{
				const int32 PivotIndex = PartitionFloat(Values, Left, Right, (Left + Right) >> 1);
				if (K == PivotIndex)
				{
					return Values[K];
				}
				if (K < PivotIndex)
				{
					Right = PivotIndex - 1;
				}
				else
				{
					Left = PivotIndex + 1;
				}
			}
			return Values[Left];
		}

		void NormalizeReliefToSea(TArray<float>& Height, double Sea, double LowFrac)
		{
			TArray<float> Copy = Height;
			const int32 PivotIndex = FMath::Min(Copy.Num() - 1, static_cast<int32>(AnastasisJs::Floor(Copy.Num() * LowFrac)));
			const float Pivot = SelectKthFloat(Copy, PivotIndex);
			// JS: shift = seaLevel - pivot (double), puis height[i] += shift (store f32).
			const double Shift = Sea - AnastasisJs::LoadF32(Pivot);
			for (int32 I = 0; I < Height.Num(); ++I)
			{
				Height[I] = AnastasisJs::StoreF32(AnastasisJs::LoadF32(Height[I]) + Shift);
			}
		}

		TArray<uint8> DistanceFieldToWater(const TArray<float>& Height, int32 W, int32 H, double Sea)
		{
			const int32 N = W * H;
			TArray<uint8> Dist;
			Dist.Init(255, N);
			TArray<int32> Queue;
			Queue.SetNumUninitialized(N);
			int32 Head = 0;
			int32 Tail = 0;
			for (int32 I = 0; I < N; ++I)
			{
				if (AnastasisJs::LoadF32(Height[I]) < Sea)
				{
					Dist[I] = 0;
					Queue[Tail++] = I;
				}
			}
			const int32 Dirs[4] = { 1, -1, W, -W };
			while (Head < Tail)
			{
				const int32 I = Queue[Head++];
				const uint8 D = Dist[I];
				if (D >= 254)
				{
					continue;
				}
				const int32 X = I % W;
				for (int32 K = 0; K < 4; ++K)
				{
					if (K == 0 && X == W - 1)
					{
						continue;
					}
					if (K == 1 && X == 0)
					{
						continue;
					}
					const int32 NI = I + Dirs[K];
					if (NI < 0 || NI >= N)
					{
						continue;
					}
					if (Dist[NI] <= D + 1)
					{
						continue;
					}
					Dist[NI] = static_cast<uint8>(D + 1);
					Queue[Tail++] = NI;
				}
			}
			return Dist;
		}

		double Intensity(double Delta, double Span)
		{
			return AnastasisMath::Clamp(Delta / Span, 0.0, 1.0);
		}

		void EnforceForestTileCap(TArray<FTile>& Tiles, uint32 Seed)
		{
			const int32 MaxForest = static_cast<int32>(AnastasisJs::Floor(Tiles.Num() * ForestTileMaxFrac));
			TArray<int32> ForestIdx;
			for (int32 I = 0; I < Tiles.Num(); ++I)
			{
				if (Tiles[I].Type == ETileType::Forest)
				{
					ForestIdx.Add(I);
				}
			}
			if (ForestIdx.Num() <= MaxForest)
			{
				for (const int32 I : ForestIdx)
				{
					Tiles[I].bHasForestMargin = false;
				}
				return;
			}
			ForestIdx.StableSort([&](int32 A, int32 B)
			{
				const double MA = Tiles[A].bHasForestMargin ? Tiles[A].ForestMargin : 0.0;
				const double MB = Tiles[B].bHasForestMargin ? Tiles[B].ForestMargin : 0.0;
				if (MA != MB)
				{
					return MA < MB;
				}
				const double HA = AnastasisWorldNoise::ValueNoise(Tiles[A].X, Tiles[A].Y, static_cast<double>(Seed) + 911.0);
				const double HB = AnastasisWorldNoise::ValueNoise(Tiles[B].X, Tiles[B].Y, static_cast<double>(Seed) + 911.0);
				if (HA != HB)
				{
					return HA < HB;
				}
				return A < B;
			});
			const int32 Cut = ForestIdx.Num() - MaxForest;
			for (int32 K = 0; K < Cut; ++K)
			{
				FTile& Tile = Tiles[ForestIdx[K]];
				const double Margin = Tile.bHasForestMargin ? Tile.ForestMargin : 0.0;
				Tile.Type = ETileType::Scrub;
				Tile.Resource = EResource::Wood;
				Tile.Amount = 8 + static_cast<int32>(AnastasisJs::Floor(AnastasisMath::Clamp(Margin / 0.28, 0.0, 1.0) * 12.0));
				Tile.bHasForestMargin = false;
			}
			for (int32 K = Cut; K < ForestIdx.Num(); ++K)
			{
				Tiles[ForestIdx[K]].bHasForestMargin = false;
			}
		}
	}

	ECropId PickFieldCropId(int32 X, int32 Y, uint32 Salt)
	{
		const double Mixed = static_cast<double>(X) * 374761393.0
			+ static_cast<double>(Y) * 668265263.0
			+ static_cast<double>(Salt) * 2246822519.0;
		uint32 H = AnastasisJs::ToUint32(Mixed);
		H = H ^ (H >> 13);
		H = AnastasisJs::Imul(H, 1274126177u);
		const double Roll = static_cast<double>(H ^ (H >> 16)) / 4294967295.0;
		if (Roll > 0.84)
		{
			return ECropId::Fallow;
		}
		if (Roll > 0.62)
		{
			return ECropId::Fruit;
		}
		if (Roll > 0.32)
		{
			return ECropId::Greens;
		}
		return ECropId::Grain;
	}

	const TCHAR* TileTypeName(ETileType Type)
	{
		switch (Type)
		{
		case ETileType::Grass: return TEXT("grass");
		case ETileType::Water: return TEXT("water");
		case ETileType::Stone: return TEXT("stone");
		case ETileType::Ruin: return TEXT("ruin");
		case ETileType::Forest: return TEXT("forest");
		case ETileType::Scrub: return TEXT("scrub");
		case ETileType::Field: return TEXT("field");
		}
		return TEXT("grass");
	}

	FWorld GenerateWorld(uint32 Seed, int32 W, int32 H)
	{
		const AnastasisWorldArchetype::FKnobs Arch = AnastasisWorldArchetype::Resolve(Seed);
		TArray<float> Height;
		Height.SetNumUninitialized(W * H);
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				Height[Y * W + X] = AnastasisJs::StoreF32(TerrainAltitude(X, Y, Seed, Arch));
			}
		}
		NormalizeReliefToSea(Height, SeaLevel, Arch.SeaLowFrac);
		ShapeMountainProfile(Height, W, H, Seed);
		const AnastasisHydrology::FResult Hydro = AnastasisHydrology::Apply(
			Height, W, H, Seed, SeaLevel,
			Arch.ValleyDig, Arch.BasinDig, Arch.ChannelDig, Arch.LakeDig, Arch.MinWaterBody);
		const TArray<uint8> WaterDist = DistanceFieldToWater(Height, W, H, SeaLevel);

		FWorld World;
		World.W = W;
		World.H = H;
		World.Archetype = Arch;
		World.Tiles.SetNum(W * H);

		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const int32 I = Y * W + X;
				const double Alt = AnastasisJs::LoadF32(Height[I]);
				double Moist = AnastasisWorldNoise::Fbm(X / 11.0, Y / 11.0, static_cast<double>(Seed) + MoistSeed) + Arch.MoistBias;
				const double Rock = AnastasisWorldNoise::Fbm(X / 9.0, Y / 9.0, static_cast<double>(Seed) + RockSeed);
				const double Ruin = AnastasisWorldNoise::Fbm(X / 5.0, Y / 5.0, static_cast<double>(Seed) + RuinSeed);
				const uint8 WD = WaterDist[I];

				if (WD > 0 && WD < 10)
				{
					const double Riparian = (1.0 - WD / 10.0) * Arch.RiparianBoost;
					Moist = AnastasisMath::Clamp(Moist + Riparian, 0.0, 1.35);
				}
				if (Alt > 0.58)
				{
					Moist -= (Alt - 0.58) * 0.35;
				}

				const double Shore = WD == 0 ? 0.0 : AnastasisMath::Clamp(1.0 - (WD - 0.4) / 4.6, 0.0, 1.0);
				const double Wetness = WD == 0 ? 1.0 : AnastasisMath::Clamp(1.0 - WD / 6.5, 0.0, 1.0);
				if (WD >= 2 && WD <= 5 && Alt < SeaLevel + 0.16)
				{
					Moist = AnastasisMath::Lerp(Moist, Arch.FieldT - 0.03, Shore * 0.72);
				}

				ETileType Type = ETileType::Grass;
				EResource Resource = EResource::None;
				int32 Amount = 0;
				const bool bStoneCandidate = Alt > Arch.Highland && Rock > Arch.RockT;

				if (Alt < SeaLevel)
				{
					Type = ETileType::Water;
				}
				else if (bStoneCandidate && ShouldKeepStoneTile(X, Y, Seed))
				{
					Type = ETileType::Stone;
					Resource = EResource::Stone;
					Amount = 18 + static_cast<int32>(AnastasisJs::Floor(Intensity(Rock - Arch.RockT, 0.42) * 27.0));
				}
				else if (Ruin > Arch.RuinT)
				{
					Type = ETileType::Ruin;
					Resource = EResource::Stone;
					Amount = 8;
				}
				else if (Moist > Arch.ForestT)
				{
					Type = ETileType::Forest;
					Resource = EResource::Wood;
					Amount = 24 + static_cast<int32>(AnastasisJs::Floor(Intensity(Moist - Arch.ForestT, 0.36) * 32.0));
				}
				else if (Moist > Arch.ForestT - ScrubMoistSpan)
				{
					const double ScrubPatch = AnastasisWorldNoise::Fbm(X / 5.5, Y / 5.5, static_cast<double>(Seed) + ScrubPatchSeed);
					if (ScrubPatch > 0.54)
					{
						Type = ETileType::Scrub;
						Resource = EResource::Wood;
						Amount = 8 + static_cast<int32>(AnastasisJs::Floor(Intensity(Moist - (Arch.ForestT - ScrubMoistSpan), ScrubMoistSpan) * 14.0));
					}
				}
				else
				{
					const double FieldPatch = AnastasisWorldNoise::Fbm(X / 6.2, Y / 6.2, static_cast<double>(Seed) + FieldPatchSeed);
					const double FieldPatchT = AnastasisMath::Clamp(0.56 - Arch.PlateauFlat * 0.14, 0.44, 0.62);
					const bool bFieldBelt = Moist < Arch.FieldT + 0.05 && Moist > Arch.FieldT - 0.16;
					const bool bRiparianField = WD >= 2 && Shore > 0.35 && Shore < 0.72 && Moist < Arch.ForestT * 0.92;
					if (Shore < 0.78 && FieldPatch > FieldPatchT && (bFieldBelt || bRiparianField))
					{
						Type = ETileType::Field;
						Resource = EResource::Food;
						const double FieldDelta = Moist < Arch.FieldT ? Arch.FieldT - Moist : Shore * 0.12;
						Amount = 11 + static_cast<int32>(AnastasisJs::Floor(Intensity(FieldDelta, 0.25) * 26.0));
					}
				}

				FTile& Tile = World.Tiles[I];
				Tile.X = X;
				Tile.Y = Y;
				Tile.Type = Type;
				Tile.Resource = Resource;
				Tile.Amount = Amount;
				Tile.Alt = AnastasisJs::Round3(Alt);
				Tile.Shade = 0.0;
				Tile.Shore = Type == ETileType::Water ? 0.0 : AnastasisJs::Round3(Shore);
				Tile.Wetness = AnastasisJs::Round3(Wetness);
				if (Type == ETileType::Field)
				{
					Tile.CropId = PickFieldCropId(X, Y, Seed);
					const double MoistureFit = 1.0 - AnastasisMath::Clamp(FMath::Abs(Moist - Arch.FieldT) / 0.3, 0.0, 1.0);
					Tile.Fertility = AnastasisJs::Round3(AnastasisMath::Clamp(0.75 + MoistureFit * 0.35 + Shore * 0.25, 0.7, 1.35));
				}
				if (Type == ETileType::Forest)
				{
					Tile.ForestMargin = Moist - Arch.ForestT;
					Tile.bHasForestMargin = true;
				}
			}
		}

		EnforceForestTileCap(World.Tiles, Seed);
		AnastasisHydrology::StampWaterFlowFields(World.Tiles, W, H, Hydro.Accum, Hydro.FlowTo, SeaLevel, Height);

		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				FTile& Tile = World.Tiles[Y * W + X];
				const double Alt = AnastasisJs::LoadF32(Height[Y * W + X]);
				if (Tile.Type == ETileType::Water)
				{
					const double Depth = AnastasisMath::Clamp((SeaLevel - Alt) / 0.2, 0.0, 1.0);
					Tile.Shade = AnastasisJs::Round3(AnastasisMath::Lerp(0.6, -1.0, Depth));
					continue;
				}
				const int32 CXM = static_cast<int32>(AnastasisMath::Clamp(static_cast<double>(X - 1), 0.0, W - 1.0));
				const int32 CXP = static_cast<int32>(AnastasisMath::Clamp(static_cast<double>(X + 1), 0.0, W - 1.0));
				const int32 CYM = static_cast<int32>(AnastasisMath::Clamp(static_cast<double>(Y - 1), 0.0, H - 1.0));
				const int32 CYP = static_cast<int32>(AnastasisMath::Clamp(static_cast<double>(Y + 1), 0.0, H - 1.0));
				const double Slope = AnastasisJs::Tanh(
					(AnastasisJs::LoadF32(Height[Y * W + CXM])
					- AnastasisJs::LoadF32(Height[Y * W + CXP])
					+ AnastasisJs::LoadF32(Height[CYM * W + X])
					- AnastasisJs::LoadF32(Height[CYP * W + X])) * LightStrength);
				Tile.Shade = AnastasisJs::Round3(AnastasisMath::Clamp(
					Slope * (1.0 - AltitudeWeight) + (Alt - 0.5) * 2.0 * AltitudeWeight,
					-1.0,
					1.0));
			}
		}

		return World;
	}
}
