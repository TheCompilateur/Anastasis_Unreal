// RELEVE DU COUP RATE ET DE LA CAUSETTE AU DEPOT — mission chat-on-haul-001.
//
//   node tools/migration/trace-chat-haul.mjs -ref <clone anastasis-ref-p3> [-ticks 5400]
//
// Sur le scenario endurance, chaque tirage `sim.rng` de `rollCraftMiss` (craftMiss.js l. 79) et de
// `maybeChatOnHaul` (npc.js l. 5505) est photographie : ce que la porte et la chance ont lu, la
// valeur tiree, et le resultat. Les vecteurs servent a Anastasis.Sim.Parite.CoupRate.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { releverTirages } from "./rng-trace-lib.mjs";
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
const INL = argOf("-inl", join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisChatHaulDrawVectors.inl"));

const url = (rel) => pathToFileURL(join(REF_DIR, rel)).href;
const miss = await import(url("src/sim/craftMiss.js"));
const tech = await import(url("src/life/techniques.js"));
const scenario = chargerScenario(join(ICI, "scenarios", "endurance.json"));

function photographier({ sim, habitant, site }) {
  const npc = sim.actors.find((a) => a.id === habitant);
  if (!npc) return null;
  if (site.lieu.endsWith("npc.js:5505")) return { kind: "chat" };
  if (!site.lieu.endsWith("craftMiss.js:79")) return null;
  // Le tirage vient d'avoir lieu ; l'estampille, pas encore (elle suit la comparaison).
  const craft = npc.workSession?.craftId;
  return {
    kind: "miss",
    craft,
    time: sim.time || 0,
    skill: npc.skill,
    swings: npc.workSession?.swingsDone | 0,
    energy: npc.energy,
    mastery: tech.bestCraftMastery(npc, craft),
    missAt: npc.craftMissAt || 0,
    lastAt: npc.craftMiss?.at || 0,
    chance: miss.craftMissChance(npc, craft),
  };
}

const { tirages } = await releverTirages(REF_DIR, scenario, TICKS, { photographier });
const bits = (x) => {
  const v = new DataView(new ArrayBuffer(8));
  v.setFloat64(0, x, true);
  return "0x" + v.getBigUint64(0, true).toString(16).padStart(16, "0") + "ull";
};
const misses = tirages.filter((t) => t.extra?.kind === "miss");
const chats = tirages.filter((t) => t.extra?.kind === "chat");
const V = [];
V.push("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
V.push("// Source: tools/migration/trace-chat-haul.mjs (reference anastasis-ref-p3, scenario endurance)");
V.push("");
V.push("// clang-format off");
V.push("");
V.push("struct FMeasuredMissDraw { int32 Tick; const char* Npc; const char* Craft; uint64 TimeBits; uint64 SkillBits; int32 Swings;");
V.push("\tuint64 EnergyBits; uint64 MasteryBits; uint64 MissAtBits; uint64 LastAtBits; uint64 ChanceBits; uint64 DrawBits; int32 Missed; };");
V.push("static const FMeasuredMissDraw MeasuredMissDraws[] = {");
for (const t of misses) {
  const e = t.extra;
  V.push(`\t{ ${t.t}, "${t.npc}", "${e.craft}", ${bits(e.time)}, ${bits(e.skill)}, ${e.swings}, ${bits(e.energy)}, ${bits(e.mastery)}, `
    + `${bits(e.missAt)}, ${bits(e.lastAt)}, ${bits(e.chance)}, ${bits(t.valeur)}, ${t.valeur < e.chance ? 1 : 0} },`);
}
V.push("};");
V.push("");
V.push("struct FMeasuredChatDraw { int32 Tick; const char* Npc; uint64 DrawBits; int32 Chats; };");
V.push("static const FMeasuredChatDraw MeasuredChatDraws[] = {");
for (const t of chats) V.push(`\t{ ${t.t}, "${t.npc}", ${bits(t.valeur)}, ${t.valeur > 0.42 ? 0 : 1} },`);
V.push("};");
mkdirSync(dirname(INL), { recursive: true });
writeFileSync(INL, V.join("\n") + "\n", "utf8");
console.error(`Ecrit: ${INL} — ${misses.length} tirages rollCraftMiss (${misses.filter((t) => t.valeur < t.extra.chance).length} rates), ${chats.length} maybeChatOnHaul`);
