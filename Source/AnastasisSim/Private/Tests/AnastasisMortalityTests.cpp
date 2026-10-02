// MORTALITY_001 -- `causeOfDeath` (branche sante epuisee) et `updateMortalityDaily` reduit (ecart n°28).
// Les libelles attendus sont ceux de life/mortality.js lignes 70-76 (lus, pas executes :
// `causeOfDeath` n'est pas exporte et `removeActor` tire dans une trentaine de modules).

#include "Misc/AutomationTest.h"
#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisMortalityTest
{
	using namespace AnastasisVillage;

	FNpc Dying(const double Health, const double Thirst, const double Hunger, const double Energy)
	{
		FNpc Npc;
		Npc.Needs.Health = Health;
		Npc.Needs.Thirst = Thirst;
		Npc.Needs.Hunger = Hunger;
		Npc.Needs.Energy = Energy;
		return Npc;
	}

	AnastasisWorld::FWorld MakeWorld()
	{
		AnastasisWorld::FWorld W;
		W.W = 32;
		W.H = 32;
		W.Tiles.SetNum(1024);
		for (int32 I = 0; I < W.Tiles.Num(); ++I)
		{
			W.Tiles[I].X = I % 32;
			W.Tiles[I].Y = I / 32;
			W.Tiles[I].Alt = 0.5;
		}
		return W;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMortalityCauseTest, "Anastasis.Sim.Mortality.Cause",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMortalityCauseTest::RunTest(const FString&)
{
	using namespace AnastasisMortalityTest;
	TestTrue(TEXT("sante 1 : vit"), FVillage::CauseOfDeath(Dying(1.0, 99, 99, 0)).IsEmpty());
	TestTrue(TEXT("sante 0.001 : vit"), FVillage::CauseOfDeath(Dying(0.001, 99, 99, 0)).IsEmpty());
	TestEqual(TEXT("soif d'abord"), FVillage::CauseOfDeath(Dying(0.0, 88, 99, 0)), FString(TEXT("de soif")));
	TestEqual(TEXT("puis faim"), FVillage::CauseOfDeath(Dying(0.0, 87.9, 88, 0)), FString(TEXT("de faim")));
	TestEqual(TEXT("puis epuisement (energie <= 20)"), FVillage::CauseOfDeath(Dying(0.0, 10, 87.9, 20)), FString(TEXT("d'epuisement")));
	TestEqual(TEXT("sinon faiblesse"), FVillage::CauseOfDeath(Dying(0.0, 10, 50, 20.1)), FString(TEXT("de faiblesse")));
	TestEqual(TEXT("sante negative : meme chose"), FVillage::CauseOfDeath(Dying(-3.0, 10, 50, 60)), FString(TEXT("de faiblesse")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMortalityDailyTest, "Anastasis.Sim.Mortality.Daily",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMortalityDailyTest::RunTest(const FString&)
{
	using namespace AnastasisMortalityTest;
	const AnastasisWorld::FWorld W = MakeWorld();
	FVillage V;
	V.Bind(W);
	const FString House = V.AddBuilding(HouseType, 10, 10, 1.0, 1);
	const FString A = V.SpawnNpc(12.5, 12.5, AnastasisNeeds::FNeeds());
	const FString B = V.SpawnNpc(14.5, 12.5, AnastasisNeeds::FNeeds());
	const FString C = V.SpawnNpc(16.5, 12.5, AnastasisNeeds::FNeeds());
	TestTrue(TEXT("foyer de A"), V.AssignHome(A, House));
	for (const FString& Id : {A, B, C})
	{
		FNpc* Npc = V.FindNpcMutable(Id);
		Npc->Relations.Add(TPair<FString, double>(A, 10.0));
		Npc->Relations.Add(TPair<FString, double>(B, 4.0));
	}

	const uint64 Before = V.Digest();
	TestEqual(TEXT("personne ne meurt en bonne sante"), V.UpdateMortalityDaily(), 0);
	TestEqual(TEXT("l'empreinte ne bouge pas"), V.Digest(), Before);

	V.FindNpcMutable(A)->Needs.Health = 0.0;
	V.FindNpcMutable(A)->Needs.Thirst = 95.0;
	TestEqual(TEXT("un mort"), V.UpdateMortalityDaily(), 1);
	TestNull(TEXT("A retire"), V.FindNpc(A));
	TestNotNull(TEXT("B reste"), V.FindNpc(B));
	TestNotNull(TEXT("C reste"), V.FindNpc(C));
	TestEqual(TEXT("une mort au registre"), V.GetDeaths().Num(), 1);
	if (V.GetDeaths().Num() == 1)
	{
		TestEqual(TEXT("qui"), V.GetDeaths()[0].NpcId, A);
		TestEqual(TEXT("de quoi"), V.GetDeaths()[0].Cause, FString(TEXT("de soif")));
	}
	TestTrue(TEXT("sa maison est libre"), V.FindBuilding(House)->Owner.IsEmpty());
	TestEqual(TEXT("et date de ce jour"), V.FindBuilding(House)->VacantSinceDay, V.GetDeaths().Num() == 1 ? V.GetDeaths()[0].Day : -2);
	for (const FNpc& Npc : V.GetActors())
	{
		for (const TPair<FString, double>& Row : Npc.Relations)
		{
			TestFalse(TEXT("plus de relation avec le mort"), Row.Key == A);
		}
	}
	TestEqual(TEXT("le lendemain, plus personne ne meurt"), V.UpdateMortalityDaily(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMortalityJobTest, "Anastasis.Sim.Mortality.DayJob",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMortalityJobTest::RunTest(const FString&)
{
	TestEqual(TEXT("lifeDaily : dixieme travail de la file de minuit"), FAnastasisSimulation::DayJobLifeDaily, 10);
	TestTrue(TEXT("dans la file"), FAnastasisSimulation::DayJobLifeDaily < FAnastasisSimulation::DayDeferredJobCount);
	return true;
}
#endif
