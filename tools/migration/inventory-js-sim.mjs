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
//   node tools/migration/inventory-js-sim.mjs [-ref <chemin>] [-out <fichier.md>]
//
// Sans -out, le rapport part sur stdout.

import { readFileSync, readdirSync, statSync, existsSync, mkdirSync, writeFileSync } from "node:fs";
import { join, resolve, dirname, relative, basename } from "node:path";
import { execFileSync } from "node:child_process";

const BS = String.fromCharCode(92);
const norm = (p) => p.split(BS).join("/");

// --- Arguments --------------------------------------------------------------

const argv = process.argv.slice(2);
const argOf = (flag, fallback) => {
  const i = argv.indexOf(flag);
  return i >= 0 && argv[i + 1] ? argv[i + 1] : fallback;
};
const REF = norm(argOf("-ref", "C:/dev/Jeux IV Kingdoms"));
const OUT = argOf("-out", null);
const SRC = join(REF, "src");

if (!existsSync(SRC)) {
  console.error(`Depot de reference introuvable: ${SRC}`);
  console.error("Passer -ref <chemin du depot JS>.");
  process.exit(2);
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

// Deja porte en C++ (voir Source/AnastasisSim/PORTAGE.md).
const PORTE = new Map([
  ["src/sim/rng.js", "Core/AnastasisRng"],
  ["src/sim/util.js", "Core/AnastasisSimMath"],
  ["src/sim/spatialGrid.js", "Core/AnastasisSpatialGrid"],
  ["src/runtime/simClock.js", "Core/AnastasisSimClock"],
  ["src/sim/world.js", "World/AnastasisWorld (+ WorldNoise)"],
  ["src/sim/worldArchetypes.js", "World/AnastasisWorldArchetype (knobs sim seuls)"],
  ["src/sim/hydrology.js", "World/AnastasisHydrology"],
  ["src/sim/fieldCrops.js", "World/AnastasisWorld::PickFieldCropId"],
]);

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
  // d'un module de logique.
  let blank = 0, comment = 0, stringData = 0, code = 0, inBlock = false;
  for (const raw of lines) {
    const l = raw.trim();
    if (inBlock) { comment++; if (l.includes("*/")) inBlock = false; continue; }
    if (l === "") { blank++; continue; }
    if (l.startsWith("//")) { comment++; continue; }
    if (l.startsWith("/*")) { comment++; if (!l.includes("*/")) inBlock = true; continue; }
    if (/^(?:[\w$."'[\]]+\s*:\s*)?(["'`])(?:(?!\1).){12,}\1\s*,?\s*$/.test(l)) { stringData++; continue; }
    code++;
  }

  const ctrl = (text.match(/\b(if|for|while|switch)\s*\(/g) || []).length;
  const exportsFn = (text.match(/^export\s+(?:async\s+)?function\s+\w+/gm) || []).length;
  const exportsConst = (text.match(/^export\s+const\s+\w+/gm) || []).length;
  const browser = (text.match(/\b(document|window|localStorage|requestAnimationFrame|HTMLElement|navigator)\s*[.(]|THREE\.|createElement/g) || []).length;
  const reexportOnly = (text.match(/^export\s+[\s\S]*?from\s*["']/gm) || []).length;

  info.set(f, {
    file: f, path: norm(relative(REF, f)),
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

// --- Classification ---------------------------------------------------------
// Premiere regle qui s'applique gagne. L'ordre compte: un baril mort reste un
// baril, un catalogue atteint reste un catalogue.

function classify(i) {
  const p = i.path;
  if (PORTE.has(p)) return { verdict: "PORTE", raison: PORTE.get(p) };
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

function vague(i, verdict) {
  if (verdict === "PORTE") return PORTE.get(i.path)?.startsWith("Core/") ? 0 : 1;
  if (VAGUE_FICHIER.has(i.path)) return VAGUE_FICHIER.get(i.path);
  for (const [d, v] of VAGUE_DOSSIER) if (i.path.startsWith(d)) return v;
  return 5;
}

function chantier(i, v) {
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
  const v = vague(i, c.verdict);
  rows.push({ ...i, ...c, vague: v, chantier: chantier(i, v) });
}

// --- Rapport ----------------------------------------------------------------

let refHead = "inconnu";
try {
  refHead = execFileSync("git", ["-C", REF, "log", "-1", "--format=%h %cs"], { encoding: "utf8" }).trim();
} catch { /* depot sans git: la provenance sera juste moins precise */ }

const sum = (list, key) => list.reduce((a, x) => a + x[key], 0);
const by = (v) => rows.filter((r) => r.verdict === v);
const md = [];
const out = (s = "") => md.push(s);

out("# Inventaire du simulateur JS — porter / generer / jeter");
out();
out("**Genere. Ne pas editer a la main.**");
out();
out("```bash");
out("node tools/migration/inventory-js-sim.mjs -out docs/migration/phase2/P2_INVENTAIRE_JS.md");
out("```");
out();
out(`Reference : \`${REF}\` — HEAD \`${refHead}\`  `);
out(`Perimetre : ${CORE_DIRS.map((d) => "`" + d + "`").join(", ")} — le rendu, l'UI, l'audio et le debug sont hors sujet par decision (AGENTS.md).`);
out();
out("Le portage ne se mesure pas en lignes de JS. Une table de contenu devient une table de");
out("donnees, pas du C++ ecrit a la main ; un filet de securite navigateur se re-decide dans");
out("Unreal, il ne se traduit pas. Ce document dit, fichier par fichier, dans quel seau il tombe.");
out();

out("## Ce qu'il y a devant");
out();
out("| | fichiers | lignes | dont code |");
out("| --- | ---: | ---: | ---: |");
for (const v of ["PORTER", "GENERER", "JETER", "PORTE"]) {
  const s = by(v);
  const nom = { PORTER: "**A porter**", GENERER: "**A generer** (donnees)", JETER: "**A jeter**", PORTE: "Deja porte" }[v];
  out(`| ${nom} | ${s.length} | ${sum(s, "lines")} | ${sum(s, "code")} |`);
}
out(`| **Total** | ${rows.length} | ${sum(rows, "lines")} | ${sum(rows, "code")} |`);
out();
const aPorter = by("PORTER");
const scinder = aPorter.filter((r) => r.scinder);
out(`Sur les ${sum(aPorter, "lines")} lignes a porter, ${sum(aPorter, "comment")} sont du commentaire et`);
out(`${sum(aPorter, "blank")} des lignes vides : **${sum(aPorter, "code")} lignes de code** portent la simulation.`);
out(`${scinder.length} de ces modules melangent logique et table de contenu : la table s'extrait, le selecteur se porte.`);
out();

out("## Reste a porter, par vague");
out();
out("L'ordre est celui de `Source/AnastasisSim/PORTAGE.md` — il suit les dependances reelles,");
out("pas l'interet du gameplay.");
out();
out("| Vague | fichiers | lignes de code |");
out("| --- | ---: | ---: |");
for (const v of [2, 3, 4, 5, 6]) {
  const s = aPorter.filter((r) => r.vague === v);
  if (!s.length) continue;
  out(`| ${LIBELLE_VAGUE[v]} | ${s.length} | ${sum(s, "code")} |`);
}
out();
out(`Les vagues 5 et 6 ne sont pas des vagues, ce sont des marecages : ${aPorter.filter((r) => r.vague >= 5).length} modules a`);
out("elles deux. Elles se decoupent en chantiers, et c'est a ce grain qu'un module se confie.");
out();
out("| Vague | Chantier | fichiers | lignes de code | plus gros module |");
out("| --- | --- | ---: | ---: | --- |");
{
  const groupes = new Map();
  for (const r of aPorter) {
    const k = `${r.vague} ${r.chantier}`;
    if (!groupes.has(k)) groupes.set(k, []);
    groupes.get(k).push(r);
  }
  const tri = [...groupes.entries()].sort((a, b) => {
    const [va] = a[0].split(" "), [vb] = b[0].split(" ");
    return Number(va) - Number(vb) || sum(b[1], "code") - sum(a[1], "code");
  });
  for (const [k, s] of tri) {
    const [v, nom] = k.split(" ");
    const gros = [...s].sort((a, b) => b.code - a.code)[0];
    out(`| ${v} | ${nom} | ${s.length} | ${sum(s, "code")} | \`${gros.path.replace(/^src[/]/, "")}\` (${gros.code}) |`);
  }
}
out();

out("## Les regles");
out();
out("Elles s'appliquent dans cet ordre, la premiere qui match gagne.");
out();
out("1. **Deja porte** — inscrit dans `PORTAGE.md`, couches 0 et 1.");
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

out("## Ce que cet inventaire ne sait pas");
out();
out("- Les verdicts venus d'une liste explicite sont des **decisions**, pas des mesures. Elles");
out("  sont dans `tools/migration/inventory-js-sim.mjs`, chacune avec sa raison, et se discutent.");
out("- `code` compte les lignes de noms d'un baril de re-export : le total de la colonne **A jeter**");
out("  est surevalue d'environ 800 lignes pour cette raison. Sans consequence, on les jette.");
out("- La part de litteraux rate les gabarits multi-lignes. Un module peut porter plus de contenu");
out("  que la colonne `txt` ne le dit — le seuil *scinder* est un plancher, pas un plafond.");
out("- **Non atteint depuis `main.js`** ne veut pas dire mort. `sim/villageSpectrum.js` (partition");
out("  spectrale du village par vecteur de Fiedler) est ecrit, documente, et branche nulle part :");
out("  c'est une decision de conception en attente, pas un dechet.");
out("- Deux ports sont partiels et le tableau ne le dit pas : `worldArchetypes.js` n'a livre que ses");
out("  reglages de simulation (air, foret, garde-robe restent a la presentation), et `world.js`");
out("  garde un vecteur `Fbm` divergent d'environ 2,5 ulp. Voir `PORTAGE.md`.");
out();

const VERDICT_ORDRE = { PORTER: 0, GENERER: 1, JETER: 2, PORTE: 3 };
const trie = [...rows].sort((a, b) =>
  VERDICT_ORDRE[a.verdict] - VERDICT_ORDRE[b.verdict] ||
  a.vague - b.vague ||
  b.code - a.code);

out("## Le detail");
out();
out("`code` exclut commentaires, lignes vides et litteraux de texte. `txt` est le nombre de lignes");
out("qui ne sont qu'un litteral. `imp` = nombre de modules du noyau qui importent celui-ci.");
out();
out("| Verdict | Module | lignes | code | txt | imp | Vague | Chantier | Note |");
out("| --- | --- | ---: | ---: | ---: | ---: | --- | --- | --- |");
for (const r of trie) {
  const coreImp = r.importers.filter(isCore).length;
  const verdict = r.scinder ? "porter + scinder" : r.verdict === "PORTE" ? "porte" : r.verdict.toLowerCase();
  const horsVague = r.verdict === "JETER" || r.verdict === "GENERER";
  const v = horsVague ? "—" : String(r.vague);
  const ch = horsVague || r.verdict === "PORTE" ? "—" : r.chantier;
  out(`| ${verdict} | \`${r.path.replace(/^src[/]/, "")}\` | ${r.lines} | ${r.code} | ${r.stringData} | ${coreImp} | ${v} | ${ch} | ${r.raison} |`);
}
out();

const report = md.join("\n");
if (OUT) {
  mkdirSync(dirname(OUT), { recursive: true });
  writeFileSync(OUT, report, "utf8");
  console.error(`Ecrit: ${OUT} (${rows.length} modules)`);
} else {
  process.stdout.write(report);
}
