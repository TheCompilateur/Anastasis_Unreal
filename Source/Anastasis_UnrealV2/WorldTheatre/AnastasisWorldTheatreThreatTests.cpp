#include "WorldTheatre/AnastasisWorldTheatreThreat.h"
#include "Geo/AnastasisGeo.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisWorldTheatreThreatTestSupport
{
using namespace AnastasisWorldTheatre;

inline FThreatSite Site(const TCHAR* Node, EThreatSign Sign, int32 Stage)
{
	FThreatSite S;
	S.Id = TEXT("t");
	S.NodeId = Node;
	S.Sign = Sign;
	S.Stage = Stage;
	return S;
}

/** Deux voisins ; parcharia : trois fumees et quatre feux ; matzouka : une fumee. */
inline TArray<FThreatSite> Sites()
{
	return {
		Site(TEXT("parcharia"), EThreatSign::Smoke, 0), Site(TEXT("parcharia"), EThreatSign::Smoke, 1), Site(TEXT("parcharia"), EThreatSign::Smoke, 2),
		Site(TEXT("parcharia"), EThreatSign::Beacon, 0), Site(TEXT("parcharia"), EThreatSign::Beacon, 1),
		Site(TEXT("parcharia"), EThreatSign::Beacon, 2), Site(TEXT("parcharia"), EThreatSign::Beacon, 3),
		Site(TEXT("matzouka"), EThreatSign::Smoke, 0),
	};
}

inline AnastasisWorldTheatreThreat::FThreatMap Calm()
{
	AnastasisWorldTheatreThreat::FThreatMap M;
	M.bLoaded = true;
	M.VillageNode = TEXT("village");
	M.NodeExcess.Add(TEXT("parcharia"), 0.0);
	M.NodeExcess.Add(TEXT("matzouka"), 0.0);
	return M;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisWorldTheatreThreatRules, "Anastasis.WorldTheatre.ThreatRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisWorldTheatreThreatRules::RunTest(const FString&)
{
	using namespace AnastasisWorldTheatreThreat;
	using namespace AnastasisWorldTheatreThreatTestSupport;
	const TArray<AnastasisWorldTheatre::FThreatSite> S = Sites();

	FThreatMap Unloaded;
	FSignals Out = Evaluate(Unloaded, S);
	bool bDark = true;
	for (const float I : Out.Site) bDark &= I == 0.0f;
	TestTrue(TEXT("no scenario loaded: nothing burns, nothing is signalled"), bDark && Out.Dread == 0.0f);

	Out = Evaluate(Calm(), S);
	bDark = true;
	for (const float I : Out.Site) bDark &= I == 0.0f;
	TestTrue(TEXT("pressures at their baseline: nothing shows (a normal frontier is not a raid)"), bDark);

	FThreatMap Burning = Calm();
	Burning.NodeExcess[TEXT("parcharia")] = 0.35;
	Out = Evaluate(Burning, S);
	TestEqual(TEXT("excess at the neighbour: its far smoke is full"), Out.Site[0], 1.0f);
	TestEqual(TEXT("...and nothing nearer yet"), Out.Site[1] + Out.Site[2], 0.0f);
	TestEqual(TEXT("...no beacon without news"), Out.Site[3], 0.0f);
	TestEqual(TEXT("...the other side stays quiet"), Out.Site[7], 0.0f);

	FThreatMap News = Calm();
	News.NewsToVillage.Add({ TEXT("parcharia"), 0.6, 0.0 });
	Out = Evaluate(News, S);
	TestTrue(TEXT("news leaving: the farthest beacon lights first"), Out.Site[3] > 0.0f && Out.Site[4] == 0.0f);
	TestEqual(TEXT("news is not smoke"), Out.Site[0] + Out.Site[1] + Out.Site[2], 0.0f);
	News.NewsToVillage[0].Progress = 0.6;
	Out = Evaluate(News, S);
	TestTrue(TEXT("news at 60 %: three beacons of four"), Out.Site[5] > 0.0f && Out.Site[6] == 0.0f);

	FThreatMap Coming = Calm();
	Coming.PressureToVillage.Add({ TEXT("parcharia"), 0.3, 0.3 });
	Out = Evaluate(Coming, S);
	TestTrue(TEXT("raid on the road, first half: mid-way smoke"), Out.Site[1] > 0.99f && Out.Site[2] == 0.0f);
	Coming.PressureToVillage[0].Progress = 0.7;
	Out = Evaluate(Coming, S);
	TestTrue(TEXT("raid on the road, second half: the smoke comes near"), Out.Site[2] > 0.99f && Out.Site[1] == 0.0f);

	FThreatMap Arrived = Burning;
	// Un noeud lointain sans site (Paipert, a trois jours) brule plus fort que le voisin : il ne doit pas voler la source.
	Arrived.NodeExcess.Add(TEXT("paipert"), 0.75);
	Arrived.VillageExcess = 0.4;
	Out = Evaluate(Arrived, S);
	TestEqual(TEXT("exposure at the village: full dread"), Out.Dread, 1.0f);
	TestTrue(TEXT("...near smoke on the side it came from"), Out.Site[2] > 0.99f);
	TestTrue(TEXT("...its beacons are held"), Out.Site[6] > 0.99f);
	TestEqual(TEXT("...the quiet side is not lit"), Out.Site[7], 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAnastasisWorldTheatreThreatScenario, "Anastasis.WorldTheatre.ThreatNewsBeforeRaid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAnastasisWorldTheatreThreatScenario::RunTest(const FString&)
{
	using namespace AnastasisWorldTheatreThreat;
	FString Json;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Anastasis/Scenario/geo-pontos-1204.json");
	if (!TestTrue(TEXT("scenario geo-pontos-1204 readable"), FFileHelper::LoadFileToString(Json, *Path))) return false;
	AnastasisGeo::FScenario Scenario;
	TArray<FString> Errors;
	if (!TestTrue(TEXT("scenario parses"), AnastasisGeo::ParseScenario(Json, Scenario, Errors))) return false;
	AnastasisGeo::FGeoWorld Geo;
	if (!TestTrue(TEXT("scenario loads"), Geo.Load(Scenario, 1, Errors))) return false;

	// Un raid a Paipert, au-dela de Parcharia : il doit passer par la frontiere avant d'atteindre le village.
	AnastasisGeo::FShockDef Raid;
	Raid.Id = TEXT("test-raid");
	Raid.Label = TEXT("raid");
	Raid.SourceNode = TEXT("paipert");
	Raid.StartDay = 1;
	Raid.DurationDays = 4;
	Raid.Emissions = { { AnastasisGeo::EPressure::Insecurity, 0.9 }, { AnastasisGeo::EPressure::Military, 0.5 } };
	Raid.InformationMagnitude = 1.0;
	Raid.Provenance.SourceId = TEXT("test");
	Raid.Provenance.Status = AnastasisGeo::EHistoricalStatus::Abstraction;
	Raid.Provenance.Confidence = 1.0;
	if (!TestFalse(TEXT("shock accepted"), Geo.InjectShock(Raid, Errors).IsEmpty())) return false;

	int32 FirstNews = 0, FirstPressure = 0, FirstExposure = 0;
	for (int32 Day = 1; Day <= 10; ++Day)
	{
		Geo.AdvanceToDay(Day);
		FThreatMap Map;
		ReadThreatMap(Geo, Day + 0.5, Map);
		if (!FirstNews && Map.NewsToVillage.Num() > 0) FirstNews = Day;
		if (!FirstPressure && Map.PressureToVillage.Num() > 0) FirstPressure = Day;
		if (!FirstExposure && Map.VillageExcess > 0.0) FirstExposure = Day;
	}
	AddInfo(FString::Printf(TEXT("news on the road on day %d, raid on the road on day %d, felt at the village on day %d"), FirstNews, FirstPressure, FirstExposure));
	TestTrue(TEXT("the news of the raid travels toward the village"), FirstNews > 0);
	TestTrue(TEXT("the raid itself travels toward the village"), FirstPressure > 0);
	TestTrue(TEXT("the beacons come before the smoke: news at least a day ahead of the raid"), FirstNews > 0 && FirstNews < FirstPressure);
	return true;
}
#endif
