// Parite du risque spatial — `spatialRiskBiasMap` de `src/sim/npc.js` (mission resource-targets-001).
//
// Les fonctions ne sont pas exportees : leur TEXTE est relu dans npc.js et execute tel quel, avec
// les vraies dependances de la reference (NEEDS, SHELTER_RAIN, clamp, dist, civilHourOf) :
//   secondsUntilThreshold, criticalNeedBudget, secondsUntilNight, rainReturnBudget, routeSeconds,
//   budgetOverrun, SURVIVAL_FORECAST, SPATIAL_RISK_GOALS, et spatialRiskBiasMap lui-meme ;
//   la prevision de survie : projectedNeedPressure, forecastNeedBias, travelPressureToTarget,
//   FORECAST_HEAVY_WORK_GOALS, FORECAST_LIGHT_WORK_GOALS, SURVIVAL_PRODUCTION_GOALS, survivalForecastBias.
// Pour spatialRiskBiasMap, les replis surs (forecast*Target, shelterRainAccess), la meteo et les
// cibles (spatialRiskTargetForGoal) sont des points fournis par le vecteur : la combinaison est
// celle de la reference, les cibles se prouvent au village (Anastasis.Sim.Harnais). Pour
// survivalForecastBias, idem, et `mealPathBlocked` est fourni (le village le calcule deja).

import { readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");
const argv = process.argv.slice(2);
const iRef = argv.indexOf("-ref");
const REF = iRef >= 0 ? argv[iRef + 1] : "C:/dev/Jeux IV Kingdoms";

/**
 * Le texte d'une declaration de npc.js : `const nom = ...;` jusqu'au `;` hors parentheses,
 * crochets et accolades ; `function nom(` jusqu'a l'accolade qui ferme son corps.
 */
function extraire(src, entete) {
  const debut = src.indexOf(entete);
  if (debut < 0) throw new Error(`${entete} introuvable dans npc.js`);
  let profondeur = 0;
  if (entete.startsWith("const ")) {
    for (let i = debut; i < src.length; i += 1) {
      const c = src[i];
      if ("([{".includes(c)) profondeur += 1;
      else if (")]}".includes(c)) profondeur -= 1;
      else if (c === ";" && profondeur === 0) return src.slice(debut, i + 1);
    }
    throw new Error(`${entete} : fin introuvable`);
  }
  // Corps de fonction : la premiere accolade apres la liste des parametres.
  let i = debut + entete.length;
  let parens = 1;
  for (; parens > 0; i += 1) {
    if (src[i] === "(") parens += 1;
    else if (src[i] === ")") parens -= 1;
  }
  i = src.indexOf("{", i);
  for (; i < src.length; i += 1) {
    if (src[i] === "{") profondeur += 1;
    else if (src[i] === "}") {
      profondeur -= 1;
      if (profondeur === 0) return src.slice(debut, i + 1);
    }
  }
  throw new Error(`${entete} : fin introuvable`);
}

let prive = null;
const stubs = {};
function charger(mod) {
  if (prive) return prive;
  const src = readFileSync(join(REF, "src", "sim", "npc.js"), "utf8");
  const morceaux = [
    "const SURVIVAL_FORECAST =", "const SPATIAL_RISK_GOALS =",
    "function secondsUntilThreshold(", "function criticalNeedBudget(", "function secondsUntilNight(",
    "function rainReturnBudget(", "function routeSeconds(", "function budgetOverrun(", "function spatialRiskBiasMap(",
    "const FORECAST_HEAVY_WORK_GOALS =", "const FORECAST_LIGHT_WORK_GOALS =", "const SURVIVAL_PRODUCTION_GOALS =",
    "function projectedNeedPressure(", "function forecastNeedBias(", "function travelPressureToTarget(",
    "function survivalForecastBias(",
  ].map((e) => extraire(src, e));
  // eslint-disable-next-line no-new-func
  prive = Function(
    "NEEDS", "SHELTER_RAIN", "clamp", "dist", "civilHourOf",
    "readSimWeather", "forecastRestTarget", "forecastEatTarget", "forecastDrinkTarget", "shelterRainAccess",
    "spatialRiskTargetForGoal", "ensureNeeds", "mealPathBlocked",
    `"use strict";\n${morceaux.join("\n")}\nreturn { secondsUntilThreshold, criticalNeedBudget, secondsUntilNight, `
      + "rainReturnBudget, routeSeconds, budgetOverrun, spatialRiskBiasMap, SPATIAL_RISK_GOALS, "
      + "travelPressureToTarget, survivalForecastBias, FORECAST_HEAVY_WORK_GOALS, FORECAST_LIGHT_WORK_GOALS };",
  )(
    mod.needs.NEEDS, mod.weather.SHELTER_RAIN, mod.util.clamp, mod.util.dist, mod.rhythm.civilHourOf,
    () => ({ rain: stubs.rain }),
    () => stubs.rest, () => stubs.eat, () => stubs.drink, () => stubs.shelter,
    (_sim, _npc, goal) => stubs.targets[goal] ?? null,
    () => {},
    () => stubs.mealBlocked,
  );
  return prive;
}

const point = (has, x, y) => (has ? { x, y } : null);
const BUTS = ["gatherWood", "gatherStone", "gatherFood", "helpFarm", "build", "explore", "maintain", "aidHousehold"];

// Habitants : besoins calmes, faim / soif / fatigue proches du seuil, au-dela, metres a 0 et 100.
const BESOINS = [[10, 5, 90], [50, 60, 55], [57.9, 67.9, 48.1], [58, 68, 48], [80, 90, 10], [0, 0, 0], [100, 100, 100], [30, 62, 70]];
// Heures : 0 h, 4 h 59, 5 h, 9 h, 18 h, 20 h 59, 21 h, et un jour plus tard.
const TEMPS = [0, 18.7, 18.75, 33.75, 67.5, 78.7, 78.75, 90 * 3 + 50.1];
const PLUIES = [0, 0.31, 0.32, 0.4, 0.479, 0.48, 0.9];
const VITESSES = [0, 1, 1.6, 3.4, 4];

// Scenes : habitant, vitesse, replis (repos, manger, boire, abri), cibles par but.
const SCENES = [
  {
    npc: [50, 50], speed: 4, safes: [[52, 50], [60, 48], [40, 50], [51, 51]],
    targets: { gatherWood: [80, 90], gatherStone: [10, 10], gatherFood: [55, 52], helpFarm: [70, 30], build: null,
      explore: [120, 120], maintain: [49.5, 57.5], aidHousehold: [50.5, 57.5] },
  },
  {
    npc: [12.25, 7.75], speed: 0, safes: [null, [12, 8], null, null],
    targets: { gatherWood: [12.25, 7.75], gatherStone: null, gatherFood: [200, 3], helpFarm: [13, 9], build: [30, 30],
      explore: null, maintain: null, aidHousehold: [100, 100] },
  },
  {
    npc: [100, 20], speed: 2.2, safes: [[30, 30], null, [100, 22], [5, 5]],
    targets: { gatherWood: [140, 25], gatherStone: [101, 21], gatherFood: [60, 80], helpFarm: [99, 19], build: [104, 20],
      explore: [150, 70], maintain: [30.5, 30.5], aidHousehold: [100.5, 20.5] },
  },
  // Tout pres : de jour et sans besoin pressant, aucune pression (carte vide).
  {
    npc: [40, 40], speed: 3.4, safes: [[41, 40], [40, 41], [39, 40], null],
    targets: { gatherWood: [42, 40], gatherStone: [40, 43], gatherFood: [41, 41], helpFarm: [38, 39], build: [40.5, 40.5],
      explore: [44, 44], maintain: [41.5, 40.5], aidHousehold: [40, 40] },
  },
];

// Toutes les cles que survivalForecastBias peut poser, dans l'ordre ou il les pose au plus tot.
const CLES_PREVISION = ["eat", "drink", "rest", "buy", "sell", "eatTogether", "shelterRain", "gatherWood", "gatherStone",
  "gatherFood", "helpFarm", "build", "deliver", "fetchInput", "haulJob", "haulCart", "explore", "craft", "maintain", "sell2",
  "closeWorkplace"].filter((k) => k !== "sell2");
// Besoins pour la prevision : sous l'envie, entre envie et critique, au-dela, metres extremes.
const BESOINS_PREVISION = [[10, 5, 90], [30, 35, 75], [40, 50, 60], [50, 60, 55], [57.9, 67.9, 48.1], [70, 80, 30], [0, 0, 0], [100, 100, 100]];

export default {
  modules: {
    npc: "src/sim/npc.js",
    needs: "src/life/needs.js",
    weather: "src/ai/weatherGoalBias.js",
    util: "src/sim/util.js",
    rhythm: "src/life/villageRhythm.js",
  },
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisSpatialRiskVectors.inl"),

  cases: [
    {
      name: "SpatialRiskThreshold",
      comment: "secondsUntilThreshold(current, critical, risePerSecond)",
      args: ["double", "double", "double"],
      ret: "double",
      inputs: croiser([0, 30, 57.99, 58, 99, NaN], [58, 68, Infinity], [0, -1, 0.64, 0.6, 0.52]),
      call: (mod, [c, k, r]) => charger(mod).secondsUntilThreshold(c, k, r),
    },
    {
      name: "SpatialRiskNeedBudget",
      comment: "criticalNeedBudget({ hunger, thirst, energy })",
      args: ["double", "double", "double"],
      ret: [{ name: "hunger", type: "double" }, { name: "thirst", type: "double" }, { name: "rest", type: "double" }],
      inputs: BESOINS,
      call: (mod, [hunger, thirst, energy]) => charger(mod).criticalNeedBudget({ hunger, thirst, energy }),
    },
    {
      name: "SpatialRiskNight",
      comment: "secondsUntilNight({ time }) — dayFracOf = (time % 90) / 90",
      args: ["double"],
      ret: "double",
      inputs: [...TEMPS, 45, 89.999, 90, 135.2, 1000.5],
      call: (mod, [time]) => charger(mod).secondsUntilNight({ time }),
    },
    {
      name: "SpatialRiskRain",
      comment: "rainReturnBudget({ rain })",
      args: ["double"],
      ret: "double",
      inputs: PLUIES,
      call: (mod, [rain]) => charger(mod).rainReturnBudget({ rain }),
    },
    {
      name: "SpatialRiskRoute",
      comment: "routeSeconds({ x, y, speed }, target, safe | null)",
      args: ["double", "double", "double", "double", "double", "bool", "double", "double"],
      ret: "double",
      inputs: croiser([[50, 50], [0.5, 99.25]], VITESSES, [[50, 50], [80.5, 12.25]], [[false, 0, 0], [true, 52, 50], [true, 50, 50]])
        .map(([[x, y], s, [tx, ty], [h, sx, sy]]) => [x, y, s, tx, ty, h, sx, sy]),
      call: (mod, [x, y, speed, tx, ty, h, sx, sy]) => charger(mod).routeSeconds({ x, y, speed }, { x: tx, y: ty }, point(h, sx, sy)),
    },
    {
      name: "SpatialRiskOverrun",
      comment: "budgetOverrun(route, available)",
      args: ["double", "double"],
      ret: "double",
      inputs: croiser([0, -2, 3, 6, 10.8, 40, 400, NaN], [0, -1, 3, 8, 20, 39.99, 1000, Infinity]),
      call: (mod, [route, available]) => charger(mod).budgetOverrun(route, available),
    },
    {
      name: "SurvivalTravel",
      comment: "travelPressureToTarget({ x, y, speed }, target | null, 20)",
      args: ["double", "double", "double", "bool", "double", "double"],
      ret: "double",
      inputs: croiser([[50, 50], [3.5, 97.25]], VITESSES, [[false, 0, 0], [true, 50, 50], [true, 60, 50], [true, 120, 90], [true, 51.7, 61.2]])
        .map(([[x, y], s, [h, tx, ty]]) => [x, y, s, h, tx, ty]),
      call: (mod, [x, y, speed, h, tx, ty]) => charger(mod).travelPressureToTarget({ x, y, speed }, point(h, tx, ty), 20),
    },
    {
      name: "SurvivalForecast",
      comment: "survivalForecastBias(sim, npc) — replis fournis (ceux des scenes), mealPathBlocked fourni",
      // scene, faim, soif, energie, nourriture du sac, repas bloque
      args: ["int", "double", "double", "double", "int", "bool"],
      ret: [{ name: "keys", type: "string" }, ...CLES_PREVISION.map((k) => ({ name: k, type: "double" }))],
      inputs: croiser([0, 1, 2, 3], BESOINS_PREVISION, [0, 2], [false, true])
        .filter(([, , food, blocked]) => !(food > 0 && blocked))
        .map(([s, [h, t, e], food, blocked]) => [s, h, t, e, food, blocked]),
      call: (mod, [s, hunger, thirst, energy, food, blocked]) => {
        const p = charger(mod);
        const scene = SCENES[s];
        const [rest, eat, drink] = scene.safes.map((q) => (q ? { x: q[0], y: q[1] } : null));
        Object.assign(stubs, { rest, eat, drink, mealBlocked: blocked });
        const npc = { x: scene.npc[0], y: scene.npc[1], speed: scene.speed, hunger, thirst, energy, inventory: { food } };
        const map = p.survivalForecastBias({ rng: () => { throw new Error("tirage inattendu"); } }, npc);
        const out = { keys: Object.keys(map).join(",") };
        for (const k of CLES_PREVISION) out[k] = map[k] ?? 0;
        for (const k of Object.keys(map)) if (!CLES_PREVISION.includes(k)) throw new Error(`cle inattendue ${k}`);
        return out;
      },
    },
    {
      name: "SpatialRiskBiasMap",
      comment: "spatialRiskBiasMap(sim, npc) — replis et cibles fournis, combinaison de la reference",
      // scene, faim, soif, energie, temps, pluie
      args: ["int", "double", "double", "double", "double", "double"],
      ret: [{ name: "keys", type: "string" }, ...BUTS.map((b) => ({ name: b, type: "double" }))],
      inputs: croiser([0, 1, 2, 3], BESOINS, [18.75, 50, 78.7, 80], [0, 0.4, 0.5]).map(([s, [h, t, e], time, rain]) => [s, h, t, e, time, rain]),
      call: (mod, [s, hunger, thirst, energy, time, rain]) => {
        const p = charger(mod);
        const scene = SCENES[s];
        const [rest, eat, drink, shelter] = scene.safes.map((q) => (q ? { x: q[0], y: q[1] } : null));
        Object.assign(stubs, { rain, rest, eat, drink, shelter, targets: {} });
        for (const [g, q] of Object.entries(scene.targets)) stubs.targets[g] = q ? { x: q[0], y: q[1] } : null;
        const sim = { time };
        const npc = { x: scene.npc[0], y: scene.npc[1], speed: scene.speed, hunger, thirst, energy };
        const map = p.spatialRiskBiasMap(sim, npc);
        const out = { keys: Object.keys(map).join(",") };
        for (const b of BUTS) out[b] = map[b] ?? 0;
        if ([...p.SPATIAL_RISK_GOALS].join(",") !== BUTS.join(",")) throw new Error("SPATIAL_RISK_GOALS a change");
        return out;
      },
    },
  ],
};
