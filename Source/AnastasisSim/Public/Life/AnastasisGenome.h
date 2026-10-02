// Genome et phenotype d'un habitant — portage de `src/life/genome.js`.
//
// Couche 1 (genome) : douze loci, deux alleles chacun, fixes pour la vie.
// Couche 2 (phenotype) : derive du genome SEUL, pur. C'est le phenotype, jamais
// le genome, que la physiologie lit (`AnastasisNeeds::FNeedFactors`).
//
// --- CE QUI DECIDE --------------------------------------------------------------
//
// Le genome ne tire JAMAIS sur `sim.rng`. Il derive son propre flux mulberry32
// de `sim.seed` et de l'identifiant de l'habitant (`deriveGenomeSeed`) : aucun
// tirage n'est ajoute au flux partage. Le C++ garde cette regle — un genome se
// recalcule a l'identique depuis (graine du monde, identifiant), sans etat.
//
// L'ordre des tirages est celui du JS, et il est observable :
//   - fondateur : pour chaque locus dans l'ordre de `GENOME_LOCI`, allele 0 puis 1 ;
//   - recombinaison : pour chaque locus, choix maternel, choix paternel, puis
//     mutation de l'allele maternel (1 ou 2 tirages), puis du paternel.
//
// `clamp01` est ici la version LOCALE de genome.js (`v < 0 ? 0 : v > 1 ? 1 : v`),
// pas celle de util.js. Elles ne different que sur NaN, que les deux laissent
// passer ; la locale est copiee telle quelle pour se relire contre la reference.
//
// Ce qui n'est pas ici : `serializeGenome` / `deserializeGenome` (clones
// defensifs pour un chemin JSON — le lecteur de sauvegarde du harnais les
// remplace) et `phenotypeSummary` (affichage arrondi).

#pragma once

#include "CoreMinimal.h"

namespace AnastasisGenome
{
	inline constexpr int32 GenomeVersion = 1;

	/** `GENOME_LOCI`, dans l'ordre de la reference. L'ordre des tirages en depend. */
	enum class ELocus : int32
	{
		MetabolicEfficiency = 0, // G01
		HydrationRetention, // G02
		MuscularEndurance, // G03
		FatigueRecovery, // G04
		TissueRepair, // G05
		SleepEfficiency, // G06
		HeatTolerance, // G07
		ColdTolerance, // G08
		InfectionResistance, // G09
		InjuryResistance, // G10
		DevelopmentalRobustness, // G11
		FertilityInvestment, // G12
		Count
	};

	inline constexpr int32 NumLoci = static_cast<int32>(ELocus::Count);

	/** Nom JS du locus (`GENOME_LOCI[i]`) — pour le lecteur de sauvegarde. */
	ANASTASISSIM_API const TCHAR* LocusName(ELocus Locus);

	/** Taux et amplitude de mutation de la recombinaison (Lot D). */
	inline constexpr double MutationRate = 0.02;
	inline constexpr double MutationAmplitude = 0.05;

	/** Amplitudes des derivees physiologiques : +/-25 % au plus. */
	inline constexpr double HydrationLossAmplitude = 0.5;
	inline constexpr double MetabolicDemandAmplitude = 0.5;
	inline constexpr double FatigueRecoveryAmplitude = 0.5;

	struct FGenome
	{
		int32 Version = GenomeVersion;
		uint32 Seed = 0u;
		TArray<FString> ParentIds;
		/** Vide = `null`. Provenance seulement, jamais un poids genetique. */
		FString MotherId;
		FString FatherId;
		/** `loci[name] = [a, b]`, rangees par `ELocus`. */
		double Alleles[NumLoci][2] = {};

		double Allele(ELocus Locus, int32 Index) const { return Alleles[static_cast<int32>(Locus)][Index]; }
	};

	/** `derivePhenotype(genome)` — douze moyennes d'alleles et six derivees. */
	struct FPhenotype
	{
		double Loci[NumLoci] = {};
		double HydrationLossMultiplier = 1.0;
		/** Represente, NON CONSOMME (GAMEPLAY_COUPLING::DEFERRED). */
		double HeatDissipationEfficiency = 1.0;
		double MetabolicDemandMultiplier = 1.0;
		/** Represente, NON CONSOMME. */
		double MetabolicPeakRecoveryMultiplier = 1.0;
		double FatigueRecoveryMultiplier = 1.0;
		/** Represente, NON CONSOMME. */
		double FatigueRecoveryStrainCost = 1.0;

		double Locus(ELocus L) const { return Loci[static_cast<int32>(L)]; }
	};

	/** `hashString` — FNV-1a 32 sur les unites UTF-16 de la chaine. */
	ANASTASISSIM_API uint32 HashString(const FString& Value);

	/** `deriveGenomeSeed(worldSeed, npcId)`. */
	ANASTASISSIM_API uint32 DeriveGenomeSeed(uint32 WorldSeed, const FString& NpcId);

	/** `createGenome` — genome fondateur, flux propre a l'habitant. */
	ANASTASISSIM_API FGenome CreateGenome(uint32 WorldSeed, const FString& NpcId, const TArray<FString>& ParentIds = TArray<FString>());

	/**
	 * `recombineGenome` — un allele de chaque parent par locus, mutation rare.
	 * Un parent nullptr compte comme `[0.5, 0.5]` a chaque locus, comme le JS.
	 */
	ANASTASISSIM_API FGenome RecombineGenome(
		uint32 WorldSeed,
		const FString& NpcId,
		const FGenome* MotherGenome,
		const FGenome* FatherGenome,
		const TArray<FString>& ParentIds = TArray<FString>(),
		const FString& MotherId = FString(),
		const FString& FatherId = FString());

	/** `derivePhenotype`. nullptr = genome absent : tous les loci a 0,5. */
	ANASTASISSIM_API FPhenotype DerivePhenotype(const FGenome* Genome);

	ANASTASISSIM_API double HydrationLossMultiplierFromRetention(double Retention);
	ANASTASISSIM_API double HeatDissipationEfficiencyFromRetention(double Retention);
	ANASTASISSIM_API double MetabolicDemandMultiplierFromEfficiency(double Efficiency);
	ANASTASISSIM_API double MetabolicPeakRecoveryMultiplierFromEfficiency(double Efficiency);
	ANASTASISSIM_API double FatigueRecoveryMultiplierFromRecovery(double Recovery);
	ANASTASISSIM_API double FatigueRecoveryStrainCostFromRecovery(double Recovery);

	/**
	 * `ensureGenome(npc, worldSeed, options)` — idempotent : cree le genome s'il
	 * manque (recombine si les DEUX parents sont fournis, fondateur sinon), puis
	 * le phenotype s'il manque. Ne remplace jamais un genome existant.
	 */
	ANASTASISSIM_API void EnsureGenome(
		TOptional<FGenome>& Genome,
		TOptional<FPhenotype>& Phenotype,
		uint32 WorldSeed,
		const FString& NpcId,
		const TArray<FString>& ParentIds = TArray<FString>(),
		const FGenome* MotherGenome = nullptr,
		const FGenome* FatherGenome = nullptr,
		const FString& MotherId = FString(),
		const FString& FatherId = FString());

	/** `genomeFingerprint` — empreinte de debug, 8 chiffres hexa (10 zeros si absent). */
	ANASTASISSIM_API FString GenomeFingerprint(const FGenome* Genome);
}
