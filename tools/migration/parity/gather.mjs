// Parite de la boucle « cueillir puis livrer » — le fermier dont le poste est le grenier.
//
// Ce qui se declare ici est pur, ou pur a habitant fourni : le score de la
// recolte et de la livraison (beliefMarketWorkScores), le biais de fin de
// tache (completionBias), les biais de trait et de competence, l'envie de
// travailler (workWillFactor), la pression morale (moralPressure), le rythme
// et le rendement des coups (swingPeriodFor, yieldPerSwing), la saison des
// champs, le poste de travail dans la parcelle, l'apprentissage.
//
// L'habitant est cree par `createNpc` avec des options EXPLICITES (metier,
// trait, nature, competence) : c'est ce que le portage fournit a SpawnNpc au
// lieu des tirages aleatoires. Le reste — perception des gisements, marche,
// session de coups, livraison dans le stock — lit le village et se prouve par
// les tests d'assemblage Anastasis.Sim.Village.Recolte.*.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// Nature moyenne : les trois axes a 1, ni qualite ni defaut.
const NATURE_NEUTRE = { corps: 1, esprit: 1, coeur: 1, qualities: [], flaws: [] };

function habitant(m, id, trait, options = {}) {
  const rng = () => 0.5;
  const npc = m.npc.createNpc(rng, id, "Test", 10.5, 10.5, {
    jobId: options.jobId ?? "farmer",
    trait: { ...m.content.TRAITS[trait] },
    nature: NATURE_NEUTRE,
    skill: options.skill ?? 1,
    skills: { craft: 1, gather: 1, trade: 1, care: 1 },
    age: 30,
  });
  npc.workplace = { id: "building-0", type: "granary", x: 12, y: 12, progress: 1 };
  // Les huit besoins sont fixes : rien ne doit venir des valeurs de depart de createNpc.
  Object.assign(npc, { hunger: 10, thirst: 10, energy: 90, social: 90, leisure: 90, hygiene: 90, health: 95, morale: 60 });
  return npc;
}

// Le strict minimum qu'appellent les scores de travail ; sans colonie, sans chantier.
function simMinimal(acteurs, extra = {}) {
  return {
    actors: new Array(acteurs),
    day: 1,
    time: 100,
    market: { stock: { food: 30 } },
    buildings: [],
    countBuildingsWith: () => 0,
    countBuildings: () => 0,
    rng: () => 0.5,
    ...extra,
  };
}

const IDS = ["npc-0", "npc-17"];
const TRAITS = [0, 1, 2, 3, 4, 5];

export default {
  modules: {
    npc: "src/sim/npc.js",
    content: "src/sim/content.js",
    memory: "src/ai/memory.js",
    needs: "src/life/needs.js",
    moral: "src/ai/moralPressure.js",
    craft: "src/sim/craftWork.js",
    crops: "src/sim/fieldCrops.js",
    posts: "src/sim/fieldWorkPosts.js",
    skills: "src/life/skills.js",
  },
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisGatherVectors.inl"),

  cases: [
    {
      name: "WorkScores",
      comment: "believedStock(food), beliefMarketWorkScores(food, deliver) — fermier au grenier",
      args: ["int", "double", "int", "string", "int"],
      ret: [
        { name: "believed", type: "double" },
        { name: "food", type: "double" },
        { name: "deliver", type: "double" },
      ],
      inputs: croiser([1, 4, 40, 80], [0, 38, 38.5, 70, 95], [0, 1, 7, 12], IDS, [0, 3, 5]),
      call: (m, [acteurs, faim, sac, id, trait]) => {
        const npc = habitant(m, id, trait);
        npc.hunger = faim;
        npc.inventory.food = sac;
        const sim = simMinimal(acteurs);
        const scores = m.npc.beliefMarketWorkScores(sim, npc);
        return { believed: m.memory.believedStock(sim, npc).food, food: scores.food, deliver: scores.deliver };
      },
    },
    {
      name: "Completion",
      comment: "completionBias(sim, npc, goal) — sac, session de recolte, besoin critique",
      args: ["string", "int", "bool", "double"],
      ret: "double",
      inputs: croiser(
        ["gatherFood", "deliver", "eat", "rest", "drink", "sell", "build", "gatherWood"],
        [0, 3, 4, 5, 6, 12],
        [false, true],
        [10, 58],
      ),
      call: (m, [goal, sac, session, faim]) => {
        const npc = habitant(m, "npc-0", 3);
        npc.inventory.food = sac;
        npc.hunger = faim;
        npc.workSession = session ? { craftId: "farm", tileX: 3, tileY: 4, swingsDone: 0 } : null;
        return m.npc.completionBias(simMinimal(4), npc, goal);
      },
    },
    {
      name: "TraitBias",
      comment: "traitGoalBias(npc, goal)",
      args: ["int", "string"],
      ret: "double",
      inputs: croiser(TRAITS, ["gatherFood", "deliver", "eat", "helpFarm", "sell", "socialize", "build", "maintain", "explore"]),
      call: (m, [trait, goal]) => m.npc.traitGoalBias(habitant(m, "npc-0", trait), goal),
    },
    {
      name: "SkillTint",
      comment: "createNpc : competences teintees par le trait (gather, trade)",
      args: ["int"],
      ret: [
        { name: "gather", type: "double" },
        { name: "trade", type: "double" },
      ],
      inputs: TRAITS.map((t) => [t]),
      call: (m, [trait]) => {
        const npc = habitant(m, "npc-0", trait);
        return { gather: npc.skills.gather, trade: npc.skills.trade };
      },
    },
    {
      name: "WorkWill",
      comment: "workWillFactor(npc) — adulte",
      args: ["double", "double", "double", "double", "double", "double"],
      ret: "double",
      inputs: croiser([0, 36, 46.99, 47, 58], [0, 40, 54, 68], [100, 68, 58, 48], [100, 48.5], [90, 35], [50, 37, 29]),
      call: (m, [faim, soif, energie, social, sante, moral]) => {
        const npc = habitant(m, "npc-0", 3);
        Object.assign(npc, { hunger: faim, thirst: soif, energy: energie, social, leisure: 100, hygiene: 100, health: sante, morale: moral });
        return m.needs.workWillFactor(npc);
      },
    },
    {
      name: "Moral",
      comment: "moralPressure(sim, npc) : effectiveWork et socialMul — sans colonie, sans deuil",
      args: ["int", "int", "double", "double"],
      ret: [
        { name: "work", type: "double" },
        { name: "social", type: "double" },
      ],
      inputs: croiser([0, 19, 20, 59, 60, 300], [0, 1, 30, 31, 61, 91, 120, 121], [0, 50], [60, 37.99, 38, 21.99, 22]),
      call: (m, [vivres, jour, faim, moral]) => {
        const npc = habitant(m, "npc-0", 3);
        npc.hunger = faim;
        npc.morale = moral;
        const sim = simMinimal(4, { day: jour, market: { stock: { food: vivres } } });
        const p = m.moral.moralPressure(sim, npc);
        return { work: p.effectiveWork, social: p.socialMul };
      },
    },
    {
      name: "Swing",
      comment: "swingPeriodFor(npc, farm), yieldPerSwing(npc, farm)",
      args: ["double", "int", "double"],
      ret: [
        { name: "period", type: "double" },
        { name: "yield", type: "int" },
      ],
      inputs: croiser([0.7, 1, 1.34, 1.35, 2.6], [0, 4, 5, 10, 14, 20], [100, 42, 30, 5]),
      call: (m, [skill, coups, energie]) => {
        const npc = habitant(m, "npc-0", 3, { skill });
        npc.energy = energie;
        npc.workSession = { craftId: "farm", tileX: 3, tileY: 4, swingsDone: coups };
        return { period: m.craft.swingPeriodFor(npc, "farm"), yield: m.craft.yieldPerSwing(npc, "farm") };
      },
    },
    {
      name: "Season",
      comment: "fieldSeasonGatherAmount(base, day)",
      args: ["int", "int"],
      ret: "int",
      inputs: croiser([2, 3], [0, 1, 30, 31, 60, 61, 90, 91, 120, 121, 250]),
      call: (m, [base, jour]) => m.crops.fieldSeasonGatherAmount(base, jour),
    },
    {
      name: "FieldPost",
      comment: "fieldWorkTarget(sim, npc, tile) — seul, ou un voisin tient deja un poste",
      args: ["string", "int", "int", "int"],
      ret: [
        { name: "x", type: "double" },
        { name: "y", type: "double" },
        { name: "preferred", type: "int" },
      ],
      inputs: croiser([...IDS, "npc-3"], [[5, 7], [40, 12], [0, 0]], [-1, 0, 3, 5, 7])
        .map(([id, [tx, ty], autre]) => [id, tx, ty, autre]),
      call: (m, [id, tx, ty, autre]) => {
        const npc = habitant(m, id, 3);
        npc.workSession = null;
        const acteurs = [npc];
        if (autre >= 0) {
          acteurs.push({ id: "npc-9", x: 0, y: 0, target: null, inside: null, workSession: { craftId: "farm", tileX: tx, tileY: ty, postIndex: autre } });
        }
        const tile = { x: tx, y: ty };
        const cible = m.posts.fieldWorkTarget({ actors: acteurs }, npc, tile);
        return { x: cible.x, y: cible.y, preferred: m.posts.preferredFieldPostIndex(npc, tile) };
      },
    },
    {
      name: "Learn",
      comment: "gainDomainSkill(npc, amount, goal) — nature moyenne",
      args: ["double", "double", "double", "string"],
      ret: [
        { name: "skill", type: "double" },
        { name: "domain", type: "double" },
        { name: "bias", type: "double" },
      ],
      inputs: croiser([0.7, 1, 2.595], [1, 0.976, 2.59], [0.008, 0.002], ["gatherFood", "deliver"]),
      call: (m, [skill, domaine, gain, goal]) => {
        const npc = { skill, goal, nature: { ...NATURE_NEUTRE }, skills: { craft: 1, gather: domaine, trade: domaine, care: 1 } };
        m.skills.gainDomainSkill(npc, gain, goal);
        const d = m.skills.domainForGoal(goal);
        return { skill: npc.skill, domain: npc.skills[d], bias: m.skills.skillGoalBias(npc, goal) };
      },
    },
  ],
};
