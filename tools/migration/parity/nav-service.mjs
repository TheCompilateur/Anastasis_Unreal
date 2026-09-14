// Parite du service de navigation — `src/sim/navService.js`.
//
// Seule la moitie CACHE est portee. L'autre moitie — la file budgetee — ne
// manipule que des acteurs (`requestPath`, `applyPathToActor`,
// `processNavQueue`) et attend que les acteurs existent.
//
// Le cache n'est pas une optimisation, et c'est pour cela qu'il passe avant la
// file. `src/sim/save.js` l'ecrit noir sur blanc a propos du champ `navCache`:
//
//   « un acteur repris avec un cache vide peut donc recevoir un chemin
//     different — meme cout, memes regles — de celui qu'une partie continue
//     aurait servi depuis son propre cache encore chaud. Sans ce champ, la
//     trajectoire divergeait des la premiere requete de chemin post-reprise. »
//
// Un cache qui ne sert pas les memes chemins fait donc marcher les habitants
// ailleurs. Il appartient a la causalite, pas a la performance.
//
// Ce fichier declare la part SCALAIRE. Les operations a etat — ordre
// d'insertion, eviction, recherche exacte puis par zone — sont dans
// `gen-nav-cache-vectors.mjs`: un scenario ne se met pas en table de scalaires.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// Vitesses de jeu reelles, plus les bords: le plan de pas bascule a s > 1 et
// plafonne a SIM_MAX_STEP_MULT = 10.
const VITESSES = [0, 0.5, 1, 1.000001, 1.5, 2, 3, 5, 8, 10, 12, 20, NaN];

export default {
  module: "src/sim/navService.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisNavServiceVectors.inl"),

  cases: [
    {
      name: "NavStepMult",
      comment: "navStepMultForSpeed — stepDt / SIM_FIXED_DT",
      args: ["double"],
      ret: "double",
      inputs: VITESSES.map((v) => [v]),
      call: (mod, [v]) => mod.navStepMultForSpeed(v),
    },

    {
      name: "NavCacheTtl",
      comment: "navCacheTtlForSpeed — TTL sim qui suit le fat-step",
      args: ["double"],
      ret: "double",
      inputs: VITESSES.map((v) => [v]),
      call: (mod, [v]) => mod.navCacheTtlForSpeed(v),
    },

    {
      name: "NavSweepInterval",
      comment: "navCacheSweepIntervalForSpeed",
      args: ["double"],
      ret: "double",
      inputs: VITESSES.map((v) => [v]),
      call: (mod, [v]) => mod.navCacheSweepIntervalForSpeed(v),
    },

    {
      // `Math.round(6 * stepMult ** 0.75)`. Deux pieges d'un coup: `**` passe
      // par `pow`, dont l'implementation peut differer d'un ulp entre V8 et la
      // libm; et `Math.round` arrondit les demis vers +Infini, la ou `round` du
      // C les arrondit en s'eloignant de zero. Sur des entrees positives les
      // deux coincident, mais la regle doit etre ecrite, pas supposee.
      name: "NavPathBudget",
      comment: "pathBudgetForSpeed — un NOMBRE DE CALCULS, jamais une duree",
      args: ["double"],
      ret: "int",
      inputs: VITESSES.map((v) => [v]),
      call: (mod, [v]) => mod.pathBudgetForSpeed(v).maxCalcs,
    },

    {
      // La cle de cache est une CHAINE, et sa forme exacte fait partie du
      // contrat: deux portages qui la composeraient differemment ne
      // partageraient jamais une entree.
      //
      // `Math.floor` et non une troncature: sur une coordonnee negative les
      // deux different, et `Math.floor(start.x / NAV_ZONE)` descend d'une zone
      // entiere du mauvais cote.
      name: "NavCacheKey",
      comment: "cacheKeyFor — exacte et par zone",
      args: ["double", "double", "double", "double", "int", "bool"],
      ret: "string",
      inputs: [
        [0, 0, 0, 0, 0, false],
        [0, 0, 0, 0, 0, true],
        [3.7, 9.2, 40.1, 12.9, 0, false],
        [3.7, 9.2, 40.1, 12.9, 0, true],
        [3.7, 9.2, 40.1, 12.9, 7, false],
        [3.7, 9.2, 40.1, 12.9, 7, true],
        [8, 8, 1, 1, 3, true],
        [7.999999, 7.999999, 1, 1, 3, true],
        [16, 0, 1, 1, 3, true],
        // Coordonnees negatives: le cas ou floor et troncature divergent.
        [-0.5, -0.5, 4, 4, 1, false],
        [-0.5, -0.5, 4, 4, 1, true],
        [-8.5, -16.5, -2.5, -3.5, 2, true],
        [-1, -1, -1, -1, 0, true],
        // Grande version, grandes coordonnees.
        [1023.4, 2047.6, 4095.1, 8191.9, 123456, false],
      ],
      call: (mod, [sx, sy, tx, ty, version, zone]) =>
        mod.cacheKeyFor({ x: sx, y: sy }, { x: tx, y: ty }, version, zone),
    },
  ],
};
