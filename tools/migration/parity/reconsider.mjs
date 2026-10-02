// Parite de la reconsideration et du quart de travail — mission reconsider-001.
//
// Fonctions de la reference executees telles quelles :
//   needsReconsiderChance (life/needs.js), villagePhaseFor (life/villageRhythm.js, la phase
//   PERSONNELLE), noteShiftGoalCommit / noteShiftArrival / shiftShields (life/workShift.js,
//   avec opensExtractionShift de sim/metiers/extractionPost.js).
// goalStickinessBonus (sim/npc.js), le collant de but de `commitGoalChoice`, sur une nature sans
// qualite ni defaut (la seule du C++, ecart n° 10).
// `committedReconsiderChance` n'est pas exportee : elle se prouve par le rejeu des 75 tirages
// mesures (Anastasis.Sim.Village.Reconsideration, tools/migration/trace-reconsider.mjs).

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// hunger, energy, social, leisure, hygiene, thirst, health, morale
const CALME = [10, 80, 70, 70, 70, 5, 95, 60];
const CRITIQUE = [90, 5, 5, 5, 5, 99, 5, 0];
const GOALS = ["gatherWood", "build", "craft", "maintain", "deliver", "sell", "fetchInput", "explore", "eat", "rest",
  "socialize", "observer", "closeWorkplace", "gatherFood", "gatherStone", ""];
const DTS = [1 / 60, 1.1, 2.2, 6];
// Faim critique seule (58) : `eat` soulage, `gatherFood` aussi sans ration.
const FAIM = [70, 80, 70, 70, 70, 5, 95, 60];
const TRAITS = [[1, 0.8, 1, 1], [1.6, 0.6, 0.4, 1.3], [0.5, 0, 1.4, 0.7]];
const COURANTS = ["gatherFood", "deliver", "build", "eat", "socialize", "explore", "observer", "", "rest"];
const METRES = ["hunger", "energy", "social", "leisure", "hygiene", "thirst", "health", "morale"];
const npcDe = (m) => Object.fromEntries(METRES.map((k, i) => [k, m[i]]));

// Instants : bords des phases (5 h, 7 h, 11 h 30, 13 h 30, 17 h 30, 21 h), et autour.
const HEURES = [0, 4.99, 5, 6.03, 7, 11.5, 11.49, 13.5, 17.4, 17.5, 20.05, 21, 23.99];
const TEMPS = HEURES.flatMap((h) => [h / 24 * 90, 90 * 3 + h / 24 * 90 + 1e-9]);

const ETATS = ["", "OFF_DUTY", "COMMUTING", "ON_SHIFT", "BREAK"];
// Lieu de travail : aucun, ferme achevee, ferme en chantier, camp de bucherons, grenier.
const POSTES = [["", 0, 0, 1], ["farm", 20, 30, 1], ["farm", 20, 30, 0.5], ["lumbercamp", 40, 12, 1], ["granary", 20, 30, 1]];
// Cible : aucune, dans la cour de la ferme (20.5+3, 30.5), au bord (4,25), dehors.
const CIBLES = [[false, 0, 0], [true, 23.5, 30.5], [true, 20.5 + 4.25, 30.5], [true, 20.5 + 4.2501, 30.5], [true, 43.5, 12.5]];

export default {
  modules: { npc: "src/sim/npc.js", needs: "src/life/needs.js", rhythm: "src/life/villageRhythm.js", shift: "src/life/workShift.js" },
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisReconsiderPureVectors.inl"),

  cases: [
    {
      name: "ReconsiderNeeds",
      comment: "needsReconsiderChance(npc, dt) — calme / critique, but de travail / explore / autre",
      args: ["double", "double", "double", "double", "double", "double", "double", "double", "string", "double"],
      ret: "double",
      inputs: croiser([CALME, CRITIQUE], GOALS, DTS).map(([m, g, dt]) => [...m, g, dt]),
      call: ({ needs }, a) => needs.needsReconsiderChance({ ...npcDe(a.slice(0, 8)), goal: a[8] }, a[9]),
    },
    {
      name: "ReconsiderPersonalPhase",
      comment: "villagePhaseFor(sim, npc) — la phase PERSONNELLE, sans mode de vie / leve-tot / noctambule / autre",
      args: ["double", "string"],
      ret: "string",
      inputs: croiser(TEMPS, ["", "earlyBird", "nightOwl", "wanderer"]),
      call: ({ rhythm }, [time, id]) => rhythm.villagePhaseFor({ time }, id ? { lifestyle: { id } } : {}).id,
    },
    {
      name: "ReconsiderShiftCommit",
      comment: "noteShiftGoalCommit puis shiftShields — etat de depart, but, critique, poste, cible",
      // etat, but du quart, startedAt, floorUntil, but commis, maintenant, critique, poste (type, x, y, progress), cible (a, x, y)
      args: ["string", "string", "double", "double", "string", "double", "bool", "string", "double", "double", "double", "bool", "double", "double"],
      ret: [
        { name: "state", type: "string" },
        { name: "goal", type: "string" },
        { name: "startedAt", type: "double" },
        { name: "floorUntil", type: "double" },
        { name: "shields", type: "bool" },
      ],
      inputs: [
        ...croiser(ETATS, ["build", "deliver", "eat", "gatherFood", "craft", "observer"], [false, true], [0, 1, 2])
          .map(([etat, but, crit, k]) => [etat, etat ? "build" : "", 100, 115, but, [101, 114.99, 115][k], crit, "", 0, 0, 1, false, 0, 0]),
        ...croiser(POSTES, CIBLES, ["gatherFood", "gatherWood", "gatherStone"])
          .map(([[t, x, y, p], [a, tx, ty], but]) => ["", "", 0, 0, but, 50, false, t, x, y, p, a, tx, ty]),
      ],
      call: ({ shift }, a) => {
        const [etat, sGoal, startedAt, floorUntil, goal, now, crit, type, wx, wy, wp, hasT, tx, ty] = a;
        const npc = { ...npcDe(crit ? CRITIQUE : CALME), goal };
        if (etat) npc.workShift = { state: etat, goal: sGoal || null, startedAt, floorUntil };
        if (type) npc.workplace = { type, x: wx, y: wy, progress: wp };
        npc.target = hasT ? { x: tx, y: ty } : null;
        const sim = { time: now };
        shift.noteShiftGoalCommit(sim, npc);
        const ws = npc.workShift;
        return {
          state: ws?.state ?? "",
          goal: ws?.goal ?? "",
          startedAt: ws?.startedAt ?? 0,
          floorUntil: ws?.floorUntil ?? 0,
          shields: shift.shiftShields(sim, npc),
        };
      },
    },
    {
      name: "ReconsiderStickiness",
      comment: "goalStickinessBonus(sim, npc, goal) — metres, but courant / ligne, bascule de phase, trait, session, sac",
      // metres x8, but courant, ligne, a basculé, il y a (s), trait x4, session, sac
      args: ["double", "double", "double", "double", "double", "double", "double", "double", "string", "string",
        "bool", "double", "double", "double", "double", "double", "bool", "int"],
      ret: "double",
      inputs: croiser([CALME, CRITIQUE, FAIM], COURANTS, [false, true], [[false, 0], [true, 1], [true, 3]], TRAITS, [false, true], [0, 4])
        .map(([m, g, autre, [flip, ago], t, ws, sac]) => [...m, g, autre ? "drink" : g, flip, ago, ...t, ws, sac]),
      call: ({ npc }, a) => {
        const [g, ligne, flip, ago, tb, tt, tg, te, ws, sac] = a.slice(8);
        const now = 500;
        const sujet = {
          ...npcDe(a.slice(0, 8)), goal: g || null, inventory: { food: sac },
          trait: { build: tb, trade: tt, gather: tg, explore: te },
          nature: { corps: 1, esprit: 1, coeur: 1, qualities: [], flaws: [] },
          workSession: ws ? { craftId: "farm" } : null,
        };
        if (flip) sujet.phaseChangedAt = now - ago;
        const sim = { time: now, countBuildings: () => 0, countPlannedBuildings: () => 0 };
        return npc.goalStickinessBonus(sim, sujet, ligne);
      },
    },
    {
      name: "ReconsiderShiftArrival",
      comment: "noteShiftArrival — COMMUTING -> ON_SHIFT, les autres etats intacts",
      args: ["string"],
      ret: "string",
      inputs: ETATS.map((e) => [e]),
      call: ({ shift }, [etat]) => {
        const npc = etat ? { workShift: { state: etat, goal: "build", startedAt: 0, floorUntil: 15 } } : {};
        shift.noteShiftArrival({ time: 1 }, npc);
        return npc.workShift?.state ?? "";
      },
    },
  ],
};
