#include "Village/AnastasisVillage.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"
#include "Village/AnastasisPlanner.h"
#include "Work/AnastasisBuild.h"
#include "World/AnastasisPathfinding.h"
#include "World/AnastasisWorld.h"

// ecart n°50 (valmire-grows-001) -- le village grandit de lui-meme.
//
// La reference ouvre ses chantiers par l'IA d'un habitant : celui dont le but est `build` et qui ne trouve
// rien a travailler appelle `tryOpenNewConstruction` (npc.js l. 5858). Le type vient de
// `pickCollectiveBuilding` (collectivePriorities.js l. 2209), l'emplacement de `findBuildSpot`
// (simulation.js l. 7848). Rien de cela n'etait porte : passe la premiere semaine, plus personne n'ouvrait
// de chantier (ecart n°18). Les arrivants, eux, viennent du monde exterieur (arrivants-001) : `maybeImmigrate`
// n'est pas porte ici.
//
// Porte ici, sur le catalogue du port (maison, grenier, puits), avec ces reductions :
//  - le grenier tient lieu de ferme : le jeu n'a pas de batiment `farm`, le fermier travaille au grenier ;
//    la porte « aucune ferme » devient « aucun grenier », et les creneaux montent a deux des qu'un grenier
//    est acheve (`constructionOpenSlots`) ;
//  - `findBuildSpot` : le repli de la reference seul (80 tirages autour du village), sans le classement
//    urbain `rankBuildSpots` ; une case qui couperait un habitant du puits est refusee (le defaut que la
//    chronique a montre, familles-feu-001) ;
//  - le chantier s'ouvre sec : ni graine de 10 % (`siteOpenSeedReady`), ni salaire (tresor vide : travail
//    collectif) ; porteur et bucherons l'approvisionnent depuis le monde (ecarts n°18 et n°43) ;
//  - la colonie posee ici ne sert qu'a decider de batir : ses biais sur les autres buts restent eteints
//    (`CollectiveDecisionOf`).

namespace AnastasisVillage
{
	namespace
	{
		using AnastasisMath::Dist;

		int32 FloorInt(double V) { return static_cast<int32>(AnastasisJs::Floor(V)); }

		/** `COLLECTIVE.maxActiveSites` (collectivePriorities.js l. 74). */
		constexpr int32 MaxActiveSites = 2;
		/** `COLLECTIVE.buildingPickMin`. */
		constexpr double BuildingPickMin = 15.0;
		/** `villageStruct().plazaRadius || 3`. */
		constexpr double PlazaRadius = 3.0;

		FString TypeLabel(const FString& Type)
		{
			if (Type == HouseType) return TEXT("maison");
			if (Type == GranaryType) return TEXT("grenier");
			if (Type == WellType) return TEXT("puits");
			return Type;
		}
	}

	void FVillage::SetGrowthEnabled(bool bEnabled)
	{
		bGrowthEnabled = bEnabled;
		if (bEnabled && !bHasColony)
		{
			// Une colonie neuve : `sim.colony` de la reference, tresor vide (le travail d'ouverture est collectif).
			bHasColony = true;
			bGrowthColony = true;
			Colony = AnastasisPlanner::FColonyState();
			ColonyTreasury = 0.0;
			UrgencyBucket.Reset();
			UrgencyCache.Reset();
		}
		else if (!bEnabled && bGrowthColony)
		{
			// Seule la colonie posee ici s'en va : celle du harnais (`RestoreColonyForHarness`) reste.
			bHasColony = false;
			bGrowthColony = false;
			Colony = AnastasisPlanner::FColonyState();
			ColonyTreasury.Reset();
			UrgencyBucket.Reset();
			UrgencyCache.Reset();
		}
	}

	int32 FVillage::GrowthOpenSlots() const
	{
		// `constructionOpenSlots` : deux chantiers, un seul tant que la « ferme » (ici le grenier) n'est pas
		// achevee ou qu'elle est elle-meme en chantier.
		int32 Cap = MaxActiveSites;
		bool bGranaryDone = false;
		int32 Active = 0;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Progress < 1.0)
			{
				++Active;
				if (B.Type == GranaryType) Cap = 1;
			}
			else if (B.Type == GranaryType)
			{
				bGranaryDone = true;
			}
		}
		if (!bGranaryDone) Cap = 1;
		return FMath::Max(0, Cap - Active);
	}

	bool FVillage::FindBuildSpot(const FString& Type, FIntPoint& OutSpot)
	{
		AnastasisBuild::FBuildCost Base;
		if (!World || !AnastasisBuild::BaseCost(Type, Base)) return false;
		const AnastasisPath::FWorldNavSource NavSource(Nav, *World);
		// `siteReachableFromVillage` : joignable depuis le coeur du village, ici le seuil du premier puits acheve
		// (le centre lui-meme peut etre sous le puits), a defaut le centre.
		FPoint From{ Settlement.X, Settlement.Y };
		bool bFrom = false;
		for (const FBuilding& Well : Buildings.GetItems())
		{
			if (Well.Type == WellType && Well.Progress >= 1.0 && Well.AccessPoints.Num() > 0)
			{
				From = Well.AccessPoints[0];
				bFrom = true;
				break;
			}
		}
		// Sans puits, la case libre la plus proche du centre (le centre lui-meme peut etre de l'eau).
		for (int32 R = 0; R <= 6 && !bFrom; ++R)
		for (int32 DY = -R; DY <= R && !bFrom; ++DY)
		for (int32 DX = -R; DX <= R && !bFrom; ++DX)
		{
			if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
			const double X = FloorInt(Settlement.X) + DX + 0.5, Y = FloorInt(Settlement.Y) + DY + 0.5;
			if (IsFootBlocked(X, Y)) continue;
			From = { X, Y };
			bFrom = true;
		}
		for (int32 I = 0; I < 80; ++I)
		{
			const double Angle = VillageRng.Next() * UE_DOUBLE_PI * 2.0;
			const double Radius = 3.0 + VillageRng.Next() * (8.0 + Buildings.Num() * 0.35);
			const int32 X = FloorInt(Settlement.X + FMath::Cos(Angle) * Radius);
			const int32 Y = FloorInt(Settlement.Y + FMath::Sin(Angle) * Radius);
			if (X < 2 || Y < 2 || X >= World->W - 2 || Y >= World->H - 2) continue;
			// `canPlanBuildingOn` : ni eau, ni route, ni batiment sur la case, hors de la placette.
			const AnastasisWorld::FTile Tile = LiveTileAt(X, Y);
			if (Tile.Type == AnastasisWorld::ETileType::Water || Tile.Type == AnastasisWorld::ETileType::Road) continue;
			bool bTaken = false;
			for (const FBuilding& B : Buildings.GetItems())
			{
				if (FloorInt(B.X) == X && FloorInt(B.Y) == Y) { bTaken = true; break; }
			}
			if (bTaken) continue;
			if (Type != WellType && Dist(X, Y, Settlement.X, Settlement.Y) <= PlazaRadius - 0.25) continue;
			if (IsFootBlocked(X + 0.5, Y + 0.5)) continue;
			TArray<FPoint> Path;
			if (!AnastasisPath::FindPath(NavSource, From, { X + 0.5, Y + 0.5 }, {}, Path)) continue;
			OutSpot = FIntPoint(X, Y);
			return true;
		}
		return false;
	}

	FString FVillage::PickCollectiveBuilding(FString& OutCause)
	{
		if (GrowthOpenSlots() <= 0)
		{
			OutCause = TEXT("creneaux de chantier pleins");
			return FString();
		}
		const int32 Pop = Actors.Num();
		int32 Granaries = 0;
		int32 Houses = 0;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Type == GranaryType) ++Granaries;
			if (B.Type == HouseType) ++Houses;
		}
		FIntPoint Probe;
		// La « ferme vitale » de la reference : ici le grenier, ou la recolte se depose.
		if (Granaries < 1 && FindBuildSpot(GranaryType, Probe))
		{
			OutCause = TEXT("aucun grenier : la recolte n'a pas ou aller");
			return GranaryType;
		}

		AnastasisPlanner::FPlannerVillage View = BuildPlannerView();
		const double HousingCap = AnastasisPlanner::HousingCapacity(View) + AnastasisPlanner::PendingHousingCapacity(View);
		const bool bHousingCrisis = HousingCap <= Pop;
		// « Un toit avant la filiere » : crise du logement averee et aucune maison planifiee. Les materiaux
		// viennent du monde (porteur, bucherons) : la porte `houseMaterialsReachable` est tenue pour ouverte.
		if (bHousingCrisis && Houses < 1 && FindBuildSpot(HouseType, Probe))
		{
			OutCause = FString::Printf(TEXT("crise du logement : %d ames pour %d places, aucune maison planifiee"), Pop, FMath::FloorToInt32(HousingCap));
			WritePlannerView(View);
			return HouseType;
		}

		// Le classement general (`boostedBuildingScores`), seuil 15, premier candidat qui a un emplacement.
		const AnastasisPlanner::FOrderedMap Scores = AnastasisPlanner::BoostedBuildingScores(View);
		WritePlannerView(View);
		TArray<TPair<FString, double>> Ranked;
		for (const TPair<FString, double>& S : Scores.Items)
		{
			if (S.Value >= BuildingPickMin) Ranked.Add(S);
		}
		// `sort((a, b) => b[1] - a[1])` : tri stable, l'ordre d'insertion departage.
		Ranked.StableSort([](const TPair<FString, double>& A, const TPair<FString, double>& B) { return A.Value > B.Value; });
		for (const TPair<FString, double>& Candidate : Ranked)
		{
			if (!FindBuildSpot(Candidate.Key, Probe)) continue;
			if (Candidate.Key == HouseType && bHousingCrisis)
			{
				OutCause = FString::Printf(TEXT("crise du logement : %d ames pour %d places"), Pop, FMath::FloorToInt32(HousingCap));
			}
			else
			{
				OutCause = FString::Printf(TEXT("classement : %s %.0f en tete de %d candidats"), *TypeLabel(Candidate.Key), Candidate.Value, Ranked.Num());
			}
			return Candidate.Key;
		}
		OutCause = Ranked.Num() ? TEXT("aucun candidat classe n'a d'emplacement constructible") : TEXT("aucun candidat au-dessus du seuil");
		return FString();
	}

	bool FVillage::TryOpenNewConstruction(FNpc& Npc)
	{
		if (!bGrowthEnabled || !World || GrowthOpenSlots() <= 0) return false;
		FString Cause;
		const FString Type = PickCollectiveBuilding(Cause);
		LastBuildDecision = (Type.IsEmpty() ? FString(TEXT("-")) : Type) + TEXT(" : ") + Cause;
		if (Type.IsEmpty()) return false;
		// « Une seule ferme en chantier a la fois » : un seul grenier.
		if (Type == GranaryType)
		{
			for (const FBuilding& B : Buildings.GetItems())
			{
				if (B.Type == GranaryType && B.Progress < 1.0) return false;
			}
		}
		FIntPoint Spot;
		if (!FindBuildSpot(Type, Spot)) return false;
		const FString Id = OpenSite(Type, Spot.X, Spot.Y, false);
		if (Id.IsEmpty()) return false;

		// Un batiment pose ne doit couper personne de l'eau (familles-feu-001 : mort de soif chez soi).
		const AnastasisPath::FWorldNavSource NavSource(Nav, *World);
		TArray<FPoint> WellDoors;
		for (const FBuilding& Well : Buildings.GetItems())
		{
			if (Well.Type == WellType && Well.Progress >= 1.0) WellDoors.Append(Well.AccessPoints);
		}
		if (WellDoors.Num() > 0)
		{
			for (const FNpc& Other : Actors.GetItems())
			{
				if (Other.Inside.bActive || IsFootBlocked(Other.X, Other.Y)) continue;
				bool bReaches = false;
				for (const FPoint& Door : WellDoors)
				{
					TArray<FPoint> Path;
					if (AnastasisPath::FindPath(NavSource, { Other.X, Other.Y }, Door, {}, Path)) { bReaches = true; break; }
				}
				if (!bReaches)
				{
					RemoveBuilding(Id);
					LastBuildDecision += FString::Printf(TEXT(" ; refuse en (%d,%d) : couperait %s du puits"), Spot.X, Spot.Y, *Other.Id);
					return false;
				}
			}
		}

		FBuilding* Site = Buildings.FindById(Id);
		if (!Site) return false;
		Site->BuilderId = Npc.Id;
		Site->OpenedById = Npc.Id;
		Site->OpenCause = Cause;
		Npc.BuildBinding = Id;
		++GrowthSitesOpened;
		// Un chantier sec a besoin d'un porteur : si le village n'en a pas (ou plus), l'habitant qui ouvre le
		// devient (la reference demande des livraisons, `requestSiteDeliveries`, ici non portees).
		if (MaterialCourierId.IsEmpty() || !FindNpc(MaterialCourierId))
		{
			MaterialCourierId = Npc.Id;
		}
		return true;
	}
}
