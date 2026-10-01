// AUTOTEST DU HARNAIS — la chaine entiere, emetteur et comparateur.
//
// Le jour ou le harnais annoncera "premier tick divergent: 812", tout le
// travail de portage se reglera sur ce nombre. Il faut donc l'avoir prouve
// avant d'en avoir besoin, sur des divergences dont on connait deja la reponse.
//
// Sans scenario (genese complete de la reference, `-ticks` ticks):
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
// Avec le scenario `endurance` (sauvegarde JS, masques actifs, `-scenario-days`
// jours, une empreinte A CHAQUE TICK):
//
//   5. Deux traces masquees -> aucune divergence sur toute la duree.
//      Les masques ne doivent introduire aucun bruit: le chargement par
//      `deserialize`, le tick recompose, la file de minuit etiquetee.
//
//   6. Un ulp sur `actors[0].x` au tick K, scenario et masques actifs ->
//      divergence annoncee au tick K.
//
//   7. Une trace SANS masque face a une trace masquee -> refus de comparer.
//      Comparer les deux jugerait les masques, pas le portage.
//
//   8. Une section HORS du jugement qui diverge n'est pas annoncee.
//      Un ulp sur `colony.treasury` (hors perimetre) au tick K: le comparateur,
//      limite par `-sections actors,buildings`, rend « identiques » et range
//      `colony` parmi les sections ignorees. Le cas verifie aussi, en lisant
//      les traces, que `colony` a bien diverge au tick K — sinon il ne
//      prouverait rien.
//
//   9. `-sections` hors du perimetre du scenario -> refus de conclure.
//
//  10. Le tick recompose SANS masque EST le tick de reference.
//      `-sans-masques -tick reference` contre `-sans-masques -tick recompose`,
//      toute la duree: aucune divergence. C'est ce qui autorise a masquer en
//      recomposant le tick: hors des etapes masquees, il ne change rien.
//
// Chaque trace est produite dans un PROCESSUS NEUF, comme en usage reel. Ce
// n'est pas une precaution de confort: deux `new Simulation(graine)` dans le
// meme processus divergent des le tick 0 sur `logs[].id`, un compteur de
// module. Le harnais l'a trouve a son premier essai. Les traces longues
// tournent en parallele (`-jobs`).
//
//   node tools/migration/selftest-harness.mjs -ref <tag de reference> [-ticks 120]
//        [-scenario tools/migration/scenarios/endurance.json] [-scenario-days 3] [-jobs 4]
//        [-sans-scenario]

import { execFileSync, spawn } from "node:child_process";
import { mkdtempSync, rmSync, readFileSync, existsSync } from "node:fs";
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
const SCENARIO = argOf("-scenario", join(ICI, "scenarios", "endurance.json"));
const JOURS = Number(argOf("-scenario-days", 3));
const JOBS = Math.max(1, Number(argOf("-jobs", 4)));
const AVEC_SCENARIO = !argv.includes("-sans-scenario");
const KS = 300; // tick perturbe dans les cas de scenario

const atelier = mkdtempSync(join(tmpdir(), "anastasis-harnais-"));

function emettre(nom, args) {
  const chemin = join(atelier, nom);
  execFileSync(process.execPath, [
    join(ICI, "emit-state-digests.mjs"),
    "-ref", REF, "-ticks", String(TICKS), "-out", chemin, ...args,
  ], { stdio: ["ignore", "ignore", "pipe"] });
  return chemin;
}

/** Les traces de scenario, en parallele, `JOBS` processus a la fois. */
async function emettreEnParallele(demandes) {
  const chemins = {};
  const file = [...demandes];
  const debut = Date.now();
  const ouvrier = async () => {
    while (file.length) {
      const { nom, args } = file.shift();
      const chemin = join(atelier, nom);
      await new Promise((ok, ko) => {
        const p = spawn(process.execPath, [join(ICI, "emit-state-digests.mjs"), "-ref", REF, "-out", chemin, ...args],
          { stdio: ["ignore", "ignore", "pipe"] });
        let err = "";
        p.stderr.on("data", (d) => { err += d; });
        p.on("close", (code) => (code === 0 ? ok() : ko(new Error(`${nom} : sortie ${code}\n${err}`))));
      });
      chemins[nom] = chemin;
    }
  };
  await Promise.all(Array.from({ length: JOBS }, ouvrier));
  console.log(`      (${demandes.length} traces de scenario en ${((Date.now() - debut) / 1000).toFixed(0)} s, ${JOBS} a la fois)`);
  return chemins;
}

function comparer(a, b, extra = []) {
  try {
    const sortie = execFileSync(process.execPath, [
      join(ICI, "compare-digests.mjs"), a, b, "-json", ...extra,
    ], { encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] });
    return JSON.parse(sortie);
  } catch (e) {
    // Le comparateur sort en 1 quand il trouve une divergence, en 3 quand il
    // refuse: ce sont des resultats, pas des pannes. Son rapport est sur
    // stdout dans tous les cas.
    if (e.stdout) return JSON.parse(e.stdout);
    throw e;
  }
}

/** Premier tick ou la section `nom` differe entre deux traces, lu directement. */
function premierEcartDeSection(a, b, nom) {
  const lire = (chemin) => new Map(readFileSync(chemin, "utf8").split(/\r?\n/).filter(Boolean).slice(1)
    .map((l) => JSON.parse(l)).map((o) => [o.t, o.s?.[nom]]));
  const ea = lire(a);
  const eb = lire(b);
  for (const [t, h] of [...ea.entries()].sort((x, y) => x[0] - y[0])) {
    if (eb.has(t) && eb.get(t) !== h) return t;
  }
  return null;
}

const resultats = [];
function verifier(nom, obtenu, attendu, detail = "") {
  const ok = obtenu === attendu;
  resultats.push({ nom, ok });
  console.log(`${ok ? "PASS" : "FAIL"}  ${nom}`);
  console.log(`      attendu: ${attendu}   obtenu: ${obtenu}${detail ? `   ${detail}` : ""}`);
}

console.log(`Autotest du harnais — reference ${REF}`);
console.log(`  sans scenario : ${TICKS} ticks, graine ${GRAINE}`);
if (AVEC_SCENARIO) console.log(`  scenario      : ${SCENARIO}, ${JOURS} jours, chaque tick`);
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

  if (AVEC_SCENARIO) {
    if (!existsSync(SCENARIO)) throw new Error(`scenario introuvable : ${SCENARIO} (build-scenario.mjs)`);
    const sc = ["-scenario", SCENARIO];
    const t = await emettreEnParallele([
      { nom: "s-base.jsonl", args: [...sc, "-days", String(JOURS)] },
      { nom: "s-jumelle.jsonl", args: [...sc, "-days", String(JOURS)] },
      { nom: "s-ref.jsonl", args: [...sc, "-days", String(JOURS), "-sans-masques", "-tick", "reference"] },
      { nom: "s-recompose.jsonl", args: [...sc, "-days", String(JOURS), "-sans-masques", "-tick", "recompose"] },
      { nom: "s-perturbee.jsonl", args: [...sc, "-ticks", String(2 * KS), "-perturb", String(KS)] },
      { nom: "s-colonie.jsonl", args: [...sc, "-ticks", String(2 * KS), "-perturb", String(KS), "-perturb-path", "colony.treasury"] },
      { nom: "s-sans-masques.jsonl", args: [...sc, "-ticks", "60", "-sans-masques"] },
    ]);

    const c5 = comparer(t["s-base.jsonl"], t["s-jumelle.jsonl"]);
    const g5 = premierEcartDeSection(t["s-base.jsonl"], t["s-jumelle.jsonl"]);
    verifier(`5. endurance, masques actifs, ${JOURS} jours, deux traces -> aucune divergence`,
      c5.premierDivergent === null && g5 === null, true,
      `(${c5.ticksCompares} ticks, ${c5.sectionsJugees?.length} sections jugees, ${c5.scenario?.masques?.length} masques ; empreinte globale : ${g5 === null ? "identique" : "ecart au tick " + g5})`);

    verifier(`6. endurance, masques actifs, un ulp sur actors[0].x au tick ${KS} -> divergence annoncee au tick ${KS}`,
      comparer(t["s-base.jsonl"], t["s-perturbee.jsonl"]).premierDivergent, KS);

    const c7 = comparer(t["s-base.jsonl"], t["s-sans-masques.jsonl"]);
    verifier("7. trace sans masque face a une trace masquee -> refus de comparer",
      c7.comparable, false, c7.incompatibles ? `(${c7.incompatibles[0]})` : "");

    const ecartColonie = premierEcartDeSection(t["s-base.jsonl"], t["s-colonie.jsonl"], "colony");
    const c8 = comparer(t["s-base.jsonl"], t["s-colonie.jsonl"], ["-sections", "actors,buildings"]);
    verifier(`8. colony.treasury perturbee au tick ${KS}, hors -sections actors,buildings -> non annoncee`,
      c8.premierDivergent === null && c8.sectionsIgnorees?.includes("colony") && ecartColonie === KS, true,
      `(colony diverge reellement au tick ${ecartColonie} ; rapport : premierDivergent=${c8.premierDivergent}, colony ignoree=${c8.sectionsIgnorees?.includes("colony")})`);

    const c9 = comparer(t["s-base.jsonl"], t["s-colonie.jsonl"], ["-sections", "actors,colony"]);
    verifier("9. -sections hors du perimetre du scenario (colony) -> refus de conclure",
      c9.comparable, false, c9.horsPerimetre ? `(${c9.horsPerimetre[0]})` : "");

    // Le comparateur juge le perimetre ; l'equivalence doit tenir sur TOUT
    // l'etat: l'empreinte globale est lue directement.
    const c10 = comparer(t["s-ref.jsonl"], t["s-recompose.jsonl"]);
    const g10 = premierEcartDeSection(t["s-ref.jsonl"], t["s-recompose.jsonl"]);
    verifier(`10. sans masque, tick recompose contre tick de reference, ${JOURS} jours -> aucune divergence, sur tout l'etat`,
      c10.premierDivergent === null && g10 === null, true,
      `(${c10.ticksCompares} ticks ; perimetre : ${c10.premierDivergent === null ? "identique" : "tick " + c10.premierDivergent} ; empreinte globale : ${g10 === null ? "identique" : "ecart au tick " + g10})`);
  }
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
