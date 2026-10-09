// ma-cabane-001 (ecart n°56) -- le joueur leve seul sa cabane, une piece pour un dormeur ; elle est a lui, il y dort,
// et personne d'autre n'y entre.

#include "Misc/AutomationTest.h"
#include "Life/AnastasisNeeds.h"
#include "Village/AnastasisVillage.h"
#include "Work/AnastasisBuild.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AnastasisCabinTest
{
	using namespace AnastasisVillage;

	constexpr double Dt = 1.0 / 60.0;

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
			W.Tiles[I].Type = AnastasisWorld::ETileType::Grass;
			W.Tiles[I].Alt = 0.5;
			W.Tiles[I].Wetness = 0.3;
		}
		return W;
	}

	AnastasisNeeds::FNeeds Rested()
	{
		AnastasisNeeds::FNeeds N;
		N.Hunger = 5.0;
		N.Thirst = 5.0;
		N.Energy = 95.0;
		return N;
	}

	/** Avance jusqu'a `Seconds`, ou jusqu'a ce que `Done` dise oui. */
	template <typename FDone>
	bool RunUntil(FVillage& V, double& Time, double Seconds, FDone Done)
	{
		const int32 Ticks = FMath::CeilToInt32(Seconds / Dt);
		for (int32 I = 0; I < Ticks; ++I)
		{
			Time += Dt;
			V.UpdateActors(Time, Dt);
			if (Done()) return true;
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCabinBuiltAloneTest, "Anastasis.Sim.Cabane.Seul",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCabinBuiltAloneTest::RunTest(const FString&)
{
	using namespace AnastasisCabinTest;
	const AnastasisWorld::FWorld W = MakeWorld();
	FVillage V;
	V.Bind(W);
	V.SetSettlement(20.5, 20.5);
	V.AddBuilding(WellType, 20, 20, 1.0, 1);
	// Un voisin sans toit, qui ne batit pas : il ne doit ni poser une piece ni dormir chez le joueur.
	const FString Neighbour = V.SpawnNpc(18.5, 24.5, Rested());
	const FString Player = V.ArriveAsPlayer(21.5, 23.5);
	if (!TestFalse(TEXT("le joueur arrive"), Player.IsEmpty())) return false;
	V.FindNpcMutable(Player)->Needs = Rested();

	const FString SiteId = V.PlayerBuildHome(TEXT("la maison du nouveau"));
	if (!TestFalse(TEXT("il trace sa cabane"), SiteId.IsEmpty())) return false;
	const FBuilding* Site = V.FindBuilding(SiteId);
	TestEqual(TEXT("une cabane, pas une maison"), Site->Type, FString(CabinType));
	TestEqual(TEXT("elle est a lui des le trace"), Site->Owner, Player);
	TestEqual(TEXT("lui seul y bat"), Site->AllowedBuilders, TArray<FString>{ Player });
	TestEqual(TEXT("un tiers d'une maison en bois"), Site->Materials.NeedWood, 8);
	TestEqual(TEXT("un quart en pierre"), Site->Materials.NeedStone, 2);
	TestEqual(TEXT("un seul dormeur"), HousingOfType(CabinType), 1);
	TestTrue(TEXT("pas deux chantiers a la fois"), V.PlayerBuildHome(TEXT("la maison du nouveau")).IsEmpty());

	// Il choisit de batir ; seul, sans attendre d'aidant pour le toit (une maison de famille l'attendrait a 50 %).
	double Time = 0.0;
	TestTrue(TEXT("il choisit de batir"), V.ChoosePlayerGoal(AnastasisBuild::GoalBuild));
	const bool bDone = RunUntil(V, Time, 240.0, [&] { return V.FindBuilding(SiteId)->IsCompleted(); });
	if (!TestTrue(FString::Printf(TEXT("la cabane est debout (%d pieces sur %d)"), V.FindBuilding(SiteId)->PiecesPlaced,
		AnastasisBuild::PieceTotal), bDone)) return false;
	Site = V.FindBuilding(SiteId);
	TestEqual(TEXT("un seul batisseur"), Site->Workers.Num(), 1);
	TestTrue(TEXT("et c'est lui"), Site->Workers.Num() == 1 && Site->Workers[0].Key == Player);
	TestEqual(TEXT("il y habite"), V.FindNpc(Player)->HomeId, SiteId);
	TestEqual(TEXT("elle reste a lui"), Site->Owner, Player);

	// Le soir des abris : le voisin sans toit ne recoit pas la cabane.
	V.AssignSheltersDaily();
	TestTrue(TEXT("le voisin n'y est pas loge"), V.FindNpc(Neighbour)->ShelterId.IsEmpty() && V.FindNpc(Neighbour)->HomeId.IsEmpty());

	// Il choisit de dormir : il entre dans sa cabane.
	TestTrue(TEXT("il choisit de dormir"), V.ChoosePlayerGoal(GoalRest));
	const bool bInside = RunUntil(V, Time, 60.0, [&]
	{
		const FNpc* Me = V.FindNpc(Player);
		return Me->Inside.bActive && Me->Inside.BuildingId == SiteId;
	});
	TestTrue(TEXT("il passe la porte de sa cabane"), bInside);
	TestEqual(TEXT("seul dedans"), V.InsideOf(SiteId).Num(), 1);
	return true;
}

#endif
