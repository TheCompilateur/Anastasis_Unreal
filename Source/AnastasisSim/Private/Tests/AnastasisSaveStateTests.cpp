#include "Misc/AutomationTest.h"

#include "Geo/AnastasisGeo.h"
#include "HAL/FileManager.h"
#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisWeatherBehavior.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"

#if WITH_DEV_AUTOMATION_TESTS

// SAVE_STATE_001 -- la sauvegarde de la simulation (chantier 4 d'IRON_CRUSADE_001).
//
// Le protocole de la breche, applique a la sauvegarde : un rechargement n'est juste que si l'etat COMPLET
// est egal (StateDigest, l'oracle de STATE_ORACLE_001, pas la projection JS) ET si le futur est egal.
//   1. AllerRetour : village vivant (chantier sec, porteur, sentiers, sol humide, ecritures que Digest()
//      ne voit pas), sauve, recharge dans une autre simulation : meme etat, memes octets resauves, puis
//      trois jours pas a pas, meme etat a chaque demi-journee.
//   2. MondeExterieur : la meme chose avec le scenario geopolitique du jeu charge, et son refus sans lui.
//   3. Refus : signature, version, fichier tronque, octets en trop, octet altere -- chaque refus rend une
//      erreur et laisse la simulation qui charge INTACTE.

namespace AnastasisSaveStateTest
{
	using namespace AnastasisVillage;

	constexpr double Dt = 1.0 / 60.0;
	constexpr int32 TicksPerDay = static_cast<int32>(FAnastasisSimulation::DayLength * 60.0);

	/** Puits, huit habitants, une maison ouverte a sec avec un porteur, sentiers et sol humide actifs. */
	void Populate(FAnastasisSimulation& Sim)
	{
		Sim.Reset(12345u, 96, 96);
		FVillage& Village = Sim.GetVillage();
		Village.SetTerrainTravelCostEnabled(true);
		Village.SetSoilWaterEnabled(true);
		Village.SetRoadEvolutionEnabled(true);
		const FPoint Centre = Village.GetSettlement();
		const int32 CX = FMath::FloorToInt32(Centre.X), CY = FMath::FloorToInt32(Centre.Y);
		Village.AddBuilding(WellType, CX, CY);
		int32 Spawned = 0;
		for (int32 R = 2; R <= 8 && Spawned < 8; ++R)
		for (int32 DY = -R; DY <= R && Spawned < 8; DY += 2)
		for (int32 DX = -R; DX <= R && Spawned < 8; DX += 2)
		{
			if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
			const double X = CX + DX + 0.5, Y = CY + DY + 0.5;
			if (Village.IsFootBlocked(X, Y)) continue;
			AnastasisNeeds::FNeeds Needs;
			Needs.Thirst = 45.0 + 4.0 * Spawned;
			Needs.Hunger = 20.0 + 3.0 * Spawned;
			Village.SpawnNpc(X, Y, Needs);
			++Spawned;
		}
		// Grenier, champs ouverts, chantier sec : la premiere case libre en spirale autour du puits.
		const auto Spiral = [CX, CY](int32 MinR, TFunctionRef<bool(int32, int32)> Try)
		{
			for (int32 R = MinR; R <= 16; ++R)
			for (int32 DY = -R; DY <= R; ++DY)
			for (int32 DX = -R; DX <= R; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != R) continue;
				if (Try(CX + DX, CY + DY)) return true;
			}
			return false;
		};
		Spiral(3, [&Village](int32 X, int32 Y) { return !Village.AddBuilding(GranaryType, X, Y).IsEmpty(); });
		int32 Fields = 0;
		Spiral(4, [&Village, &Fields](int32 X, int32 Y) { if (Village.ActivateFoodSource(X, Y)) ++Fields; return Fields >= 6; });
		Spiral(5, [&Village](int32 X, int32 Y) { return !Village.OpenSite(HouseType, X, Y, false).IsEmpty(); });
		if (Village.GetActors().Num() > 0) Village.SetMaterialCourier(Village.GetActors()[0].Id);
	}

	/** Des ecritures que `Digest()` ne voit pas (IRON_CRUSADE_001) : la sauvegarde doit les porter. */
	void WriteHiddenState(FAnastasisSimulation& Sim)
	{
		FVillage& Village = Sim.GetVillage();
		FNpc* First = Village.FindNpcMutable(Village.GetActors()[0].Id);
		First->Speed *= 1.25;
		First->Reputation += 0.2;
		First->Relations.Add(TPair<FString, double>(TEXT("npc-99"), 1.0));
		First->KnownCells.Add(9999);
		Village.SetSimRngState(Village.GetSimRngState() ^ 0x5A5Au);
		AnastasisWeatherBehavior::FSimWeather Storm;
		Storm.Rain = 0.7;
		Village.SetForcedWeather(Storm);
	}

	void Run(FAnastasisSimulation& Sim, int32 Ticks)
	{
		for (int32 I = 0; I < Ticks; ++I) Sim.Tick(Dt);
	}

	/** Le scenario geopolitique du jeu (Content/Anastasis/Scenario/geo-pontos-1204.json). */
	bool LoadGameScenario(FAutomationTestBase& Test, AnastasisGeo::FScenario& Out)
	{
		const FString Path = FPaths::ProjectContentDir() / TEXT("Anastasis/Scenario/geo-pontos-1204.json");
		FString Json;
		if (!Test.TestTrue(*FString::Printf(TEXT("scenario lu : %s"), *Path), FFileHelper::LoadFileToString(Json, *Path)))
		{
			return false;
		}
		TArray<FString> Errors;
		const bool bOk = AnastasisGeo::ParseScenario(Json, Out, Errors);
		return Test.TestTrue(*FString::Printf(TEXT("scenario valide (%s)"), *FString::Join(Errors, TEXT(" ; "))), bOk);
	}

	/**
	 * `Original` est sauve, recharge dans `Copy`, puis les deux avancent `Days` jours ; l'etat complet est
	 * compare a chaque demi-journee. Rend faux a la premiere divergence (le reste n'apprendrait rien).
	 */
	bool RoundTripAndFuture(FAutomationTestBase& Test, const TCHAR* Label, FAnastasisSimulation& Original,
		FAnastasisSimulation& Copy, const AnastasisGeo::FScenario* Scenario, int32 Days)
	{
		TArray<uint8> Bytes;
		Original.SaveState(Bytes);
		FString Error;
		if (!Test.TestTrue(*FString::Printf(TEXT("%s : rechargement accepte (%s)"), Label, *Error), Copy.LoadState(Bytes, Error, Scenario)))
		{
			Test.AddError(FString::Printf(TEXT("%s : %s"), Label, *Error));
			return false;
		}
		if (!Test.TestEqual(*FString::Printf(TEXT("%s : meme etat complet apres rechargement"), Label), Copy.StateDigest(), Original.StateDigest()))
		{
			return false;
		}
		Test.TestEqual(*FString::Printf(TEXT("%s : meme projection JS"), Label), Copy.GetVillage().Digest(), Original.GetVillage().Digest());
		TArray<uint8> Again;
		Copy.SaveState(Again);
		Test.TestTrue(*FString::Printf(TEXT("%s : la copie se resauve octet pour octet (%d octets)"), Label, Bytes.Num()), Again == Bytes);

		const uint64 Start = Original.StateDigest();
		for (int32 Half = 1; Half <= Days * 2; ++Half)
		{
			Run(Original, TicksPerDay / 2);
			Run(Copy, TicksPerDay / 2);
			if (!Test.TestEqual(*FString::Printf(TEXT("%s : meme etat a %.1f jour(s)"), Label, Half * 0.5), Copy.StateDigest(), Original.StateDigest()))
			{
				return false;
			}
		}
		Test.TestNotEqual(*FString::Printf(TEXT("%s : l'etat a bien change pendant le futur compare"), Label), Original.StateDigest(), Start);
		Test.TestTrue(*FString::Printf(TEXT("%s : le village vit encore (le futur compare n'est pas un cimetiere)"), Label),
			Original.GetVillage().GetActors().Num() > 0);
		Test.AddInfo(FString::Printf(TEXT("SAVE_STATE %s bytes=%d npcs=%d buildings=%d day=%d digest=%016llx"), Label, Bytes.Num(),
			Original.GetVillage().GetActors().Num(), Original.GetVillage().GetBuildings().Num(), Original.GetDay(), Original.StateDigest()));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSaveStateRoundTripTest,
	"Anastasis.Sim.Sauvegarde.AllerRetour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSaveStateRoundTripTest::RunTest(const FString&)
{
	using namespace AnastasisSaveStateTest;
	FAnastasisSimulation Original;
	Populate(Original);
	TestTrue(TEXT("puits et chantier poses"), Original.GetVillage().GetBuildings().Num() >= 2);
	TestTrue(TEXT("un chantier ouvert"), Original.GetVillage().ActiveSites().Num() >= 1);

	// Juste apres la mise en place : chemins demandes, file de navigation pleine, minuit pas encore passe.
	{
		FAnastasisSimulation Copy;
		Run(Original, 60 * 5);
		if (!RoundTripAndFuture(*this, TEXT("debut"), Original, Copy, nullptr, 1)) return true;
	}
	// Plus tard, apres des minuits (file de minuit, sentiers, sol humide) et des ecritures invisibles a Digest().
	{
		Run(Original, TicksPerDay + 60 * 7);
		WriteHiddenState(Original);
		FAnastasisSimulation Copy;
		RoundTripAndFuture(*this, TEXT("jour-3"), Original, Copy, nullptr, 3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSaveStateGeoTest,
	"Anastasis.Sim.Sauvegarde.MondeExterieur",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSaveStateGeoTest::RunTest(const FString&)
{
	using namespace AnastasisSaveStateTest;
	AnastasisGeo::FScenario Scenario;
	if (!LoadGameScenario(*this, Scenario)) return true;

	FAnastasisSimulation Original;
	Populate(Original);
	TArray<FString> Errors;
	if (!TestTrue(TEXT("monde exterieur charge"), Original.GetGeo().Load(Scenario, Original.GetDay(), Errors))) return true;
	Run(Original, TicksPerDay * 2 + 60 * 11);

	TArray<uint8> Bytes;
	Original.SaveState(Bytes);
	FAnastasisSimulation::FSaveHeader Header;
	FString Error;
	TestTrue(TEXT("en-tete lisible"), FAnastasisSimulation::ReadSaveHeader(Bytes, Header, Error));
	TestTrue(TEXT("l'en-tete dit qu'un scenario est requis"), Header.bGeoLoaded);
	TestEqual(TEXT("... et lequel"), Header.GeoScenarioId, Scenario.Id);

	{
		FAnastasisSimulation Copy;
		Populate(Copy);
		const uint64 Before = Copy.StateDigest();
		TestFalse(TEXT("sans scenario : refuse"), Copy.LoadState(Bytes, Error, nullptr));
		TestTrue(TEXT("... avec une raison"), Error.Contains(Scenario.Id));
		TestEqual(TEXT("... et la simulation qui chargeait est intacte"), Copy.StateDigest(), Before);
	}
	FAnastasisSimulation Copy;
	RoundTripAndFuture(*this, TEXT("monde-exterieur"), Original, Copy, &Scenario, 3);
	TestTrue(TEXT("la copie a son monde exterieur"), Copy.GetGeo().IsLoaded());
	TestEqual(TEXT("... au meme jour"), Copy.GetGeo().GetDay(), Original.GetGeo().GetDay());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSaveStateRefusalTest,
	"Anastasis.Sim.Sauvegarde.Refus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSaveStateRefusalTest::RunTest(const FString&)
{
	using namespace AnastasisSaveStateTest;
	FAnastasisSimulation Original;
	Populate(Original);
	Run(Original, 60 * 30);
	TArray<uint8> Good;
	Original.SaveState(Good);

	struct FCase
	{
		const TCHAR* Name;
		TFunction<void(TArray<uint8>&)> Spoil;
		const TCHAR* Expect;
	};
	const TArray<FCase> Cases = {
		{ TEXT("signature"), [](TArray<uint8>& B) { B[0] = 'X'; }, TEXT("ANSV") },
		{ TEXT("vide"), [](TArray<uint8>& B) { B.Reset(); }, TEXT("ANSV") },
		{ TEXT("version"), [](TArray<uint8>& B)
			{
				// La version est le premier nombre de l'en-tete : 4 (ANSV) + 1 (objet) + 1+4+7 (cle « version ») + 2 (marqueur, taille).
				const int32 At = 4 + 1 + 1 + 4 + 7 + 2;
				B[At] = static_cast<uint8>(FAnastasisSimulation::SaveFormatVersion + 1);
			}, TEXT("format") },
		{ TEXT("tronque"), [](TArray<uint8>& B) { B.SetNum(B.Num() * 2 / 3); }, TEXT("fin de fichier") },
		{ TEXT("octets en trop"), [](TArray<uint8>& B) { B.Add(0); B.Add(0); }, TEXT("en trop") },
		{ TEXT("cle alteree"), [](TArray<uint8>& B)
			{
				// La premiere cle du corps (« bootDeferred ») : une lettre changee, le format n'est plus le parcours.
				static const char Needle[] = "bootDeferred";
				for (int32 I = 0; I + 12 <= B.Num(); ++I)
				{
					if (FMemory::Memcmp(B.GetData() + I, Needle, 12) == 0) { B[I] = 'B'; return; }
				}
			}, TEXT("bootDeferred") },
	};

	for (const FCase& Case : Cases)
	{
		TArray<uint8> Bad = Good;
		Case.Spoil(Bad);
		FAnastasisSimulation Target;
		Populate(Target);
		Run(Target, 60);
		const uint64 Before = Target.StateDigest();
		FString Error;
		const bool bLoaded = Target.LoadState(Bad, Error);
		TestFalse(*FString::Printf(TEXT("%s : refuse"), Case.Name), bLoaded);
		TestTrue(*FString::Printf(TEXT("%s : l'erreur le dit (« %s »)"), Case.Name, *Error), Error.Contains(Case.Expect));
		TestEqual(*FString::Printf(TEXT("%s : la simulation qui chargeait est intacte"), Case.Name), Target.StateDigest(), Before);
		AddInfo(FString::Printf(TEXT("SAVE_STATE_REFUS %s : %s"), Case.Name, *Error));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSaveStateBiographyTest,
	"Anastasis.Sim.Sauvegarde.Biographie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// save-history-001 (ecart n°46) : la biographie des batiments est de l'etat. Fondee avant la sauvegarde, elle
// revient entiere ; le meme changement de mains apres le rechargement s'ecrit pareil dans les deux parties.
bool FAnastasisSaveStateBiographyTest::RunTest(const FString&)
{
	using namespace AnastasisSaveStateTest;
	FAnastasisSimulation Original;
	Populate(Original);
	FVillage& Village = Original.GetVillage();
	Village.SetBiographyEnabled(true);
	const FPoint Centre = Village.GetSettlement();
	FString House;
	for (int32 R = 4; R <= 14 && House.IsEmpty(); ++R)
	{
		House = Village.AddBuilding(HouseType, FMath::FloorToInt32(Centre.X) - R, FMath::FloorToInt32(Centre.Y) - 2, 1.0, 1);
	}
	if (!TestFalse(TEXT("maison achevee posee"), House.IsEmpty())) return true;
	const FString Founder = Village.GetActors()[1].Id;
	const FString Heir = Village.GetActors()[2].Id;
	TestTrue(TEXT("le fondateur prend la maison"), Village.AssignHome(Founder, House));
	Run(Original, 60 * 10);
	const FBuildingBiography* Bio = Village.FindBiography(House);
	if (!TestNotNull(TEXT("biographie observee par la simulation"), Bio)) return true;
	TestTrue(TEXT("forme fixee par son fondateur"), Bio->bFormFixed && Bio->Founder == Founder);

	TArray<uint8> Bytes;
	Original.SaveState(Bytes);
	FAnastasisSimulation Copy;
	FString Error;
	if (!TestTrue(*FString::Printf(TEXT("rechargement (%s)"), *Error), Copy.LoadState(Bytes, Error))) return true;
	const FBuildingBiography* Loaded = Copy.GetVillage().FindBiography(House);
	if (!TestNotNull(TEXT("la biographie revient"), Loaded)) return true;
	TestEqual(TEXT("... meme fondateur"), Loaded->Founder, Founder);
	TestEqual(TEXT("... memes evenements"), Loaded->Events.Num(), Bio->Events.Num());
	TestTrue(TEXT("... toujours activee"), Copy.GetVillage().IsBiographyEnabled());
	TestEqual(TEXT("meme etat complet"), Copy.StateDigest(), Original.StateDigest());

	// Le meme changement de mains dans les deux parties : la meme histoire s'ecrit.
	for (FAnastasisSimulation* Sim : { &Original, &Copy })
	{
		Sim->GetVillage().AssignHome(Heir, House);
		Run(*Sim, 60 * 10);
	}
	const FBuildingBiography* A = Original.GetVillage().FindBiography(House);
	const FBuildingBiography* B = Copy.GetVillage().FindBiography(House);
	TestTrue(TEXT("changement de mains ecrit"), A->OwnerChanges == 1 && A->Owner == Heir && A->Founder == Founder);
	TestEqual(TEXT("... pareil dans la partie rechargee"), B->OwnerChanges, A->OwnerChanges);
	TestEqual(TEXT("... memes evenements"), B->Events.Num(), A->Events.Num());
	TestEqual(TEXT("meme etat complet apres"), Copy.StateDigest(), Original.StateDigest());
	AddInfo(FString::Printf(TEXT("SAVE_HISTORY house=%s founder=%s heir=%s events=%d owner_changes=%d"),
		*House, *Founder, *Heir, A->Events.Num(), A->OwnerChanges));
	return true;
}

#endif
