// Releve de `buildScore` (npc.js l. 2733) dans la reference : la valeur de chaque terme, pour
// chaque habitant, aux ticks demandes. Sert a decouper le portage de la ligne `build` en ne
// portant que ce qui pese dans le scenario.
//
// Chargement, masques et tick : ceux d'emit-state-digests.mjs (tick recompose). Les termes sont
// recalcules avec les fonctions EXPORTEES de la reference (collectivePriorities.js,
// colonizationDoctrine.js, colonySite.js, ambitions.js, simulation.js), dans l'ordre de
// `buildScore`, AVANT le tick demande (l'etat que la decision de ce tick lira). Les trois
// fonctions privees de npc.js (`liveActiveSites`, `isEssentialBuildType`, `jobPriority`) sont
// recopiees a l'identique ici.
//
// Usage :
//   node tools/migration/trace-build-score.mjs -ref <clone anastasis-ref-p3> -scenario <json> -ticks 32,245,903

import path from "node:path";
import { pathToFileURL } from "node:url";
import { chargerReference, appliquerMasques } from "./scenarios/masks.mjs";
import { chargerScenario } from "./scenarios/scenario-format.mjs";

function arg(nom, defaut = null) {
  const i = process.argv.indexOf(nom);
  return i >= 0 && i + 1 < process.argv.length ? process.argv[i + 1] : defaut;
}
const REF = path.resolve(arg("-ref") ?? "");
const scenario = chargerScenario(arg("-scenario"));
const ticks = (arg("-ticks", "32") || "32").split(",").map(Number).sort((a, b) => a - b);

const ref = await chargerReference(REF);
const imp = (rel) => import(pathToFileURL(path.join(REF, rel)).href);
const cp = await imp("src/sim/collectivePriorities.js");
const cd = await imp("src/sim/colonizationDoctrine.js");
const cs = await imp("src/sim/colonySite.js");
const amb = await imp("src/ai/ambitions.js");

const sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
ref.deserialize(sim, JSON.parse(JSON.stringify(scenario.save)));
if (scenario.vue) ref.pinSimulationView(sim.simulationBudget, scenario.vue.x, scenario.vue.y);
const { tick } = appliquerMasques(sim, ref, [...scenario.masques], { recompose: true });

// npc.js, recopies a l'identique.
const liveActiveSites = (s) => {
  if (typeof s.activeConstructions === "function") return s.activeConstructions();
  const one = typeof s.activeConstruction === "function" ? s.activeConstruction() : null;
  return one ? [one] : [];
};
const jobPriority = (npc, goal) => {
  const rank = npc.job.priority.indexOf(goal);
  return rank < 0 ? 0 : Math.max(0, 18 - rank * 2.5);
};

function termes(npc) {
  const need = sim.buildingNeedScore();
  const hotPads = cd.liveHotPads(sim).length;
  const spine = cp.exploitSpinePending(sim);
  const amenity = cp.villageAmenityPending(sim);
  const craft = cp.villageCraftPending(sim);
  const herd = cp.villageHerdPending(sim);
  const bootstrap = cp.craftBootstrapPending(sim);
  const activeSites = liveActiveSites(sim).length;
  const treasury = sim.colony?.treasury | 0;
  const type = sim.chooseBuildingType?.() || "house";
  return {
    need, hotPads, spine: Boolean(spine), amenity: Boolean(amenity), craft: Boolean(craft), herd: Boolean(herd),
    bootstrap: bootstrap ? (bootstrap.type ?? bootstrap.id ?? true) : false,
    activeSites, treasury, type,
    traitBuild: npc.trait?.build, traitBias: npc.job?.traitBias?.build, jobId: npc.jobId,
    jobPriority: jobPriority(npc, "build"),
    planBias: amb.planBias(npc, "build"),
    colonization: cd.colonizationBuildBias(sim, npc),
    colonySite: cs.colonySiteBuildBias(sim, npc),
    collective: cp.collectiveGoalBias(sim, "build"),
  };
}

let t = 0;
for (const cible of ticks) {
  while (t < cible - 1) { tick(scenario.dt); t += 1; }
  console.log(`--- avant le tick ${cible} (temps ${sim.time.toFixed(3)}, jour ${sim.day})`);
  for (const npc of sim.actors) {
    console.log(npc.id, npc.goal, JSON.stringify(termes(npc)));
  }
}
