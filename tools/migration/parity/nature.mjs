// Parite de la nature des habitants — `src/life/nature.js` (mission lifestyle-decision-001).
//
// Ce qui se prouve ici : ce que la decision lit de la nature, apres `ensureNature` (attributs
// bornes a [0,72 ; 1,38], `|| 1` pour un attribut nul ou non numerique, qualites et defauts
// inconnus filtres, deux qualites et un defaut au plus) :
//   - `natureGoalBias(npc, goal)` sur tous les buts de la table adulte et du jeu ;
//   - `natureWorkFactor(npc)` ;
//   - `natureStickBonus(npc, goal)`.
// Le tirage d'une nature absente (`rollNature`, flux de secours) n'est pas porte (ecart n°10).

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

const QUALITES = ["", "genereux", "patient", "courageux", "loyal", "curieux", "soigneux", "eloquent", "sobre",
  "travailleur", "bienveillant", "inconnue"];
const DEFAUTS = ["", "avare", "colerique", "peureux", "paresseux", "vaniteux", "mefiant", "gourmand", "tetu", "inconnu"];
const BUTS = ["eat", "eatTogether", "rest", "relax", "relieve", "drink", "gatherWood", "gatherStone", "gatherFood",
  "helpFarm", "sell", "buy", "build", "craft", "maintain", "deliver", "fetchInput", "haulJob", "aidHousehold",
  "visitFamily", "explore", "socialize", "confront", "shelterRain", "closeWorkplace", "play", "study", "apprentice",
  "observer"];
// corps, esprit, coeur : moyens, extremes, hors bornes, nul (`|| 1`).
const ATTRIBUTS = [[1, 1, 1], [1.0071351182973012, 1.3092582467710598, 0.8163028867123648], [0.72, 1.38, 1.2],
  [0.5, 2, 1.1], [0, 0.95, 1.37]];
// Paires de qualites (deux au plus apres filtrage : la troisieme est retiree).
const PAIRES = [["", ""], ["loyal", ""], ["patient", "travailleur"], ["soigneux", "inconnue"], ["eloquent", "sobre"],
  ["genereux", "bienveillant"], ["courageux", "curieux"]];

const nature = ([corps, esprit, coeur], [q1, q2], f) => ({
  corps, esprit, coeur,
  qualities: [q1, q2].filter(Boolean),
  flaws: f ? [f] : [],
});

export default {
  module: "src/life/nature.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisNatureVectors.inl"),

  cases: [
    {
      name: "NatureGoalBias",
      comment: "natureGoalBias(npc, goal) apres ensureNature",
      // corps, esprit, coeur, qualite 1, qualite 2, defaut, but
      args: ["double", "double", "double", "string", "string", "string", "string"],
      ret: "double",
      inputs: [
        ...croiser(ATTRIBUTS, PAIRES, ["", "gourmand", "tetu", "paresseux"], BUTS)
          .map(([[c, e, h], [q1, q2], f, but]) => [c, e, h, q1, q2, f, but]),
        ...croiser(QUALITES, DEFAUTS, ["socialize", "build", "craft", "explore", "sell", "confront"])
          .map(([q, f, but]) => [1, 1, 1, q, "", f, but]),
      ],
      call: (mod, [c, e, h, q1, q2, f, but]) => mod.natureGoalBias({ nature: nature([c, e, h], [q1, q2], f) }, but),
    },
    {
      name: "NatureWorkFactor",
      comment: "natureWorkFactor(npc) : 0,92 + corps x 0,08 + travail des qualites et defauts, borne",
      args: ["double", "string", "string", "string"],
      ret: "double",
      inputs: croiser(ATTRIBUTS.map((a) => a[0]), PAIRES, DEFAUTS).map(([c, [q1, q2], f]) => [c, q1, q2, f]),
      call: (mod, [c, q1, q2, f]) => mod.natureWorkFactor({ nature: nature([c, 1, 1], [q1, q2], f) }),
    },
    {
      name: "NatureStickBonus",
      comment: "natureStickBonus(npc, goal) : tetu 0,18 ; patient, loyal, travailleur 0,08",
      args: ["string", "string", "string"],
      ret: "double",
      inputs: croiser(PAIRES, DEFAUTS).map(([[q1, q2], f]) => [q1, q2, f]),
      call: (mod, [q1, q2, f]) => mod.natureStickBonus({ nature: nature([1, 1, 1], [q1, q2], f) }, "build"),
    },
  ],
};
