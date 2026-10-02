// Mesure des tirages `sim.rng` d'un scenario du harnais — partagee par
// trace-sim-rng.mjs (le releve) et gen-goal-noise-vectors.mjs (les vecteurs).
//
// Chargement, masques et tick : exactement ceux d'emit-state-digests.mjs, en tick
// recompose (prouve identique au tick de reference par selftest-harness.mjs).
// `sim.rng` est enveloppe (meme suite, meme etat) et `ref.updateNpc` aussi, pour
// savoir QUEL habitant joue quand un tirage tombe. Rien d'autre n'est touche.

import { chargerReference, appliquerMasques } from "./scenarios/masks.mjs";

const SRC = "/src/";

/** Les cadres de la reference sur la pile : `fonction (src/...:ligne)`. */
function cadresReference() {
  const pile = new Error().stack.split(String.fromCharCode(10)).slice(1);
  const out = [];
  for (const ligne of pile) {
    const m = ligne.match(/at (?:(\S+) \()?(?:file:\/\/\/)?(.*?):(\d+):\d+\)?$/);
    if (!m) continue;
    const fichier = m[2].split(String.fromCharCode(92)).join("/");
    const i = fichier.indexOf(SRC);
    if (i < 0) continue;
    const nom = (m[1] || "(anonyme)").replace(/^Simulation\./, "sim.").replace(/^Object\./, "");
    out.push({ nom, lieu: `${fichier.slice(i + 1)}:${m[3]}` });
  }
  return out;
}

/**
 * Fait tourner `scenario` pendant `ticks` ticks et rend chaque tirage :
 * { t, etape, npc, site, appelant, chemin, decision, etat, valeur }.
 * `appelant` = le cadre de la reference juste au-dessus du site (pour un bruit de
 * but, la ligne de la table qui a appele `goalNoise`).
 */
export async function releverTirages(REF_DIR, scenario, ticks) {
  Error.stackTraceLimit = 64;
  const ref = await chargerReference(REF_DIR);
  const sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
  ref.deserialize(sim, JSON.parse(JSON.stringify(scenario.save)));
  if (scenario.vue) ref.pinSimulationView(sim.simulationBudget, scenario.vue.x, scenario.vue.y);
  const etiqueteur = { courante: "hors-tick", poser(e) { const p = this.courante; this.courante = e; return p; } };
  const applique = appliquerMasques(sim, ref, [...scenario.masques], { recompose: true, etiqueteur });
  const tick = applique.tick;

  let tickCourant = 0;
  let habitant = null;
  const updateNpcOriginal = ref.updateNpc;
  ref.updateNpc = (s, npc, dt) => {
    const avant = habitant;
    habitant = npc?.id ?? null;
    try { return updateNpcOriginal(s, npc, dt); } finally { habitant = avant; }
  };

  const tirages = [];
  const original = sim.rng;
  const enveloppe = () => {
    const etat = original.state();
    const valeur = original();
    const cadres = cadresReference();
    const site = cadres[0] ?? { nom: "?", lieu: "?" };
    const appelant = cadres[1] ?? { nom: "?", lieu: "?" };
    const fin = cadres.findIndex((c) => c.nom === "updateNpc");
    const chemin = (fin >= 0 ? cadres.slice(0, fin + 1) : cadres).map((c) => c.nom).reverse();
    tirages.push({
      t: tickCourant,
      etape: etiqueteur.courante,
      npc: habitant,
      site: `${site.nom} ${site.lieu}`,
      appelant: `${appelant.nom} ${appelant.lieu}`,
      chemin: chemin.join(" > "),
      decision: chemin.includes("chooseGoal"),
      etat,
      valeur,
    });
    return valeur;
  };
  enveloppe.state = () => original.state();
  enveloppe.setState = (s) => original.setState(s);
  sim.rng = enveloppe;

  const etatDepart = original.state();
  for (let t = 1; t <= ticks; t += 1) {
    tickCourant = t;
    tick(scenario.dt);
  }
  return { ref, tirages, etatDepart, etatFin: original.state() };
}

/** Les decisions : tirages consecutifs d'un meme habitant, au meme tick, sous chooseGoal. */
export function decisionsDe(tirages) {
  const decisions = [];
  let courante = null;
  for (const d of tirages) {
    if (!d.decision) { courante = null; continue; }
    if (!courante || courante.t !== d.t || courante.npc !== d.npc) {
      courante = { t: d.t, npc: d.npc, etatAvant: d.etat, tirages: [] };
      decisions.push(courante);
    }
    courante.tirages.push(d);
  }
  return decisions;
}
