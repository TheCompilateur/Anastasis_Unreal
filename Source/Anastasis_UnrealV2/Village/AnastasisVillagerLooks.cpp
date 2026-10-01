#include "Village/AnastasisVillagerLooks.h"

#include "Containers/SortedMap.h"
#include "Misc/Crc.h"

namespace AnastasisVillagerLooks
{
	bool IsAssignableInVillage(EAnastasisVillagerCategory Category)
	{
		switch (Category)
		{
		case EAnastasisVillagerCategory::AdultMale:
		case EAnastasisVillagerCategory::AdultFemale:
		case EAnastasisVillagerCategory::ElderMale:
		case EAnastasisVillagerCategory::ElderFemale:
			return true;
		default:
			return false;
		}
	}

	double VillageShare(EAnastasisVillagerCategory Category)
	{
		switch (Category)
		{
		case EAnastasisVillagerCategory::AdultMale:
		case EAnastasisVillagerCategory::AdultFemale:
			return 0.35;
		case EAnastasisVillagerCategory::ElderMale:
		case EAnastasisVillagerCategory::ElderFemale:
			return 0.15;
		default:
			return 0.0;
		}
	}

	TArray<int32> VillagePool(const TArray<FAnastasisVillagerLook>& Looks, FName Job)
	{
		// One queue per category, each in CRC order of its ids: stable on the id, not on the asset
		// order, so re-importing in another order changes nobody's face.
		TSortedMap<uint8, TArray<int32>> Queues;
		for (int32 Index = 0; Index < Looks.Num(); ++Index)
		{
			const FAnastasisVillagerLook& Look = Looks[Index];
			if (IsAssignableInVillage(Look.Category) && Look.bInGame && Look.Jobs.Contains(Job) && !Look.Portrait.IsNull())
			{
				Queues.FindOrAdd(static_cast<uint8>(Looks[Index].Category)).Add(Index);
			}
		}
		int32 Total = 0;
		for (TPair<uint8, TArray<int32>>& Queue : Queues)
		{
			Queue.Value.Sort([&Looks](int32 A, int32 B)
			{
				const uint32 CA = FCrc::StrCrc32(*Looks[A].LookId.ToString());
				const uint32 CB = FCrc::StrCrc32(*Looks[B].LookId.ToString());
				return CA != CB ? CA < CB : Looks[A].LookId.LexicalLess(Looks[B].LookId);
			});
			Total += Queue.Value.Num();
		}

		// Next comes the category furthest behind its VILLAGE share. A pure CRC order gave 8 women out
		// of the first 12 villagers (first PIE run); a share proportional to the portrait counts gave
		// a village half old (the settler pool is 8 adults for 8 elders).
		TArray<int32> Pool;
		TMap<uint8, int32> Taken;
		while (Pool.Num() < Total)
		{
			const TArray<int32>* Best = nullptr;
			uint8 BestKey = 0;
			double BestProgress = TNumericLimits<double>::Max();
			for (const TPair<uint8, TArray<int32>>& Queue : Queues)
			{
				const int32 Used = Taken.FindRef(Queue.Key);
				if (Used >= Queue.Value.Num())
				{
					continue;
				}
				const double Progress = (Used + 0.5) / VillageShare(static_cast<EAnastasisVillagerCategory>(Queue.Key));
				if (Progress < BestProgress)
				{
					Best = &Queue.Value;
					BestKey = Queue.Key;
					BestProgress = Progress;
				}
			}
			int32& Used = Taken.FindOrAdd(BestKey);
			Pool.Add((*Best)[Used++]);
		}
		return Pool;
	}

	int32 PickLook(const TArray<int32>& Pool, const FString& NpcId)
	{
		if (Pool.IsEmpty())
		{
			return INDEX_NONE;
		}
		FString Suffix;
		int32 Ordinal = INDEX_NONE;
		if (NpcId.Split(TEXT("-"), nullptr, &Suffix, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
			&& !Suffix.IsEmpty() && Suffix.IsNumeric())
		{
			Ordinal = FCString::Atoi(*Suffix);
		}
		const uint32 Key = Ordinal >= 0 ? static_cast<uint32>(Ordinal) : FCrc::StrCrc32(*NpcId);
		return Pool[static_cast<int32>(Key % static_cast<uint32>(Pool.Num()))];
	}

	int32 ColourContrast(const FColor& A, const FColor& B)
	{
		return FMath::Max3(FMath::Abs(A.R - B.R), FMath::Abs(A.G - B.G), FMath::Abs(A.B - B.B));
	}

	FBodyLook BodyLookFor(FName LookId, EAnastasisVillagerCategory Category)
	{
		// sRGB swatches: what an artist would pick on a colour chart. Skins are kept low in saturation:
		// under the game's sun a saturated tan reads orange (first PIE capture, 2026-10-01).
		static const FColor Skins[] = {
			FColor(170, 128, 100), FColor(150, 110, 86), FColor(128, 92, 70), FColor(186, 146, 118), FColor(108, 76, 58),
		};
		static const FColor Hairs[] = { FColor(24, 19, 17), FColor(46, 33, 24), FColor(72, 50, 34) };
		static const FColor GreyHairs[] = { FColor(140, 135, 128), FColor(196, 192, 184), FColor(110, 104, 98) };
		// Undyed wool and linen dominate: dye was dear. The first capture's ochre chiton was the colour of
		// the skin under it -- the villager read as naked; a garment now always contrasts (below).
		static const FColor Garments[] = {
			FColor(214, 204, 180), // undyed wool
			FColor(230, 224, 206), // linen
			FColor(222, 214, 192), // wool, washed
			FColor(150, 150, 142), // grey wool
			FColor(146, 50, 40),   // madder
			FColor(84, 100, 130),  // faded woad
			FColor(62, 54, 48),    // dark brown wool
			FColor(104, 106, 66),  // weld and iron, olive
		};
		static const FColor Trims[] = { FColor(66, 44, 30), FColor(92, 60, 38), FColor(48, 40, 34) };

		// Independent draws from one CRC: each colour looks at its own bits.
		const uint32 Key = FCrc::StrCrc32(*LookId.ToString());
		auto Pick = [Key](int32 Shift, int32 Count) { return static_cast<int32>((Key >> Shift) % static_cast<uint32>(Count)); };

		const bool bElder = Category == EAnastasisVillagerCategory::ElderMale || Category == EAnastasisVillagerCategory::ElderFemale;
		FBodyLook Look;
		Look.bFemale = Category == EAnastasisVillagerCategory::AdultFemale
			|| Category == EAnastasisVillagerCategory::ElderFemale
			|| Category == EAnastasisVillagerCategory::ChildFemale;
		Look.Skin = FLinearColor::FromSRGBColor(Skins[Pick(0, UE_ARRAY_COUNT(Skins))]);
		Look.Hair = FLinearColor::FromSRGBColor(bElder
			? GreyHairs[Pick(4, UE_ARRAY_COUNT(GreyHairs))]
			: Hairs[Pick(4, UE_ARRAY_COUNT(Hairs))]);
		// Never a garment that blends into the skin: the next swatch along until one stands out.
		const FColor Skin = Skins[Pick(0, UE_ARRAY_COUNT(Skins))];
		int32 GarmentIndex = Pick(8, UE_ARRAY_COUNT(Garments));
		for (int32 Tries = 0; Tries < UE_ARRAY_COUNT(Garments) && ColourContrast(Garments[GarmentIndex], Skin) < MinGarmentContrast; ++Tries)
		{
			GarmentIndex = (GarmentIndex + 1) % UE_ARRAY_COUNT(Garments);
		}
		Look.Garment = FLinearColor::FromSRGBColor(Garments[GarmentIndex]);
		Look.Trim = FLinearColor::FromSRGBColor(Trims[Pick(12, UE_ARRAY_COUNT(Trims))]);
		// A man's chiton stops at the knee, a woman's peplos at the ankle; an old man's falls lower.
		Look.Hem = Look.bFemale ? 0.07f : (bElder ? 0.22f : 0.30f);
		Look.HairLow = Look.bFemale ? 0.76f : 0.85f;
		// Manny and Quinn both stand 180 cm (measured by create-villager-body.py): brought to ~168 / ~158 cm,
		// a little less with age, +-3 % between people.
		Look.Scale = (Look.bFemale ? 0.875f : 0.93f) * (bElder ? 0.97f : 1.0f) * (0.97f + 0.06f * Pick(16, 11) / 10.0f);
		Look.PlayRate = (bElder ? 0.9f : 1.0f) * (0.95f + 0.1f * Pick(20, 11) / 10.0f);
		return Look;
	}

	namespace
	{
		FColor Desaturate(const FColor& C, float Amount)
		{
			const float Luma = 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B;
			auto Mix = [Luma, Amount](uint8 V) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(static_cast<float>(V), Luma, Amount)), 0, 255)); };
			return FColor(Mix(C.R), Mix(C.G), Mix(C.B), 255);
		}

		float Luma(const FColor& C)
		{
			return 0.2126f * C.R + 0.7152f * C.G + 0.0722f * C.B;
		}
	}

	FBodyLook BodyLookFor(const FAnastasisVillagerLook& Portrait)
	{
		FBodyLook Look = BodyLookFor(Portrait.LookId, Portrait.Category);
		// Painted skin is lit and warm on the sheets; under the game's sun it turns orange (first PIE
		// capture): a third of its saturation goes.
		FColor Skin = Portrait.BodySkin.A > 0 ? Desaturate(Portrait.BodySkin, 0.35f) : Look.Skin.ToFColor(true);
		if (Portrait.BodySkin.A > 0)
		{
			Look.Skin = FLinearColor::FromSRGBColor(Skin);
		}
		if (Portrait.BodyHead.A > 0)
		{
			Look.Hair = FLinearColor::FromSRGBColor(FColor(Portrait.BodyHead.R, Portrait.BodyHead.G, Portrait.BodyHead.B, 255));
		}
		if (Portrait.BodyGarment.A > 0)
		{
			// The drawing's garment, pushed away from the skin if the two are too close: lighter if it was
			// the lighter of the two, darker otherwise. The hue stays the drawing's.
			FColor Garment(Portrait.BodyGarment.R, Portrait.BodyGarment.G, Portrait.BodyGarment.B, 255);
			const bool bLighter = Luma(Garment) >= Luma(Skin);
			for (int32 Step = 0; Step < 6 && ColourContrast(Garment, Skin) < MinGarmentContrast; ++Step)
			{
				Garment = bLighter
					? FColor(FMath::Min(255, Garment.R + (255 - Garment.R) / 3), FMath::Min(255, Garment.G + (255 - Garment.G) / 3), FMath::Min(255, Garment.B + (255 - Garment.B) / 3), 255)
					: FColor(Garment.R * 2 / 3, Garment.G * 2 / 3, Garment.B * 2 / 3, 255);
			}
			Look.Garment = FLinearColor::FromSRGBColor(Garment);
		}
		return Look;
	}
}
