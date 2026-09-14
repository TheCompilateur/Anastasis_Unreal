// Vecteurs de parite de l'empreinte: JS -> C++.
//
// Le harnais differentiel ne vaut que si les deux cotes hachent identiquement.
// Sinon "premier tick divergent: 812" serait ambigu — la simulation a-t-elle
// devie, ou seulement le hacheur ? Ces vecteurs retirent la question.
//
// Une batterie, deux sorties. Chaque cas est declare UNE fois, en instructions
// pour l'ecrivain d'etat; le generateur en deduit (a) la valeur JS, qu'il hache
// avec `state-digest.mjs` — le meme code que le harnais — et (b) la fonction
// C++ qui rejoue les memes instructions. Aucune transcription a la main entre
// les deux, donc aucune occasion de les laisser diverger en silence.
//
//   node tools/migration/gen-digest-vectors.mjs
//
// Les doubles sont emis par leur motif binaire: un litteral decimal perdrait le
// dernier bit, et c'est ce bit qu'on teste.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { digestValue, DIGEST_SPEC_VERSION } from "./state-digest.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");
const SORTIE = process.argv[2]
  ? process.argv[2]
  : join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisDigestVectors.inl");

// --- La batterie ------------------------------------------------------------
// Instructions: Null, Bool(b), Number(x), String(s), BeginArray(n)/EndArray,
// BeginObject/Key(s)/EndObject.

const B = (nom, ops) => ({ nom, ops });

const BATTERIE = [
  B("scalaire.null", [["Null"]]),
  B("scalaire.vrai", [["Bool", true]]),
  B("scalaire.faux", [["Bool", false]]),

  B("nombre.zero", [["Number", 0]]),
  B("nombre.zero-negatif", [["Number", -0]]),
  B("nombre.un", [["Number", 1]]),
  B("nombre.demi", [["Number", 0.5]]),
  B("nombre.tiers", [["Number", 1 / 3]]),
  B("nombre.dt-du-harnais", [["Number", 1 / 30]]),
  B("nombre.negatif", [["Number", -273.15]]),
  B("nombre.tres-grand", [["Number", 1e21]]),
  B("nombre.tres-petit", [["Number", 5e-324]]),
  B("nombre.infini", [["Number", Infinity]]),
  B("nombre.infini-negatif", [["Number", -Infinity]]),
  B("nombre.nan", [["Number", NaN]]),
  // Un ulp d'ecart doit changer l'empreinte. C'est la raison d'etre du format.
  B("nombre.ulp-a", [["Number", 12.5]]),
  B("nombre.ulp-b", [["Number", nextUp(12.5)]]),

  B("chaine.vide", [["String", ""]]),
  B("chaine.ascii", [["String", "res:wood"]]),
  B("chaine.accents", [["String", "Theodoros Kalligas — fondateur"]]),
  B("chaine.grec", [["String", "ἀνάστασις"]]),
  B("chaine.hors-bmp", [["String", "\u{1F3DB}\u{FE0F}"]]),

  B("tableau.vide", [["BeginArray", 0], ["EndArray"]]),
  B("tableau.nombres", [["BeginArray", 3], ["Number", 1], ["Number", 2], ["Number", 3], ["EndArray"]]),
  B("tableau.mixte", [["BeginArray", 4], ["Null"], ["Bool", true], ["Number", -1.5], ["String", "x"], ["EndArray"]]),

  B("objet.vide", [["BeginObject"], ["EndObject"]]),
  B("objet.une-cle", [["BeginObject"], ["Key", "a"], ["Number", 1], ["EndObject"]]),
  // Ecrit dans le desordre: les deux cotes doivent trier et tomber sur la meme
  // empreinte que `objet.tri-b` ci-dessous, ecrit dans l'ordre.
  B("objet.tri-a", [["BeginObject"], ["Key", "b"], ["Number", 2], ["Key", "a"], ["Number", 1], ["EndObject"]]),
  B("objet.tri-b", [["BeginObject"], ["Key", "a"], ["Number", 1], ["Key", "b"], ["Number", 2], ["EndObject"]]),
  B("objet.cles-majuscules", [
    ["BeginObject"],
    ["Key", "Z"], ["Number", 1],
    ["Key", "a"], ["Number", 2],
    ["Key", "A"], ["Number", 3],
    ["EndObject"],
  ]),

  // Un acteur tel que `serialize(sim)` en rend: des doubles, des chaines, des
  // references par identifiant, des champs nuls.
  B("compose.acteur", [
    ["BeginObject"],
    ["Key", "id"], ["Number", 7],
    ["Key", "x"], ["Number", 41.328125],
    ["Key", "y"], ["Number", -12.75],
    ["Key", "job"], ["String", "job:woodcutter"],
    ["Key", "home"], ["Null"],
    ["Key", "hunger"], ["Number", 0.6180339887498949],
    ["Key", "carrying"], ["BeginArray", 2],
    ["BeginObject"], ["Key", "res"], ["String", "res:wood"], ["Key", "n"], ["Number", 3], ["EndObject"],
    ["BeginObject"], ["Key", "res"], ["String", "res:stone"], ["Key", "n"], ["Number", 1], ["EndObject"],
    ["EndArray"],
    ["Key", "alive"], ["Bool", true],
    ["EndObject"],
  ]),

  // Objet imbrique dans un tableau dans un objet: l'ordre de fermeture des
  // tampons est la partie de l'implementation C++ qui peut se tromper sans
  // qu'aucun cas simple ne le montre.
  B("compose.imbrication", [
    ["BeginObject"],
    ["Key", "z"], ["BeginArray", 2],
    ["BeginObject"], ["Key", "k"], ["Number", 1], ["EndObject"],
    ["BeginArray", 1], ["String", "profond"], ["EndArray"],
    ["EndArray"],
    ["Key", "a"], ["BeginObject"], ["Key", "b"], ["BeginObject"], ["Key", "c"], ["Number", 9], ["EndObject"], ["EndObject"],
    ["EndObject"],
  ]),
];

function nextUp(x) {
  const v = new DataView(new ArrayBuffer(8));
  v.setFloat64(0, x, true);
  v.setBigUint64(0, v.getBigUint64(0, true) + 1n, true);
  return v.getFloat64(0, true);
}

// --- Interpretation cote JS -------------------------------------------------
// On reconstruit la VALEUR, puis on la hache avec le meme `digestValue` que le
// harnais. Passer par un second hacheur "de test" reviendrait a tester une
// copie, pas ce qui tourne.

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
      case "Bool": poser(arg); break;
      case "Number": poser(arg); break;
      case "String": poser(arg); break;
      case "BeginArray": pile.push({ type: "tableau", valeurs: [], attendu: arg }); break;
      case "EndArray": {
        const f = pile.pop();
        if (f.valeurs.length !== f.attendu) throw new Error(`tableau: ${f.valeurs.length} elements pour ${f.attendu} annonces`);
        poser(f.valeurs);
        break;
      }
      case "BeginObject": pile.push({ type: "objet", obj: {}, cle: undefined }); break;
      case "Key": pile[pile.length - 1].cle = arg; break;
      case "EndObject": { const f = pile.pop(); poser(f.obj); break; }
      default: throw new Error(`instruction inconnue: ${op}`);
    }
  }
  if (pile.length !== 1) throw new Error("pile non refermee");
  if (pile[0].valeurs.length !== 1) throw new Error("un cas doit produire exactement une valeur");
  return pile[0].valeurs[0];
}

// --- Emission C++ -----------------------------------------------------------

const bits = (x) => {
  const v = new DataView(new ArrayBuffer(8));
  v.setFloat64(0, x, true);
  return "0x" + v.getBigUint64(0, true).toString(16).padStart(16, "0") + "ull";
};

/** Litteral C++ d'une chaine, en echappements \x pour rester ASCII pur. */
const litteralChaine = (s) => {
  const utf8 = new TextEncoder().encode(s);
  let out = "";
  for (const b of utf8) {
    if (b >= 0x20 && b < 0x7f && b !== 0x22 && b !== 0x5c) out += String.fromCharCode(b);
    else out += "\\x" + b.toString(16).padStart(2, "0");
  }
  // UTF8_TO_TCHAR relit les octets: le .inl reste ASCII, donc insensible a
  // l'encodage avec lequel un editeur le rouvrira.
  return `UTF8_TO_TCHAR("${out}")`;
};

const cpp = (ops) => {
  const lignes = [];
  let indent = 2;
  const pousser = (s) => lignes.push("\t".repeat(indent) + s);
  for (const [op, arg] of ops) {
    switch (op) {
      case "Null": pousser("W.Null();"); break;
      case "Bool": pousser(`W.Bool(${arg ? "true" : "false"});`); break;
      case "Number": pousser(`W.Number(FromBits(${bits(arg)}));`); break;
      case "String": pousser(`W.String(${litteralChaine(arg)});`); break;
      case "BeginArray": pousser(`W.BeginArray(${arg});`); indent += 1; break;
      case "EndArray": indent -= 1; pousser("W.EndArray();"); break;
      case "BeginObject": pousser("W.BeginObject();"); indent += 1; break;
      case "Key": pousser(`W.Key(${litteralChaine(arg)});`); break;
      case "EndObject": indent -= 1; pousser("W.EndObject();"); break;
      default: throw new Error(`instruction inconnue: ${op}`);
    }
  }
  return lignes;
};

const L = [];
const emit = (s = "") => L.push(s);

emit("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
emit("// Source: tools/migration/gen-digest-vectors.mjs");
emit("//");
emit("// Chaque cas est declare une seule fois, en instructions pour l'ecrivain");
emit("// d'etat. L'empreinte attendue vient de tools/migration/state-digest.mjs,");
emit("// c'est-a-dire du code que le harnais fait tourner pour de vrai.");
emit("//");
emit("// Si un cas ne passe plus: soit le C++ a devie, soit la specification JS a");
emit("// change et il faut regenerer. Ne jamais corriger une valeur attendue a la");
emit("// main pour faire passer un test.");
emit("");
emit(`static constexpr int32 DigestSpecVersion = ${DIGEST_SPEC_VERSION};`);
emit("");
emit("// clang-format off");
emit("");

const vus = new Map();
BATTERIE.forEach((cas, i) => {
  const valeur = valeurDe(cas.ops);
  const attendu = digestValue(valeur);
  vus.set(cas.nom, attendu);
  emit(`// ${cas.nom}`);
  emit(`static void BuildDigestVector${i}(AnastasisDigest::FStateWriter& W)`);
  emit("{");
  for (const l of cpp(cas.ops)) emit(l.replace(/^\t\t/, "\t"));
  emit("}");
  emit("");
});

emit("struct FDigestVector { const TCHAR* Name; void (*Build)(AnastasisDigest::FStateWriter&); uint64 Expected; };");
emit("static const FDigestVector DigestVectors[] = {");
BATTERIE.forEach((cas, i) => {
  emit(`\t{ TEXT("${cas.nom}"), &BuildDigestVector${i}, 0x${vus.get(cas.nom)}ull },`);
});
emit("};");
emit("");

// Invariants inscrits dans le fichier: ils doivent tenir des deux cotes, et le
// test C++ les verifie explicitement plutot que de les supposer.
emit("// Invariants verifies par le test, en plus des empreintes elles-memes.");
emit(`static const uint64 DigestTriDesordre = 0x${vus.get("objet.tri-a")}ull; // == objet.tri-b`);
emit(`static const uint64 DigestTriOrdre = 0x${vus.get("objet.tri-b")}ull;`);
emit(`static const uint64 DigestUlpA = 0x${vus.get("nombre.ulp-a")}ull;`);
emit(`static const uint64 DigestUlpB = 0x${vus.get("nombre.ulp-b")}ull; // != DigestUlpA`);

if (vus.get("objet.tri-a") !== vus.get("objet.tri-b")) {
  console.error("INCOHERENT: deux objets aux memes paires n'ont pas la meme empreinte.");
  process.exit(1);
}
if (vus.get("nombre.ulp-a") === vus.get("nombre.ulp-b")) {
  console.error("INCOHERENT: un ulp d'ecart ne change pas l'empreinte.");
  process.exit(1);
}

mkdirSync(dirname(SORTIE), { recursive: true });
writeFileSync(SORTIE, L.join("\n") + "\n", "utf8");
console.error(`Ecrit: ${SORTIE} — ${BATTERIE.length} vecteurs`);
