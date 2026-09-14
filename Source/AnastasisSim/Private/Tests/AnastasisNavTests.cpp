#include "Misc/AutomationTest.h"

#include "World/AnastasisNavGrid.h"
#include "World/AnastasisPathfinding.h"
#include "World/AnastasisWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Parite de la navigation contre `src/sim/navGrid.js` et `src/sim/pathfinding.js`.
 *
 * PORTAGE.md pose l'exigence: « l'ordre d'exploration de l'A* doit etre
 * identique : deux chemins de meme cout, et les PNJ ne prennent pas la meme
 * rue ». Ces vecteurs comparent donc la suite COMPLETE des points, chacun par
 * son motif binaire — pas le cout total, pas la longueur.
 *
 * Generes par tools/migration/gen-nav-vectors.mjs. Ne jamais corriger un
 * vecteur a la main: soit le portage a devie, soit la reference a change et il
 * faut regenerer.
 */
namespace AnastasisNavParity
{
	static double FromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(double));
		return Value;
	}

	static uint64 ToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(double));
		return Bits;
	}

	#include "AnastasisNavVectors.inl"

	/** Le monde de test, construit comme le generateur de vecteurs le construit. */
	struct FNavFixture
	{
		AnastasisWorld::FWorld World;
		AnastasisNav::FNavGrid Grid;

		explicit FNavFixture(uint32 Seed)
		{
			World = AnastasisWorld::GenerateWorld(Seed, NavWorldW, NavWorldH);
			AnastasisNav::InitFromWorld(Grid, World);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisNavCostTest,
	"Anastasis.Sim.Parite.NavCout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisNavCostTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisNavParity;

	// La grille de couts se verifie AVANT les chemins. Sans cela, un desaccord
	// de chemin laisserait planer le doute: mauvais A*, ou mauvaise grille ?
	//
	// Ces vecteurs portent aussi la preuve du stockage f32: le multiplicateur
	// de foret y vaut 0x3ff99999a0000000, pas les bits du double 1.6. Un
	// portage qui stockerait en double echouerait ici, et c'est voulu.
	for (const FNavCostVector& Vector : NavCostVectors)
	{
		const FNavFixture Fixture(Vector.Seed);

		const double MoveCost = AnastasisNav::MoveCostAt(Fixture.Grid, Vector.X, Vector.Y);
		if (ToBits(MoveCost) != Vector.MoveCostBits)
		{
			AddError(FString::Printf(
				TEXT("moveCostAt seed=%u (%d,%d): attendu %016llx, obtenu %016llx"),
				Vector.Seed, Vector.X, Vector.Y, Vector.MoveCostBits, ToBits(MoveCost)));
		}

		const double Traversal = AnastasisNav::TileTraversalCost(Fixture.Grid, Vector.X, Vector.Y, 10.0);
		if (ToBits(Traversal) != Vector.TraversalBits)
		{
			AddError(FString::Printf(
				TEXT("tileTraversalCost seed=%u (%d,%d): attendu %016llx, obtenu %016llx"),
				Vector.Seed, Vector.X, Vector.Y, Vector.TraversalBits, ToBits(Traversal)));
		}

		const bool bFootBlocked = AnastasisNav::FootBlockedAt(Fixture.Grid, Fixture.World, Vector.X, Vector.Y);
		TestEqual(
			*FString::Printf(TEXT("footBlockedAt seed=%u (%d,%d)"), Vector.Seed, Vector.X, Vector.Y),
			bFootBlocked, Vector.FootBlocked != 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisNavPathTest,
	"Anastasis.Sim.Parite.NavChemin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisNavPathTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisNavParity;

	// Les mondes sont construits une fois par graine: la generation coute plus
	// cher que tous les chemins reunis.
	TMap<uint32, TSharedPtr<FNavFixture>> Fixtures;
	for (const uint32 Seed : NavSeeds)
	{
		Fixtures.Add(Seed, MakeShared<FNavFixture>(Seed));
	}

	int32 CheminsTrouves = 0;
	for (const FNavPathVector& Vector : NavPathVectors)
	{
		const TSharedPtr<FNavFixture>& Fixture = Fixtures[Vector.Seed];
		const AnastasisPath::FWorldNavSource Source(Fixture->Grid, Fixture->World);

		AnastasisPath::FOptions Options;
		Options.MaxCost = FromBits(Vector.MaxCostBits);
		Options.MaxExpanded = Vector.MaxExpanded;
		Options.bAllowBlockedStart = Vector.AllowBlockedStart != 0;
		Options.bAllowBlockedTarget = Vector.AllowBlockedTarget != 0;

		const AnastasisPath::FPoint Start{ FromBits(Vector.StartXBits), FromBits(Vector.StartYBits) };
		const AnastasisPath::FPoint Target{ FromBits(Vector.TargetXBits), FromBits(Vector.TargetYBits) };

		TArray<AnastasisPath::FPoint> Path;
		const bool bFound = AnastasisPath::FindPath(Source, Start, Target, Options, Path);

		if (bFound != (Vector.Found != 0))
		{
			AddError(FString::Printf(
				TEXT("%s: la reference %s un chemin, le portage %s"),
				Vector.Name,
				Vector.Found != 0 ? TEXT("trouve") : TEXT("ne trouve pas"),
				bFound ? TEXT("en trouve un") : TEXT("n'en trouve pas")));
			continue;
		}
		if (!bFound)
		{
			continue;
		}
		CheminsTrouves += 1;

		if (Path.Num() != Vector.PointCount)
		{
			AddError(FString::Printf(
				TEXT("%s: %d points attendus, %d obtenus"),
				Vector.Name, Vector.PointCount, Path.Num()));
			continue;
		}

		for (int32 Index = 0; Index < Path.Num(); ++Index)
		{
			const uint64 ExpectedX = NavPathPoints[Vector.FirstPoint + Index][0];
			const uint64 ExpectedY = NavPathPoints[Vector.FirstPoint + Index][1];
			if (ToBits(Path[Index].X) != ExpectedX || ToBits(Path[Index].Y) != ExpectedY)
			{
				AddError(FString::Printf(
					TEXT("%s: point %d attendu (%016llx,%016llx), obtenu (%016llx,%016llx)"),
					Vector.Name, Index, ExpectedX, ExpectedY,
					ToBits(Path[Index].X), ToBits(Path[Index].Y)));
				break;
			}
		}
	}

	// Garde-fou sur les vecteurs eux-memes: une batterie ou tous les chemins
	// echoueraient passerait au vert sans rien prouver.
	TestTrue(TEXT("la batterie contient de vrais chemins"), CheminsTrouves >= 8);

	return true;
}

/**
 * Deux proprietes que les vecteurs ne montrent pas d'eux-memes, et sur
 * lesquelles repose la reproductibilite des chemins.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisNavInvariantsTest,
	"Anastasis.Sim.Parite.NavInvariants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisNavInvariantsTest::RunTest(const FString& Parameters)
{
	using namespace AnastasisNavParity;

	/** Une grille plate posee a la main: pas de monde, pas de bruit. */
	class FPlainSource final : public AnastasisPath::INavSource
	{
	public:
		FPlainSource(int32 InW, int32 InH) : W(InW), H(InH) { Blocked.AddZeroed(W * H); }
		virtual int32 GetWidth() const override { return W; }
		virtual int32 GetHeight() const override { return H; }
		virtual bool IsBlocked(int32 X, int32 Y) const override
		{
			if (X < 0 || Y < 0 || X >= W || Y >= H) return true;
			return Blocked[Y * W + X] != 0;
		}
		virtual double TraversalCost(int32 X, int32 Y, double BaseCost) const override
		{
			return IsBlocked(X, Y) ? AnastasisNav::Infinity : FMath::Max(3.0, BaseCost);
		}
		void Block(int32 X, int32 Y) { Blocked[Y * W + X] = 1; }

	private:
		int32 W;
		int32 H;
		TArray<uint8> Blocked;
	};

	// 1. Depart = arrivee: succes, chemin vide. La difference avec un echec
	//    compte pour l'appelant — « deja arrive » n'est pas « nulle part ou
	//    aller ».
	{
		FPlainSource Source(8, 8);
		TArray<AnastasisPath::FPoint> Path;
		const bool bFound = AnastasisPath::FindPath(
			Source, { 3.2, 3.9 }, { 3.7, 3.1 }, AnastasisPath::FOptions{}, Path);
		TestTrue(TEXT("sur place: succes"), bFound);
		TestEqual(TEXT("sur place: chemin vide"), Path.Num(), 0);
	}

	// 2. Une diagonale ne coupe pas entre deux obstacles adjacents. Sans cette
	//    regle, un PNJ traverserait l'angle de deux murs qui se touchent.
	{
		FPlainSource Source(8, 8);
		Source.Block(4, 3);
		Source.Block(3, 4);
		TArray<AnastasisPath::FPoint> Path;
		const bool bFound = AnastasisPath::FindPath(
			Source, { 3.5, 3.5 }, { 4.5, 4.5 }, AnastasisPath::FOptions{}, Path);
		if (TestTrue(TEXT("le coin se contourne"), bFound))
		{
			TestTrue(TEXT("et il ne se coupe pas en un seul pas"), Path.Num() > 1);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
