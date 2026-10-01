// BRAS D'EXPERIENCE — un passage de la reference JS, avec ou sans un ecart reinjecte.
//
// Le laboratoire (labo.py) lance deux bras par replique : `-ecart aucun` (temoin) et
// `-ecart nNN` (la reference + l'ecart). Ce script ne juge rien : il fait tourner la
// reference et ecrit ce qu'il voit, en JSONL, au format `labo-releve` v1 (LISEZMOI.md).
// Les grandeurs, les statistiques et le verdict sont calcules en Python, sur ce releve.
// Le meme format, ecrit par le harnais C++, servira au niveau 3 (le residu).
//
//   node tools/migration/labo_ecarts/bras.mjs -ref <anastasis-ref-p3> -terrain endurance \
//        -replique 3 -jours 3 -ecart n05 -out releve.jsonl
//
// Options :
//   -ref <chemin>      checkout PROPRE du tag anastasis-ref-p3 (REFERENCE_JS.md) ; refuse sinon
//   -terrain <nom>     `endurance` : le scenario du harnais (tools/migration/scenarios/endurance.json),
//                      charge par deserialize, ses 24 masques appliques — le contexte ou le C++ est juge.
//                      `genese` : `new Simulation(graine)`, la reference entiere, aucun masque — le jeu.
//   -replique <k>      endurance : k = 0 garde l'etat `rng` du scenario ; k > 0 pose
//                      `sim.rng.setState(graineReplique(k))`, meme village, autre suite de tirages.
//                      genese : graine du monde = 1000 + k.
//   -jours <D>         duree (jours de simulation, DAY_LENGTH de la reference)
//   -reseed <s>        apres la mise en place, `sim.rng.setState(graineReplique(s))` : meme etat de
//                      depart, suite de tirages independante. Sert au temoin-bis, qui mesure ce
//                      qu'est une trajectoire « completement decorrelee » (labo.py, decorrelation).
//   -ecart <id>        aucun | n05 | n16 | vue_village (injections.mjs)
//   -every <K>         un releve tous les K ticks (defaut 60 = 1 s simulee a dt 1/60)
//   -empreinte-every <M>  empreinte COMPLETE (serialize + digestState, perimetre du terrain)
//                      tous les M ticks (defaut 900) ; l'empreinte REDUITE est calculee a chaque tick
//   -out <fichier>     releve JSONL
//
// Pas de temps : celui du scenario (1/60) pour les deux terrains, et le tick RECOMPOSE de
// masks.mjs dans les deux cas (sans masque, il rend exactement le tick de reference :
// selftest-harness.mjs le prouve). C'est lui qui donne prise a la reinjection n05.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { Digest, digestState } from "../state-digest.mjs";
import { chargerReference, appliquerMasques } from "../scenarios/masks.mjs";
import { chargerScenario, provenanceReference } from "../scenarios/scenario-format.mjs";
import { INJECTIONS } from "./injections.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const argOf = (flag, fallback) => {
  const i = argv.indexOf(flag);
  return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : fallback;
};
const refuser = (message, code = 3) => {
  console.error(`REFUS: ${message}`);
  process.exit(code);
};

export const FORMAT_RELEVE = 1;
const REF = (argOf("-ref", null) || refuser("-ref <checkout du tag anastasis-ref-p3> est obligatoire", 2))
  .split(String.fromCharCode(92)).join("/");
const TERRAIN = argOf("-terrain", "endurance");
const REPLIQUE = Number(argOf("-replique", 0));
const JOURS = Number(argOf("-jours", 1));
const ECART = argOf("-ecart", "aucun");
const EVERY = Math.max(1, Number(argOf("-every", 60)));
const EMPREINTE_EVERY = Math.max(1, Number(argOf("-empreinte-every", 900)));
const OUT = argOf("-out", null) || refuser("-out <fichier> est obligatoire", 2);
if (!["endurance", "genese"].includes(TERRAIN)) refuser(`-terrain ${TERRAIN} : attendu endurance | genese`, 2);
if (!INJECTIONS[ECART]) refuser(`-ecart ${ECART} : connus ${Object.keys(INJECTIONS).join(", ")}`, 2);
if (!Number.isInteger(REPLIQUE) || REPLIQUE < 0) refuser(`-replique ${REPLIQUE} : entier >= 0`, 2);

const provenance = provenanceReference(REF);
if (provenance.modifie) refuser(`${REF} porte ${provenance.modifie} modification(s) sous src/ : checkout propre du tag.`);

/** Graine de la suite `sim.rng` d'une replique du scenario (k > 0). Fixe, ecrite ici, rejouable. */
export function graineReplique(k) {
  return (Math.imul(k, 0x9e3779b1) ^ 0x5bd1e995) >>> 0;
}

// `ref` est une COPIE : une reinjection peut y remplacer une etape de tickRecompose sans
// toucher l'objet charge (un seul bras par processus de toute facon : etat de module).
const ref = { ...(await chargerReference(REF)) };
let sim;
let perimetre;
let masques = [];
let graine;
if (TERRAIN === "endurance") {
  const scenario = chargerScenario(join(ICI, "..", "scenarios", "endurance.json"));
  if (scenario.reference?.commit !== provenance.commit) {
    refuser(`scenario construit contre ${scenario.reference?.commit}, -ref est ${provenance.commit}`);
  }
  if (Math.abs(scenario.dt - 1 / 60) > 1e-15) refuser(`dt du scenario ${scenario.dt}, attendu 1/60`);
  sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
  ref.deserialize(sim, JSON.parse(JSON.stringify(scenario.save)));
  if (REPLIQUE > 0) sim.rng.setState(graineReplique(REPLIQUE));
  perimetre = scenario.sections;
  masques = scenario.masques;
  graine = scenario.seed;
} else {
  graine = 1000 + REPLIQUE;
  sim = new ref.Simulation(graine);
  perimetre = null; // toutes les sections de serialize
}
const RESEED = argv.includes("-reseed") ? Number(argOf("-reseed")) : null;
if (RESEED !== null) {
  if (!Number.isInteger(RESEED) || RESEED <= 0) refuser(`-reseed ${RESEED} : entier > 0`, 2);
  sim.rng.setState(graineReplique(RESEED));
}
const DT = 1 / 60;
const TICKS = Math.round((JOURS * ref.DAY_LENGTH) / DT);

const compteur = await INJECTIONS[ECART].appliquer(sim, ref, REF);
const { tick } = appliquerMasques(sim, ref, masques, { recompose: true });

// --- Ce qu'on releve ---------------------------------------------------------
// Des valeurs brutes, sans interpretation : labo.py (observables.py) en tire les grandeurs.

const num = (v) => (Number.isFinite(v) ? v : null);
function releveHabitant(n) {
  const rel = n.relations && typeof n.relations === "object" ? Object.values(n.relations).filter(Number.isFinite) : [];
  const spots = n.mind?.spots ? Object.values(n.mind.spots) : [];
  return {
    id: n.id,
    job: n.jobId ?? null,
    stage: n.lifeStage ?? null,
    x: num(n.x),
    y: num(n.y),
    goal: n.goal ?? null,
    act: n.activity ?? null,
    in: n.inside ? 1 : 0,
    hu: num(n.hunger),
    th: num(n.thirst),
    en: num(n.energy),
    so: num(n.social),
    le: num(n.leisure),
    hy: num(n.hygiene),
    he: num(n.health),
    mo: num(n.morale),
    food: num(n.inventory?.food) ?? 0,
    gold: num(n.gold) ?? 0,
    relN: rel.length,
    relSum: rel.reduce((a, v) => a + v, 0),
    relPos: rel.filter((v) => v > 0).length,
    spots: spots.length,
    ouiDire: spots.filter((s) => s?.hearsay).length,
    home: n.home?.id ?? n.homeId ?? null,
  };
}
function releveBatiment(b) {
  const f = b.stock?.food;
  return {
    id: b.id,
    type: b.type,
    progress: num(b.progress),
    food: f && typeof f === "object" ? num(f.physical) ?? 0 : num(f) ?? 0,
  };
}
function releveTuiles() {
  const somme = { food: 0, wood: 0, stone: 0 };
  for (const t of sim.tiles) if (t.resource && somme[t.resource] !== undefined) somme[t.resource] += t.amount || 0;
  return somme;
}

/** Empreinte REDUITE, a chaque tick : flux rng + etat dynamique des habitants. Bon marche. */
function empreinteReduite() {
  const d = new Digest();
  d.number(sim.rng.state());
  for (const n of sim.actors) {
    d.string(String(n.id));
    for (const v of [n.x, n.y, n.hunger, n.thirst, n.energy, n.social, n.leisure, n.hygiene, n.health, n.morale,
      n._simBudgetAccum ?? -1]) d.number(Number(v));
    d.string(String(n.goal));
    d.number(n.inside ? 1 : 0);
  }
  return d.hex();
}
/** Empreinte COMPLETE : celle du harnais, sur le perimetre du terrain. Couteuse (~30 ms). */
function empreinteComplete() {
  const etat = ref.serialize(sim);
  const garder = perimetre ?? Object.keys(etat);
  return digestState(Object.fromEntries(garder.map((k) => [k, etat[k]]))).global;
}

const lignes = [];
lignes.push(JSON.stringify({
  kind: "labo-releve",
  format: FORMAT_RELEVE,
  source: "js",
  refCommit: provenance.commit,
  refTag: provenance.tag,
  terrain: TERRAIN,
  replique: REPLIQUE,
  graine,
  rngReplique: TERRAIN === "endurance" && REPLIQUE > 0 ? graineReplique(REPLIQUE) : null,
  reseed: RESEED !== null ? { s: RESEED, etat: graineReplique(RESEED) } : null,
  ecart: ECART,
  injection: INJECTIONS[ECART].description,
  masques: masques.length,
  perimetre: perimetre ?? "tout serialize",
  dt: DT,
  dayLength: ref.DAY_LENGTH,
  ticks: TICKS,
  every: EVERY,
  empreinteEvery: EMPREINTE_EVERY,
}));

const empreintes = []; // [t, reduite] a chaque tick ; la complete dans les releves
const ecrireReleve = (t) => {
  const ligne = {
    t,
    day: sim.day,
    time: sim.time,
    actors: sim.actors.map(releveHabitant),
    buildings: sim.buildings.map(releveBatiment),
    tiles: releveTuiles(),
    treasury: num(sim.colony?.treasury),
  };
  if (t % EMPREINTE_EVERY === 0) ligne.empreinte = empreinteComplete();
  lignes.push(JSON.stringify(ligne));
};

const debut = Date.now();
empreintes.push(empreinteReduite());
ecrireReleve(0);
for (let t = 1; t <= TICKS; t += 1) {
  tick(DT);
  empreintes.push(empreinteReduite());
  if (t % EVERY === 0 || t === TICKS) ecrireReleve(t);
}
lignes.push(JSON.stringify({
  kind: "fin",
  ticks: TICKS,
  activation: compteur,
  empreintesReduites: empreintes,
  secondes: (Date.now() - debut) / 1000,
}));

mkdirSync(dirname(OUT), { recursive: true });
writeFileSync(OUT, lignes.join("\n") + "\n", "utf8");
console.error(`Ecrit: ${OUT} — ${TERRAIN} r${REPLIQUE} ${ECART}, ${TICKS} ticks, ${((Date.now() - debut) / 1000).toFixed(1)} s, activation ${JSON.stringify(compteur)}`);
