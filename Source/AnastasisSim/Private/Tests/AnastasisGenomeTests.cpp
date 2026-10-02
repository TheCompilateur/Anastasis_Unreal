#include "Misc/AutomationTest.h"

#include "Core/AnastasisJsNumeric.h"
#include "Life/AnastasisConditioning.h"
#include "Life/AnastasisGenome.h"

#include <cstddef>

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Parite du genome (`src/life/genome.js`) et du conditionnement
 * (`src/life/conditioning.js`) — les deux sources des facteurs de besoins par
 * habitant (mission needs-factors-001).
 *
 * Vecteurs generes par tools/migration/gen-parity.mjs (declarations
 * parity/genome.mjs et parity/conditioning.mjs). Ne jamais corriger un vecteur
 * a la main.
 */
namespace AnastasisGenomeParity
{
	namespace Vecteurs
	{
#include "AnastasisGenomeVectors.inl"
#include "AnastasisConditioningVectors.inl"
	}

	static double GenomeFromBits(uint64 Bits)
	{
		double Value;
		FMemory::Memcpy(&Value, &Bits, sizeof(Value));
		return Value;
	}

	static uint64 GenomeToBits(double Value)
	{
		uint64 Bits;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	/**
	 * Deux NaN sont egaux, quel que soit leur signe. Mesure le 2026-10-02 :
	 * pour `clamp01(NaN)` de util.js (`Math.max(0, Math.min(1, NaN))`), la
	 * reference rend un NaN NEGATIF (`fff8...`), la ou `FMath::Min` /
	 * `FMath::Max` rendent le NaN d'entree (`7ff8...`). Le signe d'un NaN n'est observable en JS que par un
	 * `DataView` : aucune decision, aucun calcul de la simulation n'en depend.
	 * Meme regle que `Parite.Clamp01` (AnastasisParityTests.cpp, `IsNaN`).
	 */
	static bool BothNaN(double Got, uint64 ExpectedBits)
	{
		return FMath::IsNaN(Got) && FMath::IsNaN(GenomeFromBits(ExpectedBits));
	}

	static uint32 SeedOf(uint64 Bits)
	{
		return AnastasisJs::ToUint32(GenomeFromBits(Bits));
	}

	static FString TextOf(const ANSICHAR* Utf8)
	{
		return FString(UTF8_TO_TCHAR(Utf8));
	}

	// Les champs attendus d'un genome sont 24 `uint64` consecutifs (l0a .. l11b),
	// ceux d'un phenotype 18 (p0 .. p11 puis les six derivees). On les lit comme
	// un tableau ; ces assertions refusent de compiler si l'atelier change la forme.
	static_assert(offsetof(Vecteurs::FGenomeFounderVector, AttenduL11bBits) - offsetof(Vecteurs::FGenomeFounderVector, AttenduL0aBits) == 23 * sizeof(uint64), "alleles non contigus");
	static_assert(offsetof(Vecteurs::FGenomeRecombineVector, AttenduL11bBits) - offsetof(Vecteurs::FGenomeRecombineVector, AttenduL0aBits) == 23 * sizeof(uint64), "alleles non contigus");
	static_assert(offsetof(Vecteurs::FGenomePhenotypeVector, AttenduFatigueRecoveryStrainCostBits) - offsetof(Vecteurs::FGenomePhenotypeVector, AttenduP0Bits) == 17 * sizeof(uint64), "phenotype non contigu");
	static_assert(offsetof(Vecteurs::FGenomePhenotypeAbsentVector, AttenduFatigueRecoveryStrainCostBits) - offsetof(Vecteurs::FGenomePhenotypeAbsentVector, AttenduP0Bits) == 17 * sizeof(uint64), "phenotype non contigu");

	/** Les 18 valeurs d'un phenotype, dans l'ordre de la declaration. */
	static void PhenotypeValues(const AnastasisGenome::FPhenotype& P, double Out[18])
	{
		for (int32 L = 0; L < AnastasisGenome::NumLoci; ++L)
		{
			Out[L] = P.Loci[L];
		}
		Out[12] = P.HydrationLossMultiplier;
		Out[13] = P.HeatDissipationEfficiency;
		Out[14] = P.MetabolicDemandMultiplier;
		Out[15] = P.MetabolicPeakRecoveryMultiplier;
		Out[16] = P.FatigueRecoveryMultiplier;
		Out[17] = P.FatigueRecoveryStrainCost;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisGenomeParityTest,
	"Anastasis.Sim.Parite.Genome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisGenomeParityTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisGenomeParity;
	namespace V = AnastasisGenomeParity::Vecteurs;
	namespace G = AnastasisGenome;

	int32 Compared = 0;
	auto Check = [this, &Compared](const TCHAR* Case, int32 Index, const TCHAR* Field, double Got, uint64 Expected)
	{
		Compared += 1;
		if (P::GenomeToBits(Got) != Expected && !P::BothNaN(Got, Expected))
		{
			AddError(FString::Printf(TEXT("%s[%d].%s : attendu %016llx, obtenu %016llx"),
				Case, Index, Field, Expected, P::GenomeToBits(Got)));
		}
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::GenomeHashVectors); ++I)
	{
		const V::FGenomeHashVector& Vec = V::GenomeHashVectors[I];
		Check(TEXT("GenomeHash"), I, TEXT("hash"), static_cast<double>(G::HashString(P::TextOf(Vec.A0))), Vec.AttenduBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::GenomeSeedVectors); ++I)
	{
		const V::FGenomeSeedVector& Vec = V::GenomeSeedVectors[I];
		Check(TEXT("GenomeSeed"), I, TEXT("seed"),
			static_cast<double>(G::DeriveGenomeSeed(P::SeedOf(Vec.A0Bits), P::TextOf(Vec.A1))), Vec.AttenduBits);
	}

	auto CheckGenome = [&Check](const TCHAR* Case, int32 Index, const G::FGenome& Genome, uint64 SeedBits, const uint64* Alleles)
	{
		Check(Case, Index, TEXT("seed"), static_cast<double>(Genome.Seed), SeedBits);
		for (int32 L = 0; L < G::NumLoci; ++L)
		{
			Check(Case, Index, G::LocusName(static_cast<G::ELocus>(L)), Genome.Alleles[L][0], Alleles[2 * L]);
			Check(Case, Index, G::LocusName(static_cast<G::ELocus>(L)), Genome.Alleles[L][1], Alleles[2 * L + 1]);
		}
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::GenomeFounderVectors); ++I)
	{
		const V::FGenomeFounderVector& Vec = V::GenomeFounderVectors[I];
		const G::FGenome Genome = G::CreateGenome(P::SeedOf(Vec.A0Bits), P::TextOf(Vec.A1));
		CheckGenome(TEXT("GenomeFounder"), I, Genome, Vec.AttenduSeedBits, &Vec.AttenduL0aBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::GenomeRecombineVectors); ++I)
	{
		const V::FGenomeRecombineVector& Vec = V::GenomeRecombineVectors[I];
		const G::FGenome Mother = G::CreateGenome(P::SeedOf(Vec.A2Bits), P::TextOf(Vec.A3));
		const G::FGenome Father = G::CreateGenome(P::SeedOf(Vec.A4Bits), P::TextOf(Vec.A5));
		const G::FGenome Child = G::RecombineGenome(
			P::SeedOf(Vec.A0Bits), P::TextOf(Vec.A1),
			Vec.A6 != 0 ? &Mother : nullptr,
			Vec.A7 != 0 ? &Father : nullptr);
		CheckGenome(TEXT("GenomeRecombine"), I, Child, Vec.AttenduSeedBits, &Vec.AttenduL0aBits);
	}

	auto CheckPhenotype = [&Check](const TCHAR* Case, int32 Index, const G::FPhenotype& Phenotype, const uint64* Expected)
	{
		double Values[18];
		P::PhenotypeValues(Phenotype, Values);
		for (int32 K = 0; K < 18; ++K)
		{
			Check(Case, Index, *FString::Printf(TEXT("champ%d"), K), Values[K], Expected[K]);
		}
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::GenomePhenotypeVectors); ++I)
	{
		const V::FGenomePhenotypeVector& Vec = V::GenomePhenotypeVectors[I];
		const G::FGenome Genome = G::CreateGenome(P::SeedOf(Vec.A0Bits), P::TextOf(Vec.A1));
		CheckPhenotype(TEXT("GenomePhenotype"), I, G::DerivePhenotype(&Genome), &Vec.AttenduP0Bits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::GenomePhenotypeAbsentVectors); ++I)
	{
		CheckPhenotype(TEXT("GenomePhenotypeAbsent"), I, G::DerivePhenotype(nullptr), &V::GenomePhenotypeAbsentVectors[I].AttenduP0Bits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::GenomeMultipliersVectors); ++I)
	{
		const V::FGenomeMultipliersVector& Vec = V::GenomeMultipliersVectors[I];
		const double X = P::GenomeFromBits(Vec.A0Bits);
		Check(TEXT("GenomeMultipliers"), I, TEXT("hydrationLoss"), G::HydrationLossMultiplierFromRetention(X), Vec.AttenduHydrationLossMultiplierBits);
		Check(TEXT("GenomeMultipliers"), I, TEXT("heatDissipation"), G::HeatDissipationEfficiencyFromRetention(X), Vec.AttenduHeatDissipationEfficiencyBits);
		Check(TEXT("GenomeMultipliers"), I, TEXT("metabolicDemand"), G::MetabolicDemandMultiplierFromEfficiency(X), Vec.AttenduMetabolicDemandMultiplierBits);
		Check(TEXT("GenomeMultipliers"), I, TEXT("metabolicPeakRecovery"), G::MetabolicPeakRecoveryMultiplierFromEfficiency(X), Vec.AttenduMetabolicPeakRecoveryMultiplierBits);
		Check(TEXT("GenomeMultipliers"), I, TEXT("fatigueRecovery"), G::FatigueRecoveryMultiplierFromRecovery(X), Vec.AttenduFatigueRecoveryMultiplierBits);
		Check(TEXT("GenomeMultipliers"), I, TEXT("fatigueRecoveryStrain"), G::FatigueRecoveryStrainCostFromRecovery(X), Vec.AttenduFatigueRecoveryStrainCostBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::GenomeFingerprintVectors); ++I)
	{
		const V::FGenomeFingerprintVector& Vec = V::GenomeFingerprintVectors[I];
		const G::FGenome Genome = G::CreateGenome(P::SeedOf(Vec.A0Bits), P::TextOf(Vec.A1));
		const FString Got = G::GenomeFingerprint(&Genome);
		const FString Expected = P::TextOf(Vec.Attendu);
		Compared += 1;
		if (Got != Expected)
		{
			AddError(FString::Printf(TEXT("GenomeFingerprint[%d] : attendu %s, obtenu %s"), I, *Expected, *Got));
		}
	}

	// EnsureGenome : idempotent, et un genome deja la n'est jamais remplace.
	{
		TOptional<G::FGenome> Genome;
		TOptional<G::FPhenotype> Phenotype;
		G::EnsureGenome(Genome, Phenotype, 33344u, TEXT("npc-0"));
		const G::FGenome Direct = G::CreateGenome(33344u, TEXT("npc-0"));
		TestTrue(TEXT("EnsureGenome cree le fondateur"), Genome.IsSet() && Genome->Seed == Direct.Seed
			&& Genome->Alleles[11][1] == Direct.Alleles[11][1]);
		G::EnsureGenome(Genome, Phenotype, 7u, TEXT("npc-9"));
		TestTrue(TEXT("EnsureGenome ne remplace pas"), Genome->Seed == Direct.Seed);
	}

	AddInfo(FString::Printf(TEXT("%d valeurs comparees"), Compared));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAnastasisConditioningParityTest,
	"Anastasis.Sim.Parite.Conditionnement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAnastasisConditioningParityTest::RunTest(const FString& Parameters)
{
	namespace P = AnastasisGenomeParity;
	namespace V = AnastasisGenomeParity::Vecteurs;
	namespace C = AnastasisConditioning;

	int32 Compared = 0;
	auto Check = [this, &Compared](const TCHAR* Case, int32 Index, const TCHAR* Field, double Got, uint64 Expected)
	{
		Compared += 1;
		if (P::GenomeToBits(Got) != Expected && !P::BothNaN(Got, Expected))
		{
			AddError(FString::Printf(TEXT("%s[%d].%s : attendu %016llx, obtenu %016llx"),
				Case, Index, Field, Expected, P::GenomeToBits(Got)));
		}
	};

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::ConditioningTickVectors); ++I)
	{
		const V::FConditioningTickVector& Vec = V::ConditioningTickVectors[I];
		C::FConditioning Cond;
		Cond.WorkConditioning = P::GenomeFromBits(Vec.A0Bits);
		Cond.FatigueAdaptation = P::GenomeFromBits(Vec.A1Bits);
		Cond.RecoveryConditioning = P::GenomeFromBits(Vec.A2Bits);
		C::TickConditioning(Cond, P::GenomeFromBits(Vec.A3Bits), Vec.A4 != 0, Vec.A5 != 0, Vec.A6 != 0, Vec.A7 != 0);
		Check(TEXT("ConditioningTick"), I, TEXT("workConditioning"), Cond.WorkConditioning, Vec.AttenduWorkConditioningBits);
		Check(TEXT("ConditioningTick"), I, TEXT("fatigueAdaptation"), Cond.FatigueAdaptation, Vec.AttenduFatigueAdaptationBits);
		Check(TEXT("ConditioningTick"), I, TEXT("recoveryConditioning"), Cond.RecoveryConditioning, Vec.AttenduRecoveryConditioningBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::ConditioningMultipliersVectors); ++I)
	{
		const V::FConditioningMultipliersVector& Vec = V::ConditioningMultipliersVectors[I];
		const double X = P::GenomeFromBits(Vec.A0Bits);
		Check(TEXT("ConditioningMultipliers"), I, TEXT("energyFall"), C::FatigueAdaptationEnergyFallMultiplier(X), Vec.AttenduEnergyFallBits);
		Check(TEXT("ConditioningMultipliers"), I, TEXT("energyGain"), C::RecoveryConditioningEnergyGainMultiplier(X), Vec.AttenduEnergyGainBits);
	}

	for (int32 I = 0; I < UE_ARRAY_COUNT(V::ConditioningNeutralVectors); ++I)
	{
		const V::FConditioningNeutralVector& Vec = V::ConditioningNeutralVectors[I];
		const C::FConditioning Fresh;
		Check(TEXT("ConditioningNeutral"), I, TEXT("energyFall"), C::FatigueAdaptationEnergyFallMultiplier(), Vec.AttenduEnergyFallBits);
		Check(TEXT("ConditioningNeutral"), I, TEXT("energyGain"), C::RecoveryConditioningEnergyGainMultiplier(), Vec.AttenduEnergyGainBits);
		Check(TEXT("ConditioningNeutral"), I, TEXT("workConditioning"), Fresh.WorkConditioning, Vec.AttenduWorkConditioningBits);
		Check(TEXT("ConditioningNeutral"), I, TEXT("fatigueAdaptation"), Fresh.FatigueAdaptation, Vec.AttenduFatigueAdaptationBits);
		Check(TEXT("ConditioningNeutral"), I, TEXT("recoveryConditioning"), Fresh.RecoveryConditioning, Vec.AttenduRecoveryConditioningBits);
		TestEqual(TEXT("ConditioningNeutral.version"), Fresh.Version, Vec.AttenduVersion);
	}

	AddInfo(FString::Printf(TEXT("%d valeurs comparees"), Compared));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
