// Releve des buts d'un scenario du harnais, dans la reference JS : qui choisit quoi, quand,
// et ce que l'habitant porte. Sert a decouper une mission de buts AVANT de coder.
//
// Chargement, masques et tick : exactement ceux d'emit-state-digests.mjs (tick recompose),
// comme rng-trace-lib.mjs. Rien de la simulation n'est touche : on lit les habitants apres
// chaque tick.
//
// Usage :
//   node tools/migration/trace-goals.mjs -ref <clone anastasis-ref-p3> -scenario <json> [-ticks 16200] [-out <md>]
//
// Sortie : un compte par but (ticks passes, nombre de choix), et la liste des changements de
// but avec l'inventaire (food, wood, stone), l'activite et la cible.

import fs from "node:fs";
import path from "node:path";
import { chargerReference, appliquerMasques } from "./scenarios/masks.mjs";
import { chargerScenario } from "./scenarios/scenario-format.mjs";

function arg(nom, defaut = null) {
  const i = process.argv.indexOf(nom);
  return i >= 0 && i + 1 < process.argv.length ? process.argv[i + 1] : defaut;
}

const REF = arg("-ref");
const SCENARIO = arg("-scenario");
const TICKS = Number(arg("-ticks", "16200"));
const OUT = arg("-out");
if (!REF || !SCENARIO) {
  console.error("usage : node tools/migration/trace-goals.mjs -ref <clone> -scenario <json> [-ticks N] [-out <md>]");
  process.exit(2);
}

const scenario = chargerScenario(SCENARIO);
const ref = await chargerReference(path.resolve(REF));
const sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
ref.deserialize(sim, JSON.parse(JSON.stringify(scenario.save)));
if (scenario.vue) ref.pinSimulationView(sim.simulationBudget, scenario.vue.x, scenario.vue.y);
const { tick } = appliquerMasques(sim, ref, [...scenario.masques], { recompose: true });

const inv = (a) => `f${a.inventory?.food | 0} w${a.inventory?.wood | 0} s${a.inventory?.stone | 0}`;
const cible = (a) => (a.target ? `(${a.target.x.toFixed(1)}, ${a.target.y.toFixed(1)})` : "-");
const dernier = new Map();
const parBut = new Map();
const changements = [];
for (const a of sim.actors) dernier.set(a.id, a.goal);

for (let t = 1; t <= TICKS; t += 1) {
  tick(scenario.dt);
  for (const a of sim.actors) {
    const c = parBut.get(a.goal) || { ticks: 0, choix: 0 };
    c.ticks += 1;
    if (dernier.get(a.id) !== a.goal) {
      c.choix += 1;
      changements.push(`| ${t} | ${sim.day} | ${a.id} | ${dernier.get(a.id)} → **${a.goal}** | ${a.activity} | ${inv(a)} | ${cible(a)} |`);
      dernier.set(a.id, a.goal);
    }
    parBut.set(a.goal, c);
  }
}

const lignes = [];
lignes.push(`# Releve des buts — scenario \`${scenario.name}\`, ${TICKS} ticks`);
lignes.push("");
lignes.push(`Reference : \`${REF}\`. Produit par \`tools/migration/trace-goals.mjs\`.`);
lignes.push("");
lignes.push("| But | Ticks-habitant | Choix |");
lignes.push("|---|---|---|");
for (const [but, c] of [...parBut.entries()].sort((x, y) => y[1].ticks - x[1].ticks)) {
  lignes.push(`| ${but} | ${c.ticks} | ${c.choix} |`);
}
lignes.push("");
lignes.push("## Changements de but");
lignes.push("");
lignes.push("| Tick | Jour | Habitant | But | Activite | Porte | Cible |");
lignes.push("|---|---|---|---|---|---|---|");
lignes.push(...changements);
lignes.push("");
lignes.push("## Batiments a la fin");
lignes.push("");
for (const b of sim.buildings) {
  lignes.push(`- ${b.id} ${b.type} (${b.x}, ${b.y}) progress ${b.progress} stock ${JSON.stringify(b.stock || {})}`);
}
const texte = lignes.join("\n") + "\n";
if (OUT) fs.writeFileSync(OUT, texte);
else process.stdout.write(texte);
console.error(`buts : ${[...parBut.keys()].join(", ")} ; ${changements.length} changements`);
