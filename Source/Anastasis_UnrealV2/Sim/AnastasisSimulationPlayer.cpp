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
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Life/AnastasisBonds.h"
#include "Village/AnastasisVillagerVisual.h"

static TAutoConsoleVariable<int32> CVarPlayerPawn(
	TEXT("anastasis.Player.Pawn"),
	1,
	TEXT("player-minimal-001: 1 = the local pawn follows the incarnated inhabitant (its input drives the simulated body, it is placed on it, its portrait card is hidden). 0 = the pawn walks on its own; the inhabitant is driven only by Anastasis.Player.Move."),
	ECVF_Default);

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
	const FVector Feet = FAnastasisVillagePresentation::SimToUnreal(Simulation.GetWorld(), Player->X, Player->Y, World);
	Pawn->SetActorLocation(Feet + FVector(0.0, 0.0, HalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
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
			AnastasisVillage::FVillage& Village = Host->GetSimulation().GetVillage();
			const bool bAt = Args.Num() >= 2;
			const FString Id = Village.ArriveAsPlayer(
				bAt ? FCString::Atoi(*Args[0]) + 0.5 : -1.0,
				bAt ? FCString::Atoi(*Args[1]) + 0.5 : -1.0);
			const AnastasisVillage::FNpc* Player = Village.FindNpc(Id);
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_PLAYER arrive %s at (%.2f,%.2f), %d inhabitants"),
				Id.IsEmpty() ? TEXT("refused") : *Id, Player ? Player->X : -1.0, Player ? Player->Y : -1.0, Village.GetActors().Num());
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
	if (!Player)
	{
		return FString::Printf(TEXT("{\"player\":\"\",\"npcs\":%d}"), Village.GetActors().Num());
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
		TEXT("\"idleDays\":%.4f,\"thirst\":%.2f,\"hunger\":%.2f,\"drinks\":%d,\"meals\":%d,\"pawn\":%s,\"seenBy\":%d,\"npcs\":%d,")
		TEXT("\"bag\":%d,\"gathered\":%d,\"delivered\":%d,\"foodRemaining\":%d,\"foodStock\":%d,")
		TEXT("\"choice\":\"%s\",\"holds\":%d,\"yields\":%d,\"refusal\":\"%s\",\"options\":[%s],")
		TEXT("\"ux\":%.1f,\"uy\":%.1f,\"uz\":%.1f}"),
		*Player->Id, Player->X, Player->Y, *Player->Goal, *Player->Activity, Player->Presence, Player->Reputation,
		Player->IdleSeconds / FAnastasisSimulation::DayLength, Player->Needs.Thirst, Player->Needs.Hunger, Player->DrinksTaken, Player->MealsTaken,
		Host->IsPawnBound() ? TEXT("true") : TEXT("false"), CountSeers(Village, *Player), Village.GetActors().Num(),
		Player->InventoryFood, Player->GatheredFood, Player->DeliveredFood, FoodRemaining, FoodStock,
		Choice ? *Choice->Goal : TEXT(""), Choice ? Choice->Holds : 0, Choice ? Choice->Yields : 0,
		Refusal ? *Refusal->Reason : TEXT(""), *FString::Join(Options, TEXT(",")),
		Body.X, Body.Y, Body.Z);
}
