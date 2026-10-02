#include "Life/AnastasisGenome.h"

#include "Core/AnastasisJsNumeric.h"
#include "Core/AnastasisRng.h"

namespace AnastasisGenome
{
	namespace
	{
		/** `clamp01` LOCAL de genome.js — NaN passe, comme dans la reference. */
		double GenomeClamp01(double V)
		{
			return V < 0.0 ? 0.0 : V > 1.0 ? 1.0 : V;
		}

		/** `mutateAllele` — un tirage, deux si la mutation a lieu. */
		double MutateAllele(double Value, FAnastasisRng& Rng)
		{
			if (Rng.Next() < MutationRate)
			{
				return GenomeClamp01(Value + (Rng.Next() * 2.0 - 1.0) * MutationAmplitude);
			}
			return Value;
		}
	}

	const TCHAR* LocusName(ELocus Locus)
	{
		switch (Locus)
		{
		case ELocus::MetabolicEfficiency: return TEXT("metabolicEfficiency");
		case ELocus::HydrationRetention: return TEXT("hydrationRetention");
		case ELocus::MuscularEndurance: return TEXT("muscularEndurance");
		case ELocus::FatigueRecovery: return TEXT("fatigueRecovery");
		case ELocus::TissueRepair: return TEXT("tissueRepair");
		case ELocus::SleepEfficiency: return TEXT("sleepEfficiency");
		case ELocus::HeatTolerance: return TEXT("heatTolerance");
		case ELocus::ColdTolerance: return TEXT("coldTolerance");
		case ELocus::InfectionResistance: return TEXT("infectionResistance");
		case ELocus::InjuryResistance: return TEXT("injuryResistance");
		case ELocus::DevelopmentalRobustness: return TEXT("developmentalRobustness");
		case ELocus::FertilityInvestment: return TEXT("fertilityInvestment");
		default: return TEXT("");
		}
	}

	uint32 HashString(const FString& Value)
	{
		// `charCodeAt` lit des unites UTF-16 ; TCHAR en est une sous Windows.
		uint32 H = 2166136261u;
		for (const TCHAR Char : Value)
		{
			H ^= static_cast<uint32>(static_cast<uint16>(Char));
			H = AnastasisJs::Imul(H, 16777619u);
		}
		return H;
	}

	uint32 DeriveGenomeSeed(uint32 WorldSeed, const FString& NpcId)
	{
		return WorldSeed ^ HashString(NpcId);
	}

	FGenome CreateGenome(uint32 WorldSeed, const FString& NpcId, const TArray<FString>& ParentIds)
	{
		FGenome Genome;
		Genome.Seed = DeriveGenomeSeed(WorldSeed, NpcId);
		Genome.ParentIds = ParentIds;
		FAnastasisRng Rng(Genome.Seed);
		for (int32 Locus = 0; Locus < NumLoci; ++Locus)
		{
			// `[rng(), rng()]` : gauche puis droite.
			Genome.Alleles[Locus][0] = Rng.Next();
			Genome.Alleles[Locus][1] = Rng.Next();
		}
		return Genome;
	}

	FGenome RecombineGenome(
		uint32 WorldSeed,
		const FString& NpcId,
		const FGenome* MotherGenome,
		const FGenome* FatherGenome,
		const TArray<FString>& ParentIds,
		const FString& MotherId,
		const FString& FatherId)
	{
		FGenome Genome;
		Genome.Seed = DeriveGenomeSeed(WorldSeed, NpcId);
		Genome.ParentIds = ParentIds;
		Genome.MotherId = MotherId;
		Genome.FatherId = FatherId;
		FAnastasisRng Rng(Genome.Seed);
		for (int32 Locus = 0; Locus < NumLoci; ++Locus)
		{
			const double MotherA = MotherGenome ? MotherGenome->Alleles[Locus][0] : 0.5;
			const double MotherB = MotherGenome ? MotherGenome->Alleles[Locus][1] : 0.5;
			const double FatherA = FatherGenome ? FatherGenome->Alleles[Locus][0] : 0.5;
			const double FatherB = FatherGenome ? FatherGenome->Alleles[Locus][1] : 0.5;
			// L'ordre des tirages est celui des expressions du JS : choix maternel,
			// choix paternel, puis les deux mutations dans le litteral de tableau.
			const double FromMother = Rng.Next() < 0.5 ? MotherA : MotherB;
			const double FromFather = Rng.Next() < 0.5 ? FatherA : FatherB;
			const double MutatedMother = MutateAllele(FromMother, Rng);
			const double MutatedFather = MutateAllele(FromFather, Rng);
			Genome.Alleles[Locus][0] = MutatedMother;
			Genome.Alleles[Locus][1] = MutatedFather;
		}
		return Genome;
	}

	double HydrationLossMultiplierFromRetention(double Retention)
	{
		const double R = GenomeClamp01(Retention);
		return 1.0 + (0.5 - R) * HydrationLossAmplitude;
	}

	double HeatDissipationEfficiencyFromRetention(double Retention)
	{
		const double R = GenomeClamp01(Retention);
		return 1.0 - (R - 0.5) * HydrationLossAmplitude;
	}

	double MetabolicDemandMultiplierFromEfficiency(double Efficiency)
	{
		const double E = GenomeClamp01(Efficiency);
		return 1.0 + (0.5 - E) * MetabolicDemandAmplitude;
	}

	double MetabolicPeakRecoveryMultiplierFromEfficiency(double Efficiency)
	{
		const double E = GenomeClamp01(Efficiency);
		return 1.0 - (E - 0.5) * MetabolicDemandAmplitude;
	}

	double FatigueRecoveryMultiplierFromRecovery(double Recovery)
	{
		const double R = GenomeClamp01(Recovery);
		return 1.0 + (R - 0.5) * FatigueRecoveryAmplitude;
	}

	double FatigueRecoveryStrainCostFromRecovery(double Recovery)
	{
		const double R = GenomeClamp01(Recovery);
		return 1.0 + (R - 0.5) * FatigueRecoveryAmplitude;
	}

	FPhenotype DerivePhenotype(const FGenome* Genome)
	{
		FPhenotype Out;
		for (int32 Locus = 0; Locus < NumLoci; ++Locus)
		{
			const double A = Genome ? Genome->Alleles[Locus][0] : 0.5;
			const double B = Genome ? Genome->Alleles[Locus][1] : 0.5;
			Out.Loci[Locus] = (A + B) / 2.0;
		}
		const double Retention = Out.Locus(ELocus::HydrationRetention);
		const double Efficiency = Out.Locus(ELocus::MetabolicEfficiency);
		const double Recovery = Out.Locus(ELocus::FatigueRecovery);
		Out.HydrationLossMultiplier = HydrationLossMultiplierFromRetention(Retention);
		Out.HeatDissipationEfficiency = HeatDissipationEfficiencyFromRetention(Retention);
		Out.MetabolicDemandMultiplier = MetabolicDemandMultiplierFromEfficiency(Efficiency);
		Out.MetabolicPeakRecoveryMultiplier = MetabolicPeakRecoveryMultiplierFromEfficiency(Efficiency);
		Out.FatigueRecoveryMultiplier = FatigueRecoveryMultiplierFromRecovery(Recovery);
		Out.FatigueRecoveryStrainCost = FatigueRecoveryStrainCostFromRecovery(Recovery);
		return Out;
	}

	void EnsureGenome(
		TOptional<FGenome>& Genome,
		TOptional<FPhenotype>& Phenotype,
		uint32 WorldSeed,
		const FString& NpcId,
		const TArray<FString>& ParentIds,
		const FGenome* MotherGenome,
		const FGenome* FatherGenome,
		const FString& MotherId,
		const FString& FatherId)
	{
		if (!Genome.IsSet())
		{
			if (MotherGenome && FatherGenome)
			{
				Genome = RecombineGenome(WorldSeed, NpcId, MotherGenome, FatherGenome, ParentIds, MotherId, FatherId);
			}
			else
			{
				Genome = CreateGenome(WorldSeed, NpcId, ParentIds);
			}
		}
		if (!Phenotype.IsSet())
		{
			Phenotype = DerivePhenotype(&Genome.GetValue());
		}
	}

	FString GenomeFingerprint(const FGenome* Genome)
	{
		if (!Genome)
		{
			// Dix zeros, pas huit : c'est la reference.
			return TEXT("0000000000");
		}
		uint32 H = Genome->Seed;
		for (int32 Locus = 0; Locus < NumLoci; ++Locus)
		{
			const uint32 A = AnastasisJs::ToUint32(AnastasisJs::Round(Genome->Alleles[Locus][0] * 65535.0));
			const uint32 B = AnastasisJs::ToUint32(AnastasisJs::Round(Genome->Alleles[Locus][1] * 65535.0));
			H = AnastasisJs::Imul(H ^ A, 16777619u) ^ B;
		}
		return FString::Printf(TEXT("%08x"), H);
	}
}
