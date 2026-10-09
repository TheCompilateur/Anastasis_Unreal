#include "Village/AnastasisVillage.h"

#include "Work/AnastasisGather.h"
#include "World/AnastasisPathfinding.h"
#include "World/AnastasisWorld.h"

// ecart n°59 (faim-champs-001) -- quand le grenier se vide, des bras vont aux champs.
//
// L'etude d'une annee (annee-valmire-001) : trois cultivateurs nourrissent tout le village, six a dix adultes
// restent sans metier pour toujours, le grenier se vide des le premier hiver et des familles meurent de faim
// alors que des dizaines de milliers de portions restent sur pied. La reference rend leur metier aux fondateurs
// (`restoreFounderJobs`, `ensureWorkplacesDaily`, non portes : ecart n°40) ; ici le village embauche, chaque
// soir, quand il manque de reserves -- une regle propre, a trancher.

namespace AnastasisVillage
{
	FString FVillage::UpdateFieldHandsDaily(int32 InDay)
	{
		if (!bFieldHandsEnabled || !World) return FString();

		// Le grenier ou travaillent deja des cultivateurs (a defaut le premier acheve) ; la reserve de tous.
		int32 Stock = 0;
		const FBuilding* Granary = nullptr;
		int32 GranaryFarmers = -1;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Type != GranaryType || !B.IsCompleted()) continue;
			Stock += B.FoodPhysical;
			int32 Farmers = 0;
			for (const FNpc& N : Actors.GetItems()) if (IsGranaryWorker(N) && N.WorkplaceId == B.Id) ++Farmers;
			if (Farmers > GranaryFarmers) { GranaryFarmers = Farmers; Granary = &B; }
		}
		if (!Granary)
		{
			LastFieldHandsDecision = FString::Printf(TEXT("jour %d : aucun grenier"), InDay);
			return FString();
		}
		const int32 Pop = Actors.Num();
		int32 Farmers = 0;
		for (const FNpc& N : Actors.GetItems()) if (IsGranaryWorker(N)) ++Farmers;
		if (Stock >= Pop * FieldHandsStockDays)
		{
			LastFieldHandsDecision = FString::Printf(TEXT("jour %d : %d portions pour %d bouches, assez"), InDay, Stock, Pop);
			return FString();
		}
		if (Farmers >= FMath::DivideAndRoundUp(Pop, 2))
		{
			LastFieldHandsDecision = FString::Printf(TEXT("jour %d : %d aux champs sur %d, assez de bras"), InDay, Farmers, Pop);
			return FString();
		}

		// L'adulte sans metier le plus affame, qui atteint le grenier (la faim decide qui part le premier).
		const AnastasisPath::FWorldNavSource NavSource(Nav, *World);
		const FNpc* Best = nullptr;
		for (const FNpc& N : Actors.GetItems())
		{
			if (IsPlayer(N) || N.JobId != AnastasisGather::JobSettler || N.Id == MaterialCourierId) continue;
			if (N.Age > 0.0 && N.Age < 16.0) continue;
			if (Best && N.Needs.Hunger <= Best->Needs.Hunger) continue;
			bool bReaches = false;
			for (const FPoint& Door : Granary->AccessPoints)
			{
				TArray<FPoint> Path;
				if (AnastasisPath::FindPath(NavSource, { N.X, N.Y }, Door, {}, Path)) { bReaches = true; break; }
			}
			if (bReaches) Best = &N;
		}
		if (!Best)
		{
			LastFieldHandsDecision = FString::Printf(TEXT("jour %d : %d portions pour %d bouches, personne pour les champs"), InDay, Stock, Pop);
			return FString();
		}
		const FString Id = Best->Id;
		if (!AssignWorkplace(Id, AnastasisGather::JobFarmer, Granary->Id)) return FString();
		++FieldHandsHired;
		LastFieldHandsDecision = FString::Printf(TEXT("jour %d : %d portions pour %d bouches, %s part aux champs"), InDay, Stock, Pop, *Id);
		return Id;
	}
}
