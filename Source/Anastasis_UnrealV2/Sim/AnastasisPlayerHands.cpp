// mains-joueur-001 (ecart n°57) -- les mains du joueur, cote Unreal.
//
// Demande d'Alexandre (2026-10-09), « comme Medieval Dynasty, un peu comme Minecraft ; simplifier, pour le joueur
// humain comme pour le joueur IA » :
//   - viser : ce que la camera regarde (un batiment de la simulation, ou la tuile sous le point vise) ;
//   - maintenir le clic gauche : le geste offert se fait quand la barre est pleine, et se refait tant qu'on tient ;
//   - une barre de 8 cases en bas de l'ecran : le sac du corps (nourriture, bois, pierre) ; molette ou 1 a 8 ;
//   - clic droit : utiliser la case choisie (la nourriture se mange).
// L'hote ne decide rien : il vise, il chronometre, puis il demande le geste a la simulation
// (`FVillage::PlayerHandDo`), qui verifie la portee et dit ce qu'elle a fait ou pourquoi non.

#include "Sim/AnastasisSimulationSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "CollisionQueryParams.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Village/AnastasisVillageBuilding.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "WorldView/AnastasisWorldEmbodiment.h"
#include "WorldView/AnastasisWorldView.h"

namespace
{
	using EHand = AnastasisVillage::FVillage::EPlayerHand;

	/**
	 * Jusqu'ou porte le regard pour viser (cm), au moins. La portee du geste, elle, est jugee par la simulation.
	 * relay-mains-001 : le regard porte toujours au-dela de la main (`HandTraceTiles` cases), sinon une case a portee
	 * (1,6 case, ~32 m) restait invisible : a hauteur d'oeil, un rayon vise sur le sol a 20 m ne le touche qu'apres.
	 */
	constexpr double HandTraceCm = 3000.0;
	constexpr double HandTraceTiles = AnastasisVillage::FVillage::PlayerHandReach + 1.5;

	/** Combien de secondes il faut tenir le clic pour chaque geste (temps reel, comme un geste de la main). */
	float HoldSeconds(EHand Hand)
	{
		switch (Hand)
		{
		case EHand::Drink: return 1.0f;
		case EHand::Gather: return 1.2f;
		case EHand::Chop: return 2.0f;
		case EHand::Quarry: return 2.5f;
		case EHand::Deposit: return 0.6f;
		case EHand::Take: return 0.6f;
		case EHand::Supply: return 0.8f;
		case EHand::Build: return 1.5f;
		default: return 1.0f;
		}
	}

	/** La taille d'une case de simulation dans le monde rendu (cm). */
	double CellCm(UWorld* World)
	{
		double Scale = 1.0;
		if (World)
		{
			for (TActorIterator<AAnastasisWorldEmbodiment> It(World); It; ++It)
			{
				Scale = It->GetSnapshot().SpatialScale;
				break;
			}
		}
		return AnastasisWorldView::TileWorldSize * Scale;
	}

	/** Unreal (cm) -> simulation (tuiles) sur le plan : l'inverse de FAnastasisVillagePresentation::SimToUnreal. */
	bool UnrealToSimPlane(UWorld* World, const FVector& Location, double& OutX, double& OutY)
	{
		const double Cell = CellCm(World);
		if (!(Cell > 0.0)) return false;
		OutX = Location.X / Cell;
		OutY = Location.Y / Cell;
		return true;
	}

	/** Les choses du sac, dans l'ordre fixe de la barre : nourriture, bois, pierre ; les cases 4 a 8 sont libres. */
	struct FBagItem
	{
		const TCHAR* Name;
		int32 Count;
	};

	TArray<FBagItem> Bag(const AnastasisVillage::FVillage& Village)
	{
		const AnastasisVillage::FNpc* P = Village.PlayerActor();
		if (!P) return {};
		return { { TEXT("Nourriture"), P->InventoryFood }, { TEXT("Bois"), P->InventoryWood }, { TEXT("Pierre"), Village.PlayerStone() } };
	}
}

void UAnastasisSimulationSubsystem::NoteHand(const FString& Message)
{
	HandMessage = Message;
	HandMessageUntil = FPlatformTime::Seconds() + 2.5;
	UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_HANDS %s"), *Message);
}

bool UAnastasisSimulationSubsystem::ResolveHandFocus(FString& OutBuildingId, int32& OutTileX, int32& OutTileY) const
{
	OutBuildingId.Reset();
	OutTileX = OutTileY = -1;
	UWorld* World = GetWorld();
	const APlayerController* Controller = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	if (!Controller) return false;
	FVector Eye;
	FRotator Gaze;
	Controller->GetPlayerViewPoint(Eye, Gaze);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AnastasisPlayerHands), false);
	if (const APawn* Pawn = Controller->GetPawn()) Params.AddIgnoredActor(Pawn);
	FHitResult Hit;
	const double TraceCm = FMath::Max(HandTraceCm, CellCm(World) * HandTraceTiles);
	if (!World->LineTraceSingleByChannel(Hit, Eye, Eye + Gaze.Vector() * TraceCm, ECC_Visibility, Params)) return false;
	if (const AAnastasisVillageBuilding* Building = Cast<AAnastasisVillageBuilding>(Hit.GetActor()))
	{
		OutBuildingId = Building->GetSimId().ToString();
		return !OutBuildingId.IsEmpty();
	}
	double X = 0.0, Y = 0.0;
	if (!UnrealToSimPlane(World, Hit.ImpactPoint, X, Y)) return false;
	OutTileX = FMath::FloorToInt32(X);
	OutTileY = FMath::FloorToInt32(Y);
	return true;
}

void UAnastasisSimulationSubsystem::UpdatePlayerHands(float DeltaSeconds)
{
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisVillage::FNpc* Player = Village.PlayerActor();
	if (!Player)
	{
		HandFocusOffer = 0;
		HandHeldSeconds = 0.0f;
		return;
	}
	FString BuildingId;
	int32 TX = -1, TY = -1;
	const bool bFocus = ResolveHandFocus(BuildingId, TX, TY);
	const EHand Offer = bFocus ? Village.PlayerHandOffer(BuildingId, TX, TY) : EHand::None;
	// Changer de cible ou de geste recommence la barre.
	if (BuildingId != HandFocusBuilding || TX != HandFocusX || TY != HandFocusY || static_cast<uint8>(Offer) != HandFocusOffer)
	{
		HandHeldSeconds = 0.0f;
	}
	HandFocusBuilding = BuildingId;
	HandFocusX = TX;
	HandFocusY = TY;
	HandFocusOffer = static_cast<uint8>(Offer);
	// A portee ? Meme mesure que la simulation (seuils ou centre de la case), pour que l'ecran ne promette rien.
	bHandFocusInReach = false;
	if (Offer != EHand::None)
	{
		double Best = 1.e9;
		if (!BuildingId.IsEmpty())
		{
			if (const AnastasisVillage::FBuilding* B = Village.FindBuilding(BuildingId))
			{
				Best = FVector2D(Player->X - (B->X + 0.5), Player->Y - (B->Y + 0.5)).Length();
				for (const AnastasisVillage::FPoint& Door : B->AccessPoints)
				{
					Best = FMath::Min(Best, FVector2D(Player->X - Door.X, Player->Y - Door.Y).Length());
				}
			}
		}
		else
		{
			Best = FVector2D(Player->X - (TX + 0.5), Player->Y - (TY + 0.5)).Length();
		}
		bHandFocusInReach = Best <= AnastasisVillage::FVillage::PlayerHandReach;
	}
	if (!bHandHeld || Offer == EHand::None || !bHandFocusInReach)
	{
		HandHeldSeconds = 0.0f;
		return;
	}
	HandHeldSeconds += DeltaSeconds;
	if (HandHeldSeconds < HoldSeconds(Offer)) return;
	HandHeldSeconds = 0.0f;
	const AnastasisVillage::FVillage::FPlayerHandResult R = Village.PlayerHandDo(Offer, BuildingId, TX, TY);
	NoteHand(R.bDone
		? FString::Printf(TEXT("%s (%d)"), AnastasisVillage::FVillage::PlayerHandLabel(Offer), R.Amount)
		: FString::Printf(TEXT("%s : %s"), AnastasisVillage::FVillage::PlayerHandLabel(Offer), *R.Refusal));
	if (!R.bDone) HandHeldSeconds = -0.8f; // un refus se lit avant de reessayer
}

void UAnastasisSimulationSubsystem::SetHandHeld(bool bHeld)
{
	bHandHeld = bHeld;
	if (!bHeld) HandHeldSeconds = 0.0f;
}

void UAnastasisSimulationSubsystem::SelectHandSlot(int32 Slot)
{
	HandSlot = FMath::Clamp(Slot, 0, HandSlots - 1);
}

void UAnastasisSimulationSubsystem::CycleHandSlot(int32 Delta)
{
	HandSlot = ((HandSlot + Delta) % HandSlots + HandSlots) % HandSlots;
}

void UAnastasisSimulationSubsystem::UseSelectedSlot()
{
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	if (!Village.PlayerActor()) return;
	const TArray<FBagItem> Items = Bag(Village);
	if (HandSlot == 0)
	{
		const AnastasisVillage::FVillage::FPlayerHandResult R = Village.PlayerHandDo(EHand::Eat, FString(), -1, -1);
		NoteHand(R.bDone ? FString(TEXT("manger")) : FString::Printf(TEXT("manger : %s"), *R.Refusal));
		return;
	}
	if (Items.IsValidIndex(HandSlot) && Items[HandSlot].Count > 0)
	{
		NoteHand(FString::Printf(TEXT("%s : a donner a un chantier (viser le chantier, maintenir le clic)"), Items[HandSlot].Name));
		return;
	}
	NoteHand(TEXT("case vide"));
}

FString UAnastasisSimulationSubsystem::UseFocusedHandOnce()
{
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	FString BuildingId;
	int32 TX = -1, TY = -1;
	if (!Village.PlayerActor() || !ResolveHandFocus(BuildingId, TX, TY)) return FString();
	const EHand Offer = Village.PlayerHandOffer(BuildingId, TX, TY);
	if (Offer == EHand::None) return FString();
	const AnastasisVillage::FVillage::FPlayerHandResult R = Village.PlayerHandDo(Offer, BuildingId, TX, TY);
	const FString Line = FString::Printf(TEXT("%s %s %d %s"), AnastasisVillage::FVillage::PlayerHandLabel(Offer),
		R.bDone ? TEXT("fait") : TEXT("refuse"), R.Amount, *R.Refusal);
	NoteHand(Line);
	return Line;
}

float UAnastasisSimulationSubsystem::HandHoldFraction() const
{
	const EHand Offer = static_cast<EHand>(HandFocusOffer);
	if (Offer == EHand::None || !bHandHeld) return 0.0f;
	return FMath::Clamp(HandHeldSeconds / HoldSeconds(Offer), 0.0f, 1.0f);
}

FString UAnastasisSimulationSubsystem::HandsPromptText() const
{
	if (FPlatformTime::Seconds() < HandMessageUntil && !HandMessage.IsEmpty()) return HandMessage;
	const EHand Offer = static_cast<EHand>(HandFocusOffer);
	if (Offer == EHand::None) return FString();
	const FString Label = AnastasisVillage::FVillage::PlayerHandLabel(Offer);
	return bHandFocusInReach ? FString::Printf(TEXT("Maintenir clic gauche : %s"), *Label)
		: FString::Printf(TEXT("%s : approchez-vous"), *Label);
}

FString UAnastasisSimulationSubsystem::HandSlotText(int32 Slot) const
{
	const TArray<FBagItem> Items = Bag(Simulation.GetVillage());
	const FString Key = FString::Printf(TEXT("%d"), Slot + 1);
	if (!Items.IsValidIndex(Slot) || Items[Slot].Count <= 0) return Key;
	return FString::Printf(TEXT("%s\n%s\n%d"), *Key, Items[Slot].Name, Items[Slot].Count);
}

void UAnastasisSimulationSubsystem::EnsureHandsPanel()
{
	if (HandsPanel.IsValid()) return;
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport) return;
	const TWeakObjectPtr<UAnastasisSimulationSubsystem> WeakThis(this);
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 Slot = 0; Slot < HandSlots; ++Slot)
	{
		Row->AddSlot()
		.AutoWidth()
		.Padding(FMargin(3.0f))
		[
			SNew(SBox)
			.WidthOverride(78.0f)
			.HeightOverride(78.0f)
			[
				SNew(SBorder)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.BorderBackgroundColor_Lambda([WeakThis, Slot]()
				{
					const bool bSelected = WeakThis.IsValid() && WeakThis->HandSlot == Slot;
					return bSelected ? FLinearColor(0.85f, 0.70f, 0.35f, 0.95f) : FLinearColor(0.02f, 0.03f, 0.035f, 0.80f);
				})
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Center)
					.Text_Lambda([WeakThis, Slot]()
					{
						return FText::FromString(WeakThis.IsValid() ? WeakThis->HandSlotText(Slot) : FString());
					})
				]
			]
		];
	}
	HandsPanel = SNew(SOverlay)
		.Visibility_Lambda([WeakThis]()
		{
			return WeakThis.IsValid() && WeakThis->Simulation.IsRunning() && WeakThis->Simulation.GetVillage().PlayerActor()
				? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 28.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
			[
				SNew(STextBlock)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.Text_Lambda([WeakThis]()
				{
					return FText::FromString(WeakThis.IsValid() ? WeakThis->HandsPromptText() : FString());
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
			[
				SNew(SBox)
				.WidthOverride(260.0f)
				.HeightOverride(8.0f)
				.Visibility_Lambda([WeakThis]()
				{
					return WeakThis.IsValid() && WeakThis->HandHoldFraction() > 0.0f ? EVisibility::HitTestInvisible : EVisibility::Hidden;
				})
				[
					SNew(SProgressBar)
					.Percent_Lambda([WeakThis]()
					{
						return TOptional<float>(WeakThis.IsValid() ? WeakThis->HandHoldFraction() : 0.0f);
					})
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				Row
			]
		];
	Viewport->AddViewportWidgetContent(HandsPanel.ToSharedRef(), 21);
}

FString UAnastasisSimulationSubsystem::GetPlayerHandsStatus() const
{
	const AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisVillage::FNpc* P = Village.PlayerActor();
	if (!P) return TEXT("{\"player\":\"\"}");
	return FString::Printf(
		TEXT("{\"player\":\"%s\",\"food\":%d,\"wood\":%d,\"stone\":%d,\"slot\":%d,\"focus_building\":\"%s\",\"focus_tile\":[%d,%d],")
		TEXT("\"offer\":\"%s\",\"in_reach\":%s,\"held\":%s,\"message\":\"%s\",\"thirst\":%.1f,\"hunger\":%.1f,\"drinks\":%d,\"meals\":%d,\"x\":%.3f,\"y\":%.3f,\"panel\":%s}"),
		*P->Id, P->InventoryFood, P->InventoryWood, Village.PlayerStone(), HandSlot, *HandFocusBuilding, HandFocusX, HandFocusY,
		AnastasisVillage::FVillage::PlayerHandLabel(static_cast<EHand>(HandFocusOffer)), bHandFocusInReach ? TEXT("true") : TEXT("false"),
		bHandHeld ? TEXT("true") : TEXT("false"), *HandMessage.Replace(TEXT("\""), TEXT("'")), P->Needs.Thirst, P->Needs.Hunger, P->DrinksTaken, P->MealsTaken, P->X, P->Y,
		HandsPanel.IsValid() ? TEXT("true") : TEXT("false"));
}

FString UAnastasisSimulationDebugLibrary::GetPlayerHandsStatus(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	return Host && Host->GetSimulation().IsRunning() ? Host->GetPlayerHandsStatus() : FString(TEXT("{}"));
}

FString UAnastasisSimulationDebugLibrary::GetPlayerHandTargets(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning()) return TEXT("{}");
	const FAnastasisSimulation& Sim = Host->GetSimulation();
	const AnastasisVillage::FVillage& Village = Sim.GetVillage();
	const AnastasisVillage::FNpc* P = Village.PlayerActor();
	if (!P) return TEXT("{}");
	const auto Point = [&](double X, double Y)
	{
		const FVector U = FAnastasisVillagePresentation::SimToUnreal(Sim.GetWorld(), X, Y, World);
		return FString::Printf(TEXT("\"ux\":%.1f,\"uy\":%.1f,\"uz\":%.1f"), U.X, U.Y, U.Z);
	};
	FString Buildings;
	for (const AnastasisVillage::FBuilding& B : Village.GetBuildings())
	{
		// relay-mains-001 : les seuils (ou l'on se tient pour la main), pour qu'un agent ou une preuve y marche.
		FString Access;
		for (const AnastasisVillage::FPoint& Door : B.AccessPoints)
		{
			Access += FString::Printf(TEXT("%s[%.2f,%.2f]"), Access.IsEmpty() ? TEXT("") : TEXT(","), Door.X, Door.Y);
		}
		Buildings += FString::Printf(TEXT("%s{\"id\":\"%s\",\"type\":\"%s\",\"x\":%d,\"y\":%d,\"done\":%s,\"food\":%d,\"available\":%d,\"access\":[%s],%s}"),
			Buildings.IsEmpty() ? TEXT("") : TEXT(","), *B.Id, *B.Type, FMath::FloorToInt32(B.X), FMath::FloorToInt32(B.Y),
			B.IsCompleted() ? TEXT("true") : TEXT("false"), B.FoodPhysical, B.FoodAvailable(), *Access,
			*Point(FMath::FloorToInt32(B.X) + 0.5, FMath::FloorToInt32(B.Y) + 0.5));
	}
	FString Tiles;
	const AnastasisWorld::FWorld& W = Sim.GetWorld();
	const int32 CX = FMath::FloorToInt32(P->X), CY = FMath::FloorToInt32(P->Y);
	for (const AnastasisWorld::EResource Res : { AnastasisWorld::EResource::Food, AnastasisWorld::EResource::Wood, AnastasisWorld::EResource::Stone })
	{
		int32 BestX = -1, BestY = -1;
		double Best = 1.e9;
		for (int32 Y = FMath::Max(0, CY - 12); Y <= FMath::Min(W.H - 1, CY + 12); ++Y)
		{
			for (int32 X = FMath::Max(0, CX - 12); X <= FMath::Min(W.W - 1, CX + 12); ++X)
			{
				const AnastasisWorld::FTile T = Village.LiveTileAt(X, Y);
				if (T.Resource != Res || T.Amount <= 0) continue;
				if (Village.PlayerHandOffer(FString(), X, Y) == AnastasisVillage::FVillage::EPlayerHand::None) continue;
				const double D = FVector2D(X + 0.5 - P->X, Y + 0.5 - P->Y).Length();
				if (D < Best) { Best = D; BestX = X; BestY = Y; }
			}
		}
		if (BestX < 0) continue;
		const TCHAR* Name = Res == AnastasisWorld::EResource::Food ? TEXT("food") : Res == AnastasisWorld::EResource::Wood ? TEXT("wood") : TEXT("stone");
		Tiles += FString::Printf(TEXT("%s\"%s\":{\"x\":%d,\"y\":%d,\"amount\":%d,%s}"), Tiles.IsEmpty() ? TEXT("") : TEXT(","), Name, BestX, BestY,
			Village.LiveTileAt(BestX, BestY).Amount, *Point(BestX + 0.5, BestY + 0.5));
	}
	return FString::Printf(TEXT("{\"player\":{\"id\":\"%s\",\"x\":%.3f,\"y\":%.3f},\"buildings\":[%s],\"tiles\":{%s}}"), *P->Id, P->X, P->Y, *Buildings, *Tiles);
}

// --- Commandes : pour les preuves et les agents (les memes gestes que la souris) -----------------------------

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisPlayerHold(
	TEXT("Anastasis.Player.Hold"),
	TEXT("mains-joueur-001: Anastasis.Player.Hold 1|0 - hold or release the left mouse button (the gesture fills its bar, then repeats)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr)
		{
			Host->SetHandHeld(Args.IsValidIndex(0) && FCString::Atoi(*Args[0]) != 0);
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisPlayerUse(
	TEXT("Anastasis.Player.Use"),
	TEXT("mains-joueur-001: the gesture offered by what the camera looks at, done once without holding (same as holding the left mouse button)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr)
		{
			const FString Line = Host->UseFocusedHandOnce();
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_HANDS use %s"), Line.IsEmpty() ? TEXT("rien a faire") : *Line);
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisPlayerEat(
	TEXT("Anastasis.Player.EatFromBag"),
	TEXT("mains-joueur-001: eat one portion from the bag (right mouse button on the food slot)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr)
		{
			Host->SelectHandSlot(0);
			Host->UseSelectedSlot();
		}
	}));
