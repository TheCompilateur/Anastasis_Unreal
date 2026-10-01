// VECTEURS DU LECTEUR DE SCENARIO — l'empreinte JS du tick 0, pour le C++.
//
// `Anastasis.Sim.Harnais.Lecture` lit un scenario (sauvegarde JS) en etat C++,
// le reprojette sur les sections de `serialize`, et compare les empreintes a
// celles que la REFERENCE calcule au tick 0. Ces empreintes viennent d'ici, et
// pas du fichier relu par le C++: un C++ qui hacherait le texte de travers
// n'aurait aucune chance de retomber sur des valeurs produites independamment.
//
// Le tick 0 est celui de l'emetteur (emit-state-digests.mjs -scenario):
// `deserialize(scenario.save)` dans un processus neuf, puis `serialize`, puis
// `digestState`. Le scenario est construit au point fixe de cet aller-retour.
//
//   node tools/migration/gen-scenario-vectors.mjs -ref <tag> [-scenario tools/migration/scenarios/endurance.json]
//
// Ecrit Source/AnastasisSim/Private/Tests/AnastasisScenarioVectors.inl (ou -out).

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

import { digestState } from "./state-digest.mjs";
import { chargerReference } from "./scenarios/masks.mjs";
import { chargerScenario, provenanceReference } from "./scenarios/scenario-format.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");
const argv = process.argv.slice(2);
const argOf = (flag, fallback) => {
  const i = argv.indexOf(flag);
  return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : fallback;
};
const REF = argOf("-ref", "C:/dev/Jeux IV Kingdoms").split(String.fromCharCode(92)).join("/");
const SCENARIO = argOf("-scenario", join(ICI, "scenarios", "endurance.json"));
const SORTIE = argOf("-out", join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisScenarioVectors.inl"));

const scenario = chargerScenario(SCENARIO);
const provenance = provenanceReference(REF);
if (provenance.modifie && !argv.includes("-allow-dirty")) {
  console.error(`REFUS: ${REF} porte ${provenance.modifie} modification(s) non commitee(s) sous src/.`);
  process.exit(3);
}
if (scenario.reference?.commit && provenance.commit !== scenario.reference.commit) {
  console.error(`REFUS: scenario construit contre ${scenario.reference.commit}, -ref est ${provenance.commit}.`);
  process.exit(3);
}

const ref = await chargerReference(REF);
const sim = new ref.Simulation({ deferred: true, seed: scenario.seed });
ref.deserialize(sim, JSON.parse(JSON.stringify(scenario.save)));
const etat = ref.serialize(sim);
const { global, sections } = digestState(etat);

const perimetre = new Set(scenario.sections);
const chemin = relative(RACINE, SCENARIO).split(String.fromCharCode(92)).join("/");
const L = [];
const o = (s = "") => L.push(s);
o("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
o("// Source: tools/migration/gen-scenario-vectors.mjs");
o(`// Scenario: ${chemin} (${scenario.name}, empreinte ${scenario.empreinte})`);
o(`// Reference: ${provenance.tag ?? "-"} @ ${provenance.court}`);
o("//");
o("// Empreintes de `serialize` au tick 0 de la REFERENCE: deserialize(scenario.save)");
o("// dans un processus neuf, puis serialize, puis digestState (state-digest.mjs).");
o("// Si un cas ne passe plus: soit le lecteur C++ a devie, soit le scenario a ete");
o("// reconstruit et il faut regenerer. Ne jamais corriger une valeur a la main.");
o("");
o("// clang-format off");
o("");
o(`static const TCHAR* const ScenarioPath = TEXT("${chemin}");`);
o(`static const TCHAR* const ScenarioName = TEXT("${scenario.name}");`);
o(`static const TCHAR* const ScenarioEmpreinte = TEXT("${scenario.empreinte}");`);
o(`static constexpr int32 ScenarioW = ${etat.w};`);
o(`static constexpr int32 ScenarioH = ${etat.h};`);
o(`static constexpr int32 ScenarioTileDiffRows = ${etat.tileDiff.length};`);
o(`static constexpr int32 ScenarioBuildings = ${etat.buildings.length};`);
o(`static constexpr int32 ScenarioActors = ${etat.actors.length};`);
o(`static constexpr uint64 ScenarioGlobalDigest = 0x${global}ull;`);
o("");
o("struct FScenarioSection");
o("{");
o("\tconst TCHAR* Name;");
o("\tuint64 Digest;");
o("\t/** Dans le perimetre du scenario (`sections`). */");
o("\tbool bPerimetre;");
o("};");
o("");
o("static const FScenarioSection ScenarioSections[] =");
o("{");
for (const k of Object.keys(sections).sort()) {
  o(`\t{ TEXT("${k}"), 0x${sections[k]}ull, ${perimetre.has(k) ? "true" : "false"} },`);
}
o("};");
o("");
o("// clang-format on");

mkdirSync(dirname(SORTIE), { recursive: true });
writeFileSync(SORTIE, L.join("\n") + "\n", "utf8");
console.error(`Ecrit: ${SORTIE} — ${Object.keys(sections).length} sections (${perimetre.size} au perimetre), global ${global}`);
