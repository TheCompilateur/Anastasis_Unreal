// TRACE D'EMPREINTES — cote JS (la reference).
//
// Fait tourner le simulateur JS en tete a tete avec lui-meme ou, le jour ou le
// portage aura de quoi repondre, avec Unreal: meme graine, meme pas de temps,
// et une empreinte de l'etat a chaque echantillon. Deux traces se comparent
// avec `compare-digests.mjs`, qui rend le premier tick divergent.
//
//   node tools/migration/emit-state-digests.mjs -seed 33344 -days 3 -out a.jsonl
//
// L'etat vient de `serialize(sim)` — c'est lui, et pas une selection faite ici,
// qui fait autorite. Ses commentaires racontent les divergences deja
// combattues: le `trafficTimer` qui decide de la formation des routes, le cache
// A* dont l'absence change la trajectoire des la premiere requete, la portion
// de repas qui reste reservee sur un batiment apres rechargement. Quelqu'un a
// deja paye le prix de savoir ce qui doit figurer dans un etat canonique; le
// harnais n'a pas a le repayer, ni a decider a sa place.
//
// Options:
//   -ref <chemin>   depot du simulateur JS (defaut: C:/dev/Jeux IV Kingdoms)
//   -seed <n>       graine (defaut 33344)
//   -days <n>       duree en jours de simulation (defaut 1)
//   -ticks <n>      duree en ticks — l'emporte sur -days
//   -dt <x>         pas de temps (defaut 1/30, comme les sondes du depot JS)
//   -every <n>      echantillonner un tick sur n (defaut 1)
//   -drill <tick>   a ce tick, ajouter l'empreinte element par element des
//                   sections-tableaux (acteurs, batiments, animaux)
//   -out <fichier>  trace JSONL (defaut: stdout)
//   -perturb <tick> ajoute UN ULP a `actors[0].x` a ce tick. Instrument de
//                   l'autotest, pas une option de travail: c'est ce qui permet
//                   de verifier que le harnais annonce le bon tick sur une
//                   divergence dont on connait deja la reponse.
//
// UNE TRACE = UN PROCESSUS NEUF. Deux `new Simulation(graine)` dans le meme
// processus ne produisent pas le meme etat: les identifiants du journal de
// village (`logs[].id`, "vlog-24" puis "vlog-48") viennent d'un compteur de
// module, pas de la simulation. Le harnais l'a trouve a son premier essai. Ce
// n'est pas un bug a corriger dans le depot JS — c'est une contrainte sur la
// facon de produire les traces, et l'usage reel (JS d'un cote, Unreal de
// l'autre) y satisfait naturellement.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { pathToFileURL } from "node:url";
import { execFileSync } from "node:child_process";

import { digestState, digestElements, DIGEST_SPEC_VERSION } from "./state-digest.mjs";

const argv = process.argv.slice(2);
const argOf = (flag, fallback) => {
  const i = argv.indexOf(flag);
  return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : fallback;
};

const REF = argOf("-ref", "C:/dev/Jeux IV Kingdoms").split(String.fromCharCode(92)).join("/");
const SEED = Number(argOf("-seed", 33344));
const DT = Number(argOf("-dt", 1 / 30));
const EVERY = Math.max(1, Number(argOf("-every", 1)));
const DRILL = argv.includes("-drill") ? Number(argOf("-drill", -1)) : -1;
const PERTURB = argv.includes("-perturb") ? Number(argOf("-perturb", -1)) : -1;
const OUT = argOf("-out", null);

/** Le double immediatement superieur — un ulp, pas un epsilon choisi au doigt. */
const vueUlp = new DataView(new ArrayBuffer(8));
function ulpSuivant(x) {
  vueUlp.setFloat64(0, x, true);
  const bits = vueUlp.getBigUint64(0, true);
  vueUlp.setBigUint64(0, x >= 0 ? bits + 1n : bits - 1n, true);
  return vueUlp.getFloat64(0, true);
}

const modUrl = (rel) => pathToFileURL(join(REF, rel)).href;
const { Simulation, DAY_LENGTH } = await import(modUrl("src/sim/simulation.js"));
const { serialize } = await import(modUrl("src/sim/save.js"));

const TICKS = argv.includes("-ticks")
  ? Number(argOf("-ticks", 0))
  : Math.round((Number(argOf("-days", 1)) * DAY_LENGTH) / DT);

// Sections-tableaux qui meritent un forage: ce sont celles dont un seul
// element peut diverger sans que le reste bouge.
const SECTIONS_FORABLES = ["actors", "buildings", "animals"];

let refHead = "inconnu";
try {
  refHead = execFileSync("git", ["-C", REF, "log", "-1", "--format=%h"], { encoding: "utf8" }).trim();
} catch { /* pas de git: la provenance sera moins precise, la trace reste valide */ }

const lignes = [];
lignes.push(JSON.stringify({
  kind: "header",
  spec: DIGEST_SPEC_VERSION,
  source: "js",
  ref: REF,
  refHead,
  seed: SEED,
  dt: DT,
  ticks: TICKS,
  every: EVERY,
  dayLength: DAY_LENGTH,
  emittedAt: new Date().toISOString(),
}));

const sim = new Simulation(SEED);

// Tick 0 = l'etat au sortir de la genese, avant le premier pas. C'est le seul
// tick dont une divergence accuse la GENERATION du monde et non la boucle —
// le distinguer evite de chercher un bug de simulation la ou il n'y en a pas.
function echantillon(t) {
  const etat = serialize(sim);
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

if (PERTURB === 0 && sim.actors?.[0]) sim.actors[0].x = ulpSuivant(sim.actors[0].x);
echantillon(0);
for (let t = 1; t <= TICKS; t += 1) {
  sim.tick(DT);
  if (sim._dayDeferred?.length) sim.flushDayDeferred();
  if (t === PERTURB && sim.actors?.[0]) sim.actors[0].x = ulpSuivant(sim.actors[0].x);
  if (t % EVERY === 0 || t === DRILL) echantillon(t);
}

const sortie = lignes.join("\n") + "\n";
if (OUT) {
  mkdirSync(dirname(OUT), { recursive: true });
  writeFileSync(OUT, sortie, "utf8");
  console.error(`Ecrit: ${OUT} — ${lignes.length - 1} echantillons sur ${TICKS} ticks (graine ${SEED})`);
} else {
  process.stdout.write(sortie);
}
