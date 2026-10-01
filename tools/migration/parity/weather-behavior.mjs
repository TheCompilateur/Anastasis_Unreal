// Parite du comportement meteo des habitants — `src/ai/weatherGoalBias.js`.
//
// Ce que la pluie, la neige, le vent et la saison font a la table de decision d'un
// habitant, et quand un orage le fait lacher son travail pour un toit. La meteo
// elle-meme est prouvee a part (weather.mjs) ; ici on prouve ce que l'habitant en fait.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// Tous les buts de la table adulte, plus quelques noms hors table (eatTogether est dedans,
// play / buy aussi) et le but vide (la garde `!goal`).
const BUTS = ["eat", "eatTogether", "rest", "relax", "relieve", "drink", "gatherWood", "gatherStone",
  "gatherFood", "helpFarm", "sell", "buy", "build", "craft", "maintain", "deliver", "fetchInput",
  "haulJob", "aidHousehold", "visitFamily", "explore", "socialize", "confront", "shelterRain",
  "closeWorkplace", "play", ""];

// Metiers : sans metier, d'exterieur (le fermier du portage), d'exterieur hors bloc vitesse
// (bucheron), garde, et le colon du portage.
const METIERS = ["settler", "farmer", "woodcutter", "guard", ""];

// Ciels : chaque seuil (pluie 0,22 / orage 0,48, neige 0,28, vent 0,62) des deux cotes,
// chaque saison.
const CIELS = [
  [0, 0, 0, "summer"],
  [0.21, 0, 0.61, "summer"],
  [0.22, 0, 0.62, "summer"],
  [0.4, 0, 0.7, "spring"],
  [0.48, 0, 0.97, "autumn"],
  [0.77, 0, 0.2, "autumn"],
  [1, 0, 1, "spring"],
  [0.1, 0.27, 0.3, "winter"],
  [0.05, 0.5, 0.65, "winter"],
  [0.6, 0.94, 0.8, "winter"],
  [0, 0.3, 0, "summer"],
  [0.3, 0, 0.5, "autumn"],
];

export default {
  module: "src/ai/weatherGoalBias.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisWeatherBehaviorVectors.inl"),

  cases: [
    {
      name: "ReadSimWeather",
      comment: "readSimWeather({ seed, day, time }) — la meteo a l'heure, telle que l'habitant la lit",
      args: ["double", "int", "double"],
      ret: [
        { name: "rain", type: "double" },
        { name: "snow", type: "double" },
        { name: "wind", type: "double" },
        { name: "cover", type: "double" },
        { name: "season", type: "string" },
        { name: "clearing", type: "double" },
      ],
      inputs: croiser([12345, 4000000000], [0, 1, 4, 8, 31, 61, 94, 121], [0, 37.8, 44.9, 89.99, 135.5, 700.3]),
      call: (mod, [seed, day, time]) => mod.readSimWeather({ seed, day, time }),
    },
    {
      name: "WeatherGoalBias",
      comment: "weatherGoalBiasFromState({ rain, snow, wind, season }, { jobId }, goal)",
      args: ["double", "double", "double", "string", "string", "string"],
      ret: "double",
      inputs: croiser(CIELS, METIERS, BUTS).map(([[r, s, w, season], job, goal]) => [r, s, w, season, job, goal]),
      call: (mod, [rain, snow, wind, season, jobId, goal]) =>
        mod.weatherGoalBiasFromState({ rain, snow, wind, season }, { jobId }, goal),
    },
    {
      name: "ShelterRain",
      comment: "shouldSeekRainShelter / shelterRainScore ; cooldown < 0 = shelterCooldownUntil absent",
      args: ["double", "bool", "string", "string", "double", "double"],
      ret: [{ name: "should", type: "bool" }, { name: "score", type: "double" }],
      inputs: croiser(
        [0, 0.3, 0.47, 0.48, 0.62, 0.9, 1],
        [false, true],
        ["gatherFood", "explore", "socialize", "rest", "shelterRain", "maintain"],
        ["settler", "farmer", "butcher"],
        [[100, -1], [100, 99], [100, 100], [100, 140]],
      ).map(([rain, inside, goal, job, [time, cooldown]]) => [rain, inside, goal, job, time, cooldown]),
      call: (mod, [rain, inside, goal, jobId, time, cooldown]) => {
        const sim = { forceWeather: { rain }, time };
        const npc = { inside: inside ? { buildingId: "b0" } : null, goal, jobId,
          shelterCooldownUntil: cooldown < 0 ? null : cooldown };
        return { should: mod.shouldSeekRainShelter(sim, npc), score: mod.shelterRainScore(sim, npc) };
      },
    },
  ],
};
