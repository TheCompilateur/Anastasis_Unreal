// AUTOTEST DU HARNAIS — la chaine entiere, emetteur et comparateur.
//
// Le jour ou le harnais annoncera "premier tick divergent: 812", tout le
// travail de portage se reglera sur ce nombre. Il faut donc l'avoir prouve
// avant d'en avoir besoin, sur des divergences dont on connait deja la reponse.
//
// Trois cas, et ce qu'ils prouvent:
//
//   1. Meme graine, deux traces -> aucune divergence.
//      Prouve que l'empreinte ne bouge pas toute seule. Un harnais qui signale
//      du bruit est pire qu'un harnais absent: il fait chercher des bugs qui
//      n'existent pas.
//
//   2. Un ulp au tick 0 -> divergence annoncee au tick 0.
//      Le tick 0 est l'etat au sortir de la genese, avant le premier pas: une
//      divergence a ce tick accuse la GENERATION du monde, pas la boucle. Les
//      distinguer evite de chercher un bug de simulation la ou il n'y en a pas.
//
//   3. UN SEUL ULP ajoute a `actors[0].x` au tick K -> divergence annoncee
//      exactement au tick K.
//      C'est le cas qui compte. La doctrine du projet est qu'un ulp n'est pas
//      cosmetique: sur `dist < radius`, il fait basculer une decision de PNJ.
//      Un harnais qui raterait cet ecart laisserait passer precisement la
//      classe de bug qu'il est cense attraper, et il la laisserait passer en
//      silence.
//
//   4. Graines differentes -> refus de comparer.
//      Une divergence entre deux traces prises dans des conditions
//      differentes n'accuse rien. Le comparateur doit refuser, pas rendre un
//      "tick 0" que quelqu'un mettrait sur le dos du portage.
//
// Chaque trace est produite dans un PROCESSUS NEUF, comme en usage reel. Ce
// n'est pas une precaution de confort: deux `new Simulation(graine)` dans le
// meme processus divergent des le tick 0 sur `logs[].id`, un compteur de
// module. Le harnais l'a trouve a son premier essai.
//
//   node tools/migration/selftest-harness.mjs [-ref <chemin>] [-ticks 120]

import { execFileSync } from "node:child_process";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const ICI = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const argOf = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : d; };
const REF = argOf("-ref", "C:/dev/Jeux IV Kingdoms");
const TICKS = Number(argOf("-ticks", 120));
const GRAINE = 33344;
const K = Math.min(40, TICKS);

const atelier = mkdtempSync(join(tmpdir(), "anastasis-harnais-"));

function emettre(nom, args) {
  const chemin = join(atelier, nom);
  execFileSync(process.execPath, [
    join(ICI, "emit-state-digests.mjs"),
    "-ref", REF, "-ticks", String(TICKS), "-out", chemin, ...args,
  ], { stdio: ["ignore", "ignore", "pipe"] });
  return chemin;
}

function comparer(a, b) {
  try {
    const sortie = execFileSync(process.execPath, [
      join(ICI, "compare-digests.mjs"), a, b, "-json",
    ], { encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] });
    return JSON.parse(sortie);
  } catch (e) {
    // Le comparateur sort en 1 quand il trouve une divergence: c'est un
    // resultat, pas une panne. Son rapport est sur stdout dans les deux cas.
    if (e.stdout) return JSON.parse(e.stdout);
    throw e;
  }
}

const resultats = [];
function verifier(nom, obtenu, attendu) {
  const ok = obtenu === attendu;
  resultats.push({ nom, ok });
  console.log(`${ok ? "PASS" : "FAIL"}  ${nom}`);
  console.log(`      attendu: ${attendu}   obtenu: ${obtenu}`);
}

console.log(`Autotest du harnais — ${TICKS} ticks, graine ${GRAINE}, reference ${REF}`);
console.log();

try {
  const base = emettre("base.jsonl", ["-seed", String(GRAINE)]);
  const jumelle = emettre("jumelle.jsonl", ["-seed", String(GRAINE)]);
  const genese = emettre("genese.jsonl", ["-seed", String(GRAINE), "-perturb", "0"]);
  const perturbee = emettre("perturbee.jsonl", ["-seed", String(GRAINE), "-perturb", String(K)]);
  const autreGraine = emettre("autre.jsonl", ["-seed", String(GRAINE + 1)]);

  verifier("1. meme graine, deux traces -> aucune divergence",
    comparer(base, jumelle).premierDivergent, null);

  verifier("2. un ulp au tick 0 -> divergence annoncee au tick 0",
    comparer(base, genese).premierDivergent, 0);

  verifier(`3. un ulp sur actors[0].x au tick ${K} -> divergence annoncee au tick ${K}`,
    comparer(base, perturbee).premierDivergent, K);

  verifier("4. graines differentes -> refus de comparer",
    comparer(base, autreGraine).comparable, false);
} finally {
  rmSync(atelier, { recursive: true, force: true });
}

console.log();
const echecs = resultats.filter((r) => !r.ok);
if (echecs.length) {
  console.log(`ECHEC — ${echecs.length} cas sur ${resultats.length}.`);
  console.log("Tant que ceci echoue, aucun nombre sorti du harnais ne doit etre cru.");
  process.exit(1);
}
console.log(`OK — ${resultats.length} cas sur ${resultats.length}.`);
