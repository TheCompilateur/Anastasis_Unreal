#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisSettlementLedger.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisBuild.h"
#include "Work/AnastasisGather.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// SETTLEMENT_MORPHOGENESIS_001 : la forme d'une maison vient de son histoire, pas d'une graine.
// Intervention : on change ce que la simulation sait du foyer (metier, taille, mort, nouveau proprietaire)
// et on observe la biographie. Falsificateurs : deux foyers differents -> meme forme ; une forme qui
// change quand le proprietaire change ; une forme fixee sans foyer.

namespace AnastasisSettlementTest
{
	using namespace AnastasisVillage;
	using AnastasisArchitecture::EVariant;

	AnastasisWorld::FWorld Meadow()
	{
		AnastasisWorld::FWorld World;
		World.W = 30;
		World.H = 30;
		World.Tiles.SetNum(World.W * World.H);
		for (int32 I = 0; I < World.Tiles.Num(); ++I)
		{
			World.Tiles[I].X = I % World.W;
			World.Tiles[I].Y = I / World.W;
			World.Tiles[I].Type = AnastasisWorld::ETileType::Grass;
			World.Tiles[I].Alt = 0.5;
		}
		return World;
	}

	AnastasisNeeds::FNeeds Calm()
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 10.0;
		N.Energy = 90.0;
		N.Thirst = 5.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		N.Social = 80.0;
		N.Leisure = 80.0;
		N.Hygiene = 80.0;
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSettlementProgramTest,
	"Anastasis.Village.Settlement.Programme",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSettlementProgramTest::RunTest(const FString&)
{
	using namespace AnastasisSettlementTest;
	using AnastasisSettlement::ProgramFor;
	struct FCase { const TCHAR* Type; int32 Phase; const TCHAR* Job; int32 Household; EVariant Expected; const TCHAR* Why; };
	const FCase Cases[] = {
		{ TEXT("house"), 1, TEXT(""), 0, EVariant::HousePoor, TEXT("sans foyer : abri provisoire") },
		{ TEXT("house"), 1, AnastasisGather::JobFarmer, 1, EVariant::HouseFarm, TEXT("cultivateur : grange et cour") },
		{ TEXT("house"), 1, AnastasisBuild::JobBuilder, 1, EVariant::HouseMedium, TEXT("batisseur : rez maconne") },
		{ TEXT("house"), 1, AnastasisGather::JobSettler, 4, EVariant::HouseMedium, TEXT("foyer de 4 : deux niveaux") },
		{ TEXT("house"), 1, AnastasisGather::JobSettler, 2, EVariant::HousePoor, TEXT("foyer de 2 : une piece") },
		{ TEXT("house"), 6, AnastasisGather::JobSettler, 1, EVariant::HouseFarm, TEXT("phase 6 : la phase l'emporte") },
		{ TEXT("well"), 1, TEXT(""), 0, EVariant::Well, TEXT("puits") },
		{ TEXT("granary"), 1, TEXT(""), 0, EVariant::Storehouse, TEXT("grenier") },
	};
	for (const FCase& C : Cases)
	{
		EVariant Got;
		FString Cause;
		const bool bOk = ProgramFor(C.Type, C.Phase, C.Job, C.Household, Got, Cause);
		TestTrue(FString(C.Why) + TEXT(" : programme connu"), bOk);
		TestEqual(FString(C.Why), static_cast<int32>(Got), static_cast<int32>(C.Expected));
		TestFalse(FString(C.Why) + TEXT(" : la cause est dite"), Cause.IsEmpty());
		EVariant Again;
		FString Cause2;
		ProgramFor(C.Type, C.Phase, C.Job, C.Household, Again, Cause2);
		TestTrue(FString(C.Why) + TEXT(" : sans tirage, la meme reponse"), Again == Got && Cause2 == Cause);
	}
	EVariant Unused;
	FString UnusedCause;
	TestFalse(TEXT("type sans forme"), ProgramFor(TEXT("tavern"), 1, TEXT(""), 0, Unused, UnusedCause));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSettlementBiographyTest,
	"Anastasis.Village.Settlement.Biographie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSettlementBiographyTest::RunTest(const FString&)
{
	using namespace AnastasisSettlementTest;
	AnastasisWorld::FWorld World = Meadow();
	FVillage Village;
	Village.Bind(World);
	AnastasisSettlement::FLedger Ledger;

	const FString Farm = Village.AddBuilding(HouseType, 8, 8, 1.0, 1);
	const FString Hut = Village.AddBuilding(HouseType, 16, 8, 1.0, 1);
	const FString Refuge = Village.AddBuilding(HouseType, 22, 16, 1.0, 1);
	if (!TestFalse(TEXT("trois maisons posees"), Farm.IsEmpty() || Hut.IsEmpty() || Refuge.IsEmpty())) return false;

	// Jour 1 : personne. La forme reste provisoire.
	Ledger.Observe(Village, 1);
	const AnastasisSettlement::FBiography* B = Ledger.Find(Farm);
	if (!TestNotNull(TEXT("biographie ouverte"), B)) return false;
	TestFalse(TEXT("sans foyer, la forme n'est pas fixee"), B->bProgramFixed);
	TestEqual(TEXT("abri provisoire"), static_cast<int32>(B->Program), static_cast<int32>(EVariant::HousePoor));

	// Jour 2 : un cultivateur prend la premiere, un sans-metier la seconde.
	const FString Farmer = Village.SpawnNpc(9.5, 10.5, Calm());
	const FString Settler = Village.SpawnNpc(17.5, 10.5, Calm());
	TestTrue(TEXT("metier de cultivateur"), Village.SetJob(Farmer, AnastasisGather::JobFarmer));
	TestTrue(TEXT("maison du cultivateur"), Village.AssignHome(Farmer, Farm));
	TestTrue(TEXT("maison du sans-metier"), Village.AssignHome(Settler, Hut));
	Ledger.Observe(Village, 2);
	B = Ledger.Find(Farm);
	const AnastasisSettlement::FBiography* H = Ledger.Find(Hut);
	TestTrue(TEXT("fondee au jour 2"), B->bProgramFixed && B->FoundedDay == 2);
	TestEqual(TEXT("fondateur"), B->Founder, Farmer);
	TestEqual(TEXT("le cultivateur fonde une ferme"), static_cast<int32>(B->Program), static_cast<int32>(EVariant::HouseFarm));
	TestEqual(TEXT("le sans-metier seul fonde une piece"), static_cast<int32>(H->Program), static_cast<int32>(EVariant::HousePoor));
	TestTrue(TEXT("deux foyers differents, deux formes"), B->Program != H->Program);

	// Jour 3 : le cultivateur meurt. La maison se vide mais garde la forme de son fondateur.
	TestTrue(TEXT("le fondateur disparait"), Village.RemoveNpc(Farmer));
	Ledger.Observe(Village, 3);
	B = Ledger.Find(Farm);
	TestTrue(TEXT("evenement : proprietaire perdu"), B->Events.ContainsByPredicate([](const AnastasisSettlement::FEvent& E)
	{
		return E.Kind == AnastasisSettlement::EEvent::OwnerLost && E.Day == 3;
	}));
	TestEqual(TEXT("les murs restent ceux du cultivateur"), static_cast<int32>(B->Program), static_cast<int32>(EVariant::HouseFarm));

	// Jour 5 : un batisseur la reprend. Il herite des murs ; la forme ne bouge pas.
	const FString Builder = Village.SpawnNpc(9.5, 10.5, Calm());
	Village.SetJob(Builder, AnastasisBuild::JobBuilder);
	TestTrue(TEXT("reprise par un batisseur"), Village.AssignHome(Builder, Farm));
	Ledger.Observe(Village, 5);
	B = Ledger.Find(Farm);
	TestEqual(TEXT("un changement de mains"), B->OwnerChanges, 1);
	TestEqual(TEXT("proprietaire courant"), B->Owner, Builder);
	TestEqual(TEXT("fondateur inchange"), B->Founder, Farmer);
	TestEqual(TEXT("la forme est une histoire, pas l'etat du jour"), static_cast<int32>(B->Program), static_cast<int32>(EVariant::HouseFarm));
	TestEqual(TEXT("age depuis l'achevement"), B->AgeDays(31), 30);

	// Jours 6-7 : trois sans-toit. Une maison prise n'accueille pas d'etranger (`findOpenShelter`, fidele) :
	// ils dorment au refuge sans maitre, qui se remplit. Sa forme reste provisoire (personne ne l'a fondee).
	for (int32 K = 0; K < 3; ++K) Village.SpawnNpc(21.5 + K, 18.5, Calm());
	Village.AssignSheltersDaily();
	Ledger.Observe(Village, 6);
	Ledger.Observe(Village, 6);
	Ledger.Observe(Village, 7);
	const AnastasisSettlement::FBiography* R = Ledger.Find(Refuge);
	AddInfo(FString::Printf(TEXT("refuge : %d dormeurs (pic %d), %d nuits pleines, %d evenements ; maison du cultivateur : %d dormeur(s)"),
		R->Occupants, R->PeakOccupants, R->CrowdedDays, R->Events.Num(), Ledger.Find(Farm)->Occupants));
	TestEqual(TEXT("une maison prise n'abrite que son foyer (sans familles portees : 1)"), Ledger.Find(Farm)->Occupants, 1);
	TestTrue(TEXT("le refuge s'est rempli"), R->PeakOccupants >= 2);
	TestEqual(TEXT("les nuits pleines sont comptees une fois par jour"), R->CrowdedDays, 2);
	TestFalse(TEXT("un refuge sans foyer garde une forme provisoire"), R->bProgramFixed);
	TestTrue(TEXT("evenement : pression d'agrandissement"), R->Events.ContainsByPredicate([](const AnastasisSettlement::FEvent& E)
	{
		return E.Kind == AnastasisSettlement::EEvent::Crowded;
	}));
	return true;
}

#endif
