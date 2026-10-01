// FORAGE CHAMP PAR CHAMP — ce qui differe entre deux etats, et ou.
//
// Le comparateur de traces nomme le premier tick divergent et la section. Pour
// savoir QUEL champ, on vide l'etat des deux cotes a ce tick et on les compare
// ici:
//
//   node tools/migration/emit-state-digests.mjs -ref <tag> -scenario <s.json> -ticks 1 -dump 1 -dump-out js.json
//   (Unreal) ANASTASIS_HARNESS_DRILL=1 : Saved/HarnessTraces/<scenario>-unreal.tick1.json
//   node tools/migration/diff-states.mjs js.json Saved/HarnessTraces/endurance-unreal.tick1.json
//
// Un tableau d'objets portant un `id` se compare par identifiant (ordre signale
// a part); un nombre se compare au bit pres, et l'ecart s'ecrit en ulp quand il
// est petit. Options: -sections a,b ; -max <n> lignes (defaut 60) ; -json.

import { readFileSync } from "node:fs";

const argv = process.argv.slice(2);
const positionnels = [];
const options = new Map();
for (let i = 0; i < argv.length; i += 1) {
  if (["-sections", "-max"].includes(argv[i])) { options.set(argv[i], argv[++i]); continue; }
  if (argv[i].startsWith("-")) { options.set(argv[i], true); continue; }
  positionnels.push(argv[i]);
}
if (positionnels.length < 2) {
  console.error("Usage: node tools/migration/diff-states.mjs <a.json> <b.json> [-sections a,b] [-max n] [-json]");
  process.exit(2);
}
const MAX = Number(options.get("-max") ?? 60);
const A = JSON.parse(readFileSync(positionnels[0], "utf8"));
const B = JSON.parse(readFileSync(positionnels[1], "utf8"));
const sections = options.has("-sections")
  ? String(options.get("-sections")).split(",")
  : [...new Set([...Object.keys(A), ...Object.keys(B)])].sort();

const vue = new DataView(new ArrayBuffer(8));
const bits = (x) => { vue.setFloat64(0, x); return vue.getBigUint64(0); };
function ecartUlp(a, b) {
  if (Math.sign(a) !== Math.sign(b) || !Number.isFinite(a) || !Number.isFinite(b)) return null;
  const d = bits(a) > bits(b) ? bits(a) - bits(b) : bits(b) - bits(a);
  return d < 1000000n ? Number(d) : null;
}
const court = (v) => {
  const s = JSON.stringify(v);
  return s === undefined ? "(absent)" : s.length > 90 ? s.slice(0, 87) + "..." : s;
};

const ecarts = [];
function comparer(a, b, chemin) {
  if (typeof a === "number" && typeof b === "number") {
    if (!Object.is(a, b)) {
      const u = ecartUlp(a, b);
      ecarts.push({ chemin, a, b, note: u !== null ? `${u} ulp` : `delta ${b - a}` });
    }
    return;
  }
  if (a === null || b === null || typeof a !== "object" || typeof b !== "object" || Array.isArray(a) !== Array.isArray(b)) {
    if (JSON.stringify(a) !== JSON.stringify(b)) ecarts.push({ chemin, a, b, note: "" });
    return;
  }
  if (Array.isArray(a)) {
    const parId = (arr) => arr.every((e) => e && typeof e === "object" && typeof e.id === "string");
    if (parId(a) && parId(b) && (a.length || b.length)) {
      const ia = new Map(a.map((e, i) => [e.id, i]));
      const ib = new Map(b.map((e, i) => [e.id, i]));
      const ordreA = a.map((e) => e.id).join(","), ordreB = b.map((e) => e.id).join(",");
      if (ordreA !== ordreB) ecarts.push({ chemin: `${chemin}[ordre]`, a: ordreA, b: ordreB, note: "" });
      for (const id of new Set([...ia.keys(), ...ib.keys()])) {
        comparer(ia.has(id) ? a[ia.get(id)] : undefined, ib.has(id) ? b[ib.get(id)] : undefined, `${chemin}[${id}]`);
      }
      return;
    }
    if (a.length !== b.length) ecarts.push({ chemin: `${chemin}.length`, a: a.length, b: b.length, note: "" });
    for (let i = 0; i < Math.min(a.length, b.length); i += 1) comparer(a[i], b[i], `${chemin}[${i}]`);
    return;
  }
  for (const k of [...new Set([...Object.keys(a), ...Object.keys(b)])].sort()) comparer(a[k], b[k], `${chemin}.${k}`);
}
for (const s of sections) comparer(A[s], B[s], s);

if (options.has("-json")) {
  process.stdout.write(JSON.stringify(ecarts, null, 1) + "\n");
} else {
  console.log(`A : ${positionnels[0]}`);
  console.log(`B : ${positionnels[1]}`);
  console.log(`Sections : ${sections.join(", ")}`);
  console.log(`${ecarts.length} champ(s) different(s)${ecarts.length > MAX ? `, ${MAX} montres` : ""} :`);
  for (const e of ecarts.slice(0, MAX)) {
    console.log(`  ${e.chemin}\n      A=${court(e.a)}  B=${court(e.b)}${e.note ? `  (${e.note})` : ""}`);
  }
}
process.exit(ecarts.length ? 1 : 0);
