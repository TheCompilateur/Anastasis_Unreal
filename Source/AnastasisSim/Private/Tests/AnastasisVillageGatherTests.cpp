#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Core/AnastasisSimMath.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Sim/AnastasisSimulation.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisGather.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// Cueillir puis livrer, de bout en bout — le fermier dont le poste est le grenier.
//
//   perceive : les champs a 7 tuiles deviennent des gisements connus ->
//   table : `gatherFood` (calculee) gagne le matin -> cible : le gisement
//   dont il se souvient -> A* -> session de coups (0,36 s d'ancrage) ->
//   chaque coup : champ -2, sac +2 -> sac > 9 : `deliver` vers un seuil du
//   grenier -> 1 s au seuil, dehors -> stock du grenier + sac, jusqu'a 12
//
// Les fonctions pures sont prouvees bit a bit par Anastasis.Sim.Parite.Recolte.
// Ici on prouve l'ASSEMBLAGE : qui lit quoi, dans quel ordre, et que rien ne
// se perd — nourriture des champs = sac + grenier + repas, a chaque tick.

namespace AnastasisVillageGatherTest
{
	using namespace AnastasisVillage;
	namespace G = AnastasisGather;

	constexpr double Dt = 1.0 / 60.0;
	/** 7,2 h : le matin (7 h - 11,5 h), la ou le travail pese le plus. */
	constexpr double Morning = 27.0;

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

	void SetField(AnastasisWorld::FWorld& World, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 Amount)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				AnastasisWorld::FTile& Tile = World.Tiles[Y * World.W + X];
				Tile.Type = AnastasisWorld::ETileType::Field;
				Tile.Resource = AnastasisWorld::EResource::Food;
				Tile.Amount = Amount;
				Tile.CropId = AnastasisWorld::ECropId::Grain;
			}
		}
	}

	/** La nourriture encore aux champs : l'etat VIVANT des tuiles, le monde genere restant immuable. */
	int32 FieldFood(const FVillage& Village, const AnastasisWorld::FWorld& World)
	{
		int32 Total = 0;
		for (const AnastasisWorld::FTile& Generated : World.Tiles)
		{
			const AnastasisWorld::FTile Tile = Village.LiveTileAt(Generated.X, Generated.Y);
			if (Tile.Resource == AnastasisWorld::EResource::Food) Total += Tile.Amount;
		}
		return Total;
	}

	/** Rien d'urgent : la faim, la soif, la fatigue sont loin de leur seuil. */
	AnastasisNeeds::FNeeds Rested(double Hunger = 10.0)
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = Hunger;
		N.Energy = 90.0;
		N.Social = 80.0;
		N.Leisure = 80.0;
		N.Hygiene = 80.0;
		N.Thirst = 5.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		return N;
	}

	/** Toute la nourriture du monde : champs + sacs + stocks + repas confirmes. */
	int32 FoodEverywhere(const FVillage& Village, const AnastasisWorld::FWorld& World)
	{
		int32 Total = FieldFood(Village, World);
		for (const FNpc& Npc : Village.GetActors()) Total += Npc.InventoryFood + Npc.MealsTaken;
		for (const FBuilding& B : Village.GetBuildings()) Total += B.FoodPhysical;
		return Total;
	}

	/** Avance ; rend false si un habitant dehors est sur une case bloquee, un stock incoherent, ou de la nourriture creee/perdue. */
	bool Run(FVillage& Village, const AnastasisWorld::FWorld& World, int32 Expected, double& Time, double Seconds, TFunctionRef<bool()> Stop)
	{
		const int32 Ticks = FMath::CeilToInt32(Seconds / Dt);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Time += Dt;
			Village.UpdateActors(Time, Dt);
			for (const FNpc& Npc : Village.GetActors())
			{
				if (!Npc.Inside.bActive && Village.IsFootBlocked(Npc.X, Npc.Y)) return false;
				if (Npc.InventoryFood < 0) return false;
			}
			for (const FBuilding& B : Village.GetBuildings())
			{
				if (B.FoodPhysical < 0 || B.FoodReserved < 0 || B.FoodReserved > B.FoodPhysical || B.FoodPhysical > GranaryFoodCap) return false;
			}
			if (Expected >= 0 && FoodEverywhere(Village, World) != Expected) return false;
			if (Stop()) break;
		}
		return true;
	}

	bool IsAccessPointOf(const FBuilding& B, const FPoint& P)
	{
		for (const FPoint& A : B.AccessPoints)
		{
			if (A.X == P.X && A.Y == P.Y) return true;
		}
		return false;
	}

	uint64 Bits(double V)
	{
		uint64 B;
		FMemory::Memcpy(&B, &V, sizeof(B));
		return B;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGatherPerceptionTest,
	"Anastasis.Sim.Village.Recolte.Perception",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGatherPerceptionTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGatherTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	SetField(World, 6, 10, 8, 12, 20);
	FVillage Village;
	Village.Bind(World);

	// `spawnNpc` percoit d'office : les 9 tuiles du champ, dans l'ordre du balayage.
	const FString Near = Village.SpawnNpc(13.5, 11.5, Rested());
	const FString Far = Village.SpawnNpc(30.5, 11.5, Rested());
	const FNpc* N = Village.FindNpc(Near);
	if (!TestEqual(TEXT("9 gisements vus"), N->Spots.Num(), 9))
	{
		return false;
	}
	TestEqual(TEXT("cle de la reference, balayage ligne par ligne"), N->Spots[0].Key, FString(TEXT("6,10")));
	TestEqual(TEXT("dernier balaye"), N->Spots[8].Key, FString(TEXT("8,12")));
	TestTrue(TEXT("centre de tuile"), N->Spots[0].X == 6.5 && N->Spots[0].Y == 10.5);
	TestEqual(TEXT("ressource"), N->Spots[0].Resource, FString(TEXT("food")));
	TestEqual(TEXT("quantite vue"), N->Spots[0].Amount, 20);
	TestEqual(TEXT("loin : rien vu"), Village.FindNpc(Far)->Spots.Num(), 0);

	// `forgetEmptied` : ce qu'il voit epuise, il l'efface ; ce qui change, il le met a jour.
	World.Tiles[10 * World.W + 6].Amount = 0;
	World.Tiles[11 * World.W + 7].Amount = 13;
	Village.PerceiveNow(Near);
	N = Village.FindNpc(Near);
	TestEqual(TEXT("epuise : oublie"), N->Spots.Num(), 8);
	TestEqual(TEXT("le premier est maintenant 7,10"), N->Spots[0].Key, FString(TEXT("7,10")));
	const FResourceSpot* Mid = N->Spots.FindByPredicate([](const FResourceSpot& S) { return S.Key == TEXT("7,11"); });
	TestTrue(TEXT("vu a 13"), Mid && Mid->Amount == 13);

	// `trimMemory` : 30 gisements en vue, 26 retenus ; on lache les plus anciens de la ressource dominante.
	AnastasisWorld::FWorld Wide = MakeFlatWorld(40, 32);
	SetField(Wide, 8, 8, 13, 12, 20);
	FVillage Crowd;
	Crowd.Bind(Wide);
	const FString C = Crowd.SpawnNpc(10.5, 10.5, Rested());
	const FNpc* M = Crowd.FindNpc(C);
	TestEqual(TEXT("capacite 26"), M->Spots.Num(), G::SpotCapacity);
	// Meme jour partout : chaque ajout au-dela de 26 lache le PREMIER (rang minimal, premier trouve).
	TestEqual(TEXT("les 4 premiers sont partis"), M->Spots[0].Key, FString(TEXT("12,8")));
	TestEqual(TEXT("le dernier balaye est la"), M->Spots.Last().Key, FString(TEXT("13,12")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGatherSelectionTest,
	"Anastasis.Sim.Village.Recolte.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGatherSelectionTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGatherTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	SetField(World, 6, 10, 8, 12, 20);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
	const FString House = Village.AddBuilding(HouseType, 20, 20);
	const FString Farmer = Village.SpawnNpc(13.5, 11.5, Rested());
	const FString Settler = Village.SpawnNpc(13.5, 12.5, Rested());

	// Embauche : fermier au grenier acheve seulement.
	TestFalse(TEXT("pas de sans-metier au poste"), Village.AssignWorkplace(Settler, G::JobSettler, Granary));
	TestFalse(TEXT("une maison n'est pas un poste de fermier"), Village.AssignWorkplace(Farmer, G::JobFarmer, House));
	TestTrue(TEXT("fermier au grenier"), Village.AssignWorkplace(Farmer, G::JobFarmer, Granary));
	TestTrue(TEXT("travailleur du grenier"), Village.IsGranaryWorker(*Village.FindNpc(Farmer)));
	TestFalse(TEXT("le sans-metier ne l'est pas"), Village.IsGranaryWorker(*Village.FindNpc(Settler)));

	double Time = Morning;
	Run(Village, World, FieldFood(Village, World), Time, 5.0, [&] { return Village.FindNpc(Farmer)->Goal == GoalGatherFood; });
	const FNpc* N = Village.FindNpc(Farmer);
	if (!TestEqual(TEXT("le fermier va cueillir"), N->Goal, FString(GoalGatherFood)))
	{
		return false;
	}
	const FDecisionTrace& Why = N->LastDecision;
	TestEqual(TEXT("matin"), Why.Phase, FString(TEXT("morning")));
	TestEqual(TEXT("tete de table"), Why.TableWinner, FString(GoalGatherFood));
	TestEqual(TEXT("cible : un gisement dont il se souvient"), Why.TargetSource, FString(TEXT("spot")));
	TestTrue(TEXT("le plus proche : 8,11"), N->Target.X == 8.5 && N->Target.Y == 11.5);
	TestTrue(TEXT("deliver retire (sac vide)"), FMath::IsNaN(Why.DeliverRowScore));
	TestTrue(TEXT("deliver calcule avant l'eligibilite"), !FMath::IsNaN(Why.DeliverRowTable));

	// La ligne, recomposee a la main depuis les fonctions prouvees : memes bits.
	const AnastasisNeeds::FNeeds& Needs = N->Needs;
	const double Believed = G::BelievedFoodPresumed(Farmer);
	const bool bBlocked = G::MealPathBlocked(Needs.Hunger, 0, Believed);
	const double Wf = G::MoralEffectiveWork(Needs, Village.MarketFood(), 1)
		* AnastasisRhythm::PhaseWork(AnastasisRhythm::EPhase::Morning) * 1.0 * AnastasisMath::Clamp(0.92 + 1.0 * 0.08, 0.72, 1.22);
	AnastasisRhythm::FPhaseSubject Subject;
	Subject.bHasHomeOrShelter = false;
	Subject.Energy = Needs.Energy;
	Subject.Hunger = Needs.Hunger;
	double Expected = (G::ResourceScoreFood(2, Believed, G::JobFarmer, 1.0, 0, Needs.Hunger) + Needs.Hunger * 0.15)
		* G::SurvivalWorkFactor(GoalGatherFood, Wf, bBlocked);
	Expected += AnastasisRhythm::PhaseBias(AnastasisRhythm::EPhase::Morning, Subject, GoalGatherFood);
	Expected += 22.0;
	Expected += G::CompletionBias(GoalGatherFood, 0, 0, FString(), NeedsCritical(Needs));
	Expected += G::TraitGoalBias(G::TraitAt(G::DefaultTraitIndex), GoalGatherFood);
	Expected += G::SkillGoalBias(N->SkillGather);
	TestTrue(TEXT("facteur de travail : 0,88 (marche < 20) * 1,05"), Why.WorkFactor == Wf);
	TestEqual(TEXT("ligne gatherFood : memes bits"), Bits(Why.GatherRowTable), Bits(Expected));
	AddInfo(FString::Printf(TEXT("gatherFood %.6f (Noûs %s -> %.6f), plancher %s %.3f, facteur %.4f"),
		Why.GatherRowTable, *Why.NousType, Why.GatherRowScore, *Why.FloorGoal, Why.FloorScore, Why.WorkFactor));

	// Le sans-metier : ses lignes de travail restent au plancher, il ne cueille jamais.
	Run(Village, World, FieldFood(Village, World), Time, 6.0, [] { return false; });
	const FNpc* S = Village.FindNpc(Settler);
	TestTrue(TEXT("sans-metier : ligne non calculee"), FMath::IsNaN(S->LastDecision.GatherRowTable));
	TestNotEqual(TEXT("sans-metier : pas de recolte"), S->Goal, FString(GoalGatherFood));
	TestEqual(TEXT("sans-metier : sac vide"), S->InventoryFood, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGatherHarvestTest,
	"Anastasis.Sim.Village.Recolte.Cueillette",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGatherHarvestTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGatherTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	SetField(World, 6, 10, 8, 12, 20);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
	const FString Id = Village.SpawnNpc(13.5, 11.5, Rested());
	Village.AssignWorkplace(Id, G::JobFarmer, Granary);
	const int32 Total = FoodEverywhere(Village, World);

	double Time = Morning;
	TestTrue(TEXT("sain jusqu'a l'ancrage"), Run(Village, World, Total, Time, 15.0, [&] { return Village.FindNpc(Id)->WorkSession.bActive; }));
	const FNpc* N = Village.FindNpc(Id);
	if (!TestTrue(TEXT("session ouverte"), N->WorkSession.bActive))
	{
		return false;
	}
	const FWorkSession Session = N->WorkSession;
	TestEqual(TEXT("metier farm"), Session.CraftId, FString(TEXT("farm")));
	// `resourceTileNear` : premiere tuile de nourriture du 3 x 3 autour de lui, la ou il s'est arrete.
	const AnastasisWorld::FTile Anchor = Village.LiveTileAt(Session.TileX, Session.TileY);
	TestTrue(TEXT("ancree a une tuile de champ voisine du gisement vise"),
		Anchor.Resource == AnastasisWorld::EResource::Food && FMath::Abs(Session.TileX - 8) <= 1 && FMath::Abs(Session.TileY - 11) <= 1);
	TestTrue(TEXT("premier coup a l'arrivee + 0,36 s"), Session.NextSwingAt == Session.ArrivedAt + G::FarmArriveSeconds);
	TestTrue(TEXT("un poste de la parcelle"), Session.PostIndex >= 0 && Session.PostIndex < G::FieldPostCount);
	TestEqual(TEXT("activite"), N->Activity, FString(TEXT("recolte")));

	// Premier coup : printemps (jour 1), round(2 * 0,95) = 2.
	TestTrue(TEXT("sain jusqu'au premier coup"), Run(Village, World, Total, Time, 5.0, [&] { return Village.FindNpc(Id)->GatheredFood > 0; }));
	N = Village.FindNpc(Id);
	TestEqual(TEXT("rendement du coup"), N->GatheredFood, G::FieldSeasonGatherAmount(G::YieldPerSwingFarm(1.0), 1));
	TestEqual(TEXT("sac +2"), N->InventoryFood, 2);
	TestEqual(TEXT("champ 20 -> 18"), Village.LiveTileAt(Session.TileX, Session.TileY).Amount, 18);
	TestTrue(TEXT("pas avant l'heure"), N->WorkSession.LastSwingAt >= Session.NextSwingAt);
	double Skill = 1.0;
	double Gather = G::TintedGatherSkill(G::TraitAt(G::DefaultTraitIndex));
	G::GainDomainSkill(Skill, Gather, G::GatherSkillGain);
	TestEqual(TEXT("competence : memes bits"), Bits(N->Skill), Bits(Skill));
	TestEqual(TEXT("cueillette : memes bits"), Bits(N->SkillGather), Bits(Gather));
	TestEqual(TEXT("prochain coup : swingPeriodFor"),
		Bits(N->WorkSession.NextSwingAt - N->WorkSession.LastSwingAt),
		Bits((N->WorkSession.LastSwingAt + G::SwingPeriodFarm(N->Skill, 1, N->Needs.Energy)) - N->WorkSession.LastSwingAt));

	// Sac > 9 : il rentre livrer a SON grenier.
	TestTrue(TEXT("sain jusqu'au retour"), Run(Village, World, Total, Time, 20.0, [&] { return Village.FindNpc(Id)->Goal == GoalDeliver; }));
	N = Village.FindNpc(Id);
	if (!TestEqual(TEXT("but deliver"), N->Goal, FString(GoalDeliver)))
	{
		return false;
	}
	TestEqual(TEXT("5 coups : sac 10"), N->InventoryFood, 10);
	TestFalse(TEXT("session close"), N->WorkSession.bActive);
	TestTrue(TEXT("cible : un seuil du grenier"), N->bHasTarget && IsAccessPointOf(*Village.FindBuilding(Granary), N->Target));
	TestEqual(TEXT("par identifiant"), N->DestBuildingId, Granary);
	TestEqual(TEXT("le grenier n'a encore rien"), Village.FindBuilding(Granary)->FoodPhysical, 0);
	AddInfo(FString::Printf(TEXT("5 coups en %.2f s ; champ %d,%d : %d"), Time - Session.ArrivedAt,
		Session.TileX, Session.TileY, Village.LiveTileAt(Session.TileX, Session.TileY).Amount));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGatherDeliverTest,
	"Anastasis.Sim.Village.Recolte.Livraison",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGatherDeliverTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGatherTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	SetField(World, 6, 10, 8, 12, 20);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
	const FString Id = Village.SpawnNpc(13.5, 11.5, Rested());
	Village.AssignWorkplace(Id, G::JobFarmer, Granary);
	const int32 Total = FoodEverywhere(Village, World);

	double Time = Morning;
	TestTrue(TEXT("sain jusqu'au retour"), Run(Village, World, Total, Time, 40.0, [&] { return Village.FindNpc(Id)->Goal == GoalDeliver; }));
	const double TradeBefore = Village.FindNpc(Id)->SkillTrade;
	const double SkillBefore = Village.FindNpc(Id)->Skill;
	bool bEntered = false;
	double MoraleBefore = Village.FindNpc(Id)->Needs.Morale;
	double MoraleJump = 0.0;
	TestTrue(TEXT("sain jusqu'a la livraison"), Run(Village, World, Total, Time, 30.0, [&]
	{
		const FNpc* M = Village.FindNpc(Id);
		bEntered |= M->Inside.bActive;
		MoraleJump = M->Needs.Morale - MoraleBefore;
		MoraleBefore = M->Needs.Morale;
		return M->Deliveries > 0;
	}));
	const FNpc* N = Village.FindNpc(Id);
	if (!TestEqual(TEXT("une livraison"), N->Deliveries, 1))
	{
		return false;
	}
	TestFalse(TEXT("depot de son poste : DEHORS, au seuil"), bEntered);
	TestEqual(TEXT("grenier 0 -> 10"), Village.FindBuilding(Granary)->FoodPhysical, 10);
	TestEqual(TEXT("sac vide"), N->InventoryFood, 0);
	TestEqual(TEXT("livre"), N->DeliveredFood, 10);
	// Le moral monte de 1 au tick de la livraison (plus la derive d'un tick de besoins).
	TestTrue(TEXT("moral +1"), FMath::Abs(MoraleJump - 1.0) < 0.05);
	double Skill = SkillBefore;
	double Trade = TradeBefore;
	G::GainDomainSkill(Skill, Trade, G::DeliverSkillGain);
	TestEqual(TEXT("competence de marche : memes bits"), Bits(N->SkillTrade), Bits(Trade));
	TestEqual(TEXT("competence plate : memes bits"), Bits(N->Skill), Bits(Skill));
	TestEqual(TEXT("le marche compte le grenier"), Village.MarketFood(), 10);

	// Et il y retourne : deuxieme voyage, sans rien perdre en route.
	TestTrue(TEXT("sain jusqu'au deuxieme voyage"), Run(Village, World, Total, Time, 60.0, [&] { return Village.FindNpc(Id)->Deliveries >= 2; }));
	N = Village.FindNpc(Id);
	TestEqual(TEXT("deux livraisons"), N->Deliveries, 2);
	TestEqual(TEXT("grenier 20"), Village.FindBuilding(Granary)->FoodPhysical, 20);
	AddInfo(FString::Printf(TEXT("t=%.2f : champs %d, sac %d, grenier %d, repas %d"),
		Time, FieldFood(Village, World), N->InventoryFood, Village.FindBuilding(Granary)->FoodPhysical, N->MealsTaken));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGatherDepletionTest,
	"Anastasis.Sim.Village.Recolte.Epuisement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGatherDepletionTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGatherTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	SetField(World, 8, 11, 8, 11, 5);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
	const FString Id = Village.SpawnNpc(13.5, 11.5, Rested());
	Village.AssignWorkplace(Id, G::JobFarmer, Granary);
	const int32 Total = FoodEverywhere(Village, World);

	// 2 + 2 + 1 : la derniere prise est bornee par ce qui reste.
	double Time = Morning;
	TestTrue(TEXT("sain jusqu'a l'epuisement"), Run(Village, World, Total, Time, 25.0, [&] { return Village.LiveTileAt(8, 11).Amount == 0; }));
	const AnastasisWorld::FTile Tile = Village.LiveTileAt(8, 11);
	if (!TestEqual(TEXT("champ vide"), Tile.Amount, 0))
	{
		return false;
	}
	TestTrue(TEXT("plus de ressource"), Tile.Resource == AnastasisWorld::EResource::None);
	TestTrue(TEXT("reste un champ"), Tile.Type == AnastasisWorld::ETileType::Field);
	TestTrue(TEXT("en jachere"), Tile.CropId == AnastasisWorld::ECropId::Fallow);
	TestEqual(TEXT("le monde genere reste immuable"), World.Tiles[11 * World.W + 8].Amount, 5);
	TestEqual(TEXT("sac 5 : sous le seuil, pas de retour force"), Village.FindNpc(Id)->InventoryFood, 5);

	// Plus rien a cueillir : le sac de 5 part au grenier (deliver, eligible avec une charge).
	TestTrue(TEXT("sain jusqu'a la livraison"), Run(Village, World, Total, Time, 40.0, [&] { return Village.FindNpc(Id)->Deliveries > 0; }));
	TestEqual(TEXT("grenier 5"), Village.FindBuilding(Granary)->FoodPhysical, 5);
	Village.PerceiveNow(Id);
	TestEqual(TEXT("le champ vide est oublie"), Village.FindNpc(Id)->Spots.Num(), 0);
	// Sans gisement connu, pas d'exploration (tirage non porte) : il ne cueille plus.
	Run(Village, World, Total, Time, 8.0, [] { return false; });
	TestNotEqual(TEXT("plus de recolte"), Village.FindNpc(Id)->Goal, FString(GoalGatherFood));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGatherMultiTest,
	"Anastasis.Sim.Village.Recolte.MultiAgents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGatherMultiTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGatherTest;
	// Une seule parcelle, trois fermiers : trois postes distincts dans la tuile.
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	SetField(World, 8, 11, 8, 11, 37);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
	TArray<FString> Ids;
	for (int32 I = 0; I < 3; ++I)
	{
		Ids.Add(Village.SpawnNpc(13.5, 10.5 + I, Rested()));
		Village.AssignWorkplace(Ids.Last(), G::JobFarmer, Granary);
	}
	const int32 Total = FoodEverywhere(Village, World);

	double Time = Morning;
	bool bThreeTogether = false;
	bool bDistinct = true;
	TestTrue(TEXT("sain, rien ne se perd"), Run(Village, World, Total, Time, 60.0, [&]
	{
		TArray<int32> Posts;
		for (const FString& Id : Ids)
		{
			const FWorkSession& S = Village.FindNpc(Id)->WorkSession;
			if (S.bActive && S.TileX == 8 && S.TileY == 11 && S.PostIndex >= 0) Posts.Add(S.PostIndex);
		}
		if (Posts.Num() == 3)
		{
			bThreeTogether = true;
			bDistinct &= Posts[0] != Posts[1] && Posts[0] != Posts[2] && Posts[1] != Posts[2];
		}
		return Village.LiveTileAt(8, 11).Amount == 0 && Village.FindBuilding(Granary)->FoodPhysical > 0;
	}));
	TestTrue(TEXT("les trois a la meme parcelle"), bThreeTogether);
	TestTrue(TEXT("chacun son poste"), bDistinct);
	TestEqual(TEXT("parcelle videe"), Village.LiveTileAt(8, 11).Amount, 0);
	int32 Gathered = 0;
	for (const FString& Id : Ids) Gathered += Village.FindNpc(Id)->GatheredFood;
	TestEqual(TEXT("37 cueillis en tout"), Gathered, 37);
	TestTrue(TEXT("le grenier a recu"), Village.FindBuilding(Granary)->FoodPhysical > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGatherDestructionTest,
	"Anastasis.Sim.Village.Recolte.Destruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGatherDestructionTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGatherTest;
	// 1. Demoli pendant que le fermier rentre, sac plein.
	{
		AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
		SetField(World, 6, 10, 8, 12, 20);
		FVillage Village;
		Village.Bind(World);
		const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
		const FString Id = Village.SpawnNpc(13.5, 11.5, Rested());
		Village.AssignWorkplace(Id, G::JobFarmer, Granary);
		const int32 Total = FoodEverywhere(Village, World);
		double Time = Morning;
		Run(Village, World, Total, Time, 40.0, [&] { return Village.FindNpc(Id)->Goal == GoalDeliver; });
		if (!TestEqual(TEXT("en route pour livrer"), Village.FindNpc(Id)->Goal, FString(GoalDeliver)))
		{
			return false;
		}
		TestTrue(TEXT("demoli"), Village.RemoveBuilding(Granary));
		const FNpc* N = Village.FindNpc(Id);
		TestTrue(TEXT("plus de poste"), N->WorkplaceId.IsEmpty());
		TestEqual(TEXT("metier garde"), N->JobId, FString(G::JobFarmer));
		TestEqual(TEXT("but perdu"), N->Goal, FString(GoalObserver));
		TestFalse(TEXT("plus de cible"), N->bHasTarget);
		TestTrue(TEXT("aucune reference"), N->DestBuildingId.IsEmpty());
		TestEqual(TEXT("le sac reste plein"), N->InventoryFood, 10);
		TestTrue(TEXT("plus un travailleur du grenier"), !Village.IsGranaryWorker(*N));
		TestTrue(TEXT("sain ensuite, sans livrer ni cueillir"), Run(Village, World, Total, Time, 15.0, [&]
		{
			const FString& Goal = Village.FindNpc(Id)->Goal;
			return Goal == GoalDeliver || Goal == GoalGatherFood;
		}));
		TestTrue(TEXT("ni livrer ni cueillir sans poste"),
			Village.FindNpc(Id)->Goal != GoalDeliver && Village.FindNpc(Id)->Goal != GoalGatherFood);
	}
	// 2. Demoli pendant la recolte : la session tombe avec le poste.
	{
		AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
		SetField(World, 6, 10, 8, 12, 20);
		FVillage Village;
		Village.Bind(World);
		const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
		const FString Id = Village.SpawnNpc(13.5, 11.5, Rested());
		Village.AssignWorkplace(Id, G::JobFarmer, Granary);
		const int32 Total = FoodEverywhere(Village, World);
		double Time = Morning;
		Run(Village, World, Total, Time, 20.0, [&] { return Village.FindNpc(Id)->GatheredFood > 0; });
		TestTrue(TEXT("demoli pendant la recolte"), Village.RemoveBuilding(Granary));
		const FNpc* N = Village.FindNpc(Id);
		TestFalse(TEXT("session tombee"), N->WorkSession.bActive);
		TestEqual(TEXT("but perdu"), N->Goal, FString(GoalObserver));
		TestTrue(TEXT("sain ensuite"), Run(Village, World, Total, Time, 10.0, [] { return false; }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGatherFullTest,
	"Anastasis.Sim.Village.Recolte.Plein",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGatherFullTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGatherTest;
	// Grenier a 295 / 300 : 10 livres, 5 entrent, 5 restent au sac.
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	SetField(World, 6, 10, 8, 12, 20);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.AddBuilding(GranaryType, 16, 11);
	Village.CreditFood(Granary, 295);
	const FString Id = Village.SpawnNpc(13.5, 11.5, Rested());
	Village.AssignWorkplace(Id, G::JobFarmer, Granary);
	const int32 Total = FoodEverywhere(Village, World);
	double Time = Morning;
	TestTrue(TEXT("sain"), Run(Village, World, Total, Time, 60.0, [&] { return Village.FindNpc(Id)->Deliveries > 0; }));
	const FNpc* N = Village.FindNpc(Id);
	if (!TestEqual(TEXT("une livraison"), N->Deliveries, 1))
	{
		return false;
	}
	TestEqual(TEXT("plein a 300"), Village.FindBuilding(Granary)->FoodPhysical, GranaryFoodCap);
	TestEqual(TEXT("5 livres"), N->DeliveredFood, 5);
	TestEqual(TEXT("5 au sac"), N->InventoryFood, 5);
	// Plus de place : deliver echoue, rien ne deborde, rien ne se perd.
	TestTrue(TEXT("sain, jamais au-dela de 300"), Run(Village, World, Total, Time, 15.0, [] { return false; }));
	TestEqual(TEXT("toujours 300"), Village.FindBuilding(Granary)->FoodPhysical, GranaryFoodCap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageGatherHostTest,
	"Anastasis.Sim.Village.Recolte.Hote",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageGatherHostTest::RunTest(const FString&)
{
	using namespace AnastasisVillageGatherTest;
	FAnastasisSimulation Sim;
	Sim.Reset(12345u, 96, 96);
	FVillage& Village = Sim.GetVillage();
	const AnastasisWorld::FWorld& World = Sim.GetWorld();

	// Le champ le plus proche du centre, puis une case libre a 3-5 tuiles pour le grenier.
	int32 FX = -1;
	int32 FY = -1;
	double Best = AnastasisNav::Infinity;
	for (const AnastasisWorld::FTile& Tile : World.Tiles)
	{
		if (Tile.Resource != AnastasisWorld::EResource::Food || Tile.Amount <= 0) continue;
		const double D = AnastasisMath::Dist(Tile.X, Tile.Y, 48.0, 48.0);
		if (D < Best)
		{
			Best = D;
			FX = Tile.X;
			FY = Tile.Y;
		}
	}
	if (!TestTrue(TEXT("un champ sur la carte canonique"), FX >= 0))
	{
		return false;
	}
	FString Granary;
	for (int32 R = 3; R <= 5 && Granary.IsEmpty(); ++R)
	{
		for (int32 DY = -R; DY <= R && Granary.IsEmpty(); ++DY)
		{
			for (int32 DX = -R; DX <= R && Granary.IsEmpty(); ++DX)
			{
				const int32 X = FX + DX;
				const int32 Y = FY + DY;
				if (World.Tiles.IsValidIndex(Y * World.W + X) && World.Tiles[Y * World.W + X].Resource != AnastasisWorld::EResource::None) continue;
				if (Village.IsFootBlocked(X + 0.5, Y + 0.5) || Village.IsFootBlocked(X + 1.5, Y + 0.5)) continue;
				Granary = Village.AddBuilding(GranaryType, X, Y);
			}
		}
	}
	if (!TestFalse(TEXT("grenier pose"), Granary.IsEmpty()))
	{
		return false;
	}
	const FPoint Door = Village.FindBuilding(Granary)->AccessPoints[0];
	const FString A = Village.SpawnNpc(Door.X, Door.Y, Rested());
	TestTrue(TEXT("embauche"), Village.AssignWorkplace(A, G::JobFarmer, Granary));
	TestTrue(TEXT("il voit des champs"), Village.FindNpc(A)->Spots.ContainsByPredicate([](const FResourceSpot& S) { return S.Resource == TEXT("food"); }));

	for (int32 I = 0; I < 60 * 90 && Village.FindBuilding(Granary)->FoodPhysical == 0; ++I)
	{
		Sim.Tick(Dt);
	}
	const FNpc* N = Village.FindNpc(A);
	TestTrue(TEXT("l'hote fait livrer le fermier"), Village.FindBuilding(Granary)->FoodPhysical > 0);
	TestEqual(TEXT("ce qui est livre est ce qu'il a porte"), Village.FindBuilding(Granary)->FoodPhysical, N->DeliveredFood);
	AddInfo(FString::Printf(TEXT("champ (%d,%d), grenier %s, t=%.3f : cueilli %d, livre %d"),
		FX, FY, *Granary, Sim.GetTime(), N->GatheredFood, N->DeliveredFood));
	return true;
}

#endif
