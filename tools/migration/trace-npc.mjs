// Releve d'un habitant tick par tick dans la reference : but, activite, cible, position,
// echecs, chronometre, inventaire. Ne garde que les ticks ou l'un de ces champs change.
//
// Usage :
//   node tools/migration/trace-npc.mjs -ref <clone anastasis-ref-p3> -scenario <json> -npc npc-2 -from 30 -to 260

import path from "node:path";
import { chargerReference, appliquerMasques } from "./scenarios/masks.mjs";
import { chargerScenario } from "./scenarios/scenario-format.mjs";

function arg(nom, defaut = null) {
  const i = process.argv.indexOf(nom);
  return i >= 0 && i + 1 < process.argv.length ? process.argv[i + 1] : defaut;
}
const scenario = chargerScenario(arg("-scenario"));
const ref = await chargerReference(path.resolve(arg("-ref")));
const ID = arg("-npc", "npc-0");
const FROM = Number(arg("-from", "1"));
const TO = Number(arg("-to", "300"));

const sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
ref.deserialize(sim, JSON.parse(JSON.stringify(scenario.save)));
if (scenario.vue) ref.pinSimulationView(sim.simulationBudget, scenario.vue.x, scenario.vue.y);
const { tick } = appliquerMasques(sim, ref, [...scenario.masques], { recompose: true });

const f = (v) => (Number.isFinite(v) ? v.toFixed(2) : String(v));
let avant = "";
for (let t = 1; t <= TO; t += 1) {
  tick(scenario.dt);
  if (t < FROM) continue;
  const a = sim.actors.find((n) => n.id === ID);
  if (!a) break;
  const ligne = [
    a.goal, a.activity,
    a.target ? `cible (${f(a.target.x)}, ${f(a.target.y)})` : "sans cible",
    `echecs ${a.failedActions | 0}`,
    `inv f${a.inventory?.food | 0} w${a.inventory?.wood | 0} s${a.inventory?.stone | 0}`,
    a.workSession ? `session ${a.workSession.craftId}` : "",
    a.buildSiteId ? `site ${a.buildSiteId}` : "",
  ].join(" | ");
  if (ligne !== avant) {
    console.log(`${t}\t(${f(a.x)}, ${f(a.y)})\t${ligne}`);
    avant = ligne;
  }
}
