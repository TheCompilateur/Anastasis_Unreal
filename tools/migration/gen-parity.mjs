// Genere le .inl d'une declaration de parite.
//
//   node tools/migration/gen-parity.mjs parity/simulation-budget.mjs
//   node tools/migration/gen-parity.mjs --tous
//
// La declaration vit dans tools/migration/parity/. Elle dit quelles fonctions
// de la reference appeler et sur quelles entrees; l'atelier (parity-kit.mjs)
// fait le reste.

import { readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { genererVecteurs } from "./parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const DOSSIER = join(ICI, "parity");

const argv = process.argv.slice(2);
const argOf = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : d; };
const REF = argOf("-ref", "C:/dev/Jeux IV Kingdoms");
const tous = argv.includes("--tous");
const nommes = argv.filter((a) => !a.startsWith("-") && a !== REF);

const fichiers = tous
  ? readdirSync(DOSSIER).filter((f) => f.endsWith(".mjs")).map((f) => join(DOSSIER, f))
  : nommes.map((n) => (n.includes("/") || n.includes("\\") ? resolve(n) : join(DOSSIER, n)));

if (fichiers.length === 0) {
  console.error("Usage: node tools/migration/gen-parity.mjs <declaration.mjs> | --tous");
  console.error(`Declarations disponibles dans ${DOSSIER}:`);
  for (const f of readdirSync(DOSSIER).filter((f) => f.endsWith(".mjs"))) console.error(`  ${f}`);
  process.exit(2);
}

let echec = false;
for (const fichier of fichiers) {
  const spec = (await import(pathToFileURL(fichier).href)).default;
  spec.declaration = fichier.split(String.fromCharCode(92)).join("/").split("/tools/").pop();
  try {
    const r = await genererVecteurs(spec, { ref: REF });
    console.error(`Ecrit: ${r.sortie} — ${r.cas} cas, ${r.vecteurs} vecteurs (reference ${r.tete})`);
  } catch (e) {
    echec = true;
    console.error(`ECHEC ${fichier}: ${e.message}`);
  }
}
process.exit(echec ? 1 : 0);
