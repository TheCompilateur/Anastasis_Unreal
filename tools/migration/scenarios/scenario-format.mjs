// FORMAT DE SCENARIO — partage par le constructeur, l'emetteur et le comparateur.
//
// Un scenario est un JSON:
//   kind        "anastasis-scenario"
//   format      FORMAT_SCENARIO
//   name        nom (= recette tools/migration/scenarios/<name>.mjs)
//   reference   { tag, commit, modifie } — le commit JS contre lequel il a ete construit
//   seed, dt    graine et pas de temps de la trace
//   dayDeferred "tick" (2 travaux de minuit par tick, comme le jeu et le C++) | "flush"
//   vue         { x, y } : la vue du budget de simulation, epinglee des deux cotes
//               (`pinSimulationView`). `simulationBudget` n'est pas dans `serialize`:
//               sans ce champ, la vue reste en (0, 0) et le village est simule « de
//               loin » (bande far, 1 Hz). null = vue par defaut.
//   sections    perimetre: les sections de `serialize` que le harnais compare
//   masques     identifiants du registre (masks.mjs), appliques par l'emetteur
//   masquesDetail, etatVide, nonMasquables, auditRng, pointFixe, recette — documentation
//   empreinte   voir `empreinteScenario`
//   save        la sauvegarde JS (`serialize`), chargee par `deserialize`
//
// L'EMPREINTE couvre ce qui decide d'une trace — format, nom, graine, dt, mode de
// la file de minuit, perimetre, masques, sauvegarde — et rien de ce qui la
// documente. Elle est calculee avec le hacheur du harnais (state-digest.mjs): un
// seul hacheur dans la chaine. L'emetteur la recalcule au chargement et refuse un
// fichier dont l'empreinte ecrite ne correspond plus au contenu.

import { readFileSync } from "node:fs";
import { execFileSync } from "node:child_process";

import { digestState } from "../state-digest.mjs";

// 2 : la vue du budget (`vue`) entre dans le scenario et dans son empreinte.
export const FORMAT_SCENARIO = 2;

export function empreinteScenario(s) {
  return digestState({
    format: s.format,
    name: s.name,
    seed: s.seed,
    dt: s.dt,
    dayDeferred: s.dayDeferred,
    vue: s.vue ?? null,
    sections: s.sections,
    masques: s.masques,
    save: s.save,
  }).global;
}

export function chargerScenario(chemin) {
  const s = JSON.parse(readFileSync(chemin, "utf8"));
  if (s.kind !== "anastasis-scenario") throw new Error(`${chemin}: pas un scenario (kind=${s.kind})`);
  if (s.format !== FORMAT_SCENARIO) throw new Error(`${chemin}: format ${s.format}, attendu ${FORMAT_SCENARIO}`);
  for (const cle of ["name", "seed", "dt", "dayDeferred", "sections", "masques", "save", "empreinte"]) {
    if (s[cle] === undefined) throw new Error(`${chemin}: champ \`${cle}\` absent`);
  }
  if (!["tick", "flush"].includes(s.dayDeferred)) throw new Error(`${chemin}: dayDeferred=${s.dayDeferred}`);
  const calculee = empreinteScenario(s);
  if (calculee !== s.empreinte) {
    throw new Error(`${chemin}: empreinte ecrite ${s.empreinte}, contenu ${calculee} — fichier modifie a la main ou perime, le reconstruire`);
  }
  return s;
}

/** Commit, tag exact et nombre de modifications non commitees sous `src/` du depot JS. */
export function provenanceReference(REF) {
  const git = (...args) => {
    try { return execFileSync("git", ["-C", REF, ...args], { encoding: "utf8", stdio: ["ignore", "pipe", "ignore"] }).trim(); }
    catch { return null; }
  };
  const etat = git("status", "--porcelain", "--", "src");
  return {
    commit: git("log", "-1", "--format=%H"),
    court: git("log", "-1", "--format=%h"),
    tag: git("describe", "--tags", "--exact-match", "HEAD"),
    modifie: etat ? etat.split(/\r?\n/).length : 0,
  };
}
