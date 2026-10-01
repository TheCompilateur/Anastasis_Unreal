// TRACE D'EMPREINTES — cote JS (la reference).
//
// Fait tourner le simulateur JS en tete a tete avec lui-meme ou, le jour ou le
// portage aura de quoi repondre, avec Unreal: meme graine, meme pas de temps,
// et une empreinte de l'etat a chaque echantillon. Deux traces se comparent
// avec `compare-digests.mjs`, qui rend le premier tick divergent.
//
//   node tools/migration/emit-state-digests.mjs -seed 33344 -days 3 -out a.jsonl
//   node tools/migration/emit-state-digests.mjs -scenario tools/migration/scenarios/endurance.json -days 3 -out a.jsonl
//
// L'etat vient de `serialize(sim)` — c'est lui, et pas une selection faite ici,
// qui fait autorite. Ses commentaires racontent les divergences deja
// combattues: le `trafficTimer` qui decide de la formation des routes, le cache
// A* dont l'absence change la trajectoire des la premiere requete, la portion
// de repas qui reste reservee sur un batiment apres rechargement. Quelqu'un a
// deja paye le prix de savoir ce qui doit figurer dans un etat canonique; le
// harnais n'a pas a le repayer, ni a decider a sa place.
//
// Deux modes:
//
//   sans scenario   `new Simulation(graine)` : la genese complete de la
//                   reference (fondateurs, sagas, economie, Kosmos). La file de
//                   minuit est videe apres chaque tick (`flushDayDeferred`).
//   -scenario       l'etat de depart est une SAUVEGARDE JS chargee par
//                   `deserialize` (build-scenario.mjs), et les systemes que le
//                   C++ ne porte pas sont MASQUES de l'exterieur
//                   (scenarios/masks.mjs). Graine, pas de temps, mode de la file
//                   de minuit et masques viennent du scenario; l'en-tete de la
//                   trace porte son nom, son empreinte, son perimetre et la liste
//                   des masques, et le comparateur refuse deux traces qui ne les
//                   partagent pas.
//
// Options:
//   -ref <chemin>   depot du simulateur JS (defaut: C:/dev/Jeux IV Kingdoms).
//                   Passer un checkout propre du tag de reference
//                   (docs/migration/phase3/REFERENCE_JS.md).
//   -seed <n>       graine (defaut 33344 ; avec -scenario, celle du scenario)
//   -days <n>       duree en jours de simulation (defaut 1)
//   -ticks <n>      duree en ticks — l'emporte sur -days
//   -dt <x>         pas de temps (defaut 1/30 ; avec -scenario, celui du scenario)
//   -every <n>      echantillonner un tick sur n (defaut 1)
//   -drill <tick>   a ce tick, ajouter l'empreinte element par element des
//                   sections-tableaux (acteurs, batiments, animaux)
//   -out <fichier>  trace JSONL (defaut: stdout)
//   -scenario <f>   scenario JSON (build-scenario.mjs)
//   -sans-masques   charger le scenario SANS appliquer ses masques (l'en-tete le dit)
//   -tick <mode>    `reference` (sim.tick) ou `recompose` (scenarios/masks.mjs,
//                   tickRecompose). Par defaut: recompose des qu'un masque de tick
//                   est actif. Instrument de l'autotest: il prouve qu'un tick
//                   recompose sans masque EST le tick de reference.
//   -audit-rng      ne pas tracer: compter les tirages de sim.rng par etape du
//                   tick, et rendre le compte en JSON sur stdout
//   -allow-dirty    accepter, avec -scenario, une reference dont `src/` est modifie
//   -perturb <tick> ajoute UN ULP a un nombre de l'etat a ce tick. Instrument de
//                   l'autotest, pas une option de travail: c'est ce qui permet
//                   de verifier que le harnais annonce le bon tick sur une
//                   divergence dont on connait deja la reponse.
//   -perturb-path <chemin>  le nombre perturbe (defaut `actors.0.x`), chemin
//                   pointe dans `sim` : `colony.treasury`, `actors.1.needs.hunger`…
//   -dump <tick>    forage: a ce tick, ecrire l'etat (`serialize`) en JSON dans
//   -dump-out <f>   <f> — les sections du perimetre du scenario, ou toutes sans
//                   scenario. A comparer champ par champ (diff-states.mjs).
//
// UNE TRACE = UN PROCESSUS NEUF. Deux `new Simulation(graine)` dans le meme
// processus ne produisent pas le meme etat: les identifiants du journal de
// village (`logs[].id`, "vlog-24" puis "vlog-48") viennent d'un compteur de
// module, pas de la simulation. Le harnais l'a trouve a son premier essai. Ce
// n'est pas un bug a corriger dans le depot JS — c'est une contrainte sur la
// facon de produire les traces, et l'usage reel (JS d'un cote, Unreal de
// l'autre) y satisfait naturellement.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname } from "node:path";

import { digestState, digestElements, DIGEST_SPEC_VERSION } from "./state-digest.mjs";
import { chargerReference, appliquerMasques, compterTirages } from "./scenarios/masks.mjs";
import { chargerScenario, provenanceReference } from "./scenarios/scenario-format.mjs";

const argv = process.argv.slice(2);
const argOf = (flag, fallback) => {
  const i = argv.indexOf(flag);
  return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : fallback;
};
const refuser = (message, code = 3) => {
  console.error(`REFUS: ${message}`);
  process.exit(code);
};

const REF = argOf("-ref", "C:/dev/Jeux IV Kingdoms").split(String.fromCharCode(92)).join("/");
const SCENARIO_CHEMIN = argOf("-scenario", null);
const SANS_MASQUES = argv.includes("-sans-masques");
const AUDIT = argv.includes("-audit-rng");
const MODE_TICK = argOf("-tick", null);
const EVERY = Math.max(1, Number(argOf("-every", 1)));
const DRILL = argv.includes("-drill") ? Number(argOf("-drill", -1)) : -1;
const PERTURB = argv.includes("-perturb") ? Number(argOf("-perturb", -1)) : -1;
const PERTURB_PATH = argOf("-perturb-path", "actors.0.x");
const OUT = argOf("-out", null);
const DUMP = argv.includes("-dump") ? Number(argOf("-dump", -1)) : -1;
const DUMP_OUT = argOf("-dump-out", null);
if (DUMP >= 0 && !DUMP_OUT) refuser("-dump demande -dump-out", 2);
if (MODE_TICK && !["reference", "recompose"].includes(MODE_TICK)) refuser(`-tick ${MODE_TICK} : attendu reference | recompose`, 2);
if (!SCENARIO_CHEMIN && (SANS_MASQUES || AUDIT || MODE_TICK)) refuser("-sans-masques, -audit-rng et -tick demandent -scenario", 2);

/** Le double immediatement superieur — un ulp, pas un epsilon choisi au doigt. */
const vueUlp = new DataView(new ArrayBuffer(8));
function ulpSuivant(x) {
  vueUlp.setFloat64(0, x, true);
  const bits = vueUlp.getBigUint64(0, true);
  vueUlp.setBigUint64(0, x >= 0 ? bits + 1n : bits - 1n, true);
  return vueUlp.getFloat64(0, true);
}

/** Un ulp sur le nombre au bout de `chemin` dans `sim`. Leve si ce n'est pas un nombre fini. */
function perturber(sim, chemin) {
  const parts = chemin.split(".");
  let o = sim;
  for (const p of parts.slice(0, -1)) {
    o = o?.[p];
    if (o == null) throw new Error(`-perturb-path ${chemin} : \`${p}\` absent`);
  }
  const cle = parts[parts.length - 1];
  if (!Number.isFinite(o[cle])) throw new Error(`-perturb-path ${chemin} : pas un nombre fini (${o[cle]})`);
  o[cle] = ulpSuivant(o[cle]);
}

const provenance = provenanceReference(REF);
const scenario = SCENARIO_CHEMIN ? chargerScenario(SCENARIO_CHEMIN) : null;
if (scenario) {
  if (provenance.modifie && !argv.includes("-allow-dirty")) {
    refuser(`${REF} porte ${provenance.modifie} modification(s) non commitee(s) sous src/ : le scenario `
      + `a ete construit contre ${scenario.reference?.commit}. Checkout propre du tag (REFERENCE_JS.md), ou -allow-dirty.`);
  }
  if (scenario.reference?.commit && provenance.commit && scenario.reference.commit !== provenance.commit) {
    refuser(`scenario construit contre ${scenario.reference.commit}, -ref est ${provenance.commit}.`);
  }
  if (argv.includes("-seed") && Number(argOf("-seed")) !== scenario.seed) {
    refuser(`-seed ${argOf("-seed")} contredit la graine du scenario (${scenario.seed}).`, 2);
  }
}

const ref = await chargerReference(REF);
const { serialize, deserialize, DAY_LENGTH } = ref;

const SEED = scenario ? scenario.seed : Number(argOf("-seed", 33344));
const DT = argv.includes("-dt") ? Number(argOf("-dt")) : scenario ? scenario.dt : 1 / 30;
const TICKS = argv.includes("-ticks")
  ? Number(argOf("-ticks", 0))
  : Math.round((Number(argOf("-days", 1)) * DAY_LENGTH) / DT);
const MODE_FILE = scenario ? scenario.dayDeferred : "flush";
const MASQUES_ACTIFS = scenario && !SANS_MASQUES ? [...scenario.masques] : [];

// Sections-tableaux qui meritent un forage: ce sont celles dont un seul
// element peut diverger sans que le reste bouge.
const SECTIONS_FORABLES = ["actors", "buildings", "animals"];

// --- L'etat de depart -------------------------------------------------------

let sim;
let tick;
let modeTick = "reference";
let avertissementsChargement = 0;
let etiquette = null;
if (scenario) {
  sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
  const { warnings } = deserialize(sim, JSON.parse(JSON.stringify(scenario.save)));
  avertissementsChargement = warnings.length;
  const applique = appliquerMasques(sim, ref, MASQUES_ACTIFS, {
    recompose: MODE_TICK === "recompose",
    etiqueteur: AUDIT ? { courante: "hors-tick", poser(e) { const p = this.courante; this.courante = e; return p; } } : null,
  });
  if (MODE_TICK === "reference" && applique.recompose) {
    refuser("-tick reference : un masque de tick (ou l'audit) exige le tick recompose.", 2);
  }
  tick = applique.tick;
  modeTick = applique.recompose ? "recompose" : "reference";
  etiquette = applique.etiquette;
} else {
  sim = new ref.Simulation(SEED);
  tick = (dt) => sim.tick(dt);
}

const pas = (t) => {
  tick(DT);
  if (MODE_FILE === "flush" && sim._dayDeferred?.length) sim.flushDayDeferred();
  if (t === PERTURB) perturber(sim, PERTURB_PATH);
};

// --- Audit des tirages ------------------------------------------------------

if (AUDIT) {
  const compte = compterTirages(sim, etiquette);
  for (let t = 1; t <= TICKS; t += 1) pas(t);
  const total = Object.values(compte).reduce((a, n) => a + n, 0);
  const tirages = Object.fromEntries(Object.entries(compte).sort((a, b) => b[1] - a[1]));
  process.stdout.write(JSON.stringify({ ticks: TICKS, dt: DT, masques: MASQUES_ACTIFS, total, tirages }) + "\n");
  process.exit(0);
}

// --- La trace ---------------------------------------------------------------

const lignes = [];
lignes.push(JSON.stringify({
  kind: "header",
  spec: DIGEST_SPEC_VERSION,
  source: "js",
  ref: REF,
  refHead: provenance.court ?? "inconnu",
  refCommit: provenance.commit,
  refTag: provenance.tag,
  refModifie: provenance.modifie,
  seed: SEED,
  dt: DT,
  ticks: TICKS,
  every: EVERY,
  dayLength: DAY_LENGTH,
  dayDeferred: MODE_FILE,
  tick: modeTick,
  scenario: scenario ? {
    name: scenario.name,
    empreinte: scenario.empreinte,
    masques: MASQUES_ACTIFS,
    sections: scenario.sections,
    avertissementsChargement,
  } : null,
  perturb: PERTURB >= 0 ? { tick: PERTURB, path: PERTURB_PATH } : null,
  emittedAt: new Date().toISOString(),
}));

// Tick 0 = l'etat au sortir de la genese (ou du chargement), avant le premier
// pas. C'est le seul tick dont une divergence accuse la GENERATION du monde (ou
// le LECTEUR de sauvegarde) et non la boucle — le distinguer evite de chercher
// un bug de simulation la ou il n'y en a pas.
function echantillon(t) {
  const etat = serialize(sim);
  if (t === DUMP) {
    const garder = scenario ? scenario.sections : Object.keys(etat);
    const vidage = Object.fromEntries(garder.map((k) => [k, etat[k]]));
    mkdirSync(dirname(DUMP_OUT), { recursive: true });
    writeFileSync(DUMP_OUT, JSON.stringify(vidage), "utf8");
  }
  const { global, sections } = digestState(etat);
  const ligne = { t, day: sim.day, time: etat.time, g: global, s: sections };
  if (t === DRILL) {
    ligne.drill = {};
    for (const nom of SECTIONS_FORABLES) {
      const el = digestElements(etat[nom]);
      if (el) ligne.drill[nom] = el;
    }
  }
  lignes.push(JSON.stringify(ligne));
}

if (PERTURB === 0) perturber(sim, PERTURB_PATH);
echantillon(0);
for (let t = 1; t <= TICKS; t += 1) {
  pas(t);
  if (t % EVERY === 0 || t === DRILL || t === DUMP) echantillon(t);
}

const sortie = lignes.join("\n") + "\n";
if (OUT) {
  mkdirSync(dirname(OUT), { recursive: true });
  writeFileSync(OUT, sortie, "utf8");
  console.error(`Ecrit: ${OUT} — ${lignes.length - 1} echantillons sur ${TICKS} ticks (graine ${SEED}`
    + (scenario ? `, scenario ${scenario.name}, ${MASQUES_ACTIFS.length} masques, tick ${modeTick}` : "") + ")");
} else {
  process.stdout.write(sortie);
}
