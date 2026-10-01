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
}
