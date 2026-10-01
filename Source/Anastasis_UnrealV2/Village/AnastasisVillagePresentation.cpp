#include "Village/AnastasisVillagePresentation.h"

#include "Anastasis_UnrealV2.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Core/AnastasisStateDigest.h"
#include "Village/AnastasisVillage.h"
#include "Village/AnastasisVillageBuilding.h"
#include "Village/AnastasisVillageInteractionSubsystem.h"
#include "World/AnastasisWorld.h"
#include "WorldView/AnastasisWorldView.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"

namespace
{
	/** Le vocabulaire de la simulation vers l'enum de presentation de `main`. */
	bool BuildingHasBody(UWorld* World, const FString& SimId)
	{
		if (!World)
		{
			return false;
		}
		for (TActorIterator<AAnastasisVillageBuilding> It(World); It; ++It)
		{
			if (It->GetSimId() == FName(*SimId) && It->HasBody())
			{
				return true;
			}
		}
		return false;
	}

	bool KindForType(const FString& Type, EAnastasisVillageBuildingKind& OutKind)
	{
		if (Type == AnastasisVillage::WellType)
		{
			OutKind = EAnastasisVillageBuildingKind::Well;
			return true;
		}
		if (Type == AnastasisVillage::HouseType)
		{
			OutKind = EAnastasisVillageBuildingKind::House;
			return true;
		}
		if (Type == AnastasisVillage::GranaryType)
		{
			OutKind = EAnastasisVillageBuildingKind::Granary;
			return true;
		}
		return false;
	}

	double TileAltitude(const AnastasisWorld::FWorld& World, double SimX, double SimY)
	{
		const int32 TX = FMath::Clamp(FMath::FloorToInt32(SimX), 0, FMath::Max(0, World.W - 1));
		const int32 TY = FMath::Clamp(FMath::FloorToInt32(SimY), 0, FMath::Max(0, World.H - 1));
		const int32 Index = TY * World.W + TX;
		return World.Tiles.IsValidIndex(Index) ? World.Tiles[Index].Alt : 0.0;
	}
}

FVector FAnastasisVillagePresentation::SimToUnreal(const AnastasisWorld::FWorld& World, double SimX, double SimY, UWorld* PresentationWorld)
{
    AAnastasisWorldEmbodiment* Terrain = nullptr;
    if (PresentationWorld)
    {
        for (TActorIterator<AAnastasisWorldEmbodiment> It(PresentationWorld); It; ++It)
        {
            Terrain = *It;
            break;
        }
    }
    const double Scale = Terrain ? Terrain->GetSnapshot().SpatialScale : 1.0;
    FVector Position(SimX * AnastasisWorldView::TileWorldSize * Scale,
        SimY * AnastasisWorldView::TileWorldSize * Scale,
        AnastasisWorldView::AltitudeToUnreal(TileAltitude(World, SimX, SimY), Scale));
    if (Terrain)
    {
        // Query this world's rendered collision mesh. The Forge global cache can
        // belong to another editor/PIE world and is not a safe village anchor.
        TArray<UProceduralMeshComponent*> Surfaces;
        Terrain->GetComponents(Surfaces);
        for (UProceduralMeshComponent* Surface : Surfaces)
        {
            if (!Surface->IsVisible() || !Surface->IsCollisionEnabled()) continue;
            const FBox Bounds = Surface->Bounds.GetBox();
            FHitResult Hit;
            if (Surface->LineTraceComponent(Hit,
                FVector(Position.X, Position.Y, Bounds.Max.Z + 1000),
                FVector(Position.X, Position.Y, Bounds.Min.Z - 1000),
                FCollisionQueryParams(SCENE_QUERY_STAT(VillageGround), true)))
            {
                Position.Z = Hit.ImpactPoint.Z;
                break;
            }
        }
    }
    return Position;
}

int32 FAnastasisVillagePresentation::Sync(
	const AnastasisVillage::FVillage& Village,
	const AnastasisWorld::FWorld& World,
	UAnastasisVillageInteractionSubsystem& Rooms)
{
	int32 Changes = 0;

	// Retirer d'abord : un acteur dont l'enregistrement a disparu, ou qui a ete
	// detruit par ailleurs (fin de PIE, GC), ne doit pas survivre dans la table.
	for (auto It = Actors.CreateIterator(); It; ++It)
	{
		const bool bStillSimulated = Village.FindBuilding(It.Key()) != nullptr;
		AAnastasisVillageBuilding* Actor = It.Value().Get();
		if (bStillSimulated && Actor)
		{
			continue;
		}
		if (Actor)
		{
			Rooms.DestroyBuilding(Actor);
		}
		UE_LOG(
			LogAnastasis_UnrealV2,
			Display,
			TEXT("ANASTASIS_VILLAGE presentation: %s no longer simulated -> actor %s"),
			*It.Key(),
			Actor ? TEXT("destroyed") : TEXT("already gone"));
		It.RemoveCurrent();
		++Changes;
	}

	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
	{
		if (Actors.Contains(Building.Id))
		{
			continue;
		}
		EAnastasisVillageBuildingKind Kind;
		if (!KindForType(Building.Type, Kind))
		{
			continue;
		}
		const FVector Location = SimToUnreal(World, Building.X + 0.5, Building.Y + 0.5, Rooms.GetWorld());
		// Maison et grenier ont leur porte sur +Y. Le puits a son joug sur Y :
		// +X traverse entre les deux montants.
		const bool bDoorOnY = Kind == EAnastasisVillageBuildingKind::House
			|| Kind == EAnastasisVillageBuildingKind::Granary;
		FRotator Face(0.0, bDoorOnY ? -90.0 : 0.0, 0.0);
		if (Building.AccessPoints.Num() > 0)
		{
			const AnastasisVillage::FPoint& Approach = Building.AccessPoints[0];
			const double DX = Approach.X - (Building.X + 0.5);
			const double DY = Approach.Y - (Building.Y + 0.5);
			if (DX * DX + DY * DY > 1.0e-6)
			{
				Face.Yaw = FMath::RadiansToDegrees(FMath::Atan2(DY, DX)) + (bDoorOnY ? -90.0 : 0.0);
			}
		}
		AAnastasisVillageBuilding* Actor = Rooms.SpawnBuilding(Kind, FName(*Building.Id), FTransform(Face, Location));
		if (!Actor)
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_VILLAGE presentation: spawn refused for %s"), *Building.Id);
			continue;
		}
#if WITH_EDITOR
		Actor->SetActorLabel(FString::Printf(TEXT("SimBuilding_%s_%s"), *Building.Type, *Building.Id));
#endif
		Actors.Add(Building.Id, Actor);
		++Changes;
		UE_LOG(
			LogAnastasis_UnrealV2,
			Display,
			TEXT("ANASTASIS_VILLAGE presentation: %s (%s) tile=(%.0f,%.0f) -> actor %s at %s"),
			*Building.Id,
			*Building.Type,
			Building.X,
			Building.Y,
			*Actor->GetName(),
			*Location.ToCompactString());
	}
	return Changes;
}

void FAnastasisVillagePresentation::Clear(UAnastasisVillageInteractionSubsystem* Rooms)
{
	for (const TPair<FString, TWeakObjectPtr<AAnastasisVillageBuilding>>& Pair : Actors)
	{
		if (AAnastasisVillageBuilding* Actor = Pair.Value.Get())
		{
			if (Rooms)
			{
				Rooms->DestroyBuilding(Actor);
			}
			else
			{
				Actor->Destroy();
			}
		}
	}
	Actors.Reset();
}

AAnastasisVillageBuilding* FAnastasisVillagePresentation::FindActor(const FString& SimId) const
{
	const TWeakObjectPtr<AAnastasisVillageBuilding>* Found = Actors.Find(SimId);
	return Found ? Found->Get() : nullptr;
}

void FAnastasisVillagePresentation::DrawDebug(UWorld* World, const AnastasisVillage::FVillage& Village, const AnastasisWorld::FWorld& SimWorld)
{
	if (!World)
	{
		return;
	}
	const double Tile = AnastasisWorldView::TileWorldSize;
	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
	{
		const FVector Base = SimToUnreal(SimWorld, Building.X + 0.5, Building.Y + 0.5, World);
		const bool bHouse = Building.Type == AnastasisVillage::HouseType;
		const bool bGranary = Building.Type == AnastasisVillage::GranaryType;
		const bool bMesh = BuildingHasBody(World, Building.Id);
		if (!bMesh)
		{
			if (bGranary)
			{
				DrawDebugBox(World, Base + FVector(0, 0, Tile * 0.3), FVector(Tile * 0.4, Tile * 0.4, Tile * 0.3), FColor::Yellow, false, 0.f, 0, 4.f);
			}
			else if (bHouse)
			{
				DrawDebugBox(World, Base + FVector(0, 0, Tile * 0.35), FVector(Tile * 0.45, Tile * 0.45, Tile * 0.35), FColor::Orange, false, 0.f, 0, 4.f);
			}
			else
			{
				DrawDebugCylinder(World, Base, Base + FVector(0, 0, Tile * 0.6), Tile * 0.35, 16, FColor::Cyan, false, 0.f, 0, 4.f);
			}
		}
		for (const AnastasisVillage::FPoint& P : Building.AccessPoints)
		{
			DrawDebugPoint(World, SimToUnreal(SimWorld, P.X, P.Y, World) + FVector(0, 0, 10), 12.f, FColor::Turquoise, false, 0.f);
		}
		const TArray<FString> Users = Village.UsersOf(Building.Id);
		DrawDebugString(
			World,
			Base + FVector(0, 0, bMesh ? 480.0 : Tile * 0.9),
			bGranary
				? FString::Printf(TEXT("%s granary  nourriture %d (reserve %d)  dedans=%d  users=[%s]"),
					*Building.Id, Building.FoodPhysical, Building.FoodReserved,
					Village.InsideOf(Building.Id).Num(), *FString::Join(Users, TEXT(",")))
				: bHouse
				? FString::Printf(TEXT("%s house owner=%s  foyer/abri=%d/%d  dedans=%d  users=[%s]"),
					*Building.Id, Building.Owner.IsEmpty() ? TEXT("-") : *Building.Owner,
					Village.CountShelterOccupants(Building.Id), Village.ShelterCapacity(Building),
					Village.InsideOf(Building.Id).Num(), *FString::Join(Users, TEXT(",")))
				: FString::Printf(TEXT("%s %s  users=%d [%s]"), *Building.Id, *Building.Type, Users.Num(), *FString::Join(Users, TEXT(","))),
			nullptr,
			bGranary ? FColor::Yellow : bHouse ? FColor::Orange : FColor::Cyan,
			0.f);
	}

	for (const auto& S : Village.GetFoodSources())
	{
		const FVector Base = SimToUnreal(SimWorld, S.Position.X, S.Position.Y, World);
		const FColor Color = S.Remaining > 0 ? FColor::Green : FColor(130, 90, 60);
		DrawDebugCylinder(World, Base, Base + FVector(0,0,80), 160, 16, Color, false, 0.f, 0, 4.f);
		DrawDebugString(World, Base + FVector(0,0,180), FString::Printf(TEXT("Cueillette %d/%d%s"), S.Remaining, S.Initial, S.Remaining == 0 ? TEXT(" - epuisee") : TEXT("")), nullptr, Color, 0.f);
	}

	// Champs autour de chaque grenier, dans leur etat VIVANT (le monde genere ne bouge pas) :
	// vert tant qu'il reste a cueillir, hauteur = quantite ; brun une fois epuise.
	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
	{
		if (Building.Type != AnastasisVillage::GranaryType) continue;
		const int32 GX = FMath::FloorToInt32(Building.X);
		const int32 GY = FMath::FloorToInt32(Building.Y);
		for (int32 Y = FMath::Max(0, GY - 8); Y <= FMath::Min(SimWorld.H - 1, GY + 8); ++Y)
		{
			for (int32 X = FMath::Max(0, GX - 8); X <= FMath::Min(SimWorld.W - 1, GX + 8); ++X)
			{
				if (SimWorld.Tiles[Y * SimWorld.W + X].Resource != AnastasisWorld::EResource::Food) continue;
				const AnastasisWorld::FTile Live = Village.LiveTileAt(X, Y);
				const bool bLeft = Live.Resource == AnastasisWorld::EResource::Food && Live.Amount > 0;
				const double Height = bLeft ? Tile * 0.02 * Live.Amount : Tile * 0.02;
				const FVector Base = SimToUnreal(SimWorld, X + 0.5, Y + 0.5, World);
				DrawDebugBox(World, Base + FVector(0, 0, Height * 0.5), FVector(Tile * 0.45, Tile * 0.45, Height * 0.5),
					bLeft ? FColor::Green : FColor(130, 90, 60), false, 0.f, 0, 3.f);
				DrawDebugString(World, Base + FVector(0, 0, Height + 30.0), bLeft ? FString::FromInt(Live.Amount) : FString(TEXT("jachere")),
					nullptr, bLeft ? FColor::Green : FColor(130, 90, 60), 0.f);
			}
		}
	}

	for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
	{
		const FVector Pos = SimToUnreal(SimWorld, Npc.X, Npc.Y, World) + FVector(0, 0, Tile * 0.25);
		const float Thirst01 = static_cast<float>(FMath::Clamp(Npc.Needs.Thirst / 100.0, 0.0, 1.0));
		// Dedans : bleu, petite sphere au seuil (la position reste celle de l'entree).
		const FColor Color = Npc.Inside.bActive
			? FColor(80, 120, 255)
			: FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Thirst01).ToFColor(true);
		DrawDebugSphere(World, Pos, Tile * (Npc.Inside.bActive ? 0.1 : 0.18), 10, Color, false, 0.f, 0, 3.f);
		if (Npc.InventoryFood > 0)
		{
			DrawDebugBox(World, Pos + FVector(60,0,40), FVector(30,30,30), FColor::Yellow, false, 0.f, 0, 4.f);
			DrawDebugString(World, Pos + FVector(0,0,180), FString::Printf(TEXT("Sac: %d portions"),Npc.InventoryFood), nullptr,FColor::Yellow,0.f);
		}
		if (Npc.bHasTarget)
		{
			DrawDebugLine(World, Pos, SimToUnreal(SimWorld, Npc.Target.X, Npc.Target.Y, World) + FVector(0, 0, Tile * 0.25), Color, false, 0.f, 0, 2.f);
		}
		DrawDebugString(
			World,
			Pos + FVector(0, 0, Tile * 0.3),
			FString::Printf(
				TEXT("%s %s/%s%s faim=%.1f soif=%.1f energie=%.1f%s"),
				*Npc.Id,
				*Npc.Goal,
				*Npc.Activity,
				Npc.Inside.bActive ? *FString::Printf(TEXT(" [dans %s]"), *Npc.Inside.BuildingId) : TEXT(""),
				Npc.Needs.Hunger,
				Npc.Needs.Thirst,
				Npc.Needs.Energy,
				Npc.DestBuildingId.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" -> %s"), *Npc.DestBuildingId)),
			nullptr,
			Color,
			0.f);
	}
}

void FAnastasisVillagePresentation::LogStatus(const AnastasisVillage::FVillage& Village, double Time)
{
	UE_LOG(
		LogAnastasis_UnrealV2,
		Display,
		TEXT("ANASTASIS_VILLAGE status t=%.3f phase=%s buildings=%d actors=%d wells=%d houses=%d granaries=%d reservations=%d navVersion=%d digest=%s"),
		Time,
		AnastasisRhythm::PhaseId(AnastasisRhythm::VillagePhase(AnastasisRhythm::DayFracOf(Time))),
		Village.GetBuildings().Num(),
		Village.GetActors().Num(),
		Village.CountBuildings(AnastasisVillage::WellType),
		Village.CountBuildings(AnastasisVillage::HouseType),
		Village.CountBuildings(AnastasisVillage::GranaryType),
		Village.GetMealReservations().Num(),
		Village.GetNavVersion(),
		*AnastasisDigest::ToHex(Village.Digest()));

	for (const AnastasisVillage::FBuilding& B : Village.GetBuildings())
	{
		TArray<FString> Points;
		for (const AnastasisVillage::FPoint& P : B.AccessPoints)
		{
			Points.Add(FString::Printf(TEXT("(%.1f,%.1f)"), P.X, P.Y));
		}
		const TArray<FString> Users = Village.UsersOf(B.Id);
		UE_LOG(
			LogAnastasis_UnrealV2,
			Display,
			TEXT("ANASTASIS_VILLAGE building %s type=%s tile=(%.0f,%.0f) progress=%.2f owner=%s occupants=%d/%d food=%d reserved=%d inside=[%s] access=%s users=[%s]"),
			*B.Id,
			*B.Type,
			B.X,
			B.Y,
			B.Progress,
			B.Owner.IsEmpty() ? TEXT("-") : *B.Owner,
			Village.CountShelterOccupants(B.Id),
			Village.ShelterCapacity(B),
			B.FoodPhysical,
			B.FoodReserved,
			*FString::Join(Village.InsideOf(B.Id), TEXT(",")),
			*FString::Join(Points, TEXT(" ")),
			*FString::Join(Users, TEXT(",")));
	}

	for (const AnastasisVillage::FNpc& N : Village.GetActors())
	{
		const AnastasisVillage::FDecisionTrace& D = N.LastDecision;
		UE_LOG(
			LogAnastasis_UnrealV2,
			Display,
			TEXT("ANASTASIS_VILLAGE npc %s pos=(%.2f,%.2f) goal=%s activity=%s inside=%s home=%s shelter=%s hunger=%.2f food=%d meal=%s action=%s knows=%d meals=%d thirst=%.2f energy=%.2f hygiene=%.2f morale=%.2f health=%.2f sleepQ=%.2f target=%s dest=%s drinks=%d rests=%d | why t=%.2f phase=%s nous=%s(%.2f,u%.2f) eat=%.2f rest=%.2f drink=%.2f floor=%.2f(%s) top=%s gate=%s -> %s source=%s building=%s"),
			*N.Id,
			N.X,
			N.Y,
			*N.Goal,
			*N.Activity,
			N.Inside.bActive ? *N.Inside.BuildingId : TEXT("-"),
			N.HomeId.IsEmpty() ? TEXT("-") : *N.HomeId,
			N.ShelterId.IsEmpty() ? TEXT("-") : *N.ShelterId,
			N.Needs.Hunger,
			N.InventoryFood,
			Village.FindMealReservation(N.Id) ? *Village.FindMealReservation(N.Id)->BuildingId : TEXT("-"),
			*N.HungerAction.State,
			N.KnownStocks.Num(),
			N.MealsTaken,
			N.Needs.Thirst,
			N.Needs.Energy,
			N.Needs.Hygiene,
			N.Needs.Morale,
			N.Needs.Health,
			AnastasisVillage::FVillage::SleepQualityOf(N),
			N.bHasTarget ? *FString::Printf(TEXT("(%.1f,%.1f)"), N.Target.X, N.Target.Y) : TEXT("null"),
			N.DestBuildingId.IsEmpty() ? TEXT("-") : *N.DestBuildingId,
			N.DrinksTaken,
			N.RestsTaken,
			D.Time,
			D.Phase.IsEmpty() ? TEXT("-") : *D.Phase,
			D.NousType.IsEmpty() ? TEXT("-") : *D.NousType,
			D.NousScore,
			D.NousUrgency,
			D.EatRowScore,
			D.RestRowScore,
			D.DrinkRowScore,
			D.FloorScore,
			D.FloorGoal.IsEmpty() ? TEXT("-") : *D.FloorGoal,
			D.TableWinner.IsEmpty() ? TEXT("-") : *D.TableWinner,
			D.CommitGate.IsEmpty() ? TEXT("-") : *D.CommitGate,
			D.Winner.IsEmpty() ? TEXT("-") : *D.Winner,
			D.TargetSource.IsEmpty() ? TEXT("-") : *D.TargetSource,
			D.BuildingId.IsEmpty() ? TEXT("-") : *D.BuildingId);
	}
}
