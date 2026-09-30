#include "Work/AnastasisFields.h"

#include "Core/AnastasisJsNumeric.h"
#include "Work/AnastasisGather.h"

namespace AnastasisFields
{
	namespace
	{
		// `FIELD_SEASON_YIELD`, printemps -> hiver.
		const double SeasonRegen[4] = { 1.6, 1.85, 1.1, 0.55 };
		const double SeasonDailyChance[4] = { 0.45, 0.48, 0.35, 0.22 };

		/** `(x * A + y * B + c * C) >>> 0` puis `(h ^ (h >>> 13)) >>> 0`. */
		uint32 MixedSeed(int32 X, int32 Y, int32 Channel)
		{
			const double Sum = static_cast<double>(X) * 374761393.0
				+ static_cast<double>(Y) * 668265263.0
				+ static_cast<double>(Channel) * 2246822519.0;
			const uint32 H = AnastasisJs::ToUint32(Sum);
			return H ^ (H >> 13);
		}

		/** `(h * 1274126177) >>> 0` : produit en double, puis ToUint32. */
		uint32 DoubleMul(uint32 H)
		{
			return AnastasisJs::ToUint32(static_cast<double>(H) * 1274126177.0);
		}
	}

	double DailyChance(int32 Day)
	{
		return SeasonDailyChance[AnastasisGather::FieldSeasonFromDay(Day)];
	}

	int32 RegenAmount(int32 Base, int32 Day)
	{
		const double Mul = SeasonRegen[AnastasisGather::FieldSeasonFromDay(Day)];
		return FMath::Max(0, static_cast<int32>(AnastasisJs::Round(static_cast<double>(Base) * Mul)));
	}

	double RegrowRoll(int32 X, int32 Y, int32 Day)
	{
		return static_cast<double>(DoubleMul(MixedSeed(X, Y, Day))) / 4294967295.0;
	}

	double FieldCropHash(int32 X, int32 Y, int32 Channel)
	{
		const uint32 H = DoubleMul(MixedSeed(X, Y, Channel));
		return static_cast<double>(H ^ (H >> 16)) / 4294967295.0;
	}

	AnastasisWorld::ECropId CropAfterFallow(int32 X, int32 Y, int32 Day)
	{
		// `hash2d(x | 0, y | 0, 91 + ((day | 0) % 97))`.
		const double Roll = FieldCropHash(X, Y, 91 + (Day % 97));
		if (Roll > 0.66) return AnastasisWorld::ECropId::Fruit;
		if (Roll > 0.32) return AnastasisWorld::ECropId::Greens;
		return AnastasisWorld::ECropId::Grain;
	}

	bool RegrowFieldTile(AnastasisWorld::FTile& Tile, int32 Amount, int32 Day)
	{
		using AnastasisWorld::ECropId;
		if (Tile.Type != AnastasisWorld::ETileType::Field || !(Amount > 0)) return false;
		// `tile.fertility || 1`.
		const double Fertility = Tile.Fertility != 0.0 && !FMath::IsNaN(Tile.Fertility) ? Tile.Fertility : 1.0;
		const int32 Scaled = FMath::Max(1, static_cast<int32>(AnastasisJs::Round(Amount * Fertility)));
		const bool bFood = Tile.Resource == AnastasisWorld::EResource::Food;
		const int32 Before = bFood ? Tile.Amount : 0;
		const int32 Next = FMath::Min(FieldFoodCap, Before + Scaled);
		if (Next <= Before) return false;
		// Jachere -> culture productive avant de remettre du stock (`ensureFieldCropReady`).
		if (Before <= 0 || Tile.CropId == ECropId::Fallow)
		{
			if (Tile.CropId == ECropId::None || Tile.CropId == ECropId::Fallow)
			{
				Tile.CropId = CropAfterFallow(Tile.X, Tile.Y, Day);
			}
		}
		if (Before <= 0)
		{
			Tile.Resource = AnastasisWorld::EResource::Food;
		}
		Tile.Amount = Next;
		return true;
	}

	bool RegrowTileDaily(AnastasisWorld::FTile& Tile, int32 Day)
	{
		const int32 Amount = RegenAmount(FieldRegenPerDay, Day);
		if (Amount <= 0 || Tile.Type != AnastasisWorld::ETileType::Field) return false;
		const int32 Stock = Tile.Resource == AnastasisWorld::EResource::Food ? Tile.Amount : 0;
		if (Stock >= FieldFoodCap) return false;
		if (RegrowRoll(Tile.X, Tile.Y, Day) > DailyChance(Day)) return false;
		return RegrowFieldTile(Tile, Amount, Day);
	}
}
