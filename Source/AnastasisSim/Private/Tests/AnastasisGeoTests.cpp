#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Core/AnastasisSimClock.h"
#include "Geo/AnastasisGeo.h"
#include "Sim/AnastasisSimulation.h"

#if WITH_DEV_AUTOMATION_TESTS

// geopolitical-world-001 -- monde exterieur V1 (ecart n°38).
//
// Graphe de test, generique (aucun lieu historique) :
//
//        A ---4j--- B ---3j--- V        A : noeud lointain ou nait le choc
//         \        /          /         V : le village
//          2j    2j         6j          A-B-C forme un cycle ; C-V est un detour
//            \  /          /
//              C ---------
//
// Valeurs attendues, calculees a la main depuis les regles du scenario ci-dessous :
//   commerce  (vitesse 1, relais 0,8) : A j2 = 1 ; C j4 = 0,8 ; B j6 = 0,8 (par A-B) ; V j9 = 0,64
//   nouvelle  (vitesse 0,5, fiabilite 0,85/relais) : V la sait au j6, fiabilite 0,7225, 2 relais
//   migration (vitesse 2, relais 0,8) : B j10 = 0,64 ; V j16 = 0,512 -> 5 personnes (10/unite, plafond 6)

namespace AnastasisGeoTest
{
	using namespace AnastasisGeo;

	constexpr double Eps = 1e-12;

	FString Prov(const TCHAR* Status = TEXT("ABSTRACTION"))
	{
		return FString::Printf(TEXT("{\"source\":\"test\",\"ref\":\"AnastasisGeoTests\",\"status\":\"%s\",\"confidence\":1}"), Status);
	}

	FString Channel(double Speed)
	{
		return FString::Printf(TEXT("{\"retentionPerDay\":0.9,\"speedFactor\":%g,\"hopAttenuation\":0.8,\"minMagnitude\":0.03}"), Speed);
	}

	FString Route(const TCHAR* Id, const TCHAR* From, const TCHAR* To, double Days)
	{
		return FString::Printf(TEXT("{\"id\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"travelDays\":%g,\"provenance\":%s}"),
			Id, From, To, Days, *Prov());
	}

	FString Node(const TCHAR* Id, const TCHAR* Type)
	{
		return FString::Printf(TEXT("{\"id\":\"%s\",\"type\":\"%s\",\"provenance\":%s}"), Id, Type, *Prov());
	}

	/** Le scenario de test. `Extra` s'insere tel quel dans la racine (pour casser la validation). */
	FString ScenarioJson(const FString& NodesOverride = FString(), const FString& RoutesOverride = FString(),
		const FString& ActorsOverride = FString(), const FString& ShocksOverride = FString(), const TCHAR* Village = TEXT("V"))
	{
		const FString Nodes = !NodesOverride.IsEmpty() ? NodesOverride : FString::Join(TArray<FString>{
			Node(TEXT("A"), TEXT("PoliticalCenter")), Node(TEXT("B"), TEXT("Pass")), Node(TEXT("C"), TEXT("Region")),
			Node(TEXT("V"), TEXT("Settlement")) }, TEXT(","));
		const FString Routes = !RoutesOverride.IsEmpty() ? RoutesOverride : FString::Join(TArray<FString>{
			Route(TEXT("AB"), TEXT("A"), TEXT("B"), 4), Route(TEXT("BV"), TEXT("B"), TEXT("V"), 3),
			Route(TEXT("AC"), TEXT("A"), TEXT("C"), 2), Route(TEXT("CB"), TEXT("C"), TEXT("B"), 2),
			Route(TEXT("CV"), TEXT("C"), TEXT("V"), 6) }, TEXT(","));
		const FString Actors = !ActorsOverride.IsEmpty() ? ActorsOverride : FString::Printf(
			TEXT("{\"id\":\"far-power\",\"label\":\"Puissance lointaine\",\"influence\":[{\"node\":\"A\",\"political\":0.9,\"military\":0.7}],\"provenance\":%s}"),
			*Prov());
		return FString::Printf(TEXT("{\"id\":\"test-graph\",\"villageNode\":\"%s\",\"provenance\":%s,"
			"\"rules\":{\"maxHops\":6,\"administrationDamping\":0.5,"
			"\"channels\":{\"TradeDisruption\":%s,\"Insecurity\":%s,\"Migration\":%s,\"Military\":%s,\"Extraction\":%s},"
			"\"information\":{\"speedFactor\":0.5,\"reliabilityPerHop\":0.85,\"exaggerationPerHop\":0.15,\"minReliability\":0.2},"
			"\"migration\":{\"personsPerUnit\":10,\"maxPersonsPerBatch\":6,\"minMagnitude\":0.05}},"
			"\"nodes\":[%s],\"routes\":[%s],\"actors\":[%s],\"shocks\":[%s]}"),
			Village, *Prov(), *Channel(1.0), *Channel(1.0), *Channel(2.0), *Channel(1.0), *Channel(1.0),
			*Nodes, *Routes, *Actors, *ShocksOverride);
	}

	bool MakeScenario(FAutomationTestBase& Test, FScenario& Out, const FString& Json = ScenarioJson())
	{
		TArray<FString> Errors;
		const bool bOk = ParseScenario(Json, Out, Errors);
		for (const FString& E : Errors)
		{
			Test.AddError(E);
		}
		return bOk;
	}

	/** Le choc de reference : au noeud A, commence au jour 2, commerce 1, migration 0,8, nouvelle 1. */
	FShockDef RemoteCrisis()
	{
		FShockDef S;
		S.Id = TEXT("crisis-A");
		S.Label = TEXT("crise lointaine (test)");
		S.SourceNode = TEXT("A");
		S.ActorId = TEXT("far-power");
		S.StartDay = 2;
		S.DurationDays = 1;
		S.Emissions.Add({ EPressure::TradeDisruption, 1.0 });
		S.Emissions.Add({ EPressure::Migration, 0.8 });
		S.InformationMagnitude = 1.0;
		S.CauseTags = { TEXT("instabilite") };
		S.Provenance.SourceId = TEXT("test");
		S.Provenance.Status = EHistoricalStatus::Abstraction;
		S.Provenance.Confidence = 1.0;
		return S;
	}

	/** Monde charge au jour 1, choc injecte. */
	bool MakeWorld(FAutomationTestBase& Test, FGeoWorld& World)
	{
		FScenario Scenario;
		if (!MakeScenario(Test, Scenario))
		{
			return false;
		}
		TArray<FString> Errors;
		if (!World.Load(Scenario, 1, Errors))
		{
			Test.AddError(FString::Join(Errors, TEXT(" | ")));
			return false;
		}
		if (World.InjectShock(RemoteCrisis(), Errors).IsEmpty())
		{
			Test.AddError(FString::Join(Errors, TEXT(" | ")));
			return false;
		}
		return true;
	}

	double VillageTrade(const FGeoWorld& World)
	{
		return World.GetVillageExposure().Pressure[static_cast<int32>(EPressure::TradeDisruption)];
	}

	const FArrival* FindArrival(const FGeoWorld& World, const TCHAR* Node, EPressure Pressure)
	{
		return World.GetArrivals().FindByPredicate([&](const FArrival& A) { return A.NodeId == Node && A.Pressure == Pressure; });
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoShockPackets, "Anastasis.Sim.Geo.ChocPaquets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoShockPackets::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// TEST_01 : un choc cree ses paquets, au bon depart, et rien avant son jour.
	FGeoWorld World;
	if (!MakeWorld(*this, World))
	{
		return false;
	}
	TestEqual(TEXT("jour 1 : le choc attend son jour, aucun paquet"), World.GetPressureInTransit().Num(), 0);
	World.AdvanceToDay(2);
	TestNearlyEqual(TEXT("source A : commerce 1"), World.GetTruePressure(TEXT("A"), EPressure::TradeDisruption), 1.0, Eps);
	const FPressurePacket* AB = World.GetPressureInTransit().FindByPredicate([](const FPressurePacket& P)
		{ return P.Pressure == EPressure::TradeDisruption && P.ToNode == TEXT("B"); });
	const FPressurePacket* AC = World.GetPressureInTransit().FindByPredicate([](const FPressurePacket& P)
		{ return P.Pressure == EPressure::TradeDisruption && P.ToNode == TEXT("C"); });
	if (!TestNotNull(TEXT("paquet commerce A->B"), AB) || !TestNotNull(TEXT("paquet commerce A->C"), AC))
	{
		return false;
	}
	TestEqual(TEXT("A->B arrive au jour 6 (2 + 4)"), AB->ArrivalDay, 6);
	TestEqual(TEXT("A->C arrive au jour 4 (2 + 2)"), AC->ArrivalDay, 4);
	TestEqual(TEXT("cause racine"), AB->RootCauseId, FString(TEXT("crisis-A")));
	TestFalse(TEXT("parent : l'emission a la source"), AB->ParentPacketId.IsEmpty());
	TestEqual(TEXT("une nouvelle est partie aussi"), World.GetInformationInTransit().Num() > 0, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoNoEarlyArrival, "Anastasis.Sim.Geo.PasAvantLeDelai",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoNoEarlyArrival::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// TEST_02 + TEST_07 : tant que le paquet n'est pas arrive, le village ne sent rien, meme si les
	// noeuds intermediaires sont deja sous pression.
	FGeoWorld World;
	if (!MakeWorld(*this, World))
	{
		return false;
	}
	for (int32 D = 2; D <= 8; ++D)
	{
		World.AdvanceToDay(D);
		TestNearlyEqual(*FString::Printf(TEXT("jour %d : commerce au village = base"), D), VillageTrade(World), 0.0, Eps);
	}
	TestNearlyEqual(TEXT("jour 8 : B est deja touche (verite)"), World.GetTruePressure(TEXT("B"), EPressure::TradeDisruption) > 0.5 ? 1.0 : 0.0, 1.0, Eps);
	TestNull(TEXT("aucune arrivee de commerce au village avant le jour 9"), FindArrival(World, TEXT("V"), EPressure::TradeDisruption));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoArrivalAfterDelay, "Anastasis.Sim.Geo.ArriveeApresDelai",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoArrivalAfterDelay::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// TEST_03 + TEST_04 : l'arrivee tombe au jour calcule, avec l'intensite calculee.
	FGeoWorld World;
	if (!MakeWorld(*this, World))
	{
		return false;
	}
	World.AdvanceToDay(9);
	const FArrival* AtB = FindArrival(World, TEXT("B"), EPressure::TradeDisruption);
	const FArrival* AtV = FindArrival(World, TEXT("V"), EPressure::TradeDisruption);
	if (!TestNotNull(TEXT("arrivee a B"), AtB) || !TestNotNull(TEXT("arrivee au village"), AtV))
	{
		return false;
	}
	TestEqual(TEXT("B touche au jour 6"), AtB->Day, 6);
	TestNearlyEqual(TEXT("B : 1 x 1 x 0,8"), AtB->Magnitude, 0.8, Eps);
	TestEqual(TEXT("village touche au jour 9 (6 + 3)"), AtV->Day, 9);
	TestNearlyEqual(TEXT("village : 0,8 x 0,8"), AtV->Magnitude, 0.64, Eps);
	TestNearlyEqual(TEXT("exposition du village = 0,64 le jour de l'arrivee"), VillageTrade(World), 0.64, Eps);
	World.AdvanceToDay(10);
	// Jour 10 : la valeur d'hier decroit (x 0,9), puis le second courant arrive par le detour C-V
	// (0,64) et s'y combine : 1 - (1 - 0,576)(1 - 0,64). Il pese, il ne se relaie pas.
	TestNearlyEqual(TEXT("jour 10 : decroissance puis second courant"), VillageTrade(World), 1.0 - (1.0 - 0.64 * 0.9) * (1.0 - 0.64), Eps);
	int32 VillageTradeArrivals = 0;
	for (const FArrival& A : World.GetArrivals())
	{
		VillageTradeArrivals += A.NodeId == TEXT("V") && A.Pressure == EPressure::TradeDisruption ? 1 : 0;
	}
	TestEqual(TEXT("deux courants au village (B-V jour 9, C-V jour 10)"), VillageTradeArrivals, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoCycle, "Anastasis.Sim.Geo.CycleBorne",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoCycle::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// TEST_05 : le cycle A-B-C ne fait pas tourner la pression. Chaque (cause, canal, noeud) ne se
	// relaie qu'une fois ; un second courant pese sans repartir ; tout s'eteint.
	FGeoWorld World;
	if (!MakeWorld(*this, World))
	{
		return false;
	}
	World.AdvanceToDay(120);
	TestEqual(TEXT("plus rien en route"), World.GetPressureInTransit().Num(), 0);
	TestEqual(TEXT("plus aucune nouvelle en route"), World.GetInformationInTransit().Num(), 0);
	int32 TradeArrivals = 0;
	for (const FArrival& A : World.GetArrivals())
	{
		TradeArrivals += A.Pressure == EPressure::TradeDisruption ? 1 : 0;
	}
	// A (source), C (jour 4), B deux fois (par A et par C, jour 6), V deux fois (par B jour 9, par C jour 10).
	TestEqual(TEXT("commerce : six arrivees, nombre fixe"), TradeArrivals, 6);
	const int32 ArrivalsAt120 = World.GetArrivals().Num();
	World.AdvanceToDay(400);
	TestEqual(TEXT("rien ne repart, meme longtemps apres"), World.GetArrivals().Num(), ArrivalsAt120);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoInformationFirst, "Anastasis.Sim.Geo.NouvelleAvantLaChose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoInformationFirst::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// TEST_06 : une meme cause, des arrivees differentes. La nouvelle precede la penurie, qui precede
	// les gens ; la nouvelle est deformee et moins fiable que le vecu.
	FGeoWorld World;
	if (!MakeWorld(*this, World))
	{
		return false;
	}
	World.AdvanceToDay(5);
	TestEqual(TEXT("jour 5 : le village ne sait rien"), World.GetVillageKnowledge().Num(), 0);
	World.AdvanceToDay(6);
	const FKnownReport* Rumor = World.GetVillageKnowledge().FindByPredicate([](const FKnownReport& K) { return K.SourceType == TEXT("rumor"); });
	if (!TestNotNull(TEXT("jour 6 : la nouvelle est arrivee"), Rumor))
	{
		return false;
	}
	TestEqual(TEXT("apprise au jour 6"), Rumor->LearnedDay, 6);
	TestEqual(TEXT("evenement du jour 2"), Rumor->EventDay, 2);
	TestNearlyEqual(TEXT("fiabilite 0,85^2"), Rumor->Reliability, 0.85 * 0.85, Eps);
	TestNearlyEqual(TEXT("commerce encore a la base quand la nouvelle arrive"), VillageTrade(World), 0.0, Eps);
	World.AdvanceToDay(16);
	const FArrival* Trade = FindArrival(World, TEXT("V"), EPressure::TradeDisruption);
	const FArrival* People = FindArrival(World, TEXT("V"), EPressure::Migration);
	if (!TestNotNull(TEXT("commerce arrive"), Trade) || !TestNotNull(TEXT("migration arrive"), People))
	{
		return false;
	}
	TestTrue(TEXT("nouvelle (6) < commerce (9) < migration (16)"), Rumor->LearnedDay < Trade->Day && Trade->Day < People->Day);
	TestEqual(TEXT("migration au jour 16"), People->Day, 16);
	TestNearlyEqual(TEXT("migration 0,8 x 0,8 x 0,8"), People->Magnitude, 0.512, Eps);
	if (TestEqual(TEXT("un groupe d'arrivants"), World.GetMigrationBatches().Num(), 1))
	{
		TestEqual(TEXT("5 personnes (0,512 x 10, arrondi)"), World.GetMigrationBatches()[0].Persons, 5);
		TestEqual(TEXT("en attente de l'adaptateur local"), World.GetMigrationBatches()[0].Status, FString(TEXT("pending")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoDeterminism, "Anastasis.Sim.Geo.Determinisme",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoDeterminism::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// TEST_08 : meme scenario, memes interventions = meme etat, au caractere pres.
	FGeoWorld First;
	FGeoWorld Second;
	if (!MakeWorld(*this, First) || !MakeWorld(*this, Second))
	{
		return false;
	}
	for (int32 D = 2; D <= 30; D += 3)
	{
		First.AdvanceToDay(D);
		Second.AdvanceToDay(D);
		TestEqual(*FString::Printf(TEXT("empreinte jour %d"), D), First.Digest(), Second.Digest());
	}
	TestEqual(TEXT("etat sauve identique"), First.SaveState(), Second.SaveState());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoSaveLoad, "Anastasis.Sim.Geo.SauvegardeEnRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoSaveLoad::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// TEST_09 : sauver au jour 5, paquets en route ; recharger ; ils arrivent au meme jour.
	FGeoWorld Original;
	if (!MakeWorld(*this, Original))
	{
		return false;
	}
	Original.AdvanceToDay(5);
	TestTrue(TEXT("des paquets sont en route au jour 5"), Original.GetPressureInTransit().Num() > 0);
	const FString Saved = Original.SaveState();

	FScenario Scenario;
	if (!MakeScenario(*this, Scenario))
	{
		return false;
	}
	FGeoWorld Restored;
	TArray<FString> Errors;
	if (!TestTrue(TEXT("etat relu"), Restored.LoadState(Scenario, Saved, Errors)))
	{
		AddError(FString::Join(Errors, TEXT(" | ")));
		return false;
	}
	TestEqual(TEXT("relu = sauve"), Restored.SaveState(), Saved);
	TestEqual(TEXT("jour relu"), Restored.GetDay(), 5);
	Original.AdvanceToDay(20);
	Restored.AdvanceToDay(20);
	TestEqual(TEXT("meme suite apres rechargement"), Restored.Digest(), Original.Digest());
	const FArrival* AtV = FindArrival(Restored, TEXT("V"), EPressure::TradeDisruption);
	if (TestNotNull(TEXT("le commerce arrive apres rechargement"), AtV))
	{
		TestEqual(TEXT("au jour 9, comme sans sauvegarde"), AtV->Day, 9);
	}

	// Un etat d'un autre scenario est refuse, sans toucher au monde.
	FScenario Other = Scenario;
	Other.Id = TEXT("autre");
	FGeoWorld Untouched;
	TArray<FString> OtherErrors;
	TestFalse(TEXT("etat d'un autre scenario refuse"), Untouched.LoadState(Other, Saved, OtherErrors));
	TestFalse(TEXT("monde laisse decharge"), Untouched.IsLoaded());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoValidation, "Anastasis.Sim.Geo.ValidationDonnees",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoValidation::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// TEST_10 : chaque corruption est refusee avec un message, jamais reparee.
	auto ExpectReject = [this](const TCHAR* What, const FString& Json, const TCHAR* Needle)
	{
		FScenario Scenario;
		TArray<FString> Errors;
		const bool bOk = ParseScenario(Json, Scenario, Errors);
		TestFalse(*FString::Printf(TEXT("%s : refuse"), What), bOk);
		const bool bNamed = Errors.ContainsByPredicate([&](const FString& E) { return E.Contains(Needle); });
		TestTrue(*FString::Printf(TEXT("%s : message qui le dit ('%s' dans %s)"), What, Needle, *FString::Join(Errors, TEXT(" | "))), bNamed);
	};
	FScenario Good;
	TestTrue(TEXT("le scenario de test est valide"), MakeScenario(*this, Good));

	const FString TwoA = Node(TEXT("A"), TEXT("Region")) + TEXT(",") + Node(TEXT("A"), TEXT("Region")) + TEXT(",") + Node(TEXT("V"), TEXT("Settlement"));
	ExpectReject(TEXT("noeud en double"), ScenarioJson(TwoA, Route(TEXT("AV"), TEXT("A"), TEXT("V"), 1), TEXT(" ")), TEXT("noeud en double"));
	ExpectReject(TEXT("route vers un noeud absent"),
		ScenarioJson(FString(), Route(TEXT("AZ"), TEXT("A"), TEXT("Z"), 1)), TEXT("arrivee inconnue"));
	ExpectReject(TEXT("temps de trajet negatif"),
		ScenarioJson(FString(), Route(TEXT("AB"), TEXT("A"), TEXT("B"), -2)), TEXT("temps de trajet"));
	ExpectReject(TEXT("ancre du village absente"), ScenarioJson(FString(), FString(), FString(), FString(), TEXT("nulle-part")), TEXT("ancre du village"));
	ExpectReject(TEXT("influence sur un noeud absent"), ScenarioJson(FString(), FString(),
		FString::Printf(TEXT("{\"id\":\"x\",\"influence\":[{\"node\":\"Z\",\"political\":0.5}],\"provenance\":%s}"), *Prov())), TEXT("noeud inconnu"));
	ExpectReject(TEXT("pression hors bornes"), ScenarioJson(FString(), FString(),
		FString::Printf(TEXT("{\"id\":\"x\",\"influence\":[{\"node\":\"A\",\"military\":1.7}],\"provenance\":%s}"), *Prov())), TEXT("hors [0, 1]"));
	ExpectReject(TEXT("acteur inconnu dans un choc"), ScenarioJson(FString(), FString(), FString(),
		FString::Printf(TEXT("{\"id\":\"s1\",\"source\":\"A\",\"actor\":\"fantome\",\"startDay\":2,\"emissions\":{\"Insecurity\":0.5},\"provenance\":%s}"), *Prov())),
		TEXT("acteur inconnu"));
	const FString Shock = FString::Printf(TEXT("{\"id\":\"s1\",\"source\":\"A\",\"startDay\":2,\"emissions\":{\"Insecurity\":0.5},\"provenance\":%s}"), *Prov());
	ExpectReject(TEXT("identifiant causal en double"), ScenarioJson(FString(), FString(), FString(), Shock + TEXT(",") + Shock), TEXT("en double"));
	ExpectReject(TEXT("choc date avant le jour 1"), ScenarioJson(FString(), FString(), FString(),
		FString::Printf(TEXT("{\"id\":\"s0\",\"source\":\"A\",\"startDay\":0,\"emissions\":{\"Insecurity\":0.5},\"provenance\":%s}"), *Prov())),
		TEXT("jour de debut"));
	ExpectReject(TEXT("statut historique inconnu"), ScenarioJson(FString(), FString(), FString(),
		FString::Printf(TEXT("{\"id\":\"s2\",\"source\":\"A\",\"startDay\":2,\"emissions\":{\"Insecurity\":0.5},\"provenance\":%s}"), *Prov(TEXT("CERTAIN")))),
		TEXT("statut historique inconnu"));
	ExpectReject(TEXT("canal inconnu"), ScenarioJson(FString(), FString(), FString(),
		FString::Printf(TEXT("{\"id\":\"s3\",\"source\":\"A\",\"startDay\":2,\"emissions\":{\"Plague\":0.5},\"provenance\":%s}"), *Prov())),
		TEXT("canal inconnu"));
	ExpectReject(TEXT("JSON casse"), TEXT("{\"id\":"), TEXT("JSON illisible"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoRouteClosed, "Anastasis.Sim.Geo.RouteFermee",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoRouteClosed::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// La geographie compte : fermer B-V fait passer la penurie par le detour C-V, plus tard
	// (jour 10 au lieu de 9) ; fermer aussi C-V la bloque.
	FGeoWorld Detour;
	if (!MakeWorld(*this, Detour))
	{
		return false;
	}
	TestTrue(TEXT("route B-V fermee"), Detour.SetRouteEnabled(TEXT("BV"), false));
	Detour.AdvanceToDay(30);
	const FArrival* Late = FindArrival(Detour, TEXT("V"), EPressure::TradeDisruption);
	if (TestNotNull(TEXT("le commerce arrive par le detour"), Late))
	{
		TestEqual(TEXT("au jour 10 (C touche au jour 4, + 6)"), Late->Day, 10);
		TestNearlyEqual(TEXT("meme attenuation (deux relais)"), Late->Magnitude, 0.64, Eps);
	}

	FGeoWorld Blocked;
	if (!MakeWorld(*this, Blocked))
	{
		return false;
	}
	Blocked.SetRouteEnabled(TEXT("BV"), false);
	Blocked.SetRouteEnabled(TEXT("CV"), false);
	Blocked.AdvanceToDay(30);
	TestNull(TEXT("toutes les routes fermees : rien n'atteint le village"), FindArrival(Blocked, TEXT("V"), EPressure::TradeDisruption));
	TestEqual(TEXT("ni la nouvelle"), Blocked.GetVillageKnowledge().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoTrace, "Anastasis.Sim.Geo.TraceCausale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoTrace::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// Pourquoi le commerce du village vaut 0,64 ? Parce que le paquet X, venu de la cause Y par A->B->V.
	FGeoWorld World;
	if (!MakeWorld(*this, World))
	{
		return false;
	}
	World.AdvanceToDay(9);
	const TArray<FCauseTrace> Traces = World.TraceCause(TEXT("V"), EPressure::TradeDisruption);
	if (!TestEqual(TEXT("une arrivee a expliquer"), Traces.Num(), 1))
	{
		return false;
	}
	const FCauseTrace& T = Traces[0];
	TestEqual(TEXT("cause racine"), T.RootCauseId, FString(TEXT("crisis-A")));
	TestEqual(TEXT("acteur de la cause"), T.RootActorId, FString(TEXT("far-power")));
	if (TestEqual(TEXT("trois maillons : B->V, A->B, emission a A"), T.Steps.Num(), 3))
	{
		TestEqual(TEXT("dernier maillon"), T.Steps[0].RouteId, FString(TEXT("BV")));
		TestEqual(TEXT("maillon du milieu"), T.Steps[1].RouteId, FString(TEXT("AB")));
		TestTrue(TEXT("emission a la source"), T.Steps[2].RouteId.IsEmpty() && T.Steps[2].ToNode == TEXT("A"));
	}
	TestTrue(TEXT("la trace se lit en texte"), World.DescribeTrace(TEXT("V"), EPressure::TradeDisruption).Contains(TEXT("crisis-A")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoVillageArrivals, "Anastasis.Sim.Geo.ArrivantsAuVillage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoVillageArrivals::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// Integration : la vraie simulation, son minuit et son village. Le groupe de 5 personnes devient
	// 5 habitants le jour 16, et seulement ce jour-la.
	FAnastasisSimulation Sim;
	Sim.Reset(12345u, 96, 96);
	FScenario Scenario;
	if (!MakeScenario(*this, Scenario))
	{
		return false;
	}
	TArray<FString> Errors;
	if (!Sim.GetGeo().Load(Scenario, Sim.GetDay(), Errors) || Sim.GetGeo().InjectShock(RemoteCrisis(), Errors).IsEmpty())
	{
		AddError(FString::Join(Errors, TEXT(" | ")));
		return false;
	}
	const int32 Before = Sim.GetVillage().GetActors().Num();
	const double Step = AnastasisSimClock::FixedDt * AnastasisSimClock::MaxStepMult;
	while (Sim.GetDay() < 15)
	{
		Sim.Tick(Step);
	}
	TestEqual(TEXT("jour 15 : personne n'est arrive"), Sim.GetVillage().GetActors().Num(), Before);
	TestEqual(TEXT("le monde exterieur suit le jour de la simulation"), Sim.GetGeo().GetDay(), 15);
	while (Sim.GetDay() < 16)
	{
		Sim.Tick(Step);
	}
	TestEqual(TEXT("jour 16 : cinq arrivants"), Sim.GetVillage().GetActors().Num(), Before + 5);
	if (TestEqual(TEXT("un groupe"), Sim.GetGeo().GetMigrationBatches().Num(), 1))
	{
		const FMigrationBatch& Batch = Sim.GetGeo().GetMigrationBatches()[0];
		TestEqual(TEXT("groupe admis"), Batch.Status, FString(TEXT("admitted")));
		TestEqual(TEXT("cinq identifiants"), Batch.AdmittedIds.Num(), 5);
		for (const FString& Id : Batch.AdmittedIds)
		{
			TestNotNull(*FString::Printf(TEXT("%s est un habitant"), *Id), Sim.GetVillage().FindNpc(Id));
		}
		TestEqual(TEXT("cause tracee jusqu'au groupe"), Batch.CauseId, FString(TEXT("crisis-A")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoQuietIsInert, "Anastasis.Sim.Geo.CalmeSansEffet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoQuietIsInert::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// Non-regression : un monde exterieur charge mais calme (aucun choc) ne change rien au village,
	// au bit pres ; ni l'etat des habitants ni le flux de tirages.
	FAnastasisSimulation Plain;
	FAnastasisSimulation WithGeo;
	Plain.Reset(777u, 64, 64);
	WithGeo.Reset(777u, 64, 64);
	FScenario Scenario;
	if (!MakeScenario(*this, Scenario))
	{
		return false;
	}
	TArray<FString> Errors;
	if (!WithGeo.GetGeo().Load(Scenario, WithGeo.GetDay(), Errors))
	{
		AddError(FString::Join(Errors, TEXT(" | ")));
		return false;
	}
	for (FAnastasisSimulation* Sim : { &Plain, &WithGeo })
	{
		Sim->GetVillage().AdmitExternalArrivals(3, 3.0, 0.0);
	}
	const double Step = AnastasisSimClock::FixedDt * AnastasisSimClock::MaxStepMult;
	while (Plain.GetDay() < 4)
	{
		Plain.Tick(Step);
		WithGeo.Tick(Step);
	}
	TestEqual(TEXT("trois habitants de chaque cote"), WithGeo.GetVillage().GetActors().Num(), Plain.GetVillage().GetActors().Num());
	TestEqual(TEXT("empreinte du village identique"), WithGeo.GetVillage().Digest(), Plain.GetVillage().Digest());
	TestEqual(TEXT("flux de tirages identique"), WithGeo.GetVillage().GetSimRngState(), Plain.GetVillage().GetSimRngState());
	TestEqual(TEXT("le monde exterieur a bien avance"), WithGeo.GetGeo().GetDay(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisGeoShippedScenario, "Anastasis.Sim.Geo.ScenarioPontos1204",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisGeoShippedScenario::RunTest(const FString&)
{
	using namespace AnastasisGeoTest;
	using namespace AnastasisGeo;
	// Le scenario livre (Content/Anastasis/Scenario/geo-pontos-1204.json) valide, porte une provenance
	// sur chaque objet, et la nouvelle de 1204 finit par atteindre le village avant ses gens.
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Anastasis/Scenario/geo-pontos-1204.json"));
	FString Json;
	if (!TestTrue(*FString::Printf(TEXT("fichier lisible : %s"), *Path), FFileHelper::LoadFileToString(Json, *Path)))
	{
		return false;
	}
	FScenario Scenario;
	if (!MakeScenario(*this, Scenario, Json))
	{
		return false;
	}
	TestTrue(TEXT("au moins deux acteurs exterieurs"), Scenario.Actors.Num() >= 2);
	for (const FNode& N : Scenario.Nodes)
	{
		TestFalse(*FString::Printf(TEXT("noeud %s : source"), *N.Id), N.Provenance.SourceId.IsEmpty());
	}
	for (const FRoute& R : Scenario.Routes)
	{
		TestFalse(*FString::Printf(TEXT("route %s : source"), *R.Id), R.Provenance.SourceId.IsEmpty());
	}
	FGeoWorld World;
	TArray<FString> Errors;
	if (!TestTrue(TEXT("charge au jour 1"), World.Load(Scenario, 1, Errors)))
	{
		AddError(FString::Join(Errors, TEXT(" | ")));
		return false;
	}
	World.AdvanceToDay(90);
	const FKnownReport* News = World.GetVillageKnowledge().FindByPredicate([](const FKnownReport& K)
		{ return K.SourceType == TEXT("rumor") && K.CauseId == TEXT("fall-of-constantinople-1204"); });
	if (TestNotNull(TEXT("la nouvelle de 1204 atteint le village"), News))
	{
		const FArrival* People = FindArrival(World, TEXT("village"), EPressure::Migration);
		TestTrue(TEXT("et avant tout arrivant venu de cette cause"), !People || News->LearnedDay < People->Day);
		AddInfo(FString::Printf(TEXT("1204 : nouvelle au village le jour %d (fiabilite %.3f) ; migration %s"), News->LearnedDay, News->Reliability,
			People ? *FString::Printf(TEXT("jour %d m=%.3f"), People->Day, People->Magnitude) : TEXT("aucune")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
