// INVENTAIRE DU SIMULATEUR JS — porter / generer / jeter.
//
// Le portage vers `Source/AnastasisSim` doit savoir ce qu'il a devant lui: sur
// les ~89 000 lignes de `src/sim` + `src/life` + `src/ai` + `src/lang` +
// `src/runtime`, une partie n'est pas du code a traduire. La confondre avec le
// reste, c'est ecrire a la main en C++ des tables de donnees et des filets de
// securite navigateur qui n'ont pas d'equivalent dans Unreal.
//
// Cet outil LIT le depot de reference (jamais il n'y ecrit) et classe chaque
// module. Il est volontairement mecanique: les regles sont ci-dessous, dans
// l'ordre ou elles s'appliquent, et chaque verdict porte sa raison. Une
// classification ecrite a la main serait perimee au prochain commit du depot JS
// et invérifiable; celle-ci se refait.
//
//   node tools/migration/inventory-js-sim.mjs -ref <depot JS> [-out <fichier.md>] [-allow-dirty]
//
// Sans -out, le rapport part sur stdout.
//
// La reference est un COMMIT, pas une copie de travail: `-ref` pointe un
// checkout propre du tag de reference (docs/migration/phase3/REFERENCE_JS.md).
// Si `src/` y porte des modifications non commitees, l'outil refuse —
// l'inventaire decrirait un etat que personne d'autre ne peut reproduire.
// `-allow-dirty` passe outre, et le rapport le dit en tete.
//
// Les modules dont une PARTIE seulement est portee (tranches verticales de
// PORTAGE.md) sont declares dans `ported-functions.mjs`; `js-functions.mjs`
// situe chaque fonction dans son module pour compter les lignes qui restent.

import { readFileSync, readdirSync, statSync, existsSync, mkdirSync, writeFileSync } from "node:fs";
import { join, resolve, dirname, relative, basename } from "node:path";
import { execFileSync } from "node:child_process";
import { fileURLToPath } from "node:url";

import { findDefinitions } from "./js-functions.mjs";
import { PORTAGE_DECLARE, PORTAGE_HORS_COMPTE } from "./ported-functions.mjs";

const BS = String.fromCharCode(92);
const norm = (p) => p.split(BS).join("/");
const ICI = dirname(fileURLToPath(import.meta.url));
const CPP_ROOT = join(ICI, "..", "..", "Source", "AnastasisSim");

// --- Arguments --------------------------------------------------------------

const argv = process.argv.slice(2);
const argOf = (flag, fallback) => {
  const i = argv.indexOf(flag);
  return i >= 0 && argv[i + 1] ? argv[i + 1] : fallback;
};
const REF = norm(argOf("-ref", "C:/dev/Jeux IV Kingdoms"));
const OUT = argOf("-out", null);
const ALLOW_DIRTY = argv.includes("-allow-dirty");
const SRC = join(REF, "src");

if (!existsSync(SRC)) {
  console.error(`Depot de reference introuvable: ${SRC}`);
  console.error("Passer -ref <chemin du depot JS>.");
  process.exit(2);
}

// --- Provenance -------------------------------------------------------------

const git = (...args) => {
  try { return execFileSync("git", ["-C", REF, ...args], { encoding: "utf8", stdio: ["ignore", "pipe", "ignore"] }).trim(); }
  catch { return null; }
};
const refCommit = git("log", "-1", "--format=%H");
const refDate = git("log", "-1", "--format=%cs");
const refTag = git("describe", "--tags", "--exact-match", "HEAD");
const refDirty = git("status", "--porcelain", "--", "src");
const refDirtyCount = refDirty ? refDirty.split(/\r?\n/).length : 0;
if (refDirtyCount && !ALLOW_DIRTY) {
  console.error(`REFUS: ${REF} porte ${refDirtyCount} modification(s) non commitee(s) sous src/.`);
  console.error("L'inventaire decrit un commit. Utiliser un checkout propre du tag de reference");
  console.error("(docs/migration/phase3/REFERENCE_JS.md), ou -allow-dirty en connaissance de cause.");
  process.exit(3);
}

// --- Perimetre --------------------------------------------------------------
// Le noyau simulation. `src/render3d`, `src/render`, `src/ui`, `src/audio`,
// `src/debug` sont hors sujet par decision (AGENTS.md): le rendu se reecrit
// dans Unreal, il ne se porte pas.

const CORE_DIRS = ["src/sim/", "src/life/", "src/ai/", "src/lang/", "src/runtime/"];
const isCore = (p) => CORE_DIRS.some((d) => p.startsWith(d));

// --- Listes explicites ------------------------------------------------------
// Tout ce qui ne se deduit pas d'une mesure est ecrit ici, avec sa raison. Une
// heuristique qui se tromperait en silence sur un de ces fichiers couterait
// bien plus cher que la ligne qu'elle economise.

// Deja porte en C++, en tout ou en partie: `ported-functions.mjs`, recopie des
// tableaux de Source/AnastasisSim/PORTAGE.md.
const DECLARE = new Map(PORTAGE_DECLARE.map((d) => [d.module, d]));

// Filets de securite et amarres navigateur: Unreal a les siens. PORTAGE.md
// pose deja la regle pour cinq d'entre eux — "le choix est a refaire, pas a
// traduire". Les autres sortent de la meme famille.
const JETER_NAVIGATEUR = new Map([
  ["src/runtime/faultShield.js", "filet de securite navigateur"],
  ["src/runtime/flightRecorder.js", "filet de securite navigateur"],
  ["src/runtime/watchdog.js", "filet de securite navigateur"],
  ["src/runtime/crashReport.js", "filet de securite navigateur"],
  ["src/runtime/postMortemUi.js", "filet de securite navigateur"],
  ["src/runtime/bootTelemetry.js", "telemetrie de demarrage navigateur"],
  ["src/runtime/sessionLaunchProfile.js", "profil de lancement navigateur"],
  ["src/runtime/stateRewind.js", "rembobinage outil de dev, a refaire sur le save Unreal"],
  ["src/runtime/gameLoop.js", "requestAnimationFrame -> Tick d'Unreal"],
  ["src/runtime/index.js", "amorcage du runtime navigateur (cable les filets ci-dessus)"],
  ["src/sim/saveStore.js", "localStorage -> SaveGame d'Unreal"],
]);

// Lecture, etiquetage, affichage. Ces modules ne decident rien dans la
// simulation: ils la racontent a une UI qui n'existera pas sous cette forme.
// La couche de presentation Unreal les refera, contre son propre HUD.
const JETER_PRESENTATION = new Map([
  ["src/ai/npcInspector.js", "inspecteur Observatoire F3 (Unreal: AnastasisInspectTools)"],
  ["src/ai/explainGoal.js", "texte d'explication pour l'UI de debug"],
  ["src/ai/goalLabels.js", "libelles d'objectifs pour l'UI"],
  ["src/ai/algorithmic/debug.js", "sondes de debug"],
  ["src/sim/collectivePrioritiesPanel.js", "panneau UI"],
  ["src/sim/observability.js", "compteurs pour l'UI de dev"],
  ["src/life/engineBridge.js", "facade de lecture pour main.js et l'UI"],
  ["src/life/pneuma/PneumaBubbleDirector.js", "politique d'attention des bulles (rendu three.js)"],
]);

// Table de donnees: le contenu doit devenir une table generee, pas du C++
// ecrit a la main. Ajouter une espece ou un batiment ne doit jamais demander
// une recompilation.
const GENERER = new Map([
  ["src/sim/batiments/catalog.js", "catalogue des batiments"],
  ["src/sim/animaux/catalog.js", "catalogue des especes"],
  ["src/sim/metiers/catalog.js", "catalogue des metiers"],
  ["src/life/talkCatalog.js", "catalogue de repliques"],
]);

// Logique reelle POSEE SUR une table: la table s'extrait, le selecteur se
// porte. Les separer evite de recompiler pour ajouter une ligne de contenu.
const SCINDER = new Map([
  ["src/life/names.js", "pools onomastiques + selection"],
  ["src/life/sealTalk.js", "formules + declenchement"],
  ["src/life/landTalk.js", "formules + declenchement"],
  ["src/life/talkCanon1204.js", "canon 1204 + selection"],
  ["src/life/collectiveTalk.js", "formules + agregation"],
  ["src/life/historicalRumors.js", "corpus de rumeurs + propagation"],
  ["src/life/colonySeals.js", "sceaux + conditions"],
  ["src/sim/romanChronicle.js", "formules de chronique + declenchement"],
  ["src/life/followVignette.js", "vignettes + composition"],
  ["src/life/foundingCeremony.js", "ceremonie + deroule"],
  ["src/ai/episodes.js", "gabarits d'episodes + machine a etats"],
]);

// Chantiers — de quoi decouper une vague entre plusieurs mains. L'ordre
// compte, la premiere expression qui match gagne. Un fichier qui n'entre dans
// aucun chantier tombe dans "divers", et c'est un signal: soit le groupe
// manque, soit le module est mal range dans le depot JS.
const CHANTIERS = [
  [/^src[/]sim[/]transport[/]/, "transport et logistique"],
  [/^src[/]sim[/]animaux[/]/, "regne animal"],
  [/^src[/]sim[/]urban[/]/, "urbanisme"],
  [/^src[/]sim[/]metiers[/]/, "economie et travail"],
  [/^src[/]sim[/](economy|economyLoopMetrics|jobMarket|craft\w+|colonyStockReport|resourceRelay|haulLoad|forestSustain|constructionPieces|constructionPipeline|fieldWorkPosts)\.js$/, "economie et travail"],
  [/^src[/]sim[/](colonySite|settlementSite|watchPosts|landWaterPresets|transportProjects|villageCrown|founderCharter|landExtent)\.js$/, "urbanisme"],
  [/^src[/]sim[/](clioscope\w*|romanChronicle|villageChronicle|sagas|chronicleKindBias|valmireSignatures|eraClimateBridge|growthChapter)\.js$/, "chronique et memoire collective"],
  [/^src[/]sim[/](collective\w+|social\w+|colonizationDoctrine|oecumene|cohesionClimate)\.js$/, "societe et institutions"],
  [/^src[/]sim[/](simulation|npc|life|decisionProvider|eventPrimitives|content|weather|lifestyle|playerGenesis)\.js$/, "noyau de boucle"],
  [/^src[/]lang[/]/, "langue"],
  [/^src[/]ai[/]/, "cognition"],
  [/^src[/]life[/](talk\w*|\w*Talk|speechActs|\w*Narrative|\w*[Vv]ignette|historicalRumors|momentTalk|followMotif|witnessMemory|collectiveTalk)\.js$/, "parole et narration"],
  [/^src[/]life[/](orthodox\w+|liturgical\w+|foundingCeremony|kosmos\w+|romanCouncils|colonySeals|culturalMemory|culture\w*)\.js$/, "rites et culture"],
  [/^src[/]life[/]/, "personne et famille"],
];

// Vagues de portage — l'ordre de PORTAGE.md, par dependance reelle.
const VAGUE_FICHIER = new Map([
  ["src/sim/navGrid.js", 2], ["src/sim/pathfinding.js", 2], ["src/sim/navService.js", 2],
  ["src/sim/crowdNav.js", 2], ["src/sim/destination.js", 2], ["src/sim/landRoads.js", 2],
  ["src/sim/trafficDecay.js", 2], ["src/sim/percolation.js", 2], ["src/sim/landExtent.js", 2],
  ["src/sim/simulationBudget.js", 3], ["src/sim/logicalLod.js", 3],
  ["src/sim/save.js", 4], ["src/sim/worldChange.js", 4], ["src/sim/pristineWorld.js", 4],
  // couches 0 et 1, portees avant les vagues
  ["src/sim/rng.js", 0], ["src/sim/util.js", 0], ["src/sim/spatialGrid.js", 0], ["src/runtime/simClock.js", 0],
  ["src/sim/world.js", 1], ["src/sim/worldArchetypes.js", 1], ["src/sim/hydrology.js", 1], ["src/sim/fieldCrops.js", 1],
]);
const VAGUE_DOSSIER = [
  ["src/life/", 6], ["src/ai/", 6], ["src/lang/", 6],
  ["src/sim/", 5], ["src/runtime/", 5],
];
const LIBELLE_VAGUE = {
  0: "0 — socle deterministe",
  1: "1 — generation du monde",
  2: "2 — navigation",
  3: "3 — budget et LOD logique",
  4: "4 — etat du monde et sauvegarde",
  5: "5 — boucle de simulation",
  6: "6 — vie, IA, langue",
};

// --- Lecture du depot -------------------------------------------------------

function walk(dir, out = []) {
  for (const entry of readdirSync(dir)) {
    const full = join(dir, entry);
    if (statSync(full).isDirectory()) {
      if (entry === "node_modules" || entry === "vendor") continue;
      walk(full, out);
    } else if (entry.endsWith(".js") || entry.endsWith(".mjs")) {
      out.push(norm(full));
    }
  }
  return out;
}

const files = walk(SRC);
const known = new Set(files);

function resolveSpec(fromFile, spec) {
  if (!spec.startsWith(".")) return null; // paquet externe
  const base = norm(resolve(dirname(fromFile), spec));
  for (const c of [base, base + ".js", base + ".mjs", base + "/index.js"]) {
    if (known.has(norm(c))) return norm(c);
  }
  return null;
}

const IMPORT_RE = /(?:^|\n)\s*(?:import|export)[\s\S]*?from\s*["']([^"']+)["']/g;
const BARE_RE = /(?:^|\n)\s*import\s*["']([^"']+)["']/g;
const DYN_RE = /import\(\s*["']([^"']+)["']\s*\)/g;

const info = new Map();
for (const f of files) {
  const text = readFileSync(f, "utf8");
  const lines = text.split(/\r?\n/);

  const deps = new Set();
  for (const re of [IMPORT_RE, BARE_RE, DYN_RE]) {
    re.lastIndex = 0;
    let m;
    while ((m = re.exec(text))) {
      const r = resolveSpec(f, m[1]);
      if (r) deps.add(r);
    }
  }

  // Comptage par nature de ligne. `stringData` isole les lignes qui ne sont
  // qu'un litteral de texte — c'est la mesure qui separe une table de contenu
  // d'un module de logique. `kinds` garde la nature de chaque ligne: c'est
  // elle qui compte les lignes de code d'une fonction portee.
  let blank = 0, comment = 0, stringData = 0, code = 0, inBlock = false;
  const kinds = new Array(lines.length);
  lines.forEach((raw, n) => {
    const l = raw.trim();
    if (inBlock) { comment++; kinds[n] = "c"; if (l.includes("*/")) inBlock = false; return; }
    if (l === "") { blank++; kinds[n] = "b"; return; }
    if (l.startsWith("//")) { comment++; kinds[n] = "c"; return; }
    if (l.startsWith("/*")) { comment++; kinds[n] = "c"; if (!l.includes("*/")) inBlock = true; return; }
    if (/^(?:[\w$."'[\]]+\s*:\s*)?(["'`])(?:(?!\1).){12,}\1\s*,?\s*$/.test(l)) { stringData++; kinds[n] = "s"; return; }
    code++; kinds[n] = "x";
  });

  const ctrl = (text.match(/\b(if|for|while|switch)\s*\(/g) || []).length;
  const exportsFn = (text.match(/^export\s+(?:async\s+)?function\s+\w+/gm) || []).length;
  const exportsConst = (text.match(/^export\s+const\s+\w+/gm) || []).length;
  const browser = (text.match(/\b(document|window|localStorage|requestAnimationFrame|HTMLElement|navigator)\s*[.(]|THREE\.|createElement/g) || []).length;
  const reexportOnly = (text.match(/^export\s+[\s\S]*?from\s*["']/gm) || []).length;

  info.set(f, {
    file: f, path: norm(relative(REF, f)), text, kinds,
    lines: lines.length, blank, comment, stringData, code,
    ctrl, exportsFn, exportsConst, browser, reexportOnly,
    deps: [...deps], importers: [],
  });
}
for (const [, i] of info) for (const d of i.deps) info.get(d)?.importers.push(i.path);

// Atteignabilite depuis le point d'entree du jeu: un module que personne
// n'atteint n'a pas besoin d'etre porte avant qu'on ait decide de le brancher.
const reachable = new Set();
{
  const stack = [norm(join(SRC, "main.js"))].filter((f) => known.has(f));
  while (stack.length) {
    const f = stack.pop();
    if (reachable.has(f)) continue;
    reachable.add(f);
    for (const d of info.get(f).deps) if (!reachable.has(d)) stack.push(d);
  }
}

// --- Ce que le C++ nomme ----------------------------------------------------
// Pour un module que PORTAGE.md cite sans liste de fonctions, on retient les
// fonctions que le C++ nomme (nom JS ou PascalCase, hors tests) sur une ligne
// qui ne dit pas qu'elles ne sont PAS portees. Les commentaires du portage
// citent souvent la reference pour dire ce qu'elle fait et que le C++ ne fait
// pas: ces lignes-la ne valent pas preuve.

const NON_PORTE_RE = /non port|pas port|n'est pas port|ne sont pas port|non suivi|observation seule/i;
const cppNames = new Set();
const cppFunctions = new Set();
{
  const visit = (d) => {
    if (!existsSync(d)) return;
    for (const e of readdirSync(d)) {
      const f = join(d, e);
      if (statSync(f).isDirectory()) { if (e !== "Tests") visit(f); continue; }
      if (!/\.(h|cpp)$/.test(e)) continue;
      for (const line of readFileSync(f, "utf8").split(/\r?\n/)) {
        if (NON_PORTE_RE.test(line)) continue;
        for (const w of line.match(/[A-Za-z_$][\w$]*/g) || []) cppNames.add(w);
        for (const m of line.matchAll(/\b([A-Z]\w*)\s*\(/g)) cppFunctions.add(m[1]);
      }
    }
  };
  visit(CPP_ROOT);
}
const pascal = (n) => n.charAt(0).toUpperCase() + n.slice(1);
// Deux exigences. Module que PORTAGE.md cite EN ENTIER ou sans liste: il suffit
// que le C++ nomme la fonction, la revendication est deja la. Module dont
// PORTAGE.md liste les fonctions: une fonction hors liste ne compte portee que
// si le C++ a une fonction de son nom (PascalCase suivi d'une parenthese) — un
// commentaire qui la cite ne suffit pas, il dit souvent ce que le C++ ne fait pas.
const cppNomme = (nom, decl) => {
  const alias = decl.alias?.[nom];
  if (alias && (cppNames.has(alias) || cppFunctions.has(alias))) return true;
  const indulgent = decl.entier === true || !decl.fonctions;
  return indulgent
    ? cppNames.has(nom) || cppNames.has(pascal(nom))
    : cppFunctions.has(pascal(nom));
};

// --- Portage partiel --------------------------------------------------------
// Pour un module declare: quelles fonctions sont portees, reduites, hors
// perimetre, et combien de lignes de code restent. Une ligne compte une fois,
// meme si elle est dans une fonction imbriquee.

function bilanPortage(i, decl) {
  const { defs } = findDefinitions(i.text);
  // Definitions de premier rang: les fonctions imbriquees suivent leur hote.
  const tops = defs.filter((d) => !defs.some((o) => o !== d && o.start <= d.start && o.end >= d.end && (o.start < d.start || o.end > d.end)));
  const noms = new Set(tops.map((d) => d.name));
  const explicites = new Set(decl.fonctions || []);
  const reduites = new Set(decl.reduites || []);
  const hors = new Map(Object.entries(decl.hors || {}));
  const citation = decl.citation !== false;

  const statut = new Map();
  for (const d of tops) {
    if (statut.has(d.name)) continue;
    if (hors.has(d.name)) statut.set(d.name, "hors");
    else if (reduites.has(d.name)) statut.set(d.name, "reduite");
    else if (explicites.has(d.name)) statut.set(d.name, "portee");
    else if (citation && cppNomme(d.name, decl)) statut.set(d.name, "portee");
    else statut.set(d.name, "reste");
  }
  // Nomme par PORTAGE.md et absent du module: le dire, ne pas l'inventer.
  const introuvables = [...explicites, ...reduites, ...hors.keys()].filter((n) => !noms.has(n));

  const marque = new Array(i.kinds.length).fill(null);
  const rang = { portee: 3, hors: 2, reduite: 1, reste: 0 };
  for (const d of tops) {
    const s = statut.get(d.name);
    for (let l = d.startLine; l <= d.endLine; l += 1) {
      if (marque[l] === null || rang[s] > rang[marque[l]]) marque[l] = s;
    }
  }
  let codePorte = 0, codeHors = 0, codeReduit = 0, codeResteFn = 0, codeHorsFn = 0;
  i.kinds.forEach((k, l) => {
    if (k !== "x") return;
    const s = marque[l];
    if (s === "portee") codePorte++;
    else if (s === "hors") codeHors++;
    else if (s === "reduite") codeReduit++;
    else if (s === "reste") codeResteFn++;
    else codeHorsFn++;
  });
  const fonctions = (s) => tops.filter((d, k) => statut.get(d.name) === s && tops.findIndex((o) => o.name === d.name) === k).map((d) => d.name);
  const portees = fonctions("portee");
  const reste = fonctions("reste");
  // Code hors fonction (imports, constantes, tables): porte avec le module s'il
  // est complet; sinon reparti au prorata du code de fonctions qui reste — une
  // table sert les fonctions qui la lisent, portees ou non.
  // Complet: aucune fonction ne reste, aucune n'est reduite. Que PORTAGE.md ait
  // cite le module en entier ou fonction par fonction ne change rien: c'est le
  // module qu'on juge, pas la facon dont la ligne a ete ecrite.
  const complet = reste.length === 0 && fonctions("reduite").length === 0;
  const fnReste = codeReduit + codeResteFn;
  const horsFnReste = complet ? 0 : Math.round(codeHorsFn * (fnReste / Math.max(1, fnReste + codePorte)));
  return {
    complet,
    total: new Set(tops.map((d) => d.name)).size,
    portees, reduitesListe: fonctions("reduite"), horsListe: fonctions("hors"), reste, introuvables,
    codePorte: codePorte + (codeHorsFn - horsFnReste), codeHors, codeReduit, codeResteFn, codeHorsFn, horsFnReste,
    codeReste: fnReste + horsFnReste,
    resteGros: tops.filter((d) => statut.get(d.name) === "reste")
      .map((d) => ({ name: d.name, code: i.kinds.slice(d.startLine, d.endLine + 1).filter((k) => k === "x").length }))
      .sort((a, b) => b.code - a.code),
  };
}

// --- Classification ---------------------------------------------------------
// Premiere regle qui s'applique gagne. L'ordre compte: un baril mort reste un
// baril, un catalogue atteint reste un catalogue.

function classify(i) {
  const p = i.path;
  if (DECLARE.has(p)) {
    const decl = DECLARE.get(p);
    const bilan = bilanPortage(i, decl);
    return bilan.complet
      ? { verdict: "PORTE", raison: decl.cpp, bilan, decl }
      : { verdict: "PARTIEL", raison: decl.cpp, bilan, decl };
  }
  if (JETER_NAVIGATEUR.has(p)) return { verdict: "JETER", raison: JETER_NAVIGATEUR.get(p) };
  if (JETER_PRESENTATION.has(p)) return { verdict: "JETER", raison: JETER_PRESENTATION.get(p) };
  if (basename(p) === "index.js" && i.reexportOnly > 0 && i.ctrl === 0) {
    return { verdict: "JETER", raison: "baril de re-export, sans equivalent C++" };
  }
  if (!reachable.has(i.file)) {
    return { verdict: "JETER", raison: "non atteint depuis src/main.js — a brancher ou a enterrer, pas a porter" };
  }
  if (GENERER.has(p)) return { verdict: "GENERER", raison: GENERER.get(p) };
  // Les listes explicites passent AVANT le seuil: un module de `SCINDER` est
  // riche en litteraux par construction, mais il decide quand meme quelque
  // chose. Le classer "table de contenu" jetterait sa logique avec l'eau du
  // bain.
  if (SCINDER.has(p)) return { verdict: "PORTER", raison: SCINDER.get(p), scinder: true };
  if (i.dataShare >= 0.4) return { verdict: "GENERER", raison: `table de contenu (${Math.round(i.dataShare * 100)}% de litteraux)` };
  if (i.dataShare >= 0.15) return { verdict: "PORTER", raison: `logique posee sur une table (${Math.round(i.dataShare * 100)}% de litteraux)`, scinder: true };
  return { verdict: "PORTER", raison: "" };
}

function vague(i) {
  if (VAGUE_FICHIER.has(i.path)) return VAGUE_FICHIER.get(i.path);
  for (const [d, v] of VAGUE_DOSSIER) if (i.path.startsWith(d)) return v;
  return 5;
}

function chantier(i, v) {
  if (v === 0) return "socle";
  if (v === 1) return "generation du monde";
  if (v === 2) return "navigation";
  if (v === 3) return "budget et LOD";
  if (v === 4) return "etat et sauvegarde";
  for (const [re, nom] of CHANTIERS) if (re.test(i.path)) return nom;
  return "divers";
}

const rows = [];
for (const f of files) {
  const i = info.get(f);
  if (!isCore(i.path)) continue;
  i.dataShare = i.stringData / Math.max(1, i.stringData + i.code);
  const c = classify(i);
  const v = vague(i);
  // `aPorter`: lignes de code qui restent a porter — tout le module pour
  // PORTER, le reste du bilan pour PARTIEL, rien sinon.
  const aPorter = c.verdict === "PORTER" ? i.code : c.verdict === "PARTIEL" ? c.bilan.codeReste : 0;
  rows.push({ ...i, text: undefined, kinds: undefined, ...c, vague: v, chantier: chantier(i, v), aPorter });
}

const declaresAbsents = PORTAGE_DECLARE.filter((d) => !rows.some((r) => r.path === d.module)).map((d) => d.module);

// Controle du scanner: une definition qui commence en colonne 0 est de premier
// rang. Si le scanner la voit DANS une autre, il a mal lu une chaine, un
// gabarit ou une regex au-dessus, et les extents de ce fichier sont faux.
const avales = [];
for (const r of rows) {
  const text = readFileSync(r.file, "utf8");
  const { defs } = findDefinitions(text);
  for (const d of defs) {
    if (d.start !== 0 && text[d.start - 1] !== "\n") continue;
    if (defs.some((o) => o !== d && o.start < d.start && o.end >= d.end)) avales.push(`${r.path.replace(/^src[/]/, "")}:${d.startLine + 1} ${d.name}`);
  }
}

// --- Rapport ----------------------------------------------------------------

const sum = (list, key) => list.reduce((a, x) => a + x[key], 0);
const sumB = (list, key) => list.reduce((a, x) => a + (x.bilan?.[key] ?? 0), 0);
const by = (v) => rows.filter((r) => r.verdict === v);
const md = [];
const out = (s = "") => md.push(s);
const court = (p) => p.replace(/^src[/]/, "");

out("# Inventaire du simulateur JS — porter / generer / jeter");
out();
out("**Genere. Ne pas editer a la main.**");
out();
out("```bash");
out("# -ref : checkout propre du tag de reference, voir docs/migration/phase3/REFERENCE_JS.md");
out("node tools/migration/inventory-js-sim.mjs -ref <depot JS> -out docs/migration/phase2/P2_INVENTAIRE_JS.md");
out("```");
out();
out(`Reference : ${refTag ? `tag \`${refTag}\` — ` : ""}commit \`${refCommit ? refCommit.slice(0, 7) : "inconnu"}\`${refDate ? ` (${refDate})` : ""}  `);
if (refDirtyCount) {
  out(`**COPIE DE TRAVAIL MODIFIEE** : ${refDirtyCount} fichier(s) non commite(s) sous \`src/\` (\`-allow-dirty\`). Cet inventaire ne decrit pas le commit ci-dessus.  `);
}
out(`Perimetre : ${CORE_DIRS.map((d) => "`" + d + "`").join(", ")} — le rendu, l'UI, l'audio et le debug sont hors sujet par decision (AGENTS.md).`);
out();
out("Le portage ne se mesure pas en lignes de JS. Une table de contenu devient une table de");
out("donnees, pas du C++ ecrit a la main ; un filet de securite navigateur se re-decide dans");
out("Unreal, il ne se traduit pas. Ce document dit, fichier par fichier, dans quel seau il tombe.");
out();

out("## Ce qu'il y a devant");
out();
out("| | fichiers | lignes | dont code | dont code a porter |");
out("| --- | ---: | ---: | ---: | ---: |");
for (const v of ["PORTER", "PARTIEL", "GENERER", "JETER", "PORTE"]) {
  const s = by(v);
  const nom = { PORTER: "**A porter**", PARTIEL: "**Partiellement porte**", GENERER: "**A generer** (donnees)", JETER: "**A jeter**", PORTE: "Deja porte" }[v];
  out(`| ${nom} | ${s.length} | ${sum(s, "lines")} | ${sum(s, "code")} | ${sum(s, "aPorter")} |`);
}
out(`| **Total** | ${rows.length} | ${sum(rows, "lines")} | ${sum(rows, "code")} | ${sum(rows, "aPorter")} |`);
out();
const aPorter = by("PORTER");
const partiels = by("PARTIEL");
const scinder = aPorter.filter((r) => r.scinder);
out(`Sur les ${sum(aPorter, "lines")} lignes des modules a porter, ${sum(aPorter, "comment")} sont du commentaire et`);
out(`${sum(aPorter, "blank")} des lignes vides : **${sum(aPorter, "code")} lignes de code**. Les ${partiels.length} modules`);
out(`partiellement portes ajoutent **${sumB(partiels, "codeReste")} lignes de code** qui restent (sur ${sum(partiels, "code")} ;`);
out(`${sumB(partiels, "codePorte")} portees, ${sumB(partiels, "codeHors")} hors perimetre). Total a porter : **${sum(rows, "aPorter")} lignes de code**.`);
out(`${scinder.length} modules a porter melangent logique et table de contenu : la table s'extrait, le selecteur se porte.`);
out();

out("## Reste a porter, par vague");
out();
out("L'ordre est celui de `Source/AnastasisSim/PORTAGE.md` — il suit les dependances reelles,");
out("pas l'interet du gameplay. Un module partiellement porte compte pour ce qui lui reste.");
out();
const restants = rows.filter((r) => r.aPorter > 0);
out("| Vague | fichiers | dont partiels | lignes de code a porter |");
out("| --- | ---: | ---: | ---: |");
for (const v of [0, 1, 2, 3, 4, 5, 6]) {
  const s = restants.filter((r) => r.vague === v);
  if (!s.length) continue;
  out(`| ${LIBELLE_VAGUE[v]} | ${s.length} | ${s.filter((r) => r.verdict === "PARTIEL").length} | ${sum(s, "aPorter")} |`);
}
out();
out(`Les vagues 5 et 6 ne sont pas des vagues, ce sont des marecages : ${restants.filter((r) => r.vague >= 5).length} modules a`);
out("elles deux. Elles se decoupent en chantiers, et c'est a ce grain qu'un module se confie.");
out();
out("| Vague | Chantier | fichiers | lignes de code a porter | plus gros reste |");
out("| --- | --- | ---: | ---: | --- |");
{
  const groupes = new Map();
  for (const r of restants) {
    const k = `${r.vague}|${r.chantier}`;
    if (!groupes.has(k)) groupes.set(k, []);
    groupes.get(k).push(r);
  }
  const tri = [...groupes.entries()].sort((a, b) => {
    const [va] = a[0].split("|"), [vb] = b[0].split("|");
    return Number(va) - Number(vb) || sum(b[1], "aPorter") - sum(a[1], "aPorter");
  });
  for (const [k, s] of tri) {
    const [v, nom] = k.split("|");
    const gros = [...s].sort((a, b) => b.aPorter - a.aPorter)[0];
    out(`| ${v} | ${nom} | ${s.length} | ${sum(s, "aPorter")} | \`${court(gros.path)}\` (${gros.aPorter}${gros.verdict === "PARTIEL" ? ", partiel" : ""}) |`);
  }
}
out();

out("## Partiellement porte");
out();
out("Modules dont PORTAGE.md declare une partie portee (`tools/migration/ported-functions.mjs`).");
out("`code` = lignes de code du module ; `porte` = lignes des fonctions portees ; `hors` = fonctions");
out("ecartees du portage par PORTAGE.md (observation, three.js) ; `reste` = fonctions non portees et fonctions");
out("**reduites** (une branche portee sur plusieurs). Le code hors fonction (imports, constantes, tables) est");
out("reparti entre `porte` et `reste` au prorata du code de fonctions.");
out();
out("| Module | C++ | fonctions portees | reduites | code | porte | hors | reste | source PORTAGE.md |");
out("| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |");
for (const r of [...partiels].sort((a, b) => b.bilan.codeReste - a.bilan.codeReste)) {
  const b = r.bilan;
  out(`| \`${court(r.path)}\` | ${r.decl.cpp} | ${b.portees.length} / ${b.total} | ${b.reduitesListe.length} | ${r.code} | ${b.codePorte} | ${b.codeHors} | **${b.codeReste}** | ${r.decl.source} |`);
}
out();
out("### Detail par module");
out();
for (const r of [...partiels].sort((a, b) => a.path.localeCompare(b.path))) {
  const b = r.bilan;
  out(`**\`${court(r.path)}\`** — ${r.decl.cpp}. Reste ${b.codeReste} lignes de code sur ${r.code}` +
      (b.horsFnReste ? ` (dont ${b.horsFnReste} des ${b.codeHorsFn} lignes hors fonction, au prorata)` : "") + ".");
  out();
  out(`- portees (${b.portees.length}) : ${b.portees.map((n) => "`" + n + "`").join(", ") || "aucune"}`);
  if (b.reduitesListe.length) out(`- reduites (${b.reduitesListe.length}) : ${b.reduitesListe.map((n) => "`" + n + "`").join(", ")}`);
  if (b.horsListe.length) out(`- hors perimetre (${b.horsListe.length}) : ${b.horsListe.map((n) => "`" + n + "`").join(", ")}`);
  if (b.reste.length) {
    const montre = b.resteGros.slice(0, 8).map((x) => `\`${x.name}\` (${x.code})`).join(", ");
    out(`- restent (${b.reste.length} fonctions) : ${montre}${b.reste.length > 8 ? `, … et ${b.reste.length - 8} autres` : ""}`);
  }
  if (b.introuvables.length) out(`- **nommees par PORTAGE.md, introuvables dans le module** : ${b.introuvables.map((n) => "`" + n + "`").join(", ")}`);
  out();
}
const portes = by("PORTE").filter((r) => r.bilan);
if (portes.length) {
  out("### Deja porte, controle");
  out();
  out("Classes *porte* parce que chaque fonction y est portee — nommee par PORTAGE.md ou retrouvee dans le");
  out("C++ — ou ecartee par PORTAGE.md :");
  out();
  out("| Module | C++ | fonctions | hors perimetre | source |");
  out("| --- | --- | ---: | --- | --- |");
  for (const r of portes) {
    out(`| \`${court(r.path)}\` | ${r.decl.cpp} | ${r.bilan.portees.length} | ${r.bilan.horsListe.map((n) => "`" + n + "`").join(", ") || "—"} | ${r.decl.source} |`);
  }
  out();
}
out("Cites par PORTAGE.md, non comptes en fonctions :");
out();
for (const [m, note] of PORTAGE_HORS_COMPTE) out(`- \`${court(m)}\` — ${note}`);
if (declaresAbsents.length) {
  out();
  out(`**Declares mais absents de la reference** : ${declaresAbsents.map((m) => "`" + m + "`").join(", ")}.`);
}
out();

out("## Les regles");
out();
out("Elles s'appliquent dans cet ordre, la premiere qui match gagne.");
out();
out("1. **Porte / partiellement porte** — declare dans `ported-functions.mjs`, recopie des tableaux de");
out("   `PORTAGE.md`. *Porte* seulement si **chaque** fonction du module est portee (ou ecartee par");
out("   PORTAGE.md) et qu'aucune n'y est reduite. Sinon *partiel*, et ce qui reste compte dans les lignes");
out("   a porter.");
out("2. **Jeter / navigateur** — filets de securite et amarres DOM. `PORTAGE.md` pose deja la regle :");
out("   Unreal a ses propres equivalents, *le choix est a refaire, pas a traduire*.");
out("3. **Jeter / presentation** — lit la simulation pour la raconter a une UI qui n'existera pas");
out("   sous cette forme. Refait contre le HUD Unreal.");
out("4. **Jeter / baril** — `index.js` de re-export : sans objet en C++.");
out("5. **Jeter / non atteint** — aucun chemin depuis `src/main.js`. A brancher ou a enterrer,");
out("   decision de conception, pas de portage.");
out("6. **Generer** — table de contenu. Ajouter un batiment, une espece ou une replique ne doit");
out("   jamais demander une recompilation.");
out("7. **Porter** — le reste. Marque *scinder* quand une table de contenu y est melee.");
out();
out("Une fonction est **portee** si PORTAGE.md la nomme. Sinon, le C++ de `Source/AnastasisSim` (hors");
out("tests) est interroge :");
out();
out("- module que PORTAGE.md cite **en entier** ou sans liste : il suffit que le C++ nomme la fonction (nom JS,");
out("  PascalCase ou alias declare) sur une ligne qui ne dit pas qu'elle n'est *pas* portee ;");
out("- module dont PORTAGE.md **liste** les fonctions : il faut une fonction C++ de ce nom (PascalCase suivi");
out("  d'une parenthese). Un commentaire qui cite la reference ne suffit pas — il dit souvent ce que le C++");
out("  ne fait pas (`exploreTarget` tire `sim.rng` : non porte).");
out("- `simulation.js` et `npc.js` ne sont pas interroges : seule la liste de PORTAGE.md compte.");
out();

out("## Ce que cet inventaire ne sait pas");
out();
out("- Les verdicts venus d'une liste explicite sont des **decisions**, pas des mesures. Elles");
out("  sont dans `tools/migration/inventory-js-sim.mjs` et `ported-functions.mjs`, chacune avec sa raison,");
out("  et se discutent.");
out("- **Portee ne veut pas dire prouvee bit a bit.** La colonne dit ce que PORTAGE.md declare porte ; la");
out("  preuve est dans les tests `Anastasis.Sim.Parite.*` cites par PORTAGE.md.");
out("- Pour un module cite en entier, la recherche de noms dans le C++ est **indulgente** : un nom generique");
out("  (`push`, `pop`, `bump`) y est trouve sans que la fonction JS soit forcement celle-la. Pour un module a");
out("  liste, elle est **stricte** : une fonction aidante absorbee sans nom par une fonction portee compte dans");
out("  le reste. Le reste d'un module partiel est donc un ordre de grandeur, pas un decompte au mot pres.");
out("- Les extents de fonctions viennent d'un scanner (`js-functions.mjs`), pas d'un parseur JS : une");
out("  expression reguliere precedee de `)` serait mal lue. Controle a chaque generation : sur les");
out(`  ${rows.length} modules du perimetre, ${avales.length} definition(s) en colonne 0 avalee(s) par une autre` +
    (avales.length ? ` — **le scanner s'est trompe** : ${avales.slice(0, 5).map((a) => "`" + a + "`").join(", ")}.` : " (attendu : 0)."));
out("- `code` compte les lignes de noms d'un baril de re-export : le total de la colonne **A jeter**");
out("  est surevalue d'environ 800 lignes pour cette raison. Sans consequence, on les jette.");
out("- La part de litteraux rate les gabarits multi-lignes. Un module peut porter plus de contenu");
out("  que la colonne `txt` ne le dit — le seuil *scinder* est un plancher, pas un plafond.");
out("- **Non atteint depuis `main.js`** ne veut pas dire mort. `sim/villageSpectrum.js` (partition");
out("  spectrale du village par vecteur de Fiedler) est ecrit, documente, et branche nulle part :");
out("  c'est une decision de conception en attente, pas un dechet.");
out("- `world.js` garde un vecteur `Fbm` divergent d'environ 2,5 ulp. Voir `PORTAGE.md`.");
out();

const VERDICT_ORDRE = { PORTER: 0, PARTIEL: 1, GENERER: 2, JETER: 3, PORTE: 4 };
const trie = [...rows].sort((a, b) =>
  VERDICT_ORDRE[a.verdict] - VERDICT_ORDRE[b.verdict] ||
  a.vague - b.vague ||
  b.aPorter - a.aPorter ||
  b.code - a.code);

out("## Le detail");
out();
out("`code` exclut commentaires, lignes vides et litteraux de texte. `reste` = lignes de code a porter.");
out("`txt` est le nombre de lignes qui ne sont qu'un litteral. `imp` = nombre de modules du noyau qui");
out("importent celui-ci.");
out();
out("| Verdict | Module | lignes | code | reste | txt | imp | Vague | Chantier | Note |");
out("| --- | --- | ---: | ---: | ---: | ---: | ---: | --- | --- | --- |");
for (const r of trie) {
  const coreImp = r.importers.filter(isCore).length;
  const verdict = r.scinder ? "porter + scinder"
    : r.verdict === "PORTE" ? "porte"
    : r.verdict === "PARTIEL" ? "partiel"
    : r.verdict.toLowerCase();
  const horsVague = r.verdict === "JETER" || r.verdict === "GENERER";
  const v = horsVague ? "—" : String(r.vague);
  const ch = horsVague || r.verdict === "PORTE" ? "—" : r.chantier;
  const note = r.verdict === "PARTIEL"
    ? `${r.raison} — ${r.bilan.portees.length}/${r.bilan.total} fonctions portees`
    : r.raison;
  out(`| ${verdict} | \`${court(r.path)}\` | ${r.lines} | ${r.code} | ${r.aPorter} | ${r.stringData} | ${coreImp} | ${v} | ${ch} | ${note} |`);
}
out();

const report = md.join("\n");
if (OUT) {
  mkdirSync(dirname(OUT), { recursive: true });
  writeFileSync(OUT, report, "utf8");
  console.error(`Ecrit: ${OUT} (${rows.length} modules ; ${by("PORTE").length} portes, ${partiels.length} partiels, ${aPorter.length} a porter)`);
} else {
  process.stdout.write(report);
}
