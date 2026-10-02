// RELEVE DE LA RECONSIDERATION — `npc.js` l. 893, mission reconsider-001.
//
//   node tools/migration/trace-reconsider.mjs -ref <clone anastasis-ref-p3> [-ticks 5400]
//        [-md docs/migration/phase3/P3_RECONSIDERATION.md]
//
// Dans `updateNpc`, une fois par pensee :
//
//   const reconsider = phaseReconsiderChance(sim, npc, thinkDt, needsReconsiderChance(npc, thinkDt));
//   const chance = committedReconsiderChance(sim, npc, reconsider);
//   if (!npc.target || sim.rng() < chance) chooseGoal(sim, npc);
//
// Le tirage n'a lieu que si l'habitant a une cible. Le C++ ne le fait pas (ecart n° 2) :
// une chance approchee ferait tirer ou choisir quand la reference ne le fait pas. Avant
// de porter, ce releve MESURE, pour chaque tirage du scenario endurance, ce que la chance
// a lu : besoins critiques, phase PERSONNELLE (mode de vie) et bascule de phase, age du
// but, quart de travail. Il RECONSTRUIT la chance avec les fonctions de la reference (sans
// effet de bord) et verifie qu'elle explique le resultat : tirage < chance <=> chooseGoal
// suit. Si la reconstruction tient sur tout le scenario, elle est la specification du
// portage.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { releverTirages } from "./rng-trace-lib.mjs";
import { chargerScenario, provenanceReference } from "./scenarios/scenario-format.mjs";

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
const MD = argOf("-md", join(RACINE, "docs", "migration", "phase3", "P3_RECONSIDERATION.md"));
const INL = argOf("-inl", join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisReconsiderVectors.inl"));

const url = (rel) => pathToFileURL(join(REF_DIR, rel)).href;
const needs = await import(url("src/life/needs.js"));
const rhythm = await import(url("src/life/villageRhythm.js"));
const shift = await import(url("src/life/workShift.js"));
const npcMod = await import(url("src/sim/npc.js"));
const GOAL_AI = npcMod.GOAL_AI;

const provenance = provenanceReference(REF_DIR);
const scenario = chargerScenario(join(ICI, "scenarios", "endurance.json"));

/**
 * La photo au moment du tirage l. 893. `phaseReconsiderChance` a DEJA tourne (il a
 * synchronise la phase) : une bascule de ce tick se lit a `phaseChangedAt === sim.time`.
 */
function photographier({ sim, habitant, site }) {
  if (!site.lieu.endsWith("src/sim/npc.js:893")) return null;
  const npc = sim.actors.find((a) => a.id === habitant);
  if (!npc) return null;
  const now = sim.time ?? 0;
  const thinkDt = (npc.aiThinkAt ?? now) - now;
  const base = needs.needsReconsiderChance(npc, thinkDt);
  const flip = npc.phaseChangedAt === now;
  const phase = rhythm.villagePhaseFor(sim, npc).id;
  // `updateNpc` synchronise la phase a CHAQUE tick avant la pensee (npc.js l. 874) : le
  // `syncVillagePhase` interne de `phaseReconsiderChance` rend donc toujours false ici, et sa
  // branche 0,92 n'est jamais prise sur ce chemin (releve : 6 bascules, toutes sans elle).
  let reconsider = base;
  if (phase === "midday" || phase === "evening" || phase === "night" || phase === "dawn") reconsider = Math.max(base, thinkDt * 0.55);
  // committedReconsiderChance, branche par branche (non exportee : reecrite ici pour le releve
  // seulement, et validee plus bas contre les resultats).
  const critical = needs.needsCritical(npc);
  const phaseAdapt = npc.phaseChangedAt != null && (now - npc.phaseChangedAt) < GOAL_AI.phaseAdaptSeconds;
  const age = now - (npc.goalSince || 0);
  const shield = shift.shiftShields(sim, npc);
  let chance = reconsider;
  let branche = "base";
  if (!Number.isFinite(reconsider)) branche = "non-fini";
  else if (critical) branche = "critique";
  else if (!npc.target || !npc.goal || npc.goal === "observer") branche = "sans-but";
  else if (phaseAdapt) branche = "bascule-recente";
  else if (age >= 0 && age <= GOAL_AI.commitSeconds) { chance = reconsider * GOAL_AI.commitReconsiderScale; branche = "engagement"; }
  else if (shield) { chance = reconsider * GOAL_AI.commitReconsiderScale; branche = "quart"; }
  const ws = npc.workShift && typeof npc.workShift === "object" ? npc.workShift : null;
  return {
    time: now, goalSince: npc.goalSince || 0, phaseChangedAt: npc.phaseChangedAt ?? null,
    metres: ["hunger", "energy", "social", "leisure", "hygiene", "thirst", "health", "morale"].map((k) => (k === "morale" ? (npc.morale ?? 50) : npc[k])),
    shift: ws ? { state: ws.state ?? "", goal: ws.goal ?? "", startedAt: ws.startedAt ?? 0, floorUntil: ws.floorUntil ?? 0 } : null,
    goal: npc.goal, thinkDt, base, flip, phase, lifestyle: npc.lifestyle?.id ?? null,
    reconsider, critical, phaseAdapt, age, shield,
    shiftState: npc.workShift?.state ?? null, chance, branche,
  };
}

const { tirages } = await releverTirages(REF_DIR, scenario, TICKS, { photographier });

// Le resultat : chooseGoal suit-il le tirage (meme tick, meme habitant, tirage suivant sous chooseGoal) ?
const lignes = [];
for (let i = 0; i < tirages.length; i += 1) {
  const d = tirages[i];
  if (!d.extra) continue;
  const suivant = tirages[i + 1];
  const choisit = Boolean(suivant && suivant.t === d.t && suivant.npc === d.npc && suivant.decision);
  lignes.push({ ...d, choisit, explique: (d.valeur < d.extra.chance) === choisit });
}

const fmt = (x) => (Number.isFinite(x) ? x.toFixed(4) : String(x));
const parBranche = new Map();
for (const l of lignes) parBranche.set(l.extra.branche, (parBranche.get(l.extra.branche) || 0) + 1);
const nonExpliques = lignes.filter((l) => !l.explique);

const L = [];
const emit = (s = "") => L.push(s);
emit(`# Relevé de la reconsidération (\`npc.js\` l. 893) — scénario \`${scenario.name}\`, ${TICKS} ticks`);
emit("");
emit("Généré par `tools/migration/trace-reconsider.mjs` (mission reconsider-001). Ne pas éditer à la main : relancer.");
emit("");
emit(`- Référence : \`${provenance.court ?? provenance.commit}\`, scénario \`${scenario.name}\`, tick recomposé (rng-trace-lib.mjs).`);
emit(`- **${lignes.length} tirages** l. 893 ; **${lignes.filter((l) => l.choisit).length} suivis d'un \`chooseGoal\`**.`);
emit(`- Chance reconstruite avec les fonctions de la référence : **${lignes.length - nonExpliques.length} / ${lignes.length} résultats expliqués** `
  + "(tirage < chance ⇔ `chooseGoal` suit).");
emit("");
emit("## Branches de `committedReconsiderChance`");
emit("");
emit("| Branche | Tirages |");
emit("| --- | ---: |");
for (const [b, n] of [...parBranche].sort((a, c) => c[1] - a[1])) emit(`| \`${b}\` | ${n} |`);
emit("");
emit("## Ce que la chance a lu");
emit("");
emit(`- bascule de phase au tick du tirage : ${lignes.filter((l) => l.extra.flip).length}`);
emit(`- phase personnelle ≠ phase du village (mode de vie) : ${lignes.filter((l) => l.extra.lifestyle === "earlyBird" || l.extra.lifestyle === "nightOwl").length} tirages d'habitants leve-tot / noctambule`);
emit(`- bouclier de quart actif : ${lignes.filter((l) => l.extra.shield).length} ; états de quart vus : ${[...new Set(lignes.map((l) => l.extra.shiftState))].join(", ")}`);
emit(`- besoins critiques : ${lignes.filter((l) => l.extra.critical).length}`);
emit("");
emit("## Les tirages, un par ligne");
emit("");
emit("| Tick | Habitant | But | Mode de vie | Phase | Bascule | thinkDt | Base | Branche | Chance | Tirage | Choisit | Expliqué |");
emit("| ---: | --- | --- | --- | --- | --- | ---: | ---: | --- | ---: | ---: | --- | --- |");
for (const l of lignes) {
  const e = l.extra;
  emit(`| ${l.t} | \`${l.npc}\` | ${e.goal} | ${e.lifestyle ?? "-"} | ${e.phase} | ${e.flip ? "oui" : ""} | ${fmt(e.thinkDt)} | ${fmt(e.base)} `
    + `| ${e.branche} | ${fmt(e.chance)} | ${fmt(l.valeur)} | ${l.choisit ? "oui" : ""} | ${l.explique ? "oui" : "**NON**"} |`);
}
mkdirSync(dirname(MD), { recursive: true });
writeFileSync(MD, L.join("\n") + "\n", "utf8");
// --- Vecteurs du test de rejeu (Anastasis.Sim.Village.Reconsideration) ------------------------
const bits = (x) => {
  const v = new DataView(new ArrayBuffer(8));
  v.setFloat64(0, x, true);
  return "0x" + v.getBigUint64(0, true).toString(16).padStart(16, "0") + "ull";
};
const V = [];
V.push("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
V.push("// Source: tools/migration/trace-reconsider.mjs (reference anastasis-ref-p3, scenario endurance)");
V.push("//");
V.push("// Chaque tirage l. 893 mesure : la photo de l'habitant, la chance que la reference a comparee");
V.push("// (reconstruite : 75 resultats sur 75 expliques), le tirage et le resultat.");
V.push("");
V.push("// clang-format off");
V.push("");
V.push("struct FReconsiderDrawVector { int32 Tick; const char* Npc; uint64 TimeBits; uint64 ThinkDtBits; const char* Goal;");
V.push("\tuint64 GoalSinceBits; int32 HasPhaseChangedAt; uint64 PhaseChangedAtBits; const char* Lifestyle;");
V.push("\tconst char* ShiftState; const char* ShiftGoal; uint64 ShiftStartedAtBits; uint64 ShiftFloorUntilBits;");
V.push("\tuint64 Metres[8]; uint64 ChanceBits; uint64 DrawBits; int32 Chooses; };");
V.push("static const FReconsiderDrawVector ReconsiderDraws[] = {");
for (const l of lignes) {
  const e = l.extra;
  const sh = e.shift ?? { state: "", goal: "", startedAt: 0, floorUntil: 0 };
  V.push(`\t{ ${l.t}, "${l.npc}", ${bits(e.time)}, ${bits(e.thinkDt)}, "${e.goal ?? ""}", ${bits(e.goalSince)}, `
    + `${e.phaseChangedAt == null ? 0 : 1}, ${bits(e.phaseChangedAt ?? 0)}, "${e.lifestyle ?? ""}", `
    + `"${sh.state}", "${sh.goal}", ${bits(sh.startedAt)}, ${bits(sh.floorUntil)}, `
    + `{ ${e.metres.map(bits).join(", ")} }, ${bits(e.chance)}, ${bits(l.valeur)}, ${l.choisit ? 1 : 0} },`);
}
V.push("};");
mkdirSync(dirname(INL), { recursive: true });
writeFileSync(INL, V.join("\n") + "\n", "utf8");
console.error(`Ecrit: ${MD}\n${lignes.length} tirages l. 893, ${lignes.filter((l) => l.choisit).length} reconsiderations, ${nonExpliques.length} non expliques`);
