// RELEVE DE LA NAVIGATION — mission nav-wiring-001.
//
//   node tools/migration/trace-nav.mjs -ref <clone anastasis-ref-p3> [-ticks 600]
//        [-scenario tools/migration/scenarios/endurance.json] [-md <sortie.md>] [-actor npc-2 -at 32]
//
// Avant de brancher le service de navigation dans le village C++ (`requestPath`, la file budgetee,
// le cache exact et de zone), il faut savoir quelles branches le scenario du harnais atteint :
// cache servi ou A* immediat ou mise en file, file videe autour de la boucle des PNJ, contournement
// local (`steerAroundBlock`), file de porte (`doorQueueWaypoint`), hesitation, facteur de vitesse,
// anti-blocage, passages. Meme methode que trace-spatial-risk.mjs : copie de la reference dont les
// fonctions listees sont enveloppees (renommees `<nom>__o`), methodes de `Simulation` enveloppees sur
// le prototype ; la simulation n'est pas modifiee.
// `-actor <id> -at <tick>` ajoute le detail des appels de cet habitant a ce tick.

import { cpSync, mkdtempSync, readFileSync, writeFileSync, rmSync, mkdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ICI = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const argOf = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : d; };
const REF = argOf("-ref", null);
if (!REF) {
  console.error("-ref <clone de anastasis-ref-p3> requis");
  process.exit(2);
}
const TICKS = Number(argOf("-ticks", "600"));
const SCENARIO = argOf("-scenario", join(ICI, "scenarios", "endurance.json"));
const MD = argOf("-md", null);
const DETAIL_ACTOR = argOf("-actor", null);
const DETAIL_AT = Number(argOf("-at", "-1"));

const COPIE = mkdtempSync(join(tmpdir(), "nav-"));
cpSync(join(REF, "src"), join(COPIE, "src"), { recursive: true });
cpSync(join(REF, "package.json"), join(COPIE, "package.json"));
const ENVELOPPES = {
  "src/sim/navService.js": ["requestPath", "lookupCachedPath", "storeCachedPath", "applyPathToActor", "processNavQueue",
    "beginNavTick", "sweepNavCache", "resolveNavJob"],
  "src/sim/crowdNav.js": ["doorQueueWaypoint"],
  "src/sim/simulation.js": ["movementSpeedFactor"],
};
for (const [rel, noms] of Object.entries(ENVELOPPES)) {
  const p = join(COPIE, rel);
  let src = readFileSync(p, "utf8");
  for (const nom of noms) {
    const re = new RegExp(`(export\\s+)?function\\s+${nom}\\s*\\(`);
    if (!re.test(src)) throw new Error(`${nom} introuvable dans ${rel}`);
    src = src.replace(re, (_m, exp) =>
      `${exp || ""}function ${nom}(...__a) { return globalThis.__nav(${JSON.stringify(nom)}, ${nom}__o, __a, this); }\nfunction ${nom}__o(`);
  }
  writeFileSync(p, src, "utf8");
}

// --- Enregistreur -------------------------------------------------------------------------------
let tick = 0;
let simRef = null;
const compte = new Map();
const premier = new Map();
const detail = [];
const note = (cle) => {
  compte.set(cle, (compte.get(cle) || 0) + 1);
  if (!premier.has(cle)) premier.set(cle, tick);
};
const acteurDe = (nom, a) => {
  if (["requestPath", "applyPathToActor", "doorQueueWaypoint", "movementSpeedFactor"].includes(nom)) {
    return nom === "applyPathToActor" || nom === "requestPath" ? a[1] : a[1];
  }
  return null;
};
globalThis.__nav = (nom, fn, args, self) => {
  const acteur = acteurDe(nom, args);
  const avant = acteur ? { role: acteur.navigation?.doorQueueRole ?? null } : null;
  let svc = null;
  if (nom === "requestPath" || nom === "processNavQueue") svc = args[0]?.navService;
  const calcAvant = svc ? svc.calcThisTick : 0;
  const fileAvant = svc ? svc.queue.length : 0;
  const r = fn.apply(self, args);
  let cle = nom;
  if (nom === "requestPath") {
    const s = args[0].navService;
    if (r && s.calcThisTick === calcAvant) cle += " -> cache";
    else if (r) cle += " -> A* immediat";
    else if (s.queue.length > fileAvant || s.pendingByActor.has(acteur.id)) cle += " -> file";
    else cle += " -> null";
  } else if (nom === "lookupCachedPath") {
    cle += r ? " -> servi" : " -> manque";
  } else if (nom === "processNavQueue") {
    cle += fileAvant ? ` -> file ${fileAvant}, resolus ${r}` : " -> vide";
  } else if (nom === "doorQueueWaypoint") {
    cle += ` -> role ${acteur.navigation?.doorQueueRole ?? null}`;
  } else if (nom === "movementSpeedFactor") {
    cle += r === 1 ? " -> 1" : " -> autre";
  } else if (nom === "applyPathToActor") {
    cle += args[3] && args[3].length ? ` -> chemin (${args[4] || "astar"})` : " -> echec";
  }
  note(cle);
  if (acteur && DETAIL_ACTOR && acteur.id === DETAIL_ACTOR && tick === DETAIL_AT) {
    detail.push(`${cle}${nom === "requestPath" ? ` cible (${args[2].x}, ${args[2].y}) depuis (${acteur.x}, ${acteur.y})` : ""}`);
  }
  return r;
};

const { chargerReference, appliquerMasques } = await import(pathToFileURL(join(ICI, "scenarios", "masks.mjs")).href);
const { chargerScenario } = await import(pathToFileURL(join(ICI, "scenarios", "scenario-format.mjs")).href);
const scenario = chargerScenario(SCENARIO);
const ref = await chargerReference(COPIE);
for (const m of ["moveActor", "nextWaypoint", "steerAroundBlock", "resolveStuckActor", "recordPassage", "separateCrowdedActors"]) {
  const o = ref.Simulation.prototype[m];
  if (typeof o !== "function") continue;
  ref.Simulation.prototype[m] = function (...a) {
    const acteur = m === "separateCrowdedActors" ? null : a[0];
    const h0 = acteur ? acteur.hesitationTimer || 0 : 0;
    const c0 = acteur ? acteur.hesitationCooldown || 0 : 0;
    const r = o.apply(this, a);
    let cle = `sim.${m}`;
    if (m === "moveActor" && (acteur.hesitationCooldown || 0) > Math.max(0, c0 - a[2]) + 1e-12) cle += " -> hesitation tiree";
    if (m === "resolveStuckActor") cle += ` -> ${r?.action}`;
    note(cle);
    if (acteur && DETAIL_ACTOR && acteur.id === DETAIL_ACTOR && tick === DETAIL_AT) detail.push(cle);
    return r;
  };
}
const sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
simRef = sim;
ref.deserialize(sim, JSON.parse(JSON.stringify(scenario.save)));
if (scenario.vue) ref.pinSimulationView(sim.simulationBudget, scenario.vue.x, scenario.vue.y);
const applique = appliquerMasques(sim, ref, [...scenario.masques], { recompose: true });
for (let t = 1; t <= TICKS; t += 1) {
  tick = t;
  applique.tick(scenario.dt);
}
rmSync(COPIE, { recursive: true, force: true });

const L = [];
L.push(`# Releve de la navigation — ${scenario.nom || "endurance"}, ${TICKS} ticks`);
L.push("");
L.push(`Genere par \`node tools/migration/trace-nav.mjs -ref <clone> -ticks ${TICKS}\` (mission nav-wiring-001).`);
L.push(`Cache de navigation a la fin : ${sim.navService?.cache?.size ?? 0} entrees ; file : ${sim.navService?.queue?.length ?? 0}.`);
L.push("");
L.push("| Appels | Premier tick | Fonction -> branche |");
L.push("|---:|---:|---|");
for (const [k, v] of [...compte.entries()].sort((a, b) => a[0].localeCompare(b[0]))) L.push(`| ${v} | ${premier.get(k)} | \`${k}\` |`);
if (DETAIL_ACTOR) {
  L.push("");
  L.push(`## ${DETAIL_ACTOR} au tick ${DETAIL_AT}`);
  L.push("");
  for (const d of detail) L.push(`- ${d}`);
}
L.push("");
const texte = L.join("\n");
if (MD) { mkdirSync(dirname(MD), { recursive: true }); writeFileSync(MD, texte, "utf8"); }
console.log(texte);
