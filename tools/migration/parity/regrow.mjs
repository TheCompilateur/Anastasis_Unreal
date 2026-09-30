// Parite de la repousse des champs — `Simulation.regrowFieldsDaily` et
// `regrowFieldTile` (src/sim/simulation.js), `fieldSeasonRegenAmount`,
// `rotateFieldCropId` et `ensureFieldCropReady` (src/sim/fieldCrops.js).
//
// La methode de la reference est appelee telle quelle sur un `this` minimal :
// le jour, UNE tuile, et les deux crochets qu'elle touche (indexResource,
// fieldCropEvents). Chaque vecteur dit si la tuile a repousse, et ce qu'elle
// porte apres : ressource, quantite, culture.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// Positions sur la carte canonique (96 x 96), bords compris.
const POSITIONS = [[0, 0], [7, 3], [46, 46], [95, 95], [12, 80], [63, 21], [30, 30], [88, 5]];
// Jours : chaque saison, les frontieres (30/31, 120/121), le modulo 97 du tirage de culture.
const JOURS = [1, 2, 29, 30, 31, 60, 61, 90, 91, 97, 98, 120, 121, 250];
// Etats de tuile : [ressource, quantite, culture, fertilite]
const ETATS = [
  ["", 0, "fallow", 0],        // epuisee, en jachere, fertilite absente (|| 1)
  ["", 0, "fallow", 0.4],      // epuisee, peu fertile (max(1, ...))
  ["food", 12, "grain", 1],
  ["food", 36, "fruit", 1.3],  // borne au plafond 37
  ["food", 37, "greens", 1],   // au plafond : jamais
  ["food", 5, "fallow", 0.75], // du stock mais en jachere : la culture repart
];

export default {
  modules: {
    sim: "src/sim/simulation.js",
    crops: "src/sim/fieldCrops.js",
  },
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisRegrowVectors.inl"),

  cases: [
    {
      name: "RegrowDaily",
      comment: "Simulation.regrowFieldsDaily sur une seule tuile field",
      args: ["int", "int", "int", "string", "int", "string", "double"],
      ret: [
        { name: "grown", type: "bool" },
        { name: "resource", type: "string" },
        { name: "amount", type: "int" },
        { name: "crop", type: "string" },
      ],
      inputs: croiser(POSITIONS, JOURS, ETATS)
        .map(([[x, y], jour, [res, qte, culture, fert]]) => [x, y, jour, res, qte, culture, fert]),
      call: (m, [x, y, jour, res, qte, culture, fert]) => {
        const S = m.sim.Simulation;
        const tile = { x, y, type: "field", resource: res || null, amount: qte, cropId: culture, fertility: fert };
        const that = { day: jour, tiles: [tile], indexResource() {}, fieldCropEvents: 0 };
        that.regrowFieldTile = S.prototype.regrowFieldTile.bind(that);
        const grown = S.prototype.regrowFieldsDaily.call(that);
        return { grown: grown > 0, resource: tile.resource || "", amount: tile.amount | 0, crop: tile.cropId || "" };
      },
    },
    {
      name: "RegenAmount",
      comment: "fieldSeasonRegenAmount(FIELD_REGEN_PER_DAY, day), chance du jour",
      args: ["int"],
      ret: [
        { name: "amount", type: "int" },
        { name: "chance", type: "double" },
      ],
      inputs: JOURS.map((j) => [j]),
      call: (m, [jour]) => ({
        amount: m.crops.fieldSeasonRegenAmount(m.sim.Simulation.FIELD_REGEN_PER_DAY, jour),
        chance: m.crops.fieldSeasonYieldOf(jour).dailyChance,
      }),
    },
  ],
};
