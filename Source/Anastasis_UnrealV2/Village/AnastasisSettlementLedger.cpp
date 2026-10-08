#include "Village/AnastasisSettlementLedger.h"

#include "Anastasis_UnrealV2.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisBuild.h"
#include "Work/AnastasisGather.h"

namespace AnastasisSettlement
{
	using AnastasisArchitecture::EVariant;

	const TCHAR* EventName(EEvent Kind)
	{
		switch (Kind)
		{
		case EEvent::Seen: return TEXT("seen");
		case EEvent::Completed: return TEXT("completed");
		case EEvent::Founded: return TEXT("founded");
		case EEvent::OwnerChanged: return TEXT("owner_changed");
		case EEvent::OwnerLost: return TEXT("owner_lost");
		case EEvent::Crowded: return TEXT("crowded");
		case EEvent::Vacated: return TEXT("vacated");
		case EEvent::Reoccupied: return TEXT("reoccupied");
		default: return TEXT("?");
		}
	}

	bool ProgramFor(const FString& Type, int32 HousePhase, const FString& OwnerJob, int32 Household,
		EVariant& OutProgram, FString& OutCause)
	{
		if (Type == AnastasisVillage::WellType)
		{
			OutProgram = EVariant::Well;
			OutCause = TEXT("puits commun : margelle, treuil, aire dallee, abreuvoir");
			return true;
		}
		if (Type == AnastasisVillage::GranaryType)
		{
			OutProgram = EVariant::Storehouse;
			OutCause = TEXT("grenier commun : reserve maconnee et fenil, a l'echelle du village");
			return true;
		}
		if (Type != AnastasisVillage::HouseType)
		{
			return false;
		}
		if (OwnerJob.IsEmpty())
		{
			OutProgram = EVariant::HousePoor;
			OutCause = TEXT("abri sans foyer : une piece, en attendant qu'on la prenne");
			return true;
		}
		// Le metier du fondateur : ce que le foyer doit loger en plus des dormeurs.
		int32 Tier = 0;
		if (OwnerJob == AnastasisGather::JobFarmer)
		{
			Tier = 2;
			OutCause = FString::Printf(TEXT("fonde par un cultivateur (foyer de %d) : grange, fenil, aire et cour close pour la recolte"), Household);
		}
		else if (OwnerJob == AnastasisBuild::JobBuilder)
		{
			Tier = 1;
			OutCause = FString::Printf(TEXT("fonde par un batisseur (foyer de %d) : rez maconne, etage a pan de bois"), Household);
		}
		else if (Household >= 4)
		{
			Tier = 1;
			OutCause = FString::Printf(TEXT("foyer de %d sans metier propre : deux niveaux pour dormir tous"), Household);
		}
		else
		{
			OutCause = FString::Printf(TEXT("foyer de %d sans metier propre : une piece, foyer au sol"), Household);
		}
		// La phase de la reference (agrandissements, `HOUSE_PHASES`) : aujourd'hui toujours 1 dans le port.
		const int32 PhaseTier = HousePhase >= 5 ? 2 : (HousePhase >= 3 ? 1 : 0);
		if (PhaseTier > Tier)
		{
			Tier = PhaseTier;
			OutCause += FString::Printf(TEXT(" ; agrandie (phase %d)"), HousePhase);
		}
		OutProgram = Tier == 2 ? EVariant::HouseFarm : (Tier == 1 ? EVariant::HouseMedium : EVariant::HousePoor);
		return true;
	}

	int32 FLedger::Observe(const AnastasisVillage::FVillage& Village, int32 Day)
	{
		int32 Written = 0;
		auto Add = [&](FBiography& B, EEvent Kind, const FString& Detail)
		{
			B.Events.Add({Day, Kind, Detail});
			++Written;
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_SETTLEMENT event day=%d id=%s %s %s"),
				Day, *B.Id, EventName(Kind), *Detail);
		};
		for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
		{
			FBiography* Found = Bios.Find(Building.Id);
			if (!Found)
			{
				FBiography& New = Bios.Add(Building.Id);
				New.Id = Building.Id;
				New.Type = Building.Type;
				New.CellX = FMath::FloorToInt32(Building.X);
				New.CellY = FMath::FloorToInt32(Building.Y);
				New.FirstSeenDay = Day;
				ProgramFor(Building.Type, Building.HousePhase, FString(), 0, New.Program, New.ProgramCause);
				Add(New, EEvent::Seen, Building.IsCompleted() ? TEXT("pose acheve") : TEXT("chantier ouvert"));
				Found = &New;
			}
			FBiography& B = *Found;
			const bool bHouse = Building.Type == AnastasisVillage::HouseType;
			if (Building.IsCompleted() && B.CompletedDay < 0)
			{
				B.CompletedDay = Building.CompletedDay >= 0 ? Building.CompletedDay : Day;
				Add(B, EEvent::Completed, Building.CompletedById.IsEmpty() ? FString(TEXT("-"))
					: FString::Printf(TEXT("acheve par %s"), *Building.CompletedById));
				if (!bHouse && !B.bProgramFixed)
				{
					B.bProgramFixed = true;
					B.FoundedDay = B.CompletedDay;
				}
			}
			B.Occupants = bHouse ? Village.CountShelterOccupants(Building.Id) : 0;
			B.PeakOccupants = FMath::Max(B.PeakOccupants, B.Occupants);
			const FString Owner = Building.Owner;
			if (bHouse && Building.IsCompleted() && !Owner.IsEmpty() && !B.bProgramFixed)
			{
				const AnastasisVillage::FNpc* Npc = Village.FindNpc(Owner);
				B.Founder = Owner;
				B.FounderJob = Npc ? Npc->JobId : FString(TEXT("settler"));
				B.FounderHousehold = FMath::Max(1, B.Occupants);
				ProgramFor(Building.Type, Building.HousePhase, B.FounderJob, B.FounderHousehold, B.Program, B.ProgramCause);
				B.bProgramFixed = true;
				B.FoundedDay = Day;
				B.Owner = Owner;
				Add(B, EEvent::Founded, FString::Printf(TEXT("%s (%s) -> %s : %s"), *Owner, *B.FounderJob,
					AnastasisArchitecture::Get(B.Program).Id, *B.ProgramCause));
			}
			else if (B.bProgramFixed && bHouse && Owner != B.Owner)
			{
				if (Owner.IsEmpty())
				{
					Add(B, EEvent::OwnerLost, FString::Printf(TEXT("%s n'y est plus ; la maison garde la forme de son fondateur %s"),
						*B.Owner, *B.Founder));
				}
				else
				{
					const AnastasisVillage::FNpc* Npc = Village.FindNpc(Owner);
					++B.OwnerChanges;
					Add(B, EEvent::OwnerChanged, FString::Printf(TEXT("%s -> %s (%s) : herite des murs de %s"),
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
					const int32 Capacity = Village.ShelterCapacity(Building);
					if (Capacity > 0 && B.Occupants >= Capacity - 1)
					{
						if (B.CrowdedDays == 0)
						{
							Add(B, EEvent::Crowded, FString::Printf(TEXT("%d dormeurs pour %d places : pression d'agrandissement (resolveHouseUpgrades non porte : or et marche absents)"),
								B.Occupants, Capacity));
						}
						++B.CrowdedDays;
					}
				}
				if (bHouse && B.bWasOccupied && !bOccupied)
				{
					++B.VacancyEpisodes;
					Add(B, EEvent::Vacated, TEXT("plus personne n'y dort"));
				}
				else if (bHouse && !B.bWasOccupied && bOccupied && B.FoundedDay >= 0 && B.VacancyEpisodes > 0)
				{
					Add(B, EEvent::Reoccupied, FString::Printf(TEXT("%d y dorment de nouveau"), B.Occupants));
				}
				B.bWasOccupied = bOccupied;
			}
		}
		return Written;
	}

	void FLedger::Log(int32 Day) const
	{
		TArray<FString> Ids;
		Bios.GetKeys(Ids);
		Ids.Sort();
		for (const FString& Id : Ids)
		{
			const FBiography& B = Bios[Id];
			UE_LOG(LogAnastasis_UnrealV2, Display,
				TEXT("ANASTASIS_SETTLEMENT bio id=%s type=%s cell=(%d,%d) program=%s fixed=%d founded=%d founder=%s job=%s household=%d owner=%s occupants=%d peak=%d crowded_days=%d owner_changes=%d vacancies=%d age=%d events=%d cause=\"%s\""),
				*B.Id, *B.Type, B.CellX, B.CellY, AnastasisArchitecture::Get(B.Program).Id, B.bProgramFixed ? 1 : 0, B.FoundedDay,
				B.Founder.IsEmpty() ? TEXT("-") : *B.Founder, B.FounderJob.IsEmpty() ? TEXT("-") : *B.FounderJob, B.FounderHousehold,
				B.Owner.IsEmpty() ? TEXT("-") : *B.Owner, B.Occupants, B.PeakOccupants, B.CrowdedDays, B.OwnerChanges,
				B.VacancyEpisodes, B.AgeDays(Day), B.Events.Num(), *B.ProgramCause);
		}
	}
}
