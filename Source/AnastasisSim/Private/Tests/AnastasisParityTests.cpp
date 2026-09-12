#include "Misc/AutomationTest.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisRng.h"
#include "Core/AnastasisSimClock.h"
#include "Core/AnastasisSimMath.h"
#include "Core/AnastasisSpatialGrid.h"
#include "World/AnastasisWorld.h"
#include "World/AnastasisWorldArchetype.h"
#include "World/AnastasisWorldNoise.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Parite bit a bit entre le socle C++ et le simulateur JS de reference.
 *
 * Ces tests ne verifient pas que le portage est "a peu pres juste": ils
 * verifient qu'il rend EXACTEMENT les memes doubles. C'est le seul niveau
 * d'exigence utile ici — une sauvegarde JS doit pouvoir se rejouer dans Unreal
 * et donner le meme village, et un bug reproduit avec une seed doit se
 * reproduire des deux cotes. Un ecart d'un ulp sur un hash suffit a faire
 * basculer un Math.floor(hash * n) de categorie: arbres, roches et herbes
 * changent de place.
 *
 * Les vecteurs sont generes par tools/unreal/gen-parity-vectors.mjs, execute
 * dans le depot JS. Ne jamais les corriger a la main pour faire passer un test:
 * si un vecteur ne passe plus, soit le portage a devie, soit la reference JS a
 * change et il faut regenerer.
 */
namespace AnastasisParity
{
	/** Relit un motif binaire en double, sans passer par un litteral decimal. */
	static double FromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(double));
		return Value;
	}

	/** Motif binaire d'un double, pour comparer y compris NaN et -0. */
	static uint64 ToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(double));
		return Bits;
	}

	#include "AnastasisParityVectors.inl"
}

namespace
{
	/**
	 * Egalite stricte sur les bits. Deux NaN au meme motif sont egaux ici, ce
	 * que `==` refuserait — et on a justement des vecteurs NaN a verifier.
	 */
	bool BitsEqual(double Actual, uint64 ExpectedBits)
	{
		return AnastasisParity::ToBits(Actual) == ExpectedBits;
	}

	FString BitsMismatch(const TCHAR* Label, double Actual, uint64 ExpectedBits)
	{
		return FString::Printf(
			TEXT("%s: obtenu %.17g (0x%016llx), attendu %.17g (0x%016llx)"),
			Label,
			Actual,
			AnastasisParity::ToBits(Actual),
			AnastasisParity::FromBits(ExpectedBits),
			ExpectedBits);
	}
}

// --- RNG --------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisRngParityTest,
	"Anastasis.Sim.Parite.Rng",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisRngParityTest::RunTest(const FString&)
{
	using namespace AnastasisParity;

	FAnastasisRng Rng;
	for (const FRngVector& Vector : RngVectors)
	{
		// Chaque groupe de vecteurs repart du seed a son premier tirage.
		if (Vector.DrawCount == 1u)
		{
			Rng.SetState(Vector.Seed);
		}

		const double Drawn = Rng.Next();
		if (!BitsEqual(Drawn, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(
				*FString::Printf(TEXT("Rng(seed=%u) tirage %u"), Vector.Seed, Vector.DrawCount),
				Drawn,
				Vector.ExpectedBits));
		}

		// L'etat compte autant que la valeur: c'est lui qu'une sauvegarde
		// restaure. Un etat divergent donne une partie qui repart juste puis
		// derive au tirage suivant.
		TestEqual(
			*FString::Printf(TEXT("Etat Rng(seed=%u) apres %u tirages"), Vector.Seed, Vector.DrawCount),
			Rng.GetState(),
			Vector.ExpectedState);
	}

	// Le RNG de secours doit avoir le seed convenu, sinon les chemins de
	// reparation divergent silencieusement entre les deux moteurs.
	FAnastasisRng FallbackProbe(AnastasisFallbackRngSeed);
	TestEqual(TEXT("Seed du RNG de secours"), FallbackProbe.GetState(), 0x9e3779b1u);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisRngHelpersParityTest,
	"Anastasis.Sim.Parite.RngHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisRngHelpersParityTest::RunTest(const FString&)
{
	using namespace AnastasisParity;

	// Le generateur emet les vecteurs par groupes contigus: 4 tirages par seed
	// pour Range, 6 pour IntRange. On resynchronise sur ce decoupage plutot que
	// sur le seed, qui peut se repeter d'un groupe a l'autre.
	constexpr int32 RangeDrawsPerSeed = 4;
	{
		FAnastasisRng Rng;
		int32 DrawIndex = 0;
		for (const FRngRangeVector& Vector : RngRangeVectors)
		{
			if (DrawIndex == 0)
			{
				Rng.SetState(Vector.Seed);
			}
			const double Min = FromBits(Vector.MinBits);
			const double Max = FromBits(Vector.MaxBits);
			const double Value = Rng.Range(Min, Max);
			if (!BitsEqual(Value, Vector.ExpectedBits))
			{
				AddError(BitsMismatch(
					*FString::Printf(TEXT("Rng::Range(seed=%u, %g, %g)"), Vector.Seed, Min, Max),
					Value,
					Vector.ExpectedBits));
			}
			DrawIndex = (DrawIndex + 1) % RangeDrawsPerSeed;
		}
	}

	constexpr int32 IntDrawsPerSeed = 6;
	{
		FAnastasisRng Rng;
		int32 DrawIndex = 0;
		for (const FRngIntVector& Vector : RngIntVectors)
		{
			if (DrawIndex == 0)
			{
				Rng.SetState(Vector.Seed);
			}
			const int32 Value = Rng.IntRange(Vector.Min, Vector.Max);
			TestEqual(
				*FString::Printf(TEXT("Rng::IntRange(seed=%u, %d, %d)"), Vector.Seed, Vector.Min, Vector.Max),
				Value,
				Vector.Expected);
			DrawIndex = (DrawIndex + 1) % IntDrawsPerSeed;
		}
	}

	return true;
}

// --- Fonctions mathematiques ------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisHashParityTest,
	"Anastasis.Sim.Parite.Hash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisHashParityTest::RunTest(const FString&)
{
	using namespace AnastasisParity;

	for (const FHash2dVector& Vector : Hash2dVectors)
	{
		const double X = FromBits(Vector.XBits);
		const double Y = FromBits(Vector.YBits);
		const double Value = AnastasisMath::Hash2d(X, Y, Vector.Channel);
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(
				*FString::Printf(TEXT("Hash2d(%.17g, %.17g, %d)"), X, Y, Vector.Channel),
				Value,
				Vector.ExpectedBits));
		}
	}

	for (const FHashTextVector& Vector : HashTextVectors)
	{
		const double Value = AnastasisMath::HashText(FString(Vector.Text), Vector.Channel);
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(
				*FString::Printf(TEXT("HashText(\"%s\", %d)"), Vector.Text, Vector.Channel),
				Value,
				Vector.ExpectedBits));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisMathParityTest,
	"Anastasis.Sim.Parite.Math",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisMathParityTest::RunTest(const FString&)
{
	using namespace AnastasisParity;

	for (const FSmoothstepVector& Vector : SmoothstepVectors)
	{
		const double Edge0 = FromBits(Vector.Edge0Bits);
		const double Edge1 = FromBits(Vector.Edge1Bits);
		const double Input = FromBits(Vector.ValueBits);
		const double Value = AnastasisMath::Smoothstep(Edge0, Edge1, Input);
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(
				*FString::Printf(TEXT("Smoothstep(%.17g, %.17g, %.17g)"), Edge0, Edge1, Input),
				Value,
				Vector.ExpectedBits));
		}
	}

	for (const FUnaryVector& Vector : Smoothstep01Vectors)
	{
		const double Value = AnastasisMath::Smoothstep01(FromBits(Vector.InputBits));
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(TEXT("Smoothstep01"), Value, Vector.ExpectedBits));
		}
	}

	for (const FUnaryVector& Vector : Clamp01CoerceVectors)
	{
		const double Value = AnastasisMath::Clamp01Coerce(FromBits(Vector.InputBits));
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(TEXT("Clamp01Coerce"), Value, Vector.ExpectedBits));
		}
	}

	for (const FLerpVector& Vector : LerpVectors)
	{
		const double Value = AnastasisMath::Lerp(FromBits(Vector.ABits), FromBits(Vector.BBits), FromBits(Vector.TBits));
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(TEXT("Lerp"), Value, Vector.ExpectedBits));
		}
	}

	// Dist passe par la reimplementation de Math.hypot de V8. Si ce bloc casse,
	// c'est la premiere piste: std::hypot et sqrt(dx*dx+dy*dy) donnent des
	// resultats voisins mais differents.
	for (const FDistVector& Vector : DistVectors)
	{
		const double AX = FromBits(Vector.AXBits);
		const double AY = FromBits(Vector.AYBits);
		const double BX = FromBits(Vector.BXBits);
		const double BY = FromBits(Vector.BYBits);
		const double Value = AnastasisMath::Dist(AX, AY, BX, BY);
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(
				*FString::Printf(TEXT("Dist((%.17g, %.17g), (%.17g, %.17g))"), AX, AY, BX, BY),
				Value,
				Vector.ExpectedBits));
		}
	}

	// Clamp01 doit PROPAGER NaN la ou Clamp01Coerce le ramene a 0. Les deux
	// existent pour cette raison exacte; les confondre masque des bugs.
	TestTrue(
		TEXT("Clamp01 propage NaN"),
		FMath::IsNaN(AnastasisMath::Clamp01(FromBits(0x7ff8000000000000ull))));
	TestEqual(TEXT("Clamp01Coerce ramene NaN a 0"),
		AnastasisMath::Clamp01Coerce(FromBits(0x7ff8000000000000ull)), 0.0);

	return true;
}

// --- Semantique JS ----------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisJsNumericTest,
	"Anastasis.Sim.Parite.SemantiqueJs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisJsNumericTest::RunTest(const FString&)
{
	using namespace AnastasisJs;

	// Cas de reference d'ECMA-262 ToUint32, verifies contre `x >>> 0` en JS.
	TestEqual(TEXT("ToUint32(0)"), ToUint32(0.0), 0u);
	TestEqual(TEXT("ToUint32(-1)"), ToUint32(-1.0), 4294967295u);
	TestEqual(TEXT("ToUint32(3.9)"), ToUint32(3.9), 3u);
	TestEqual(TEXT("ToUint32(-3.9)"), ToUint32(-3.9), 4294967293u);
	TestEqual(TEXT("ToUint32(2^32)"), ToUint32(4294967296.0), 0u);
	TestEqual(TEXT("ToUint32(2^32 + 5)"), ToUint32(4294967301.0), 5u);
	TestEqual(TEXT("ToUint32(1e21)"), ToUint32(1e21), 3162799616u);

	// C'est le cas qui piege les portages entiers: la partie fractionnaire est
	// tronquee APRES la multiplication en double, pas avant.
	TestEqual(TEXT("ToUint32(3.5 * 374761393)"), ToUint32(3.5 * 374761393.0), 1311664875u);

	TestEqual(TEXT("Imul enroule modulo 2^32"), Imul(0xffffffffu, 3u), 4294967293u);

	// `Number(v) || 1`: 0 est falsy en JS et retombe donc sur le defaut. Sans
	// cette subtilite, StepPlan(0) diviserait par zero au lieu de rendre 1x.
	TestEqual(TEXT("NumberOr(0, 1)"), NumberOr(0.0, 1.0), 1.0);
	TestEqual(TEXT("NumberOr(2, 1)"), NumberOr(2.0, 1.0), 2.0);

	return true;
}

// --- Horloge de simulation --------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSimClockParityTest,
	"Anastasis.Sim.Parite.Horloge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSimClockParityTest::RunTest(const FString&)
{
	using namespace AnastasisParity;

	for (const FStepPlanVector& Vector : StepPlanVectors)
	{
		const double Scale = FromBits(Vector.ScaleBits);
		const AnastasisSimClock::FStepPlan Plan = AnastasisSimClock::StepPlan(Scale);
		const FString Label = FString::Printf(TEXT("StepPlan(%.17g)"), Scale);

		if (!BitsEqual(Plan.StepDt, Vector.StepDtBits))
		{
			AddError(BitsMismatch(*(Label + TEXT(" StepDt")), Plan.StepDt, Vector.StepDtBits));
		}
		TestEqual(*(Label + TEXT(" TargetSteps")), Plan.TargetSteps, Vector.TargetSteps);
		if (!BitsEqual(Plan.TimePerFrame, Vector.TimePerFrameBits))
		{
			AddError(BitsMismatch(*(Label + TEXT(" TimePerFrame")), Plan.TimePerFrame, Vector.TimePerFrameBits));
		}
		if (!BitsEqual(Plan.TimeGain, Vector.TimeGainBits))
		{
			AddError(BitsMismatch(*(Label + TEXT(" TimeGain")), Plan.TimeGain, Vector.TimeGainBits));
		}
	}

	for (const FWallBudgetVector& Vector : WallBudgetVectors)
	{
		const double Scale = FromBits(Vector.ScaleBits);
		const double WallFrameMs = FromBits(Vector.WallFrameMsBits);
		const double Value = AnastasisSimClock::WallBudgetMs(Scale, WallFrameMs, Vector.bWorldBuilding);
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(
				*FString::Printf(TEXT("WallBudgetMs(%.17g, %.17g, %s)"),
					Scale, WallFrameMs, Vector.bWorldBuilding ? TEXT("true") : TEXT("false")),
				Value,
				Vector.ExpectedBits));
		}
	}

	for (const FUnaryVector& Vector : KeepLagVectors)
	{
		const double Value = AnastasisSimClock::KeepLagSeconds(FromBits(Vector.InputBits));
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(TEXT("KeepLagSeconds"), Value, Vector.ExpectedBits));
		}
	}

	for (const FFrameDeltaVector& Vector : FrameDeltaVectors)
	{
		const double WallMs = FromBits(Vector.WallMsBits);
		const AnastasisSimClock::FFrameDelta Delta = AnastasisSimClock::FrameDelta(WallMs);
		if (!BitsEqual(Delta.Dt, Vector.DtBits))
		{
			AddError(BitsMismatch(
				*FString::Printf(TEXT("FrameDelta(%.17g).Dt"), WallMs), Delta.Dt, Vector.DtBits));
		}
		if (!BitsEqual(Delta.LastWallFrameMs, Vector.LastWallFrameMsBits))
		{
			AddError(BitsMismatch(
				*FString::Printf(TEXT("FrameDelta(%.17g).LastWallFrameMs"), WallMs),
				Delta.LastWallFrameMs,
				Vector.LastWallFrameMsBits));
		}
	}

	// Invariant de conception, pas seulement de parite: quelle que soit la
	// vitesse demandee, une frame ne doit jamais planifier plus de 2 ticks.
	// C'est ce qui empeche la sim de figer le rendu quand la charge monte.
	for (const double Scale : { 1.0, 2.0, 5.0, 10.0, 50.0, 1000.0 })
	{
		TestTrue(
			*FString::Printf(TEXT("StepPlan(%.17g) plafonne a 2 ticks"), Scale),
			AnastasisSimClock::StepPlan(Scale).TargetSteps <= 2);
	}

	return true;
}

// --- Grille spatiale --------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisSpatialGridTest,
	"Anastasis.Sim.Parite.GrilleSpatiale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisSpatialGridTest::RunTest(const FString&)
{
	using namespace AnastasisParity;

	for (const FCellKeyVector& Vector : CellKeyVectors)
	{
		TestEqual(
			*FString::Printf(TEXT("PackCellKey(%d, %d)"), Vector.CellX, Vector.CellY),
			AnastasisSpatialGrid::PackCellKey(Vector.CellX, Vector.CellY),
			Vector.Expected);
	}

	// La grille doit rendre les memes voisins qu'un scan exhaustif — sinon elle
	// n'accelere rien, elle change la simulation.
	TArray<FVector2D> Points;
	FAnastasisRng Rng(20260906u);
	for (int32 Index = 0; Index < 500; ++Index)
	{
		Points.Add(FVector2D(Rng.Range(-40.0, 40.0), Rng.Range(-40.0, 40.0)));
	}
	// Deux points inexploitables: ils doivent etre ignores, pas bucketises en
	// case 0 (ce qui surchargerait toutes les requetes autour de l'origine).
	Points.Add(FVector2D(FromBits(0x7ff8000000000000ull), 3.0));
	Points.Add(FVector2D(4.0, FromBits(0x7ff0000000000000ull)));

	AnastasisSpatialGrid::FGrid Grid;
	Grid.Rebuild(Points, AnastasisSpatialGrid::DefaultCellSize);

	const double Radius = 6.0;
	for (const FVector2D& Center : { FVector2D(0.0, 0.0), FVector2D(-31.5, 12.25), FVector2D(39.0, -39.0) })
	{
		TSet<int32> FromGrid;
		Grid.ForEachNear(Center.X, Center.Y, Radius, [&](int32 Index)
		{
			if (AnastasisMath::Dist(Points[Index].X, Points[Index].Y, Center.X, Center.Y) <= Radius)
			{
				FromGrid.Add(Index);
			}
		});

		TSet<int32> FromScan;
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			if (!FMath::IsFinite(Points[Index].X) || !FMath::IsFinite(Points[Index].Y))
			{
				continue;
			}
			if (AnastasisMath::Dist(Points[Index].X, Points[Index].Y, Center.X, Center.Y) <= Radius)
			{
				FromScan.Add(Index);
			}
		}

		TestEqual(
			*FString::Printf(TEXT("Voisins trouves autour de (%g, %g)"), Center.X, Center.Y),
			FromGrid.Num(),
			FromScan.Num());
		TestTrue(
			*FString::Printf(TEXT("Grille == scan exhaustif autour de (%g, %g)"), Center.X, Center.Y),
			FromGrid.Includes(FromScan));
	}

	// Un rebuild remet a zero sans fuir: meme jeu de points, meme total.
	int32 TotalBefore = 0;
	for (const TPair<uint32, TArray<int32>>& Pair : Grid.Cells)
	{
		TotalBefore += Pair.Value.Num();
	}
	Grid.Rebuild(Points, AnastasisSpatialGrid::DefaultCellSize);
	int32 TotalAfter = 0;
	for (const TPair<uint32, TArray<int32>>& Pair : Grid.Cells)
	{
		TotalAfter += Pair.Value.Num();
	}
	TestEqual(TEXT("Le rebuild ne duplique pas les acteurs"), TotalAfter, TotalBefore);
	TestEqual(TEXT("Les deux points non finis sont ignores"), TotalAfter, Points.Num() - 2);

	return true;
}

// --- Worldgen ---------------------------------------------------------------

namespace
{
	uint32 TypeFingerprint(const TArray<AnastasisWorld::FTile>& Tiles)
	{
		uint32 Hash = 2166136261u;
		for (const AnastasisWorld::FTile& Tile : Tiles)
		{
			Hash ^= static_cast<uint32>(Tile.Type);
			Hash = AnastasisJs::Imul(Hash, 16777619u);
		}
		return Hash;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisLibmParityTest,
	"Anastasis.Sim.Parite.Libm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisLibmParityTest::RunTest(const FString&)
{
	using namespace AnastasisParity;

	for (const FLibmUnaryVector& Vector : SinVectors)
	{
		const double Value = AnastasisJs::Sin(FromBits(Vector.InputBits));
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(TEXT("Math.sin"), Value, Vector.ExpectedBits));
		}
	}
	for (const FLibmUnaryVector& Vector : LogVectors)
	{
		const double Value = AnastasisJs::Log(FromBits(Vector.InputBits));
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(TEXT("Math.log"), Value, Vector.ExpectedBits));
		}
	}
	for (const FLibmUnaryVector& Vector : TanhVectors)
	{
		const double Value = AnastasisJs::Tanh(FromBits(Vector.InputBits));
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(TEXT("Math.tanh"), Value, Vector.ExpectedBits));
		}
	}
	for (const FLibmPowVector& Vector : PowVectors)
	{
		const double Value = AnastasisJs::Pow(FromBits(Vector.BaseBits), FromBits(Vector.ExpBits));
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(TEXT("Math.pow"), Value, Vector.ExpectedBits));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisArchetypeParityTest,
	"Anastasis.Sim.Parite.Archetype",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisArchetypeParityTest::RunTest(const FString&)
{
	using namespace AnastasisParity;

	for (const FArchetypeVector& Vector : ArchetypeVectors)
	{
		const AnastasisWorldArchetype::FKnobs Arch = AnastasisWorldArchetype::Resolve(Vector.Seed);
		TestEqual(
			*FString::Printf(TEXT("PickId(%u)"), Vector.Seed),
			static_cast<uint32>(AnastasisWorldArchetype::PickId(Vector.Seed)),
			Vector.Id);
		if (!BitsEqual(Arch.SeaLowFrac, Vector.SeaLowFracBits))
		{
			AddError(BitsMismatch(*FString::Printf(TEXT("seaLowFrac seed=%u"), Vector.Seed), Arch.SeaLowFrac, Vector.SeaLowFracBits));
		}
		if (!BitsEqual(Arch.ForestT, Vector.ForestTBits))
		{
			AddError(BitsMismatch(*FString::Printf(TEXT("forestT seed=%u"), Vector.Seed), Arch.ForestT, Vector.ForestTBits));
		}
		if (!BitsEqual(Arch.FieldT, Vector.FieldTBits))
		{
			AddError(BitsMismatch(*FString::Printf(TEXT("fieldT seed=%u"), Vector.Seed), Arch.FieldT, Vector.FieldTBits));
		}
		if (!BitsEqual(Arch.MoistBias, Vector.MoistBiasBits))
		{
			AddError(BitsMismatch(*FString::Printf(TEXT("moistBias seed=%u"), Vector.Seed), Arch.MoistBias, Vector.MoistBiasBits));
		}
		TestEqual(*FString::Printf(TEXT("clearRadius seed=%u"), Vector.Seed), Arch.ClearRadius, Vector.ClearRadius);
		TestEqual(*FString::Printf(TEXT("extent.w seed=%u"), Vector.Seed), Arch.ExtentW, Vector.ExtentW);
		TestEqual(*FString::Printf(TEXT("extent.h seed=%u"), Vector.Seed), Arch.ExtentH, Vector.ExtentH);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisFbmParityTest,
	"Anastasis.Sim.Parite.Fbm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisFbmParityTest::RunTest(const FString&)
{
	using namespace AnastasisParity;

	for (const FFbmVector& Vector : FbmVectors)
	{
		const double X = FromBits(Vector.XBits);
		const double Y = FromBits(Vector.YBits);
		const double Seed = FromBits(Vector.SeedBits);
		const double Value = AnastasisWorldNoise::Fbm(X, Y, Seed);
		if (!BitsEqual(Value, Vector.ExpectedBits))
		{
			AddError(BitsMismatch(
				*FString::Printf(TEXT("fbm(%.17g, %.17g, %.17g)"), X, Y, Seed),
				Value,
				Vector.ExpectedBits));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisCropParityTest,
	"Anastasis.Sim.Parite.Culture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisCropParityTest::RunTest(const FString&)
{
	using namespace AnastasisParity;

	for (const FCropVector& Vector : CropVectors)
	{
		TestEqual(
			*FString::Printf(TEXT("pickFieldCropId(%d, %d, %u)"), Vector.X, Vector.Y, Vector.Salt),
			static_cast<uint32>(AnastasisWorld::PickFieldCropId(Vector.X, Vector.Y, Vector.Salt)),
			Vector.Crop);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisWorldParityTest,
	"Anastasis.Sim.Parite.Monde",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisWorldParityTest::RunTest(const FString&)
{
	using namespace AnastasisParity;

	for (const FWorldMapVector& Vector : WorldMapVectors)
	{
		const AnastasisWorld::FWorld World = AnastasisWorld::GenerateWorld(Vector.Seed, Vector.W, Vector.H);
		uint32 Counts[AnastasisWorld::TileTypeCount] = {};
		for (const AnastasisWorld::FTile& Tile : World.Tiles)
		{
			Counts[static_cast<uint8>(Tile.Type)] += 1;
		}
		for (int32 Type = 0; Type < AnastasisWorld::TileTypeCount; ++Type)
		{
			TestEqual(
				*FString::Printf(TEXT("Compte type %d seed=%u %dx%d"), Type, Vector.Seed, Vector.W, Vector.H),
				Counts[Type],
				Vector.Counts[Type]);
		}
		TestEqual(
			*FString::Printf(TEXT("Empreinte types seed=%u %dx%d"), Vector.Seed, Vector.W, Vector.H),
			TypeFingerprint(World.Tiles),
			Vector.Fingerprint);
		TestEqual(
			*FString::Printf(TEXT("Origine type seed=%u"), Vector.Seed),
			static_cast<uint32>(World.Tiles[0].Type),
			Vector.OriginType);
		TestEqual(
			*FString::Printf(TEXT("Origine amount seed=%u"), Vector.Seed),
			World.Tiles[0].Amount,
			Vector.OriginAmount);
		if (!BitsEqual(World.Tiles[0].Alt, Vector.OriginAltBits))
		{
			AddError(BitsMismatch(
				*FString::Printf(TEXT("Origine alt seed=%u"), Vector.Seed),
				World.Tiles[0].Alt,
				Vector.OriginAltBits));
		}
		const uint32 ForestCap = static_cast<uint32>(AnastasisJs::Floor(World.Tiles.Num() * AnastasisWorld::ForestTileMaxFrac));
		TestEqual(
			*FString::Printf(TEXT("Plafond foret seed=%u"), Vector.Seed),
			ForestCap,
			Vector.ForestCap);
	}

	for (const FWorldSampleVector& Vector : WorldSampleVectors)
	{
		const AnastasisWorld::FWorld World = AnastasisWorld::GenerateWorld(Vector.Seed, Vector.W, Vector.H);
		const AnastasisWorld::FTile& Tile = World.Tiles[Vector.Y * Vector.W + Vector.X];
		const FString Label = FString::Printf(
			TEXT("tuile(%d,%d) %dx%d seed=%u"), Vector.X, Vector.Y, Vector.W, Vector.H, Vector.Seed);
		TestEqual(*(Label + TEXT(" type")), static_cast<uint32>(Tile.Type), Vector.Type);
		TestEqual(*(Label + TEXT(" amount")), Tile.Amount, Vector.Amount);
		TestEqual(*(Label + TEXT(" crop")), static_cast<uint32>(Tile.CropId), Vector.Crop);
		if (!BitsEqual(Tile.Alt, Vector.AltBits))
		{
			AddError(BitsMismatch(*(Label + TEXT(" alt")), Tile.Alt, Vector.AltBits));
		}
		if (!BitsEqual(Tile.Shade, Vector.ShadeBits))
		{
			AddError(BitsMismatch(*(Label + TEXT(" shade")), Tile.Shade, Vector.ShadeBits));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
