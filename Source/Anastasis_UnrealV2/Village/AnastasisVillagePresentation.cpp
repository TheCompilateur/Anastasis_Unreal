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
	bool KindForType(const FString& Type, EAnastasisVillageBuildingKind& OutKind)
	{
		if (Type == AnastasisVillage::WellType)
		{
			OutKind = EAnastasisVillageBuildingKind::Well;
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
		AAnastasisVillageBuilding* Actor = Rooms.SpawnBuilding(Kind, FName(*Building.Id), FTransform(Location));
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
		DrawDebugCylinder(World, Base, Base + FVector(0, 0, Tile * 0.6), Tile * 0.35, 16, FColor::Cyan, false, 0.f, 0, 4.f);
		for (const AnastasisVillage::FPoint& P : Building.AccessPoints)
		{
			DrawDebugPoint(World, SimToUnreal(SimWorld, P.X, P.Y, World) + FVector(0, 0, 10), 12.f, FColor::Turquoise, false, 0.f);
		}
		const TArray<FString> Users = Village.UsersOf(Building.Id);
		DrawDebugString(
			World,
			Base + FVector(0, 0, Tile * 0.9),
			FString::Printf(TEXT("%s %s  users=%d [%s]"), *Building.Id, *Building.Type, Users.Num(), *FString::Join(Users, TEXT(","))),
			nullptr,
			FColor::Cyan,
			0.f);
	}

	for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
	{
		const FVector Pos = SimToUnreal(SimWorld, Npc.X, Npc.Y, World) + FVector(0, 0, Tile * 0.25);
		const float Thirst01 = static_cast<float>(FMath::Clamp(Npc.Needs.Thirst / 100.0, 0.0, 1.0));
		const FColor Color = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Thirst01).ToFColor(true);
		DrawDebugSphere(World, Pos, Tile * 0.18, 10, Color, false, 0.f, 0, 3.f);
		if (Npc.bHasTarget)
		{
			DrawDebugLine(World, Pos, SimToUnreal(SimWorld, Npc.Target.X, Npc.Target.Y, World) + FVector(0, 0, Tile * 0.25), Color, false, 0.f, 0, 2.f);
		}
		DrawDebugString(
			World,
			Pos + FVector(0, 0, Tile * 0.3),
			FString::Printf(
				TEXT("%s %s/%s soif=%.1f%s"),
				*Npc.Id,
				*Npc.Goal,
				*Npc.Activity,
				Npc.Needs.Thirst,
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
		TEXT("ANASTASIS_VILLAGE status t=%.3f buildings=%d actors=%d wells=%d navVersion=%d digest=%s"),
		Time,
		Village.GetBuildings().Num(),
		Village.GetActors().Num(),
		Village.CountBuildings(AnastasisVillage::WellType),
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
			TEXT("ANASTASIS_VILLAGE building %s type=%s tile=(%.0f,%.0f) progress=%.2f access=%s users=[%s]"),
			*B.Id,
			*B.Type,
			B.X,
			B.Y,
			B.Progress,
			*FString::Join(Points, TEXT(" ")),
			*FString::Join(Users, TEXT(",")));
	}

	for (const AnastasisVillage::FNpc& N : Village.GetActors())
	{
		const AnastasisVillage::FDecisionTrace& D = N.LastDecision;
		UE_LOG(
			LogAnastasis_UnrealV2,
			Display,
			TEXT("ANASTASIS_VILLAGE npc %s pos=(%.2f,%.2f) goal=%s activity=%s thirst=%.2f hygiene=%.2f morale=%.2f health=%.2f target=%s dest=%s drinks=%d | why t=%.2f drinkRow=%.2f floor=%.0f -> %s source=%s building=%s"),
			*N.Id,
			N.X,
			N.Y,
			*N.Goal,
			*N.Activity,
			N.Needs.Thirst,
			N.Needs.Hygiene,
			N.Needs.Morale,
			N.Needs.Health,
			N.bHasTarget ? *FString::Printf(TEXT("(%.1f,%.1f)"), N.Target.X, N.Target.Y) : TEXT("null"),
			N.DestBuildingId.IsEmpty() ? TEXT("-") : *N.DestBuildingId,
			N.DrinksTaken,
			D.Time,
			D.DrinkRowScore,
			AnastasisVillage::UnportedGoalsFloor,
			D.Winner.IsEmpty() ? TEXT("-") : *D.Winner,
			D.TargetSource.IsEmpty() ? TEXT("-") : *D.TargetSource,
			D.BuildingId.IsEmpty() ? TEXT("-") : *D.BuildingId);
	}
}
