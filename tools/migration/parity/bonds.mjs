// Parite des liens et des rumeurs — la branche AVEC compagnon de `socialize()`.
//
// Deux adultes sans famille ni partenaire, nature moyenne, trait « gardien »,
// sans colonie (pas de sceau), chronique vide (pas de ragot), `langV0` faux.
// Ce qui se declare ici est pur, ou pur a habitants fournis :
//
//   bonds.js         companionAffinity, bondTalkGain, bumpRelation (+ palier, moodlet)
//   talk.js          canStartTalk, speakWorthFor, shouldSpeakNow, talkMaxTurnsFor,
//                    talkHoldDurationFor, replyUtteranceFor (le refus seulement)
//   socialMemory.js  noteMeeting (fiche + theorie de l'esprit), socialMemoryBias,
//                    pickRememberedSeek
//   speechActs.js    createInformResourceSpotActs + commitSpeechActs (gisements entendus)
//   moodlets.js      stampMoodlet, tickMoodlets, moodletGoalBias
//   needs.js         dominantNeedLabel
//
// L'ordre des conversations, le gel des sessions et le choix du compagnon dans
// la grille lisent le village : ils se prouvent par Anastasis.Sim.Village.Liens.*.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

const NATURE_NEUTRE = { corps: 1, esprit: 1, coeur: 1, qualities: [], flaws: [] };

function habitant(m, id, options = {}) {
  const rng = () => 0.5;
  const npc = m.npc.createNpc(rng, id, "Test", 10.5, 10.5, {
    jobId: options.jobId ?? "settler",
    trait: { ...m.content.TRAITS[3] },
    nature: { ...NATURE_NEUTRE },
    skill: 1,
    skills: { craft: 1, gather: 1, trade: 1, care: 1 },
    age: 30,
  });
  Object.assign(npc, { hunger: 10, thirst: 10, energy: 90, social: 60, leisure: 90, hygiene: 90, health: 95, morale: 60 });
  npc.goal = options.goal ?? "observer";
  npc.activity = "attend";
  npc.relations = {};
  npc.moodlets = [];
  return npc;
}

// Besoins : [faim, soif, energie, social, loisir, hygiene, sante, moral]
const BESOINS = [
  [10, 10, 90, 60, 90, 90, 95, 60],   // rien
  [58, 10, 90, 60, 90, 90, 95, 60],   // faim critique -> urgent
  [57.9, 67.9, 48.1, 60, 90, 90, 95, 60], // juste sous les seuils
  [10, 10, 48, 60, 90, 90, 95, 60],   // energie 48 -> urgent
  [70, 10, 90, 60, 90, 90, 95, 60],   // faim 70 dominante
  [10, 10, 30, 60, 90, 90, 95, 60],   // fatigue 70 -> urgent aussi
  [60, 10, 90, 20, 90, 90, 95, 60],   // solitude 80 dominante, faim 60
  [10, 10, 90, 60, 90, 90, 95, 10],   // desespoir
  [20, 72, 90, 60, 90, 90, 95, 60],   // soif dominante
];
const poser = (npc, b) => Object.assign(npc, {
  hunger: b[0], thirst: b[1], energy: b[2], social: b[3], leisure: b[4], hygiene: b[5], health: b[6], morale: b[7],
});

export default {
  modules: {
    npc: "src/sim/npc.js",
    content: "src/sim/content.js",
    bonds: "src/life/bonds.js",
    talk: "src/life/talk.js",
    social: "src/ai/socialMemory.js",
    acts: "src/life/speechActs.js",
    moodlets: "src/life/moodlets.js",
    needs: "src/life/needs.js",
  },
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisBondsVectors.inl"),

  cases: [
    {
      name: "Affinity",
      comment: "companionAffinity(npc, other) — relation, confiance / etiquette, collegue, disponible",
      args: ["double", "double", "string", "bool", "int"],
      ret: "double",
      inputs: croiser([-30, -18, -17.9, 0, 5, 44.9, 45, 80], [0, 16, -20, 25], ["", "ally", "rival"], [false, true], [0, 1, 2])
        .map(([rel, trust, tag, collegue, dispo]) => [rel, trust, tag, collegue, dispo]),
      call: (m, [rel, trust, tag, collegue, dispo]) => {
        const a = habitant(m, "npc-0", { jobId: "farmer" });
        const b = habitant(m, "npc-1", { jobId: collegue ? "farmer" : "settler", goal: dispo === 1 ? "socialize" : "observer" });
        if (dispo === 2) b.talkWithId = "npc-7";
        a.relations[b.id] = rel;
        a.mind.people = { [b.id]: { id: b.id, name: "B", trust, tag: tag || null, day: 1, meets: 1, hearsay: false } };
        return m.bonds.companionAffinity(a, b);
      },
    },
    {
      name: "TalkGain",
      comment: "bondTalkGain(npc, other) — inconnu, presque ami, ami",
      args: ["double"],
      ret: "int",
      inputs: [[0], [44.99], [45], [90], [-50]],
      call: (m, [rel]) => {
        const a = habitant(m, "npc-0");
        const b = habitant(m, "npc-1");
        a.relations[b.id] = rel;
        return m.bonds.bondTalkGain(a, b);
      },
    },
    {
      name: "CanStart",
      comment: "canStartTalk(sim, a, b) — cooldown de paire 18 s, fatigue 60 s / 2",
      args: ["double", "double", "double", "int", "double", "int", "double"],
      ret: "bool",
      inputs: croiser([100], [-1, 82.5, 81.9], [-1, 85], [0, 1, 2, 3], [50, 39.9], [0, 2], [41, 30])
        .map(([t, aAt, bAt, fa, faAt, fb, fbAt]) => [t, aAt, bAt, fa, faAt, fb, fbAt]),
      call: (m, [t, aAt, bAt, fa, faAt, fb, fbAt]) => {
        const a = habitant(m, "npc-0");
        const b = habitant(m, "npc-1");
        if (aAt >= 0) a.lastTalk = { withId: b.id, at: aAt };
        if (bAt >= 0) b.lastTalk = { withId: a.id, at: bAt };
        if (fa) a.talkFatigue = { [b.id]: { count: fa, at: faAt } };
        if (fb) b.talkFatigue = { [a.id]: { count: fb, at: fbAt } };
        return m.talk.canStartTalk({ time: t }, a, b);
      },
    },
    {
      name: "SpeakWorth",
      comment: "speakWorthFor(speaker, listener, sim) — besoins, fil de conversation, lien",
      args: ["int", "int", "bool", "double"],
      ret: "double",
      inputs: croiser(BESOINS.map((_, i) => i), [0, 1, 3], [false, true], [0, 45, -45, 20])
        .map(([bs, bl, fil, rel]) => [bs, bl, fil, rel]),
      call: (m, [bs, bl, fil, rel]) => {
        const s = poser(habitant(m, "npc-0"), BESOINS[bs]);
        const l = poser(habitant(m, "npc-1"), BESOINS[bl]);
        if (fil) s.talkChainTopic = "job";
        s.relations[l.id] = rel;
        return m.talk.speakWorthFor(s, l, { time: 100, life: {} });
      },
    },
    {
      name: "ShouldSpeak",
      comment: "shouldSpeakNow(sim, speaker, listener) — porte FNV, budget de rue (4 / 22 s)",
      args: ["string", "string", "double", "int", "bool"],
      ret: "bool",
      inputs: croiser(["npc-0", "npc-3", "npc-12"], ["npc-1", "npc-4", "npc-17"], [37.5, 100.1, 123.45, 200, 333.3, 451.9], [0, 3, 4], [false, true])
        .filter(([s, l]) => s !== l)
        .map(([s, l, t, emis, fil]) => [s, l, t, emis, fil]),
      call: (m, [s, l, t, emis, fil]) => {
        const a = habitant(m, s);
        const b = habitant(m, l);
        if (fil) a.talkChainTopic = "job";
        const sim = { time: t, life: { recentVillageEmits: Array.from({ length: emis }, (_, i) => ({ at: t - 1 - i })) } };
        return m.talk.shouldSpeakNow(sim, a, b);
      },
    },
    {
      name: "Turns",
      comment: "talkMaxTurnsFor(speaker, listener, { fatigue }) et talkHoldDurationFor",
      args: ["string", "string", "double", "int", "int", "string"],
      ret: [
        { name: "turns", type: "int" },
        { name: "hold", type: "double" },
      ],
      inputs: croiser([["npc-0", "npc-1"], ["npc-2", "npc-9"], ["npc-14", "npc-3"]], [0, 45, -45], [0, 1, 2], [0, 1, 3], ["observer", "gatherFood"])
        .map(([[s, l], rel, fat, bes, goal]) => [s, l, rel, fat, bes, goal]),
      call: (m, [s, l, rel, fat, bes, goal]) => {
        const a = poser(habitant(m, s, { goal }), BESOINS[bes]);
        const b = habitant(m, l);
        a.relations[b.id] = rel;
        return { turns: m.talk.talkMaxTurnsFor(a, b, { fatigue: fat }), hold: m.talk.talkHoldDurationFor(a, b) };
      },
    },
    {
      name: "Refuse",
      comment: "replyUtteranceFor(...).detail === \"refuse\" — besoin de l'auditeur, lien, fatigue, graine",
      args: ["int", "double", "int", "double"],
      ret: "bool",
      inputs: croiser(BESOINS.map((_, i) => i), [0, 45, -45, 20], [0, 1, 2, 5], [7, 1234567, 4294967295, 99991, 31337, 2718281828])
        .map(([b, rel, fat, seed]) => [b, rel, fat, seed]),
      call: (m, [b, rel, fat, seed]) => {
        const replier = poser(habitant(m, "npc-1"), BESOINS[b]);
        const other = habitant(m, "npc-0");
        replier.relations[other.id] = rel;
        const kind = m.talk.bondKindBetween(replier, other);
        const r = m.talk.replyUtteranceFor(replier, other, { topic: "bond", detail: null }, { kind, seed, time: 101, fatigue: fat, sim: { time: 100, life: {} } });
        return r?.detail === "refuse";
      },
    },
    {
      name: "Stage",
      comment: "bumpRelation(sim, a, b, dA, dB) — palier et moodlet newFriend",
      args: ["double", "double", "double", "double", "double"],
      ret: [
        { name: "relA", type: "double" },
        { name: "relB", type: "double" },
        { name: "friendA", type: "bool" },
        { name: "friendB", type: "bool" },
        { name: "moraleA", type: "double" },
        { name: "moraleB", type: "double" },
      ],
      inputs: croiser([0, 9, 14, 39, 42, 44, 64, 69, 79, 84, 99], [0, 40], [6, 8], [5, 7], [60, 0, 99])
        .map(([ra, rb, da, db, mo]) => [ra, rb, da, db, mo]),
      call: (m, [ra, rb, da, db, mo]) => {
        const a = habitant(m, "npc-0");
        const b = habitant(m, "npc-1");
        a.relations[b.id] = ra;
        b.relations[a.id] = rb;
        a.morale = mo;
        b.morale = mo;
        m.bonds.bumpRelation({ time: 50, day: 1 }, a, b, da, db);
        return {
          relA: a.relations[b.id], relB: b.relations[a.id],
          friendA: (a.moodlets || []).some((x) => x.id === "newFriend"),
          friendB: (b.moodlets || []).some((x) => x.id === "newFriend"),
          moraleA: a.morale, moraleB: b.morale,
        };
      },
    },
    {
      name: "Meeting",
      comment: "noteMeeting(sim, a, b) repete : fiche (confiance, etiquette, rencontres), theorie de l'esprit",
      args: ["int", "string"],
      ret: [
        { name: "trustA", type: "double" },
        { name: "tagA", type: "string" },
        { name: "meetsA", type: "int" },
        { name: "trustB", type: "double" },
        { name: "tagB", type: "string" },
        { name: "tomConf", type: "double" },
        { name: "tomGoal", type: "string" },
      ],
      inputs: croiser([1, 2, 6, 7, 8, 9, 10, 11, 12, 25], ["gatherFood", "socialize"]),
      call: (m, [n, goal]) => {
        const a = habitant(m, "npc-0");
        const b = habitant(m, "npc-1", { goal });
        for (let i = 0; i < n; i += 1) m.social.noteMeeting({ day: 1 + Math.floor(i / 4), time: 10 * i }, a, b);
        const ra = a.mind.people[b.id];
        const rb = b.mind.people[a.id];
        const tom = a.mind.tom[b.id];
        return { trustA: ra.trust, tagA: ra.tag || "", meetsA: ra.meets, trustB: rb.trust, tagB: rb.tag || "", tomConf: tom.confidence, tomGoal: tom.estimatedGoal || "" };
      },
    },
    {
      name: "MemoryBias",
      comment: "socialMemoryBias(npc, \"socialize\") — moyenne des confiances, allies",
      args: ["string"],
      ret: "double",
      inputs: [[""], ["0"], ["2.5"], ["22.5"], ["22.5,2.5"], ["50,50,50,50"], ["-20,10"], ["30,25,-19,5,0"], ["50,50,50,50,50,50,50"]],
      call: (m, [trusts]) => {
        const a = habitant(m, "npc-0");
        a.mind.people = {};
        trusts.split(",").filter(Boolean).forEach((t, i) => {
          const trust = Number(t);
          a.mind.people[`npc-${i + 1}`] = { id: `npc-${i + 1}`, trust, tag: trust >= 22 ? "ally" : trust <= -18 ? "rival" : null, day: 1, meets: 1, hearsay: false };
        });
        return m.social.socialMemoryBias(a, "socialize");
      },
    },
    {
      name: "Seek",
      comment: "pickRememberedSeek(sim, npc) — confiance >= 16 ou allie, portee 22, but cru au travail",
      args: ["string", "string", "string", "double"],
      ret: "string",
      inputs: croiser(["10,17,22", "15.9,16,40", "-20,30,16"], ["5,30,12", "25,8,3"], ["observer,gatherFood,observer", "gatherFood,gatherFood,observer"], [60, 40])
        .map(([trusts, dists, goals, social]) => [trusts, dists, goals, social]),
      call: (m, [trusts, dists, goals, social]) => {
        const a = habitant(m, "npc-0");
        a.social = social;
        a.mind.people = {};
        a.mind.tom = {};
        const ts = trusts.split(",").map(Number);
        const ds = dists.split(",").map(Number);
        const gs = goals.split(",");
        const actors = [a];
        ts.forEach((trust, i) => {
          const id = `npc-${i + 1}`;
          const o = habitant(m, id);
          o.x = a.x + ds[i];
          o.y = a.y;
          actors.push(o);
          a.mind.people[id] = { id, trust, tag: trust >= 22 ? "ally" : trust <= -18 ? "rival" : null, day: 1, meets: 1, hearsay: false };
          a.mind.tom[id] = { estimatedGoal: gs[i], confidence: 0.35, day: 1 };
        });
        const sim = { time: 10, day: 1, actorById: (id) => actors.find((x) => x.id === id) || null };
        return m.social.pickRememberedSeek(sim, a)?.id || "";
      },
    },
    {
      name: "SpotRumor",
      comment: "createInformResourceSpotActs (limite 2) puis commitSpeechActs : gisements entendus",
      args: ["int", "string", "string", "double"],
      ret: [
        { name: "keys", type: "string" },
        { name: "first", type: "string" },
      ],
      inputs: croiser([1, 3, 5], ["", "1", "0,2"], ["", "2", "1,3"], [0, 0.34, 0.5, 0.99])
        .map(([n, hearsay, known, r]) => [n, hearsay, known, r]),
      call: (m, [n, hearsay, known, r]) => {
        const src = habitant(m, "npc-0");
        const tgt = habitant(m, "npc-1");
        const ouiDire = new Set(hearsay.split(",").filter(Boolean).map(Number));
        const connus = new Set(known.split(",").filter(Boolean).map(Number));
        for (let i = 0; i < n; i += 1) {
          const key = `${4 + i},${7 + i}`;
          src.mind.spots[key] = { x: 4.5 + i, y: 7.5 + i, resource: i % 2 ? "wood" : "food", amount: 10 + i, day: 1 + i, hearsay: ouiDire.has(i) };
          if (connus.has(i)) tgt.mind.spots[key] = { x: 4.5 + i, y: 7.5 + i, resource: "food", amount: 1, day: 3, hearsay: false };
        }
        const sim = { time: 12.5, day: 4, rng: () => r, actors: [src, tgt] };
        const acts = m.acts.createInformResourceSpotActs(sim, src, tgt, { limit: 2 });
        m.acts.commitSpeechActs(sim, acts, { source: src, target: tgt });
        const added = Object.keys(tgt.mind.spots).filter((k) => tgt.mind.spots[k].hearsay);
        const f = added.length ? tgt.mind.spots[added[0]] : null;
        return {
          keys: added.join(";"),
          first: f ? `${f.resource}|${f.amount}|${f.day}|${f.hopCount}|${f.sourceId}|${f.originalSourceId}|${f.receivedDay}` : "",
        };
      },
    },
    {
      name: "Moodlet",
      comment: "stampMoodlet newFriend, tickMoodlets, moodletGoalBias(socialize)",
      args: ["double", "double", "double", "int"],
      ret: [
        { name: "morale", type: "double" },
        { name: "bias", type: "double" },
        { name: "count", type: "int" },
      ],
      inputs: croiser([60, 0, 99.99], [0.5, 30, 71.9, 72.1, 80], [1 / 60, 1], [1, 2]),
      call: (m, [moral, apres, dt, poses]) => {
        const a = habitant(m, "npc-0");
        a.morale = moral;
        for (let i = 0; i < poses; i += 1) m.moodlets.stampMoodlet(a, "newFriend", { at: 10 + i });
        m.moodlets.tickMoodlets(a, dt, 10 + apres);
        return { morale: a.morale, bias: m.moodlets.moodletGoalBias(a, "socialize", 10 + apres), count: (a.moodlets || []).length };
      },
    },
    {
      name: "Dominant",
      comment: "dominantNeedLabel(npc) — id et valeur",
      args: ["int"],
      ret: [
        { name: "id", type: "string" },
        { name: "value", type: "double" },
      ],
      inputs: BESOINS.map((_, i) => [i]).concat([[100]]),
      call: (m, [i]) => {
        const a = habitant(m, "npc-0");
        if (i === 100) poser(a, [40, 40, 60, 60, 60, 60, 60, 38]);
        else poser(a, BESOINS[i]);
        const d = m.needs.dominantNeedLabel(a);
        return { id: d.id, value: d.value };
      },
    },
  ],
};
