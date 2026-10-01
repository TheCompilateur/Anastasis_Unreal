// PREMIER TICK DIVERGENT.
//
// Compare deux traces d'empreintes et repond a la seule question qui compte:
// ou les deux simulations ont-elles cesse d'etre la meme, et dans quelle
// section. Tout le reste du rapport est du confort; cette ligne-la est le
// harnais.
//
//   node tools/migration/compare-digests.mjs a.jsonl b.jsonl
//   node tools/migration/compare-digests.mjs a.jsonl b.jsonl -sections actors,buildings
//
// Le rapport nomme aussi, pour CHAQUE section, le tick ou elle diverge en
// premier. C'est ce qui transforme une divergence en ordre de travail: si
// `economy` part au tick 812 et `actors` au tick 813, on ne cherche pas dans
// les acteurs — on cherche ce que l'economie leur a fait.
//
// PERIMETRE. Une trace de scenario (emit-state-digests.mjs -scenario) porte
// dans son en-tete le perimetre du scenario: les sections que le harnais sait
// juger, parce que le C++ les projette et que les systemes qui les ecrivent
// sont portes ou masques. Le comparateur ne conclut que sur ce perimetre, ou
// sur la partie que `-sections` en retient. Il REFUSE:
//   - deux traces dont le scenario (nom, empreinte) ou les masques different —
//     comme il refuse deja deux graines differentes ;
//   - une section demandee par `-sections` hors du perimetre du scenario, ou
//     absente des traces.
// Une section hors du jugement n'est jamais annoncee divergente: le rapport la
// nomme parmi les sections ignorees, sans verdict.
//
// Options:
//   -sections a,b,c  sections a juger (defaut: le perimetre du scenario, ou
//                    toutes les sections pour deux traces sans scenario)
//   -max <n>         nombre de sections / elements detailles (defaut 12)
//   -out <f>         ecrire le rapport dans un fichier plutot que sur stdout
//   -json            sortie machine
//
// Sortie: 0 identiques, 1 divergence, 3 refus de comparer, 2 usage.

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname } from "node:path";

const argv = process.argv.slice(2);
const AVEC_VALEUR = new Set(["-max", "-out", "-sections"]);
const drapeaux = new Map();
const positionnels = [];
for (let i = 0; i < argv.length; i += 1) {
  const a = argv[i];
  if (AVEC_VALEUR.has(a)) { drapeaux.set(a, argv[++i]); continue; }
  if (a.startsWith("-")) { drapeaux.set(a, true); continue; }
  positionnels.push(a);
}
const MAX = Number(drapeaux.get("-max") ?? 12);
const OUT = drapeaux.get("-out") ?? null;
const JSON_OUT = drapeaux.has("-json");
const SECTIONS_DEMANDEES = typeof drapeaux.get("-sections") === "string"
  ? drapeaux.get("-sections").split(",").map((s) => s.trim()).filter(Boolean)
  : null;

if (positionnels.length < 2) {
  console.error("Usage: node tools/migration/compare-digests.mjs <a.jsonl> <b.jsonl> [-sections a,b] [-max n] [-out f] [-json]");
  process.exit(2);
}

function lire(chemin) {
  const lignes = readFileSync(chemin, "utf8").split(/\r?\n/).filter((l) => l.trim() !== "");
  const objets = lignes.map((l) => JSON.parse(l));
  const header = objets[0]?.kind === "header" ? objets[0] : null;
  if (!header) throw new Error(`${chemin}: premiere ligne absente ou non conforme (attendu kind=header)`);
  const echantillons = new Map();
  for (const o of objets.slice(1)) echantillons.set(o.t, o);
  return { chemin, header, echantillons };
}

const A = lire(positionnels[0]);
const B = lire(positionnels[1]);

const R = [];
const out = (s = "") => R.push(s);
const ecrire = (texte, machine, code) => {
  const sortie = JSON_OUT ? JSON.stringify(machine, null, 1) + "\n" : texte;
  if (OUT) {
    mkdirSync(dirname(OUT), { recursive: true });
    writeFileSync(OUT, sortie, "utf8");
    console.error(`Ecrit: ${OUT}`);
  } else {
    process.stdout.write(sortie);
  }
  process.exit(code);
};

const decrireScenario = (h) => (h.scenario
  ? `scenario=${h.scenario.name} empreinte=${h.scenario.empreinte} masques=${h.scenario.masques.length}`
  : "scenario=aucun");

out("RAPPORT DE DIVERGENCE");
out("=====================");
out();
out(`A : ${A.chemin}`);
out(`    source=${A.header.source} graine=${A.header.seed} dt=${A.header.dt} ticks=${A.header.ticks} every=${A.header.every} ref=${A.header.refHead ?? "-"}`);
out(`    ${decrireScenario(A.header)}`);
out(`B : ${B.chemin}`);
out(`    source=${B.header.source} graine=${B.header.seed} dt=${B.header.dt} ticks=${B.header.ticks} every=${B.header.every} ref=${B.header.refHead ?? "-"}`);
out(`    ${decrireScenario(B.header)}`);
out();

// --- Compatibilite ---------------------------------------------------------
// Comparer deux traces prises dans des conditions differentes ne prouve rien
// et produirait une divergence au premier tick, qu'on mettrait sur le dos du
// portage. Mieux vaut refuser que mentir.
const incompatibles = [];
for (const cle of ["spec", "seed", "dt", "dayLength"]) {
  if (A.header[cle] !== B.header[cle]) {
    incompatibles.push(`${cle}: ${A.header[cle]} vs ${B.header[cle]}`);
  }
}
// Le scenario et ses masques font partie des conditions. Une trace masquee face
// a une trace qui ne l'est pas jugerait les masques, pas le portage.
const sA = A.header.scenario ?? null;
const sB = B.header.scenario ?? null;
if (!!sA !== !!sB) {
  incompatibles.push(`scenario: ${sA ? sA.name : "aucun"} vs ${sB ? sB.name : "aucun"}`);
} else if (sA && sB) {
  if (sA.name !== sB.name) incompatibles.push(`scenario: ${sA.name} vs ${sB.name}`);
  if (sA.empreinte !== sB.empreinte) incompatibles.push(`empreinte du scenario: ${sA.empreinte} vs ${sB.empreinte}`);
  const mA = [...(sA.masques || [])].sort();
  const mB = [...(sB.masques || [])].sort();
  if (mA.join(",") !== mB.join(",")) {
    const seulsA = mA.filter((m) => !mB.includes(m));
    const seulsB = mB.filter((m) => !mA.includes(m));
    incompatibles.push(`masques: ${mA.length} vs ${mB.length}` +
      (seulsA.length ? ` ; seulement dans A : ${seulsA.join(", ")}` : "") +
      (seulsB.length ? ` ; seulement dans B : ${seulsB.join(", ")}` : ""));
  }
  if ((sA.sections || []).join(",") !== (sB.sections || []).join(",")) {
    incompatibles.push(`perimetre: ${(sA.sections || []).join(",")} vs ${(sB.sections || []).join(",")}`);
  }
}

if (incompatibles.length) {
  out("TRACES INCOMPARABLES — conditions differentes :");
  for (const i of incompatibles) out(`  - ${i}`);
  out();
  out("Une divergence entre ces deux traces n'accuserait pas la simulation.");
  ecrire(R.join("\n") + "\n", { comparable: false, incompatibles }, 3);
}

// --- Perimetre --------------------------------------------------------------
const ticksCommuns = [...A.echantillons.keys()].filter((t) => B.echantillons.has(t)).sort((x, y) => x - y);
const sectionsVues = new Set();
for (const t of ticksCommuns) {
  for (const k of Object.keys(A.echantillons.get(t).s || {})) sectionsVues.add(k);
  for (const k of Object.keys(B.echantillons.get(t).s || {})) sectionsVues.add(k);
}
const perimetre = sA ? [...sA.sections] : null;
const refusPerimetre = [];
if (SECTIONS_DEMANDEES) {
  for (const k of SECTIONS_DEMANDEES) {
    if (perimetre && !perimetre.includes(k)) refusPerimetre.push(`${k} : hors du perimetre du scenario ${sA.name} (${perimetre.join(", ")})`);
    else if (ticksCommuns.length && !sectionsVues.has(k)) refusPerimetre.push(`${k} : absente des deux traces`);
  }
}
if (perimetre) {
  for (const k of perimetre) {
    if (ticksCommuns.length && !sectionsVues.has(k)) refusPerimetre.push(`${k} : section du perimetre absente des traces`);
  }
}
if (refusPerimetre.length) {
  out("REFUS DE CONCLURE — sections hors perimetre :");
  for (const r of refusPerimetre) out(`  - ${r}`);
  out();
  out("Le harnais ne juge que ce que le scenario declare. Une section hors perimetre diverge pour des");
  out("raisons que personne n'a encore decide de juger (systeme non porte, non masque, non projete).");
  ecrire(R.join("\n") + "\n", { comparable: false, horsPerimetre: refusPerimetre }, 3);
}
const jugees = SECTIONS_DEMANDEES ?? perimetre ?? [...sectionsVues].sort();
const ignorees = [...sectionsVues].filter((k) => !jugees.includes(k)).sort();

// --- Comparaison ------------------------------------------------------------
// Sur les seules sections jugees. L'empreinte globale `g` couvre tout l'etat:
// elle ne sert que quand tout est juge.
const toutJuge = ignorees.length === 0 && !SECTIONS_DEMANDEES && !perimetre;
const differe = (a, b) => (toutJuge ? a.g !== b.g : jugees.some((k) => a.s?.[k] !== b.s?.[k]));

let premierDivergent = null;
const premierParSection = new Map();
for (const t of ticksCommuns) {
  const a = A.echantillons.get(t);
  const b = B.echantillons.get(t);
  if (!differe(a, b)) continue;
  if (premierDivergent === null) premierDivergent = t;
  for (const k of jugees) {
    if (a.s?.[k] !== b.s?.[k] && !premierParSection.has(k)) premierParSection.set(k, t);
  }
}

// Persistance: une divergence qui disparait au tick suivant ne se traite pas
// comme une derive — c'est souvent un champ transitoire (un timer, une file),
// et le dire evite de partir sur la mauvaise piste.
let persistante = null;
if (premierDivergent !== null) {
  const apres = ticksCommuns.filter((t) => t > premierDivergent);
  const divergentsApres = apres.filter((t) => differe(A.echantillons.get(t), B.echantillons.get(t)));
  persistante = apres.length === 0 ? null : divergentsApres.length === apres.length;
}

// --- Forage ----------------------------------------------------------------
const forage = [];
if (premierDivergent !== null) {
  const a = A.echantillons.get(premierDivergent);
  const b = B.echantillons.get(premierDivergent);
  if (a.drill && b.drill) {
    for (const section of Object.keys(a.drill)) {
      if (!b.drill[section] || !jugees.includes(section)) continue;
      const parA = new Map(a.drill[section].map((e) => [e.id ?? `#${e.i}`, e]));
      const parB = new Map(b.drill[section].map((e) => [e.id ?? `#${e.i}`, e]));
      const cles = new Set([...parA.keys(), ...parB.keys()]);
      const ecarts = [];
      for (const k of cles) {
        const ea = parA.get(k), eb = parB.get(k);
        if (!ea) ecarts.push({ id: k, etat: "absent de A" });
        else if (!eb) ecarts.push({ id: k, etat: "absent de B" });
        else if (ea.h !== eb.h) ecarts.push({ id: k, etat: "empreinte differente", i: ea.i });
      }
      if (ecarts.length) forage.push({ section, total: cles.size, ecarts });
    }
  }
}

// --- Rapport ----------------------------------------------------------------
out(`Ticks compares : ${ticksCommuns.length}` +
    (ticksCommuns.length ? ` (de ${ticksCommuns[0]} a ${ticksCommuns[ticksCommuns.length - 1]})` : ""));
const seulA = [...A.echantillons.keys()].filter((t) => !B.echantillons.has(t)).length;
const seulB = [...B.echantillons.keys()].filter((t) => !A.echantillons.has(t)).length;
if (seulA || seulB) out(`Ticks presents d'un seul cote : ${seulA} dans A, ${seulB} dans B — non compares.`);
if (sA) out(`Masques du scenario (${sA.masques.length}) : ${sA.masques.join(", ") || "aucun"}`);
out(`Sections jugees (${jugees.length})${SECTIONS_DEMANDEES ? " [-sections]" : perimetre ? " [perimetre du scenario]" : ""} : ${jugees.join(", ")}`);
out(`Sections ignorees (${ignorees.length}), sans verdict : ${ignorees.join(", ") || "aucune"}`);
out();

if (!ticksCommuns.length) {
  out("AUCUN TICK EN COMMUN. Les deux traces n'echantillonnent pas les memes ticks.");
} else if (premierDivergent === null) {
  out(`IDENTIQUES sur les ${ticksCommuns.length} ticks compares.`);
  out(`${jugees.length} sections jugees, aucune n'a devie.`);
} else {
  const ech = A.echantillons.get(premierDivergent);
  out(`PREMIER TICK DIVERGENT : ${premierDivergent}` +
      (ech ? `  (jour ${ech.day}, temps ${ech.time})` : ""));
  if (persistante === true) out("La divergence persiste jusqu'a la fin de la trace.");
  else if (persistante === false) out("La divergence est transitoire : des ticks ulterieurs redeviennent identiques.");
  out();

  const a = A.echantillons.get(premierDivergent);
  const b = B.echantillons.get(premierDivergent);
  const auPremier = jugees.filter((k) => a.s?.[k] !== b.s?.[k]).sort();
  out(`Sections divergentes a ce tick (${auPremier.length}) :`);
  for (const k of auPremier.slice(0, MAX)) {
    out(`  ${k.padEnd(22)} A=${a.s?.[k] ?? "(absente)"}  B=${b.s?.[k] ?? "(absente)"}`);
  }
  if (auPremier.length > MAX) out(`  … et ${auPremier.length - MAX} autres`);
  out();

  out("Premiere divergence par section — l'ordre est l'ordre de travail :");
  const ordre = [...premierParSection.entries()].sort((x, y) => x[1] - y[1] || x[0].localeCompare(y[0]));
  for (const [k, t] of ordre.slice(0, MAX)) out(`  tick ${String(t).padStart(8)}  ${k}`);
  if (ordre.length > MAX) out(`  … et ${ordre.length - MAX} autres sections`);
  const stables = jugees.filter((k) => !premierParSection.has(k)).sort();
  out();
  out(`Sections jugees restees identiques sur toute la trace (${stables.length}) : ${stables.join(", ") || "aucune"}`);

  if (forage.length) {
    out();
    out(`Forage au tick ${premierDivergent} :`);
    for (const f of forage) {
      out(`  ${f.section} — ${f.ecarts.length} element(s) sur ${f.total}`);
      for (const e of f.ecarts.slice(0, MAX)) out(`    ${String(e.id).padEnd(18)} ${e.etat}`);
      if (f.ecarts.length > MAX) out(`    … et ${f.ecarts.length - MAX} autres`);
    }
  } else {
    out();
    out(`Pas de forage a ce tick. Relancer les deux emetteurs avec -drill ${premierDivergent}`);
    out("pour obtenir l'empreinte element par element des acteurs, batiments et animaux.");
  }
}
out();

ecrire(R.join("\n") + "\n", {
  comparable: true,
  scenario: sA ? { name: sA.name, empreinte: sA.empreinte, masques: sA.masques } : null,
  sectionsJugees: jugees,
  sectionsIgnorees: ignorees,
  ticksCompares: ticksCommuns.length,
  premierDivergent,
  persistante,
  parSection: Object.fromEntries(premierParSection),
  forage,
}, premierDivergent === null ? 0 : 1);
