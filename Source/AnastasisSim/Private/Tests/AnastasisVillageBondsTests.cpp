#include "Misc/AutomationTest.h"

#include "Life/AnastasisBonds.h"
#include "Life/AnastasisNeeds.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// Liens et rumeurs, de bout en bout — la branche AVEC compagnon de `socialize()`.
//
//   socialize gagne (solitude) -> cible : le puits (ou l'ami, ou la personne
//   memorisee) -> sur place : compagnon le mieux place dans la grille du tick ->
//   relation +6 / +5, besoins des deux, actes de parole sur les gisements ->
//   porte de parole -> session : les DEUX sont figes, 2 tours de 2,2 s ->
//   fiches de personne et theorie de l'esprit -> a minuit, 8e tick : l'oubli
//
// Les fonctions pures sont prouvees bit a bit par Anastasis.Sim.Parite.Liens.
// Ici on prouve l'ASSEMBLAGE : qui parle a qui, qui se fige, ce qui passe d'une
// tete a l'autre, et quand la memoire s'efface.

namespace AnastasisVillageBondsTest
{
	using namespace AnastasisVillage;
	namespace BD = AnastasisBonds;

	constexpr double Dt = 1.0 / 60.0;
	/** 12 h : midi, le creux du jour ou l'on se retrouve au puits. */
	constexpr double Midday = 45.0;

	AnastasisWorld::FWorld MakeFlatWorld(int32 W, int32 H)
	{
		AnastasisWorld::FWorld World;
		World.W = W;
		World.H = H;
		World.Tiles.SetNum(W * H);
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				AnastasisWorld::FTile& Tile = World.Tiles[Y * W + X];
				Tile.X = X;
				Tile.Y = Y;
				Tile.Type = AnastasisWorld::ETileType::Grass;
				Tile.Alt = 0.5;
				Tile.Wetness = 0.3;
			}
		}
		return World;
	}

	/** Seul : la solitude pese, rien d'autre ne presse. */
	AnastasisNeeds::FNeeds Lonely()
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 10.0;
		N.Energy = 90.0;
		N.Social = 25.0;
		N.Leisure = 80.0;
		N.Hygiene = 80.0;
		N.Thirst = 5.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		return N;
	}

	/** Avance ; rend false si un habitant dehors est sur une case bloquee. */
	bool Run(FVillage& Village, double& Time, double Seconds, TFunctionRef<bool()> Stop)
	{
		const int32 Ticks = FMath::CeilToInt32(Seconds / Dt);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Time += Dt;
			Village.UpdateActors(Time, Dt);
			for (const FNpc& Npc : Village.GetActors())
			{
				if (!Npc.Inside.bActive && Village.IsFootBlocked(Npc.X, Npc.Y)) return false;
			}
			if (Stop()) break;
		}
		return true;
	}

	const BD::FPersonRow* Row(const FNpc& Npc, const FString& Id)
	{
		return BD::FindPerson(Npc.People, Id);
	}

	bool HasNewFriend(const FNpc& Npc)
	{
		return Npc.Moodlets.ContainsByPredicate([](const BD::FMoodlet& M) { return M.Id == TEXT("newFriend"); });
	}

	int32 Talks(const FVillage& Village)
	{
		int32 N = 0;
		for (const FNpc& Npc : Village.GetActors()) N += Npc.TalksWithCompanion;
		return N;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageBondsConversationTest,
	"Anastasis.Sim.Village.Liens.Conversation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageBondsConversationTest::RunTest(const FString&)
{
	using namespace AnastasisVillageBondsTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	Village.AddBuilding(WellType, 16, 11);
	const FString A = Village.SpawnNpc(13.5, 14.5, Lonely());
	const FString B = Village.SpawnNpc(19.5, 14.5, Lonely());

	double Time = Midday;
	TestTrue(TEXT("sain jusqu'a la premiere conversation"), Run(Village, Time, 240.0,
		[&] { return Village.IsTalking(*Village.FindNpc(A)) || Village.IsTalking(*Village.FindNpc(B)); }));
	const FNpc* NA = Village.FindNpc(A);
	const FNpc* NB = Village.FindNpc(B);
	if (!TestTrue(TEXT("une conversation s'ouvre"), Village.IsTalking(*NA) && Village.IsTalking(*NB)))
	{
		return false;
	}
	// Une seule session, partagee : memes bornes, meme starter.
	TestEqual(TEXT("A parle a B"), NA->TalkWithId, B);
	TestEqual(TEXT("B parle a A"), NB->TalkWithId, A);
	TestTrue(TEXT("meme fin de session"), NA->TalkUntil == NB->TalkUntil);
	TestEqual(TEXT("meme starter"), NA->TalkStarterId, NB->TalkStarterId);
	TestEqual(TEXT("premier tour"), NA->TalkTurn, 1);
	const double Start = Time;
	const FString FirstStarter = NA->TalkStarterId;
	const FNpc& FirstListener = FirstStarter == A ? *NB : *NA;
	const bool bRefusedAtOnce = FirstListener.LastTalk.bValid && FirstListener.LastTalk.bRefuse;
	if (bRefusedAtOnce)
	{
		// Refus de repondre : la session tombe a 0,45 s, un seul tour.
		TestEqual(TEXT("refus : un tour"), NA->TalkMaxTurns, 1);
		TestTrue(TEXT("refus : 0,45 s"), NA->TalkUntil == Time + BD::SessionSecondsUrgent);
	}
	else
	{
		// Fatigue comptee APRES le premier propos (1) : min(2, max(1, 3 - 1)) = 2 tours de 2,2 s.
		TestEqual(TEXT("deux tours entre inconnus fatigues d'un propos"), NA->TalkMaxTurns, 2);
		TestTrue(TEXT("session = 2 x 2,2 s"), NA->TalkUntil == Time + BD::TurnSeconds * 2);
	}

	const FString StarterId = NA->TalkStarterId;
	const FNpc& Starter = StarterId == A ? *NA : *NB;
	const FNpc& Other = StarterId == A ? *NB : *NA;
	// L'echange (relation, besoins, rumeurs, fiches) precede la porte de parole : entre
	// inconnus calmes, `shouldSpeakNow` ne laisse passer qu'environ 12 % des propos, et
	// un echange sans parole ne pose ni cooldown ni fatigue. N echanges avant la session.
	const int32 Exchanges = Talks(Village);
	TestTrue(TEXT("au moins un echange"), Exchanges >= 1);
	// `bumpRelation(npc, other, 6, max(4, 6 - 1))` : round(6 x 1,05) = 6 entre inconnus,
	// tant que la relation reste sous l'amitie (45).
	TestEqual(TEXT("relations : 6 + 5 par echange"),
		FVillage::RelationOf(Starter, Other.Id) + FVillage::RelationOf(Other, Starter.Id), 11.0 * Exchanges);
	const BD::FPersonRow* SR = Row(Starter, Other.Id);
	const BD::FPersonRow* OR = Row(Other, Starter.Id);
	if (TestNotNull(TEXT("fiche du starter"), SR) && TestNotNull(TEXT("fiche de l'autre"), OR))
	{
		// MEET_TRUST a celui qui socialise, x 0,85 a l'autre.
		TestEqual(TEXT("confiance : 2,5 + 2,125 par echange"), SR->Trust + OR->Trust, (BD::MeetTrust + BD::MeetTrust * 0.85) * Exchanges);
		TestEqual(TEXT("une rencontre par echange (starter)"), SR->Meets, Exchanges);
		TestEqual(TEXT("une rencontre par echange (autre)"), OR->Meets, Exchanges);
	}
	TestTrue(TEXT("theorie de l'esprit : une entree sur l'autre"),
		Starter.Tom.ContainsByPredicate([&](const BD::FTomEntry& T) { return T.Id == Other.Id; }));

	// Le gel : ni l'un ni l'autre ne bouge tant que dure la session ; le starter avance le tour.
	const double AX = NA->X, AY = NA->Y, BX = NB->X, BY = NB->Y;
	double Until = NA->TalkUntil;
	int32 FrozenTicks = 0;
	int32 MaxTurn = 1;
	bool bStill = true;
	bool bSocialise = true;
	bool bRefused = bRefusedAtOnce;
	TestTrue(TEXT("sain pendant la session"), Run(Village, Time, 10.0, [&]
	{
		const FNpc* XA = Village.FindNpc(A);
		const FNpc* XB = Village.FindNpc(B);
		if (!Village.IsTalking(*XA) && !Village.IsTalking(*XB)) return true;
		++FrozenTicks;
		bStill &= XA->X == AX && XA->Y == AY && XB->X == BX && XB->Y == BY;
		bSocialise &= XA->Activity == TEXT("socialise") && XB->Activity == TEXT("socialise");
		MaxTurn = FMath::Max(MaxTurn, XA->TalkTurn);
		Until = XA->TalkUntil;
		bRefused |= (XA->LastTalk.bValid && XA->LastTalk.bRefuse) || (XB->LastTalk.bValid && XB->LastTalk.bRefuse);
		return false;
	}));
	TestTrue(TEXT("les deux restent figes"), bStill);
	TestTrue(TEXT("activite : socialise"), bSocialise);
	if (!bRefused)
	{
		TestEqual(TEXT("le deuxieme tour a eu lieu"), MaxTurn, 2);
	}
	// Le gel couvre la session jusqu'a sa derniere borne, au tick pres.
	TestTrue(TEXT("le gel dure la session"), FMath::Abs(FrozenTicks * Dt - (Until - Start)) <= 2.0 * Dt);
	AddInfo(FString::Printf(TEXT("%d echanges avant la session ; starter %s, gel %d ticks (%.2f s), session %.3f s, tours %d, refus %s"),
		Exchanges, *StarterId, FrozenTicks, FrozenTicks * Dt, Until - Start, MaxTurn, bRefused ? TEXT("oui") : TEXT("non")));

	// La session se referme : plus d'ancre, plus de partenaire.
	Run(Village, Time, Dt, [] { return false; });
	TestTrue(TEXT("session effacee chez A"), Village.FindNpc(A)->TalkWithId.IsEmpty());
	TestTrue(TEXT("session effacee chez B"), Village.FindNpc(B)->TalkWithId.IsEmpty());
	TestFalse(TEXT("l'ancre du regard n'est pas une destination"), Village.FindNpc(A)->bTalkAnchor || Village.FindNpc(B)->bTalkAnchor);

	// Cooldown de paire (18 s) : ils restent ensemble, sans nouvelle conversation.
	TestTrue(TEXT("sain pendant le cooldown"), Run(Village, Time, 10.0, [] { return false; }));
	TestEqual(TEXT("pas de nouvel echange avant 18 s"), Talks(Village), Exchanges);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageBondsFriendshipTest,
	"Anastasis.Sim.Village.Liens.Amitie",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageBondsFriendshipTest::RunTest(const FString&)
{
	using namespace AnastasisVillageBondsTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	Village.AddBuilding(WellType, 16, 11);
	const FString A = Village.SpawnNpc(13.5, 14.5, Lonely());
	const FString B = Village.SpawnNpc(19.5, 14.5, Lonely());
	// Presque amis : 44 de part et d'autre (palier « connaissance »).
	Village.FindNpcMutable(A)->Relations.Add(TPair<FString, double>(B, 44.0));
	Village.FindNpcMutable(B)->Relations.Add(TPair<FString, double>(A, 44.0));

	double Time = Midday;
	TestTrue(TEXT("sain jusqu'a la conversation"), Run(Village, Time, 240.0, [&] { return Talks(Village) > 0; }));
	const FNpc* NA = Village.FindNpc(A);
	const FNpc* NB = Village.FindNpc(B);
	if (!TestEqual(TEXT("une conversation"), Talks(Village), 1))
	{
		return false;
	}
	const FNpc& Starter = NA->TalksWithCompanion > 0 ? *NA : *NB;
	const FNpc& Other = NA->TalksWithCompanion > 0 ? *NB : *NA;
	TestEqual(TEXT("44 + 6 : amis"), FVillage::RelationOf(Starter, Other.Id), 50.0);
	TestEqual(TEXT("44 + 5"), FVillage::RelationOf(Other, Starter.Id), 49.0);
	TestEqual(TEXT("palier franchi"), BD::BondStageRank(FVillage::RelationOf(Starter, Other.Id)), 2);
	// `noteBondStageCross` : le moodlet newFriend aux DEUX, 72 s.
	TestTrue(TEXT("newFriend chez le starter"), HasNewFriend(Starter));
	TestTrue(TEXT("newFriend chez l'autre"), HasNewFriend(Other));
	for (const BD::FMoodlet& M : Starter.Moodlets)
	{
		if (M.Id == TEXT("newFriend")) TestEqual(TEXT("72 s"), M.Until - M.At, BD::NewFriendSeconds);
	}
	// Desormais amis : la porte de parole et le gain de la prochaine fois changent.
	TestEqual(TEXT("lien : amis"), static_cast<int32>(BD::BondKindBetween(
		FVillage::RelationOf(Starter, Other.Id), FVillage::RelationOf(Other, Starter.Id), Starter.JobId, Other.JobId)),
		static_cast<int32>(BD::EBondKind::Friend));
	TestEqual(TEXT("gain d'amis : round(8 x 1,05)"), BD::BondTalkGain(true), 8);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageBondsRumorTest,
	"Anastasis.Sim.Village.Liens.Rumeur",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageBondsRumorTest::RunTest(const FString&)
{
	using namespace AnastasisVillageBondsTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	for (int32 Y = 10; Y <= 12; ++Y)
	{
		for (int32 X = 4; X <= 6; ++X)
		{
			AnastasisWorld::FTile& Tile = World.Tiles[Y * World.W + X];
			Tile.Type = AnastasisWorld::ETileType::Field;
			Tile.Resource = AnastasisWorld::EResource::Food;
			Tile.Amount = 20;
			Tile.CropId = AnastasisWorld::ECropId::Grain;
		}
	}
	FVillage Village;
	Village.Bind(World);
	Village.AddBuilding(WellType, 30, 11);
	// A a vu le champ (perception au spawn), puis il est revenu au puits.
	const FString A = Village.SpawnNpc(7.5, 11.5, Lonely());
	FNpc* MA = Village.FindNpcMutable(A);
	MA->X = 28.5;
	MA->Y = 14.5;
	// B ne l'a jamais vu : 26 tuiles du champ, la perception porte a 7.
	const FString B = Village.SpawnNpc(33.5, 14.5, Lonely());
	TArray<FResourceSpot> Seen;
	for (const FResourceSpot& S : Village.FindNpc(A)->Spots)
	{
		if (S.Resource == TEXT("food")) Seen.Add(S);
	}
	if (!TestTrue(TEXT("A connait le champ"), Seen.Num() > 0))
	{
		return false;
	}
	TestEqual(TEXT("B ne connait aucun gisement"), Village.FindNpc(B)->Spots.Num(), 0);

	double Time = Midday;
	TestTrue(TEXT("sain jusqu'a la rumeur"), Run(Village, Time, 240.0, [&] { return Village.FindNpc(B)->RumorsHeard > 0; }));
	const FNpc* NA = Village.FindNpc(A);
	const FNpc* NB = Village.FindNpc(B);
	if (!TestTrue(TEXT("B a entendu parler du champ"), NB->RumorsHeard > 0))
	{
		return false;
	}
	// `createInformResourceSpotActs(..., { limit: 2 })` : deux gisements par echange.
	TestEqual(TEXT("deux gisements au premier echange"), NB->RumorsHeard, FMath::Min(2, Seen.Num()));
	TestEqual(TEXT("A les a racontes"), NA->RumorsShared, NB->RumorsHeard);
	for (const FResourceSpot& S : NB->Spots)
	{
		TestTrue(TEXT("on-dit, pas vu"), S.bHearsay);
		TestEqual(TEXT("dit par A"), S.SourceId, A);
		TestEqual(TEXT("source d'origine : A"), S.OriginalSourceId, A);
		TestEqual(TEXT("une bouche"), S.HopCount, 1);
		TestEqual(TEXT("entendu le jour 1"), S.ReceivedDay, 1);
		const FResourceSpot* Origin = Seen.FindByPredicate([&](const FResourceSpot& O) { return O.Key == S.Key; });
		if (TestNotNull(TEXT("un gisement que A a vu"), Origin))
		{
			TestEqual(TEXT("age du souvenir : le jour ou A l'a vu"), S.Day, Origin->Day);
			TestEqual(TEXT("quantite rapportee"), S.Amount, Origin->Amount);
		}
	}
	// Seul ce qu'on a vu de ses yeux se colporte : B n'a rien a redire.
	TestEqual(TEXT("un on-dit ne se recolporte pas"),
		CreateInformSpotActs(NB->Spots, TArray<FResourceSpot>(), B, 0.0, 2).Num(), 0);
	AddInfo(FString::Printf(TEXT("rumeur a t=%.2f : %d gisements, premier %s"), Time, NB->RumorsHeard,
		NB->Spots.Num() > 0 ? *NB->Spots[0].Key : TEXT("-")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageBondsMemoryTest,
	"Anastasis.Sim.Village.Liens.Memoire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageBondsMemoryTest::RunTest(const FString&)
{
	using namespace AnastasisVillageBondsTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(48, 32);
	FVillage Village;
	Village.Bind(World);
	Village.AddBuilding(WellType, 8, 11);
	// A se souvient de B, un allie : il ira vers lui plutot qu'au puits.
	const FString A = Village.SpawnNpc(10.5, 14.5, Lonely());
	AnastasisNeeds::FNeeds Busy = Lonely();
	Busy.Social = 90.0;
	const FString B = Village.SpawnNpc(26.5, 14.5, Busy);
	BD::FPersonRow Ally;
	Ally.Id = B;
	Ally.Trust = 30.0;
	Ally.Tag = BD::EPersonTag::Ally;
	Ally.Day = 1;
	Ally.Meets = 3;
	Village.FindNpcMutable(A)->People.Add(Ally);

	double Time = Midday;
	TestTrue(TEXT("sain jusqu'a la decision"), Run(Village, Time, 30.0, [&]
	{
		const FNpc* N = Village.FindNpc(A);
		return N->Goal == GoalSocialize && N->bHasTarget;
	}));
	const FNpc* NA = Village.FindNpc(A);
	if (!TestEqual(TEXT("A va socialiser"), NA->Goal, FString(GoalSocialize)))
	{
		return false;
	}
	TestEqual(TEXT("vers la personne memorisee"), NA->SocialSeekId, B);
	TestEqual(TEXT("couche memoire"), NA->LastDecision.TargetSource, FString(TEXT("memory")));
	const FNpc* NB = Village.FindNpc(B);
	TestTrue(TEXT("la cible est B, pas le puits"), FMath::Abs(NA->Target.X - NB->X) < 2.0 && FMath::Abs(NA->Target.Y - NB->Y) < 2.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageBondsForgetTest,
	"Anastasis.Sim.Village.Liens.Oubli",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageBondsForgetTest::RunTest(const FString&)
{
	using namespace AnastasisVillageBondsTest;
	// Le vrai hote : l'oubli est le travail `memory` (n°14) de la file de minuit, 2 par tick.
	FAnastasisSimulation Sim;
	Sim.Reset(12345u, 96, 96);
	FVillage& Village = Sim.GetVillage();
	FPoint Spot;
	bool bFound = false;
	for (int32 R = 0; R < 30 && !bFound; ++R)
	{
		for (int32 DY = -R; DY <= R && !bFound; ++DY)
		{
			for (int32 DX = -R; DX <= R && !bFound; ++DX)
			{
				if (Village.IsFootBlocked(48 + DX + 0.5, 48 + DY + 0.5)) continue;
				Spot = { 48 + DX + 0.5, 48 + DY + 0.5 };
				bFound = true;
			}
		}
	}
	if (!TestTrue(TEXT("une case libre"), bFound))
	{
		return false;
	}
	AnastasisNeeds::FNeeds N = Lonely();
	N.Social = 90.0;
	const FString Id = Village.SpawnNpc(Spot.X, Spot.Y, N);
	int32 Ticks = 0;
	while (Sim.GetDay() == 1 && Ticks < 20000)
	{
		Sim.Tick(Dt);
		++Ticks;
	}
	TestEqual(TEXT("jour 2"), Sim.GetDay(), 2);
	TestEqual(TEXT("tick de minuit : jobs 0 et 1 faits"), Sim.GetDeferredRemaining(), 15);
	// Souvenirs poses apres le tick de minuit, d'une ressource que la perception ne rapporte
	// pas (« clay ») : le rognage a 26 gisements ne les touche pas.
	FNpc* M = Village.FindNpcMutable(Id);
	// Au jour 2 : 2 - (-13) = 15 > 14 s'efface ; 2 - (-12) = 14 reste.
	auto Remember = [&](const TCHAR* Key, double X, int32 Day, bool bHearsay)
	{
		FResourceSpot S;
		S.Key = Key;
		S.X = X;
		S.Y = 0.5;
		S.Resource = TEXT("clay");
		S.Amount = 5;
		S.Day = Day;
		S.bHearsay = bHearsay;
		M->Spots.Add(S);
	};
	Remember(TEXT("0,0"), 0.5, -13, false);
	Remember(TEXT("1,0"), 1.5, -12, false);
	Remember(TEXT("2,0"), 2.5, -13, true);
	auto Meet = [&](const TCHAR* Who, int32 Day)
	{
		BD::FPersonRow P;
		P.Id = Who;
		P.Trust = 5.0;
		P.Day = Day;
		P.Meets = 1;
		M->People.Add(P);
	};
	// Fiches : 2 - (-25) = 27 > 26 s'efface ; 2 - (-24) = 26 reste.
	Meet(TEXT("old"), -25);
	Meet(TEXT("recent"), -24);

	auto Has = [&](const TCHAR* Key) { return Village.FindNpc(Id)->Spots.ContainsByPredicate([&](const FResourceSpot& S) { return S.Key == Key; }); };
	auto Knows = [&](const TCHAR* Who) { return BD::FindPerson(Village.FindNpc(Id)->People, Who) != nullptr; };

	// Les ticks 2 a 7 : jobs 2 a 13. Le 8e : 14 (`memory`) et 15.
	for (int32 K = 2; K <= 7; ++K)
	{
		Sim.Tick(Dt);
		TestTrue(*FString::Printf(TEXT("tick %d : encore en memoire"), K), Has(TEXT("0,0")) && Knows(TEXT("old")));
	}
	TestEqual(TEXT("3 travaux restent avant le 8e tick"), Sim.GetDeferredRemaining(), 3);
	Sim.Tick(Dt);
	TestFalse(TEXT("8e tick : le gisement de 15 jours s'efface"), Has(TEXT("0,0")));
	TestTrue(TEXT("celui de 14 jours reste"), Has(TEXT("1,0")));
	TestFalse(TEXT("l'on-dit vieilli s'efface aussi"), Has(TEXT("2,0")));
	TestFalse(TEXT("la fiche de 27 jours s'efface"), Knows(TEXT("old")));
	TestTrue(TEXT("celle de 26 jours reste"), Knows(TEXT("recent")));
	TestEqual(TEXT("file : 1 travail restant"), Sim.GetDeferredRemaining(), 1);
	return true;
}

#endif
