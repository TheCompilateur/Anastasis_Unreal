// memoire-decisions-001 -- la memoire episodique (ai/episodes.js, ecart n°47) et la maison de famille batie a
// plusieurs (ecart n°48) : deformation et legende, temoins, recit, oubli, poids sur les buts, decider de batir,
// demander de l'aide, le refus retenu, et l'aide rendue.

#include "Misc/AutomationTest.h"
#include "Life/AnastasisEpisodes.h"
#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace AnastasisEpisodesTest
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

	E::FEpisode Lived(const TCHAR* Kind, double Weight, double Tone, double Detail = 0.0)
	{
		E::FEpisode Event;
		Event.Id = TEXT("npc-0-e1");
		Event.RootId = Event.Id;
		Event.Kind = Kind;
		Event.Weight = Weight;
		Event.Tone = Tone;
		Event.Detail = Detail;
		Event.AboutName = TEXT("Theodoros");
		return Event;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpisodesRetellTest, "Anastasis.Sim.Episodes.Deformation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEpisodesRetellTest::RunTest(const FString&)
{
	using namespace AnastasisEpisodesTest;
	// Un conteur craintif (qui n'explore jamais, moral au plus bas) : 1,45 + 50/90 depasse 1,9, le plafond.
	const E::FBias Fearful = E::StorytellerBias(0.0, 0.0, 50.0);
	TestEqual(TEXT("peur au plafond"), Fearful.Fear, 1.9);
	int32 Draws = 0;
	auto Rng = [&Draws]() { ++Draws; return 0.25; };
	E::FEpisode Event = Lived(TEXT("robbed"), 32.0, -1.0, 4.0);
	const E::FEpisode Once = E::Retell(Event, Fearful, Rng);
	TestEqual(TEXT("une bouche de plus"), Once.Hops, 1);
	TestFalse(TEXT("plus vecu"), Once.bFirsthand);
	TestEqual(TEXT("toujours une perte : x0,92 au plus"), Once.Weight, 32.0 * 0.92);
	TestEqual(TEXT("le chiffre enfle : round(4 x 1,9)"), Once.Detail, 8.0);
	TestEqual(TEXT("une bouche : aucun tirage"), Draws, 0);
	TestEqual(TEXT("meme racine"), Once.RootId, Event.Id);
	const E::FEpisode Twice = E::Retell(Once, Fearful, Rng);
	TestEqual(TEXT("deux bouches : le nom (1) puis le lieu (2)"), Draws, 3);
	TestTrue(TEXT("le nom se perd (tirage 0,25 < 0,5)"), Twice.AboutName.IsEmpty());
	TestEqual(TEXT("le lieu derive : floor(0,25 x 5) - 2"), Twice.X, -1);
	const E::FEpisode Thrice = E::Retell(Twice, Fearful, Rng);
	TestTrue(TEXT("trois bouches : une legende"), E::IsLegend(Thrice));
	TestFalse(TEXT("deux bouches : pas encore"), E::IsLegend(Twice));
	// Variante de texte : FNV-1a de l'identifiant, stable.
	TestEqual(TEXT("variante stable"), E::VariantIndex(Event, 5), E::VariantIndex(Event, 5));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpisodesRecordTest, "Anastasis.Sim.Episodes.Temoins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEpisodesRecordTest::RunTest(const FString&)
{
	using namespace AnastasisEpisodesTest;
	const AnastasisWorld::FWorld W = MakeWorld();
	FVillage V;
	V.Bind(W);
	const FString Subject = V.SpawnNpc(20.5, 20.5, AnastasisNeeds::FNeeds());
	TArray<FString> Near;
	for (int32 I = 0; I < 6; ++I) Near.Add(V.SpawnNpc(21.5 + I * 0.5, 20.5, AnastasisNeeds::FNeeds()));
	const FString Child = V.SpawnNpc(20.5, 21.5, AnastasisNeeds::FNeeds());
	V.SetIdentity(Child, TEXT("Euphrosyne"), FString(), TEXT("female"), 9.0);
	const FString Far = V.SpawnNpc(40.5, 40.5, AnastasisNeeds::FNeeds());

	const uint64 Before = V.StateDigest();
	FVillage::FEpisodeOptions Options;
	Options.AboutId = Subject;
	Options.RootId = TEXT("death-test");
	TestEqual(TEXT("quatre temoins au plus"), V.RecordWitnesses(Subject, TEXT("bereaved"), Options, 7.0), 4);
	TestTrue(TEXT("un enfant ne temoigne pas"), V.FindNpc(Child)->Chronicle.Events.IsEmpty());
	TestTrue(TEXT("trop loin pour voir"), V.FindNpc(Far)->Chronicle.Events.IsEmpty());
	const E::FEpisode& Seen = V.FindNpc(Near[0])->Chronicle.Events[0];
	TestEqual(TEXT("temoin : 26 x 0,55"), Seen.Weight, 26.0 * 0.55);
	TestTrue(TEXT("vecu de ses yeux"), Seen.bFirsthand);
	TestTrue(TEXT("on ne vit pas deux fois le meme fait"), V.RecordEpisode(Near[0], TEXT("bereaved"), Options).IsEmpty());
	TestTrue(TEXT("un type inconnu ne s'enregistre pas"), V.RecordEpisode(Near[0], TEXT("dragon"), FVillage::FEpisodeOptions()).IsEmpty());
	TestNotEqual(TEXT("la memoire est de l'etat"), V.StateDigest(), Before);

	// La capacite : huit souvenirs, les plus lourds (le vecu a une avance de 6).
	for (int32 I = 0; I < 12; ++I)
	{
		FVillage::FEpisodeOptions More;
		More.RootId = FString::Printf(TEXT("r%d"), I);
		More.Weight = 5.0 + I;
		V.RecordEpisode(Far, TEXT("raised"), More);
	}
	TestEqual(TEXT("huit souvenirs au plus"), V.FindNpc(Far)->Chronicle.Events.Num(), E::Constants::Capacity);
	TestEqual(TEXT("le plus lourd en tete"), V.FindNpc(Far)->Chronicle.Events[0].Weight, 16.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpisodesShareTest, "Anastasis.Sim.Episodes.Recit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEpisodesShareTest::RunTest(const FString&)
{
	using namespace AnastasisEpisodesTest;
	const AnastasisWorld::FWorld W = MakeWorld();
	FVillage V;
	V.Bind(W);
	const FString Teller = V.SpawnNpc(20.5, 20.5, AnastasisNeeds::FNeeds());
	const FString Listener = V.SpawnNpc(21.5, 20.5, AnastasisNeeds::FNeeds());
	FVillage::FEpisodeOptions Fire;
	Fire.Note = TEXT("la mere d'Eudokia");
	const FString Id = V.RecordEpisode(Teller, TEXT("leftBehind"), Fire);
	TestFalse(TEXT("souvenir vecu"), Id.IsEmpty());
	// De vive voix (le feu) : une bouche, aucun tirage.
	TestTrue(TEXT("il l'entend"), V.TellEpisode(Teller, Listener, Id));
	TestFalse(TEXT("on ne reraconte pas ce qu'il sait deja"), V.TellEpisode(Teller, Listener, Id));
	const E::FEpisode& Heard = V.FindNpc(Listener)->Chronicle.Events[0];
	TestEqual(TEXT("une bouche"), Heard.Hops, 1);
	TestEqual(TEXT("de la bouche du conteur"), Heard.SourceId, Teller);
	TestEqual(TEXT("qui l'a vecu"), Heard.OriginalSourceId, Teller);
	TestEqual(TEXT("confiance d'un temoin direct"), Heard.Confidence, 0.95);
	TestEqual(TEXT("la note suit"), Heard.Note, FString(TEXT("la mere d'Eudokia")));
	TestEqual(TEXT("raconte"), V.FindNpc(Teller)->Chronicle.Told, 1);
	TestEqual(TEXT("entendu"), V.FindNpc(Listener)->Chronicle.Heard, 1);

	// L'oubli : -0,7 par jour ; un souvenir sous le poids 4 ou de plus de trente jours s'en va.
	const double W0 = V.FindNpc(Teller)->Chronicle.Events[0].Weight;
	V.FadeEpisodesDaily(1);
	TestEqual(TEXT("le temps use"), V.FindNpc(Teller)->Chronicle.Events[0].Weight, W0 - 0.7);
	V.FadeEpisodesDaily(40);
	TestTrue(TEXT("trente jours : oublie"), V.FindNpc(Teller)->Chronicle.Events.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpisodesBiasTest, "Anastasis.Sim.Episodes.Poids",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEpisodesBiasTest::RunTest(const FString&)
{
	using namespace AnastasisEpisodesTest;
	E::FChronicle Chronicle;
	TestEqual(TEXT("sans souvenir : rien"), E::GoalBias(Chronicle, TEXT("build")), 0.0);
	E::FEpisode Raised = Lived(TEXT("raised"), 15.0, 1.0);
	Chronicle.Events.Add(Raised);
	TestEqual(TEXT("raised -> build : 15 x 0,9 x 0,2"), E::GoalBias(Chronicle, TEXT("build")), 15.0 * 0.9 * 0.2);
	E::FEpisode Grief = Lived(TEXT("bereaved"), 26.0, -1.0);
	Grief.Id = TEXT("g");
	Grief.AboutId = TEXT("npc-7");
	Grief.bFirsthand = false;
	Chronicle.Events.Add(Grief);
	TestEqual(TEXT("un on-dit pese 0,45"), E::GoalBias(Chronicle, TEXT("rest")), 26.0 * 0.45 * 0.45 * 0.2);
	TestEqual(TEXT("ressenti envers quelqu'un"), E::Feeling(Chronicle, TEXT("npc-7")), -26.0 * 0.45);
	TestNotNull(TEXT("la raison d'un ressenti"), E::StrongestAbout(Chronicle, TEXT("npc-7")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFamilyHouseHelpTest, "Anastasis.Sim.Episodes.MaisonAPlusieurs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFamilyHouseHelpTest::RunTest(const FString&)
{
	using namespace AnastasisEpisodesTest;
	const AnastasisWorld::FWorld W = MakeWorld();
	FVillage V;
	V.Bind(W);
	TestFalse(TEXT("le puits"), V.AddBuilding(WellType, 20, 20, 1.0, 1).IsEmpty());
	// Deux familles sans maison, et une voisine.
	const FString A = V.SpawnNpc(16.5, 16.5, AnastasisNeeds::FNeeds());
	const FString A2 = V.SpawnNpc(16.5, 17.5, AnastasisNeeds::FNeeds());
	const FString B = V.SpawnNpc(26.5, 26.5, AnastasisNeeds::FNeeds());
	const FString Weak = V.SpawnNpc(24.5, 16.5, AnastasisNeeds::FNeeds());
	const FString FamA = V.AddFamily(TEXT("la famille de A"));
	const FString FamB = V.AddFamily(TEXT("la famille de B"));
	V.JoinFamily(A, FamA, true, TEXT("chef"));
	V.JoinFamily(A2, FamA, true, TEXT("epouse"));
	V.JoinFamily(B, FamB, true, TEXT("chef"));
	V.FindNpcMutable(Weak)->Needs.Health = 20.0;

	// Le premier soir : une seule famille decide (une par soir).
	V.UpdateFamilyHousesDaily();
	const FBuilding* SiteA = nullptr;
	int32 Sites = 0;
	for (const FBuilding& Building : V.GetBuildings())
	{
		if (Building.OwnerFamilyId.IsEmpty()) continue;
		++Sites;
		if (Building.OwnerFamilyId == FamA) SiteA = &Building;
	}
	TestEqual(TEXT("une maison decidee ce soir"), Sites, 1);
	if (!TestNotNull(TEXT("la famille de A batit"), SiteA)) return false;
	TestTrue(TEXT("les siens y travaillent"), FVillage::CanBuildAt(*SiteA, A2));
	TestFalse(TEXT("les autres, pas sans avoir dit oui"), FVillage::CanBuildAt(*SiteA, B));
	TestTrue(TEXT("on ne demande pas le premier soir"), V.GetHelpLog().IsEmpty());

	// Le lendemain : A demande ; B batit aussi maintenant (refus : son propre toit), la voisine est faible.
	V.UpdateFamilyHousesDaily();
	TArray<FVillage::FHelpAnswer> Log = V.GetHelpLog();
	TestEqual(TEXT("deux demandes par jour"), Log.Num(), 2);
	const FVillage::FHelpAnswer* ToWeak = Log.FindByPredicate([&Weak](const FVillage::FHelpAnswer& H) { return H.ToId == Weak; });
	if (TestNotNull(TEXT("A demande a la voisine"), ToWeak))
	{
		TestFalse(TEXT("la voisine refuse"), ToWeak->bAccepted);
		TestEqual(TEXT("parce qu'elle est trop faible"), ToWeak->Reason, FString(TEXT("faible")));
	}
	// Le refus se retient.
	bool bRemembers = false;
	for (const E::FEpisode& Event : V.FindNpc(A)->Chronicle.Events)
	{
		if (Event.Kind == TEXT("refusedHelp") && Event.AboutId == Weak) bRemembers = true;
	}
	TestTrue(TEXT("A se souvient du refus"), bRemembers);

	// Le souvenir decide : si B se souvient que A l'a aide, B accepte, et le dit.
	const FBuilding* Site = V.FindBuilding(SiteA->Id);
	FVillage::FEpisodeOptions Debt;
	Debt.AboutId = A;
	Debt.RootId = TEXT("aide-ancienne");
	V.RecordEpisode(B, TEXT("helped"), Debt);
	const FVillage::FHelpAnswer Owed = V.EvaluateHelp(*V.FindNpc(A), *V.FindNpc(B), *Site);
	TestTrue(TEXT("tu m'as aide : oui"), Owed.bAccepted);
	TestEqual(TEXT("raison : la dette rendue"), Owed.Reason, FString(TEXT("dette_rendue")));
	FVillage::FEpisodeOptions Grudge;
	Grudge.AboutId = A;
	Grudge.RootId = TEXT("refus-ancien");
	Grudge.Weight = 80.0;
	V.RecordEpisode(B, TEXT("refusedHelp"), Grudge);
	const FVillage::FHelpAnswer Back = V.EvaluateHelp(*V.FindNpc(A), *V.FindNpc(B), *Site);
	TestFalse(TEXT("tu m'as refuse : non"), Back.bAccepted);
	return true;
}

#endif
