// PREMIER TICK DIVERGENT.
//
// Compare deux traces d'empreintes et repond a la seule question qui compte:
// ou les deux simulations ont-elles cesse d'etre la meme, et dans quelle
// section. Tout le reste du rapport est du confort; cette ligne-la est le
// harnais.
//
//   node tools/migration/compare-digests.mjs a.jsonl b.jsonl
//
// Le rapport nomme aussi, pour CHAQUE section, le tick ou elle diverge en
// premier. C'est ce qui transforme une divergence en ordre de travail: si
// `economy` part au tick 812 et `actors` au tick 813, on ne cherche pas dans
// les acteurs — on cherche ce que l'economie leur a fait.
//
// Options:
//   -max <n>     nombre de sections / elements detailles (defaut 12)
//   -out <f>     ecrire le rapport dans un fichier plutot que sur stdout
//   -json        sortie machine

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname } from "node:path";

const argv = process.argv.slice(2);
const AVEC_VALEUR = new Set(["-max", "-out"]);
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

if (positionnels.length < 2) {
  console.error("Usage: node tools/migration/compare-digests.mjs <a.jsonl> <b.jsonl> [-max n] [-out f] [-json]");
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

const ticksCommuns = [...A.echantillons.keys()].filter((t) => B.echantillons.has(t)).sort((x, y) => x - y);

// --- Comparaison ------------------------------------------------------------
let premierDivergent = null;
const premierParSection = new Map();
const sectionsVues = new Set();

for (const t of ticksCommuns) {
  const a = A.echantillons.get(t);
  const b = B.echantillons.get(t);
  const cles = new Set([...Object.keys(a.s || {}), ...Object.keys(b.s || {})]);
  for (const k of cles) sectionsVues.add(k);
  if (a.g === b.g) continue;
  if (premierDivergent === null) premierDivergent = t;
  for (const k of cles) {
    if (a.s?.[k] !== b.s?.[k] && !premierParSection.has(k)) premierParSection.set(k, t);
  }
}

// Persistance: une divergence qui disparait au tick suivant ne se traite pas
// comme une derive — c'est souvent un champ transitoire (un timer, une file),
// et le dire evite de partir sur la mauvaise piste.
let persistante = null;
if (premierDivergent !== null) {
  const apres = ticksCommuns.filter((t) => t > premierDivergent);
  const divergentsApres = apres.filter((t) => A.echantillons.get(t).g !== B.echantillons.get(t).g);
  persistante = apres.length === 0 ? null : divergentsApres.length === apres.length;
}

// --- Forage ----------------------------------------------------------------
const forage = [];
if (premierDivergent !== null) {
  const a = A.echantillons.get(premierDivergent);
  const b = B.echantillons.get(premierDivergent);
  if (a.drill && b.drill) {
    for (const section of Object.keys(a.drill)) {
      if (!b.drill[section]) continue;
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
const R = [];
const out = (s = "") => R.push(s);

out("RAPPORT DE DIVERGENCE");
out("=====================");
out();
out(`A : ${A.chemin}`);
out(`    source=${A.header.source} graine=${A.header.seed} dt=${A.header.dt} ticks=${A.header.ticks} every=${A.header.every} ref=${A.header.refHead ?? "-"}`);
out(`B : ${B.chemin}`);
out(`    source=${B.header.source} graine=${B.header.seed} dt=${B.header.dt} ticks=${B.header.ticks} every=${B.header.every} ref=${B.header.refHead ?? "-"}`);
out();

if (incompatibles.length) {
  out("TRACES INCOMPARABLES — conditions differentes :");
  for (const i of incompatibles) out(`  - ${i}`);
  out();
  out("Une divergence entre ces deux traces n'accuserait pas la simulation.");
  const texte = R.join("\n") + "\n";
  if (OUT) { mkdirSync(dirname(OUT), { recursive: true }); writeFileSync(OUT, texte, "utf8"); }
  else process.stdout.write(JSON_OUT ? JSON.stringify({ comparable: false, incompatibles }, null, 1) + "\n" : texte);
  process.exit(3);
}

out(`Ticks compares : ${ticksCommuns.length}` +
    (ticksCommuns.length ? ` (de ${ticksCommuns[0]} a ${ticksCommuns[ticksCommuns.length - 1]})` : ""));
const seulA = [...A.echantillons.keys()].filter((t) => !B.echantillons.has(t)).length;
const seulB = [...B.echantillons.keys()].filter((t) => !A.echantillons.has(t)).length;
if (seulA || seulB) out(`Ticks presents d'un seul cote : ${seulA} dans A, ${seulB} dans B — non compares.`);
out();

if (!ticksCommuns.length) {
  out("AUCUN TICK EN COMMUN. Les deux traces n'echantillonnent pas les memes ticks.");
} else if (premierDivergent === null) {
  out(`IDENTIQUES sur les ${ticksCommuns.length} ticks compares.`);
  out(`${sectionsVues.size} sections suivies, aucune n'a devie.`);
} else {
  const ech = A.echantillons.get(premierDivergent);
  out(`PREMIER TICK DIVERGENT : ${premierDivergent}` +
      (ech ? `  (jour ${ech.day}, temps ${ech.time})` : ""));
  if (persistante === true) out("La divergence persiste jusqu'a la fin de la trace.");
  else if (persistante === false) out("La divergence est transitoire : des ticks ulterieurs redeviennent identiques.");
  out();

  const a = A.echantillons.get(premierDivergent);
  const b = B.echantillons.get(premierDivergent);
  const auPremier = [...sectionsVues].filter((k) => a.s?.[k] !== b.s?.[k]).sort();
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
  const stables = [...sectionsVues].filter((k) => !premierParSection.has(k)).sort();
  out();
  out(`Sections restees identiques sur toute la trace (${stables.length}) : ${stables.join(", ") || "aucune"}`);

  if (forage.length) {
    out();
    out(`Forage au tick ${premierDivergent} :`);
    for (const f of forage) {
      out(`  ${f.section} — ${f.ecarts.length} element(s) sur ${f.total}`);
      for (const e of f.ecarts.slice(0, MAX)) out(`    ${String(e.id).padEnd(18)} ${e.etat}`);
      if (f.ecarts.length > MAX) out(`    … et ${f.ecarts.length - MAX} autres`);
    }
  } else if (premierDivergent !== null) {
    out();
    out(`Pas de forage a ce tick. Relancer les deux emetteurs avec -drill ${premierDivergent}`);
    out("pour obtenir l'empreinte element par element des acteurs, batiments et animaux.");
  }
}
out();

const texte = R.join("\n") + "\n";
const machine = {
  comparable: true,
  ticksCompares: ticksCommuns.length,
  premierDivergent,
  persistante,
  parSection: Object.fromEntries(premierParSection),
  forage,
};
const sortie = JSON_OUT ? JSON.stringify(machine, null, 1) + "\n" : texte;
if (OUT) {
  mkdirSync(dirname(OUT), { recursive: true });
  writeFileSync(OUT, sortie, "utf8");
  console.error(`Ecrit: ${OUT}`);
} else {
  process.stdout.write(sortie);
}
process.exit(premierDivergent === null ? 0 : 1);
