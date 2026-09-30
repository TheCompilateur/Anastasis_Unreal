// Parite de l'inertie Noûs — `src/ai/algorithmic/inertia.js`.
// Garder l'action courante si la nouvelle n'est que legerement meilleure,
// sans jamais bloquer une urgence.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { croiser } from "../parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// (type courant, score courant), (type candidat, score, urgence), ecoule, cooldown restant
const COURANTS = [["work", 0.2], ["seek_food", 0.3]];
const CANDIDATS = [["work", 0.25, 0.3], ["seek_food", 0.35, 0.5], ["seek_food", 0.6, 0.5], ["seek_food", 0.4, 0.9], ["sleep", 0.5, 0.86]];
const ECOULES = [0.5, 2.5, 10];
const COOLDOWNS = [0, 1.5];

export default {
  module: "src/ai/algorithmic/inertia.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisNousInertiaVectors.inl"),

  cases: [
    {
      name: "Inertia",
      comment: "evaluateInertia (sans danger)",
      args: ["string", "double", "string", "double", "double", "double", "double"],
      ret: [
        { name: "keep", type: "bool" },
        { name: "reason", type: "string" },
      ],
      inputs: croiser(COURANTS, CANDIDATS, ECOULES, COOLDOWNS)
        .map(([[ct, cs], [nt, ns, nu], e, cd]) => [ct, cs, nt, ns, nu, e, cd]),
      call: (mod, [ct, cs, nt, ns, nu, e, cd]) => mod.evaluateInertia({
        current: { type: ct, score: cs },
        candidate: { type: nt, score: ns, urgency: nu },
        elapsedSeconds: e,
        failureCooldownLeft: cd,
        dangerUrgency: 0,
      }),
    },
  ],
};
