// STATE_FIELDS — chaque champ d'etat de la simulation est lu par l'empreinte d'etat, ou classe.
//
// `FVillage::Digest()` est la projection de parite JS, au perimetre fige. Les tests C++ qui veulent
// savoir « est-ce le meme etat ? » ou « quelque chose a-t-il ecrit ? » lisent `StateDigest()`
// (STATE_ORACLE_001). Cet oracle ne vaut que s'il suit les structures : IRON_CRUSADE_001 a montre
// qu'une ecriture invisible dans `Speed` ou `sim.rng` change le futur en ~8 s simulees. Ce
// controleur refuse donc un champ d'une structure du registre qui n'est pas :
//   - lu dans le parcours d'etat de sa structure (`VisitState` / `ArchiveState`, qui hache ET sauve : `<acces><Champ>` dans le corps de la fonction declaree) ;
//   - ou classe hors etat dans le registre, avec sa raison : `cache:`, `derive:`, `pointeur:`,
//     `observation:` ou `lacune:` (etat reel pas encore lu : visible, compte, a fermer).
// Il refuse aussi une entree perimee (champ disparu) ou contradictoire (classee ET lue).
//
//   node tools/migration/check-state-fields.mjs                controle strict de l'arbre
//   node tools/migration/check-state-fields.mjs -base main     portail de mission (appele par `finish`) :
//       seuls les problemes que la branche AJOUTE echouent ; ceux deja presents a la base de fusion
//       (un champ verse par une autre mission) sortent en WARN. Plusieurs agents versent en parallele :
//       une mission ne doit pas payer le champ d'une autre.
//   node tools/migration/check-state-fields.mjs -liste         chaque structure : lus / hors etat / lacunes
//
// Registre : tools/migration/state-fields.json.
// Sortie : STATE_FIELDS::PASS / STATE_FIELDS::SKIP (registre absent) / STATE_FIELDS::FAIL ; code 1 si FAIL.
// Aucune dependance : node >= 18 et git.

import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const REGISTRE = "tools/migration/state-fields.json";
const RAISONS = ["cache", "derive", "pointeur", "observation", "lacune"];

const argv = process.argv.slice(2);
const liste = argv.includes("-liste");
const baseRef = argv.includes("-base") ? argv[argv.indexOf("-base") + 1] : null;

if (!existsSync(join(ROOT, REGISTRE))) {
  console.log(`STATE_FIELDS::SKIP registre ${REGISTRE} absent`);
  process.exit(0);
}

// --- lecture d'un arbre (disque, ou un commit) ------------------------------

function git(...args) {
  return execFileSync("git", ["-C", ROOT, ...args], { encoding: "utf8", stdio: ["ignore", "pipe", "ignore"] });
}

/** Contenu d'un fichier : sur le disque (`rev` nul) ou dans un commit ; null s'il n'existe pas. */
function readText(rev, file) {
  if (!rev) return existsSync(join(ROOT, file)) ? readFileSync(join(ROOT, file), "utf8") : null;
  try { return git("show", `${rev}:${file}`); } catch { return null; }
}

// --- lecture C++ ------------------------------------------------------------

/** Retire commentaires, chaines et caracteres litteraux en gardant les longueurs (positions stables). */
function strip(src) {
  let out = "";
  for (let i = 0; i < src.length; ) {
    const c = src[i], d = src[i + 1];
    if (c === "/" && d === "/") { while (i < src.length && src[i] !== "\n") { out += " "; i++; } continue; }
    if (c === "/" && d === "*") {
      out += "  "; i += 2;
      while (i < src.length && !(src[i] === "*" && src[i + 1] === "/")) { out += src[i] === "\n" ? "\n" : " "; i++; }
      out += "  "; i += 2; continue;
    }
    if (c === '"' || c === "'") {
      const q = c; out += " "; i++;
      while (i < src.length && src[i] !== q) { if (src[i] === "\\") { out += " "; i++; } out += src[i] === "\n" ? "\n" : " "; i++; }
      out += " "; i++; continue;
    }
    out += c; i++;
  }
  return out;
}

/** Index de l'accolade fermante qui correspond a celle ouverte en `open`. */
function closing(text, open) {
  let depth = 0;
  for (let i = open; i < text.length; i++) {
    if (text[i] === "{") depth++;
    else if (text[i] === "}") { depth--; if (depth === 0) return i; }
  }
  return -1;
}

/** Retire les arguments de gabarit `<...>` (imbriques) d'une declaration. */
function dropTemplates(s) {
  let out = "", depth = 0;
  for (const c of s) {
    if (c === "<") { depth++; continue; }
    if (c === ">") { if (depth > 0) depth--; continue; }
    if (depth === 0) out += c;
  }
  return out;
}

/** Champs de donnees d'une structure (profondeur 1 de son corps ; fonctions, statiques, types imbriques exclus). */
function fieldsOf(text, file, debut) {
  if (text === null) return { error: `${file} introuvable` };
  const src = strip(text);
  const re = new RegExp(`(?:^|\\n)\\s*(?:struct|class)\\s+(?:ANASTASISSIM_API\\s+)?${debut}\\b[^;{]*\\{`);
  const m = re.exec(src);
  if (!m) return { error: `structure ${debut} introuvable dans ${file}` };
  const open = m.index + m[0].length - 1;
  const end = closing(src, open);
  if (end < 0) return { error: `corps de ${debut} non ferme dans ${file}` };
  // Aplatir les blocs internes (types imbriques, corps de fonctions) : seule la profondeur 1 compte.
  let body = "", depth = 0;
  for (let i = open + 1; i < end; i++) {
    const c = src[i];
    if (c === "{") { depth++; if (depth === 1) body += "{"; continue; }
    if (c === "}") { depth--; if (depth === 0) body += "}"; continue; }
    if (depth === 0) body += c;
  }
  const fields = [];
  for (let stmt of body.split(/[;}]/)) {
    stmt = stmt.replace(/\{/g, " ").replace(/\b(?:public|private|protected)\s*:/g, " ").trim();
    if (!stmt) continue;
    if (/^(?:static|using|typedef|friend|enum|struct|class|template|GENERATED_BODY)\b/.test(stmt)) continue;
    const flat = dropTemplates(stmt);
    const head = flat.split("=")[0];
    // Fonction, constructeur, operateur, ou reste d'une liste de parametres coupee par un `= {}`.
    const unbalanced = (flat.match(/\(/g) || []).length !== (flat.match(/\)/g) || []).length;
    if (/[()]/.test(head) || unbalanced || /\boperator\b/.test(flat)) continue;
    const name = /([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*$/.exec(head.trim());
    if (name && /\s/.test(head.trim())) fields.push(name[1]);
  }
  return { fields };
}

/** Corps de la fonction dont la signature est donnee mot pour mot (espaces libres). */
function bodyOf(text, file, signature) {
  if (text === null) return { error: `${file} introuvable` };
  const src = strip(text);
  const pattern = signature.trim().split(/\s+/).map((t) => t.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")).join("\\s*");
  const m = new RegExp(pattern + "\\s*(?:const\\s*)?\\{").exec(src);
  if (!m) return { error: `fonction « ${signature} » introuvable dans ${file}` };
  const open = m.index + m[0].length - 1;
  const end = closing(src, open);
  return { body: src.slice(open, end + 1) };
}

// --- controle d'un arbre ----------------------------------------------------

/** Les problemes d'un arbre (disque si `rev` nul) ; null si le registre n'y existe pas. */
function check(rev) {
  const reg = readText(rev, REGISTRE);
  if (reg === null) return null;
  const registre = JSON.parse(reg);
  const problems = [];
  const lignes = [];
  let lacunes = 0;
  for (const s of registre.structures) {
    const parsed = fieldsOf(readText(rev, s.entete), s.entete, s.nom);
    if (parsed.error) { problems.push(parsed.error); continue; }
    const reader = bodyOf(readText(rev, s.lecteur.fichier), s.lecteur.fichier, s.lecteur.fonction);
    if (reader.error) { problems.push(`${s.nom} : ${reader.error}`); continue; }
    const acces = s.lecteur.acces ?? "";
    const esc = acces.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    const hors = s.hors_etat ?? {};
    const lus = [], classes = [];
    for (const f of parsed.fields) {
      const read = new RegExp(`${acces ? "" : "(?<![.\\w>])"}${esc}${f}\\b`).test(reader.body);
      const raison = hors[f];
      if (read && raison !== undefined) problems.push(`${s.nom}.${f} : lu par le parcours d'etat ET classe hors etat (« ${raison} ») -- retirer l'un des deux`);
      else if (read) lus.push(f);
      else if (raison !== undefined) {
        const kind = String(raison).split(":")[0].trim();
        if (!RAISONS.includes(kind)) problems.push(`${s.nom}.${f} : raison « ${raison} » -- commencer par ${RAISONS.map((r) => r + ":").join(" ")}`);
        if (kind === "lacune") lacunes++;
        classes.push(`${f} (${kind})`);
      } else {
        problems.push(`${s.nom}.${f} : champ ni lu par « ${s.lecteur.fonction} » (${s.lecteur.fichier}) ni classe dans ${REGISTRE}`);
      }
    }
    for (const f of Object.keys(hors)) {
      if (!parsed.fields.includes(f)) problems.push(`${s.nom}.${f} : classe dans ${REGISTRE} mais absent de ${s.entete} (entree perimee)`);
    }
    lignes.push(`  ${s.nom.padEnd(18)} ${String(parsed.fields.length).padStart(3)} champs, ${String(lus.length).padStart(3)} lus` +
      (classes.length ? `, hors etat : ${classes.join(", ")}` : ""));
  }
  return { problems, lignes, lacunes, structures: registre.structures.length };
}

// --- verdict ----------------------------------------------------------------

const head = check(null);
let inherited = new Set();
if (baseRef) {
  let mb = null;
  try { mb = git("merge-base", baseRef, "HEAD").trim(); } catch { mb = null; }
  if (!mb) { console.log(`STATE_FIELDS::FAIL base de fusion introuvable avec ${baseRef}`); process.exit(1); }
  const base = check(mb);
  if (base) inherited = new Set(base.problems);
}
const fails = head.problems.filter((p) => !inherited.has(p));
const warns = head.problems.filter((p) => inherited.has(p));

if (liste || fails.length) head.lignes.forEach((l) => console.log(l));
for (const w of warns) console.log(`  WARN ${w} (deja sur ${baseRef} : a classer au prochain passage)`);
for (const f of fails) console.log(`  FAIL ${f}`);
if (fails.length) {
  console.log(`STATE_FIELDS::FAIL ${fails.length} probleme(s) -- un champ d'etat se lit dans StateDigest ou se classe (STATE_ORACLE_001)`);
  process.exit(1);
}
console.log(`STATE_FIELDS::PASS structures=${head.structures} lacunes=${head.lacunes}` + (warns.length ? ` warn=${warns.length}` : ""));
