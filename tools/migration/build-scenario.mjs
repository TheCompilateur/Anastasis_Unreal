// CONSTRUCTEUR DE SCENARIO — un etat de depart commun, ecrit par `serialize`.
//
// Le harnais ne peut comparer JS et Unreal que s'ils partent du meme etat. La
// reference demarre avec les fondateurs romains, les sagas, l'economie et
// Kosmos; le C++ ne sait pas construire cet etat. La sortie (P2_MODELE_DONNEES.md,
// decision 2; P3_PLAN.md §2): un SCENARIO est une sauvegarde JS. Le C++ la lira
// (sim-state-reader-001), l'emetteur JS la charge par `deserialize`, et les deux
// tournent.
//
//   node tools/migration/build-scenario.mjs -ref <depot JS> -scenario endurance -out tools/migration/scenarios/endurance.json
//
// Options:
//   -ref <chemin>      checkout PROPRE du tag de reference (REFERENCE_JS.md)
//   -scenario <nom>    recette `tools/migration/scenarios/<nom>.mjs`
//   -out <fichier>     scenario JSON
//   -audit-days <n>    jours de l'audit des tirages (defaut 3)
//   -allow-dirty       accepter une reference dont `src/` est modifie
//
// La construction n'utilise que l'API publique de la reference:
//   new Simulation({ deferred: true, seed }) puis resetWorldBase(seed, { pickSite: false })
//     — le monde de la graine, sans site, sans habitant, sans faune ;
//   serialize puis deserialize — le monde tel qu'une sauvegarde le decrit (couronne
//     du village, champs retires de la clairiere) ;
//   la recette (addBuilding, spawnNpc…) ;
//   serialize — le scenario.
//
// L'etat ecrit est un POINT FIXE de l'aller-retour: deserialize(scenario) puis
// serialize rend le meme etat, section par section. Sans cela, le tick 0 de
// l'emetteur (qui charge par deserialize) ne serait pas l'etat du fichier, et le
// lecteur C++ serait juge contre un etat que personne n'a ecrit. Le constructeur
// itere l'aller-retour jusqu'au point fixe, et refuse s'il ne l'atteint pas.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { execFileSync } from "node:child_process";

import { digestState } from "./state-digest.mjs";
import { chargerReference, MASQUES, NON_MASQUABLES } from "./scenarios/masks.mjs";
import { empreinteScenario, provenanceReference, FORMAT_SCENARIO } from "./scenarios/scenario-format.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const argv = process.argv.slice(2);
const argOf = (flag, fallback) => {
  const i = argv.indexOf(flag);
  return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : fallback;
};
const REF = argOf("-ref", "C:/dev/Jeux IV Kingdoms").split(String.fromCharCode(92)).join("/");
const NOM = argOf("-scenario", null);
const OUT = argOf("-out", null);
const AUDIT_JOURS = Number(argOf("-audit-days", 3));
if (!NOM || !OUT) {
  console.error("Usage: node tools/migration/build-scenario.mjs -ref <depot JS> -scenario <nom> -out <fichier.json>");
  process.exit(2);
}

const provenance = provenanceReference(REF);
if (provenance.modifie && !argv.includes("-allow-dirty")) {
  console.error(`REFUS: ${REF} porte ${provenance.modifie} modification(s) non commitee(s) sous src/.`);
  console.error("Un scenario se construit contre un commit: checkout propre du tag de reference (REFERENCE_JS.md).");
  process.exit(3);
}

const ref = await chargerReference(REF);
const recette = await import(pathToFileURL(join(ICI, "scenarios", `${NOM}.mjs`)).href);
const meta = recette.meta;
const inconnus = meta.masques.filter((id) => !MASQUES[id]);
if (inconnus.length) {
  console.error(`Masque(s) inconnu(s) dans ${NOM}.mjs : ${inconnus.join(", ")}`);
  process.exit(2);
}

const copie = (o) => JSON.parse(JSON.stringify(o));
const charger = (save) => {
  const sim = new ref.Simulation({ deferred: true, seed: save.seed });
  const { warnings } = ref.deserialize(sim, copie(save));
  return { sim, warnings };
};

// 1) Le monde de la graine, sans site, puis tel qu'une sauvegarde le decrit.
const vierge = new ref.Simulation({ deferred: true, seed: meta.seed });
vierge.resetWorldBase(meta.seed, { pickSite: false });
const { sim } = charger(ref.serialize(vierge));

// 2) La recette.
const essai = (fn) => fn(charger(ref.serialize(sim)).sim);
const resultat = recette.construire(sim, { ref, essai });

// 3) Point fixe de l'aller-retour.
let save = copie(ref.serialize(sim));
let iterations = 0;
let instables = [];
let instablesAuPremier = [];
let avertissements = [];
for (;;) {
  const { sim: relu, warnings } = charger(save);
  avertissements = warnings;
  const resauve = copie(ref.serialize(relu));
  const a = digestState(save).sections;
  const b = digestState(resauve).sections;
  instables = [...new Set([...Object.keys(a), ...Object.keys(b)])].filter((k) => a[k] !== b[k]).sort();
  iterations += 1;
  if (iterations === 1) instablesAuPremier = instables;
  if (!instables.length) break;
  if (iterations >= 4) {
    console.error(`REFUS: pas de point fixe apres ${iterations} allers-retours ; sections instables : ${instables.join(", ")}`);
    process.exit(4);
  }
  save = resauve;
}

// 4) Audit des tirages: ce que chaque systeme tire dans sim.rng, masques
//    leves puis poses. Dans un processus neuf (etat de module propre), par
//    l'emetteur: c'est lui qui applique les masques en usage reel.
// La vue du budget : `"settlement"` = le centre du village de la sauvegarde.
let vue = null;
if (meta.vue === "settlement") vue = { x: save.settlement.x, y: save.settlement.y };
else if (meta.vue && Number.isFinite(meta.vue.x) && Number.isFinite(meta.vue.y)) vue = { x: meta.vue.x, y: meta.vue.y };
else if (meta.vue != null) { console.error(`vue inconnue : ${JSON.stringify(meta.vue)}`); process.exit(2); }

const brouillon = {
  kind: "anastasis-scenario", format: FORMAT_SCENARIO, name: meta.name,
  seed: meta.seed, dt: meta.dt, dayDeferred: meta.dayDeferred, vue,
  sections: meta.sections, masques: meta.masques, save,
};
brouillon.empreinte = empreinteScenario(brouillon);
const tmp = join(dirname(OUT), `.${NOM}.audit.json`);
mkdirSync(dirname(OUT), { recursive: true });
writeFileSync(tmp, JSON.stringify(brouillon), "utf8");
const audit = (sansMasques) => JSON.parse(execFileSync(process.execPath, [
  join(ICI, "emit-state-digests.mjs"), "-ref", REF, "-scenario", tmp, "-days", String(AUDIT_JOURS),
  "-audit-rng", ...(sansMasques ? ["-sans-masques"] : []),
  ...(provenance.modifie ? ["-allow-dirty"] : []),
], { encoding: "utf8", stdio: ["ignore", "pipe", "inherit"] }));
const leve = audit(true);
const pose = audit(false);
try { (await import("node:fs")).rmSync(tmp); } catch { /* sans consequence */ }

// Un masque de minuit s'appelle `minuit.<travail>` ; l'etiquette d'audit aussi.
// `economieJournaliere` couvre l'etiquette `onNewDay` (la partie critique), et
// `minuit.memory.partiel` le travail `minuit.memory`.
const etiquettePour = (id) => (id === "economieJournaliere" ? "onNewDay"
  : id === "minuit.memory.partiel" ? "minuit.memory" : id);
const masques = meta.masques.map((id) => {
  const etiquette = etiquettePour(id);
  const sans = leve.tirages[etiquette] || 0;
  const avec = pose.tirages[etiquette] || 0;
  return {
    id,
    ...MASQUES[id],
    rng: {
      [`tiragesSansMasque_${AUDIT_JOURS}j`]: sans,
      [`tiragesAvecMasque_${AUDIT_JOURS}j`]: avec,
      verdict: sans > 0 ? "TIRE dans sim.rng : divergence attendue tant qu'il n'est pas porte" : "ne tire pas sur ce scenario",
    },
  };
});

const scenario = {
  kind: "anastasis-scenario",
  format: FORMAT_SCENARIO,
  name: meta.name,
  description: meta.description,
  reference: { tag: provenance.tag, commit: provenance.commit, modifie: provenance.modifie },
  construitPar: `node tools/migration/build-scenario.mjs -ref <anastasis-ref-p3> -scenario ${NOM} -out <ce fichier>`,
  seed: meta.seed,
  dt: meta.dt,
  dayDeferred: meta.dayDeferred,
  vue,
  sections: meta.sections,
  masques: meta.masques,
  masquesDetail: masques,
  etatVide: meta.etatVide,
  nonMasquables: NON_MASQUABLES,
  auditRng: {
    jours: AUDIT_JOURS,
    sansMasques: leve,
    avecMasques: pose,
    lecture: "tirages de sim.rng par etape du tick (etiquettes de tickRecompose et des travaux de minuit), "
      + "scenario charge par deserialize, dt du scenario. `sansMasques` : tous les systemes tournent ; "
      + "`avecMasques` : usage reel. L'ordre des etapes restantes est celui du tick de reference.",
  },
  pointFixe: {
    allersRetours: iterations,
    instablesAuPremierAllerRetour: instablesAuPremier,
    lecture: "l'etat construit, recharge puis resauve, change d'abord dans ces sections ; le fichier porte l'etat "
      + "apres rechargement, qui se resauve a l'identique (point fixe). C'est cet etat que le lecteur C++ doit reproduire.",
    avertissementsDeserialize: avertissements,
  },
  recette: resultat,
  save,
};
scenario.empreinte = empreinteScenario(scenario);

// Le scenario en tete, lisible ; la sauvegarde a la fin, compacte.
const texte = JSON.stringify({ ...scenario, save: "__SAVE__" }, null, 2)
  .replace('"__SAVE__"', JSON.stringify(save)) + "\n";
writeFileSync(OUT, texte, "utf8");
console.error(`Ecrit: ${OUT} — scenario ${meta.name}, empreinte ${scenario.empreinte}, ${meta.masques.length} masques, `
  + `point fixe en ${iterations} aller(s)-retour(s), ${(texte.length / 1024).toFixed(0)} Ko`);
