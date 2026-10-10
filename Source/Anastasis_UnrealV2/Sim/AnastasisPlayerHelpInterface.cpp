// player-help-scene-001 : les demandes du joueur et leur lecture en jeu.
// La presentation lit le village ; seule l'action E appelle le verbe commun FVillage::AskHelp.

#include "Sim/AnastasisSimulationSubsystem.h"

#include "Engine/GameViewportClient.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FString PersonName(const AnastasisVillage::FNpc& Npc)
	{
		return Npc.Name.IsEmpty() ? Npc.Id : Npc.Name;
	}

	FString HelpReason(const FString& Reason)
	{
		if (Reason == TEXT("dette_rendue")) return TEXT("se souvient de ton aide");
		if (Reason == TEXT("amitie")) return TEXT("tient a toi");
		if (Reason == TEXT("voisin")) return TEXT("veut aider un voisin");
		if (Reason == TEXT("refus_rendu")) return TEXT("se souvient de ton refus");
		if (Reason == TEXT("son_toit")) return TEXT("doit finir son propre toit");
		if (Reason == TEXT("dette")) return TEXT("attend que tu lui rendes son aide");
		if (Reason == TEXT("occupe")) return TEXT("a deja un autre travail");
		if (Reason == TEXT("faible")) return TEXT("est trop faible aujourd'hui");
		if (Reason == TEXT("inconnu")) return TEXT("ne te connait pas encore");
		return Reason;
	}

	FString InvalidHelpReason(const FString& Reason)
	{
		if (Reason == TEXT("personne_absente")) return TEXT("personne a qui parler");
		if (Reason == TEXT("chantier_absent")) return TEXT("chantier familial ouvert");
		if (Reason == TEXT("pas_mandate")) return TEXT("droit de demander pour ce chantier");
		if (Reason == TEXT("deja_sollicite")) return TEXT("nouvelle personne a solliciter");
		if (Reason == TEXT("trop_jeune")) return TEXT("adulte a qui demander");
		if (Reason == TEXT("trop_loin")) return TEXT("te rapprocher pour etre entendu");
		if (Reason == TEXT("chantier_inaccessible")) return TEXT("chemin ouvert jusqu'au chantier");
		return Reason;
	}
}

void UAnastasisSimulationSubsystem::EnsureHelpPanel()
{
	if (HelpPanel.IsValid()) return;
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
	if (!Viewport) return;
	const TWeakObjectPtr<UAnastasisSimulationSubsystem> WeakThis(this);
	HelpPanel = SNew(SOverlay)
		.Visibility_Lambda([WeakThis]()
		{
			return WeakThis.IsValid() && (WeakThis->bHelpSceneActive || (WeakThis->bStartVillage && !WeakThis->Founders.IsEmpty()))
				? EVisibility::HitTestInvisible : EVisibility::Collapsed;
		})
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(24.0f))
		[
			SNew(SBox)
			.WidthOverride(560.0f)
			[
				SNew(SBorder)
				.Padding(FMargin(18.0f))
				.BorderBackgroundColor(FLinearColor(0.018f, 0.026f, 0.032f, 0.88f))
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text_Lambda([WeakThis]()
					{
						return FText::FromString(WeakThis.IsValid() ? WeakThis->HelpPanelText() : FString());
					})
				]
			]
		];
	Viewport->AddViewportWidgetContent(HelpPanel.ToSharedRef(), 20);
}

bool UAnastasisSimulationSubsystem::FindHelpFocus(FString& OutSiteId, FString& OutPersonId) const
{
	OutSiteId.Reset();
	OutPersonId.Reset();
	const UWorld* World = GetWorld();
	if (!World || !Simulation.IsRunning()) return false;
	const AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisVillage::FNpc* Player = Village.PlayerActor();
	if (!Player) return false;
	for (const AnastasisVillage::FBuilding& Site : Village.GetBuildings())
	{
		if (Site.Progress >= 1.0 || Site.OwnerFamilyId.IsEmpty()) continue;
		if (!Site.AllowedBuilders.Contains(Player->Id)) continue;
		OutSiteId = Site.Id;
		break;
	}
	if (OutSiteId.IsEmpty()) return false;
	const APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	if (!Controller) return false;
	FVector Eye;
	FRotator Gaze;
	Controller->GetPlayerViewPoint(Eye, Gaze);
	const FVector Forward = Gaze.Vector();
	double Best = 0.55;
	for (const AnastasisVillage::FNpc& Other : Village.GetActors())
	{
		if (Other.Id == Player->Id) continue;
		const double Distance = FVector2D(Other.X - Player->X, Other.Y - Player->Y).Length();
		if (Distance > AnastasisVillage::FVillage::HelpSpeakingRange) continue;
		const FVector OtherHead = FAnastasisVillagePresentation::SimToUnreal(Simulation.GetWorld(), Other.X, Other.Y,
			const_cast<UWorld*>(World)) + FVector(0.0, 0.0, 155.0);
		const double Facing = FVector::DotProduct(Forward, (OtherHead - Eye).GetSafeNormal());
		if (Facing > Best)
		{
			Best = Facing;
			OutPersonId = Other.Id;
		}
	}
	return !OutPersonId.IsEmpty();
}

void UAnastasisSimulationSubsystem::StartHelpScene()
{
	if (!Simulation.IsRunning() || !bStartVillage || Founders.IsEmpty())
	{
		HelpFeedback = TEXT("Valmire se met encore en place.");
		return;
	}
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	if (Village.PlayerActor())
	{
		// arrivant-seul-001 : le joueur du debut de partie est arrive seul. Entree ne lui prend la place d'aucune famille, et le
		// dire vaut mieux que ne rien faire.
		if (!bHelpSceneActive) HelpFeedback = TEXT("Arrive seul, tu n'as pas de famille dont prendre la place : Entree ne fait rien.");
		return;
	}
	FString SiteId;
	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
	{
		if (!Building.OwnerFamilyId.IsEmpty() && Building.Progress < 1.0) { SiteId = Building.Id; break; }
	}
	if (SiteId.IsEmpty())
	{
		// L'hote ouvre la premiere parcelle par la regle commune ; la famille posera les pieces elle-meme.
		Village.UpdateFamilyHousesDaily();
		for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
		{
			if (!Building.OwnerFamilyId.IsEmpty() && Building.Progress < 1.0) { SiteId = Building.Id; break; }
		}
	}
	const AnastasisVillage::FBuilding* Site = Village.FindBuilding(SiteId);
	if (!Site)
	{
		HelpFeedback = TEXT("Aucune famille n'a pu ouvrir de chantier ici.");
		return;
	}
	for (const AnastasisVillage::FVillage::FFamily& Family : Village.GetFamilies())
	{
		if (Family.Id != Site->OwnerFamilyId) continue;
		for (const FString& Id : Family.Adults)
		{
			const AnastasisVillage::FNpc* Adult = Village.FindNpc(Id);
			if (Adult && Adult->KinRole == TEXT("chef") && Village.Incarnate(Id))
			{
				bHelpSceneActive = true;
				HelpFeedback = FString::Printf(TEXT("Le toit de %s attend des bras. Parle a un voisin, ou consacre ton temps au chantier."), *Family.Name);
				return;
			}
		}
	}
	HelpFeedback = TEXT("Aucun chef de famille ne peut prendre la main.");
}

void UAnastasisSimulationSubsystem::AskFocusedHelp()
{
	if (!bHelpSceneActive) return;
	FString SiteId;
	FString PersonId;
	if (!FindHelpFocus(SiteId, PersonId))
	{
		HelpFeedback = TEXT("Approche une personne et regarde-la pour lui parler.");
		return;
	}
	AskHelpToId(PersonId);
}

void UAnastasisSimulationSubsystem::AskHelpToId(const FString& PersonId)
{
	if (!bHelpSceneActive || !Simulation.IsRunning()) return;
	AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisVillage::FNpc* Player = Village.PlayerActor();
	if (!Player) return;
	FString SiteId;
	for (const AnastasisVillage::FBuilding& Site : Village.GetBuildings())
	{
		if (Site.Progress < 1.0 && !Site.OwnerFamilyId.IsEmpty() && Site.AllowedBuilders.Contains(Player->Id))
		{
			SiteId = Site.Id;
			break;
		}
	}
	if (SiteId.IsEmpty())
	{
		HelpFeedback = TEXT("Aucun toit ouvert auquel tu puisses demander de l'aide.");
		return;
	}
	const AnastasisVillage::FNpc* Asked = Village.FindNpc(PersonId);
	const FString Name = Asked ? PersonName(*Asked) : PersonId;
	const AnastasisVillage::FVillage::FHelpRequestResult Result = Village.AskHelp(Village.GetPlayerPersonId(), PersonId, SiteId);
	if (!Result.bValid)
	{
		HelpFeedback = FString::Printf(TEXT("%s : il faut %s."), *Name, *InvalidHelpReason(Result.InvalidReason));
		return;
	}
	HelpFeedback = FString::Printf(TEXT("%s %s : %s. L'accord ne pose aucune piece a sa place."), *Name,
		Result.Answer.bAccepted ? TEXT("accepte") : TEXT("refuse"), *HelpReason(Result.Answer.Reason));
}

void UAnastasisSimulationSubsystem::ChooseHelpSceneWork()
{
	if (bHelpSceneActive && Simulation.IsRunning() && Simulation.GetVillage().ChoosePlayerGoal(TEXT("build")))
		HelpFeedback = TEXT("Tu consacres ton temps au chantier. Ton corps et l'acces au site restent prioritaires.");
}

void UAnastasisSimulationSubsystem::StopHelpSceneWork()
{
	if (bHelpSceneActive && Simulation.IsRunning() && Simulation.GetVillage().ChoosePlayerGoal(FString()))
		HelpFeedback = TEXT("Tu retires ton intention de batir.");
}

void UAnastasisSimulationSubsystem::ToggleHelpNotebook()
{
	if (bHelpSceneActive) bHelpNotebookOpen = !bHelpNotebookOpen;
}

FString UAnastasisSimulationSubsystem::ArrivalHelpText(int32 Day, const FString& Name)
{
	return FString::Printf(
		TEXT("VALMIRE  |  JOUR %d  |  %s\n")
		TEXT("Tu es arrive seul, sans famille.\n")
		TEXT("Marcher : Z Q S D (ou W A S D). Regarder : la souris.\n")
		TEXT("Ton corps a faim, soif, sommeil : la ligne BUTS, en haut de l'ecran, dit ce qu'il peut faire et la touche de chaque but.\n")
		TEXT("Temps : 8 le double, 9 le divise. Trop vite, le village ne te voit plus.\n"),
		Day, *Name);
}

FString UAnastasisSimulationSubsystem::HelpPanelText() const
{
	if (!Simulation.IsRunning()) return FString();
	const AnastasisVillage::FVillage& Village = Simulation.GetVillage();
	const AnastasisVillage::FNpc* Player = Village.PlayerActor();
	if (!Player)
	{
		return FString::Printf(TEXT("VALMIRE  |  PREMIERE JOURNEE\n[ENTREE] Vivre la journee d'une famille qui batit son toit.\n%s"),
			*HelpFeedback);
	}
	// arrivant-seul-001 (Alexandre, 2026-10-09 : « je veux rester seul arrivant ») : tant que la scene d'entraide n'est pas ouverte,
	// le panneau n'annonce que ce qui marche. E, F, X et J n'agissent qu'a l'interieur de cette scene.
	if (!bHelpSceneActive)
	{
		FString Arrival = ArrivalHelpText(Simulation.GetDay(), PersonName(*Player));
		if (!HelpFeedback.IsEmpty()) Arrival += HelpFeedback + TEXT("\n");
		return Arrival;
	}
	FString Text = FString::Printf(TEXT("VALMIRE  |  JOUR %d  |  %s\n"), Simulation.GetDay(), *PersonName(*Player));
	const AnastasisVillage::FBuilding* Site = nullptr;
	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
	{
		if (Building.OwnerFamilyId == Player->FamilyId && !Building.OwnerFamilyId.IsEmpty())
		{
			Site = &Building;
			if (Building.Progress < 1.0) break;
		}
	}
	if (Site)
	{
		Text += Site->Progress >= 1.0 ? TEXT("Le toit est leve.\n") :
			FString::Printf(TEXT("Toit : %d pieces posees. La famille travaille ; une aide exterieure est necessaire.\n"), Site->PiecesPlaced);
		for (const AnastasisVillage::FVillage::FHelpAnswer& Answer : Village.GetHelpLog())
		{
			if (Answer.FromId != Player->Id || Answer.SiteId != Site->Id || !Answer.bAccepted) continue;
			const AnastasisVillage::FNpc* Helper = Village.FindNpc(Answer.ToId);
			const TPair<FString, int32>* Work = Site->Workers.FindByPredicate([&Answer](const TPair<FString, int32>& Pair)
			{
				return Pair.Key == Answer.ToId;
			});
			Text += FString::Printf(TEXT("%s a dit oui : %s\n"), Helper ? *PersonName(*Helper) : *Answer.ToId,
				Work && Work->Value > 0 ? TEXT("a pose des pieces") : TEXT("n'a pas encore travaille"));
		}
	}
	FString FocusSite;
	FString FocusPerson;
	if (FindHelpFocus(FocusSite, FocusPerson))
	{
		const AnastasisVillage::FNpc* Other = Village.FindNpc(FocusPerson);
		Text += FString::Printf(TEXT("[E] Demander a %s de venir aider au toit.\n"), Other ? *PersonName(*Other) : *FocusPerson);
	}
	else Text += TEXT("Approche un voisin et regarde-le pour demander de l'aide.\n");
	Text += TEXT("[F] Batir  [X] Retirer l'intention  [J] Carnet\n");
	if (!HelpFeedback.IsEmpty()) Text += HelpFeedback + TEXT("\n");
	if (bHelpNotebookOpen)
	{
		const TArray<AnastasisNotebook::FNote>& Notes = Notebook.GetNotes();
		Text += FString::Printf(TEXT("CARNET  |  %d choses entendues\n"), Notes.Num());
		for (int32 I = FMath::Max(0, Notes.Num() - 4); I < Notes.Num(); ++I)
		{
			const AnastasisNotebook::FNote& Note = Notes[I];
			Text += FString::Printf(TEXT("J%d, %s (%s) : %s\n"), Note.Day, *Note.TellerName,
				Note.bToMe ? TEXT("a moi") : TEXT("entendu"), *Note.Text.Left(140));
		}
	}
	return Text;
}

namespace
{
	UAnastasisSimulationSubsystem* HelpHost(const UObject* Context)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
		return World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	}
}

void UAnastasisSimulationDebugLibrary::StartHelpScene(const UObject* WorldContextObject)
{
	if (UAnastasisSimulationSubsystem* Host = HelpHost(WorldContextObject)) Host->StartHelpScene();
}

void UAnastasisSimulationDebugLibrary::AskHelpToId(const UObject* WorldContextObject, const FString& PersonId)
{
	if (UAnastasisSimulationSubsystem* Host = HelpHost(WorldContextObject)) Host->AskHelpToId(PersonId);
}

FString UAnastasisSimulationDebugLibrary::GetHelpSceneStatus(const UObject* WorldContextObject)
{
	const UAnastasisSimulationSubsystem* Host = HelpHost(WorldContextObject);
	if (!Host || !Host->GetSimulation().IsRunning()) return TEXT("{}");
	const AnastasisVillage::FVillage& Village = Host->GetSimulation().GetVillage();
	const AnastasisVillage::FNpc* Player = Village.PlayerActor();
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("active"), Host->IsHelpSceneActive());
	Root->SetBoolField(TEXT("ready"), Host->HasStartVillage());
	Root->SetBoolField(TEXT("panel"), Host->HasHelpPanel());
	Root->SetStringField(TEXT("player"), Player ? Player->Id : FString());
	if (Player)
	{
		Root->SetNumberField(TEXT("playerX"), Player->X);
		Root->SetNumberField(TEXT("playerY"), Player->Y);
	}
	TArray<TSharedPtr<FJsonValue>> People;
	for (const AnastasisVillage::FNpc& Other : Village.GetActors())
	{
		if (!Player || Other.Id == Player->Id) continue;
		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("id"), Other.Id);
		Row->SetStringField(TEXT("name"), PersonName(Other));
		Row->SetStringField(TEXT("family"), Other.FamilyId);
		Row->SetNumberField(TEXT("x"), Other.X);
		Row->SetNumberField(TEXT("y"), Other.Y);
		Row->SetNumberField(TEXT("distance"), FVector2D(Other.X - Player->X, Other.Y - Player->Y).Size());
		People.Add(MakeShared<FJsonValueObject>(Row));
	}
	Root->SetArrayField(TEXT("people"), People);
	TArray<TSharedPtr<FJsonValue>> Sites;
	for (const AnastasisVillage::FBuilding& Building : Village.GetBuildings())
	{
		if (Building.OwnerFamilyId.IsEmpty()) continue;
		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("id"), Building.Id);
		Row->SetStringField(TEXT("family"), Building.OwnerFamilyId);
		Row->SetNumberField(TEXT("progress"), Building.Progress);
		Row->SetNumberField(TEXT("pieces"), Building.PiecesPlaced);
		Row->SetBoolField(TEXT("playerAllowed"), Player && Building.AllowedBuilders.Contains(Player->Id));
		TArray<TSharedPtr<FJsonValue>> Workers;
		for (const TPair<FString, int32>& Worker : Building.Workers)
		{
			const TSharedRef<FJsonObject> Work = MakeShared<FJsonObject>();
			Work->SetStringField(TEXT("id"), Worker.Key);
			Work->SetNumberField(TEXT("pieces"), Worker.Value);
			Workers.Add(MakeShared<FJsonValueObject>(Work));
		}
		Row->SetArrayField(TEXT("workers"), Workers);
		Sites.Add(MakeShared<FJsonValueObject>(Row));
	}
	Root->SetArrayField(TEXT("sites"), Sites);
	TArray<TSharedPtr<FJsonValue>> Answers;
	for (const AnastasisVillage::FVillage::FHelpAnswer& Answer : Village.GetHelpLog())
	{
		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("from"), Answer.FromId);
		Row->SetStringField(TEXT("to"), Answer.ToId);
		Row->SetStringField(TEXT("site"), Answer.SiteId);
		Row->SetBoolField(TEXT("accepted"), Answer.bAccepted);
		Row->SetStringField(TEXT("reason"), Answer.Reason);
		Answers.Add(MakeShared<FJsonValueObject>(Row));
	}
	Root->SetArrayField(TEXT("answers"), Answers);
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	return Json;
}
