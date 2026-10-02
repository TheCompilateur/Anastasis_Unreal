// Parite du mode de vie — `src/sim/lifestyle.js` (mission lifestyle-001).
//
// Ce qui se prouve ici :
//   - le TIRAGE (`assignLifestyle`, `ensureLifestyle`) : le mode de vie choisi ET
//     l'etat du flux apres coup, avec un flux a graine et avec le flux de secours
//     global (`fallbackRng`, remis a une graine connue avant chaque vecteur) ;
//   - les penchants (`lifestyleBias`), la marche, la duree a l'interieur, la
//     destination, sur les six modes de vie, les cinq phases et tous les buts lus ;
//   - les deux fonctions a ETAT, sur des suites rejouees des deux cotes :
//     `lifestyleNotePlaceUse` (une meme entree de lieu touchee par plusieurs modes
//     de vie : l'ordre des cles de l'objet compte) et `lifestyleDailyUpdate` sur
//     des dizaines de jours.
//
// Les suites sont tirees d'un mulberry32 a graine (`makeRng`), que le C++ refait
// avec `FAnastasisRng` : meme graine, memes jours, memes buts.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

export const IDS = ["earlyBird", "nightOwl", "workhorse", "wanderer", "familyFirst", "tavernRegular"];

// Une fraction par phase du village (nuit, aube, matin, midi, apres-midi, soir), et
// les bords de la nuit (21 h pile, 5 h pile). Fractions copiees de needs.mjs.
const FRACS = [0.05, 0.22, 0.3, 0.5, 0.65, 0.8, 0.875, 0.8749, 0.20833333333333334, 0.9];

// Tous les buts que le module lit, plus un but que personne ne lit.
const GOALS = [
  "rest", "visitFamily", "socialize", "relax", "play", "explore", "maintain", "study",
  "gatherWood", "build", "craft", "deliver", "sell", "buy", "eat", "observer",
];

const STAGES = ["", "child", "teen", "elder", "adult"];
const JOBS = ["", "guard", "builder", "weaver", "farmer", "butcher", "merchant", "innkeeper", "priest", "settler"];
const PREFERENCES = ["", "nightOwl", "tavernRegular", "familyFirst", "bogus"];

// Profils pour les penchants : maison, famille (partenaire ou enfants), competence, energie.
// homeId, familyId, partnerId, childCount, skill, energy, hasTarget
const PROFILS = [
  ["", "", "", 0, 0, 0, false], // ni maison ni famille, energie 0 => `|| 100`
  ["h1", "f1", "p1", 0, 1.5, 30, true],
  ["h1", "f1", "", 2, 5, 80, false],
  ["h1", "", "", 0, 3.99, 20, true],
];

const sujet = ([homeId, familyId, partnerId, childCount, skill, energy, hasTarget], goal) => ({
  home: homeId ? { id: homeId } : null,
  familyId: familyId || undefined,
  partnerId: partnerId || undefined,
  childIds: Array.from({ length: childCount }, (_, i) => `c${i}`),
  skill, energy, goal,
  target: hasTarget ? { x: 1, y: 1 } : null,
});

const avecMode = (npc, id) => {
  npc.lifestyle = { id, sinceDay: 1, rhythmScore: 0, lastNotedDay: 0 };
  return npc;
};

// Le monde de lifestyleTarget, par code. Le test C++ refait la meme liste.
//   0 : aucune taverne | 1 : taverne inachevee seule | 2 : inachevee puis deux achevees
//   3 : une achevee ; et b9 (le lieu favori) existe dans 2 et 3
const MONDES = [
  [{ id: "b1", type: "house", progress: 1 }],
  [{ id: "b1", type: "house", progress: 1 }, { id: "b2", type: "tavern", progress: 0.5 }],
  [{ id: "b2", type: "tavern", progress: 0.99 }, { id: "b3", type: "tavern", progress: 1 }, { id: "b4", type: "tavern", progress: 1 }, { id: "b9", type: "workshop", progress: 1 }],
  [{ id: "b5", type: "tavern", progress: 1 }, { id: "b9", type: "workshop", progress: 1 }],
];
const simMonde = (code) => ({
  buildings: MONDES[code],
  buildingAccessPoint: (b) => ({ x: 0, y: 0, buildingId: b.id }),
  buildingById: (id) => MONDES[code].find((b) => b.id === id),
});

const KINDS = ["gatherWood", "craft", "socialize", "socialise", "relax", "relaxe", "visitFamily", "rest", "drink"];

export default {
  modules: { lifestyle: "src/sim/lifestyle.js", rng: "src/sim/rng.js" },
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisLifestyleVectors.inl"),

  cases: [
    {
      name: "LifestyleDayPhase",
      comment: "dayPhase(frac) — aube -> morning, apres-midi -> day",
      args: ["double"],
      ret: "string",
      inputs: [...FRACS, 0, 0.999, 1, 1.3, -0.2].map((f) => [f]),
      call: ({ lifestyle }, [f]) => lifestyle.dayPhase(f),
    },
    {
      name: "LifestyleInfo",
      comment: "lifestyleForId — inconnu = flaneur",
      args: ["string"],
      ret: ["label", "short", "color", "marker"].map((name) => ({ name, type: "string" })),
      inputs: [...IDS, "bogus", ""].map((id) => [id]),
      call: ({ lifestyle }, [id]) => lifestyle.lifestyleForId(id),
    },
    {
      name: "LifestyleAssign",
      comment: "assignLifestyle(rng a graine, npc, preferred) : mode choisi et flux apres",
      // graine, preference, stade, apprenti, metier, explore, build, trade, familyId, enfants
      args: ["double", "string", "string", "bool", "string", "double", "double", "double", "string", "int"],
      ret: [
        { name: "id", type: "string" },
        { name: "sinceDay", type: "double" },
        { name: "rhythmScore", type: "double" },
        { name: "lastNotedDay", type: "double" },
        { name: "rngState", type: "double" },
      ],
      inputs: [
        ...croiser([7, 12345, 4294967295], PREFERENCES, STAGES, JOBS).map(([seed, pref, stage, job]) =>
          [seed, pref, stage, stage === "teen" && job === "", job, 1, 1, 1, "", 0]),
        ...croiser([3, 99, 2026], [[1.2, 0, 0], [0, 1.09, 1.08], [0, 0, 1.5], [1.081, 1.2, 1.3]], [["", 0], ["f1", 0], ["", 2]])
          .map(([seed, [e, b, t], [fam, kids]]) => [seed, "", "", false, "", e, b, t, fam, kids]),
      ],
      call: ({ lifestyle, rng }, a) => {
        const r = rng.makeRng(a[0]);
        const npc = {
          lifeStage: a[2] || undefined, apprenticing: a[3], jobId: a[4] || undefined,
          trait: { explore: a[5], build: a[6], trade: a[7] },
          familyId: a[8] || undefined, childIds: Array.from({ length: a[9] }, (_, i) => `c${i}`),
        };
        const l = lifestyle.assignLifestyle(r, npc, a[1] || null);
        return { ...l, rngState: r.state() };
      },
    },
    {
      name: "LifestyleAssignFallback",
      comment: "assignLifestyle(null, npc) : le flux de secours, remis a une graine avant chaque vecteur",
      args: ["double", "string"],
      ret: [{ name: "id", type: "string" }, { name: "rngState", type: "double" }],
      inputs: croiser([0x9e3779b1, 1, 77, 31337, 4000000000], ["", "farmer", "guard"]),
      call: ({ lifestyle, rng }, [state, job]) => {
        rng.fallbackRng.setState(state);
        const l = lifestyle.assignLifestyle(null, { jobId: job || undefined });
        return { id: l.id, rngState: rng.fallbackRng.state() };
      },
    },
    {
      name: "LifestyleEnsure",
      comment: "ensureLifestyle : garde un mode connu sans tirer, retire un mode absent ou inconnu",
      args: ["double", "bool", "string", "double", "double", "double"],
      ret: [
        { name: "id", type: "string" },
        { name: "sinceDay", type: "double" },
        { name: "rhythmScore", type: "double" },
        { name: "lastNotedDay", type: "double" },
        { name: "rngState", type: "double" },
      ],
      inputs: [
        ...croiser([11, 12345], IDS).map(([seed, id]) => [seed, true, id, 3, 42.5, 7]),
        ...croiser([11, 12345, 600], [[false, ""], [true, "bogus"], [true, ""]]).map(([seed, [has, id]]) => [seed, has, id, 3, 42.5, 7]),
      ],
      call: ({ lifestyle, rng }, [seed, has, id, since, rhythm, noted]) => {
        const r = rng.makeRng(seed);
        const npc = { jobId: "farmer" };
        if (has) npc.lifestyle = { id, sinceDay: since, rhythmScore: rhythm, lastNotedDay: noted };
        const l = lifestyle.ensureLifestyle(npc, r);
        return { ...l, rngState: r.state() };
      },
    },
    {
      name: "LifestyleBias",
      comment: "lifestyleBias — six modes, dix fractions de jour, seize buts, quatre profils",
      args: ["string", "double", "string", "int"],
      ret: "double",
      inputs: croiser(IDS, FRACS, GOALS, [0, 1, 2, 3]),
      call: ({ lifestyle }, [id, frac, goal, profil]) => {
        const npc = avecMode(sujet(PROFILS[profil], goal), id);
        return lifestyle.lifestyleBias({ dayFrac: () => frac }, npc, goal);
      },
    },
    {
      name: "LifestyleTravel",
      comment: "lifestyleTravelFactor — but et cible de l'habitant",
      args: ["string", "double", "string", "int"],
      ret: "double",
      inputs: croiser(IDS, FRACS, GOALS, [0, 1]),
      call: ({ lifestyle }, [id, frac, goal, profil]) => {
        const npc = avecMode(sujet(PROFILS[profil], goal), id);
        return lifestyle.lifestyleTravelFactor({ dayFrac: () => frac }, npc);
      },
    },
    {
      name: "LifestyleIndoor",
      comment: "lifestyleIndoorDuration(npc, goal, base)",
      args: ["string", "string", "double"],
      ret: "double",
      inputs: croiser(IDS, GOALS, [4.2, 11.5, 0.1 + 0.2]),
      call: ({ lifestyle }, [id, goal, base]) => lifestyle.lifestyleIndoorDuration(avecMode({}, id), goal, base),
    },
    {
      name: "LifestyleTarget",
      comment: "lifestyleTarget — batiment choisi (vide = fallback garde)",
      // mode, but, monde, maison, lieu favori
      args: ["string", "string", "int", "bool", "string"],
      ret: "string",
      inputs: croiser(IDS, ["socialize", "relax", "visitFamily", "rest", "explore"], [0, 1, 2, 3], [false, true], ["", "b9"]),
      call: ({ lifestyle }, [id, goal, monde, maison, favori]) => {
        const npc = avecMode({ home: maison ? { id: "h1" } : null, placeMemory: favori ? { favoriteBuildingId: favori } : {} }, id);
        const t = lifestyle.lifestyleTarget(simMonde(monde), npc, goal, { x: 9, y: 9, buildingId: "" });
        return t.buildingId;
      },
    },
    {
      name: "LifestylePlaceUse",
      comment: "lifestyleNotePlaceUse — suite de 12 usages d'un meme lieu par des habitants de modes divers",
      // graine de la suite, maison de l'acteur (vide = sans maison), batiment de l'entree (vide = absent)
      args: ["double", "string", "string"],
      ret: [
        { name: "work", type: "double" },
        { name: "social", type: "double" },
        { name: "home", type: "double" },
        { name: "keys", type: "string" },
        ...IDS.map((id) => ({ name: `v_${id}`, type: "double" })),
      ],
      inputs: croiser([1, 2, 3, 404, 8080], [["", ""], ["h1", "h1"], ["h1", "b2"], ["", "b2"]]).map(([s, [h, b]]) => [s, h, b]),
      call: ({ lifestyle, rng }, [seed, homeId, buildingId]) => {
        const r = rng.makeRng(seed);
        const entry = { work: 0, social: 0, home: 0 };
        if (buildingId) entry.buildingId = buildingId;
        for (let k = 0; k < 12; k += 1) {
          const id = IDS[Math.floor(r() * IDS.length)];
          const kind = KINDS[Math.floor(r() * KINDS.length)];
          const amount = r() < 0.25 ? 0.01 : r() * 3;
          const actor = avecMode({ home: homeId ? { id: homeId } : null }, id);
          lifestyle.lifestyleNotePlaceUse(actor, kind, entry, amount);
        }
        const out = { work: entry.work, social: entry.social, home: entry.home, keys: Object.keys(entry.lifestyle).join(",") };
        for (const id of IDS) out[`v_${id}`] = entry.lifestyle[id] ?? -1;
        return out;
      },
    },
    {
      name: "LifestyleDaily",
      comment: "lifestyleDailyUpdate — 60 pas sur plusieurs jours (meme jour = sans effet)",
      // mode, score de depart, graine de la suite
      args: ["string", "double", "double"],
      ret: [
        { name: "rhythmScore", type: "double" },
        { name: "lastNotedDay", type: "double" },
        { name: "aligned", type: "int" },
      ],
      inputs: croiser(IDS, [0, 0.1, 99.5], [5, 6, 7, 1999]),
      call: ({ lifestyle, rng }, [id, start, seed]) => {
        const r = rng.makeRng(seed);
        const npc = avecMode({}, id);
        npc.lifestyle.rhythmScore = start;
        let day = 1;
        let montees = 0;
        for (let k = 0; k < 60; k += 1) {
          day += Math.floor(r() * 3);
          const frac = r();
          npc.goal = GOALS[Math.floor(r() * GOALS.length)];
          const avant = npc.lifestyle.rhythmScore;
          lifestyle.lifestyleDailyUpdate({ day, dayFrac: () => frac }, npc);
          if (npc.lifestyle.rhythmScore > avant) montees += 1;
        }
        return { rhythmScore: npc.lifestyle.rhythmScore, lastNotedDay: npc.lifestyle.lastNotedDay, aligned: montees };
      },
    },
  ],
};
