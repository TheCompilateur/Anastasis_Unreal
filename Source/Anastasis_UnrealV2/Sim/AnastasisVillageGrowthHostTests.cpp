// valmire-grows-001 (ecart n°50) -- Valmire continue de pousser : le vrai village du lancement (les quatre
// familles et le moine, l'ouverture), trente jours sans rendu, croissance allumee puis eteinte (temoin).
// Les deux chroniques sont ecrites dans Saved/Chronicle/ pour etre lues ; le verdict, c'est Alexandre qui
// le lit. Le test ne retient que des faits : chantiers ouverts par les habitants, acheves, et pas plus de
// morts que sans croissance.

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sim/AnastasisSimulationSubsystem.h"
#include "Sim/AnastasisTimeWarp.h"
#include "Sim/AnastasisVillageChronicle.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisVillageGrowthHostTest
{
	constexpr uint32 kSeed = 12345u;

	struct FScratchHost
	{
		UWorld* World = nullptr;
		UAnastasisSimulationSubsystem* Host = nullptr;

		FScratchHost()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (World)
			{
				FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
				Context.SetCurrentWorld(World);
				Host = World->GetSubsystem<UAnastasisSimulationSubsystem>();
			}
		}

		~FScratchHost()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	struct FResult
	{
		bool bSeeded = false;
		int32 SitesOpened = 0;
		int32 OpenedDone = 0;
		int32 Deaths = 0;
		int32 Npcs = 0;
		int32 Buildings = 0;
		int32 Houses = 0;
		FString Path;
		TArray<FString> Sites;
	};

	/** Le village du lancement, `Days` jours par tranches de quatre heures, sa chronique ecrite sous `Name`. */
	FResult Play(bool bGrowth, int32 Days, const TCHAR* Name)
	{
		FResult R;
		IConsoleVariable* Growth = IConsoleManager::Get().FindConsoleVariable(TEXT("anastasis.Village.Growth"));
		const int32 Before = Growth ? Growth->GetInt() : 1;
		if (Growth) Growth->Set(bGrowth ? 1 : 0, ECVF_SetByCode);
		{
			FScratchHost Scratch;
			if (Scratch.Host)
			{
				Scratch.Host->ResetCanonical(kSeed);
				const AnastasisVillage::FPoint Settlement = Scratch.Host->GetSimulation().GetVillage().GetSettlement();
				R.bSeeded = !Scratch.Host->SeedStartVillage(12, FMath::FloorToInt32(Settlement.X), FMath::FloorToInt32(Settlement.Y)).IsEmpty();
			}
			if (R.bSeeded)
			{
				FAnastasisSimulation& Sim = Scratch.Host->GetSimulation();
				AnastasisChronicle::FVillageChronicle Chronicle = Scratch.Host->GetChronicle();
				Chronicle.Observe(Sim);
				const double Chunk = FAnastasisSimulation::DayLength / 6.0;
				for (int32 I = 0; I < Days * 6; ++I)
				{
					AnastasisTimeWarp::Advance(Sim, Chunk);
					Chronicle.Observe(Sim);
				}
				const AnastasisVillage::FVillage& V = Sim.GetVillage();
				R.SitesOpened = V.GetGrowthSitesOpened();
				R.Deaths = V.GetDeaths().Num();
				R.Npcs = V.GetActors().Num();
				R.Buildings = V.GetBuildings().Num();
				for (const AnastasisVillage::FBuilding& B : V.GetBuildings())
				{
					if (B.Type == AnastasisVillage::HouseType && B.Progress >= 1.0) ++R.Houses;
					if (B.OpenedById.IsEmpty()) continue;
					if (B.Progress >= 1.0) ++R.OpenedDone;
					const AnastasisBuild::FSiteMaterials& M = B.Materials;
					R.Sites.Add(FString::Printf(TEXT("%s %s par %s jour %d (%s) %.0f%% bois %d/%d+%d pierre %d/%d+%d"), *B.Id, *B.Type, *B.OpenedById, B.CreatedDay,
						*B.OpenCause, B.Progress * 100.0, M.ConsumedWood, M.NeedWood, M.StockWood, M.ConsumedStone, M.NeedStone, M.StockStone));
				}
				if (const AnastasisVillage::FNpc* Courier = V.FindNpc(V.GetMaterialCourierId()))
				{
					R.Sites.Add(FString::Printf(TEXT("courier %s goal=%s activity=%s carry=%d resource=%d source=%d retry_at=%.1f now=%.1f delivered=%d critical_hunger=%.0f thirst=%.0f energy=%.0f"),
						*Courier->Id, *Courier->Goal, *Courier->Activity, Courier->MaterialCarry, static_cast<int32>(Courier->MaterialResource),
						Courier->MaterialSourceIndex, Courier->MaterialRetryAt, Sim.GetTime(), Courier->MaterialsDelivered,
						Courier->Needs.Hunger, Courier->Needs.Thirst, Courier->Needs.Energy));
					for (const AnastasisVillage::FBuilding& B : V.GetBuildings())
					{
						if (B.Progress < 1.0) R.Sites.Add(FString::Printf(TEXT("site %s doors=%d x=%.0f y=%.0f"), *B.Id, B.AccessPoints.Num(), B.X, B.Y));
					}
				}
				else
				{
					R.Sites.Add(TEXT("courier aucun"));
				}
				R.Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Chronicle"), Name));
				FFileHelper::SaveStringToFile(Chronicle.Render(), *R.Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			}
		}
		if (Growth) Growth->Set(Before, ECVF_SetByCode);
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGrowthValmireTest,
	"Anastasis.Village.Croissance.Valmire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGrowthValmireTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGrowthHostTest;
	const FResult Grown = Play(true, 30, TEXT("croissance-30-jours.txt"));
	const FResult Witness = Play(false, 30, TEXT("croissance-temoin-30-jours.txt"));
	if (!TestTrue(TEXT("village du lancement pose (croissance)"), Grown.bSeeded) || !TestTrue(TEXT("... et temoin"), Witness.bSeeded)) return false;
	for (const FString& Site : Grown.Sites) AddInfo(TEXT("VALMIRE_GROWS site ") + Site);
	AddInfo(FString::Printf(TEXT("VALMIRE_GROWS growth sites_opened=%d opened_done=%d deaths=%d npcs=%d buildings=%d houses=%d chronicle=%s"),
		Grown.SitesOpened, Grown.OpenedDone, Grown.Deaths, Grown.Npcs, Grown.Buildings, Grown.Houses, *Grown.Path));
	AddInfo(FString::Printf(TEXT("VALMIRE_GROWS witness sites_opened=%d opened_done=%d deaths=%d npcs=%d buildings=%d houses=%d chronicle=%s"),
		Witness.SitesOpened, Witness.OpenedDone, Witness.Deaths, Witness.Npcs, Witness.Buildings, Witness.Houses, *Witness.Path));

	TestEqual(TEXT("temoin : personne n'ouvre de chantier de lui-meme"), Witness.SitesOpened, 0);
	TestTrue(TEXT("les habitants ouvrent d'eux-memes des chantiers"), Grown.SitesOpened >= 2);
	TestTrue(TEXT("au moins un de ces chantiers s'acheve en trente jours"), Grown.OpenedDone >= 1);
	TestTrue(TEXT("pas plus de morts que le temoin"), Grown.Deaths <= Witness.Deaths);
	TestTrue(TEXT("le village a plus de batiments que le temoin"), Grown.Buildings > Witness.Buildings);
	return true;
}

#endif
