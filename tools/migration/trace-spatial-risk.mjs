// RELEVE DE `spatialRiskBiasMap` — mission resource-targets-001.
//
//   node tools/migration/trace-spatial-risk.mjs -ref <clone anastasis-ref-p3> [-ticks 600]
//        [-scenario tools/migration/scenarios/endurance.json] [-md <sortie.md>] [-json <sortie.json>]
//
// La preparation `spatialRiskBiasMap` d'adultScores (npc.js) appelle, pour huit buts, une cible et
// trois ou quatre replis surs (manger, boire, dormir, s'abriter). Avant de porter, il faut savoir
// quelles branches le scenario du harnais atteint, si un biais sort non nul, si la preparation tire
// dans `sim.rng` et ce qu'elle ECRIT (le filtre paresseux des seuils, `ensureBuildingAccessPoints`).
//
// Methode : une copie de la reference dans un dossier temporaire, ou chaque fonction listee est
// renommee `<nom>__o` et remplacee par une enveloppe qui note l'appel (nom, resultat) quand elle tourne
// sous `spatialRiskBiasMap`. Les methodes de `Simulation` sont enveloppees sur le prototype. La copie
// n'est jamais ecrite ailleurs ; la simulation n'est pas modifiee (les enveloppes rendent le resultat).
// Chargement, masques et tick : ceux d'emit-state-digests.mjs (tick recompose).

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
const JSON_OUT = argOf("-json", null);

// --- Copie instrumentee ----------------------------------------------------------------------
const COPIE = mkdtempSync(join(tmpdir(), "spatial-risk-"));
cpSync(join(REF, "src"), join(COPIE, "src"), { recursive: true });
cpSync(join(REF, "package.json"), join(COPIE, "package.json"));

const ENVELOPPES = {
  "src/sim/npc.js": [
    "spatialRiskBiasMap", "spatialRiskTargetForGoal", "forecastEatTarget", "forecastDrinkTarget", "forecastRestTarget",
    "drinkTarget", "shelterRainAccess", "stableBuildingAccess", "gatherWoodTarget",
    "recallOrSearch", "extractionCourtTarget", "householdAidTarget", "householdAidPlan", "criticalNeedBudget",
    "secondsUntilNight", "rainReturnBudget", "routeSeconds", "budgetOverrun",
  ],
  "src/ai/dayIntent.js": ["intentExploreHint"],
  "src/ai/memory.js": ["bestKnownWater", "bestKnownBed", "recallResource", "exploreTarget", "seedHomeBedBelief", "bestKnownBelief"],
  "src/sim/fieldWorkPosts.js": ["fieldWorkTarget"],
  "src/sim/navGrid.js": ["ensureBuildingAccessPoints", "computeBuildingAccessPoints", "pickBuildingAccessPoint"],
};
let enveloppesPosees = 0;
for (const [rel, noms] of Object.entries(ENVELOPPES)) {
  const p = join(COPIE, rel);
  let src = readFileSync(p, "utf8");
  for (const nom of noms) {
    const re = new RegExp(`(export\\s+)?function\\s+${nom}\\s*\\(`);
    if (!re.test(src)) throw new Error(`${nom} introuvable dans ${rel}`);
    src = src.replace(re, (_m, exp) =>
      `${exp || ""}function ${nom}(...__a) { return globalThis.__rec(${JSON.stringify(nom)}, ${nom}__o, __a, this); }\nfunction ${nom}__o(`);
    enveloppesPosees += 1;
  }
  writeFileSync(p, src, "utf8");
}
// Les autres modules importes par npc.js : on regarde leur nom sur la pile seulement.
const IMPORTES = ["wantsColonizationClear", "isUrgentColonizationClear", "canSawAtMill",
  "isLivestockDepot", "extractionPostFor"];

// --- Enregistreur -----------------------------------------------------------------------------
const pile = [];
let courant = null; // racine de l'appel spatialRiskBiasMap en cours
const appels = [];
const resume = (v) => {
  if (v === null || v === undefined) return v === null ? "null" : "undef";
  if (typeof v === "number") return Number.isFinite(v) ? Math.round(v * 1000) / 1000 : String(v);
  if (Array.isArray(v)) return `[${v.length}]`;
  if (typeof v === "object" && Number.isFinite(v.x) && Number.isFinite(v.y)) return `(${v.x},${v.y})`;
  if (typeof v === "object") return JSON.stringify(v).slice(0, 120);
  return String(v);
};
globalThis.__rec = (nom, fn, args, self) => {
  const racine = nom === "spatialRiskBiasMap" && !courant;
  if (!courant && !racine) return fn.apply(self, args);
  const noeud = { n: nom, k: [] };
  if (nom === "spatialRiskTargetForGoal") noeud.goal = args[2];
  if (racine) {
    courant = noeud;
    noeud.npc = args[1]?.id;
    noeud.time = args[0]?.time;
    noeud.seuilsAvant = seuils(args[0]);
    noeud.tirages = 0;
  } else {
    pile[pile.length - 1].k.push(noeud);
  }
  pile.push(noeud);
  try {
    const r = fn.apply(self, args);
    noeud.r = nom === "spatialRiskBiasMap" ? { ...r } : resume(r);
    return r;
  } finally {
    pile.pop();
    if (racine) {
      noeud.seuilsApres = seuils(args[0]);
      appels.push(noeud);
      courant = null;
    }
  }
};
function seuils(sim) {
  const out = {};
  for (const b of sim?.buildings || []) out[b.id] = JSON.stringify(b.accessPoints ?? null);
  return out;
}

// --- Simulation -------------------------------------------------------------------------------
const { chargerReference, appliquerMasques } = await import(pathToFileURL(join(ICI, "scenarios", "masks.mjs")).href);
const { chargerScenario } = await import(pathToFileURL(join(ICI, "scenarios", "scenario-format.mjs")).href);
const scenario = chargerScenario(SCENARIO);
const ref = await chargerReference(COPIE);
const METHODES = ["buildingAccessPoint", "socialPos", "marketAccessPoint", "marketPos", "drinkAccessPoint", "accessPointNear",
  "findTendFieldNear", "farmPos", "constructionAccessPoint", "maintenancePos", "colonizationWoodTarget", "actorById",
  "workCommutePos", "pickDailyBuilding", "plazaMeetingPoint", "plannedMarketPos", "completedBuildingEntries"];
for (const m of METHODES) {
  const o = ref.Simulation.prototype[m];
  if (typeof o !== "function") { console.error(`(methode absente : ${m})`); continue; }
  ref.Simulation.prototype[m] = function (...a) { return globalThis.__rec(`sim.${m}`, o, a, this); };
}
const sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
ref.deserialize(sim, JSON.parse(JSON.stringify(scenario.save)));
if (scenario.vue) ref.pinSimulationView(sim.simulationBudget, scenario.vue.x, scenario.vue.y);
const applique = appliquerMasques(sim, ref, [...scenario.masques], { recompose: true });
const original = sim.rng;
const enveloppe = () => {
  if (courant) {
    courant.tirages += 1;
    const cadres = new Error().stack.split("\n").slice(2, 12).map((l) => l.trim().replace(/^at /, "").split(" ")[0]);
    (courant.sitesTirage ||= []).push(cadres.filter((c) => !c.startsWith("globalThis")).join(" < "));
  }
  return original();
};
enveloppe.state = () => original.state();
enveloppe.setState = (s) => original.setState(s);
sim.rng = enveloppe;
let tick = 0;
const parTick = [];
for (let t = 1; t <= TICKS; t += 1) {
  tick = t;
  const n0 = appels.length;
  applique.tick(scenario.dt);
  for (let i = n0; i < appels.length; i += 1) appels[i].t = t;
}
rmSync(COPIE, { recursive: true, force: true });

// --- Agregats -----------------------------------------------------------------------------------
const signature = (noeud) => `${noeud.n}${noeud.r === "null" || noeud.r === "undef" ? "=null" : ""}${noeud.k.length ? `{${noeud.k.map(signature).join(",")}}` : ""}`;
const parBut = new Map();
const repli = new Map();
let biaisNonNuls = 0;
const biais = [];
const ecritures = [];
let tirages = 0;
const sitesTirage = new Map();
for (const a of appels) {
  tirages += a.tirages;
  for (const s of a.sitesTirage || []) sitesTirage.set(s, (sitesTirage.get(s) || 0) + 1);
  const cles = Object.keys(a.r || {});
  if (cles.length) {
    biaisNonNuls += 1;
    biais.push({ t: a.t, npc: a.npc, map: a.r });
  }
  for (const [id, av] of Object.entries(a.seuilsAvant)) {
    if (a.seuilsApres[id] !== av) ecritures.push({ t: a.t, npc: a.npc, building: id, avant: av, apres: a.seuilsApres[id] });
  }
  for (const k of a.k) {
    if (k.n === "spatialRiskTargetForGoal") {
      const cle = `${k.goal} :: ${k.k.map(signature).join(",") || "(direct)"} -> ${k.r === "null" ? "null" : "cible"}`;
      parBut.set(cle, (parBut.get(cle) || 0) + 1);
    } else {
      const cle = signature(k);
      repli.set(cle, (repli.get(cle) || 0) + 1);
    }
  }
}
const tri = (m) => [...m.entries()].sort((x, y) => y[1] - x[1]);
const L = [];
L.push(`# Releve de spatialRiskBiasMap — ${scenario.nom || "endurance"}, ${TICKS} ticks`);
L.push("");
L.push(`Genere par \`node tools/migration/trace-spatial-risk.mjs -ref <clone> -ticks ${TICKS}\` (mission resource-targets-001).`);
L.push(`${enveloppesPosees} fonctions de la reference enveloppees, ${METHODES.length} methodes de Simulation. Fonctions importees vues par la pile seulement : ${IMPORTES.join(", ")}.`);
L.push("");
L.push(`- appels de \`spatialRiskBiasMap\` : **${appels.length}** (${new Set(appels.map((a) => a.npc)).size} habitants, ticks ${appels[0]?.t ?? "-"} a ${appels.at(-1)?.t ?? "-"})`);
L.push(`- biais non nul : **${biaisNonNuls}** appels`);
L.push(`- tirages \`sim.rng\` sous la preparation : **${tirages}**`);
L.push(`- ecritures de \`building.accessPoints\` sous la preparation : **${ecritures.length}**`);
L.push("");
L.push("## Cibles par but (branches atteintes)");
L.push("");
L.push("| Appels | But :: chemin -> resultat |");
L.push("|---:|---|");
for (const [k, v] of tri(parBut)) L.push(`| ${v} | \`${k.replace(/\|/g, "\\|")}\` |`);
L.push("");
L.push("## Replis surs et budgets (hors cibles)");
L.push("");
L.push("| Appels | Chemin |");
L.push("|---:|---|");
for (const [k, v] of tri(repli)) L.push(`| ${v} | \`${k.replace(/\|/g, "\\|")}\` |`);
L.push("");
L.push("## Biais non nuls");
L.push("");
if (!biais.length) L.push("Aucun.");
for (const b of biais.slice(0, 60)) L.push(`- tick ${b.t}, ${b.npc} : \`${JSON.stringify(b.map)}\``);
if (biais.length > 60) L.push(`- ... (${biais.length - 60} de plus)`);
L.push("");
L.push("## Ecritures des seuils");
L.push("");
if (!ecritures.length) L.push("Aucune.");
for (const e of ecritures) L.push(`- tick ${e.t}, ${e.npc}, ${e.building} : \`${e.avant}\` -> \`${e.apres}\``);
L.push("");
L.push("## Tirages");
L.push("");
if (!sitesTirage.size) L.push("Aucun.");
for (const [k, v] of tri(sitesTirage)) L.push(`- ${v} x \`${k}\``);
L.push("");
const texte = L.join("\n");
if (MD) { mkdirSync(dirname(MD), { recursive: true }); writeFileSync(MD, texte, "utf8"); }
if (JSON_OUT) writeFileSync(JSON_OUT, JSON.stringify({ appels: appels.length, biais, ecritures, parBut: tri(parBut), repli: tri(repli) }, null, 1));
console.log(texte);
