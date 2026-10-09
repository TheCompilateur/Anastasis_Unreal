// player-minimal-001 -- le joueur minimal, cote Unreal.
//
// Le joueur est un habitant de la simulation (FVillage::PlayerPersonId). L'hote ne fait que trois choses :
//   1. la direction du pawn local devient la direction de marche du corps incarne (avant les pas) ;
//   2. le pawn est pose sur ce corps apres les pas -- la camera suit l'habitant, jamais un fantome --
//      et sa carte portrait est cachee (on ne se voit pas soi-meme) ;
//   3. le temoin TIME_WARP_001 regarde cet habitant et dit au village ce qu'il en a vu.
// Collisions, vitesse, besoins : ceux du corps simule. Le pawn n'a plus de marche propre tant qu'il est lie.

#include "Sim/AnastasisSimulationSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Components/CapsuleComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Life/AnastasisBonds.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Village/AnastasisArchitecture.h"
#include "Village/AnastasisVillageBuilding.h"
#include "Village/AnastasisVillagerVisual.h"
#include "Work/AnastasisBuild.h"
#include "WorldView/AnastasisVisualMode.h"

/** ma-cabane-001 : ou le corps se tient dans sa cabane, cm locaux (a cote du banc-lit, loin du foyer). */
static const FVector2D CabinRestSpot(-110.0, -60.0);

static TAutoConsoleVariable<int32> CVarPlayerPawn(
	TEXT("anastasis.Player.Pawn"),
	1,
	TEXT("player-minimal-001: 1 = the local pawn follows the incarnated inhabitant (its input drives the simulated body, it is placed on it, its portrait card is hidden). 0 = the pawn walks on its own; the inhabitant is driven only by Anastasis.Player.Move."),
	ECVF_Default);

// player-start-001 (mandat d'Alexandre, 2026-10-08) : appuyer sur Play = debut du jeu. Nom distinct de la commande
// Anastasis.Player.Arrive (le ConsoleManager ignore la casse : meme nom = Fatal au chargement de la DLL).
static TAutoConsoleVariable<int32> CVarPlayerAutoArrive(
	TEXT("anastasis.Player.AutoArrive"),
	1,
	TEXT("player-start-001: 1 = when play begins, once the start village is seeded, the player-inhabitant arrives in it (the "
		"Anastasis.Player.Arrive path) and the pawn faces the well; anastasis.Visual.Mode 2 forces it. 0 = observer start, as "
		"before (scripted editors launched by tools/unreal get 0: Start-AnastasisEditor, editor-batch). Read when the start village is seeded."),
	ECVF_Default);

FString UAnastasisSimulationSubsystem::ArrivePlayer(double TileX, double TileY, const TCHAR* Why)
{
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const FString Id = Village.ArriveAsPlayer(TileX, TileY);
	// voix-conseil-001 : dans un village de foyers, le joueur a un nom, que les autres disent dans son dos (et un age :
	// sans lui, la chronique le prend pour un ancien). « Nikolaos », le nom que la chronique lui donnait deja. Ici et non dans
	// la commande : l'arrivee du debut de partie (TryAutoArrive) passe par la meme fonction.
	if (!Id.IsEmpty() && !Village.GetFamilies().IsEmpty())
	{
		const AnastasisVillage::FNpc* Arrived = Village.FindNpc(Id);
		if (Arrived && Arrived->Name.IsEmpty()) Village.SetIdentity(Id, TEXT("Nikolaos"), FString(), TEXT("male"), 30.0);
	}
	const AnastasisVillage::FNpc* Player = Village.FindNpc(Id);
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER arrive %s at (%.2f,%.2f), %d inhabitants (%s)"),
		Id.IsEmpty() ? TEXT("refused") : *Id, Player ? Player->X : -1.0, Player ? Player->Y : -1.0, Village.GetActors().Num(), Why);
	return Id;
}

void UAnastasisSimulationSubsystem::TryAutoArrive()
{
	const bool bForced = AnastasisVisualMode::Get() == EAnastasisVisualMode::Player;
	if (CVarPlayerAutoArrive.GetValueOnGameThread() == 0 && !bForced)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER_START off (anastasis.Player.AutoArrive 0): observer start"));
		return;
	}
	if (Simulation.GetVillage().PlayerActor())
	{
		return; // deja incarne (une sauvegarde, une commande) : on ne cree pas un second joueur.
	}
	const FString Id = ArrivePlayer(-1.0, -1.0, TEXT("start of play"));
	bArrivedAtStart = !Id.IsEmpty();
	bFaceVillagePending = bArrivedAtStart;
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER_START %s %s"),
		bArrivedAtStart ? TEXT("arrived") : TEXT("refused"), bArrivedAtStart ? *Id : TEXT("(no free ground)"));
}

void UAnastasisSimulationSubsystem::FacePawnTowardVillage(APawn& Pawn)
{
	bFaceVillagePending = false;
	const AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	double TX = Village.GetSettlement().X;
	double TY = Village.GetSettlement().Y;
	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
	{
		if (Building.Type == AnastasisVillage::WellType)
		{
			TX = Building.X + 0.5;
			TY = Building.Y + 0.5;
			break;
		}
	}
	const FVector Target = FAnastasisVillagePresentation::SimToUnreal(Simulation.GetWorld(), TX, TY, GetWorld());
	const FVector From = Pawn.GetActorLocation();
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(Target.Y - From.Y, Target.X - From.X));
	// Regard un peu plonge : la placette et le puits a hauteur d'yeux, pas le ciel.
	const FRotator Look(-6.0, Yaw, 0.0);
	if (AController* Controller = Pawn.GetController())
	{
		Controller->SetControlRotation(Look);
	}
	Pawn.SetActorRotation(FRotator(0.0, Yaw, 0.0));
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER_START facing village yaw=%.1f distance=%.0f m"),
		Yaw, FVector::Dist2D(From, Target) / 100.0);
}

void UAnastasisSimulationSubsystem::SetScriptedDrive(double DX, double DY)
{
	bScriptedDrive = !(FMath::IsNearlyZero(DX) && FMath::IsNearlyZero(DY));
	ScriptedDrive = FVector2D(DX, DY);
	if (!bScriptedDrive)
	{
		Simulation.GetVillage().SetPlayerMovementInput(0.0, 0.0);
	}
}

void UAnastasisSimulationSubsystem::ApplyPlayerInput()
{
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	if (!Village.PlayerActor())
	{
		bScriptedDrive = false;
		return;
	}
	if (bScriptedDrive)
	{
		Village.SetPlayerMovementInput(ScriptedDrive.X, ScriptedDrive.Y);
		return;
	}
	if (const APawn* Pawn = BoundPawn.Get())
	{
		// Vecteur d'entree monde de la derniere frame (AddMovementInput). Axes : x simule = +X Unreal,
		// y simule = +Y Unreal, echelle uniforme (FAnastasisVillagePresentation::SimToUnreal) -- la
		// direction se transpose telle quelle.
		const FVector In = Pawn->GetLastMovementInputVector();
		Village.SetPlayerMovementInput(In.X, In.Y);
	}
}

void UAnastasisSimulationSubsystem::ObservePlayerTime(double SimSeconds, double Multiplier)
{
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisVillage::FNpc* Player = Village.PlayerActor();
	if (!Player)
	{
		WitnessPersonId.Reset();
		return;
	}
	if (Player->Id != WitnessPersonId)
	{
		// Une autre personne : le village se souvient de ce qu'il en a vu, pas de la precedente.
		Witness.Presence = Player->Presence;
		Witness.IdleSeconds = Player->IdleSeconds;
		WitnessPersonId = Player->Id;
	}
	const double IdleBefore = Witness.IdleSeconds;
	Witness.Observe(SimSeconds, Multiplier, FAnastasisSimulation::DayLength);
	Village.ObservePlayer(Witness.Presence, Witness.IdleSeconds - IdleBefore);
}

void UAnastasisSimulationSubsystem::UnbindPawn()
{
	if (APawn* Pawn = BoundPawn.Get())
	{
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER pawn released: %s walks on its own"), *Pawn->GetName());
	}
	BoundPawn.Reset();
}

void UAnastasisSimulationSubsystem::PlacePlayerPawn()
{
	const AnastasisVillage::FNpc* Player = Simulation.GetVillage().PlayerActor();
	UWorld* World = GetWorld();
	if (!Player || !World || CVarPlayerPawn.GetValueOnGameThread() == 0)
	{
		UnbindPawn();
		return;
	}
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Pawn)
	{
		return;
	}
	if (BoundPawn.Get() != Pawn)
	{
		UnbindPawn();
		BoundPawn = Pawn;
		// Plus de marche Unreal : l'entree est toujours consommee (ConsumeInputVector passe avant
		// tout le reste dans TickComponent), seule la simulation deplace le corps.
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			Character->GetCharacterMovement()->DisableMovement();
		}
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER pawn %s follows %s"), *Pawn->GetName(), *Player->Id);
	}
	double HalfHeight = 0.0;
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	FVector Feet = FAnastasisVillagePresentation::SimToUnreal(Simulation.GetWorld(), Player->X, Player->Y, World);
	// ma-cabane-001 : la simulation le dit dedans (il dort, il mange) ; le corps passe la porte et se tient pres du banc,
	// sur le sol de la piece. Ailleurs, il reste au seuil comme avant.
	bool bIndoors = false;
	if (Player->Inside.bActive)
	{
		const AnastasisVillage::FBuilding* Home = Simulation.GetVillage().FindBuilding(Player->Inside.BuildingId);
		const AAnastasisVillageBuilding* Actor = VillagePresentation.FindActor(Player->Inside.BuildingId);
		if (Home && Actor && Home->Type == AnastasisVillage::CabinType && Actor->HasArchitecture())
		{
			const AnastasisArchitecture::FArchetype& A = AnastasisArchitecture::Get(Actor->GetVariant());
			const double Floor = A.Rooms.Num() > 0 ? A.Rooms[0].FloorCm : A.DoorLocal.Z;
			Feet = Actor->GetActorTransform().TransformPosition(
				FVector(CabinRestSpot.X, CabinRestSpot.Y, Floor + Actor->GetPadOffset()));
			bIndoors = true;
		}
	}
	Pawn->SetActorLocation(Feet + FVector(0.0, 0.0, HalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
	if (bFaceVillagePending)
	{
		FacePawnTowardVillage(*Pawn);
	}
	if (bIndoors != bPawnIndoors)
	{
		bPawnIndoors = bIndoors;
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER pawn %s %s at %s"), bIndoors ? TEXT("inside") : TEXT("outside"),
			Player->Inside.bActive ? *Player->Inside.BuildingId : TEXT("-"), *Feet.ToCompactString());
	}
	if (AAnastasisVillagerVisual* Card = VillagePresentation.FindVillager(Player->Id))
	{
		Card->SetActorHiddenInGame(true);
	}
}

namespace
{
	/** Habitants qui voient `Player` a leur portee de compagnon (la regle FVillage::Sees). */
	int32 CountSeers(const AnastasisVillage::FVillage& Village, const AnastasisVillage::FNpc& Player)
	{
		int32 Seers = 0;
		for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
		{
			if (Npc.Id == Player.Id) continue;
			const double D = FMath::Sqrt(FMath::Square(Npc.X - Player.X) + FMath::Square(Npc.Y - Player.Y));
			if (AnastasisVillage::FVillage::Sees(Player, D, AnastasisBonds::MaxOutdoor)) ++Seers;
		}
		return Seers;
	}

	/** Le nom d'un but tel que le joueur le lit. */
	FString GoalLabel(const FString& Goal)
	{
		if (Goal == TEXT("drink")) return TEXT("boire");
		if (Goal == TEXT("eat")) return TEXT("manger");
		if (Goal == TEXT("rest")) return TEXT("dormir");
		if (Goal == TEXT("socialize")) return TEXT("parler");
		if (Goal == TEXT("relax")) return TEXT("se detendre");
		if (Goal == TEXT("gatherFood")) return TEXT("recolter");
		if (Goal == TEXT("deliver")) return TEXT("livrer");
		if (Goal == TEXT("build")) return TEXT("batir");
		if (Goal == TEXT("shelterRain")) return TEXT("s'abriter");
		if (Goal == TEXT("idle")) return TEXT("attendre");
		return Goal;
	}

	/**
	 * Le remede que le corps reclame quand il passe devant (ecart n°21) : c'est au joueur de le choisir. Vide si le
	 * corps ne parle pas. Memes seuils que `FVillage::BodyOverrides`.
	 */
	FString BodyAsks(const AnastasisVillage::FNpc& Npc)
	{
		namespace D = AnastasisVillage::PlayerDecision;
		if (Npc.Needs.Thirst >= D::ThirstRelease) return TEXT("drink");
		if (Npc.Needs.Energy <= D::EnergyRelease) return TEXT("rest");
		if (Npc.Needs.Hunger >= D::HungerRelease) return TEXT("eat");
		return FString();
	}

	/** Pourquoi une intention cede, en clair. */
	FString RefusalLabel(const FString& Reason)
	{
		if (Reason == AnastasisVillage::PlayerDecision::RefusalNotInTable) return TEXT("impossible ici et maintenant");
		if (Reason == AnastasisVillage::PlayerDecision::RefusalLocked) return TEXT("un verrou tient (orage, charge)");
		if (Reason == AnastasisVillage::PlayerDecision::RefusalBody) return TEXT("le corps passe devant");
		return Reason;
	}
}

void UAnastasisSimulationSubsystem::DrawPlayerOverlay() const
{
	const AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisVillage::FNpc* Player = Village.PlayerActor();
	if (!Player || !GEngine)
	{
		return;
	}
	GEngine->AddOnScreenDebugMessage(
		0xA51A53,
		0.0f,
		Player->Presence < 0.5 ? FColor::Orange : FColor::Cyan,
		FString::Printf(
			TEXT("JOUEUR %s  %s   presence %.0f %%  reputation %.0f  oisif %.1f j  vu par %d"),
			*Player->Id,
			*Player->Activity,
			Player->Presence * 100.0,
			Player->Reputation,
			Player->IdleSeconds / FAnastasisSimulation::DayLength,
			CountSeers(Village, *Player)));

	// player-goals-001 : la table du joueur, numerotee comme les touches 1 a 5, puis l'intention et le refus.
	FString Options;
	const TArray<AnastasisVillage::FPlayerGoalOption>& Table = Village.GetPlayerGoalOptions();
	for (int32 I = 0; I < FMath::Min(5, Table.Num()); ++I)
	{
		Options += FString::Printf(TEXT("  %d %s"), I + 1, *GoalLabel(Table[I].Goal));
	}
	const AnastasisVillage::FPlayerGoalChoice* Choice = Village.GetPlayerGoalChoice();
	const AnastasisVillage::FPlayerRefusal* Refusal = Village.GetPlayerRefusal();
	FString Intent = Choice ? FString::Printf(TEXT("   but: %s"), *GoalLabel(Choice->Goal)) : FString(TEXT("   but: aucun"));
	if (Refusal)
	{
		Intent += FString::Printf(TEXT("  -- refuse : %s"), *RefusalLabel(Refusal->Reason));
		// Le corps passe devant : dire ce qu'il reclame, sinon le joueur attend sans savoir quoi choisir.
		const FString Asks = BodyAsks(*Player);
		if (Refusal->Reason == AnastasisVillage::PlayerDecision::RefusalBody && !Asks.IsEmpty())
		{
			Intent += FString::Printf(TEXT(" : il faut %s"), *GoalLabel(Asks));
		}
	}
	GEngine->AddOnScreenDebugMessage(
		0xA51A54,
		0.0f,
		Refusal ? FColor::Orange : FColor::Cyan,
		TEXT("BUTS") + Options + TEXT("  0 rien") + Intent);
	if (!Village.GetFoodSources().IsEmpty())
	{
		int32 Remaining = 0;
		int32 Stock = 0;
		for (const AnastasisVillage::FFoodSource& Source : Village.GetFoodSources()) Remaining += Source.Remaining;
		for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings()) Stock += Building.FoodPhysical;
		GEngine->AddOnScreenDebugMessage(
			0xA51A55, 0.0f, FColor::Green,
			FString::Printf(TEXT("F7 recolter  F8 livrer  F9 manger | champ %d  sac %d  grenier %d  repas %d  faim %.0f"),
				Remaining, Player->InventoryFood, Stock, Player->MealsTaken, Player->Needs.Hunger));
	}
}

namespace
{
	UAnastasisSimulationSubsystem* PlayerHost(UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		if (!Host || !Host->GetSimulation().IsRunning())
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_PLAYER: no running simulation in this world (PIE only)"));
			return nullptr;
		}
		return Host;
	}
}

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisPlayerArrive(
	TEXT("Anastasis.Player.Arrive"),
	TEXT("Anastasis.Player.Arrive [TileX TileY] - an ordinary inhabitant arrives (default: settlement + (2, 3)) and is incarnated: "
		"he waits until driven (Nous never decides for him). The local pawn follows him (anastasis.Player.Pawn)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = PlayerHost(World))
		{
			const bool bAt = Args.Num() >= 2;
			Host->ArrivePlayer(
				bAt ? FCString::Atoi(*Args[0]) + 0.5 : -1.0,
				bAt ? FCString::Atoi(*Args[1]) + 0.5 : -1.0,
				TEXT("console"));
		}
	}));

// First playable material loop: one finite generated source, an empty depot, then the player's hand.
// The existing village scenario owns all resource setup; incarnation only changes who decides for its inhabitant.
static FAutoConsoleCommandWithWorld CmdAnastasisPlayerFoodLoop(
	TEXT("Anastasis.Player.FoodLoop"),
	TEXT("PIE food loop: replace the opening village with a finite source and empty granary, then incarnate its inhabitant. F7 gather, F8 deliver, F9 eat."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = PlayerHost(World);
		if (!Host || !Host->SeedFoodSupply()) return;
		AnastasisVillage::FVillage& Village = Host->GetSimulation().GetVillage();
		const TArray<AnastasisVillage::FNpc>& Actors = Village.GetActors();
		if (Actors.IsEmpty() || !Village.Incarnate(Actors[0].Id))
		{
			UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_PLAYER food loop: scenario has no incarnatable inhabitant"));
			return;
		}
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER food loop: incarnated %s; finite source, empty granary"),
			*Actors[0].Id);
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisPlayerIncarnate(
	TEXT("Anastasis.Player.Incarnate"),
	TEXT("Anastasis.Player.Incarnate <npc-N> - take over a living inhabitant."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = PlayerHost(World);
		if (!Host || !Args.IsValidIndex(0)) return;
		const bool bOk = Host->GetSimulation().GetVillage().Incarnate(Args[0]);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER incarnate %s -> %s"), *Args[0], bOk ? TEXT("ok") : TEXT("unknown id"));
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisPlayerRelease(
	TEXT("Anastasis.Player.Release"),
	TEXT("Give the incarnated inhabitant back to Nous; the pawn walks on its own again (observer mode)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = PlayerHost(World))
		{
			Host->SetScriptedDrive(0.0, 0.0);
			const FString Previous = Host->GetSimulation().GetVillage().Release();
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER release %s"), Previous.IsEmpty() ? TEXT("(nobody)") : *Previous);
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisPlayerMove(
	TEXT("Anastasis.Player.Move"),
	TEXT("Anastasis.Player.Move <dx> <dy> - walk direction of the incarnated inhabitant in simulation axes (x = +X, y = +Y Unreal), held until "
		"'Anastasis.Player.Move 0 0', which gives the direction back to the pawn. For agents and proofs."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = PlayerHost(World);
		if (!Host || Args.Num() < 2) return;
		const double DX = FCString::Atod(*Args[0]);
		const double DY = FCString::Atod(*Args[1]);
		Host->SetScriptedDrive(DX, DY);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER move %g %g"), DX, DY);
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisPlayerStatus(
	TEXT("Anastasis.Player.Status"),
	TEXT("Logs the incarnated inhabitant: position, goal, presence, reputation, idle days, how many inhabitants see him."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER status %s"), *UAnastasisSimulationDebugLibrary::GetPlayerStatus(World));
	}));

// --- La main du joueur (player-goals-001) ----------------------------------------------------------

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisPlayerGoal(
	TEXT("Anastasis.Player.Goal"),
	TEXT("Anastasis.Player.Goal <goal|none> - pose an intention for the incarnated inhabitant (drink, eat, rest, socialize, relax, "
		"gatherFood, deliver, build, shelterRain), held until withdrawn ('none'). It is decided at his next thought, in the table "
		"Nous would read, under the same locks: a goal that cannot pass now yields, and says why (Anastasis.Player.Status)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = PlayerHost(World);
		if (!Host) return;
		const FString Goal = (!Args.IsValidIndex(0) || Args[0].Equals(TEXT("none"), ESearchCase::IgnoreCase)) ? FString() : Args[0];
		const bool bOk = Host->GetSimulation().GetVillage().ChoosePlayerGoal(Goal);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER goal %s -> %s"), Goal.IsEmpty() ? TEXT("(withdrawn)") : *Goal,
			bOk ? TEXT("posed") : TEXT("no incarnated inhabitant"));
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisPlayerChoose(
	TEXT("Anastasis.Player.Choose"),
	TEXT("Anastasis.Player.Choose <n> - pose the n-th goal of the player's table as shown on the BUTS line (keys 1 to 5 in PIE)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = PlayerHost(World);
		if (!Host || !Args.IsValidIndex(0)) return;
		AnastasisVillage::FVillage& Village = Host->GetSimulation().GetVillage();
		const TArray<AnastasisVillage::FPlayerGoalOption>& Table = Village.GetPlayerGoalOptions();
		const int32 Index = FCString::Atoi(*Args[0]) - 1;
		if (!Village.PlayerActor())
		{
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER choose %s -> no incarnated inhabitant (Anastasis.Player.Arrive)"), *Args[0]);
			return;
		}
		if (!Table.IsValidIndex(Index))
		{
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER choose %s -> no such line (%d goals in the table)"), *Args[0], Table.Num());
			return;
		}
		Village.ChoosePlayerGoal(Table[Index].Goal);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER choose %d -> %s"), Index + 1, *Table[Index].Goal);
	}));

FString UAnastasisSimulationDebugLibrary::GetPlayerStatus(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning()) return TEXT("{}");
	const AnastasisVillage::FVillage& Village = Host->GetSimulation().GetVillage();
	const AnastasisVillage::FNpc* Player = Village.PlayerActor();
	// player-start-001 : ce que le debut de partie a fait, et ou le pawn se tient par rapport au puits.
	const int32 AutoArrive = CVarPlayerAutoArrive.GetValueOnGameThread();
	double WellX = Village.GetSettlement().X, WellY = Village.GetSettlement().Y;
	bool bWell = false;
	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
	{
		if (Building.Type == AnastasisVillage::WellType) { WellX = Building.X + 0.5; WellY = Building.Y + 0.5; bWell = true; break; }
	}
	const FVector Well = FAnastasisVillagePresentation::SimToUnreal(Host->GetSimulation().GetWorld(), WellX, WellY, const_cast<UWorld*>(World));
	const APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	const FVector PawnAt = LocalPawn ? LocalPawn->GetActorLocation() : FVector::ZeroVector;
	double FacingErr = 180.0;
	if (LocalPawn)
	{
		const FRotator View = LocalPawn->GetController() ? LocalPawn->GetController()->GetControlRotation() : LocalPawn->GetActorRotation();
		const double ToWell = FMath::RadiansToDegrees(FMath::Atan2(Well.Y - PawnAt.Y, Well.X - PawnAt.X));
		FacingErr = FMath::Abs(FRotator::NormalizeAxis(View.Yaw - ToWell));
	}
	const FString Start = FString::Printf(
		TEXT("\"autoArrive\":%d,\"visualMode\":\"%s\",\"arrivedAtStart\":%s,\"well\":%s,\"wx\":%.1f,\"wy\":%.1f,\"wz\":%.1f,")
		TEXT("\"pawnPresent\":%s,\"px\":%.1f,\"py\":%.1f,\"pz\":%.1f,\"pawnToWellM\":%.2f,\"facingErrDeg\":%.1f"),
		AutoArrive, AnastasisVisualMode::Name(AnastasisVisualMode::Get()), Host->HasArrivedAtStart() ? TEXT("true") : TEXT("false"),
		bWell ? TEXT("true") : TEXT("false"), Well.X, Well.Y, Well.Z,
		LocalPawn ? TEXT("true") : TEXT("false"), PawnAt.X, PawnAt.Y, PawnAt.Z,
		LocalPawn ? FVector::Dist2D(PawnAt, Well) / 100.0 : -1.0, FacingErr);
	if (!Player)
	{
		return FString::Printf(TEXT("{\"player\":\"\",\"npcs\":%d,%s}"), Village.GetActors().Num(), *Start);
	}
	const FVector Body = FAnastasisVillagePresentation::SimToUnreal(Host->GetSimulation().GetWorld(), Player->X, Player->Y, const_cast<UWorld*>(World));
	// player-goals-001 : l'intention, le dernier refus et la table du joueur.
	const AnastasisVillage::FPlayerGoalChoice* Choice = Village.GetPlayerGoalChoice();
	const AnastasisVillage::FPlayerRefusal* Refusal = Village.GetPlayerRefusal();
	int32 FoodRemaining = 0;
	int32 FoodStock = 0;
	for (const AnastasisVillage::FFoodSource& Source : Village.GetFoodSources()) FoodRemaining += Source.Remaining;
	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings()) FoodStock += Building.FoodPhysical;
	TArray<FString> Options;
	for (const AnastasisVillage::FPlayerGoalOption& O : Village.GetPlayerGoalOptions())
	{
		Options.Add(FString::Printf(TEXT("\"%s\""), *O.Goal));
	}
	return FString::Printf(
		TEXT("{\"player\":\"%s\",\"x\":%.4f,\"y\":%.4f,\"goal\":\"%s\",\"activity\":\"%s\",\"presence\":%.4f,\"reputation\":%.4f,")
		TEXT("\"idleDays\":%.4f,\"thirst\":%.2f,\"hunger\":%.2f,\"energy\":%.2f,\"body\":\"%s\",\"drinks\":%d,\"meals\":%d,\"pawn\":%s,\"seenBy\":%d,\"npcs\":%d,")
		TEXT("\"bag\":%d,\"gathered\":%d,\"delivered\":%d,\"foodRemaining\":%d,\"foodStock\":%d,")
		TEXT("\"choice\":\"%s\",\"holds\":%d,\"yields\":%d,\"refusal\":\"%s\",\"options\":[%s],")
		TEXT("\"ux\":%.1f,\"uy\":%.1f,\"uz\":%.1f,%s}"),
		*Player->Id, Player->X, Player->Y, *Player->Goal, *Player->Activity, Player->Presence, Player->Reputation,
		Player->IdleSeconds / FAnastasisSimulation::DayLength, Player->Needs.Thirst, Player->Needs.Hunger, Player->Needs.Energy, *BodyAsks(*Player),
		Player->DrinksTaken, Player->MealsTaken,
		Host->IsPawnBound() ? TEXT("true") : TEXT("false"), CountSeers(Village, *Player), Village.GetActors().Num(),
		Player->InventoryFood, Player->GatheredFood, Player->DeliveredFood, FoodRemaining, FoodStock,
		Choice ? *Choice->Goal : TEXT(""), Choice ? Choice->Holds : 0, Choice ? Choice->Yields : 0,
		Refusal ? *Refusal->Reason : TEXT(""), *FString::Join(Options, TEXT(",")),
		Body.X, Body.Y, Body.Z, *Start);
}

FString UAnastasisSimulationDebugLibrary::GetCabinStatus(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning()) return TEXT("{}");
	const AnastasisVillage::FVillage& Village = Host->GetSimulation().GetVillage();
	const AnastasisVillage::FNpc* Player = Village.PlayerActor();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("player"), Player ? Player->Id : FString());
	Root->SetNumberField(TEXT("day"), Host->GetSimulation().GetDay());
	const AnastasisVillage::FBuilding* Site = nullptr;
	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
	{
		if (Player && Building.Type == AnastasisVillage::CabinType && Building.Owner == Player->Id) Site = &Building;
	}
	Root->SetStringField(TEXT("site"), Site ? Site->Id : FString());
	auto Vec = [](const FVector& V)
	{
		TArray<TSharedPtr<FJsonValue>> A;
		A.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.X)));
		A.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.Y)));
		A.Add(MakeShared<FJsonValueNumber>(FMath::RoundToDouble(V.Z)));
		return A;
	};
	if (Player)
	{
		Root->SetNumberField(TEXT("x"), Player->X);
		Root->SetNumberField(TEXT("y"), Player->Y);
		Root->SetStringField(TEXT("home"), Player->HomeId);
		Root->SetStringField(TEXT("family"), Player->FamilyId);
		Root->SetBoolField(TEXT("inside"), Player->Inside.bActive);
		Root->SetStringField(TEXT("inside_building"), Player->Inside.bActive ? Player->Inside.BuildingId : FString());
		Root->SetStringField(TEXT("activity"), Player->Activity);
		Root->SetStringField(TEXT("goal"), Player->Goal);
		Root->SetNumberField(TEXT("energy"), Player->Needs.Energy);
		Root->SetBoolField(TEXT("pawn_indoors"), Host->IsPawnIndoors());
		if (const APawn* Pawn = Host->GetBoundPawn()) Root->SetArrayField(TEXT("pawn"), Vec(Pawn->GetActorLocation()));
	}
	if (Site)
	{
		Root->SetStringField(TEXT("type"), Site->Type);
		Root->SetNumberField(TEXT("progress"), Site->Progress);
		Root->SetNumberField(TEXT("pieces"), Site->PiecesPlaced);
		Root->SetNumberField(TEXT("piece_total"), AnastasisBuild::PieceTotal);
		Root->SetStringField(TEXT("owner"), Site->Owner);
		Root->SetNumberField(TEXT("tile_x"), Site->X);
		Root->SetNumberField(TEXT("tile_y"), Site->Y);
		Root->SetNumberField(TEXT("created_day"), Site->CreatedDay);
		Root->SetNumberField(TEXT("completed_day"), Site->CompletedDay);
		Root->SetNumberField(TEXT("need_wood"), Site->Materials.NeedWood);
		Root->SetNumberField(TEXT("need_stone"), Site->Materials.NeedStone);
		TArray<TSharedPtr<FJsonValue>> Workers;
		for (const TPair<FString, int32>& Worker : Site->Workers)
		{
			Workers.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%s:%d"), *Worker.Key, Worker.Value)));
		}
		Root->SetArrayField(TEXT("workers"), Workers);
		TArray<TSharedPtr<FJsonValue>> Others;
		for (const FString& Id : Village.InsideOf(Site->Id))
		{
			if (!Player || Id != Player->Id) Others.Add(MakeShared<FJsonValueString>(Id));
		}
		Root->SetArrayField(TEXT("others_inside"), Others);
		TArray<TSharedPtr<FJsonValue>> Lodgers;
		for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
		{
			if (Player && Npc.Id == Player->Id) continue;
			if (Npc.HomeId == Site->Id || Npc.ShelterId == Site->Id) Lodgers.Add(MakeShared<FJsonValueString>(Npc.Id));
		}
		Root->SetArrayField(TEXT("others_lodged"), Lodgers);
		if (const AAnastasisVillageBuilding* Actor = Host->GetVillagePresentation().FindActor(Site->Id))
		{
			const FTransform Xf = Actor->GetActorTransform();
			Root->SetBoolField(TEXT("actor"), true);
			Root->SetBoolField(TEXT("architecture"), Actor->HasArchitecture());
			Root->SetArrayField(TEXT("location"), Vec(Xf.GetLocation()));
			Root->SetNumberField(TEXT("yaw"), Xf.Rotator().Yaw);
			Root->SetNumberField(TEXT("pad"), Actor->GetPadOffset());
			if (Actor->HasArchitecture())
			{
				const AnastasisArchitecture::FArchetype& A = AnastasisArchitecture::Get(Actor->GetVariant());
				const FVector Pad(0.0, 0.0, Actor->GetPadOffset());
				Root->SetStringField(TEXT("archetype"), A.Id);
				Root->SetArrayField(TEXT("door"), Vec(Xf.TransformPosition(A.DoorLocal + Pad)));
				Root->SetArrayField(TEXT("entry"), Vec(Xf.TransformPosition(A.EntryLocal + Pad)));
				Root->SetArrayField(TEXT("hearth"), Vec(Xf.TransformPosition(A.HearthLocal + Pad)));
				TArray<TSharedPtr<FJsonValue>> Foot;
				Foot.Add(MakeShared<FJsonValueNumber>(A.Footprint.Min.X));
				Foot.Add(MakeShared<FJsonValueNumber>(A.Footprint.Min.Y));
				Foot.Add(MakeShared<FJsonValueNumber>(A.Footprint.Max.X));
				Foot.Add(MakeShared<FJsonValueNumber>(A.Footprint.Max.Y));
				Root->SetArrayField(TEXT("footprint"), Foot);
				Root->SetNumberField(TEXT("floor"), A.Rooms.Num() > 0 ? A.Rooms[0].FloorCm + Actor->GetPadOffset() : Actor->GetPadOffset());
				if (const APawn* Pawn = Host->GetBoundPawn())
				{
					Root->SetArrayField(TEXT("pawn_local"), Vec(Xf.InverseTransformPosition(Pawn->GetActorLocation())));
				}
			}
		}
		else
		{
			Root->SetBoolField(TEXT("actor"), false);
		}
	}
	FString Out;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

// --- voix-conseil-001 (ecart n°54) : le joueur vote, repond, batit et demande, avec les memes regles que les autres ---

namespace
{
	bool ReadYes(const TArray<FString>& Args, bool& bOutYes)
	{
		if (!Args.IsValidIndex(0)) return false;
		const FString A = Args[0].ToLower();
		if (A == TEXT("oui") || A == TEXT("yes") || A == TEXT("1")) { bOutYes = true; return true; }
		if (A == TEXT("non") || A == TEXT("no") || A == TEXT("0")) { bOutYes = false; return true; }
		return false;
	}

	/** Un habitant par son identifiant (`npc-7`) ou par son prenom tel que la chronique le dit (« Konstantinos »). */
	FString FindInhabitant(const UAnastasisSimulationSubsystem& Host, const FString& Who)
	{
		const AnastasisVillage::FVillage& Village = Host.GetSimulation().GetVillage();
		if (Village.FindNpc(Who)) return Who;
		for (const AnastasisVillage::FNpc& Npc : Village.GetActors())
		{
			if (Npc.Name.Equals(Who, ESearchCase::IgnoreCase) || Host.GetChronicle().NameOf(Npc.Id).Equals(Who, ESearchCase::IgnoreCase)) return Npc.Id;
		}
		return FString();
	}
}

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisPlayerVote(
	TEXT("Anastasis.Player.Vote"),
	TEXT("Anastasis.Player.Vote <oui|non> - the player's voice at tonight's council, on the oldest group waiting at the gate. "
		"An unproven newcomer counts for half a voice. voix-conseil-001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = PlayerHost(World);
		bool bYes = false;
		if (!Host || !ReadYes(Args, bYes)) return;
		const FString Family = Host->GetSimulation().GetVillage().CastPlayerVote(bYes);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER vote %s -> %s"), bYes ? TEXT("oui") : TEXT("non"),
			Family.IsEmpty() ? TEXT("no group waiting (or no incarnated inhabitant)") : *Family);
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisPlayerHelp(
	TEXT("Anastasis.Player.Help"),
	TEXT("Anastasis.Player.Help <oui|non> - answer the oldest request for help made to the player. Unanswered by the next "
		"evening, it counts as a refusal. voix-conseil-001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = PlayerHost(World);
		bool bYes = false;
		if (!Host || !ReadYes(Args, bYes)) return;
		const bool bOk = Host->GetSimulation().GetVillage().AnswerPlayerAsk(bYes);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER help %s -> %s"), bYes ? TEXT("oui") : TEXT("non"),
			bOk ? TEXT("answered") : TEXT("nobody is asking"));
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisPlayerBuild(
	TEXT("Anastasis.Player.Build"),
	TEXT("Anastasis.Player.Build - the player decides to build his home: he becomes the head of his own household and his "
		"cabin is traced near him -- one room, one sleeper, his alone, raised by his own hands (choose 'build'). "
		"voix-conseil-001, ma-cabane-001."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = PlayerHost(World);
		if (!Host) return;
		AnastasisVillage::FVillage& Village = Host->GetSimulation().GetVillage();
		const FString Me = Village.GetPlayerPersonId();
		const FString Name = Me.IsEmpty() ? FString() : FString::Printf(TEXT("la maison de %s"), *Host->GetChronicle().NameOf(Me));
		const FString Site = Village.PlayerBuildHome(Name);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER build -> %s"), Site.IsEmpty() ? TEXT("refused (no ground, a roof already, or already building)") : *Site);
		if (!Site.IsEmpty()) UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER cabin %s"), *UAnastasisSimulationDebugLibrary::GetCabinStatus(World));
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisPlayerAsk(
	TEXT("Anastasis.Player.Ask"),
	TEXT("Anastasis.Player.Ask <prenom|npc-id> - the player asks an inhabitant to help on his house; the answer comes at once, "
		"with its reason (Anastasis.Chronicle.Print). voix-conseil-001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = PlayerHost(World);
		if (!Host || !Args.IsValidIndex(0)) return;
		const FString Who = FindInhabitant(*Host, Args[0]);
		AnastasisVillage::FVillage::FHelpAnswer Answer;
		const bool bOk = !Who.IsEmpty() && Host->GetSimulation().GetVillage().PlayerAskHelp(Who, Answer);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER ask %s -> %s"), *Args[0],
			!bOk ? TEXT("impossible (nobody by that name, or no house of yours being built)")
				: *FString::Printf(TEXT("%s (%s)"), Answer.bAccepted ? TEXT("oui") : TEXT("non"), *Answer.Reason));
	}));
