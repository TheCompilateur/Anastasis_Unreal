// ATELIER DE VECTEURS DE PARITE — declarer au lieu d'ecrire.
//
// Trois modules portes, trois generateurs ecrits a la main. Pour les 198 qui
// restent, ce n'est pas une methode: c'est un cout fixe par module, paye en
// entier a chaque fois, et une occasion de plus de se tromper dans le
// harnais plutot que dans le portage.
//
// Ici, un module se declare. On dit quelle fonction JS appeler, avec quels
// types d'arguments, et sur quelles entrees. L'atelier va chercher la
// reference, l'appelle telle quelle, et emet le `.inl` que le test C++ relit.
//
// Ce qui ne change pas, et qui est tout l'interet:
//
//   - les doubles sortent par leur MOTIF BINAIRE, jamais en decimal — un
//     litteral decimal perdrait le dernier bit, et c'est ce bit qu'on teste;
//   - la valeur attendue vient de la reference EXECUTEE, jamais d'une valeur
//     recopiee a la main;
//   - le fichier emis porte sa provenance et l'interdiction de le retoucher.
//
// Limite assumee: les arguments et les retours sont des scalaires (double,
// entier, booleen, chaine) ou un enregistrement de scalaires. Les fonctions a
// etat — un A*, un hacheur, une boucle de tick — demandent un generateur
// dedie, et c'est normal: leur difficulte est ailleurs que dans la plomberie.

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { pathToFileURL } from "node:url";
import { execFileSync } from "node:child_process";

// --- Encodage des valeurs ---------------------------------------------------

const vue = new DataView(new ArrayBuffer(8));

/** Motif binaire d'un double, en litteral C++ non ambigu. */
export function bits(valeur) {
  vue.setFloat64(0, valeur, true);
  return "0x" + vue.getBigUint64(0, true).toString(16).padStart(16, "0") + "ull";
}

/**
 * Litteral C++ d'une chaine UTF-8, en echappements \x pour rester ASCII pur.
 *
 * Le `.inl` reste ainsi insensible a l'encodage avec lequel un editeur le
 * rouvrira — et les chaines de la reference contiennent du grec et des accents.
 *
 * C'est un `const ANSICHAR*`, pas un `const TCHAR*`, et ce n'est pas un detail:
 * `UTF8_TO_TCHAR` construit un objet de conversion TEMPORAIRE. Dans un
 * initialiseur statique, ce temporaire meurt a la fin de l'expression et la
 * table ne garde qu'un pointeur pendouillant — en pratique, des chaines vides.
 * Le piege a coute une passe de tests: les vecteurs comparaient "" a "normal"
 * et accusaient le portage. La table porte donc les octets, et la conversion se
 * fait au point d'usage, dans le corps du test, ou le temporaire vit assez
 * longtemps.
 */
export function chaineCpp(texte) {
  const utf8 = new TextEncoder().encode(String(texte));
  let out = "";
  for (const b of utf8) {
    if (b >= 0x20 && b < 0x7f && b !== 0x22 && b !== 0x5c) out += String.fromCharCode(b);
    else out += "\\x" + b.toString(16).padStart(2, "0");
  }
  return `"${out}"`;
}

/** Un type declare -> comment il s'ecrit en C++, et comment on l'y relit. */
const TYPES = {
  double: {
    champ: (nom) => `uint64 ${nom}Bits`,
    valeur: (v) => bits(Number(v)),
    lecture: (nom) => `FromBits(Vecteur.${nom}Bits)`,
  },
  int: {
    champ: (nom) => `int32 ${nom}`,
    valeur: (v) => `${Math.trunc(Number(v))}`,
    lecture: (nom) => `Vecteur.${nom}`,
  },
  bool: {
    champ: (nom) => `int32 ${nom}`,
    valeur: (v) => (v ? "1" : "0"),
    lecture: (nom) => `Vecteur.${nom} != 0`,
  },
  string: {
    // Octets UTF-8. La conversion en TCHAR se fait dans le test — voir
    // `chaineCpp` pour la raison, elle a deja mordu une fois.
    champ: (nom) => `const ANSICHAR* ${nom}`,
    valeur: (v) => chaineCpp(v),
    lecture: (nom) => `UTF8_TO_TCHAR(Vecteur.${nom})`,
  },
};

function typeDe(nom) {
  const t = TYPES[nom];
  if (!t) throw new Error(`type inconnu: ${nom} (connus: ${Object.keys(TYPES).join(", ")})`);
  return t;
}

// --- Emission ---------------------------------------------------------------

/** Un cas declare -> son struct, sa table, et la valeur attendue par entree. */
function emettreCas(mod, cas, lignes) {
  const emit = (s = "") => lignes.push(s);
  const argTypes = (cas.args ?? []).map(typeDe);
  const retour = normaliserRetour(cas.ret);

  const champs = [];
  argTypes.forEach((t, i) => champs.push(t.champ(`A${i}`)));
  for (const f of retour) champs.push(typeDe(f.type).champ(`Attendu${majuscule(f.name)}`));

  emit(`// ${cas.name} — ${cas.comment ?? cas.jsName ?? ""}`.trimEnd());
  emit(`struct F${cas.name}Vector { ${champs.join("; ")}; };`);
  emit(`static const F${cas.name}Vector ${cas.name}Vectors[] = {`);

  for (const entree of cas.inputs) {
    const args = Array.isArray(entree) ? entree : [entree];
    if (args.length !== argTypes.length) {
      throw new Error(`${cas.name}: ${args.length} arguments pour ${argTypes.length} types declares`);
    }
    const produit = cas.call(mod, args);
    const cellules = [];
    argTypes.forEach((t, i) => cellules.push(t.valeur(args[i])));
    for (const f of retour) {
      const valeur = retour.length === 1 && f.name === "" ? produit : produit?.[f.name];
      if (valeur === undefined) {
        throw new Error(`${cas.name}: la reference n'a pas rendu de champ "${f.name}"`);
      }
      cellules.push(typeDe(f.type).valeur(valeur));
    }
    emit(`\t{ ${cellules.join(", ")} },`);
  }
  emit("};");
  emit("");
}

function normaliserRetour(ret) {
  if (typeof ret === "string") return [{ name: "", type: ret }];
  if (Array.isArray(ret)) return ret;
  throw new Error("ret doit etre un type ou une liste de { name, type }");
}

const majuscule = (s) => (s ? s[0].toUpperCase() + s.slice(1) : "");

/**
 * Genere le `.inl` d'une declaration.
 *
 * @param spec { module, out, cases, ref? }
 */
export async function genererVecteurs(spec, options = {}) {
  const REF = (options.ref ?? spec.ref ?? "C:/dev/Jeux IV Kingdoms")
    .split(String.fromCharCode(92)).join("/");
  const mod = await import(pathToFileURL(join(REF, spec.module)).href);

  let tete = "inconnu";
  try {
    tete = execFileSync("git", ["-C", REF, "log", "-1", "--format=%h"], { encoding: "utf8" }).trim();
  } catch { /* depot sans git: la provenance sera moins precise */ }

  const lignes = [];
  const emit = (s = "") => lignes.push(s);
  emit("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
  emit("// Source: tools/migration/gen-parity.mjs");
  emit(`// Declaration: ${spec.declaration ?? "(inconnue)"}`);
  emit(`// Reference: ${spec.module} @ ${tete}`);
  emit("//");
  emit("// Les valeurs attendues viennent de la reference EXECUTEE, et les doubles de");
  emit("// leur motif binaire: un litteral decimal perdrait le dernier bit, et c'est");
  emit("// ce bit qu'on teste.");
  emit("//");
  emit("// Si un cas ne passe plus: soit le portage a devie, soit la reference a change");
  emit("// et il faut regenerer. Ne jamais corriger une valeur attendue a la main.");
  emit("");
  emit("// clang-format off");
  emit("");

  for (const cas of spec.cases) {
    emettreCas(mod, cas, lignes);
  }

  const sortie = options.out ?? spec.out;
  mkdirSync(dirname(sortie), { recursive: true });
  writeFileSync(sortie, lignes.join("\n") + "\n", "utf8");

  const total = spec.cases.reduce((n, c) => n + c.inputs.length, 0);
  return { sortie, cas: spec.cases.length, vecteurs: total, tete };
}

/**
 * Produit cartesien, pour couvrir une grille d'entrees sans l'ecrire a la main.
 *
 *   croiser([0, 0.5, 1], ["near", "far"])  ->  [[0,"near"], [0,"far"], ...]
 */
export function croiser(...listes) {
  return listes.reduce(
    (acc, liste) => acc.flatMap((prefixe) => liste.map((v) => [...prefixe, v])),
    [[]],
  );
}
