// ABANDON_001 -- `stampHouseVacant`, `stampHouseOccupied`, `vacantAgeDays`, `vacantAgeBand`
// (src/sim/collectivePriorities.js). Les valeurs attendues sont CELLES DU JS : elles viennent de
// l'execution des vraies fonctions de la reference, pas d'un calcul a la main.
// Regeneration : un script node qui importe collectivePriorities.js et imprime
// {owner, type, vacantSinceDay, createdDay, day} -> {days, band} (voir fiche abandon-001).

#include "Misc/AutomationTest.h"
#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace AnastasisVillage;

namespace AnastasisVacancyTest
{

	struct FCase
	{
		const TCHAR* Type;
		const TCHAR* Owner;
		int32 VacantSince; // -1 = null
		int32 Created;
		int32 Day;
		int32 Days;
		int32 Band;
	};

	// Sortie du JS reference, ligne a ligne.
	const FCase Cases[] = {
		{TEXT("house"), TEXT(""), 10, 2, 10, 0, 0},
		{TEXT("house"), TEXT(""), 10, 2, 15, 5, 0},
		{TEXT("house"), TEXT(""), 10, 2, 16, 6, 1},
		{TEXT("house"), TEXT(""), 10, 2, 27, 17, 1},
		{TEXT("house"), TEXT(""), 10, 2, 28, 18, 2},
		{TEXT("house"), TEXT(""), 10, 2, 54, 44, 2},
		{TEXT("house"), TEXT(""), 10, 2, 55, 45, 3},
		{TEXT("house"), TEXT(""), 10, 2, 300, 290, 3},
		{TEXT("house"), TEXT(""), -1, 4, 30, 26, 2},
		{TEXT("house"), TEXT(""), -1, 0, 30, 0, 0},
		{TEXT("house"), TEXT("npc-1"), 3, 2, 90, 0, 0},
		{TEXT("granary"), TEXT(""), 3, 2, 90, 0, 0},
		{TEXT("house"), TEXT(""), 50, 2, 40, 0, 0},
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVacancyAgeParityTest, "Anastasis.Sim.Vacancy.AgeParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVacancyAgeParityTest::RunTest(const FString&)
{
	using namespace AnastasisVacancyTest;
	for (int32 I = 0; I < UE_ARRAY_COUNT(Cases); ++I)
	{
		const FCase& C = Cases[I];
		FBuilding B;
		B.Type = C.Type;
		B.Owner = C.Owner;
		B.VacantSinceDay = C.VacantSince;
		B.CreatedDay = C.Created;
		TestEqual(FString::Printf(TEXT("cas %d vacantAgeDays"), I), VacantAgeDays(B, C.Day), C.Days);
		TestEqual(FString::Printf(TEXT("cas %d vacantAgeBand"), I), VacantAgeBand(B, C.Day), C.Band);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVacancyStampParityTest, "Anastasis.Sim.Vacancy.StampParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVacancyStampParityTest::RunTest(const FString&)
{
	// JS : stampHouseVacant(b, 7) -> 7 ; (b, 9) -> reste 7 ; proprietaire -> null ; stampHouseOccupied -> null.
	FBuilding B;
	B.Type = HouseType;
	StampHouseVacant(B, 7);
	TestEqual(TEXT("premier tampon"), B.VacantSinceDay, 7);
	StampHouseVacant(B, 9);
	TestEqual(TEXT("le second ne reecrit pas"), B.VacantSinceDay, 7);
	B.Owner = TEXT("npc-2");
	StampHouseVacant(B, 11);
	TestEqual(TEXT("maison possedee : null"), B.VacantSinceDay, -1);
	B.Owner.Reset();
	B.VacantSinceDay = 5;
	StampHouseOccupied(B);
	TestEqual(TEXT("occupee : null"), B.VacantSinceDay, -1);
	FBuilding Granary;
	Granary.Type = GranaryType;
	StampHouseVacant(Granary, 3);
	TestEqual(TEXT("un grenier n'a pas de vacance"), Granary.VacantSinceDay, -1);
	return true;
}

// Les trois endroits ou la simulation fait changer une maison de main.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVacancyVillageTest, "Anastasis.Sim.Vacancy.VillageHooks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVacancyVillageTest::RunTest(const FString&)
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
	FVillage V;
	V.Bind(W);
	const FString House = V.AddBuilding(HouseType, 10, 10, 1.0, 4);
	TestFalse(TEXT("maison posee"), House.IsEmpty());
	TestEqual(TEXT("addBuilding : une maison sans proprietaire date de sa pose"), V.FindBuilding(House)->VacantSinceDay, 4);

	const FString Npc = V.SpawnNpc(12.5, 12.5, AnastasisNeeds::FNeeds());
	TestTrue(TEXT("foyer attribue"), V.AssignHome(Npc, House));
	TestEqual(TEXT("assignHome : occupee, horloge effacee"), V.FindBuilding(House)->VacantSinceDay, -1);

	TestTrue(TEXT("l'habitant disparait"), V.RemoveNpc(Npc));
	TestEqual(TEXT("releaseHome : libre depuis aujourd'hui (jour 1 a l'instant 0)"), V.FindBuilding(House)->VacantSinceDay, 1);
	TestEqual(TEXT("age zero le jour meme"), VacantAgeDays(*V.FindBuilding(House), 1), 0);
	TestEqual(TEXT("45 jours plus tard : long abandon"), VacantAgeBand(*V.FindBuilding(House), 46), 3);

	const FString Well = V.AddBuilding(WellType, 14, 14, 1.0, 4);
	TestEqual(TEXT("un puits n'a pas de vacance"), V.FindBuilding(Well)->VacantSinceDay, -1);
	return true;
}
#endif
