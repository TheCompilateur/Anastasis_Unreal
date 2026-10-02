// Parite du genome — `src/life/genome.js` (mission needs-factors-001).
//
// Le genome nourrit les facteurs de besoins par son phenotype. Ce qui se prouve
// ici : le hachage de l'identifiant, la graine propre a l'habitant, les tirages
// du fondateur et de la recombinaison (ordre compris, mutations comprises), le
// phenotype et ses six derivees, l'empreinte de debug.
//
// Les genomes se comparent ALLELE PAR ALLELE, par leur motif binaire : un
// tirage de trop ou de moins decale tout le reste du flux, et ce decalage
// n'apparait qu'au locus suivant.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

const LOCI = [
  "metabolicEfficiency", "hydrationRetention", "muscularEndurance", "fatigueRecovery",
  "tissueRepair", "sleepEfficiency", "heatTolerance", "coldTolerance",
  "infectionResistance", "injuryResistance", "developmentalRobustness", "fertilityInvestment",
];

// Champs a plat : l0a, l0b ... l11a, l11b, dans l'ordre de GENOME_LOCI.
const ALLELES = LOCI.flatMap((_, i) => [`l${i}a`, `l${i}b`]).map((name) => ({ name, type: "double" }));
const aPlat = (genome) => {
  const out = { seed: genome.seed >>> 0 };
  LOCI.forEach((name, i) => {
    out[`l${i}a`] = genome.loci[name][0];
    out[`l${i}b`] = genome.loci[name][1];
  });
  return out;
};

// Graines du monde : celle du scenario endurance, des petites, la borne 2^32-1.
const GRAINES = [33344, 0, 7, 1204, 4294967295, 2654435761];
const IDS = ["npc-0", "npc-1", "npc-4", "npc-17", "npc-123", "", "Ἀναστάσιος"];

const DERIVEES = [
  "hydrationLossMultiplier", "heatDissipationEfficiency", "metabolicDemandMultiplier",
  "metabolicPeakRecoveryMultiplier", "fatigueRecoveryMultiplier", "fatigueRecoveryStrainCost",
];
const PHENOTYPE = [...LOCI.map((_, i) => `p${i}`), ...DERIVEES].map((name) => ({ name, type: "double" }));
const phenotypeAPlat = (p) => {
  const out = {};
  LOCI.forEach((name, i) => { out[`p${i}`] = p[name]; });
  for (const d of DERIVEES) out[d] = p[d];
  return out;
};

// Bornes de clamp01 et median, plus un NaN qui doit traverser.
const VALEURS = [-0.5, -0, 0, 1e-12, 0.1 + 0.2, 0.25, 0.5, 0.5000000000000001, 0.75, 0.9999999999999999, 1, 1.5, NaN];

// Couples de parents : graines et identifiants melanges, un parent absent.
const ENFANTS = [];
for (let k = 0; k < 40; k += 1) {
  ENFANTS.push([GRAINES[k % GRAINES.length], `npc-${100 + k}`, 33344 + k, `npc-${k}`, 7 + 3 * k, `npc-${50 + k}`, k % 7 !== 3, k % 11 !== 5]);
}

export default {
  module: "src/life/genome.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisGenomeVectors.inl"),

  cases: [
    {
      name: "GenomeHash",
      comment: "hashString (non exportee) lue par deriveGenomeSeed(0, id) — 0 ^ h = h",
      args: ["string"],
      ret: "double",
      inputs: [...IDS, "a", "building-3", "npc-9999"].map((id) => [id]),
      call: (mod, [id]) => mod.deriveGenomeSeed(0, id),
    },
    {
      name: "GenomeSeed",
      comment: "deriveGenomeSeed(worldSeed, npcId)",
      args: ["double", "string"],
      ret: "double",
      inputs: croiser(GRAINES, IDS),
      call: (mod, [seed, id]) => mod.deriveGenomeSeed(seed, id),
    },
    {
      name: "GenomeFounder",
      comment: "createGenome — 24 alleles dans l'ordre de GENOME_LOCI",
      args: ["double", "string"],
      ret: [{ name: "seed", type: "double" }, ...ALLELES],
      inputs: croiser(GRAINES, IDS),
      call: (mod, [seed, id]) => aPlat(mod.createGenome(seed, id)),
    },
    {
      name: "GenomeRecombine",
      comment: "recombineGenome — parents fondateurs, parent absent = [0.5, 0.5]",
      args: ["double", "string", "double", "string", "double", "string", "bool", "bool"],
      ret: [{ name: "seed", type: "double" }, ...ALLELES],
      inputs: ENFANTS,
      call: (mod, [seed, id, mSeed, mId, pSeed, pId, avecMere, avecPere]) => aPlat(mod.recombineGenome(
        seed, id,
        avecMere ? mod.createGenome(mSeed, mId) : null,
        avecPere ? mod.createGenome(pSeed, pId) : null,
      )),
    },
    {
      name: "GenomePhenotype",
      comment: "derivePhenotype(createGenome(seed, id))",
      args: ["double", "string"],
      ret: PHENOTYPE,
      inputs: croiser(GRAINES, IDS),
      call: (mod, [seed, id]) => phenotypeAPlat(mod.derivePhenotype(mod.createGenome(seed, id))),
    },
    {
      name: "GenomePhenotypeAbsent",
      comment: "derivePhenotype(null) — tous les loci a 0,5",
      args: [],
      ret: PHENOTYPE,
      inputs: [[]],
      call: (mod) => phenotypeAPlat(mod.derivePhenotype(null)),
    },
    {
      name: "GenomeMultipliers",
      comment: "les six derivees physiologiques, bornes de clamp01 et NaN",
      args: ["double"],
      ret: DERIVEES.map((name) => ({ name, type: "double" })),
      inputs: VALEURS.map((v) => [v]),
      call: (mod, [v]) => ({
        hydrationLossMultiplier: mod.hydrationLossMultiplierFromRetention(v),
        heatDissipationEfficiency: mod.heatDissipationEfficiencyFromRetention(v),
        metabolicDemandMultiplier: mod.metabolicDemandMultiplierFromEfficiency(v),
        metabolicPeakRecoveryMultiplier: mod.metabolicPeakRecoveryMultiplierFromEfficiency(v),
        fatigueRecoveryMultiplier: mod.fatigueRecoveryMultiplierFromRecovery(v),
        fatigueRecoveryStrainCost: mod.fatigueRecoveryStrainCostFromRecovery(v),
      }),
    },
    {
      name: "GenomeFingerprint",
      comment: "genomeFingerprint(createGenome(seed, id))",
      args: ["double", "string"],
      ret: "string",
      inputs: croiser(GRAINES, IDS),
      call: (mod, [seed, id]) => mod.genomeFingerprint(mod.createGenome(seed, id)),
    },
  ],
};
