// Parite de la meteo commune — `src/sim/weather.js`.
//
// Le ciel est lu par la simulation (`weather.rain > 0.2` dans simulation.js) et
// par le rendu : c'est une verite de monde, prouvee bit a bit comme le rythme.
// Couvre les quatre saisons (jours 1..120 et une deuxieme annee), des heures de
// part et d'autre des bornes du givre et du front, des fractions hors [0,1)
// (le `%` de JS), l'appel SANS heure (celui de la simulation), et un climat de terre.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// Graines : la canonique, petites, et au-dela de 2^31 (le `>>> 0`).
const GRAINES = [12345, 1, 987654321, 4000000000];
// Jours : chaque saison, ses bords (30/31, 60/61...), une deuxieme annee, un jour flottant.
const JOURS = [1, 2, 7, 29.5, 30, 31, 45, 60, 61, 75, 90, 91, 100, 119, 120, 121, 200.75];
// Heures : minuit, fin de nuit (<0.14), aube du givre, matin, midi, soir, >0.88, hors bornes.
const HEURES = [0, 0.05, 0.13, 0.17, 0.3, 0.42, 0.5, 0.63, 0.9, 0.999, 1.3, -0.2];

const CHAMPS = [
  { name: "cover", type: "double" },
  { name: "coverBase", type: "double" },
  { name: "rain", type: "double" },
  { name: "snow", type: "double" },
  { name: "frost", type: "double" },
  { name: "precip", type: "double" },
  { name: "clearing", type: "double" },
  { name: "peak", type: "double" },
  { name: "season", type: "string" },
  { name: "wind", type: "double" },
  { name: "windDir", type: "double" },
  { name: "dirX", type: "double" },
  { name: "dirZ", type: "double" },
];

export default {
  module: "src/sim/weather.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisWeatherVectors.inl"),

  cases: [
    {
      name: "WeatherAtHour",
      comment: "weatherAt(seed, day, null, dayFrac) — l'appel du rendu",
      args: ["double", "double", "double"],
      ret: CHAMPS,
      inputs: croiser(GRAINES, JOURS, HEURES),
      call: (mod, [seed, day, frac]) => mod.weatherAt(seed, day, null, frac),
    },
    {
      name: "WeatherAtDaily",
      comment: "weatherAt(seed, day) — l'appel de la simulation, sans heure",
      args: ["double", "double"],
      ret: CHAMPS,
      inputs: croiser(GRAINES, JOURS),
      call: (mod, [seed, day]) => mod.weatherAt(seed, day),
    },
    {
      name: "WeatherAtClimate",
      comment: "weatherAt avec un biais de terre { coverBias, rainBias, windBias }",
      args: ["double", "double", "double", "double", "double", "double"],
      ret: CHAMPS,
      inputs: croiser([12345, 4000000000], [3, 47, 95], [0.2, 0.63], [[0.12, 0.08, -0.1], [-0.2, 0, 0.3]])
        .map(([s, d, f, [c, r, w]]) => [s, d, f, c, r, w]),
      call: (mod, [seed, day, frac, coverBias, rainBias, windBias]) =>
        mod.weatherAt(seed, day, { coverBias, rainBias, windBias }, frac),
    },
    {
      name: "WetnessHumidity",
      comment: "weatherWetnessAt / weatherHumidityAt sur weatherAt(seed, day, null, dayFrac)",
      args: ["double", "double", "double"],
      ret: [{ name: "wetness", type: "double" }, { name: "humidity", type: "double" }],
      inputs: croiser(GRAINES, JOURS, [0.17, 0.5, 0.9]),
      call: (mod, [seed, day, frac]) => {
        const w = mod.weatherAt(seed, day, null, frac);
        return { wetness: mod.weatherWetnessAt(w), humidity: mod.weatherHumidityAt(w) };
      },
    },
  ],
};
