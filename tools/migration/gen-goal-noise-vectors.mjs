// Vecteurs du bruit de decision (`goalNoise`, src/sim/npc.js) — mission sim-rng-001.
//
//   node tools/migration/gen-goal-noise-vectors.mjs -ref <clone anastasis-ref-p3> [-ticks 5400]
//
// `goalNoise` n'est pas exporte par npc.js. On l'execute quand meme TEL QU'IL EST
// ECRIT : sa source est extraite du fichier de la reference et evaluee avec le
// `GOAL_AI` exporte par le module. Aucune formule n'est recopiee ici.
//
// Trois familles de vecteurs :
//   1. la fonction pure : graine, amplitude -> valeur (motif binaire) et etat apres ;
//   2. les amplitudes de la SOURCE : chaque appel `goalNoise(sim, N)` de npc.js,
//      par ligne — le C++ verifie que chaque entree de sa table pointe une ligne qui
//      porte cette amplitude ;
//   3. les decisions MESUREES du scenario endurance (rng-trace-lib.mjs) : pour chaque
//      `chooseGoal`, l'etat du flux au premier bruit, et la suite (ligne appelante,
//      amplitude, valeur) des bruits dans l'ordre ou la reference les a tires. Le C++
//      refait la suite depuis cet etat avec SA table, dans SON ordre.

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { releverTirages, decisionsDe } from "./rng-trace-lib.mjs";
import { chargerScenario } from "./scenarios/scenario-format.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");
const argv = process.argv.slice(2);
const argOf = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : d; };
const REF = argOf("-ref", null);
if (!REF) {
  console.error("-ref <clone de anastasis-ref-p3> requis");
  process.exit(2);
}
const REF_DIR = REF.split(String.fromCharCode(92)).join("/");
const TICKS = Number(argOf("-ticks", 5400));
const SORTIE = argOf("-out", join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisGoalNoiseVectors.inl"));

const url = (rel) => pathToFileURL(join(REF_DIR, rel)).href;
const npcModule = await import(url("src/sim/npc.js"));
const { makeRng } = await import(url("src/sim/rng.js"));

// --- goalNoise de la reference, execute tel quel ---------------------------------

const SOURCE_NPC = readFileSync(join(REF_DIR, "src/sim/npc.js"), "utf8");
const m = SOURCE_NPC.match(/function goalNoise\(sim, amp = 10\) \{([\s\S]*?)\n\}/);
if (!m) throw new Error("goalNoise introuvable dans src/sim/npc.js");
const goalNoise = new Function("GOAL_AI", `return function goalNoise(sim, amp = 10) {${m[1]}\n};`)(npcModule.GOAL_AI);

const bits = (x) => {
  const v = new DataView(new ArrayBuffer(8));
  v.setFloat64(0, x, true);
  return "0x" + v.getBigUint64(0, true).toString(16).padStart(16, "0") + "ull";
};
const noiseAt = (etat, amp) => {
  const rng = makeRng(0);
  rng.setState(etat);
  const valeur = goalNoise({ rng }, amp);
  return { valeur, apres: rng.state() };
};

// --- 1. Fonction pure ----------------------------------------------------------------

const GRAINES = [0, 1, 7, 12345, 2576143622, 0x9e3779b1, 4294967295];
const AMPS = [4, 5, 6, 7, 8, 10, 14, 16, 0, 3.5];
const pures = [];
for (const g of GRAINES) for (const a of AMPS) pures.push({ g, a, ...noiseAt(g, a) });

// --- 2. Amplitudes de la source ---------------------------------------------------------

const lignesSource = SOURCE_NPC.split(/\r?\n/);
const sources = [];
lignesSource.forEach((texte, i) => {
  for (const mm of texte.matchAll(/goalNoise\(sim, (\d+(?:\.\d+)?)\)/g)) sources.push({ ligne: i + 1, amp: Number(mm[1]) });
});
const ampDeLigne = new Map(sources.map((s) => [s.ligne, s.amp]));

// --- 3. Decisions mesurees -------------------------------------------------------------

const scenario = chargerScenario(join(ICI, "scenarios", "endurance.json"));
const { tirages } = await releverTirages(REF_DIR, scenario, TICKS);
const decisions = [];
for (const d of decisionsDe(tirages)) {
  const bruits = d.tirages.filter((x) => x.site.startsWith("goalNoise "));
  if (bruits.length === 0) continue;
  // Les bruits d'une decision sont-ils consecutifs dans le flux ? Le C++ les rejoue
  // depuis l'etat du premier : il faut qu'aucun autre tirage ne s'intercale.
  const premier = d.tirages.indexOf(bruits[0]);
  const contigus = d.tirages.slice(premier, premier + bruits.length).every((x) => x.site.startsWith("goalNoise "));
  const suite = bruits.map((b) => {
    const ligne = Number(b.appelant.split(":").pop());
    const amp = ampDeLigne.get(ligne);
    if (amp === undefined) throw new Error(`appel de goalNoise sans amplitude lisible : ${b.appelant}`);
    return { ligne, amp, ...noiseAt(b.etat, amp) };
  });
  decisions.push({ t: d.t, npc: d.npc, etat: bruits[0].etat, avant: premier, contigus, suite });
}

// --- Emission ----------------------------------------------------------------------------

const L = [];
const emit = (s = "") => L.push(s);
emit("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
emit("// Source: tools/migration/gen-goal-noise-vectors.mjs (reference anastasis-ref-p3)");
emit("//");
emit("// goalNoise execute tel qu'il est ecrit dans src/sim/npc.js ; decisions mesurees sur le");
emit(`// scenario endurance, ${TICKS} ticks (rng-trace-lib.mjs). Ne jamais corriger un vecteur a la main.`);
emit("");
emit("// clang-format off");
emit("");
emit("struct FGoalNoisePureVector { uint32 State; uint64 AmpBits; uint64 ValueBits; uint32 StateAfter; };");
emit("static const FGoalNoisePureVector GoalNoisePureVectors[] = {");
for (const p of pures) emit(`\t{ ${p.g >>> 0}u, ${bits(p.a)}, ${bits(p.valeur)}, ${p.apres >>> 0}u },`);
emit("};");
emit("");
emit("// Chaque appel `goalNoise(sim, N)` de src/sim/npc.js : ligne, amplitude.");
emit("struct FGoalNoiseSourceVector { int32 Line; uint64 AmpBits; };");
emit("static const FGoalNoiseSourceVector GoalNoiseSourceVectors[] = {");
for (const s of sources) emit(`\t{ ${s.ligne}, ${bits(s.amp)} },`);
emit("};");
emit("");
emit("struct FGoalNoiseDrawVector { int32 Line; uint64 AmpBits; uint64 ValueBits; };");
emit("static const FGoalNoiseDrawVector GoalNoiseDraws[] = {");
let premier = 0;
const lignesDecisions = [];
for (const d of decisions) {
  for (const s of d.suite) emit(`\t{ ${s.ligne}, ${bits(s.amp)}, ${bits(s.valeur)} },`);
  lignesDecisions.push(`\t{ ${d.t}, "${d.npc}", ${d.etat >>> 0}u, ${d.avant}, ${d.contigus ? 1 : 0}, ${premier}, ${d.suite.length} },`);
  premier += d.suite.length;
}
emit("};");
emit("");
emit("// Une decision mesuree : tick, habitant, etat du flux au premier bruit, nombre de tirages");
emit("// AVANT ce bruit dans la decision (exploreTarget...), bruits contigus, premier bruit, nombre.");
emit("struct FGoalNoiseDecisionVector { int32 Tick; const char* Npc; uint32 State; int32 DrawsBefore; int32 Contiguous; int32 First; int32 Count; };");
emit("static const FGoalNoiseDecisionVector GoalNoiseDecisions[] = {");
for (const l of lignesDecisions) emit(l);
emit("};");

mkdirSync(dirname(SORTIE), { recursive: true });
writeFileSync(SORTIE, L.join("\n") + "\n", "utf8");
const nonContigus = decisions.filter((d) => !d.contigus).length;
console.error(`Ecrit: ${SORTIE}\n  ${pures.length} pures, ${sources.length} appels dans la source, `
  + `${decisions.length} decisions (${premier} bruits, ${nonContigus} non contigues)`);
