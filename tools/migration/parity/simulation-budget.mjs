// Parite du directeur de budget — `src/sim/simulationBudget.js`.
//
// Seul le NOYAU CAUSAL est porte, et c'est la reference elle-meme qui trace la
// ligne. Son en-tete porte une loi datee du 10/08/2026:
//
//   « LA MACHINE PEUT CHANGER LA VITESSE A LAQUELLE LE MONDE EST CALCULE.
//     ELLE NE DOIT PAS CHANGER LE MONDE QUI EST CALCULE. »
//
// avec la preuve du bug qu'elle a corrige: graine 12345, jour 4, 6 594 ticks des
// deux cotes, population 6 contre 5 et tresor 328 contre 321 — parce que
// `budget.pressure` etait une moyenne mobile du temps mur MESURE.
//
// Depuis, `pressure` n'accepte qu'une source deterministe. Tout ce qui touche au
// chronometre — `ema`, `noteSimulationBudgetFrame`, `frameWallMs`, `simMs`,
// `observedTier`, le HUD — est declare observation seule et ne decide plus rien.
// Ce sont donc des candidats a la couche de PRESENTATION Unreal, pas au module
// de simulation, et ils ne sont pas portes.
//
// Reste ici ce qui decide vraiment: le palier de pression, les multiplicateurs
// de budget, la bande de cadence d'un PNJ et son intervalle.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// Pressions choisies pour tomber DE PART ET D'AUTRE des seuils 0.38 et 0.72,
// et dessus. Un seuil teste seulement en son milieu ne prouve pas ou il est.
const PRESSIONS = [0, 0.1, 0.37, 0.38, 0.379999, 0.5, 0.71, 0.72, 0.719999, 0.9, 1, 1.5, -0.2, NaN];

const BANDES = ["near", "medium", "far", "invisible", "inconnue"];

export default {
  module: "src/sim/simulationBudget.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisBudgetVectors.inl"),

  cases: [
    {
      name: "PressureTier",
      comment: "simulationPressureTier",
      args: ["double"],
      ret: "string",
      inputs: PRESSIONS.map((p) => [p]),
      call: (mod, [p]) => mod.simulationPressureTier(p),
    },

    {
      name: "BudgetMultipliers",
      comment: "simulationBudgetMultipliers",
      args: ["double"],
      ret: [
        { name: "pathBudgetMul", type: "double" },
        { name: "animalTickMul", type: "double" },
        { name: "transportTickMul", type: "double" },
      ],
      inputs: PRESSIONS.map((p) => [p]),
      call: (mod, [p]) => mod.simulationBudgetMultipliers({ pressure: p }),
    },

    {
      name: "BandInterval",
      comment: "npcSimulationIntervalForBand",
      args: ["double", "string"],
      ret: "double",
      inputs: croiser(PRESSIONS, BANDES),
      call: (mod, [p, band]) => mod.npcSimulationIntervalForBand({ pressure: p }, band),
    },

    {
      // `classifyNpcSimulationBand` passe par `Math.hypot`, que V8 implemente
      // par mise a l'echelle sur le max puis sommation de Kahan. Un
      // `sqrt(dx*dx+dy*dy)` donnerait un resultat voisin — et ferait basculer
      // un PNJ de bande au bord d'un rayon. D'ou des entrees posees
      // exactement sur les rayons.
      name: "NpcBand",
      comment: "classifyNpcSimulationBand (passe par Math.hypot)",
      args: ["double", "double", "double", "double", "double", "bool"],
      ret: "string",
      inputs: [
        // pression, vue x/y, npc x/y, inside
        [0, 0, 0, 0, 0, false],
        [0, 0, 0, 18, 0, false],
        [0, 0, 0, 17.999999, 0, false],
        [0, 0, 0, 18.000001, 0, false],
        [0, 0, 0, 42, 0, false],
        [0, 0, 0, 84, 0, false],
        [0, 0, 0, 84.000001, 0, false],
        [0, 0, 0, 12.7279220613578, 12.7279220613578, false],
        [0, 0, 0, 3, 4, false],
        [0, 10.5, -7.25, 21.75, 13.5, false],
        [0, 0, 0, 5, 5, true],
        [0.5, 0, 0, 18, 0, false],
        [0.5, 0, 0, 12.96, 0, false],
        [1, 0, 0, 12.96, 0, false],
        [1, 0, 0, 68.88, 0, false],
        [0.72, 0, 0, 42, 0, false],
        [0.38, 0, 0, 84, 0, false],
        [0, -1e6, 1e6, 1e6, -1e6, false],

        // LES TROIS ENTREES QUI PROUVENT QUE `Math.hypot` COMPTE.
        //
        // Cherchees exprès: sur chacune, `Math.hypot` rend EXACTEMENT le rayon
        // et `sqrt(dx*dx+dy*dy)` rend le double juste au-dessus. Le test
        // `d <= rayon` bascule donc, et le PNJ change de bande — near contre
        // medium, medium contre far, far contre invisible.
        //
        // Sans elles, remplacer `JsHypot` par un `sqrt` naif ne cassait AUCUN
        // vecteur: la claim de l'en-tete etait affirmee, pas testee. Verifie le
        // 2026-09-14 en comparant les deux formules sur toute la batterie —
        // deux entrees differaient en bits, aucune ne changeait de bande.
        [0, 0, 0, 17.99999999653022, 0.00035342917350614206, false],
        [0, 0, 0, 41.99999999190385, 0.0008246680715143314, false],
        [0, 0, 0, 83.9999999838077, 0.0016493361430286629, false],
      ],
      call: (mod, [pressure, vx, vy, nx, ny, inside]) =>
        mod.classifyNpcSimulationBand(
          { pressure, view: { x: vx, y: vy } },
          { x: nx, y: ny, inside },
        ),
    },
  ],
};
