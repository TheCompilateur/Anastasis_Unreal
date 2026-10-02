// Releve des PREMIERES ECRITURES d'une cle sur les habitants de la reference : a quel tick, et par
// quelle fonction, une cle absente de la sauvegarde apparait (ou une cle presente change pour la
// premiere fois). Sert a porter ce que la premiere pensee d'un habitant ecrit, au meme moment.
//
// Chargement, masques et tick : ceux d'emit-state-digests.mjs (tick recompose). Chaque cle suivie
// est remplacee sur l'objet habitant par un accesseur qui note la premiere ecriture (tick, valeur,
// pile reduite aux fonctions de src/), puis rend la main a une propriete ordinaire.
//
// Usage :
//   node tools/migration/trace-first-writes.mjs -ref <clone anastasis-ref-p3> -scenario <json>
//        [-ticks 40] [-keys goalExplain,workShift,...] [-npc npc-2]

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
const TICKS = Number(arg("-ticks", "40"));
const ONLY = arg("-npc", null);
const KEYS = (arg("-keys", "goalExplain,streetDecision,workShift,hungerAction,_algoDebug,hesitationTimer,hesitationCooldown,nocturnalIntent,socialSeekId,buildBinding,activitySince,workTimer,trafficTimer,lastMoveDir")).split(",");

const ref = await chargerReference(REF);
const sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
ref.deserialize(sim, JSON.parse(JSON.stringify(scenario.save)));
if (scenario.vue) ref.pinSimulationView(sim.simulationBudget, scenario.vue.x, scenario.vue.y);
const { tick } = appliquerMasques(sim, ref, [...scenario.masques], { recompose: true });

let t = 0;
const pile = () => (new Error().stack || "").split("\n").slice(3)
  .filter((l) => l.includes(`${path.sep}src${path.sep}`) || l.includes("/src/"))
  .slice(0, 6)
  .map((l) => l.trim().replace(/^at /, "").replace(/\(?file:\/\/\/?.*\/src\//, "src/").replace(/\)$/, ""))
  .join(" < ");

for (const npc of sim.actors) {
  if (ONLY && npc.id !== ONLY) continue;
  for (const key of KEYS) {
    let value = npc[key];
    const had = Object.prototype.hasOwnProperty.call(npc, key);
    let noted = false;
    Object.defineProperty(npc, key, {
      configurable: true,
      enumerable: had,
      get() { return value; },
      set(v) {
        if (!noted) {
          noted = true;
          console.log(`tick ${t + 1}  ${npc.id}.${key}${had ? "" : " (nouvelle)"} = ${JSON.stringify(v)?.slice(0, 120)}`);
          console.log(`    ${pile()}`);
          // Rendre une propriete ordinaire, enumerable : l'empreinte doit voir la cle.
          delete npc[key];
          npc[key] = v;
          return;
        }
        value = v;
      },
    });
  }
}
// `mind.failures` : un objet imbrique.
for (const npc of sim.actors) {
  if (ONLY && npc.id !== ONLY) continue;
  const mind = npc.mind;
  if (!mind) continue;
  let value = mind.failures;
  const had = Object.prototype.hasOwnProperty.call(mind, "failures");
  let noted = false;
  Object.defineProperty(mind, "failures", {
    configurable: true,
    enumerable: had,
    get() { return value; },
    set(v) {
      if (!noted) {
        noted = true;
        console.log(`tick ${t + 1}  ${npc.id}.mind.failures${had ? "" : " (nouvelle)"} = ${JSON.stringify(v)}`);
        console.log(`    ${pile()}`);
        delete mind.failures;
        mind.failures = v;
        return;
      }
      value = v;
    },
  });
}
while (t < TICKS) { tick(scenario.dt); t += 1; }
