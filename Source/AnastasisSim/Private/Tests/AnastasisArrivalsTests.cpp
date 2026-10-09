// arrivants-001 (ecart n°49) -- un groupe arrive par la route ; le conseil du soir l'accueille ou le renvoie,
// et ce qu'on a fait la derniere fois pese sur la fois suivante.

#include "Misc/AutomationTest.h"
#include "Life/AnastasisEpisodes.h"
#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisArrivalsTest
{
	using namespace AnastasisVillage;
	namespace E = AnastasisEpisodes;

	AnastasisWorld::FWorld MakeWorld(int32 Size = 48)
	{
		AnastasisWorld::FWorld W;
		W.W = Size;
		W.H = Size;
		W.Tiles.SetNum(Size * Size);
		for (int32 I = 0; I < W.Tiles.Num(); ++I)
		{
			W.Tiles[I].X = I % Size;
			W.Tiles[I].Y = I / Size;
			W.Tiles[I].Alt = 0.5;
		}
		return W;
	}

	/** Deux familles accueillies, chacune avec son chef ; un grenier vide. */
	struct FValley
	{
		AnastasisWorld::FWorld W = MakeWorld();
		FVillage V;
		FString ChefA;
		FString ChefB;
		FString Granary;

		FValley()
		{
			V.Bind(W);
			V.SetSettlement(20.5, 20.5);
			V.AddBuilding(WellType, 20, 20, 1.0, 1);
			Granary = V.AddBuilding(GranaryType, 23, 20, 1.0, 1);
			ChefA = V.SpawnNpc(18.5, 18.5, AnastasisNeeds::FNeeds());
			ChefB = V.SpawnNpc(22.5, 18.5, AnastasisNeeds::FNeeds());
			const FString FamA = V.AddFamily(TEXT("la famille de A"));
			const FString FamB = V.AddFamily(TEXT("la famille de B"));
			V.JoinFamily(ChefA, FamA, true, TEXT("chef"));
			V.JoinFamily(ChefB, FamB, true, TEXT("chef"));
			FVillage::FArrivalGroup First;
			First.FamilyName = TEXT("la famille de Theophilos");
			First.Members = { { TEXT("Theophilos"), TEXT("male"), 46.0, TEXT("chef"), true }, { TEXT("Anna"), TEXT("female"), 40.0, TEXT("epouse"), true },
				{ TEXT("Kyranna"), TEXT("female"), 8.0, TEXT("fille"), false } };
			FVillage::FArrivalGroup Second;
			Second.FamilyName = TEXT("les bergers de Lazaros");
			Second.Members = { { TEXT("Lazaros"), TEXT("male"), 33.0, TEXT("chef"), true }, { TEXT("Gregorios"), TEXT("male"), 29.0, TEXT("frere"), true } };
			V.SetArrivalPool({ First, Second });
		}

		/** Le lendemain : la simulation avance d'un jour (sans pas de vie). */
		int32 Days = 0;
		void NextDay()
		{
			++Days;
			V.UpdateActors(static_cast<double>(Days) * AnastasisRhythm::DayLength + 1.0, 0.0);
		}

		bool Remembers(const FString& NpcId, const TCHAR* Kind) const
		{
			const FNpc* Npc = V.FindNpc(NpcId);
			return Npc && Npc->Chronicle.Events.ContainsByPredicate([Kind](const E::FEpisode& Event) { return Event.Kind == Kind; });
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArrivalsWithoutFamiliesTest, "Anastasis.Sim.Arrivants.SansFoyer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArrivalsWithoutFamiliesTest::RunTest(const FString&)
{
	using namespace AnastasisArrivalsTest;
	// Sans foyer (le harnais, la partie d'avant) : des habitants ordinaires, ni famille, ni conseil.
	const AnastasisWorld::FWorld W = MakeWorld();
	FVillage V;
	V.Bind(W);
	V.SetSettlement(20.5, 20.5);
	const TArray<FString> Ids = V.AdmitExternalArrivals(3, 4.0, 0.0, TEXT("le pillage des hameaux"), TEXT("Paipert"), 1);
	TestEqual(TEXT("trois arrivants"), Ids.Num(), 3);
	TestTrue(TEXT("aucun foyer"), V.GetFamilies().IsEmpty());
	V.UpdateArrivalCouncilDaily(0.0, 2);
	TestTrue(TEXT("aucun conseil"), V.GetCouncilLog().IsEmpty());
	for (const FString& Id : Ids)
	{
		const bool bFled = V.FindNpc(Id)->Chronicle.Events.ContainsByPredicate([](const AnastasisEpisodes::FEpisode& Event) { return Event.Kind == TEXT("fled"); });
		TestFalse(TEXT("aucun souvenir de fuite"), bFled);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArrivalsCouncilTest, "Anastasis.Sim.Arrivants.Conseil",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FArrivalsCouncilTest::RunTest(const FString&)
{
	using namespace AnastasisArrivalsTest;
	FValley Valley;
	FVillage& V = Valley.V;

	// Un groupe arrive : il devient une famille en attente, avec ses noms et ce qu'il a fui.
	const TArray<FString> First = V.AdmitExternalArrivals(3, 4.0, 0.0, TEXT("le pillage des hameaux hauts de Paipert"), TEXT("Paipert"), 1);
	if (!TestEqual(TEXT("trois arrivants"), First.Num(), 3)) return false;
	const FNpc* Theophilos = V.FindNpc(First[0]);
	TestEqual(TEXT("le premier est Theophilos"), Theophilos->Name, FString(TEXT("Theophilos")));
	const FVillage::FFamily* Guests = V.FindFamily(Theophilos->FamilyId);
	if (!TestNotNull(TEXT("une famille"), Guests)) return false;
	TestTrue(TEXT("en attente du conseil"), Guests->bGuest);
	TestEqual(TEXT("deux adultes, une enfant"), Guests->Adults.Num() * 10 + Guests->Dependents.Num(), 21);
	TestTrue(TEXT("le chef se souvient de ce qu'il a fui"), Valley.Remembers(First[0], TEXT("fled")));
	TestFalse(TEXT("l'enfant ne raconte pas la fuite"), Valley.Remembers(First[2], TEXT("fled")));

	// Le soir meme, pas de conseil : ils viennent d'arriver.
	V.UpdateArrivalCouncilDaily(0.0, 1);
	TestTrue(TEXT("pas de conseil le jour de l'arrivee"), V.GetCouncilLog().IsEmpty());

	// Le lendemain, grenier vide : les deux chefs disent non, pour le grain ; le groupe repart.
	Valley.NextDay();
	V.UpdateArrivalCouncilDaily(0.0, 2);
	if (!TestEqual(TEXT("un conseil"), V.GetCouncilLog().Num(), 1)) return false;
	const FVillage::FCouncil& Refused = V.GetCouncilLog()[0];
	TestFalse(TEXT("refuse"), Refused.bAccepted);
	TestEqual(TEXT("deux voix"), Refused.Votes.Num(), 2);
	for (const FVillage::FWelcomeVote& Vote : Refused.Votes)
	{
		TestFalse(TEXT("non"), Vote.bYes);
		TestEqual(TEXT("pour le grain"), Vote.Reason, FString(TEXT("grenier")));
	}
	for (const FString& Id : First) TestNull(TEXT("reparti"), V.FindNpc(Id));
	TestTrue(TEXT("la famille est partie"), V.FindFamily(Refused.FamilyId)->bLeft);
	TestTrue(TEXT("le chef A se souvient d'avoir dit non"), Valley.Remembers(Valley.ChefA, TEXT("hostingRefusal")));

	// Un second groupe, le grenier plein : le remords et le grain l'accueillent.
	V.CreditFood(Valley.Granary, 400);
	const TArray<FString> Second = V.AdmitExternalArrivals(2, 4.0, 1.0, TEXT("les bergers chasses des hauts paturages"), TEXT("Cheriana"), 2);
	if (!TestEqual(TEXT("deux arrivants"), Second.Num(), 2)) return false;
	TestEqual(TEXT("le deuxieme groupe prend le deuxieme nom"), V.FindNpc(Second[0])->Name, FString(TEXT("Lazaros")));
	Valley.NextDay();
	V.UpdateArrivalCouncilDaily(0.0, 3);
	if (!TestEqual(TEXT("deux conseils"), V.GetCouncilLog().Num(), 2)) return false;
	const FVillage::FCouncil& Welcomed = V.GetCouncilLog()[1];
	TestTrue(TEXT("accueilli"), Welcomed.bAccepted);
	for (const FVillage::FWelcomeVote& Vote : Welcomed.Votes)
	{
		TestTrue(TEXT("oui"), Vote.bYes);
		TestEqual(TEXT("par remords"), Vote.Reason, FString(TEXT("remords")));
	}
	const FVillage::FFamily* Shepherds = V.FindFamily(Welcomed.FamilyId);
	TestFalse(TEXT("plus en attente"), Shepherds->bGuest);
	TestTrue(TEXT("le chef A se souvient d'avoir accueilli"), Valley.Remembers(Valley.ChefA, TEXT("hosting")));

	// Accueillis, ils batissent comme les autres : leur parcelle, le soir meme ou suivant.
	V.UpdateFamilyHousesDaily();
	V.UpdateFamilyHousesDaily();
	V.UpdateFamilyHousesDaily();
	bool bTheirSite = false;
	for (const FBuilding& Building : V.GetBuildings()) bTheirSite |= Building.OwnerFamilyId == Welcomed.FamilyId;
	TestTrue(TEXT("les accueillis tracent leur maison"), bTheirSite);

	// Le souvenir d'un exil fait dire oui : un chef qui a fui sa maison.
	FVillage::FEpisodeOptions Exile;
	Exile.Note = TEXT("la prise de la Ville");
	V.RecordEpisode(Valley.ChefB, TEXT("fall"), Exile);
	FVillage::FFamily Strangers;
	Strangers.Adults = { TEXT("npc-x") };
	const FVillage::FWelcomeVote Exiled = V.EvaluateWelcome(*V.FindNpc(Valley.ChefB), Strangers, 0.0);
	TestTrue(TEXT("nous aussi : oui"), Exiled.bYes);

	// La peur du dehors fait dire non.
	const FVillage::FWelcomeVote Afraid = V.EvaluateWelcome(*V.FindNpc(Valley.ChefB), Strangers, 1.0);
	TestFalse(TEXT("avec ce qui se passe dehors : non"), Afraid.bYes);
	TestEqual(TEXT("raison : la peur"), Afraid.Reason, FString(TEXT("peur")));
	return true;
}

#endif
