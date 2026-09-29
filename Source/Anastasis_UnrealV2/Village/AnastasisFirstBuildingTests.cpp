#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "SmartObjectComponent.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "Village/AnastasisVillageBuilding.h"
#include "Village/AnastasisVillageInteractionSubsystem.h"
#include "Village/AnastasisVillagePresentation.h"
#include "Village/AnastasisVillageTags.h"
#include "WorldView/AnastasisWorldView.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisFirstBuildingTest
{
	UWorld* FindWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (World->WorldType == EWorldType::Editor
				|| World->WorldType == EWorldType::Game
				|| World->WorldType == EWorldType::PIE))
			{
				return World;
			}
		}
		return nullptr;
	}
}

/**
 * Le batiment existe a deux niveaux, et le lien va dans un seul sens.
 *
 * Simulation : `building-N` dans FVillage, case bloquee, seuils.
 * Unreal     : AAnastasisVillageBuilding (chemin de spawn de `main`), SimId = building-N,
 *              pose au centre de sa tuile, Smart Object `Activity.Drink` enregistre.
 * Retrait    : l'enregistrement disparait -> l'acteur et son Smart Object aussi,
 *              sans reference pendante cote presentation.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisFirstBuildingPresentationTest,
	"Anastasis.Village.FirstBuilding.Presentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisFirstBuildingPresentationTest::RunTest(const FString&)
{
	UWorld* World = AnastasisFirstBuildingTest::FindWorld();
	if (!TestNotNull(TEXT("editor/game world"), World))
	{
		return false;
	}
	UAnastasisVillageInteractionSubsystem* Rooms = World->GetSubsystem<UAnastasisVillageInteractionSubsystem>();
	if (!TestNotNull(TEXT("village interaction subsystem"), Rooms))
	{
		return false;
	}

	FAnastasisSimulation Sim;
	Sim.Reset(AnastasisWorldView::ReferenceSeed, AnastasisWorldView::ReferenceWidth, AnastasisWorldView::ReferenceHeight);
	AnastasisVillage::FVillage& Village = Sim.GetVillage();

	FString WellId;
	for (int32 R = 0; R < 30 && WellId.IsEmpty(); ++R)
	{
		for (int32 DY = -R; DY <= R && WellId.IsEmpty(); ++DY)
		{
			for (int32 DX = -R; DX <= R && WellId.IsEmpty(); ++DX)
			{
				WellId = Village.AddBuilding(AnastasisVillage::WellType, 48 + DX, 48 + DY);
			}
		}
	}
	if (!TestFalse(TEXT("puits pose dans la simulation"), WellId.IsEmpty()))
	{
		return false;
	}
	const AnastasisVillage::FBuilding Record = *Village.FindBuilding(WellId);

	FAnastasisVillagePresentation Presentation;
	TestEqual(TEXT("un acteur cree"), Presentation.Sync(Village, Sim.GetWorld(), *Rooms), 1);
	TestEqual(TEXT("second Sync : rien a faire"), Presentation.Sync(Village, Sim.GetWorld(), *Rooms), 0);

	AAnastasisVillageBuilding* Actor = Presentation.FindActor(WellId);
	if (!TestNotNull(TEXT("acteur reflete"), Actor))
	{
		return false;
	}
	TestEqual(TEXT("SimId = identifiant de simulation"), Actor->GetSimId(), FName(*WellId));
	TestTrue(TEXT("type Well"), Actor->GetKind() == EAnastasisVillageBuildingKind::Well);
	const FVector Expected = FAnastasisVillagePresentation::SimToUnreal(Sim.GetWorld(), Record.X + 0.5, Record.Y + 0.5);
	TestTrue(TEXT("pose au centre de sa tuile"), Actor->GetActorLocation().Equals(Expected, 0.01));
	TestTrue(
		TEXT("XY en tuiles, jamais en uu absolus"),
		FMath::IsNearlyEqual(Actor->GetActorLocation().X, (Record.X + 0.5) * AnastasisWorldView::TileWorldSize, 0.01));
	TestTrue(
		TEXT("Smart Object enregistre"),
		Actor->GetSmartObject() && Actor->GetSmartObject()->GetRegisteredHandle().IsValid());

	const FAnastasisVillageQueryResult Found = Rooms->FindNearestInteraction(
		TAG_Anastasis_Activity_Drink,
		Expected,
		static_cast<float>(AnastasisWorldView::TileWorldSize * 3.0));
	TestTrue(TEXT("Activity.Drink trouvable pres du puits"), Found.IsValid());
	TestEqual(TEXT("la requete Unreal remonte a building-N"), Found.SimId, FName(*WellId));

	// Retrait dans la SIMULATION : la presentation suit, rien ne reste.
	TestTrue(TEXT("RemoveBuilding"), Village.RemoveBuilding(WellId));
	TestEqual(TEXT("un acteur detruit"), Presentation.Sync(Village, Sim.GetWorld(), *Rooms), 1);
	TestNull(TEXT("plus d'acteur pour cet identifiant"), Presentation.FindActor(WellId));
	TestEqual(TEXT("table de presentation vide"), Presentation.Num(), 0);
	const FAnastasisVillageQueryResult After = Rooms->FindNearestInteraction(
		TAG_Anastasis_Activity_Drink,
		Expected,
		static_cast<float>(AnastasisWorldView::TileWorldSize * 3.0));
	TestFalse(TEXT("Smart Object desenregistre"), After.IsValid() && After.SimId == FName(*WellId));

	Presentation.Clear(Rooms);
	return true;
}

#endif
