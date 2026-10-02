// Parite du coup rate et de la causette au depot — mission chat-on-haul-001.
//
// Fonctions de la reference executees telles quelles :
//   sim/craftMiss.js   craftMissChance, canRollCraftMiss (avec craftFatigueOf et bestCraftMastery)
//   life/talk.js       shouldSpeakNow avec `ambientChance` (la causette en livrant passe 0,1)
//
// Le tirage, l'estampille et la reprise lisent le village : ils se prouvent par
// Anastasis.Sim.Village.CoupRate et Anastasis.Sim.Village.CausetteDepot, et par les tirages
// `rollCraftMiss` mesures sur endurance (tools/migration/trace-chat-haul.mjs).

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

const NATURE_NEUTRE = { corps: 1, esprit: 1, coeur: 1, qualities: [], flaws: [] };

function habitant(m, id) {
  const rng = () => 0.5;
  const npc = m.npc.createNpc(rng, id, "Test", 10.5, 10.5, {
    jobId: "settler",
    trait: { ...m.content.TRAITS[3] },
    nature: { ...NATURE_NEUTRE },
    skill: 1,
    skills: { craft: 1, gather: 1, trade: 1, care: 1 },
    age: 30,
  });
  Object.assign(npc, { hunger: 10, thirst: 10, energy: 90, social: 60, leisure: 90, hygiene: 90, health: 95, morale: 60 });
  npc.goal = "observer";
  npc.activity = "attend";
  npc.relations = {};
  npc.moodlets = [];
  return npc;
}

const CRAFTS = ["farm", "build", "chop", "quarry", "tend", "maintain", "saw", "", "explore"];
const SKILLS = [0, 0.7, 1, 1.35, 2.4];
const SWINGS = [0, 4, 5, 9, 14, 20];
const ENERGIES = [100, 42, 41.9, 20, 0];
const MASTERIES = [0, 0.5, 1];

export default {
  modules: {
    npc: "src/sim/npc.js",
    content: "src/sim/content.js",
    miss: "src/sim/craftMiss.js",
    tech: "src/life/techniques.js",
    talk: "src/life/talk.js",
  },
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisChatHaulVectors.inl"),

  cases: [
    {
      name: "CraftMissChance",
      comment: "craftMissChance(npc, craftId) — profil, competence, coups de la session, energie, maitrise du meilleur savoir-faire",
      args: ["string", "double", "int", "double", "double"],
      ret: "double",
      inputs: croiser(CRAFTS, SKILLS, SWINGS, ENERGIES, MASTERIES),
      call: ({ miss, tech }, [craft, skill, swings, energy, mastery]) => {
        const npc = { skill, energy, workSession: { swingsDone: swings }, techniques: {} };
        // La maitrise ne compte que si le metier a un savoir-faire : on la pose sur le premier.
        const ids = tech.techniqueIdsForCraft(craft);
        if (mastery > 0 && ids.length > 0) npc.techniques[ids[0]] = { mastery };
        return miss.craftMissChance(npc, craft);
      },
    },
    {
      name: "CraftMissMastery",
      comment: "bestCraftMastery(npc, craftId) pour la meme pose : la maitrise que le C++ recoit en parametre",
      args: ["string", "double"],
      ret: "double",
      inputs: croiser(CRAFTS, MASTERIES),
      call: ({ tech }, [craft, mastery]) => {
        const npc = { techniques: {} };
        const ids = tech.techniqueIdsForCraft(craft);
        if (mastery > 0 && ids.length > 0) npc.techniques[ids[0]] = { mastery };
        return tech.bestCraftMastery(npc, craft);
      },
    },
    {
      name: "CraftMissCanRoll",
      comment: "canRollCraftMiss(sim, npc, craftId) — maintenant, craftMissAt, craftMiss.at (absents = 0)",
      args: ["string", "double", "double", "double"],
      ret: "bool",
      inputs: croiser(["farm", "build", "explore"], [0, 9.4, 9.5, 30, 100.25], [0, 20.5, 90.75], [0, 20.5, 90.75]),
      call: ({ miss }, [craft, now, missAt, lastAt]) => {
        const npc = {};
        if (missAt) npc.craftMissAt = missAt;
        if (lastAt) npc.craftMiss = { kind: "whiff", craftId: craft, at: lastAt };
        return miss.canRollCraftMiss({ time: now }, npc, craft);
      },
    },
    {
      name: "ShouldSpeakAmbient",
      comment: "shouldSpeakNow(sim, speaker, listener, { ambientChance: 0.1 }) — la causette en livrant",
      args: ["string", "string", "double", "int", "bool"],
      ret: "bool",
      inputs: croiser(["npc-0", "npc-3", "npc-12"], ["npc-1", "npc-4", "npc-17"], [37.5, 100.1, 123.45, 200, 333.3, 451.9], [0, 3, 4], [false, true])
        .filter(([s, l]) => s !== l),
      call: (m, [s, l, t, emis, fil]) => {
        const a = habitant(m, s);
        const b = habitant(m, l);
        if (fil) a.talkChainTopic = "job";
        const sim = { time: t, life: { recentVillageEmits: Array.from({ length: emis }, (_, i) => ({ at: t - 1 - i })) } };
        return m.talk.shouldSpeakNow(sim, a, b, { ambientChance: 0.1 });
      },
    },
  ],
};
