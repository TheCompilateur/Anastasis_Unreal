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
		return AnastasisVillage::BiographyEventName(Kind);
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
		if (Type == AnastasisVillage::CabinType)
		{
			OutProgram = EVariant::Cabin;
			OutCause = TEXT("cabane levee seul : une piece, un foyer, un lit");
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
		for (const TPair<FString, AnastasisVillage::FBuildingBiography>& Pair : Village.GetBiographies())
		{
			const AnastasisVillage::FBuildingBiography& Fact = Pair.Value;
			FBiography& B = Bios.FindOrAdd(Pair.Key);
			B.Id = Fact.Id;
			B.Type = Fact.Type;
			B.CellX = Fact.CellX;
			B.CellY = Fact.CellY;
			B.FirstSeenDay = Fact.FirstSeenDay;
			B.CompletedDay = Fact.CompletedDay;
			B.FoundedDay = Fact.FoundedDay;
			B.Founder = Fact.Founder;
			B.FounderJob = Fact.FounderJob;
			B.FounderHousehold = Fact.FounderHousehold;
			B.bProgramFixed = Fact.bFormFixed;
			B.Owner = Fact.Owner;
			B.Occupants = Fact.Occupants;
			B.PeakOccupants = Fact.PeakOccupants;
			B.CrowdedDays = Fact.CrowdedDays;
			B.OwnerChanges = Fact.OwnerChanges;
			B.VacancyEpisodes = Fact.VacancyEpisodes;
			B.bWasOccupied = Fact.bWasOccupied;
			B.LastObservedDay = Fact.LastObservedDay;
			B.Events = Fact.Events;
			// La forme : celle du fondateur une fois la maison prise, sinon celle du type a la premiere vue.
			const bool bFounded = Fact.bFormFixed && Fact.Type == AnastasisVillage::HouseType;
			ProgramFor(Fact.Type, Fact.FormHousePhase, bFounded ? Fact.FounderJob : FString(), bFounded ? Fact.FounderHousehold : 0,
				B.Program, B.ProgramCause);

			int32& Logged = LoggedEvents.FindOrAdd(Pair.Key);
			for (; Logged < Fact.Events.Num(); ++Logged)
			{
				const FEvent& E = Fact.Events[Logged];
				const FString Shape = E.Kind == EEvent::Founded
					? FString::Printf(TEXT(" -> %s : %s"), AnastasisArchitecture::Get(B.Program).Id, *B.ProgramCause)
					: FString();
				UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_SETTLEMENT event day=%d id=%s %s %s%s"),
					E.Day, *B.Id, EventName(E.Kind), *E.Detail, *Shape);
				++Written;
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
