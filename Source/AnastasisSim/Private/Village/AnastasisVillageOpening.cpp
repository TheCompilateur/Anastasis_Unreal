// opening-in-sim-001 -- le village d'ouverture du jeu Unreal, decide par la simulation.
//
// EXTENSION — ecart n°40. La reference fonde sa colonie par `populateFoundingLife` (fondateurs tires par
// `sim.rng`, `seedStarterHomes`, `ensureWorkplacesDaily`, `restoreFounderJobs`), qui n'est pas porte. Le jeu
// Unreal ouvre sur un autre village : un puits et des colons poses par l'hote (`SeedFirstWell`), puis les
// decisions ci-dessous, prises jusqu'ici par l'hote (`UAnastasisSimulationSubsystem::SeedOpening*` et
// `AssignCompletedOpeningHome`) et deplacees ici A L'IDENTIQUE : memes parcours, memes tests de chemin, meme
// ordre, aucun tirage. Le verrou du chantier d'ouverture vit dans le village ; `Bind` l'efface.

#include "Village/AnastasisVillage.h"

#include "World/AnastasisPathfinding.h"
#include "World/AnastasisWorld.h"

namespace AnastasisVillage
{
	const TCHAR* OpeningHomeStatusName(EOpeningHomeStatus Status)
	{
		switch (Status)
		{
		case EOpeningHomeStatus::Pending: return TEXT("pending");
		case EOpeningHomeStatus::Assigned: return TEXT("assigned");
		case EOpeningHomeStatus::Unassigned: return TEXT("unassigned");
		case EOpeningHomeStatus::SiteGone: return TEXT("site_gone");
		default: return TEXT("none");
		}
	}

	FOpeningReport FVillage::SeedOpeningVillage(int32 InDay, bool bOpenConstruction)
	{
		FOpeningReport Report;
		if (!World)
		{
			return Report;
		}
		const AnastasisWorld::FWorld& W = *World;
		const AnastasisPath::FWorldNavSource NavSource(Nav, W);

		// --- Foyer et poste du premier colon (ex-SeedOpeningHousehold) ---------------------------------
		FString OpeningWorkId;
		if (!Actors.GetItems().IsEmpty())
		{
			Report.bHousehold = true;
			const FNpc& Resident = Actors.GetItems()[0];
			const FPoint Origin{ Resident.X, Resident.Y };
			const FString ResidentId = Resident.Id;
			// familles-feu-001 : un batiment pose a cote du resident peut l'enfermer. Il ne suffit pas qu'il atteigne
			// sa porte : il doit encore atteindre le puits, sinon il meurt de soif chez lui (vu par la chronique).
			auto ReachesWell = [&]()
			{
				bool bAnyWell = false;
				for (const FBuilding& Well : Buildings.GetItems())
				{
					if (Well.Type != WellType || Well.Progress < 1.0) continue;
					bAnyWell = true;
					for (const FPoint& Door : Well.AccessPoints)
					{
						TArray<FPoint> Path;
						if (AnastasisPath::FindPath(NavSource, Origin, Door, {}, Path)) return true;
					}
				}
				return !bAnyWell;
			};
			auto Reachable = [&](const FBuilding& Building)
			{
				for (const FPoint& Door : Building.AccessPoints)
				{
					TArray<FPoint> Path;
					if (AnastasisPath::FindPath(NavSource, Origin, Door, {}, Path)) return ReachesWell();
				}
				return false;
			};
			// Anneaux carres croissants autour de (CX, CY) : la premiere case libre dont un seuil est atteint.
			auto PlaceReachable = [&](const FString& Type, int32 CX, int32 CY, int32 RadiusMax)
			{
				for (int32 R = 1; R <= RadiusMax; ++R)
				for (int32 DY = -R; DY <= R; ++DY)
				for (int32 DX = -R; DX <= R; ++DX)
				{
					if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
					const int32 X = CX + DX, Y = CY + DY;
					if (X < 2 || Y < 2 || X >= W.W - 2 || Y >= W.H - 2
						|| IsFootBlocked(X + 0.5, Y + 0.5)) continue;
					const FString Id = AddBuilding(Type, X, Y, 1.0, InDay);
					if (Id.IsEmpty()) continue;
					if (const FBuilding* Building = FindBuilding(Id))
					{
						if (Reachable(*Building)) return Id;
					}
					RemoveBuilding(Id);
				}
				return FString();
			};
			const FString HomeId = PlaceReachable(HouseType, FMath::FloorToInt32(Origin.X), FMath::FloorToInt32(Origin.Y), 5);
			if (!HomeId.IsEmpty()) AssignHome(ResidentId, HomeId);

			// Le poste doit desservir une vraie nourriture du monde, comme FirstFarmer.
			FString WorkId;
			TArray<const AnastasisWorld::FTile*> Fields;
			for (const AnastasisWorld::FTile& Tile : W.Tiles)
			{
				const AnastasisWorld::FTile Live = LiveTileAt(Tile.X, Tile.Y);
				if (Live.Resource == AnastasisWorld::EResource::Food && Live.Amount > 0) Fields.Add(&Tile);
			}
			// Meme tri que l'hote (TArray::Sort, distance de Manhattan au colon) : meme ordre, meme grenier.
			Fields.Sort([&](const AnastasisWorld::FTile& A, const AnastasisWorld::FTile& B)
			{
				const auto Distance = [&](const AnastasisWorld::FTile& T)
				{
					return FMath::Abs(T.X - FMath::FloorToInt32(Origin.X))
						+ FMath::Abs(T.Y - FMath::FloorToInt32(Origin.Y));
				};
				return Distance(A) < Distance(B);
			});
			for (const AnastasisWorld::FTile* Field : Fields)
			{
				TArray<FPoint> Path;
				if (!AnastasisPath::FindPath(NavSource, Origin, { Field->X + 0.5, Field->Y + 0.5 }, {}, Path)) continue;
				WorkId = PlaceReachable(GranaryType, Field->X, Field->Y, 4);
				if (!WorkId.IsEmpty()) break;
			}
			if (!WorkId.IsEmpty() && AssignWorkplace(ResidentId, AnastasisGather::JobFarmer, WorkId)) OpeningWorkId = WorkId;
			Report.ResidentId = ResidentId;
			Report.HomeId = HomeId;
			Report.WorkId = WorkId;
		}

		// --- Chantier d'ouverture et ses batisseurs (ex-SeedOpeningConstruction) ------------------------
		if (bOpenConstruction && Actors.GetItems().Num() >= 2)
		{
			Report.bConstructionTried = true;
			// Les habitants initiaux peuvent etre sur des ilots de navigation differents.
			// Chercher autour de chacun, puis choisir les ouvriers qui atteignent vraiment l'acces.
			bool bOpened = false;
			for (int32 Anchor = 1; Anchor < Actors.GetItems().Num() && !bOpened; ++Anchor)
			for (int32 R = 2; R <= 5 && !bOpened; ++R)
			for (int32 DY = -R; DY <= R && !bOpened; ++DY)
			for (int32 DX = -R; DX <= R && !bOpened; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
				const FNpc& AnchorNpc = Actors.GetItems()[Anchor];
				const int32 X = FMath::FloorToInt32(AnchorNpc.X) + DX;
				const int32 Y = FMath::FloorToInt32(AnchorNpc.Y) + DY;
				if (X < 2 || Y < 2 || X >= W.W - 2 || Y >= W.H - 2
					|| LiveTileAt(X, Y).Resource != AnastasisWorld::EResource::None
					|| IsFootBlocked(X + 0.5, Y + 0.5)) continue;
				// Chantier reellement sec : un porteur retire bois et pierre du terrain vivant.
				const FString SiteId = OpenSite(HouseType, X, Y, false);
				if (SiteId.IsEmpty()) continue;
				const FBuilding* Site = FindBuilding(SiteId);
				TArray<FString> ReachableBuilders;
				if (Site)
				{
					for (int32 N = 1; N < Actors.GetItems().Num(); ++N)
					{
						const FNpc& Builder = Actors.GetItems()[N];
						for (const FPoint& Door : Site->AccessPoints)
						{
							TArray<FPoint> Path;
							if (AnastasisPath::FindPath(NavSource, { Builder.X, Builder.Y }, Door, {}, Path))
							{
								ReachableBuilders.Add(Builder.Id);
								break;
							}
						}
						if (ReachableBuilders.Num() == 2) break;
					}
				}
				if (ReachableBuilders.IsEmpty())
				{
					RemoveBuilding(SiteId);
					continue;
				}
				// Le verrou de la regle d'attribution (AssignCompletedOpeningHome).
				OpeningSiteId = SiteId;
				OpeningHome = FOpeningHomeOutcome();
				OpeningHome.Status = EOpeningHomeStatus::Pending;
				OpeningHome.SiteId = SiteId;
				for (const FString& BuilderId : ReachableBuilders) SetJob(BuilderId, AnastasisBuild::JobBuilder);
				SetMaterialCourier(ReachableBuilders[0]);
				Report.SiteId = SiteId;
				Report.SiteX = X;
				Report.SiteY = Y;
				Report.Builders = ReachableBuilders;
				Report.CourierId = ReachableBuilders[0];
				if (const FBuilding* Opened = FindBuilding(SiteId))
				{
					Report.StockWood = Opened->Materials.StockWood;
					Report.StockStone = Opened->Materials.StockStone;
				}
				bOpened = true;
			}
		}

		// --- Colons encore libres embauches au grenier (ex-SeedOpeningWorkforce) ------------------------
		if (!OpeningWorkId.IsEmpty())
		{
			if (const FBuilding* Granary = FindBuilding(OpeningWorkId))
			{
				Report.bWorkforce = true;
				TArray<FString> Recruits;
				for (const FNpc& Npc : Actors.GetItems())
				{
					if (Npc.JobId != AnastasisGather::JobSettler) continue;
					for (const FPoint& Door : Granary->AccessPoints)
					{
						TArray<FPoint> Path;
						if (AnastasisPath::FindPath(NavSource, { Npc.X, Npc.Y }, Door, {}, Path))
						{
							Recruits.Add(Npc.Id);
							break;
						}
					}
					if (Recruits.Num() == 2) break;
				}
				for (const FString& Id : Recruits) AssignWorkplace(Id, AnastasisGather::JobFarmer, OpeningWorkId);
				Report.Farmers = Recruits;
			}
		}
		return Report;
	}

	void FVillage::AssignCompletedOpeningHome()
	{
		// ecart n°40 : sans chantier d'ouverture (tout village du harnais), rien n'est lu ni ecrit.
		if (OpeningSiteId.IsEmpty() || !World)
		{
			return;
		}
		const FBuilding* House = FindBuilding(OpeningSiteId);
		if (!House)
		{
			OpeningSiteId.Reset();
			OpeningHome.Status = EOpeningHomeStatus::SiteGone;
			OpeningHome.Time = Now;
			return;
		}
		if (!House->IsCompleted())
		{
			return;
		}
		const FString HouseId = OpeningSiteId;
		OpeningSiteId.Reset(); // Un seul essai apres l'achevement, aucun pathfinding par pas.
		const AnastasisPath::FWorldNavSource NavSource(Nav, *World);
		FString BestId;
		int32 BestLength = MAX_int32;
		for (const FNpc& Npc : Actors.GetItems())
		{
			if (Npc.JobId != AnastasisBuild::JobBuilder || !Npc.HomeId.IsEmpty()) continue;
			for (const FPoint& Door : House->AccessPoints)
			{
				TArray<FPoint> Path;
				if (AnastasisPath::FindPath(NavSource, { Npc.X, Npc.Y }, Door, {}, Path) && Path.Num() < BestLength)
				{
					BestId = Npc.Id;
					BestLength = Path.Num();
				}
			}
		}
		OpeningHome.Time = Now;
		if (!BestId.IsEmpty() && AssignHome(BestId, HouseId))
		{
			OpeningHome.Status = EOpeningHomeStatus::Assigned;
			OpeningHome.NpcId = BestId;
		}
		else
		{
			OpeningHome.Status = EOpeningHomeStatus::Unassigned;
		}
	}
}
