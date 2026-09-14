// Vecteurs de la trace d'empreintes: JS -> C++, et la trace JS de reference.
//
// Le harnais differentiel attend une trace des deux cotes. Celle du JS existe
// et tourne; celle d'Unreal n'existe pas, parce que le C++ n'a pas encore
// d'etat a projeter. Mais on peut prouver la MOITIE UNREAL des maintenant, sans
// simulation: si le C++ sait ecrire une trace qu'un etat synthetique rend
// identique a celle du JS pour le meme etat, alors brancher une vraie
// simulation dessus ne sera plus qu'un cablage.
//
// Deux sorties, d'une seule declaration:
//
//   1. `AnastasisTraceVectors.inl` — les empreintes attendues, plus la fonction
//      C++ qui rebatit chaque section. Aucune transcription a la main, donc
//      aucune occasion de derive entre les deux cotes.
//   2. `fixtures/trace-synthetique.jsonl` — la trace JS des memes echantillons,
//      ecrite par le MEME `digestState` que l'emetteur reel.
//
// La preuve de bout en bout est alors: le test C++ ecrit sa trace, et
// `compare-digests.mjs` — l'outil du harnais, pas un comparateur de test —
// declare les deux identiques.
//
//   node tools/migration/gen-trace-vectors.mjs

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { digestState, DIGEST_SPEC_VERSION } from "./state-digest.mjs";
import { bits, chaineCpp } from "./parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");
const SORTIE_INL = join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisTraceVectors.inl");
const SORTIE_JSONL = join(ICI, "fixtures", "trace-synthetique.jsonl");

// Conditions de la trace. `dt` et `dayLength` sont compares par le comparateur:
// ils doivent se relire a l'identique des deux cotes.
const GRAINE = 33344;
const DT = 1 / 30;
const DAY_LENGTH = 90;

// --- Les echantillons -------------------------------------------------------
// Declares en instructions pour l'ecrivain d'etat, comme les vecteurs
// d'empreinte: le generateur en tire la valeur JS ET le code C++ qui la rejoue.

const sample = (t, day, time, sections) => ({ t, day, time, sections });

const ECHANTILLONS = [
  sample(0, 1, 37.8, {
    clock: [
      ["BeginObject"],
      ["Key", "day"], ["Number", 1],
      ["Key", "time"], ["Number", 37.8],
      ["EndObject"],
    ],
    counters: [
      ["BeginArray", 3], ["Number", 0], ["Number", 0], ["Number", 0], ["EndArray"],
    ],
    village: [
      ["BeginObject"],
      ["Key", "name"], ["String", "Valmire"],
      ["Key", "founded"], ["Bool", false],
      ["Key", "treasury"], ["Number", 0],
      ["Key", "patron"], ["Null"],
      ["EndObject"],
    ],
  }),

  sample(1, 1, 37.83333333333333, {
    clock: [
      ["BeginObject"],
      ["Key", "day"], ["Number", 1],
      ["Key", "time"], ["Number", 37.83333333333333],
      ["EndObject"],
    ],
    counters: [
      ["BeginArray", 3], ["Number", 1], ["Number", 0], ["Number", 0], ["EndArray"],
    ],
    village: [
      ["BeginObject"],
      ["Key", "name"], ["String", "Valmire"],
      ["Key", "founded"], ["Bool", true],
      ["Key", "treasury"], ["Number", 12.5],
      ["Key", "patron"], ["String", "Theodoros Kalligas"],
      ["EndObject"],
    ],
  }),

  // Un echantillon qui traverse un minuit, avec des valeurs qui mettent le
  // format a l'epreuve: un tiers non representable, du texte non-ASCII, un
  // tableau imbrique.
  sample(2700, 2, 90.00000000000003, {
    clock: [
      ["BeginObject"],
      ["Key", "day"], ["Number", 2],
      ["Key", "time"], ["Number", 90.00000000000003],
      ["EndObject"],
    ],
    counters: [
      ["BeginArray", 3], ["Number", 2700], ["Number", 1], ["Number", 1 / 3], ["EndArray"],
    ],
    village: [
      ["BeginObject"],
      ["Key", "name"], ["String", "Valmire"],
      ["Key", "founded"], ["Bool", true],
      ["Key", "treasury"], ["Number", -0.1],
      ["Key", "patron"], ["String", "Θεόδωρος"],
      ["Key", "roads"], ["BeginArray", 2],
      ["BeginArray", 2], ["Number", 4], ["Number", 7], ["EndArray"],
      ["BeginArray", 2], ["Number", 9], ["Number", 2], ["EndArray"],
      ["EndArray"],
      ["EndObject"],
    ],
  }),
];

// --- Interpretation cote JS -------------------------------------------------

function valeurDe(ops) {
  const pile = [{ type: "racine", valeurs: [] }];
  const poser = (v) => {
    const haut = pile[pile.length - 1];
    if (haut.type === "objet") {
      if (haut.cle === undefined) throw new Error("valeur sans cle dans un objet");
      haut.obj[haut.cle] = v;
      haut.cle = undefined;
    } else {
      haut.valeurs.push(v);
    }
  };
  for (const [op, arg] of ops) {
    switch (op) {
      case "Null": poser(null); break;
      case "Bool": case "Number": case "String": poser(arg); break;
      case "BeginArray": pile.push({ type: "tableau", valeurs: [], attendu: arg }); break;
      case "EndArray": {
        const f = pile.pop();
        if (f.valeurs.length !== f.attendu) throw new Error(`tableau: ${f.valeurs.length} pour ${f.attendu} annonces`);
        poser(f.valeurs);
        break;
      }
      case "BeginObject": pile.push({ type: "objet", obj: {}, cle: undefined }); break;
      case "Key": pile[pile.length - 1].cle = arg; break;
      case "EndObject": { const f = pile.pop(); poser(f.obj); break; }
      default: throw new Error(`instruction inconnue: ${op}`);
    }
  }
  if (pile.length !== 1 || pile[0].valeurs.length !== 1) throw new Error("une section doit produire une valeur");
  return pile[0].valeurs[0];
}

// --- Emission C++ -----------------------------------------------------------

/**
 * Litteral de chaine dans un CORPS DE FONCTION.
 *
 * `chaineCpp` rend les octets UTF-8 nus, et c'est a l'appelant de dire comment
 * les relire. Dans une table statique on garde un `const ANSICHAR*` converti au
 * point d'usage; ici on est dans une fonction, donc `UTF8_TO_TCHAR` est valide
 * et NECESSAIRE: sans lui, `FString` relit les octets comme de l'ANSI et le
 * grec devient du charabia. Diagnostique par une section qui divergeait seule,
 * celle qui portait "Theodoros" en grec.
 */
const litteral = (v) => `UTF8_TO_TCHAR(${chaineCpp(v)})`;

const cppOps = (ops, indent = 1) => {
  const lignes = [];
  let niveau = indent;
  const pousser = (s) => lignes.push("\t".repeat(niveau) + s);
  for (const [op, arg] of ops) {
    switch (op) {
      case "Null": pousser("W.Null();"); break;
      case "Bool": pousser(`W.Bool(${arg ? "true" : "false"});`); break;
      case "Number": pousser(`W.Number(FromBits(${bits(arg)}));`); break;
      case "String": pousser(`W.String(${litteral(arg)});`); break;
      case "BeginArray": pousser(`W.BeginArray(${arg});`); niveau += 1; break;
      case "EndArray": niveau -= 1; pousser("W.EndArray();"); break;
      case "BeginObject": pousser("W.BeginObject();"); niveau += 1; break;
      case "Key": pousser(`W.Key(${litteral(arg)});`); break;
      case "EndObject": niveau -= 1; pousser("W.EndObject();"); break;
      default: throw new Error(`instruction inconnue: ${op}`);
    }
  }
  return lignes;
};

const L = [];
const emit = (s = "") => L.push(s);

emit("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
emit("// Source: tools/migration/gen-trace-vectors.mjs");
emit("//");
emit("// Empreintes attendues d'une trace synthetique, et le code qui rebatit");
emit("// chaque section. La trace JS des MEMES echantillons est dans");
emit("// tools/migration/fixtures/trace-synthetique.jsonl: le test C++ ecrit la");
emit("// sienne a cote, et compare-digests.mjs — l'outil du harnais — declare.");
emit("");
emit("// clang-format off");
emit("");
emit(`static constexpr int32 TraceSpecVersion = ${DIGEST_SPEC_VERSION};`);
emit(`static constexpr uint32 TraceSeed = ${GRAINE}u;`);
emit(`static const uint64 TraceDtBits = ${bits(DT)};`);
emit(`static const uint64 TraceDayLengthBits = ${bits(DAY_LENGTH)};`);
emit(`static constexpr int32 TraceSampleCount = ${ECHANTILLONS.length};`);
emit("");

const lignesJsonl = [];
lignesJsonl.push(JSON.stringify({
  kind: "header",
  spec: DIGEST_SPEC_VERSION,
  source: "js",
  ref: "(synthetique)",
  refHead: "synthetique",
  seed: GRAINE,
  dt: DT,
  ticks: ECHANTILLONS[ECHANTILLONS.length - 1].t,
  every: 1,
  dayLength: DAY_LENGTH,
  // Horodatage FIXE, et volontairement: cette trace est un temoin versionne.
  // Avec `new Date()`, chaque regeneration produirait un diff, et une preuve
  // qui change toute seule cesse d'etre une preuve. Le comparateur ne lit pas
  // ce champ.
  emittedAt: "1204-04-13T00:00:00.000Z",
}));

const nomsSections = [...new Set(ECHANTILLONS.flatMap((e) => Object.keys(e.sections)))].sort();
emit(`static const ANSICHAR* const TraceSectionNames[] = { ${nomsSections.map((n) => `"${n}"`).join(", ")} };`);
emit(`static constexpr int32 TraceSectionCount = ${nomsSections.length};`);
emit("");

ECHANTILLONS.forEach((ech, i) => {
  // L'etat JS, puis l'empreinte par le meme code que l'emetteur reel.
  const etat = {};
  for (const [nom, ops] of Object.entries(ech.sections)) etat[nom] = valeurDe(ops);
  const { global, sections } = digestState(etat);
  ech._global = global;
  ech._sections = sections;

  lignesJsonl.push(JSON.stringify({ t: ech.t, day: ech.day, time: ech.time, g: global, s: sections }));

  for (const nom of nomsSections) {
    emit(`// echantillon ${i}, section ${nom}`);
    emit(`static void BuildTraceSection${i}_${nom}(AnastasisDigest::FStateWriter& W)`);
    emit("{");
    for (const l of cppOps(ech.sections[nom])) emit(l);
    emit("}");
  }
  emit("");
});

emit("struct FTraceSampleVector {");
emit("\tint32 T; int32 Day; uint64 TimeBits;");
emit("\tconst ANSICHAR* GlobalHex;");
emit("\tconst ANSICHAR* SectionHex[TraceSectionCount];");
emit("\tvoid (*Build[TraceSectionCount])(AnastasisDigest::FStateWriter&);");
emit("};");
emit("static const FTraceSampleVector TraceSamples[] = {");
ECHANTILLONS.forEach((ech, i) => {
  const hex = nomsSections.map((n) => `"${ech._sections[n]}"`).join(", ");
  const build = nomsSections.map((n) => `&BuildTraceSection${i}_${n}`).join(", ");
  emit(`\t{ ${ech.t}, ${ech.day}, ${bits(ech.time)}, "${ech._global}", { ${hex} }, { ${build} } },`);
});
emit("};");

mkdirSync(dirname(SORTIE_INL), { recursive: true });
writeFileSync(SORTIE_INL, L.join("\n") + "\n", "utf8");
mkdirSync(dirname(SORTIE_JSONL), { recursive: true });
writeFileSync(SORTIE_JSONL, lignesJsonl.join("\n") + "\n", "utf8");

console.error(`Ecrit: ${SORTIE_INL} — ${ECHANTILLONS.length} echantillons, ${nomsSections.length} sections`);
console.error(`Ecrit: ${SORTIE_JSONL} — trace JS de reference`);
