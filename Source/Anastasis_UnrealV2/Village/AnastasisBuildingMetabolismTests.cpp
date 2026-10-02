#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisBuildingMetabolism.h"
#include "Village/AnastasisVillage.h"
#include "Village/AnastasisVillageBuilding.h"
#include "Village/AnastasisVillageInteractionSubsystem.h"
#include "Village/AnastasisVillagePresentation.h"
#include "WorldView/AnastasisWorldView.h"

#if WITH_DEV_AUTOMATION_TESTS

using AnastasisMetabolism::EMode;
using AnastasisMetabolism::EOccupancy;
using AnastasisMetabolism::FInput;

namespace
{
	FInput House(const bool bCompleted, const int32 Residents, const int32 Inside, const double Daylight)
	{
		FInput In;
		In.bDwelling = true;
		In.bCompleted = bCompleted;
		In.Residents = Residents;
		In.Inside = Inside;
		In.Daylight = Daylight;
		return In;
	}
}

/** Ce que la simulation sait d'une maison, rangé en cinq états, sans repli. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisMetabolismOccupancyTest,
	"Anastasis.Village.Metabolism.Occupancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisMetabolismOccupancyTest::RunTest(const FString&)
{
	TestTrue(TEXT("chantier"), AnastasisMetabolism::OccupancyOf(House(false, 3, 2, 0.0)) == EOccupancy::Site);
	TestTrue(TEXT("vide"), AnastasisMetabolism::OccupancyOf(House(true, 0, 0, 0.0)) == EOccupancy::Vacant);
	TestTrue(TEXT("foyer sorti"), AnastasisMetabolism::OccupancyOf(House(true, 2, 0, 0.0)) == EOccupancy::Resident);
	TestTrue(TEXT("habitee"), AnastasisMetabolism::OccupancyOf(House(true, 2, 1, 0.0)) == EOccupancy::Inhabited);
	TestTrue(TEXT("abri sans foyer attribue mais quelqu'un dedans"),
		AnastasisMetabolism::OccupancyOf(House(true, 0, 1, 0.0)) == EOccupancy::Inhabited);
	FInput Well;
	Well.bCompleted = true;
	Well.Residents = 5;
	Well.Inside = 5;
	TestTrue(TEXT("un puits n'est pas un logement"), AnastasisMetabolism::OccupancyOf(Well) == EOccupancy::NotADwelling);
	return true;
}

/**
 * L'invariant du GHOST SETTLEMENT : pour une maison que la simulation dit vide, la lumiere est nulle
 * a TOUTE heure en mode Truth. Le temoin faux (WrongWitness) l'allume : c'est ce qui le rend faux.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisMetabolismGhostTest,
	"Anastasis.Village.Metabolism.GhostSettlement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisMetabolismGhostTest::RunTest(const FString&)
{
	double MaxVacantTruth = 0.0;
	double MaxVacantLie = 0.0;
	double MaxInhabited = 0.0;
	double MinInhabitedAtNight = 1.0;
	for (int32 I = 0; I <= 100; ++I)
	{
		const double Daylight = I / 100.0;
		MaxVacantTruth = FMath::Max(MaxVacantTruth, AnastasisMetabolism::Derive(House(true, 0, 0, Daylight), EMode::Truth).Hearth);
		MaxVacantLie = FMath::Max(MaxVacantLie, AnastasisMetabolism::Derive(House(true, 0, 0, Daylight), EMode::WrongWitness).Hearth);
		MaxInhabited = FMath::Max(MaxInhabited, AnastasisMetabolism::Derive(House(true, 1, 1, Daylight), EMode::Truth).Hearth);
	}
	MinInhabitedAtNight = AnastasisMetabolism::Derive(House(true, 1, 1, 0.0), EMode::Truth).Hearth;
	TestEqual(TEXT("maison vide : jamais de lumiere (Truth)"), MaxVacantTruth, 0.0);
	TestTrue(TEXT("le temoin faux allume la meme maison vide"), MaxVacantLie > 0.99);
	TestTrue(TEXT("maison habitee : allumee en pleine nuit"), MinInhabitedAtNight > 0.99);
	TestTrue(TEXT("maison habitee : un foyer ne s'allume pas en plein jour"),
		AnastasisMetabolism::Derive(House(true, 1, 1, 1.0), EMode::Truth).Hearth < 1.0e-9);
	TestTrue(TEXT("la lumiere ne croit qu'avec l'obscurite"),
		AnastasisMetabolism::Derive(House(true, 1, 1, 0.7), EMode::Truth).Hearth
			< AnastasisMetabolism::Derive(House(true, 1, 1, 0.3), EMode::Truth).Hearth);
	TestEqual(TEXT("foyer sorti : braises, un quart"),
		AnastasisMetabolism::Derive(House(true, 2, 0, 0.0), EMode::Truth).Hearth, AnastasisMetabolism::ResidentHearth);
	TestEqual(TEXT("chantier : rien"), AnastasisMetabolism::Derive(House(false, 4, 4, 0.0), EMode::Truth).Hearth, 0.0);
	TestEqual(TEXT("Off : l'ancien rendu"), AnastasisMetabolism::Derive(House(true, 4, 4, 0.0), EMode::Off).Hearth, 0.0);
	TestTrue(TEXT("mode hors table borne"), AnastasisMetabolism::ModeFromInt(99) == EMode::WrongWitness
		&& AnastasisMetabolism::ModeFromInt(-3) == EMode::Off);
	return true;
}

/**
 * De bout en bout, simulation -> acteur : un village habite allume sa maison la nuit ; son habitant
 * disparu (la mort de la reference, ici RemoveNpc), le batiment reste debout et s'eteint.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisMetabolismProjectionTest,
	"Anastasis.Village.Metabolism.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisMetabolismProjectionTest::RunTest(const FString&)
{
	UWorld* World = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* Candidate = Context.World();
			if (Candidate && (Candidate->WorldType == EWorldType::Editor
				|| Candidate->WorldType == EWorldType::Game || Candidate->WorldType == EWorldType::PIE))
			{
				World = Candidate;
				break;
			}
		}
	}
	UAnastasisVillageInteractionSubsystem* Rooms = World ? World->GetSubsystem<UAnastasisVillageInteractionSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("village interaction subsystem"), Rooms))
	{
		return false;
	}

	FAnastasisSimulation Sim;
	Sim.Reset(AnastasisWorldView::ReferenceSeed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
	AnastasisVillage::FVillage& Village = Sim.GetVillage();
	FString HouseId;
	for (int32 R = 0; R < 30 && HouseId.IsEmpty(); ++R)
	{
		for (int32 DY = -R; DY <= R && HouseId.IsEmpty(); ++DY)
		{
			for (int32 DX = -R; DX <= R && HouseId.IsEmpty(); ++DX)
			{
				HouseId = Village.AddBuilding(AnastasisVillage::HouseType, 40 + DX, 40 + DY);
			}
		}
	}
	if (!TestFalse(TEXT("maison posee"), HouseId.IsEmpty()))
	{
		return false;
	}
	const FString Npc = Village.SpawnNpc(40.5, 43.5, AnastasisNeeds::FNeeds());
	TestTrue(TEXT("foyer attribue"), Village.AssignHome(Npc, HouseId));

	FAnastasisVillagePresentation Presentation;
	const double Night = 0.0;
	Presentation.Sync(Village, Sim.GetWorld(), *Rooms, Night, EMode::Truth);
	AAnastasisVillageBuilding* Actor = Presentation.FindActor(HouseId);
	if (!TestNotNull(TEXT("acteur reflete"), Actor))
	{
		return false;
	}
	TestEqual(TEXT("habitant vivant, hors de la maison : braises"), Actor->GetHearth(), AnastasisMetabolism::ResidentHearth);

	Presentation.Sync(Village, Sim.GetWorld(), *Rooms, 1.0, EMode::Truth);
	TestEqual(TEXT("plein jour : eteinte"), Actor->GetHearth(), 0.0);

	TestTrue(TEXT("l'habitant disparait"), Village.RemoveNpc(Npc));
	Presentation.Sync(Village, Sim.GetWorld(), *Rooms, Night, EMode::Truth);
	TestNotNull(TEXT("le batiment reste debout"), Presentation.FindActor(HouseId));
	TestEqual(TEXT("village mort, maison debout : noire la nuit"), Actor->GetHearth(), 0.0);

	Presentation.Sync(Village, Sim.GetWorld(), *Rooms, Night, EMode::WrongWitness);
	TestTrue(TEXT("temoin faux : la meme maison vide est allumee"), Actor->GetHearth() > 0.99);

	Presentation.Clear(Rooms);
	return true;
}

/**
 * ABANDON_001 : l'usure suit les jours de vacance (ancrage : bandes de la reference 6 / 18 / 45 j),
 * jamais l'heure, jamais l'occupation. Une maison habitee ne vieillit pas ; le temoin faux reste propre.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisMetabolismNeglectTest,
	"Anastasis.Village.Metabolism.Neglect",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisMetabolismNeglectTest::RunTest(const FString&)
{
	TestEqual(TEXT("jour 0"), AnastasisMetabolism::NeglectForDays(0), 0.0);
	TestEqual(TEXT("6 j : vide"), AnastasisMetabolism::NeglectForDays(6), 0.2);
	TestEqual(TEXT("18 j : use"), AnastasisMetabolism::NeglectForDays(18), 0.55);
	TestEqual(TEXT("45 j : long abandon"), AnastasisMetabolism::NeglectForDays(45), 1.0);
	TestEqual(TEXT("au-dela, plafonne"), AnastasisMetabolism::NeglectForDays(900), 1.0);
	TestEqual(TEXT("negatif borne"), AnastasisMetabolism::NeglectForDays(-4), 0.0);
	double Previous = -1.0;
	bool bMonotonic = true;
	for (int32 D = 0; D <= 60; ++D)
	{
		const double N = AnastasisMetabolism::NeglectForDays(D);
		bMonotonic &= N >= Previous;
		Previous = N;
	}
	TestTrue(TEXT("monotone en jours vides"), bMonotonic);

	FInput Empty = House(true, 0, 0, 1.0);
	Empty.VacantDays = 45;
	TestEqual(TEXT("maison vide depuis 45 j (Truth)"), AnastasisMetabolism::Derive(Empty, EMode::Truth).Neglect, 1.0);
	TestEqual(TEXT("meme maison, temoin faux : propre"), AnastasisMetabolism::Derive(Empty, EMode::WrongWitness).Neglect, 0.0);
	TestEqual(TEXT("meme maison, Off : ancien rendu"), AnastasisMetabolism::Derive(Empty, EMode::Off).Neglect, 0.0);
	FInput Lived = House(true, 1, 0, 1.0);
	Lived.VacantDays = 45;
	TestEqual(TEXT("un foyer vivant ne s'use pas, quoi que dise l'horloge"), AnastasisMetabolism::Derive(Lived, EMode::Truth).Neglect, 0.0);
	FInput Site = House(false, 0, 0, 1.0);
	Site.VacantDays = 45;
	TestEqual(TEXT("un chantier ne s'use pas"), AnastasisMetabolism::Derive(Site, EMode::Truth).Neglect, 0.0);
	return true;
}

#endif
