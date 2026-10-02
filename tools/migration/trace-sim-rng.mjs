// RELEVE DES TIRAGES `sim.rng` — cote JS (la reference), mission sim-rng-001.
//
// Le C++ n'a pas encore de flux `sim.rng` branche sur les decisions (ecarts n°1 et
// n°16). Avant de brancher quoi que ce soit, il faut savoir CE QUI tire dans le
// scenario du harnais, dans quel ordre et combien de fois : une table de scores a
// 14 bruits fixes est precedee, dans la reference, de tirages dont le nombre
// depend de l'etat (exploreTarget, intention du jour, ambition...). Ce script le
// MESURE au lieu de le deduire.
//
//   node tools/migration/trace-sim-rng.mjs -ref <clone anastasis-ref-p3> [-ticks 600]
//        [-scenario tools/migration/scenarios/endurance.json]
//        [-md docs/migration/phase3/P3_RNG_RELEVE.md] [-csv docs/migration/phase3/P3_RNG_RELEVE_600.csv]
//
// La mesure vit dans rng-trace-lib.mjs (partagee avec gen-goal-noise-vectors.mjs) :
// chargement, masques et tick d'emit-state-digests.mjs, `sim.rng` et `ref.updateNpc`
// enveloppes, rien d'autre change dans la simulation.
//
// Pour chaque tirage : le tick, l'etape du tick (etiquettes de masks.mjs),
// l'habitant en cours, le SITE (premiere fonction de la reference sur la pile,
// fichier:ligne) et le CHEMIN (les fonctions de la reference depuis updateNpc).
// Une DECISION est la suite des tirages d'un meme habitant, au meme tick, dont le
// chemin passe par `chooseGoal`.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import { releverTirages, decisionsDe } from "./rng-trace-lib.mjs";
import { chargerScenario, provenanceReference } from "./scenarios/scenario-format.mjs";
const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");

const argv = process.argv.slice(2);
const argOf = (flag, fallback) => {
  const i = argv.indexOf(flag);
  return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : fallback;
};
const REF = argOf("-ref", null);
if (!REF) {
  console.error("-ref <clone de anastasis-ref-p3> requis (docs/migration/phase3/REFERENCE_JS.md)");
  process.exit(2);
}
const REF_DIR = REF.split(String.fromCharCode(92)).join("/");
const SCENARIO_CHEMIN = argOf("-scenario", join(ICI, "scenarios", "endurance.json"));
const TICKS = Number(argOf("-ticks", 600));
const MD = argOf("-md", join(RACINE, "docs", "migration", "phase3", "P3_RNG_RELEVE.md"));
const CSV = argOf("-csv", join(RACINE, "docs", "migration", "phase3", `P3_RNG_RELEVE_${TICKS}.csv`));
const TICK_FORE = Number(argOf("-fore", 32));

const provenance = provenanceReference(REF_DIR);
const scenario = chargerScenario(SCENARIO_CHEMIN);
if (provenance.modifie) {
  console.error(`REFUS: ${REF_DIR} porte ${provenance.modifie} modification(s) sous src/ (REFERENCE_JS.md).`);
  process.exit(3);
}
if (scenario.reference?.commit && provenance.commit && scenario.reference.commit !== provenance.commit) {
  console.error(`REFUS: scenario construit contre ${scenario.reference.commit}, -ref est ${provenance.commit}.`);
  process.exit(3);
}

const { tirages, etatDepart, etatFin } = await releverTirages(REF_DIR, scenario, TICKS);

// --- Agregats ------------------------------------------------------------------

const parSite = new Map();
for (const d of tirages) parSite.set(d.site, (parSite.get(d.site) || 0) + 1);
const parEtape = new Map();
for (const d of tirages) parEtape.set(d.etape, (parEtape.get(d.etape) || 0) + 1);

const decisions = decisionsDe(tirages);
const resume = (liste) => {
  // Suite des sites, les repetitions consecutives comprimees : `exploreTarget x6`.
  const out = [];
  for (const d of liste) {
    const nom = d.site.split(" ")[0];
    const dernier = out[out.length - 1];
    if (dernier && dernier.nom === nom) dernier.n += 1;
    else out.push({ nom, n: 1 });
  }
  return out.map((s) => (s.n > 1 ? `${s.nom} x${s.n}` : s.nom)).join(", ");
};

const parNombre = new Map();
for (const d of decisions) parNombre.set(d.tirages.length, (parNombre.get(d.tirages.length) || 0) + 1);

// --- Sorties --------------------------------------------------------------------

mkdirSync(dirname(CSV), { recursive: true });
const csv = ["tick;etape;habitant;decision;etat_avant;site;appelant;chemin"];
for (const d of tirages) {
  csv.push([d.t, d.etape, d.npc ?? "", d.decision ? 1 : 0, d.etat, d.site, d.appelant, d.chemin].join(";"));
}
writeFileSync(CSV, csv.join("\n") + "\n", "utf8");

const premierFore = tirages.find((d) => d.t >= TICK_FORE);
const L = [];
const emit = (s = "") => L.push(s);
emit(`# Relevé des tirages \`sim.rng\` — scénario \`${scenario.name}\`, ${TICKS} ticks`);
emit("");
emit("Généré par `tools/migration/trace-sim-rng.mjs` (mission sim-rng-001). Ne pas éditer à la main : relancer.");
emit("");
emit("```bash");
emit(`node tools/migration/trace-sim-rng.mjs -ref <clone anastasis-ref-p3> -ticks ${TICKS}`);
emit("```");
emit("");
emit(`- Référence : \`${provenance.court ?? provenance.commit}\`${provenance.tag ? ` (tag \`${provenance.tag}\`)` : ""}, scénario \`${scenario.name}\` `
  + `(graine ${scenario.seed}, dt ${scenario.dt}, vue ${scenario.vue ? `(${scenario.vue.x}, ${scenario.vue.y})` : "aucune"}), `
  + `${scenario.masques.length} masques, tick recomposé.`);
emit(`- État \`sim.rng\` : ${etatDepart} au départ, ${etatFin} après ${TICKS} ticks.`);
emit(`- **${tirages.length} tirages**, dont ${tirages.filter((d) => d.decision).length} dans \`chooseGoal\`, en **${decisions.length} décisions**.`);
emit(`- Tirages bruts : \`${CSV.split(String.fromCharCode(92)).join("/").split("/docs/").pop() ? `docs/${CSV.split(String.fromCharCode(92)).join("/").split("/docs/").pop()}` : CSV}\` (un par ligne : tick, étape, habitant, décision, état avant, site, appelant, chemin).`);
emit("");
emit(`## Premier tirage au tick ${TICK_FORE} ou après`);
emit("");
if (premierFore) {
  emit(`- tick ${premierFore.t}, étape \`${premierFore.etape}\`, habitant \`${premierFore.npc ?? "-"}\`, état avant ${premierFore.etat}`);
  emit(`- site : \`${premierFore.site}\``);
  emit(`- chemin : \`${premierFore.chemin}\``);
  const memeTick = tirages.filter((d) => d.t === premierFore.t);
  emit(`- ${memeTick.length} tirage(s) à ce tick : ${resume(memeTick)}`);
} else {
  emit("Aucun tirage.");
}
emit("");
emit("## Par site (fonction et ligne de la référence qui appelle `sim.rng`)");
emit("");
emit("| Tirages | Site |");
emit("| ---: | --- |");
for (const [site, n] of [...parSite].sort((a, b) => b[1] - a[1])) emit(`| ${n} | \`${site}\` |`);
emit("");
emit("## Par étape du tick");
emit("");
emit("| Tirages | Étape |");
emit("| ---: | --- |");
for (const [e, n] of [...parEtape].sort((a, b) => b[1] - a[1])) emit(`| ${n} | \`${e}\` |`);
emit("");
emit("## Tirages par décision (`chooseGoal`)");
emit("");
emit("| Tirages | Décisions |");
emit("| ---: | ---: |");
for (const [n, k] of [...parNombre].sort((a, b) => a[0] - b[0])) emit(`| ${n} | ${k} |`);
emit("");
emit("## Les décisions, une par ligne");
emit("");
emit("Sites dans l'ordre des tirages ; `x6` = six tirages consécutifs au même site.");
emit("");
emit("| Tick | Habitant | État avant | Tirages | Sites |");
emit("| ---: | --- | ---: | ---: | --- |");
for (const d of decisions) emit(`| ${d.t} | \`${d.npc}\` | ${d.etatAvant} | ${d.tirages.length} | ${resume(d.tirages)} |`);
emit("");
emit("## Tirages hors décision");
emit("");
const hors = tirages.filter((d) => !d.decision);
if (hors.length === 0) emit("Aucun.");
else {
  emit("| Tick | Étape | Habitant | Site | Chemin |");
  emit("| ---: | --- | --- | --- | --- |");
  for (const d of hors.slice(0, 400)) emit(`| ${d.t} | \`${d.etape}\` | \`${d.npc ?? "-"}\` | \`${d.site}\` | \`${d.chemin}\` |`);
  if (hors.length > 400) emit(`| … | | | ${hors.length - 400} de plus dans le CSV | |`);
}
mkdirSync(dirname(MD), { recursive: true });
writeFileSync(MD, L.join("\n") + "\n", "utf8");
console.error(`Ecrit: ${MD}\n       ${CSV}\n${tirages.length} tirages, ${decisions.length} decisions, ${TICKS} ticks`);
