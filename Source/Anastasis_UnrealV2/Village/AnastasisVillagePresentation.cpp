#include "Village/AnastasisVillagePresentation.h"

#include "Anastasis_UnrealV2.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Core/AnastasisStateDigest.h"
#include "Village/AnastasisVillage.h"
#include "Village/AnastasisVillageBuilding.h"
#include "Village/AnastasisVillageInteractionSubsystem.h"
#include "Village/AnastasisVillagerLooks.h"
#include "Village/AnastasisVillagerVisual.h"
#include "World/AnastasisWorld.h"
#include "WorldView/AnastasisPresentationRegistry.h"
#include "Animation/BlendSpace.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "WorldView/AnastasisWorldView.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "EngineUtils.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "Village/AnastasisArchitecture.h"

namespace
{
	TAutoConsoleVariable<int32> CVarArchitecture(
		TEXT("anastasis.Village.Architecture"),
		1,
		TEXT("ARCHITECTURE_SCALE_001 : 1 = maisonnees a l'echelle humaine (corps + assise terrassee + parcelle defrichee) ; ")
		TEXT("0 = anciens meshes de la tuile de 4 m (temoin d'A/B). Lu a la creation de chaque batiment."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarArchitectureClearMargin(
		TEXT("anastasis.Village.ArchitectureClearMargin"),
		120.f,
		TEXT("ARCHITECTURE_SCALE_001 : marge (cm) autour de l'emprise d'une maisonnee ou herbe et arbres sont ecartes."),
		ECVF_Default);

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
		// La cabane du joueur (ma-cabane-001) se presente comme une maison : on y dort.
		if (Type == AnastasisVillage::HouseType || Type == AnastasisVillage::CabinType)
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
    double Ground = 0.0;
    if (Terrain && TraceGround(PresentationWorld, Position.X, Position.Y, Ground))
    {
        Position.Z = Ground;
    }
    return Position;
}

bool FAnastasisVillagePresentation::TraceGround(UWorld* PresentationWorld, const double X, const double Y, double& OutZ)
{
    if (!PresentationWorld)
    {
        return false;
    }
    for (TActorIterator<AAnastasisWorldEmbodiment> It(PresentationWorld); It; ++It)
    {
        // Query this world's rendered collision mesh. The Forge global cache can
        // belong to another editor/PIE world and is not a safe village anchor.
        TArray<UProceduralMeshComponent*> Surfaces;
        It->GetComponents(Surfaces);
        for (UProceduralMeshComponent* Surface : Surfaces)
        {
            if (!Surface->IsVisible() || !Surface->IsCollisionEnabled()) continue;
            const FBox Bounds = Surface->Bounds.GetBox();
            FHitResult Hit;
            if (Surface->LineTraceComponent(Hit,
                FVector(X, Y, Bounds.Max.Z + 1000),
                FVector(X, Y, Bounds.Min.Z - 1000),
                FCollisionQueryParams(SCENE_QUERY_STAT(VillageGround), true)))
            {
                OutZ = Hit.ImpactPoint.Z;
                return true;
            }
        }
        break;
    }
    return false;
}

int32 FAnastasisVillagePresentation::SettleArchitecture(AAnastasisVillageBuilding& Actor,
    const AnastasisVillage::FVillage& Village, const FString& BuildingId, UWorld* PresentationWorld,
    AnastasisArchitecture::EVariant Variant)
{
    const AnastasisVillage::FBuilding* Building = Village.FindBuilding(BuildingId);
    if (!Building || CVarArchitecture.GetValueOnGameThread() == 0
        || !Actor.ApplyArchitecture(Variant))
    {
        return -1;
    }
    const AnastasisArchitecture::FArchetype& A = AnastasisArchitecture::Get(Variant);
    const FTransform Xf = Actor.GetActorTransform();

    // ARCH-10 : la cour se pose a la mediane du terrain sous l'emprise. Aval : terrasse sur soutenement ;
    // amont : l'assise s'enterre. Jamais posee sur le seul point central.
    TArray<double> Heights;
    for (const FVector2D& L : AnastasisArchitecture::FootprintSamples(A, 5))
    {
        const FVector W = Xf.TransformPosition(FVector(L.X, L.Y, 0.0));
        double Z = 0.0;
        if (TraceGround(PresentationWorld, W.X, W.Y, Z))
        {
            Heights.Add(Z);
        }
    }
    double Pad = Xf.GetLocation().Z;
    double Lo = Pad;
    double Hi = Pad;
    if (Heights.Num() > 0)
    {
        Pad = AnastasisArchitecture::PadLevel(Heights);
        Lo = FMath::Min(Heights);
        Hi = FMath::Max(Heights);
    }
    Actor.SetPadOffset(Pad - Xf.GetLocation().Z);

    // La parcelle defrichee : rien ne pousse dans la cour ni a travers un toit.
    const double Margin = FMath::Max(0.f, CVarArchitectureClearMargin.GetValueOnGameThread());
    const FBox2D Zone(A.Footprint.Min - FVector2D(Margin), A.Footprint.Max + FVector2D(Margin));
    FBox WorldBox(ForceInit);
    const FVector2D Corners[4] = {Zone.Min, Zone.Max, FVector2D(Zone.Min.X, Zone.Max.Y), FVector2D(Zone.Max.X, Zone.Min.Y)};
    for (const FVector2D& Corner : Corners)
    {
        WorldBox += Xf.TransformPosition(FVector(Corner.X, Corner.Y, 0.0));
    }
    WorldBox.Min.Z = -1.0e9;
    WorldBox.Max.Z = 1.0e9;
    int32 Cleared = 0;
    int32 Components = 0;
    for (TActorIterator<AActor> It(PresentationWorld); It; ++It)
    {
        if (It->IsA<AAnastasisVillageBuilding>() || It->IsA<AAnastasisVillagerVisual>())
        {
            continue;
        }
        TArray<UHierarchicalInstancedStaticMeshComponent*> Hisms;
        It->GetComponents(Hisms);
        for (UHierarchicalInstancedStaticMeshComponent* Hism : Hisms)
        {
            if (!Hism || Hism->GetInstanceCount() == 0 || !Hism->Bounds.GetBox().Intersect(WorldBox))
            {
                continue;
            }
            int32 Here = 0;
            for (int32 I = 0; I < Hism->GetInstanceCount(); ++I)
            {
                FTransform T;
                if (!Hism->GetInstanceTransform(I, T, true) || T.GetScale3D().IsNearlyZero(1.e-4))
                {
                    continue;
                }
                const FVector L = Xf.InverseTransformPosition(T.GetLocation());
                if (Zone.IsInside(FVector2D(L.X, L.Y)))
                {
                    T.SetScale3D(FVector::ZeroVector);
                    Hism->UpdateInstanceTransform(I, T, true, false, true);
                    ++Here;
                }
            }
            if (Here > 0)
            {
                Hism->MarkRenderStateDirty();
                Cleared += Here;
                ++Components;
            }
        }
    }
    UE_LOG(LogAnastasis_UnrealV2, Display,
        TEXT("ANASTASIS_ARCH settle %s archetype=%s pad=%.0f ground=[%.0f..%.0f] samples=%d offset=%.0f cleared=%d hism=%d"),
        *BuildingId, A.Id, Pad, Lo, Hi, Heights.Num(), Pad - Xf.GetLocation().Z, Cleared, Components);
    return Cleared;
}


int32 FAnastasisVillagePresentation::Sync(
	const AnastasisVillage::FVillage& Village,
	const AnastasisWorld::FWorld& World,
	UAnastasisVillageInteractionSubsystem& Rooms,
	const double Daylight,
	const AnastasisMetabolism::EMode Mode,
	const int32 Day)
{
	int32 Changes = 0;
	// SETTLEMENT_MORPHOGENESIS_001 : la biographie d'abord, la forme ensuite (elle en depend).
	Ledger.Observe(Village, Day);
	const auto ProgramOf = [&](const AnastasisVillage::FBuilding& Building)
	{
		const AnastasisSettlement::FBiography* Bio = Ledger.Find(Building.Id);
		return Bio ? Bio->Program : AnastasisArchitecture::EVariant::HousePoor;
	};
	const auto Age = [&](AAnastasisVillageBuilding& Actor, const AnastasisVillage::FBuilding& Building)
	{
		// La patine monte avec les jours depuis l'achevement : a moitie en ~45 jours, presque pleine a 120.
		const AnastasisSettlement::FBiography* Bio = Ledger.Find(Building.Id);
		const double Days = Bio ? Bio->AgeDays(Day) : 0.0;
		Actor.SetWeathering(0.12 + 0.88 * (1.0 - FMath::Exp(-Days / 60.0)));
	};

	// ICEBERG_001 : ce que la simulation sait d'une maison, traduit en foyer. Rien n'est ecrit en retour.
	const auto ApplyMetabolism = [&](AAnastasisVillageBuilding& Actor, const AnastasisVillage::FBuilding& Building)
	{
		AnastasisMetabolism::FInput In;
		In.bDwelling = Building.Type == AnastasisVillage::HouseType || Building.Type == AnastasisVillage::CabinType;
		In.bCompleted = Building.IsCompleted();
		In.Daylight = Daylight;
		if (In.bDwelling && In.bCompleted)
		{
			In.Residents = Village.CountShelterOccupants(Building.Id);
			In.Inside = Village.InsideOf(Building.Id).Num();
			// Les jours vides vont de `vacantSinceDay` (reference) ; une maison finie ne vieillit pas
			// avant son achevement (la reference date aussi les chantiers, le visiteur n'en voit pas).
			In.VacantDays = AnastasisVillage::VacantAgeDays(Building, Day);
			if (Building.CompletedDay >= 0)
			{
				In.VacantDays = FMath::Min(In.VacantDays, FMath::Max(0, Day - Building.CompletedDay));
			}
		}
		const AnastasisMetabolism::FState State = AnastasisMetabolism::Derive(In, Mode);
		Actor.SetHearth(State.Hearth);
		Actor.SetNeglect(State.Neglect);
	};

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
			for (auto Use = InteractionUses.CreateIterator(); Use; ++Use)
			{
				if (Use.Value().BuildingId == It.Key())
				{
					if (Use.Value().Claim.IsValid()) Rooms.Release(Use.Value().Claim);
					Use.RemoveCurrent();
				}
			}
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
		if (const TWeakObjectPtr<AAnastasisVillageBuilding>* Existing = Actors.Find(Building.Id))
		{
			if (AAnastasisVillageBuilding* Actor = Existing->Get())
			{
				// Un foyer vient de prendre la maison : elle prend la forme de son fondateur.
				const AnastasisArchitecture::EVariant Program = ProgramOf(Building);
				if (Actor->HasArchitecture() && Actor->GetVariant() != Program)
				{
					const AnastasisArchitecture::EVariant Was = Actor->GetVariant();
					SettleArchitecture(*Actor, Village, Building.Id, Rooms.GetWorld(), Program);
					const AnastasisSettlement::FBiography* Bio = Ledger.Find(Building.Id);
					UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_SETTLEMENT reform id=%s %s -> %s day=%d cause=\"%s\""),
						*Building.Id, AnastasisArchitecture::Get(Was).Id, AnastasisArchitecture::Get(Program).Id, Day,
						Bio ? *Bio->ProgramCause : TEXT("-"));
					++Changes;
				}
				Actor->SetConstructionProgress(Building.Progress);
				Actor->SetSiteStock(Building.Materials.StockWood, Building.Materials.NeedWood,
					Building.Materials.StockStone, Building.Materials.NeedStone,
					Building.bHasMaterials && !Building.IsCompleted());
				ApplyMetabolism(*Actor, Building);
				Age(*Actor, Building);
			}
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
		SettleArchitecture(*Actor, Village, Building.Id, Rooms.GetWorld(), ProgramOf(Building));
		Actor->SetConstructionProgress(Building.Progress);
		Actor->SetSiteStock(Building.Materials.StockWood, Building.Materials.NeedWood,
			Building.Materials.StockStone, Building.Materials.NeedStone,
			Building.bHasMaterials && !Building.IsCompleted());
		ApplyMetabolism(*Actor, Building);
		Age(*Actor, Building);
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
	// Les sentiers nes du passage, et le seuil de chaque parcelle qui les rejoint.
	TArray<FAnastasisSettlementPaths::FDoorLink> Doors;
	for (const TPair<FString, TWeakObjectPtr<AAnastasisVillageBuilding>>& Pair : Actors)
	{
		const AAnastasisVillageBuilding* Actor = Pair.Value.Get();
		const AnastasisVillage::FBuilding* Building = Village.FindBuilding(Pair.Key);
		if (!Actor || !Building || !Actor->HasArchitecture() || Building->AccessPoints.Num() == 0) continue;
		const AnastasisArchitecture::FArchetype& A = AnastasisArchitecture::Get(Actor->GetVariant());
		FAnastasisSettlementPaths::FDoorLink Link;
		Link.Entry = Actor->GetActorTransform().TransformPosition(A.EntryLocal);
		Link.AccessTileX = FMath::FloorToInt32(Building->AccessPoints[0].X);
		Link.AccessTileY = FMath::FloorToInt32(Building->AccessPoints[0].Y);
		Doors.Add(Link);
	}
	Changes += Paths.Sync(Village, World, Rooms.GetWorld(), Doors) > 0 ? 1 : 0;
	return Changes;
}

void FAnastasisVillagePresentation::SyncInteractions(
	const AnastasisVillage::FVillage& Village,
	UAnastasisVillageInteractionSubsystem& Rooms)
{
	UWorld* World = Rooms.GetWorld();
	if (!World) return;
	const double Now = World->GetTimeSeconds();
	TSet<FString> Current;
	for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
	{
		if (!Npc.Inside.bActive) continue;
		const FString& BuildingId = Npc.Inside.BuildingId;
		AAnastasisVillageBuilding* Building = FindActor(BuildingId);
		if (!Building) continue;
		Current.Add(Npc.Id);
		FInteractionUse& Use = InteractionUses.FindOrAdd(Npc.Id);
		if (Use.BuildingId != BuildingId)
		{
			if (Use.Claim.IsValid()) Rooms.Release(Use.Claim);
			Use = FInteractionUse();
			Use.BuildingId = BuildingId;
		}
		if (Use.Claim.IsValid() || Now < Use.NextAttemptAt) continue;
		Use.NextAttemptAt = Now + 2.0;
		const FAnastasisVillageQueryResult Point = Rooms.FindNearestInteraction(
			Rooms.ActivityTagFor(Building->GetKind()), Building->GetActorLocation(), 500.0f, FName(*BuildingId));
		if (!Point.IsValid()) continue;
		const FSmartObjectClaimHandle Claim = Rooms.Claim(Point.Request.SlotHandle);
		if (!Claim.IsValid()) continue;
		if (!Rooms.Use(Claim))
		{
			Rooms.Release(Claim);
			continue;
		}
		Use.Claim = Claim;
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE interaction npc=%s building=%s activity=%s claimed"),
			*Npc.Id, *BuildingId, *Npc.Activity);
	}
	for (auto It = InteractionUses.CreateIterator(); It; ++It)
	{
		if (Current.Contains(It.Key())) continue;
		if (It.Value().Claim.IsValid()) Rooms.Release(It.Value().Claim);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE interaction npc=%s building=%s released"),
			*It.Key(), *It.Value().BuildingId);
		It.RemoveCurrent();
	}
}

bool FAnastasisVillagePresentation::HasInteractionClaim(const FString& NpcId) const
{
	const FInteractionUse* Use = InteractionUses.Find(NpcId);
	return Use && Use->Claim.IsValid();
}

int32 FAnastasisVillagePresentation::SyncVillagers(
	const AnastasisVillage::FVillage& Village,
	const AnastasisWorld::FWorld& World,
	UWorld* PresentationWorld,
	const UAnastasisPresentationRegistry& Registry,
	bool bEnabled,
	double StepAlpha,
	bool bStepped)
{
	int32 Changes = 0;
	for (auto It = Villagers.CreateIterator(); It; ++It)
	{
		AAnastasisVillagerVisual* Actor = It.Value().Get();
		if (bEnabled && Actor && Village.FindNpc(It.Key()))
		{
			continue;
		}
		if (Actor)
		{
			Actor->Destroy();
		}
		VillagerJobs.Remove(It.Key());
		VillagerTracks.Remove(It.Key());
		It.RemoveCurrent();
		++Changes;
	}
	if (!bEnabled || !PresentationWorld)
	{
		return Changes;
	}

	TMap<FName, TArray<int32>> Pools;
	for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
	{
		const FVector2D Now(Npc.X, Npc.Y);
		FVillagerTrack& Track = VillagerTracks.FindOrAdd(Npc.Id, FVillagerTrack{ Now, Now });
		if (bStepped)
		{
			// Un pas a eu lieu : l'ancienne position courante devient le depart de l'interpolation.
			Track.Prev = FVector2D::Distance(Track.Curr, Now) > 2.0 ? Now : Track.Curr;
			Track.Curr = Now;
		}
		else if (FVector2D::Distance(Track.Curr, Now) > 2.0)
		{
			Track = FVillagerTrack{ Now, Now };
		}
		const FVector2D Drawn = FMath::Lerp(Track.Prev, Track.Curr, FMath::Clamp(StepAlpha, 0.0, 1.0));
		const FVector Feet = SimToUnreal(World, Drawn.X, Drawn.Y, PresentationWorld);
		const FName Job(*Npc.JobId);
		AAnastasisVillagerVisual* Actor = FindVillager(Npc.Id);
		if (Actor && VillagerJobs.FindRef(Npc.Id) != Job)
		{
			// Le metier change la palette et l'identite visuelle du corps 3D.
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE villager %s job %s -> %s: body rebound"),
				*Npc.Id, *VillagerJobs.FindRef(Npc.Id).ToString(), *Job.ToString());
			Actor->Destroy();
			Villagers.Remove(Npc.Id);
			VillagerJobs.Remove(Npc.Id);
			Actor = nullptr;
			++Changes;
		}
		if (!Actor)
		{
			TArray<int32>* Pool = Pools.Find(Job);
			if (!Pool)
			{
				Pool = &Pools.Add(Job, AnastasisVillagerLooks::VillagePool(Registry.Villagers, Job));
			}
			// familles-feu-001 : un habitant dont la simulation connait le sexe et l'age porte un portrait de sa
			// categorie (Eudokia, une femme ; Michael, un enfant). Sans portrait de sa categorie, le tirage d'avant.
			int32 LookIndex = INDEX_NONE;
			EAnastasisVillagerCategory Category = EAnastasisVillagerCategory::AdultMale;
			if (AnastasisVillagerLooks::CategoryFor(Npc.Gender, Npc.Age, Category))
			{
				// Un pool par sexe et par age est petit : on passe un visage deja porte par un autre habitant.
				const TArray<int32> Person = AnastasisVillagerLooks::PersonPool(Registry.Villagers, Job, Category);
				const int32 First = AnastasisVillagerLooks::PickLook(Person, Npc.Id);
				if (First != INDEX_NONE)
				{
					TSet<FName> Worn;
					for (const TPair<FString, TWeakObjectPtr<AAnastasisVillagerVisual>>& Other : Villagers)
					{
						if (const AAnastasisVillagerVisual* OtherActor = Other.Value.Get()) Worn.Add(OtherActor->GetLookId());
					}
					const int32 Start = Person.IndexOfByKey(First);
					for (int32 Step = 0; Step < Person.Num() && LookIndex == INDEX_NONE; ++Step)
					{
						const int32 Candidate = Person[(Start + Step) % Person.Num()];
						if (!Worn.Contains(Registry.Villagers[Candidate].LookId)) LookIndex = Candidate;
					}
					if (LookIndex == INDEX_NONE) LookIndex = First;
				}
			}
			if (LookIndex == INDEX_NONE) LookIndex = AnastasisVillagerLooks::PickLook(*Pool, Npc.Id);
			if (LookIndex == INDEX_NONE)
			{
				if (!bWarnedNoLooks)
				{
					bWarnedNoLooks = true;
					UE_LOG(LogAnastasis_UnrealV2, Warning,
						TEXT("ANASTASIS_VILLAGE villager style: none for job %s (looks=%d pool=%d)"),
						*Job.ToString(), Registry.Villagers.Num(), Pool->Num());
				}
				continue;
			}
			const FAnastasisVillagerLook& Look = Registry.Villagers[LookIndex];
			const AnastasisVillagerLooks::FBodyLook BodyLook = AnastasisVillagerLooks::BodyLookFor(Look);
			USkeletalMesh* BodyMesh = (BodyLook.bFemale ? Registry.VillagerBodyFemale : Registry.VillagerBodyMale).LoadSynchronous();
			UBlendSpace* Locomotion = Registry.VillagerLocomotion.LoadSynchronous();
			UMaterialInterface* BodyMaterial = Registry.VillagerBodyMaterial.LoadSynchronous();
			if (!BodyMesh || !Locomotion || !BodyMaterial)
			{
				if (!bWarnedNoBody)
				{
					bWarnedNoBody = true;
					UE_LOG(LogAnastasis_UnrealV2, Error,
						TEXT("ANASTASIS_VILLAGE villager bodies: missing (mesh=%s locomotion=%s material=%s) -- no PNG fallback"),
						*(BodyLook.bFemale ? Registry.VillagerBodyFemale : Registry.VillagerBodyMale).ToString(),
						*Registry.VillagerLocomotion.ToString(), *Registry.VillagerBodyMaterial.ToString());
				}
				continue;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.ObjectFlags |= RF_Transient;
			Actor = PresentationWorld->SpawnActor<AAnastasisVillagerVisual>(Feet, FRotator::ZeroRotator, Params);
			if (!Actor)
			{
				continue;
			}
			const FName LookId = Look.LookId;
			Actor->SetLook(LookId, nullptr, nullptr);
			// Les PNG sont des references d'atelier. Les teintes deja mesurees guident le corps 3D,
			// mais aucun portrait n'est charge ni affiche dans le jeu.
			const bool bBody = Actor->SetBody(BodyMesh, Locomotion, BodyMaterial, BodyLook);
			if (!bBody)
			{
				UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_VILLAGE villager %s: failed to bind 3D body -- no PNG fallback"), *Npc.Id);
				Actor->Destroy();
				continue;
			}
			Actor->ShowBody(true);
#if WITH_EDITOR
			Actor->SetActorLabel(FString::Printf(TEXT("Villager_%s_%s"), *Npc.Id, *LookId.ToString()));
#endif
			Villagers.Add(Npc.Id, Actor);
			VillagerJobs.Add(Npc.Id, Job);
			++Changes;
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_VILLAGE villager %s (%s) -> look %s at %s body=%s"),
				*Npc.Id, *Job.ToString(), *LookId.ToString(), *Feet.ToCompactString(), TEXT("3d"));
		}
		Actor->MoveFeetTo(Feet);
		// Dedans : la simulation garde la position du seuil ; cacher le corps au lieu de le laisser au seuil.
		Actor->SetActorHiddenInGame(Npc.Inside.bActive);
	}
	return Changes;
}

AAnastasisVillagerVisual* FAnastasisVillagePresentation::FindVillager(const FString& NpcId) const
{
	const TWeakObjectPtr<AAnastasisVillagerVisual>* Found = Villagers.Find(NpcId);
	return Found ? Found->Get() : nullptr;
}

void FAnastasisVillagePresentation::Clear(UAnastasisVillageInteractionSubsystem* Rooms)
{
	if (Rooms)
	{
		for (const TPair<FString, FInteractionUse>& Use : InteractionUses)
		{
			if (Use.Value.Claim.IsValid()) Rooms->Release(Use.Value.Claim);
		}
	}
	InteractionUses.Reset();
	for (const TPair<FString, TWeakObjectPtr<AAnastasisVillagerVisual>>& Pair : Villagers)
	{
		if (AAnastasisVillagerVisual* Actor = Pair.Value.Get())
		{
			Actor->Destroy();
		}
	}
	Villagers.Reset();
	VillagerJobs.Reset();
	VillagerTracks.Reset();

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
	Ledger.Reset();
	Paths.Clear();
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
		if (Building.Progress < 1.0)
		{
			// Chantier : pieces posees, materiaux poses / devis, stock du site.
			const AnastasisBuild::FSiteMaterials& M = Building.Materials;
			DrawDebugString(
				World,
				Base + FVector(0, 0, bMesh ? 480.0 : Tile * 0.9),
				FString::Printf(TEXT("%s chantier %s  %d/%d  bois %d/%d (site %d)  pierre %d/%d (site %d)  bras=%d"),
					*Building.Id, *Building.Type, Building.PiecesPlaced, AnastasisBuild::PieceTotal,
					M.ConsumedWood, M.NeedWood, M.StockWood, M.ConsumedStone, M.NeedStone, M.StockStone, Building.Workers.Num()),
				nullptr,
				FColor::Silver,
				0.f);
			continue;
		}
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
		// Le corps du joueur est la camera (pawn lie, vue premiere personne) : sa sphere de 3,6 m centree a 5 m de
		// haut debordait a hauteur d'yeux, avec son etiquette, et collait a l'ecran. Le joueur ne se dessine pas.
		if (Village.IsPlayer(Npc)) continue;
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
				TEXT("%s %s/%s%s faim=%.1f soif=%.1f energie=%.1f%s | choix=%s phase=%s porte=%s nav=%s"),
				*Npc.Id,
				*Npc.Goal,
				*Npc.Activity,
				Npc.Inside.bActive ? *FString::Printf(TEXT(" [dans %s]"), *Npc.Inside.BuildingId) : TEXT(""),
				Npc.Needs.Hunger,
				Npc.Needs.Thirst,
				Npc.Needs.Energy,
				Npc.DestBuildingId.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" -> %s"), *Npc.DestBuildingId),
				*Npc.LastDecision.Winner,
				*Npc.LastDecision.Phase,
				*Npc.LastDecision.CommitGate,
				Npc.bPathFailed ? TEXT("echec") : Npc.bHasTarget ? TEXT("trajet") : TEXT("libre")),
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
			TEXT("ANASTASIS_VILLAGE building %s type=%s tile=(%.0f,%.0f) progress=%.2f pieces=%d site=%d/%d+%d,%d/%d+%d owner=%s occupants=%d/%d food=%d reserved=%d inside=[%s] access=%s users=[%s]"),
			*B.Id,
			*B.Type,
			B.X,
			B.Y,
			B.Progress,
			B.PiecesPlaced,
			B.Materials.ConsumedWood,
			B.Materials.NeedWood,
			B.Materials.StockWood,
			B.Materials.ConsumedStone,
			B.Materials.NeedStone,
			B.Materials.StockStone,
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

void FAnastasisVillagePresentation::DrawSettlementDebug(UWorld* World, const AnastasisVillage::FVillage& Village,
	const AnastasisWorld::FWorld& SimWorld, const int32 Day) const
{
	if (!World || SimWorld.W <= 0)
	{
		return;
	}
	// Passage par case (sim.traffic) : un point dont la taille et la couleur suivent le compteur ;
	// sentiers nes (Road) : un carre brun et le jour de leur naissance.
	const TArray<float>& Traffic = Village.GetTraffic();
	for (int32 Index = 0; Index < Traffic.Num(); ++Index)
	{
		const double T = Traffic[Index];
		if (T < 1.0) continue;
		const int32 X = Index % SimWorld.W;
		const int32 Y = Index / SimWorld.W;
		const FVector P = SimToUnreal(SimWorld, X + 0.5, Y + 0.5, World) + FVector(0, 0, 60);
		const float A = static_cast<float>(FMath::Clamp(T / 60.0, 0.0, 1.0));
		DrawDebugPoint(World, P, 6.f + 22.f * A, FLinearColor::LerpUsingHSV(FLinearColor(0.2f, 0.6f, 1.f), FLinearColor(1.f, 0.25f, 0.1f), A).ToFColor(true), false, 0.f);
	}
	for (const TPair<int32, AnastasisTraffic::FRoadTile>& Road : Village.GetRoads())
	{
		const int32 X = Road.Key % SimWorld.W;
		const int32 Y = Road.Key / SimWorld.W;
		const FVector P = SimToUnreal(SimWorld, X + 0.5, Y + 0.5, World);
		DrawDebugBox(World, P + FVector(0, 0, 30), FVector(160, 160, 25), FColor(150, 100, 50), false, 0.f, 0, 6.f);
		DrawDebugString(World, P + FVector(0, 0, 120), FString::Printf(TEXT("sentier j%d (passage %.0f)"), Road.Value.BuiltDay, Road.Value.TrafficAtBirth),
			nullptr, FColor(220, 170, 110), 0.f);
	}
	// La biographie au-dessus de chaque batiment : pourquoi cette forme, depuis quand, pour qui.
	for (const TPair<FString, AnastasisSettlement::FBiography>& Pair : Ledger.GetAll())
	{
		const AnastasisSettlement::FBiography& B = Pair.Value;
		const FVector P = SimToUnreal(SimWorld, B.CellX + 0.5, B.CellY + 0.5, World);
		DrawDebugString(World, P + FVector(0, 0, 1150),
			FString::Printf(TEXT("%s %s | fonde j%d par %s (%s) | age %d j | %d/%d dorment | %d nuits pleines | %s"),
				*B.Id, AnastasisArchitecture::Get(B.Program).Id, B.FoundedDay, B.Founder.IsEmpty() ? TEXT("-") : *B.Founder,
				B.FounderJob.IsEmpty() ? TEXT("-") : *B.FounderJob, B.AgeDays(Day), B.Occupants, B.PeakOccupants, B.CrowdedDays, *B.ProgramCause),
			nullptr, FColor(240, 220, 160), 0.f);
	}
}
