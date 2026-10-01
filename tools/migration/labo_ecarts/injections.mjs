// REINJECTIONS — un ecart de portage, reproduit DANS la reference JS.
//
// Le laboratoire (LISEZMOI.md) mesure l'effet d'un ecart en faisant tourner la
// reference deux fois sur le meme etat : telle quelle, puis avec l'ecart. Une
// reinjection ne modifie jamais le depot JS : elle agit sur l'instance `sim` ou
// sur l'objet `ref` que `tickRecompose` appelle (scenarios/masks.mjs), comme un
// masque. Elle rend un compteur d'ACTIVATION : un ecart qui ne s'active jamais
// dans un scenario y est dormant, et le verdict le dit au lieu de conclure NEUTRE.
//
// Une reinjection reproduit ce que fait le C++, pas ce qu'on imagine qu'il fait :
// chaque entree cite l'endroit du C++ qu'elle imite.

import { join } from "node:path";
import { pathToFileURL } from "node:url";

export const INJECTIONS = {
  aucun: {
    description: "la reference telle quelle (bras temoin)",
    async appliquer() {
      return { activations: 0 };
    },
  },

  // Ecart n° 5 — le C++ ne branche pas `consumeNpcSimulationCadence` (Core/AnastasisSimBudget.h
  // porte, FVillage::UpdateActors ne l'appelle pas) : chaque habitant pense a chaque tick, avec le
  // dt du tick. Dans la reference, `consumeNpcSimulationCadence` rend `run` a chaque tick quand
  // `sim.simulationBudget` est absent. On le retire donc le temps d'un `updateNpc`, et seulement la.
  // Activation : un appel ou la reference aurait classe l'habitant hors de la bande `near`.
  n05: {
    description: "cadence par bande debranchee : chaque habitant pense a chaque tick (C++ FVillage::UpdateActors)",
    async appliquer(sim, ref, REF) {
      const budget = await import(pathToFileURL(join(REF, "src/sim/simulationBudget.js")).href);
      const compteur = { activations: 0, appels: 0 };
      const original = ref.updateNpc;
      ref.updateNpc = (s, npc, dt) => {
        const directeur = s.simulationBudget;
        compteur.appels += 1;
        if (directeur && budget.classifyNpcSimulationBand(directeur, npc) !== "near") compteur.activations += 1;
        s.simulationBudget = null;
        try { return original(s, npc, dt); } finally { s.simulationBudget = directeur; }
      };
      return compteur;
    },
  },

  // CONTROLE de l'ecart n° 5, pas un ecart : la reference avec la vue du budget EPINGLEE sur le
  // village (barycentre des batiments au depart, `pinSimulationView`), comme une camera de joueur
  // qui regarde le village. Sans camera (harnais, banc), la vue reste en (0, 0) et le village
  // tombe en bande `far` (P3_PREMIER_RAPPORT.md). Activation : un appel ou l'habitant est en `near`.
  vue_village: {
    description: "controle : vue du budget epinglee sur le barycentre des batiments (camera sur le village)",
    async appliquer(sim, ref, REF) {
      const budget = await import(pathToFileURL(join(REF, "src/sim/simulationBudget.js")).href);
      if (!sim.simulationBudget) sim.simulationBudget = ref.createSimulationBudgetDirector();
      const bs = sim.buildings.length ? sim.buildings : sim.actors;
      const x = bs.reduce((s, b) => s + b.x, 0) / bs.length;
      const y = bs.reduce((s, b) => s + b.y, 0) / bs.length;
      budget.pinSimulationView(sim.simulationBudget, x, y);
      const compteur = { activations: 0, appels: 0, vue: { x, y } };
      const original = ref.updateNpc;
      ref.updateNpc = (s, npc, dt) => {
        compteur.appels += 1;
        if (budget.classifyNpcSimulationBand(s.simulationBudget, npc) === "near") compteur.activations += 1;
        return original(s, npc, dt);
      };
      return compteur;
    },
  },

  // Ecart n° 16 — le C++ tire les actes de parole sur les gisements dans `VillageRng`, un flux
  // mulberry32 propre au village, graine = graine du monde (AnastasisSimulation.cpp,
  // `Village.SetRngSeed(Seed)` ; AnastasisVillage.cpp, `SocializeWithCompanion` et
  // `ExchangeSpotRumors`). Dans la reference, ce tirage est le `sim.rng()` de
  // `createInformResourceSpotActs` (src/life/speechActs.js). On enveloppe `sim.rng` : un tirage
  // demande par cette fonction part sur le flux du village, tous les autres sur `sim.rng`.
  // L'etat sauvegarde reste celui de `sim.rng` (comme le C++, qui ne sauvegarde pas VillageRng).
  // Activation : un tirage detourne.
  n16: {
    description: "tirages des rumeurs de gisements sur un flux propre au village (C++ VillageRng)",
    async appliquer(sim, ref, REF) {
      const { makeRng } = await import(pathToFileURL(join(REF, "src/sim/rng.js")).href);
      const village = makeRng(sim.seed >>> 0);
      const principal = sim.rng;
      const compteur = { activations: 0 };
      const enveloppe = () => {
        // Pile : [0] Error, [1] enveloppe, [2] l'appelant direct.
        const appelant = (new Error().stack || "").split("\n")[2] || "";
        if (appelant.includes("createInformResourceSpotActs")) {
          compteur.activations += 1;
          return village();
        }
        return principal();
      };
      enveloppe.state = () => principal.state();
      enveloppe.setState = (v) => principal.setState(v);
      sim.rng = enveloppe;
      return compteur;
    },
  },
};
