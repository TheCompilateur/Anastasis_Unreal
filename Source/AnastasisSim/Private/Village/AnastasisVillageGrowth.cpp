#include "Village/AnastasisVillage.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisSimMath.h"
#include "Work/AnastasisBuild.h"
#include "World/AnastasisPathfinding.h"
#include "World/AnastasisWorld.h"

// ecart n°50 (valmire-grows-001) -- le village decide de ses batiments communs.
//
// Les maisons sont l'affaire des familles : chacune decide de batir la sienne et demande de l'aide (ecart
// n°48, relay-memoire-001). Le village, lui, decide ensemble, chaque soir, des batiments COMMUNS quand il en
// manque (decision d'Alexandre, 2026-10-08) : un grenier s'il n'y en a pas ou si la recolte deborde, un
// puits de plus quand on est trop nombreux pour ceux qui existent. Un habitant en trace l'emplacement ; le
// chantier s'ouvre sec, que porteur et bucherons approvisionnent, et que les batisseurs levent.
//
// La reference ouvre ses chantiers autrement : l'IA d'un habitant au but `build` appelle
// `tryOpenNewConstruction` (npc.js l. 5858), le type vient de `pickCollectiveBuilding` (classement du
// planificateur), l'emplacement de `findBuildSpot` (simulation.js l. 7848). De cela, seuls restent ici les
// creneaux (`constructionOpenSlots`, le grenier tenant lieu de ferme) et le repli de `findBuildSpot`
// (80 tirages autour du village). Le choix du type suit des regles propres, lisibles, a trancher :
//  - aucun grenier, ou tous pleins a 90 % de `GranaryFoodCap` : un grenier ;
//  - plus de `SoulsPerWell` ames par puits : un puits.
// Une case qui couperait un habitant du puits est refusee (le defaut que la chronique a montre,
// familles-feu-001).

namespace AnastasisVillage
{
	namespace
	{
		using AnastasisMath::Dist;

		int32 FloorInt(double V) { return static_cast<int32>(AnastasisJs::Floor(V)); }

		/** `COLLECTIVE.maxActiveSites` (collectivePriorities.js l. 74). */
		constexpr int32 MaxActiveSites = 2;
		/** `villageStruct().plazaRadius || 3`. */
		constexpr double PlazaRadius = 3.0;
		/** Regle propre (ecart n°50) : au-dela, le village veut un puits de plus. */
		constexpr int32 SoulsPerWell = 15;
		/** Regle propre (ecart n°50) : un grenier rempli a ce point deborde. */
		constexpr double GranaryFullShare = 0.9;
	}

	void FVillage::SetGrowthEnabled(bool bEnabled)
	{
		bGrowthEnabled = bEnabled;
	}

	int32 FVillage::GrowthOpenSlots() const
	{
		// `constructionOpenSlots` : deux chantiers, un seul tant que la « ferme » (ici le grenier) n'est pas
		// achevee ou qu'elle est elle-meme en chantier. Les maisons de famille en chantier comptent.
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
		// (le centre lui-meme peut etre sous le puits ou dans l'eau), a defaut la case libre la plus proche.
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

	FString FVillage::PickCommonBuilding(FString& OutCause)
	{
		if (GrowthOpenSlots() <= 0)
		{
			OutCause = TEXT("creneaux de chantier pleins");
			return FString();
		}
		int32 Granaries = 0;
		int32 FullGranaries = 0;
		bool bGranarySite = false;
		int32 Wells = 0;
		for (const FBuilding& B : Buildings.GetItems())
		{
			if (B.Type == GranaryType)
			{
				++Granaries;
				if (B.Progress < 1.0) bGranarySite = true;
				else if (B.FoodPhysical >= GranaryFullShare * GranaryFoodCap) ++FullGranaries;
			}
			if (B.Type == WellType) ++Wells;
		}
		const int32 Pop = Actors.Num();
		FIntPoint Probe;
		if (Granaries < 1 && FindBuildSpot(GranaryType, Probe))
		{
			OutCause = TEXT("aucun grenier : la recolte n'a pas ou aller");
			return GranaryType;
		}
		if (!bGranarySite && Granaries > 0 && FullGranaries == Granaries && FindBuildSpot(GranaryType, Probe))
		{
			OutCause = FString::Printf(TEXT("le grenier deborde : %d greniers pleins"), Granaries);
			return GranaryType;
		}
		if (Wells < FMath::DivideAndRoundUp(Pop, SoulsPerWell) && FindBuildSpot(WellType, Probe))
		{
			OutCause = FString::Printf(TEXT("%d ames pour %d puits"), Pop, Wells);
			return WellType;
		}
		OutCause = TEXT("rien ne manque");
		return FString();
	}

	FString FVillage::UpdateCommonBuildingsDaily(int32 InDay)
	{
		if (!bGrowthEnabled || !World || Actors.Num() == 0) return FString();
		FString Cause;
		const FString Type = PickCommonBuilding(Cause);
		LastBuildDecision = FString::Printf(TEXT("jour %d : %s : %s"), InDay, Type.IsEmpty() ? TEXT("-") : *Type, *Cause);
		if (Type.IsEmpty()) return FString();
		FIntPoint Spot;
		if (!FindBuildSpot(Type, Spot)) return FString();
		const FString Id = OpenSite(Type, Spot.X, Spot.Y, false);
		if (Id.IsEmpty()) return FString();

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
					return FString();
				}
			}
		}

		// Qui trace l'emplacement : un batisseur, a defaut le premier habitant (l'ordre du village).
		const FNpc* Opener = nullptr;
		for (const FNpc& N : Actors.GetItems())
		{
			if (IsPlayer(N)) continue;
			if (N.JobId == AnastasisBuild::JobBuilder) { Opener = &N; break; }
			if (!Opener) Opener = &N;
		}
		FBuilding* Site = Buildings.FindById(Id);
		if (!Site || !Opener) return Id;
		Site->BuilderId = Opener->Id;
		Site->OpenedById = Opener->Id;
		Site->OpenCause = Cause;
		++GrowthSitesOpened;
		// Un chantier sec a besoin d'un porteur : si le village n'en a pas (ou plus), celui qui trace le devient.
		if (MaterialCourierId.IsEmpty() || !FindNpc(MaterialCourierId))
		{
			MaterialCourierId = Opener->Id;
		}
		return Id;
	}
}
