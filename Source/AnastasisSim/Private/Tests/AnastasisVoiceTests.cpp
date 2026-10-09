// voix-conseil-001 (ecart n°54) -- le joueur vote au conseil (une demi-voix tant qu'il n'est pas eprouve), on lui
// demande de l'aide (son silence est un refus), et quand il demande a son tour, on lui repond selon ce qu'on dit de lui.

#include "Misc/AutomationTest.h"
#include "Life/AnastasisEpisodes.h"
#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisVoiceTest
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

	/** Deux familles fondatrices, un grenier, et le joueur arrive. */
	struct FHamlet
	{
		AnastasisWorld::FWorld W = MakeWorld();
		FVillage V;
		FString ChefA;
		FString ChefB;
		FString Player;
		FString Granary;
		int32 Days = 0;

		FHamlet()
		{
			V.Bind(W);
			V.SetSettlement(20.5, 20.5);
			V.AddBuilding(WellType, 20, 20, 1.0, 1);
			Granary = V.AddBuilding(GranaryType, 23, 20, 1.0, 1);
			ChefA = V.SpawnNpc(18.5, 18.5, AnastasisNeeds::FNeeds());
			ChefB = V.SpawnNpc(22.5, 18.5, AnastasisNeeds::FNeeds());
			V.JoinFamily(ChefA, V.AddFamily(TEXT("la famille de A")), true, TEXT("chef"));
			V.JoinFamily(ChefB, V.AddFamily(TEXT("la famille de B")), true, TEXT("chef"));
			Player = V.ArriveAsPlayer(21.5, 23.5);
		}

		void NextDay()
		{
			++Days;
			V.UpdateActors(static_cast<double>(Days) * AnastasisRhythm::DayLength + 1.0, 0.0);
		}

		bool Remembers(const FString& NpcId, const TCHAR* Kind, const FString& AboutId, bool bFirsthand) const
		{
			const FNpc* Npc = V.FindNpc(NpcId);
			return Npc && Npc->Chronicle.Events.ContainsByPredicate([&](const E::FEpisode& Event)
			{
				return Event.Kind == Kind && Event.AboutId == AboutId && Event.bFirsthand == bFirsthand;
			});
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceHalfVoteTest, "Anastasis.Sim.Voix.DemiVoix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoiceHalfVoteTest::RunTest(const FString&)
{
	using namespace AnastasisVoiceTest;
	FHamlet H;
	if (!TestFalse(TEXT("le joueur arrive"), H.Player.IsEmpty())) return false;
	TestTrue(TEXT("un nouveau pas encore eprouve"), H.V.IsUnproven(*H.V.FindNpc(H.Player)));
	TestFalse(TEXT("un fondateur l'est"), H.V.IsUnproven(*H.V.FindNpc(H.ChefA)));
	TestTrue(TEXT("pas de groupe, pas de vote"), H.V.CastPlayerVote(false).IsEmpty());

	// Un groupe arrive, le grenier est plein, les chefs ont un toit : ils diront oui ; le joueur dit non.
	H.V.CreditFood(H.Granary, 400);
	const FString House = H.V.AddBuilding(HouseType, 16, 22, 1.0, 1);
	H.V.FindNpcMutable(H.ChefA)->HomeId = House;
	H.V.FindNpcMutable(H.ChefB)->HomeId = House;
	const TArray<FString> Guests = H.V.AdmitExternalArrivals(2, 5.0, 0.0, TEXT("le pillage des hameaux"), TEXT("Paipert"), 1);
	if (!TestEqual(TEXT("deux arrivants"), Guests.Num(), 2)) return false;
	const FString GuestFamily = H.V.FindNpc(Guests[0])->FamilyId;
	TestEqual(TEXT("la voix va au groupe en attente"), H.V.CastPlayerVote(false), GuestFamily);
	H.NextDay();
	H.V.UpdateArrivalCouncilDaily(0.0, 2);
	if (!TestEqual(TEXT("un conseil"), H.V.GetCouncilLog().Num(), 1)) return false;
	const FVillage::FCouncil& Council = H.V.GetCouncilLog()[0];
	const FVillage::FWelcomeVote* Voice = Council.Votes.FindByPredicate([&H](const FVillage::FWelcomeVote& V) { return V.VoterId == H.Player; });
	if (!TestNotNull(TEXT("la voix du joueur compte"), Voice)) return false;
	TestFalse(TEXT("il a dit non"), Voice->bYes);
	TestEqual(TEXT("une demi-voix"), Voice->Weight, 0.5);
	TestTrue(TEXT("deux oui contre une demi-voix : accueillis"), Council.bAccepted);
	TestTrue(TEXT("le chef A se souvient que le joueur a dit non"), H.Remembers(H.ChefA, TEXT("votedNo"), H.Player, true));
	TestTrue(TEXT("le chef B aussi"), H.Remembers(H.ChefB, TEXT("votedNo"), H.Player, true));
	TestTrue(TEXT("la voix posee est consommee"), H.V.CastPlayerVote(true).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceSilenceTest, "Anastasis.Sim.Voix.Silence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoiceSilenceTest::RunTest(const FString&)
{
	using namespace AnastasisVoiceTest;
	FHamlet H;
	if (!TestFalse(TEXT("le joueur arrive"), H.Player.IsEmpty())) return false;
	// Le premier soir, une famille trace sa parcelle ; le lendemain, son chef demande -- au joueur aussi.
	H.V.UpdateFamilyHousesDaily();
	H.NextDay();
	H.V.UpdateFamilyHousesDaily();
	if (!TestEqual(TEXT("une demande faite au joueur"), H.V.GetPlayerAsks().Num(), 1)) return false;
	const FString Asker = H.V.GetPlayerAsks()[0].FromId;
	TestFalse(TEXT("elle attend sa reponse"), H.V.GetPlayerAsks()[0].bAnswered);
	// Il ne repond pas : le soir suivant, c'est un refus, et on s'en souvient.
	H.NextDay();
	H.V.UpdateFamilyHousesDaily();
	TestTrue(TEXT("la demande est close"), H.V.GetPlayerAsks()[0].bAnswered);
	const FVillage::FHelpAnswer* Silence = H.V.GetHelpLog().FindByPredicate([&H](const FVillage::FHelpAnswer& A) { return A.ToId == H.Player; });
	if (TestNotNull(TEXT("une reponse au nom du joueur"), Silence))
	{
		TestFalse(TEXT("un refus"), Silence->bAccepted);
		TestEqual(TEXT("le silence"), Silence->Reason, FString(TEXT("silence")));
	}
	TestTrue(TEXT("on se souvient de son silence"), H.Remembers(Asker, TEXT("refusedHelp"), H.Player, true));
	// Repondre maintenant ne touche pas une demande deja close : seulement une plus recente, s'il y en a.
	H.V.AnswerPlayerAsk(true);
	TestFalse(TEXT("le silence reste un refus"), H.V.GetPlayerAsks()[0].bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoiceHearsayTest, "Anastasis.Sim.Voix.OnDit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVoiceHearsayTest::RunTest(const FString&)
{
	using namespace AnastasisVoiceTest;
	FHamlet H;
	if (!TestFalse(TEXT("le joueur arrive"), H.Player.IsEmpty())) return false;
	// Le joueur a refuse d'aider A, et A l'a raconte a B.
	FVillage::FEpisodeOptions Refusal;
	Refusal.AboutId = H.Player;
	Refusal.RootId = TEXT("refus-ancien");
	const FString Episode = H.V.RecordEpisode(H.ChefA, TEXT("refusedHelp"), Refusal);
	TestTrue(TEXT("A le raconte a B"), H.V.TellEpisode(H.ChefA, H.ChefB, Episode));
	TestTrue(TEXT("B le sait par ou-dire"), H.Remembers(H.ChefB, TEXT("refusedHelp"), H.Player, false));
	// Le joueur decide de batir, puis va demander a B.
	const FString Site = H.V.PlayerBuildHome(TEXT("la maison du nouveau"));
	if (!TestFalse(TEXT("le joueur trace sa parcelle"), Site.IsEmpty())) return false;
	TestEqual(TEXT("il en est le chef"), H.V.FindNpc(H.Player)->KinRole, FString(TEXT("chef")));
	TestTrue(TEXT("pas deux chantiers a la fois"), H.V.PlayerBuildHome(TEXT("la maison du nouveau")).IsEmpty());
	FVillage::FHelpAnswer Answer;
	if (!TestTrue(TEXT("il demande a B"), H.V.PlayerAskHelp(H.ChefB, Answer))) return false;
	TestFalse(TEXT("B refuse"), Answer.bAccepted);
	TestEqual(TEXT("parce qu'on dit qu'il laisse les autres sans bras"), Answer.Reason, FString(TEXT("on_dit")));
	TestTrue(TEXT("le joueur se souvient du refus"), H.Remembers(H.Player, TEXT("refusedHelp"), H.ChefB, true));
	return true;
}

#endif
