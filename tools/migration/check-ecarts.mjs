// ECARTS — un comportement C++ qui n'est pas la reference se declare a son entree.
//
// Le portage absorbe le simulateur JS en masse, puis viendra la stabilisation.
// Elle ne marche que si, devant une divergence du harnais, on sait dire tout de
// suite : bug de portage, ou ecart connu ? Ce controleur tient la seconde
// moitie de la reponse a jour. Protocole : docs/migration/PROTOCOLE_ECARTS.md.
// Registre : Source/AnastasisSim/ECARTS.md.
//
//   node tools/migration/check-ecarts.mjs
//       registre bien forme, et chaque marque `ecart n°N` du code pointe vers
//       une fiche ouverte.
//   node tools/migration/check-ecarts.mjs -base main -handoff docs/unreal/handoffs/<m>.md
//       portail de mission (appele par `agent-worktree.ps1 finish`). Si la
//       mission touche le C++ de Source/AnastasisSim (hors tests), sa fiche de
//       passation a une section `## ECARTS` qui nomme les ecarts ouverts,
//       modifies ou fermes, ou dit `AUCUN` et pourquoi ; et pas de tirage hors
//       `sim.rng`.
//   node tools/migration/check-ecarts.mjs -section actors
//       les ecarts ouverts qui font diverger une section du harnais : les
//       suspects connus, a ecarter avant de chercher un bug.
//   node tools/migration/check-ecarts.mjs -bilan
//       compte par destin : la mesure de la stabilisation.
//
// Sortie : ECARTS::PASS / ECARTS::NON_CONCERNE / ECARTS::FAIL ; code 1 si FAIL.
// Aucune dependance : node >= 18 et git.

import { execFileSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, isAbsolute, join, relative, resolve, sep } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const REGISTRE = "Source/AnastasisSim/ECARTS.md";
const SIM = "Source/AnastasisSim/";

const CLASSES = ["REDUIT", "SUBSTITUT", "EXTENSION", "REFERENCE"];
const DESTINS = ["A_FERMER", "A_TRANCHER", "ASSUME"];
const STATUTS = ["OUVERT", "FERME"];
const REQUIS = ["classe", "destin", "statut", "entree", "reference", "cpp", "harnais"];
/** Sections de `serialize` que le harnais juge aujourd'hui (scenario endurance). */
const SECTIONS_CONNUES = ["seed", "rng", "w", "h", "time", "day", "tileDiff", "buildings", "actors", "mealReservations"];

/** Marque de code : `ecart n°10`, `ECART DECLARE n°1`, `écart n° 16`. */
const MARQUE = /[ée]cart(?:\s+d[ée]clar[ée]e?)?\s+n\s*°\s*(\d+)/gi;
/** Tirages hors `sim.rng` : non deterministes, interdits dans la simulation (P3_PLAN.md §7). */
const TIRAGE_INTERDIT = /\bFRandomStream\b|\bFMath::(?:S?Rand|FRand|RandRange|FRandRange|RandInit|VRand|RandBool)\b|\bstd::(?:mt19937|random_device|rand)\b|(?<![\w.:>])rand\s*\(/;
/** Un nouveau generateur deterministe : un flux de plus a cote de `sim.rng`. */
const NOUVEAU_FLUX = /\bFAnastasisRng\s*\(|\bFAnastasisRng\s+\w+\s*(?:[;={(]|$)/;
/** Le code avoue une reduction. */
const AVEU = /\bnon\s+port[ée]e?s?\b/i;

// --- arguments --------------------------------------------------------------

function args(argv) {
  const o = {};
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "-bilan") o.bilan = true;
    else if (a === "-base" || a === "-handoff" || a === "-section") o[a.slice(1)] = argv[++i];
    else if (a === "-h" || a === "--help") o.aide = true;
    else throw new Error(`argument inconnu : ${a}`);
  }
  return o;
}

// --- lecture ----------------------------------------------------------------

const lire = (p) => readFileSync(p, "utf8").replace(/\r\n?/g, "\n");

function git(...a) {
  return execFileSync("git", a, { cwd: ROOT, encoding: "utf8", maxBuffer: 64 * 1024 * 1024, stdio: ["ignore", "pipe", "pipe"] }).replace(/\r\n?/g, "\n");
}

/** Fiches du registre : { n, titre, champs, texte, ligne }. */
export function lireRegistre(texte) {
  const fiches = [];
  let f = null;
  texte.split("\n").forEach((l, i) => {
    const t = /^###\s+n\s*°\s*(\d+)\s+[—-]+\s+(.+)$/.exec(l);
    if (t) {
      f = { n: Number(t[1]), titre: t[2].trim(), champs: {}, texte: "", ligne: i + 1 };
      fiches.push(f);
      return;
    }
    if (/^#{1,3}\s/.test(l)) { f = null; return; }
    if (!f) return;
    const c = /^-\s+\*\*(\w+)\*\*\s*:\s*(.*)$/.exec(l);
    if (c) f.champs[c[1]] = c[2].trim();
    else if (l.trim()) f.texte += l.trim() + " ";
  });
  return fiches;
}

/** Problemes d'une liste de fiches : { fail: [], warn: [] }. */
export function validerRegistre(fiches, masquesConnus) {
  const fail = [];
  const warn = [];
  const vus = new Map();
  for (const f of fiches) {
    const id = `n° ${f.n} (ligne ${f.ligne})`;
    if (vus.has(f.n)) fail.push(`${id} : numero deja pris ligne ${vus.get(f.n)}`);
    vus.set(f.n, f.ligne);
    for (const k of REQUIS) if (!f.champs[k]) fail.push(`${id} : champ \`${k}\` manquant`);
    const { classe, destin, statut } = f.champs;
    if (classe && !CLASSES.includes(classe)) fail.push(`${id} : classe \`${classe}\` (attendu ${CLASSES.join(" | ")})`);
    if (destin && !DESTINS.includes(destin)) fail.push(`${id} : destin \`${destin}\` (attendu ${DESTINS.join(" | ")})`);
    if (statut && !STATUTS.includes(statut)) fail.push(`${id} : statut \`${statut}\` (attendu ${STATUTS.join(" | ")})`);
    if (destin === "A_FERMER" && statut !== "FERME") {
      if (!f.champs.fermeture) fail.push(`${id} : A_FERMER sans \`fermeture\` (quelle mission le fermera ?)`);
      else if (/[àa] attribuer/i.test(f.champs.fermeture)) warn.push(`${id} : une partie de la fermeture est « a attribuer »`);
    }
    if (destin === "ASSUME" && !/\d{4}-\d{2}-\d{2}.*Alexandre/.test(f.champs.decision || "")) {
      fail.push(`${id} : ASSUME sans \`decision\` datee d'Alexandre — un agent ecrit A_TRANCHER`);
    }
    if (classe === "EXTENSION" && !f.champs.activation) fail.push(`${id} : EXTENSION sans \`activation\``);
    if (statut === "FERME" && !f.champs.ferme_par) fail.push(`${id} : FERME sans \`ferme_par\``);
    if (f.champs.harnais && f.champs.harnais !== "aucune") {
      for (const s of sectionsDe(f)) {
        if (!SECTIONS_CONNUES.includes(s)) warn.push(`${id} : section \`${s}\` hors du perimetre actuel du harnais`);
      }
    }
    if (f.champs.masques && masquesConnus) {
      for (const m of f.champs.masques.split(",").map((x) => x.trim()).filter(Boolean)) {
        if (!masquesConnus.has(m)) fail.push(`${id} : masque \`${m}\` absent de tools/migration/scenarios/masks.mjs`);
      }
    }
    if (f.texte.trim().length < 40) warn.push(`${id} : texte trop court pour dire ce que fait le C++`);
  }
  return { fail, warn };
}

const sectionsDe = (f) => (f.champs.harnais || "").split(",").map((s) => s.trim()).filter((s) => s && s !== "aucune");

async function masquesConnus() {
  try {
    const m = await import(pathToFileURL(join(ROOT, "tools/migration/scenarios/masks.mjs")).href);
    return new Set(Object.keys(m.MASQUES || {}));
  } catch {
    return null; // pas de verdict sur les masques plutot qu'un faux FAIL
  }
}

/** Marques `ecart n°N` dans Source/ : Map n -> ["fichier:ligne"]. */
function marquesDuCode() {
  const out = new Map();
  const marcher = (dir) => {
    for (const nom of readdirSync(dir)) {
      const p = join(dir, nom);
      if (statSync(p).isDirectory()) { marcher(p); continue; }
      if (!/\.(h|hpp|cpp|inl)$/.test(nom)) continue;
      const rel = relative(ROOT, p).split(sep).join("/");
      lire(p).split("\n").forEach((l, i) => {
        for (const m of l.matchAll(MARQUE)) {
          const n = Number(m[1]);
          if (!out.has(n)) out.set(n, []);
          out.get(n).push(`${rel}:${i + 1}`);
        }
      });
    }
  };
  if (existsSync(join(ROOT, "Source"))) marcher(join(ROOT, "Source"));
  return out;
}

function verifierMarques(fiches, marques) {
  const fail = [];
  const warn = [];
  const parN = new Map(fiches.map((f) => [f.n, f]));
  for (const [n, lieux] of [...marques].sort((a, b) => a[0] - b[0])) {
    const f = parN.get(n);
    if (!f) fail.push(`marque ecart n°${n} sans fiche dans ${REGISTRE} : ${lieux.slice(0, 3).join(", ")}${lieux.length > 3 ? " …" : ""}`);
    else if (f.champs.statut === "FERME") fail.push(`marque ecart n°${n} vers une fiche FERME (marque perimee) : ${lieux.slice(0, 3).join(", ")}`);
  }
  const sansMarque = fiches
    .filter((f) => f.champs.statut === "OUVERT" && f.champs.classe !== "REFERENCE" && !marques.has(f.n))
    .map((f) => f.n);
  if (sansMarque.length) {
    warn.push(`ecarts ouverts sans marque \`ecart n°N\` dans le code : ${sansMarque.join(", ")} — a poser au prochain passage dans ce code`);
  }
  return { fail, warn };
}

// --- portail de mission -----------------------------------------------------

/** Section `## ECARTS` d'une fiche de passation, ou null. */
export function sectionEcarts(fiche) {
  const m = /^##\s+ECARTS\s*$([\s\S]*?)(?=^##\s|(?![\s\S]))/m.exec(fiche);
  return m ? m[1].trim() : null;
}

function fichiersTouches(base) {
  const mb = git("merge-base", base, "HEAD").trim();
  const suivis = git("diff", "--name-only", mb).split("\n").filter(Boolean);
  const nouveaux = git("ls-files", "--others", "--exclude-standard").split("\n").filter(Boolean);
  return { mb, fichiers: [...new Set([...suivis, ...nouveaux])], nouveaux: new Set(nouveaux) };
}

const estSimNonTest = (p) =>
  p.startsWith(SIM) && /\.(h|hpp|cpp|inl)$/.test(p) && !p.includes("/Private/Tests/");

/** Lignes ajoutees par la mission dans un fichier : [{ ligne, texte }]. */
function lignesAjoutees(mb, p, estNouveau) {
  if (estNouveau || !existsSync(join(ROOT, p))) {
    if (!existsSync(join(ROOT, p))) return [];
    return lire(join(ROOT, p)).split("\n").map((texte, i) => ({ ligne: i + 1, texte }));
  }
  const out = [];
  let n = 0;
  for (const l of git("diff", "-U0", mb, "--", p).split("\n")) {
    const h = /^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@/.exec(l);
    if (h) { n = Number(h[1]); continue; }
    if (l.startsWith("+++")) continue;
    if (l.startsWith("+")) out.push({ ligne: n++, texte: l.slice(1) });
  }
  return out;
}

function registreA(ref) {
  try { return lireRegistre(git("show", `${ref}:${REGISTRE}`).toString()); } catch { return []; }
}

function portail(o, fiches) {
  const fail = [];
  const warn = [];
  const { mb, fichiers, nouveaux } = fichiersTouches(o.base);
  const sim = fichiers.filter(estSimNonTest);
  const registreTouche = fichiers.includes(REGISTRE);
  if (!sim.length && !registreTouche) return { concerne: false, fail, warn };

  const cheminFiche = o.handoff ? (isAbsolute(o.handoff) ? o.handoff : join(ROOT, o.handoff)) : null;
  if (!cheminFiche || !existsSync(cheminFiche)) {
    fail.push(`fiche de passation introuvable (${o.handoff || "-handoff absent"}) : la mission touche ${SIM}`);
    return { concerne: true, fail, warn, sim };
  }
  const section = sectionEcarts(lire(cheminFiche));
  const nomFiche = relative(ROOT, cheminFiche).split(sep).join("/");
  if (section === null) {
    fail.push(`${nomFiche} : section \`## ECARTS\` manquante — la mission touche ${sim.length} fichier(s) de ${SIM}`);
    sim.slice(0, 6).forEach((p) => fail.push(`    ${p}`));
    return { concerne: true, fail, warn, sim };
  }
  const citeAucun = /^\s*AUCUN\b/m.test(section);
  // `n° 4`, et les plages `n° 1 à 18` / `n° 1-18`.
  const cites = new Set();
  for (const m of section.matchAll(/n\s*°\s*(\d+)(?:\s*(?:à|a|-)\s*(\d+))?/g)) {
    const de = Number(m[1]);
    const a = m[2] ? Number(m[2]) : de;
    for (let n = de; n <= Math.min(a, de + 200); n++) cites.add(n);
  }
  if (/<[^>]+>/.test(section)) fail.push(`${nomFiche} : section ECARTS encore au gabarit (« <...> »)`);
  if (!citeAucun && !cites.size) fail.push(`${nomFiche} : section ECARTS sans \`n° N\` ni \`AUCUN — <pourquoi>\``);
  if (citeAucun && cites.size) warn.push(`${nomFiche} : section ECARTS dit AUCUN et cite pourtant ${[...cites].map((n) => `n° ${n}`).join(", ")}`);
  if (citeAucun && section.replace(/AUCUN/, "").replace(/[\s—:-]/g, "").length < 20) {
    fail.push(`${nomFiche} : \`AUCUN\` sans justification (quels tests de parite prouvent la fidelite ?)`);
  }
  const parN = new Map(fiches.map((f) => [f.n, f]));
  for (const n of cites) if (!parN.has(n)) fail.push(`${nomFiche} : cite n° ${n}, absent de ${REGISTRE}`);

  // Fiches nouvelles ou dont le statut / destin change : la passation doit les nommer.
  const avant = new Map(registreA(mb).map((f) => [f.n, f]));
  for (const f of fiches) {
    const a = avant.get(f.n);
    const change = !a ? "nouvelle" :
      a.champs.statut !== f.champs.statut ? `statut ${a.champs.statut} -> ${f.champs.statut}` :
      a.champs.destin !== f.champs.destin ? `destin ${a.champs.destin} -> ${f.champs.destin}` : null;
    if (change && !cites.has(f.n)) fail.push(`${nomFiche} : la fiche n° ${f.n} est ${change} dans ce lot mais la section ECARTS ne la nomme pas`);
    if (change && f.champs.destin === "ASSUME" && (!a || a.champs.destin !== "ASSUME")) {
      warn.push(`n° ${f.n} passe a ASSUME : seul Alexandre decide ; decision citee : ${f.champs.decision || "(aucune)"}`);
    }
  }

  // Le code ajoute par la mission.
  for (const p of sim) {
    for (const { ligne, texte } of lignesAjoutees(mb, p, nouveaux.has(p))) {
      const code = texte.replace(/\/\/.*$/, "");
      if (TIRAGE_INTERDIT.test(code)) fail.push(`${p}:${ligne} : tirage hors \`sim.rng\` (P3_PLAN.md §7) : ${texte.trim()}`);
      else if (NOUVEAU_FLUX.test(code)) warn.push(`${p}:${ligne} : nouveau FAnastasisRng — un flux a cote de \`sim.rng\` est un ecart (n° 16) : ${texte.trim()}`);
      if (AVEU.test(texte) && !texte.match(MARQUE)) {
        if (citeAucun) fail.push(`${p}:${ligne} : le code dit « non porte » mais la fiche dit AUCUN : ${texte.trim()}`);
        else warn.push(`${p}:${ligne} : « non porte » sans marque \`ecart n°N\` : ${texte.trim()}`);
      }
    }
  }
  return { concerne: true, fail, warn, sim };
}

// --- requetes ---------------------------------------------------------------

function parSection(fiches, s) {
  const ouverts = fiches.filter((f) => f.champs.statut === "OUVERT" && sectionsDe(f).includes(s));
  console.log(`ECARTS OUVERTS QUI TOUCHENT \`${s}\` : ${ouverts.length}`);
  console.log("Des suspects connus, a ecarter avant de chercher un bug de portage.\n");
  for (const f of ouverts) {
    console.log(`  n° ${String(f.n).padStart(2)}  ${f.champs.classe.padEnd(9)} ${f.champs.destin.padEnd(10)} ${f.titre}`);
    if (f.champs.masques) console.log(`        masques : ${f.champs.masques}`);
    if (f.champs.fermeture) console.log(`        fermeture : ${f.champs.fermeture}`);
  }
}

function bilan(fiches) {
  const ouverts = fiches.filter((f) => f.champs.statut === "OUVERT");
  const compte = (k, v) => ouverts.filter((f) => f.champs[k] === v).map((f) => f.n);
  console.log(`ECARTS : ${fiches.length} fiches, ${ouverts.length} ouvertes, ${fiches.length - ouverts.length} fermees\n`);
  console.log("Par destin (ouverts) :");
  for (const d of DESTINS) { const l = compte("destin", d); console.log(`  ${d.padEnd(10)} ${String(l.length).padStart(3)}   ${l.map((n) => `n°${n}`).join(" ")}`); }
  console.log("\nPar classe (ouverts) :");
  for (const c of CLASSES) { const l = compte("classe", c); console.log(`  ${c.padEnd(10)} ${String(l.length).padStart(3)}   ${l.map((n) => `n°${n}`).join(" ")}`); }
  const tr = compte("destin", "A_TRANCHER").length;
  const af = compte("destin", "A_FERMER").length;
  console.log(`\nSTABILISATION::${tr + af === 0 ? "ATTEINTE" : "EN_COURS"} A_TRANCHER=${tr} A_FERMER=${af}`);
  console.log("La stabilisation est finie quand il ne reste que des ecarts FERME ou ASSUME.");
}

// --- main -------------------------------------------------------------------

async function main() {
  const o = args(process.argv.slice(2));
  if (o.aide) {
    console.log(readFileSync(fileURLToPath(import.meta.url), "utf8").split("\n").filter((l) => l.startsWith("//")).slice(0, 28).join("\n"));
    return 0;
  }
  if (!existsSync(join(ROOT, REGISTRE))) { console.log(`ECARTS::FAIL registre absent : ${REGISTRE}`); return 1; }
  const fiches = lireRegistre(lire(join(ROOT, REGISTRE)));
  if (o.section) { parSection(fiches, o.section); return 0; }
  if (o.bilan) { bilan(fiches); return 0; }

  const reg = validerRegistre(fiches, await masquesConnus());
  const mar = verifierMarques(fiches, marquesDuCode());
  let fail = [...reg.fail, ...mar.fail];
  let warn = [...reg.warn, ...mar.warn];
  let concerne = true;

  if (o.base) {
    const p = portail(o, fiches);
    concerne = p.concerne;
    if (!concerne) {
      // Le registre est partage : un defaut deja sur main ne bloque pas une mission qui n'y touche pas.
      warn = [...fail.map((x) => `(registre) ${x}`), ...warn];
      fail = [];
    }
    fail.push(...p.fail);
    warn.push(...p.warn);
    if (p.sim?.length) console.log(`Mission : ${p.sim.length} fichier(s) C++ de ${SIM} touches`);
  }

  for (const x of fail) console.log(`  FAIL ${x}`);
  for (const x of warn) console.log(`  WARN ${x}`);
  const ouverts = fiches.filter((f) => f.champs.statut === "OUVERT").length;
  const verdict = fail.length ? "FAIL" : concerne ? "PASS" : "NON_CONCERNE";
  console.log(`ECARTS::${verdict} fiches=${fiches.length} ouvertes=${ouverts} fail=${fail.length} warn=${warn.length}`);
  if (fail.length) console.log("Protocole : docs/migration/PROTOCOLE_ECARTS.md");
  return fail.length ? 1 : 0;
}

if (import.meta.url === pathToFileURL(process.argv[1] || "").href) {
  main().then((c) => process.exit(c), (e) => { console.log(`ECARTS::FAIL ${e.message}`); process.exit(1); });
}
