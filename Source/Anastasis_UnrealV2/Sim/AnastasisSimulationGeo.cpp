// geopolitical-world-001 -- le monde exterieur, cote Unreal (ecart n°38 dans AnastasisSim).
//
// L'hote ne decide rien : il charge un scenario (donnees historiques, avec leur provenance), le
// confie a la simulation, et donne aux agents et aux preuves de quoi intervenir et regarder.
// Desactive par defaut : sans `Anastasis.Geo.Load`, la partie est celle d'avant.
//
//   Anastasis.Geo.Load [chemin]        charge un scenario (defaut : anastasis.Geo.ScenarioPath)
//   Anastasis.Geo.Unload
//   Anastasis.Geo.Status               noeuds, routes, acteurs, chocs, paquets en route, exposition
//   Anastasis.Geo.Node <id>            pressions et influences d'un noeud
//   Anastasis.Geo.Exposure             ce que le village subit et ce qu'il sait
//   Anastasis.Geo.Trace <noeud> <canal>  pourquoi cette pression : paquet, routes, cause, source
//   Anastasis.Geo.Inject <noeud> Canal=0.8 ... [info=0.9] [duration=3] [start=12] [actor=x] [id=x] [label=x] [tags=a,b]
//   Anastasis.Geo.Route <id> <0|1>     ferme / rouvre une route
//   Anastasis.Geo.Save <nom> / Anastasis.Geo.Restore <nom>   etat vivant dans Saved/GeoState/<nom>.json

#include "Sim/AnastasisSimulationSubsystem.h"

#include "Anastasis_UnrealV2.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

static TAutoConsoleVariable<FString> CVarGeoScenarioPath(
	TEXT("anastasis.Geo.ScenarioPath"),
	TEXT("Anastasis/Scenario/geo-pontos-1204.json"),
	TEXT("geopolitical-world-001: scenario du monde exterieur lu par Anastasis.Geo.Load sans argument, relatif a Content/ (ou absolu)."),
	ECVF_Default);

namespace
{
	UAnastasisSimulationSubsystem* RunningHost(UWorld* World, const TCHAR* Command)
	{
		UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
		if (!Host || !Host->GetSimulation().IsRunning())
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_GEO %s: no running simulation in this world (PIE only)"), Command);
			return nullptr;
		}
		return Host;
	}

	void LogLines(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		for (const FString& Line : Lines)
		{
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("%s"), *Line);
		}
	}

	FString ResolveScenarioPath(const FString& Arg)
	{
		const FString Raw = Arg.IsEmpty() ? CVarGeoScenarioPath.GetValueOnGameThread() : Arg;
		return FPaths::IsRelative(Raw) ? FPaths::Combine(FPaths::ProjectContentDir(), Raw) : Raw;
	}

	bool ReadScenario(const FString& Path, AnastasisGeo::FScenario& Out)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_GEO load refused: cannot read %s"), *Path);
			return false;
		}
		TArray<FString> Errors;
		if (!AnastasisGeo::ParseScenario(Json, Out, Errors))
		{
			for (const FString& E : Errors)
			{
				UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_GEO scenario invalid: %s"), *E);
			}
			UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_GEO load refused: %d error(s) in %s"), Errors.Num(), *Path);
			return false;
		}
		return true;
	}

	FString StatePath(const FString& Name)
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GeoState"), (Name.IsEmpty() ? FString(TEXT("geo-state")) : Name) + TEXT(".json"));
	}
}

void UAnastasisSimulationSubsystem::LogGeoEvents()
{
	for (const FString& Line : Simulation.GetGeo().DrainEvents())
	{
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_GEO %s"), *Line);
	}
}

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisGeoLoad(
	TEXT("Anastasis.Geo.Load"),
	TEXT("Anastasis.Geo.Load [path] - loads the outside-world scenario (default anastasis.Geo.ScenarioPath, relative to Content/). "
		"Refuses an invalid scenario and logs every error. geopolitical-world-001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = RunningHost(World, TEXT("load"));
		if (!Host)
		{
			return;
		}
		const FString Path = ResolveScenarioPath(Args.IsValidIndex(0) ? Args[0] : FString());
		AnastasisGeo::FScenario Scenario;
		if (!ReadScenario(Path, Scenario))
		{
			return;
		}
		FAnastasisSimulation& Sim = Host->GetSimulation();
		TArray<FString> Errors;
		if (!Sim.GetGeo().Load(Scenario, Sim.GetDay(), Errors))
		{
			for (const FString& E : Errors)
			{
				UE_LOG(LogAnastasis_UnrealV2, Error, TEXT("ANASTASIS_GEO load refused: %s"), *E);
			}
			return;
		}
		Sim.AdmitGeoMigration();
		Host->LogGeoEvents();
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_GEO loaded %s from %s at day %d"), *Scenario.Id, *Path, Sim.GetDay());
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisGeoUnload(
	TEXT("Anastasis.Geo.Unload"),
	TEXT("Unloads the outside world: the village is a closed valley again. geopolitical-world-001."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = RunningHost(World, TEXT("unload")))
		{
			Host->GetSimulation().GetGeo().Unload();
			UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_GEO unloaded"));
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisGeoStatus(
	TEXT("Anastasis.Geo.Status"),
	TEXT("Logs the outside world: nodes, routes, actors, shocks, packets and rumors in transit, village exposure. geopolitical-world-001."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = RunningHost(World, TEXT("status")))
		{
			Host->LogGeoEvents();
			LogLines(Host->GetSimulation().GetGeo().DescribeStatus());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisGeoNode(
	TEXT("Anastasis.Geo.Node"),
	TEXT("Anastasis.Geo.Node <id> - true pressures, baselines and actor influences of one node. geopolitical-world-001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = RunningHost(World, TEXT("node")))
		{
			LogLines(Host->GetSimulation().GetGeo().DescribeNode(Args.IsValidIndex(0) ? Args[0] : FString()));
		}
	}));

static FAutoConsoleCommandWithWorld CmdAnastasisGeoExposure(
	TEXT("Anastasis.Geo.Exposure"),
	TEXT("What reaches the village (exposure) and what it knows (rumors and lived pressure). geopolitical-world-001."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UAnastasisSimulationSubsystem* Host = RunningHost(World, TEXT("exposure")))
		{
			LogLines(Host->GetSimulation().GetGeo().DescribeExposure());
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisGeoTrace(
	TEXT("Anastasis.Geo.Trace"),
	TEXT("Anastasis.Geo.Trace <node> <TradeDisruption|Insecurity|Migration|Military|Extraction> - why this pressure: packets, routes, root cause, source. geopolitical-world-001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = RunningHost(World, TEXT("trace"));
		if (!Host)
		{
			return;
		}
		AnastasisGeo::EPressure Pressure;
		if (Args.Num() < 2 || !AnastasisGeo::ParsePressure(Args[1], Pressure))
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_GEO trace refused: usage Anastasis.Geo.Trace <node> <channel>"));
			return;
		}
		LogLines(Host->GetSimulation().GetGeo().DescribeTrace(Args[0], Pressure));
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisGeoInject(
	TEXT("Anastasis.Geo.Inject"),
	TEXT("Anastasis.Geo.Inject <node> TradeDisruption=1 Migration=0.6 ... [info=0.9] [duration=3] [start=<day>] [actor=<id>] [id=<id>] [label=<text_with_underscores>] [tags=a,b] - "
		"a developer shock (provenance: dev-intervention, ABSTRACTION). It starts today unless start= is given. geopolitical-world-001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = RunningHost(World, TEXT("inject"));
		if (!Host)
		{
			return;
		}
		FAnastasisSimulation& Sim = Host->GetSimulation();
		if (Args.Num() < 2)
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_GEO inject refused: usage Anastasis.Geo.Inject <node> Channel=magnitude ..."));
			return;
		}
		AnastasisGeo::FShockDef Shock;
		Shock.SourceNode = Args[0];
		Shock.StartDay = Sim.GetDay();
		Shock.Label = TEXT("intervention de developpement");
		Shock.Provenance.SourceId = TEXT("dev-intervention");
		Shock.Provenance.SourceRef = TEXT("Anastasis.Geo.Inject");
		Shock.Provenance.Status = AnastasisGeo::EHistoricalStatus::Abstraction;
		Shock.Provenance.Confidence = 1.0;
		for (int32 I = 1; I < Args.Num(); ++I)
		{
			FString Key;
			FString Value;
			if (!Args[I].Split(TEXT("="), &Key, &Value))
			{
				UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_GEO inject refused: '%s' is not key=value"), *Args[I]);
				return;
			}
			AnastasisGeo::EPressure Pressure;
			if (AnastasisGeo::ParsePressure(Key, Pressure))
			{
				Shock.Emissions.Add({ Pressure, FCString::Atod(*Value) });
			}
			else if (Key == TEXT("info"))
			{
				Shock.InformationMagnitude = FCString::Atod(*Value);
			}
			else if (Key == TEXT("duration"))
			{
				Shock.DurationDays = FCString::Atoi(*Value);
			}
			else if (Key == TEXT("start"))
			{
				Shock.StartDay = FCString::Atoi(*Value);
			}
			else if (Key == TEXT("actor"))
			{
				Shock.ActorId = Value;
			}
			else if (Key == TEXT("id"))
			{
				Shock.Id = Value;
			}
			else if (Key == TEXT("label"))
			{
				Shock.Label = Value.Replace(TEXT("_"), TEXT(" "));
			}
			else if (Key == TEXT("tags"))
			{
				Value.ParseIntoArray(Shock.CauseTags, TEXT(","));
			}
			else
			{
				UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_GEO inject refused: unknown key '%s'"), *Key);
				return;
			}
		}
		TArray<FString> Errors;
		const FString Id = Sim.GetGeo().InjectShock(Shock, Errors);
		if (Id.IsEmpty())
		{
			for (const FString& E : Errors)
			{
				UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_GEO inject refused: %s"), *E);
			}
			return;
		}
		Sim.AdmitGeoMigration();
		Host->LogGeoEvents();
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisGeoRoute(
	TEXT("Anastasis.Geo.Route"),
	TEXT("Anastasis.Geo.Route <id> <0|1> - closes or reopens a route; packets already on it still arrive, none leave on it. geopolitical-world-001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = RunningHost(World, TEXT("route"));
		if (!Host)
		{
			return;
		}
		if (Args.Num() < 2 || !Host->GetSimulation().GetGeo().SetRouteEnabled(Args[0], Args[1] != TEXT("0")))
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_GEO route refused: usage Anastasis.Geo.Route <known id> <0|1>"));
			return;
		}
		Host->LogGeoEvents();
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisGeoSave(
	TEXT("Anastasis.Geo.Save"),
	TEXT("Anastasis.Geo.Save [name] - writes the live outside-world state to Saved/GeoState/<name>.json. geopolitical-world-001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = RunningHost(World, TEXT("save"));
		if (!Host || !Host->GetSimulation().GetGeo().IsLoaded())
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_GEO save refused: no scenario loaded"));
			return;
		}
		const FString Path = StatePath(Args.IsValidIndex(0) ? Args[0] : FString());
		const bool bOk = FFileHelper::SaveStringToFile(Host->GetSimulation().GetGeo().SaveState(), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_GEO save %s %s digest=%llu"), bOk ? TEXT("ok") : TEXT("FAILED"), *Path,
			Host->GetSimulation().GetGeo().Digest());
	}));

static FAutoConsoleCommandWithWorldAndArgs CmdAnastasisGeoRestore(
	TEXT("Anastasis.Geo.Restore"),
	TEXT("Anastasis.Geo.Restore [name] - reloads Saved/GeoState/<name>.json onto the loaded scenario; packets in transit keep their arrival day. geopolitical-world-001."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UAnastasisSimulationSubsystem* Host = RunningHost(World, TEXT("restore"));
		if (!Host || !Host->GetSimulation().GetGeo().IsLoaded())
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_GEO restore refused: load the scenario first"));
			return;
		}
		const FString Path = StatePath(Args.IsValidIndex(0) ? Args[0] : FString());
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_GEO restore refused: cannot read %s"), *Path);
			return;
		}
		AnastasisGeo::FGeoWorld& Geo = Host->GetSimulation().GetGeo();
		const AnastasisGeo::FScenario Scenario = Geo.GetScenario();
		TArray<FString> Errors;
		if (!Geo.LoadState(Scenario, Json, Errors))
		{
			for (const FString& E : Errors)
			{
				UE_LOG(LogAnastasis_UnrealV2, Warning, TEXT("ANASTASIS_GEO restore refused: %s"), *E);
			}
			return;
		}
		Host->LogGeoEvents();
		UE_LOG(LogAnastasis_UnrealV2, Display, TEXT("ANASTASIS_GEO restore ok %s digest=%llu"), *Path, Geo.Digest());
	}));

FString UAnastasisSimulationDebugLibrary::GetGeoStatus(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	if (!Host)
	{
		return TEXT("{}");
	}
	const FAnastasisSimulation& Sim = Host->GetSimulation();
	return FString::Printf(TEXT("{\"simDay\":%d,\"actors\":%d,\"geo\":%s}"), Sim.GetDay(), Sim.GetVillage().GetActors().Num(),
		*Sim.GetGeo().StatusJson());
}

FString UAnastasisSimulationDebugLibrary::GetGeoTrace(const UObject* WorldContextObject, const FString& NodeId, const FString& Pressure)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const UAnastasisSimulationSubsystem* Host = World ? World->GetSubsystem<UAnastasisSimulationSubsystem>() : nullptr;
	AnastasisGeo::EPressure Parsed;
	if (!Host || !AnastasisGeo::ParsePressure(Pressure, Parsed))
	{
		return FString();
	}
	return Host->GetSimulation().GetGeo().DescribeTrace(NodeId, Parsed);
}
