// REPLIQUES DE LA REFERENCE — mission familles-feu-001.
//
//   node tools/migration/gen-talk-lines.mjs -ref <clone ou git archive de anastasis-ref-p3 (fee66ae)>
//
// Recopie telles quelles les repliques francaises que la reference fait dire aux habitants : catalogue
// de paroles (talkCatalog.js, canon 1204 applique), scenes (momentTalk.js), episodes (ai/episodes.js),
// vignettes, paroles collectives, de sceau, de terre et de memoire de temoin. Chaque liste de phrases
// devient un « pool » nomme par son chemin : `talkCatalog.BOND_LINES.partner`. Les gabarits gardent
// leurs trous (`{who}`, `{tale}`...). Le ton est celui de la reference : francais SANS accents.
//
// Sortie : Content/Anastasis/Dialogue/repliques-reference.json. Ne pas l'editer a la main : relancer.
// Les repliques propres a ANASTASIS (feu du premier soir, demande d'aide, vie de tous les jours)
// vivent a cote, dans repliques-valmire.json, ecrites a la main.

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");
const argv = process.argv.slice(2);
const argOf = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : d; };
const REF = argOf("-ref", null);
if (!REF) {
  console.error("-ref <clone de anastasis-ref-p3> requis");
  process.exit(2);
}
const REF_DIR = REF.split(String.fromCharCode(92)).join("/");
const OUT = argOf("-out", join(RACINE, "Content", "Anastasis", "Dialogue", "repliques-reference.json"));
const url = (rel) => pathToFileURL(join(REF_DIR, rel)).href;

// Modules lus, dans l'ordre. Un module qui ne se charge pas hors navigateur est signale, pas ignore en silence.
const MODULES = [
  ["talkCatalog", "src/life/talkCatalog.js"],
  ["momentTalk", "src/life/momentTalk.js"],
  ["episodes", "src/ai/episodes.js"],
  ["followVignette", "src/life/followVignette.js"],
  ["collectiveTalk", "src/life/collectiveTalk.js"],
  ["sealTalk", "src/life/sealTalk.js"],
  ["landTalk", "src/life/landTalk.js"],
  ["witnessMemory", "src/life/witnessMemory.js"],
];

const pools = {};
const failures = [];
const isLine = (v) => typeof v === "string" && /\s/.test(v.trim()) && v.trim().length >= 6;

// Une liste dont tous les elements sont des phrases devient un pool ; un objet se descend.
function walk(prefix, value, depth) {
  if (depth > 6 || value == null) return;
  if (Array.isArray(value)) {
    if (value.length && value.every(isLine)) {
      pools[prefix] = value.map((s) => s.trim());
      return;
    }
    value.forEach((item, i) => walk(`${prefix}.${i}`, item, depth + 1));
    return;
  }
  if (typeof value === "object") {
    for (const key of Object.keys(value)) walk(`${prefix}.${key}`, value[key], depth + 1);
  }
}

// Une table non exportee (`const HUNGER_LINES = [...]`) est relue du source et evaluee seule, comme
// gen-planner-catalog.mjs le fait pour CONSUME_ORDER. Une table qui reference autre chose est signalee.
function sourceTables(rel) {
  const src = readFileSync(join(REF_DIR, rel), "utf8");
  const found = [];
  const re = /\bconst ([A-Z][A-Z0-9_]+)\s*=\s*(?:Object\.freeze\(\s*)?[[{]/g;
  let m;
  while ((m = re.exec(src)) !== null) {
    const name = m[1];
    let i = src.indexOf("=", m.index) + 1;
    let depth = 0;
    let j = i;
    let quote = null;
    for (; j < src.length; j += 1) {
      const c = src[j];
      if (quote) {
        if (c === "\\") { j += 1; continue; }
        if (c === quote) quote = null;
        continue;
      }
      if (c === '"' || c === "'" || c === "`") { quote = c; continue; }
      if (c === "(" || c === "[" || c === "{") depth += 1;
      else if (c === ")" || c === "]" || c === "}") { depth -= 1; if (depth === 0) { j += 1; break; } }
    }
    const expr = src.slice(i, j).trim().replace(/Object\.freeze/g, "");
    try {
      // eslint-disable-next-line no-new-func
      found.push([name, new Function(`"use strict"; return (${expr});`)()]);
    } catch (err) {
      found.push([name, null, err.message]);
    }
  }
  return found;
}

for (const [name, rel] of MODULES) {
  const seen = new Set();
  try {
    const mod = await import(url(rel));
    for (const exp of Object.keys(mod).sort()) {
      if (typeof mod[exp] === "function") continue;
      seen.add(exp);
      walk(`${name}.${exp}`, mod[exp], 0);
    }
  } catch (err) {
    failures.push(`${name}: ${err.message}`);
  }
  for (const [table, value, error] of sourceTables(rel)) {
    if (seen.has(table)) continue;
    if (error) {
      if (/LINES|FRAMES|TALK|PHRASE|SAY/.test(table)) failures.push(`${name}.${table}: ${error}`);
      continue;
    }
    walk(`${name}.${table}`, value, 0);
  }
}

const names = Object.keys(pools).sort();
const sorted = {};
let lines = 0;
for (const n of names) {
  sorted[n] = pools[n];
  lines += pools[n].length;
}
const out = {
  _doc: "Repliques francaises de la reference JS, recopiees par tools/migration/gen-talk-lines.mjs. Ne pas editer : relancer. Ton de la reference : sans accents.",
  source: "anastasis-ref-p3 (fee66ae)",
  pools: names.length,
  lines,
  failures,
  data: sorted,
};
mkdirSync(dirname(OUT), { recursive: true });
writeFileSync(OUT, JSON.stringify(out, null, 2) + "\n", "utf8");
console.log(`TALK_LINES pools=${names.length} lines=${lines} failures=${failures.length} -> ${OUT}`);
for (const f of failures) console.log(`  FAIL ${f}`);
