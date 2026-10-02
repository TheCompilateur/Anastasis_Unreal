#include "Misc/AutomationTest.h"

#include "Life/AnastasisNeeds.h"
#include "Life/AnastasisVillageRhythm.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisBuild.h"
#include "Work/AnastasisGather.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// Le chantier, de bout en bout — un batiment monte sous les coups de ses batisseurs.
//
//   l'hote ouvre un chantier (devis livre ou non) -> la ligne `build` (besoin 85)
//   gagne -> cible : le seuil du premier chantier -> session `build` (0,4 s
//   d'ancrage) -> chaque coup : une piece, sa part du devis prise au stock du site
//   -> 22 pieces : acheve, il sert (maison habitable, grenier embauche)
//
// Les fonctions pures sont prouvees bit a bit par Anastasis.Sim.Parite.Chantier.
// Ici on prouve l'ASSEMBLAGE : qui bat, dans quel ordre, et que rien ne se perd —
// devis = consomme + stock du site, a chaque tick.

namespace AnastasisVillageBuildTest
{
	using namespace AnastasisVillage;
	namespace B = AnastasisBuild;
	namespace G = AnastasisGather;

	constexpr double Dt = 1.0 / 60.0;
	/** 7,2 h : le matin, la ou le travail pese le plus. */
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

	AnastasisNeeds::FNeeds Rested()
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 10.0;
		N.Energy = 90.0;
		N.Social = 80.0;
		N.Leisure = 80.0;
		N.Hygiene = 80.0;
		N.Thirst = 5.0;
		N.Health = 95.0;
		N.Morale = 60.0;
		return N;
	}

	/** Le devis tient : consomme + stock du site = devis livre, a chaque tick ; rien d'inacheve n'est habite. */
	bool SiteConserved(const FBuilding& Site, int32 DeliveredWood, int32 DeliveredStone)
	{
		const B::FSiteMaterials& M = Site.Materials;
		return M.ConsumedWood + M.StockWood == DeliveredWood && M.ConsumedStone + M.StockStone == DeliveredStone
			&& M.ConsumedWood <= M.NeedWood && M.ConsumedStone <= M.NeedStone
			&& Site.PiecesPlaced >= 0 && Site.PiecesPlaced <= B::PieceTotal;
	}

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

	FString SpawnBuilder(FVillage& Village, double X, double Y)
	{
		const FString Id = Village.SpawnNpc(X, Y, Rested());
		Village.SetJob(Id, B::JobBuilder);
		return Id;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageBuildOpenTest,
	"Anastasis.Sim.Village.Chantier.Ouverture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageBuildOpenTest::RunTest(const FString&)
{
	using namespace AnastasisVillageBuildTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	const FString House = Village.OpenSite(HouseType, 16, 11, true);
	const FBuilding* Site = Village.FindBuilding(House);
	if (!TestNotNull(TEXT("chantier ouvert"), Site)) return false;
	TestEqual(TEXT("progres 0"), Site->Progress, 0.0);
	TestEqual(TEXT("aucune piece"), Site->PiecesPlaced, 0);
	TestEqual(TEXT("devis bois de la maison"), Site->Materials.NeedWood, 24);
	TestEqual(TEXT("devis pierre de la maison"), Site->Materials.NeedStone, 8);
	TestEqual(TEXT("livre : stock bois"), Site->Materials.StockWood, 24);
	TestEqual(TEXT("livre : stock pierre"), Site->Materials.StockStone, 8);
	TestEqual(TEXT("un chantier actif"), Village.ActiveSites().Num(), 1);
	TestEqual(TEXT("un chantier ne compte pas comme maison"), Village.CountBuildings(HouseType), 0);

	const FString Npc = Village.SpawnNpc(10.5, 14.5, Rested());
	TestFalse(TEXT("une maison inachevee ne loge personne"), Village.AssignHome(Npc, House));

	const FString Dry = Village.OpenSite(WellType, 24, 11, false);
	const FBuilding* DrySite = Village.FindBuilding(Dry);
	if (TestNotNull(TEXT("puits a sec ouvert"), DrySite))
	{
		TestEqual(TEXT("devis du puits"), DrySite->Materials.NeedStone, 18);
		TestEqual(TEXT("rien de livre"), DrySite->Materials.StockWood + DrySite->Materials.StockStone, 0);
		TestEqual(TEXT("credit borne par la capacite du site (bois 80)"), Village.CreditSiteMaterials(Dry, 200, 0), 80);
	}
	TestTrue(TEXT("type hors catalogue porte : refuse"), Village.OpenSite(TEXT("tavern"), 30, 11, true).IsEmpty());
	TestFalse(TEXT("metier inconnu refuse"), Village.SetJob(Npc, TEXT("priest")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageBuildBuilderTest,
	"Anastasis.Sim.Village.Chantier.Batisseur",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageBuildBuilderTest::RunTest(const FString&)
{
	using namespace AnastasisVillageBuildTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	const FString House = Village.OpenSite(HouseType, 16, 11, true);
	const FString Id = SpawnBuilder(Village, 10.5, 14.5);
	const double CraftBefore = Village.FindNpc(Id)->SkillCraft;
	const double MoraleBefore = Village.FindNpc(Id)->Needs.Morale;

	double Time = Morning;
	bool bConserved = true;
	int32 LastPieces = 0;
	bool bMonotone = true;
	double FirstSwingAt = -1.0;
	double ArrivedAt = -1.0;
	TestTrue(TEXT("sain jusqu'a l'achevement"), Run(Village, Time, 180.0, [&]
	{
		const FBuilding* Site = Village.FindBuilding(House);
		const FNpc* N = Village.FindNpc(Id);
		bConserved &= SiteConserved(*Site, 24, 8);
		bMonotone &= Site->PiecesPlaced >= LastPieces;
		if (ArrivedAt < 0.0 && N->WorkSession.bActive && N->WorkSession.CraftId == B::CraftBuild) ArrivedAt = N->WorkSession.ArrivedAt;
		if (FirstSwingAt < 0.0 && Site->PiecesPlaced == 1) FirstSwingAt = Time;
		LastPieces = Site->PiecesPlaced;
		return Site->Progress >= 1.0;
	}));
	const FBuilding* Site = Village.FindBuilding(House);
	const FNpc* N = Village.FindNpc(Id);
	if (!TestEqual(TEXT("maison achevee"), Site->Progress, 1.0)) return false;
	TestTrue(TEXT("devis conserve a chaque tick"), bConserved);
	TestTrue(TEXT("les pieces ne font que monter"), bMonotone);
	TestEqual(TEXT("22 pieces"), Site->PiecesPlaced, B::PieceTotal);
	TestEqual(TEXT("tout le bois pose"), Site->Materials.ConsumedWood, 24);
	TestEqual(TEXT("toute la pierre posee"), Site->Materials.ConsumedStone, 8);
	TestEqual(TEXT("stock du site vide"), Site->Materials.StockWood + Site->Materials.StockStone, 0);
	TestEqual(TEXT("le batisseur a pose les 22"), N->PiecesPlaced, B::PieceTotal);
	TestEqual(TEXT("un achevement a son nom"), N->BuildingsCompleted, 1);
	TestEqual(TEXT("acheve par lui"), Site->CompletedById, Id);
	TestEqual(TEXT("acheve le jour 1"), Site->CompletedDay, 1);
	TestTrue(TEXT("premier coup apres l'ancrage de 0,4 s"), ArrivedAt >= 0.0 && FirstSwingAt >= ArrivedAt + B::ArriveSeconds);
	TestTrue(TEXT("la main s'est exercee (craft)"), N->SkillCraft > CraftBefore);
	TestTrue(TEXT("moral : +1 par piece, +4 a l'achevement (borne a 100)"), N->Needs.Morale >= FMath::Min(100.0, MoraleBefore + 10.0));
	TestFalse(TEXT("session close a l'achevement"), N->WorkSession.bActive);
	TestEqual(TEXT("plus aucun chantier"), Village.ActiveSites().Num(), 0);
	TestEqual(TEXT("la maison compte"), Village.CountBuildings(HouseType), 1);
	TestTrue(TEXT("et elle loge"), Village.AssignHome(Id, House));
	AddInfo(FString::Printf(TEXT("ancrage %.2f, premier coup %.2f, acheve a t=%.2f (%.1f s de chantier), craft %.4f -> %.4f"),
		ArrivedAt, FirstSwingAt, Time, Time - ArrivedAt, CraftBefore, N->SkillCraft));

	// Sans chantier, le batisseur ne reste pas plante : il repense et vaque.
	TestTrue(TEXT("sain apres"), Run(Village, Time, 6.0, [] { return false; }));
	TestTrue(TEXT("il a quitte le chantier"), Village.FindNpc(Id)->Goal != B::GoalBuild);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageBuildDryTest,
	"Anastasis.Sim.Village.Chantier.ASec",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageBuildDryTest::RunTest(const FString&)
{
	using namespace AnastasisVillageBuildTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	// Puits : 10 bois, 18 pierre. 3 + 3 livres : une unite de chaque par piece, trois pieces.
	const FString Well = Village.OpenSite(WellType, 16, 11, false);
	Village.CreditSiteMaterials(Well, 3, 3);
	const FString Id = SpawnBuilder(Village, 10.5, 14.5);

	double Time = Morning;
	bool bConserved = true;
	TestTrue(TEXT("sain a sec"), Run(Village, Time, 40.0, [&]
	{
		bConserved &= SiteConserved(*Village.FindBuilding(Well), 3, 3);
		return false;
	}));
	const FBuilding* Site = Village.FindBuilding(Well);
	TestTrue(TEXT("devis conserve"), bConserved);
	TestEqual(TEXT("trois pieces, puis plus rien"), Site->PiecesPlaced, 3);
	TestTrue(TEXT("toujours en chantier"), Site->Progress < 1.0);
	TestEqual(TEXT("stock du site vide"), Site->Materials.StockWood + Site->Materials.StockStone, 0);
	TestFalse(TEXT("le puits a sec ne sert pas"), Village.CountBuildings(WellType) > 0);

	// Le reste du devis arrive (l'hote le livre : les porteurs ne sont pas portes).
	Village.CreditSiteMaterials(Well, 7, 15);
	bConserved = true;
	TestTrue(TEXT("sain jusqu'a l'achevement"), Run(Village, Time, 120.0, [&]
	{
		bConserved &= SiteConserved(*Village.FindBuilding(Well), 10, 18);
		return Village.FindBuilding(Well)->Progress >= 1.0;
	}));
	Site = Village.FindBuilding(Well);
	TestTrue(TEXT("devis conserve, livraison comprise"), bConserved);
	TestEqual(TEXT("puits acheve"), Site->Progress, 1.0);
	TestEqual(TEXT("devis entierement pose"), Site->Materials.ConsumedWood * 100 + Site->Materials.ConsumedStone, 10 * 100 + 18);
	TestEqual(TEXT("le puits compte"), Village.CountBuildings(WellType), 1);
	AddInfo(FString::Printf(TEXT("relance et achevement a t=%.2f"), Time));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageBuildCrewTest,
	"Anastasis.Sim.Village.Chantier.Plusieurs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageBuildCrewTest::RunTest(const FString&)
{
	using namespace AnastasisVillageBuildTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	const FString Granary = Village.OpenSite(GranaryType, 16, 11, true);
	const FString A = SpawnBuilder(Village, 10.5, 14.5);
	const FString Bld = SpawnBuilder(Village, 22.5, 14.5);
	// Un sans-metier aussi : la ligne `build` vaut pour tous (85 x trait x 1).
	const FString C = Village.SpawnNpc(16.5, 17.5, Rested());
	const FString Farmer = Village.SpawnNpc(12.5, 17.5, Rested());
	TestFalse(TEXT("pas de fermier au grenier inacheve"), Village.AssignWorkplace(Farmer, G::JobFarmer, Granary));

	double Time = Morning;
	bool bConserved = true;
	TestTrue(TEXT("sain jusqu'a l'achevement"), Run(Village, Time, 180.0, [&]
	{
		bConserved &= SiteConserved(*Village.FindBuilding(Granary), 26, 16);
		return Village.FindBuilding(Granary)->Progress >= 1.0;
	}));
	const FBuilding* Site = Village.FindBuilding(Granary);
	if (!TestEqual(TEXT("grenier acheve"), Site->Progress, 1.0)) return false;
	TestTrue(TEXT("devis conserve"), bConserved);
	int32 Total = 0;
	for (const TPair<FString, int32>& W : Site->Workers) Total += W.Value;
	TestEqual(TEXT("22 pieces au registre des bras"), Total, B::PieceTotal);
	TestTrue(TEXT("plusieurs bras"), Site->Workers.Num() >= 2);
	int32 ByNpc = 0;
	for (const FNpc& N : Village.GetActors()) ByNpc += N.PiecesPlaced;
	TestEqual(TEXT("chacun compte ses pieces"), ByNpc, B::PieceTotal);
	TestTrue(TEXT("le grenier acheve embauche son fermier"), Village.AssignWorkplace(Farmer, G::JobFarmer, Granary));
	FString Crew;
	for (const TPair<FString, int32>& W : Site->Workers) Crew += FString::Printf(TEXT("%s:%d "), *W.Key, W.Value);
	AddInfo(FString::Printf(TEXT("acheve a t=%.2f par %s; sans-metier %s"), Time, *Crew, *C));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisVillageBuildScoreTest,
	"Anastasis.Sim.Village.Chantier.Score",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisVillageBuildScoreTest::RunTest(const FString&)
{
	using namespace AnastasisVillageBuildTest;
	AnastasisWorld::FWorld World = MakeFlatWorld(40, 32);
	FVillage Village;
	Village.Bind(World);
	// Sans chantier : `build` reste au plancher des buts non portes, rien n'est calcule.
	const FString Id = SpawnBuilder(Village, 10.5, 14.5);
	double Time = Morning;
	TestTrue(TEXT("sain sans chantier"), Run(Village, Time, 2.0, [&] { return Village.FindNpc(Id)->LastDecision.Time > 0.0; }));
	TestEqual(TEXT("sans chantier, pas de ligne calculee"), Village.FindNpc(Id)->LastDecision.BuildRowScore, 0.0);
	TestTrue(TEXT("sans chantier, le batisseur ne bat pas"), Village.FindNpc(Id)->Goal != B::GoalBuild);

	Village.OpenSite(HouseType, 16, 11, true);
	const double Opened = Time;
	TestTrue(TEXT("sain jusqu'a la decision"), Run(Village, Time, 6.0, [&]
	{
		const FNpc* N = Village.FindNpc(Id);
		return N->LastDecision.Time > Opened && N->LastDecision.BuildRowScore != 0.0;
	}));
	const FNpc* N = Village.FindNpc(Id);
	const FDecisionTrace& T = N->LastDecision;
	if (!TestTrue(TEXT("ligne calculee"), T.BuildRowScore != 0.0)) return false;
	// gardien (build 1), batisseur (biais 1,35 ; +18 ; rang 2 -> 13) : 85 x 1 x 1,35 + 18 + 13.
	const double Base = B::BuildScoreActiveSite(1.0, B::JobBuilder);
	TestEqual(TEXT("buildScore du batisseur"), Base, 85.0 * 1.35 + 18.0 + 13.0);
	AnastasisRhythm::FPhaseSubject Subject;
	Subject.bHasHomeOrShelter = false;
	Subject.Energy = N->Needs.Energy;
	Subject.Hunger = N->Needs.Hunger;
	const double Phase = AnastasisRhythm::PhaseBias(AnastasisRhythm::VillagePhase(AnastasisRhythm::DayFracOf(T.Time)), Subject, B::GoalBuild);
	// Ni sac, ni session au moment de la decision : la fin de tache ne pese pas.
	// `(buildScore + goalNoise(sim, 14)) * wf` (perception-explore-001 : le bruit est tire).
	const double Expected = (Base + T.RowNoise.FindRef(B::GoalBuild)) * T.WorkFactor + Phase + 0.0 + 0.0 + (N->SkillCraft - 1.0) * 8.0;
	TestTrue(TEXT("la ligne build = (buildScore + bruit) x wf + rythme + trait + competence"), FMath::Abs(T.BuildRowScore - Expected) < 1e-9);
	TestEqual(TEXT("elle gagne"), T.Winner, FString(B::GoalBuild));
	TestEqual(TEXT("cible : le chantier"), T.TargetSource, FString(TEXT("site")));
	AddInfo(FString::Printf(TEXT("ligne build %.4f (base %.2f x wf %.4f + rythme %.2f)"), T.BuildRowScore, Base, T.WorkFactor, Phase));
	return true;
}

#endif
