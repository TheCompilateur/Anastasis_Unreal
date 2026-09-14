// Vecteurs du cache de chemins: un SCENARIO, pas une table de scalaires.
//
// L'ordre d'insertion, l'eviction, la purge a la lecture: rien de tout cela ne
// se met en vecteurs entree/sortie. Ce qui se met en vecteurs, c'est une SUITE
// D'OPERATIONS et ce que la reference rend a chacune — plus l'ordre final des
// cles, qui est l'etat qu'on ne verrait pas autrement.
//
// Pourquoi l'ordre compte, et ce n'est pas une precaution de style: l'eviction
// retire les 80 premieres cles dans l'ordre d'insertion d'une `Map` JS. Un
// portage sur `TMap`, sans ordre, evincerait d'autres entrees — donc servirait
// d'autres chemins, donc enverrait des habitants ailleurs.
//
//   node tools/migration/gen-nav-cache-vectors.mjs

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { bits, chaineCpp } from "./parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");

const argv = process.argv.slice(2);
const argOf = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : d; };
const REF = argOf("-ref", "C:/dev/Jeux IV Kingdoms").split(String.fromCharCode(92)).join("/");
const SORTIE = argOf("-out", join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisNavCacheVectors.inl"));

const mod = await import(pathToFileURL(join(REF, "src/sim/navService.js")).href);
const { lookupCachedPath, storeCachedPath, sweepNavCache, createNavService,
        navCacheTtlForSpeed, NAV_ZONE } = mod;

// --- Le scenario ------------------------------------------------------------
// Chaque operation exerce un mecanisme nomme. Un scenario qui ne ferait que
// ranger et relire ne prouverait rien de ce qui peut mal tourner.

const OPS = [];
const store = (sx, sy, tx, ty, len) => OPS.push({ op: "store", sx, sy, tx, ty, len });
const lookup = (sx, sy, tx, ty) => OPS.push({ op: "lookup", sx, sy, tx, ty });
const sweep = () => OPS.push({ op: "sweep" });
const temps = (t) => OPS.push({ op: "time", t });
const version = (v) => OPS.push({ op: "version", v });

// 1. Ranger, puis retrouver exactement.
store(4.5, 4.5, 40.5, 12.5, 6);
lookup(4.5, 4.5, 40.5, 12.5);

// 2. Meme zone, cellule differente: succes par zone si le depart est proche du
//    premier noeud du chemin range.
lookup(5.5, 4.5, 40.5, 12.5);
lookup(6.5, 6.5, 40.5, 12.5);

// 3. Meme zone mais loin du premier noeud: la zone ne doit PAS servir, sinon
//    l'habitant commencerait par revenir sur ses pas.
store(0.5, 0.5, 63.5, 63.5, 4);
lookup(7.5, 7.5, 63.5, 63.5);

// 4. Cible differente: rien en cache.
lookup(4.5, 4.5, 1.5, 1.5);

// 5. Le temps passe sous le TTL: toujours frais.
temps(19);
lookup(4.5, 4.5, 40.5, 12.5);

// 6. Le temps depasse le TTL: perime, et la lecture purge.
temps(21);
lookup(4.5, 4.5, 40.5, 12.5);

// 7. On range a nouveau, puis la version du monde bouge: tout est invalide.
temps(22);
store(4.5, 4.5, 40.5, 12.5, 6);
lookup(4.5, 4.5, 40.5, 12.5);
version(1);
lookup(4.5, 4.5, 40.5, 12.5);

// 8. Balayage: ce qui reste de l'ancienne version doit partir.
sweep();

// 9. Eviction.
//
//    Chaque `store` pose DEUX cles — exacte et zone — mais elles ne comptent
//    double que si elles sont DISTINCTES. Une premiere version de ce scenario
//    gardait la meme cible pour tous les rangements: les cles de zone se
//    confondaient, le cache plafonnait a 268 entrees, et aucune eviction ne se
//    declenchait. Le garde-fou du test C++ l'a signale — c'est exactement ce
//    pour quoi il existe.
//
//    La cible varie donc avec le rang, ce qui rend les deux cles distinctes a
//    chaque coup: 260 rangements posent 520 cles, franchissent le seuil de 480
//    et declenchent le retrait des 80 plus anciennes.
temps(30);
for (let i = 0; i < 260; i += 1) {
  const sx = (i % 61) + 0.5;
  const sy = Math.floor(i / 61) + 0.5;
  const tx = (i % 50) + 0.5;
  const ty = Math.floor(i / 50) + 0.5;
  store(sx, sy, tx, ty, 3);
}

// 10. Apres eviction, les plus anciennes cles ont disparu et les recentes non.
lookup(0.5, 0.5, 0.5, 0.5);
lookup(60.5, 3.5, 9.5, 5.5);
sweep();

// --- Execution contre la reference ------------------------------------------

const sim = {
  navVersion: 0,
  time: 0,
  speedScale: 1,
  navService: createNavService(),
};
const TTL = navCacheTtlForSpeed(1);

/** Chemin synthetique: le premier noeud est la case de depart. */
const cheminDe = (sx, sy, len) =>
  Array.from({ length: len }, (_, i) => ({ x: Math.floor(sx) + 0.5 + i, y: Math.floor(sy) + 0.5 }));

const resultats = [];
for (const o of OPS) {
  let hit = 0;
  let longueur = 0;
  let premierX = 0;
  let premierY = 0;
  let retire = 0;

  switch (o.op) {
    case "store":
      storeCachedPath(sim, { x: o.sx, y: o.sy }, { x: o.tx, y: o.ty }, cheminDe(o.sx, o.sy, o.len));
      break;
    case "lookup": {
      const p = lookupCachedPath(sim, { x: o.sx, y: o.sy }, { x: o.tx, y: o.ty });
      if (p) {
        hit = 1;
        longueur = p.length;
        if (p.length) { premierX = p[0].x; premierY = p[0].y; }
      }
      break;
    }
    case "sweep":
      retire = sweepNavCache(sim);
      break;
    case "time":
      sim.time = o.t;
      break;
    case "version":
      sim.navVersion = o.v;
      break;
    default:
      throw new Error(`operation inconnue: ${o.op}`);
  }

  resultats.push({ hit, longueur, premierX, premierY, retire, taille: sim.navService.cache.size });
}

const ordreFinal = [...sim.navService.cache.keys()];

// --- Emission ---------------------------------------------------------------

const CODES = { store: 0, lookup: 1, sweep: 2, time: 3, version: 4 };

const L = [];
const emit = (s = "") => L.push(s);

emit("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
emit("// Source: tools/migration/gen-nav-cache-vectors.mjs");
emit("//");
emit("// Un SCENARIO, pas une table de scalaires: l'ordre d'insertion, l'eviction");
emit("// et la purge a la lecture ne se mettent pas en entree/sortie. On rejoue la");
emit("// meme suite d'operations des deux cotes et on compare ce que chacune rend,");
emit("// plus l'ordre final des cles — l'etat qu'on ne verrait pas autrement.");
emit("");
emit("// clang-format off");
emit("");
emit(`static const uint64 NavCacheTtlBits = ${bits(TTL)};`);
emit(`static constexpr int32 NavCacheZone = ${NAV_ZONE};`);
emit("");
emit("enum class ENavCacheOp : uint8 { Store = 0, Lookup = 1, Sweep = 2, Time = 3, Version = 4 };");
emit("");
emit("struct FNavCacheOp {");
emit("\tENavCacheOp Op;");
emit("\tuint64 SXBits; uint64 SYBits; uint64 TXBits; uint64 TYBits;");
emit("\tint32 PathLen; uint64 TimeBits; int32 Version;");
emit("\t// Ce que la reference a rendu:");
emit("\tint32 Hit; int32 OutLen; uint64 FirstXBits; uint64 FirstYBits; int32 Removed; int32 Size;");
emit("};");
emit("static const FNavCacheOp NavCacheOps[] = {");
OPS.forEach((o, i) => {
  const r = resultats[i];
  const nom = ["Store", "Lookup", "Sweep", "Time", "Version"][CODES[o.op]];
  emit(`\t{ ENavCacheOp::${nom}, ` +
    `${bits(o.sx ?? 0)}, ${bits(o.sy ?? 0)}, ${bits(o.tx ?? 0)}, ${bits(o.ty ?? 0)}, ` +
    `${o.len ?? 0}, ${bits(o.t ?? 0)}, ${o.v ?? 0}, ` +
    `${r.hit}, ${r.longueur}, ${bits(r.premierX)}, ${bits(r.premierY)}, ${r.retire}, ${r.taille} },`);
});
emit("};");
emit("");
emit("// L'ordre final des cles. C'est lui que `TMap` seul ne saurait pas tenir.");
emit(`static constexpr int32 NavCacheFinalOrderCount = ${ordreFinal.length};`);
emit("static const ANSICHAR* const NavCacheFinalOrder[] = {");
for (const k of ordreFinal) emit(`\t${chaineCpp(k)},`);
emit("};");

mkdirSync(dirname(SORTIE), { recursive: true });
writeFileSync(SORTIE, L.join("\n") + "\n", "utf8");
console.error(`Ecrit: ${SORTIE} — ${OPS.length} operations, ${ordreFinal.length} cles finales`);
