// SAVE_STATE_001 -- sauver et recharger une partie, cote Unreal.
//
//   Anastasis.Sim.Save [slot]   Saved/SaveGames/<slot>.sav (defaut anastasis.Sim.SaveSlot)
//   Anastasis.Sim.Load [slot]
//
// L'hote ne serialise rien lui-meme : FAnastasisSimulation::SaveState rend l'etat complet (le parcours
// de l'empreinte d'etat), l'hote y joint ses verrous de scenario et le chemin du scenario exterieur, dans
// un USaveGame. Au chargement, la simulation est remplacee (refus = rien ne change), puis la presentation
// est retiree et refaite depuis l'etat relu. Voir docs/unreal/SAVE_STATE_001.md.

#include "Sim/AnastasisSaveGame.h"
#include "Sim/AnastasisSimulationSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Core/AnastasisStateDigest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Village/AnastasisVillageInteractionSubsystem.h"
#include "WorldView/AnastasisPresentationResolver.h"
#include "WorldView/AnastasisAnthropicSubsystem.h"

static TAutoConsoleVariable<FString> CVarSimSaveSlot(
	TEXT("anastasis.Sim.SaveSlot"),
	TEXT("anastasis"),
	TEXT("SAVE_STATE_001: slot de Anastasis.Sim.Save / Anastasis.Sim.Load sans argument (Saved/SaveGames/<slot>.sav)."),
	ECVF_Default);

namespace
{
	FString QuoteJson(const FString& Value)
	{
		return TEXT("\"") + Value.ReplaceCharWithEscapedChar() + TEXT("\"");
	}

	FString SlotOrDefault(const FString& Slot)
	{
		return Slot.IsEmpty() ? CVarSimSaveSlot.GetValueOnGameThread() : Slot;
	}
}

bool UAnastasisSimulationSubsystem::SaveGameToSlot(const FString& InSlot, FString& OutMessage)
{
	const FString Slot = SlotOrDefault(InSlot);
	if (!Simulation.IsRunning())
	{
		OutMessage = TEXT("aucune simulation en cours (PIE seulement)");
		return false;
	}
	UAnastasisSaveGame* Save = Cast<UAnastasisSaveGame>(UGameplayStatics::CreateSaveGameObject(UAnastasisSaveGame::StaticClass()));
	if (!Save)
	{
		OutMessage = TEXT("conteneur de sauvegarde non cree");
		return false;
	}
	Save->HostVersion = UAnastasisSaveGame::HostFormatVersion;
	Simulation.SaveState(Save->SimState);
	Save->StateDigest = AnastasisDigest::ToHex(Simulation.StateDigest());
	Save->SavedAtUtc = FDateTime::UtcNow().ToIso8601();
	Save->GeoScenarioPath = Simulation.GetGeo().IsLoaded() ? GeoScenarioPath : FString();
	Save->bStartVillage = bStartVillage;
	Save->OpeningSiteId = OpeningSiteId;
	Save->OpeningWorkId = OpeningWorkId;
	Save->FarmerGranaryId = FarmerGranaryId;
	Save->FirstSiteId = FirstSiteId;
	Save->FarmerField = FarmerField;
	if (!UGameplayStatics::SaveGameToSlot(Save, Slot, 0))
	{
		OutMessage = FString::Printf(TEXT("ecriture du slot %s refusee"), *Slot);
		return false;
	}
	SaveStatus = FString::Printf(
		TEXT("{\"op\":\"save\",\"ok\":true,\"slot\":%s,\"bytes\":%d,\"digest\":%s,\"time\":%.17g,\"day\":%d}"),
		*QuoteJson(Slot), Save->SimState.Num(), *QuoteJson(Save->StateDigest), Simulation.GetTime(), Simulation.GetDay());
	OutMessage = FString::Printf(TEXT("slot=%s bytes=%d digest=%s day=%d time=%.17g npcs=%d buildings=%d geo=%s"),
		*Slot, Save->SimState.Num(), *Save->StateDigest, Simulation.GetDay(), Simulation.GetTime(),
		Simulation.GetVillage().GetActors().Num(), Simulation.GetVillage().GetBuildings().Num(),
		Save->GeoScenarioPath.IsEmpty() ? TEXT("-") : *Save->GeoScenarioPath);
	return true;
}

bool UAnastasisSimulationSubsystem::LoadGameFromSlot(const FString& InSlot, FString& OutMessage)
{
	const FString Slot = SlotOrDefault(InSlot);
	const auto Refuse = [this, &Slot, &OutMessage](const FString& Why)
	{
		OutMessage = FString::Printf(TEXT("slot=%s refuse : %s"), *Slot, *Why);
		SaveStatus = FString::Printf(TEXT("{\"op\":\"load\",\"ok\":false,\"slot\":%s,\"error\":%s}"), *QuoteJson(Slot), *QuoteJson(Why));
		return false;
	};
	if (!Simulation.IsRunning())
	{
		return Refuse(TEXT("aucune simulation en cours (PIE seulement)"));
	}
	if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		return Refuse(TEXT("slot absent"));
	}
	UAnastasisSaveGame* Save = Cast<UAnastasisSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Save)
	{
		return Refuse(TEXT("slot illisible ou d'un autre type"));
	}
	if (Save->HostVersion != UAnastasisSaveGame::HostFormatVersion)
	{
		return Refuse(FString::Printf(TEXT("conteneur version %d, ce jeu lit %d"), Save->HostVersion, UAnastasisSaveGame::HostFormatVersion));
	}

	FAnastasisSimulation::FSaveHeader Header;
	FString Error;
	if (!FAnastasisSimulation::ReadSaveHeader(Save->SimState, Header, Error))
	{
		return Refuse(Error);
	}
	AnastasisGeo::FScenario Scenario;
	if (Header.bGeoLoaded)
	{
		if (Save->GeoScenarioPath.IsEmpty() || !ReadGeoScenarioFile(Save->GeoScenarioPath, Scenario))
		{
			return Refuse(FString::Printf(TEXT("scenario exterieur %s illisible (%s)"), *Header.GeoScenarioId, *Save->GeoScenarioPath));
		}
	}
	if (!Simulation.LoadState(Save->SimState, Error, Header.bGeoLoaded ? &Scenario : nullptr))
	{
		return Refuse(Error);
	}

	// La simulation est la partie sauvee. La presentation de l'ancienne partie part, elle se refait.
	UWorld* World = GetWorld();
	VillagePresentation.Clear(World ? World->GetSubsystem<UAnastasisVillageInteractionSubsystem>() : nullptr);
	if (UAnastasisAnthropicSubsystem* Anthropic = World ? World->GetSubsystem<UAnastasisAnthropicSubsystem>() : nullptr)
	{
		Anthropic->ResetPresentation();
	}
	RainCanopyActor.Reset();
	bPendingStartVillage = false;
	StartVillageWait = 0.0;
	bStartVillage = Save->bStartVillage;
	OpeningSiteId = Save->OpeningSiteId;
	OpeningWorkId = Save->OpeningWorkId;
	FarmerGranaryId = Save->FarmerGranaryId;
	FirstSiteId = Save->FirstSiteId;
	FarmerField = Save->FarmerField;
	GeoScenarioPath = Header.bGeoLoaded ? Save->GeoScenarioPath : FString();
	// Le temoin du joueur repart de ce que la simulation sait de lui (presence, jours oisifs).
	WitnessPersonId.Reset();
	PresentationAccumulator = Simulation.GetAccumulator();
	LoggedDay = Simulation.GetDay();
	BindRainCanopy();
	const int32 Synced = SyncVillagePresentation();
	// Les cartes des habitants, comme apres Anastasis.Sim.Advance : tout de suite, meme si le temps est fige.
	const IConsoleVariable* Portraits = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Village.Portraits"));
	VillagePresentation.SyncVillagers(
		Simulation.GetVillage(), Simulation.GetWorld(), World,
		AnastasisPresentation::GetRegistry(), !Portraits || Portraits->GetInt() != 0,
		1.0, true);
	PlacePlayerPawn();

	const FString Digest = AnastasisDigest::ToHex(Simulation.StateDigest());
	const bool bSame = Digest == Save->StateDigest;
	SaveStatus = FString::Printf(
		TEXT("{\"op\":\"load\",\"ok\":true,\"slot\":%s,\"bytes\":%d,\"digest\":%s,\"saved_digest\":%s,\"digest_match\":%s,")
		TEXT("\"time\":%.17g,\"day\":%d,\"presentation_synced\":%d}"),
		*QuoteJson(Slot), Save->SimState.Num(), *QuoteJson(Digest), *QuoteJson(Save->StateDigest), bSame ? TEXT("true") : TEXT("false"),
		Simulation.GetTime(), Simulation.GetDay(), Synced);
	OutMessage = FString::Printf(TEXT("slot=%s bytes=%d digest=%s saved=%s match=%d day=%d time=%.17g npcs=%d buildings=%d geo=%s"),
		*Slot, Save->SimState.Num(), *Digest, *Save->StateDigest, bSame ? 1 : 0, Simulation.GetDay(), Simulation.GetTime(),
		Simulation.GetVillage().GetActors().Num(), Simulation.GetVillage().GetBuildings().Num(),
		GeoScenarioPath.IsEmpty() ? TEXT("-") : *GeoScenarioPath);
	return true;
}

FString UAnastasisSimulationDebugLibrary::GetSaveStatus(const UObject* WorldContextObject)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host || !Host->GetSimulation().IsRunning())
	{
		return TEXT("{}");
	}
	const FAnastasisSimulation& Sim = Host->GetSimulation();
	return FString::Printf(
		TEXT("{\"digest\":%s,\"time\":%.17g,\"day\":%d,\"npcs\":%d,\"buildings\":%d,\"building_actors\":%d,\"villager_actors\":%d,")
		TEXT("\"geo_loaded\":%s,\"last\":%s}"),
		*QuoteJson(AnastasisDigest::ToHex(Sim.StateDigest())), Sim.GetTime(), Sim.GetDay(),
		Sim.GetVillage().GetActors().Num(), Sim.GetVillage().GetBuildings().Num(),
		Host->GetVillagePresentation().Num(), Host->GetVillagePresentation().NumVillagers(),
		Sim.GetGeo().IsLoaded() ? TEXT("true") : TEXT("false"), *Host->GetSaveStatus());
}

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisSimSave(
	TEXT("Anastasis.Sim.Save"),
	TEXT("Anastasis.Sim.Save [slot] - sauve la partie (etat complet de la simulation + verrous de l'hote) dans "
		"Saved/SaveGames/<slot>.sav ; defaut anastasis.Sim.SaveSlot. SAVE_STATE_001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		FString Message = TEXT("aucun hote dans ce monde (PIE seulement)");
		if (Host && Host->SaveGameToSlot(Args.IsValidIndex(0) ? Args[0] : FString(), Message))
		{
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_SAVE saved %s"), *Message);
			return;
		}
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_SAVE save refused: %s"), *Message);
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisSimLoad(
	TEXT("Anastasis.Sim.Load"),
	TEXT("Anastasis.Sim.Load [slot] - recharge une partie sauvee : simulation remplacee (refus = rien ne change), "
		"presentation refaite. Defaut anastasis.Sim.SaveSlot. SAVE_STATE_001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		FString Message = TEXT("aucun hote dans ce monde (PIE seulement)");
		if (Host && Host->LoadGameFromSlot(Args.IsValidIndex(0) ? Args[0] : FString(), Message))
		{
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_SAVE loaded %s"), *Message);
			UE_LOG(LogAnastasis_UnrealV2, Display,
				TEXT("ANASTASIS_SAVE presentation history not restored: anthropic memory (experimental) restarts from the loaded state; building biographies are simulation state and were restored (SAVE_STATE_001)"));
			return;
		}
		UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_SAVE load refused: %s"), *Message);
	}));
