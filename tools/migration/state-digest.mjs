// EMPREINTE CANONIQUE D'UN ETAT DE SIMULATION — cote JS.
//
// Le portage doit pouvoir repondre a une seule question: "a quel tick les deux
// simulations ont-elles cesse d'etre la meme ?". Les vecteurs de parite par
// fonction ne le disent pas — ils testent une fonction pure, pas huit mille
// lignes d'etat qui s'influencent. Il faut une empreinte de l'etat entier,
// calculee des deux cotes, comparable tick par tick.
//
// Ce fichier EST la specification. `Source/AnastasisSim/Public/Core/
// AnastasisStateDigest.h` la reimplemente en C++, et le test
// `Anastasis.Sim.Empreinte.*` prouve que les deux produisent les memes bits sur
// une batterie de valeurs generee ici. Sans cette preuve, une divergence
// d'empreinte serait ambigue: la simulation a-t-elle devie, ou seulement le
// hacheur ? Le harnais ne vaut que si cette question ne se pose jamais.
//
// --- LE FORMAT ------------------------------------------------------------
//
// Un flux d'octets typees, hache en FNV-1a 64 bits. Chaque valeur commence par
// son etiquette:
//
//   0x01  null
//   0x02  false
//   0x03  true
//   0x04  nombre    + 8 octets IEEE754 petit-boutiste (motif binaire brut)
//   0x05  chaine    + longueur uint32 LE + octets UTF-8
//   0x06  tableau   + longueur uint32 LE + chaque element
//   0x07  objet     + nombre de cles uint32 LE + (cle encodee, valeur encodee)*
//   0x08  undefined (uniquement dans un tableau — voir plus bas)
//
// Trois choix qui ne sont pas negociables, et pourquoi:
//
// 1. LES NOMBRES PAR LEUR MOTIF BINAIRE, jamais par leur ecriture decimale.
//    C'est la meme regle que les vecteurs de parite: un litteral decimal perd
//    le dernier bit, et c'est exactement ce bit qu'on surveille. Un ulp
//    d'ecart sur une distance fait basculer un `dist < radius`, donc une
//    decision de PNJ.
//
// 2. LES CLES D'OBJET TRIEES par unite de code UTF-16 croissante. L'ordre
//    d'insertion d'un objet JS n'a pas d'equivalent en C++, et il depend de
//    l'ordre d'ecriture du code, pas de l'etat. Le trier rend l'empreinte
//    independante de la forme du code des deux cotes.
//
// 3. LES CLES DE VALEUR `undefined` SONT OMISES d'un objet. `{a: undefined}`
//    et `{}` decrivent le meme etat — JSON les confond deja, et le C++ n'a
//    aucun moyen de les distinguer. Dans un TABLEAU en revanche, un trou
//    compte: il decale les indices. D'ou l'etiquette 0x08.
//
// NaN est normalise en 0x7ff8000000000000: V8 peut porter plusieurs charges
// utiles pour un meme NaN, et le C++ aussi. `-0` en revanche garde ses bits —
// il se distingue de `+0`, et la doctrine du projet est qu'un bit qui bouge
// est un bit qui compte. Si cela produit un faux positif un jour, ce sera une
// decision a prendre explicitement, pas un choix par defaut a subir.

const TAG_NULL = 0x01;
const TAG_FALSE = 0x02;
const TAG_TRUE = 0x03;
const TAG_NUMBER = 0x04;
const TAG_STRING = 0x05;
const TAG_ARRAY = 0x06;
const TAG_OBJECT = 0x07;
const TAG_UNDEFINED = 0x08;

export const DIGEST_SPEC_VERSION = 1;

const NAN_BITS = new Uint8Array([0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x7f]);
const scratch = new DataView(new ArrayBuffer(8));
const encoder = new TextEncoder();

// FNV-1a 64 bits, tenu en deux moities de 32 bits.
//
// Pourquoi pas BigInt: le harnais hache des centaines de milliers d'etats de
// 200 Ko. Un BigInt par octet rendrait la mesure trop lente pour etre faite,
// et une preuve qu'on ne lance pas ne prouve rien. Les produits partiels
// ci-dessous tiennent tous sous 2^53, donc ils sont exacts en double.
const PRIME_LO = 0x1b3;
const PRIME_HI = 0x100;
const TWO32 = 4294967296;

export class Digest {
  constructor() {
    this.hi = 0xcbf29ce4;
    this.lo = 0x84222325;
  }

  byte(b) {
    const lo = (this.lo ^ (b & 0xff)) >>> 0;
    // (hi:lo) * (PRIME_HI:PRIME_LO) mod 2^64
    const loProd = (lo & 0xffff) * PRIME_LO + ((lo >>> 16) * PRIME_LO) * 65536;
    const newLo = loProd % TWO32;
    const carry = (loProd - newLo) / TWO32;
    this.lo = newLo;
    this.hi = (this.hi * PRIME_LO + lo * PRIME_HI + carry) % TWO32;
    return this;
  }

  bytes(arr) {
    for (let i = 0; i < arr.length; i += 1) this.byte(arr[i]);
    return this;
  }

  uint32(n) {
    this.byte(n & 0xff).byte((n >>> 8) & 0xff).byte((n >>> 16) & 0xff).byte((n >>> 24) & 0xff);
    return this;
  }

  number(v) {
    this.byte(TAG_NUMBER);
    if (Number.isNaN(v)) return this.bytes(NAN_BITS);
    scratch.setFloat64(0, v, true);
    for (let i = 0; i < 8; i += 1) this.byte(scratch.getUint8(i));
    return this;
  }

  string(s) {
    const utf8 = encoder.encode(s);
    this.byte(TAG_STRING).uint32(utf8.length);
    return this.bytes(utf8);
  }

  // Encode une valeur quelconque issue de `serialize(sim)`.
  value(v) {
    if (v === null) return this.byte(TAG_NULL);
    if (v === undefined) return this.byte(TAG_UNDEFINED);
    switch (typeof v) {
      case "boolean": return this.byte(v ? TAG_TRUE : TAG_FALSE);
      case "number": return this.number(v);
      case "string": return this.string(v);
      case "bigint": return this.string(v.toString());
      case "function": return this.byte(TAG_NULL); // n'a pas d'etat
      default: break;
    }
    if (Array.isArray(v)) {
      this.byte(TAG_ARRAY).uint32(v.length);
      for (let i = 0; i < v.length; i += 1) this.value(v[i]);
      return this;
    }
    if (v instanceof Map) return this.value([...v.entries()]);
    if (v instanceof Set) return this.value([...v.values()]);
    if (ArrayBuffer.isView(v)) {
      this.byte(TAG_ARRAY).uint32(v.length);
      for (let i = 0; i < v.length; i += 1) this.number(v[i]);
      return this;
    }
    const keys = Object.keys(v).filter((k) => v[k] !== undefined).sort();
    this.byte(TAG_OBJECT).uint32(keys.length);
    for (const k of keys) {
      this.string(k);
      this.value(v[k]);
    }
    return this;
  }

  hex() {
    return this.hi.toString(16).padStart(8, "0") + this.lo.toString(16).padStart(8, "0");
  }
}

/** Empreinte d'une valeur isolee. */
export function digestValue(v) {
  return new Digest().value(v).hex();
}

/**
 * Empreinte d'un etat complet, section par section.
 *
 * Une empreinte globale unique dirait "ca a divergé", pas "quoi". Le cout d'une
 * empreinte par section de premier niveau est nul a cote de ce qu'elle fait
 * gagner: le rapport nomme `actors` ou `economy` au lieu de laisser fouiller
 * 200 Ko d'etat.
 *
 * L'empreinte globale n'est PAS le hachage de l'etat entier mais celui de la
 * suite (cle, empreinte de section): deux etats qui ont les memes sections ont
 * le meme global, et la relation reste verifiable a la main.
 */
export function digestState(state) {
  const sections = {};
  const keys = Object.keys(state).filter((k) => state[k] !== undefined).sort();
  const global = new Digest();
  for (const k of keys) {
    const h = digestValue(state[k]);
    sections[k] = h;
    global.string(k).string(h);
  }
  return { global: global.hex(), sections };
}

/**
 * Empreinte element par element d'une section-tableau, pour le second passage.
 *
 * On ne l'enregistre pas a chaque tick: sur un tableau de cent acteurs c'est
 * cent fois plus de trace pour une information dont on n'a besoin qu'une fois,
 * au tick ou la divergence apparait. Le harnais fait donc deux passes — la
 * trace large d'abord, le forage ensuite — et c'est la meme regle que partout
 * ici: une preuve se refait.
 */
export function digestElements(arr, idKey = "id") {
  if (!Array.isArray(arr)) return null;
  return arr.map((el, i) => ({
    i,
    id: el && typeof el === "object" && el[idKey] !== undefined ? el[idKey] : null,
    h: digestValue(el),
  }));
}
