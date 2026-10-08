#include "Village/AnastasisVillage.h"

// ecart n°46 (save-history-001) -- la biographie des batiments, observee par la simulation.
//
// Portee depuis l'hote Unreal (AnastasisSettlement::FLedger::Observe, SETTLEMENT_MORPHOGENESIS_001), a
// une difference pres : la forme (le programme d'architecture) n'est plus calculee ici. La simulation
// garde les faits qui la decident (phase de la maison, metier et foyer du fondateur) ; l'hote en deduit
// la forme. Les memes transitions, dans le meme ordre, avec les memes compteurs ; observees a chaque pas
// au lieu de chaque image, elles ne manquent plus un changement de mains pendant un saut de temps.

namespace AnastasisVillage
{
	const TCHAR* BiographyEventName(EBiographyEvent Kind)
	{
		switch (Kind)
		{
		case EBiographyEvent::Seen: return TEXT("seen");
		case EBiographyEvent::Completed: return TEXT("completed");
		case EBiographyEvent::Founded: return TEXT("founded");
		case EBiographyEvent::OwnerChanged: return TEXT("owner_changed");
		case EBiographyEvent::OwnerLost: return TEXT("owner_lost");
		case EBiographyEvent::Crowded: return TEXT("crowded");
		case EBiographyEvent::Vacated: return TEXT("vacated");
		case EBiographyEvent::Reoccupied: return TEXT("reoccupied");
		default: return TEXT("?");
		}
	}

	int32 FVillage::ObserveBiographies(int32 Day)
	{
		int32 Written = 0;
		auto Add = [&](FBuildingBiography& B, EBiographyEvent Kind, const FString& Detail)
		{
			B.Events.Add({Day, Kind, Detail});
			++Written;
		};
		for (const FBuilding& Building : Buildings.GetItems())
		{
			FBuildingBiography* Found = Biographies.Find(Building.Id);
			if (!Found)
			{
				FBuildingBiography& New = Biographies.Add(Building.Id);
				New.Id = Building.Id;
				New.Type = Building.Type;
				New.CellX = FMath::FloorToInt32(Building.X);
				New.CellY = FMath::FloorToInt32(Building.Y);
				New.FirstSeenDay = Day;
				New.FormHousePhase = Building.HousePhase;
				Add(New, EBiographyEvent::Seen, Building.IsCompleted() ? TEXT("pose acheve") : TEXT("chantier ouvert"));
				Found = &New;
			}
			FBuildingBiography& B = *Found;
			const bool bHouse = Building.Type == HouseType;
			if (Building.IsCompleted() && B.CompletedDay < 0)
			{
				B.CompletedDay = Building.CompletedDay >= 0 ? Building.CompletedDay : Day;
				Add(B, EBiographyEvent::Completed, Building.CompletedById.IsEmpty() ? FString(TEXT("-"))
					: FString::Printf(TEXT("acheve par %s"), *Building.CompletedById));
				if (!bHouse && !B.bFormFixed)
				{
					B.bFormFixed = true;
					B.FoundedDay = B.CompletedDay;
				}
			}
			B.Occupants = bHouse ? CountShelterOccupants(Building.Id) : 0;
			B.PeakOccupants = FMath::Max(B.PeakOccupants, B.Occupants);
			const FString& Owner = Building.Owner;
			if (bHouse && Building.IsCompleted() && !Owner.IsEmpty() && !B.bFormFixed)
			{
				const FNpc* Npc = FindNpc(Owner);
				B.Founder = Owner;
				B.FounderJob = Npc ? Npc->JobId : FString(TEXT("settler"));
				B.FounderHousehold = FMath::Max(1, B.Occupants);
				B.FormHousePhase = Building.HousePhase;
				B.bFormFixed = true;
				B.FoundedDay = Day;
				B.Owner = Owner;
				Add(B, EBiographyEvent::Founded, FString::Printf(TEXT("%s (%s), foyer de %d"), *Owner, *B.FounderJob, B.FounderHousehold));
			}
			else if (B.bFormFixed && bHouse && Owner != B.Owner)
			{
				if (Owner.IsEmpty())
				{
					Add(B, EBiographyEvent::OwnerLost, FString::Printf(TEXT("%s n'y est plus ; la maison garde la forme de son fondateur %s"),
						*B.Owner, *B.Founder));
				}
				else
				{
					const FNpc* Npc = FindNpc(Owner);
					++B.OwnerChanges;
					Add(B, EBiographyEvent::OwnerChanged, FString::Printf(TEXT("%s -> %s (%s) : herite des murs de %s"),
						B.Owner.IsEmpty() ? TEXT("-") : *B.Owner, *Owner, Npc ? *Npc->JobId : TEXT("?"), *B.Founder));
				}
				B.Owner = Owner;
			}
			// Une fois par jour simule : les nuits pleines, les vides et les retours.
			if (Day != B.LastObservedDay && Building.IsCompleted())
			{
				B.LastObservedDay = Day;
				const bool bOccupied = B.Occupants > 0;
				if (bHouse)
				{
					const int32 Capacity = ShelterCapacity(Building);
					if (Capacity > 0 && B.Occupants >= Capacity - 1)
					{
						if (B.CrowdedDays == 0)
						{
							Add(B, EBiographyEvent::Crowded, FString::Printf(TEXT("%d dormeurs pour %d places : pression d'agrandissement (resolveHouseUpgrades non porte : or et marche absents)"),
								B.Occupants, Capacity));
						}
						++B.CrowdedDays;
					}
				}
				if (bHouse && B.bWasOccupied && !bOccupied)
				{
					++B.VacancyEpisodes;
					Add(B, EBiographyEvent::Vacated, TEXT("plus personne n'y dort"));
				}
				else if (bHouse && !B.bWasOccupied && bOccupied && B.FoundedDay >= 0 && B.VacancyEpisodes > 0)
				{
					Add(B, EBiographyEvent::Reoccupied, FString::Printf(TEXT("%d y dorment de nouveau"), B.Occupants));
				}
				B.bWasOccupied = bOccupied;
			}
		}
		return Written;
	}
}
