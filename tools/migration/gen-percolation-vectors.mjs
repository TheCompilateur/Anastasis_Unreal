// Vecteurs de parite de la percolation: JS -> C++.
//
// `src/sim/percolation.js` repond a une question que l'A* repondait mal: cette
// case est-elle dans le MEME MONDE PIETONNIER que le hameau. Son en-tete porte
// la mesure qui l'a fait naitre (12/08/2026): des chantiers ouvraient au coeur
// d'un massif, cernes d'arbres debout, materiaux livres, et pas une piece posee
// pendant 55 jours — le batisseur ne pouvait pas s'approcher.
//
// Ce que ces vecteurs verifient, et pourquoi c'est plus qu'une comparaison de
// nombres:
//
//   - les IDENTIFIANTS de composante, pas seulement leur nombre. Ils sont
//     attribues dans l'ordre de balayage, et tout le reste s'y refere;
//   - la composante du hameau quand sa case est BLOQUEE — le camp est un
//     batiment. Le JS balaye alors des carres concentriques et prend la plus
//     grande composante voisine, les egalites tranchees par l'ordre de
//     parcours. Une boucle ecrite dans l'autre sens rendrait un autre hameau;
//   - les fractions de percolation, en double.
//
//   node tools/migration/gen-percolation-vectors.mjs

import { writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { chargerReference, monterNavSim, REF_PAR_DEFAUT } from "./js-nav-sim.mjs";
import { bits } from "./parity-kit.mjs";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..");

const argv = process.argv.slice(2);
const argOf = (f, d) => { const i = argv.indexOf(f); return i >= 0 && argv[i + 1] !== undefined ? argv[i + 1] : d; };
const REF = argOf("-ref", REF_PAR_DEFAUT);
const SORTIE = argOf("-out", join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisPercolationVectors.inl"));

const SEEDS = [33344, 7, 1204];
const W = 64;
const H = 64;

const mod = await chargerReference(REF);
const { walkComponents, settlementWalkComponent, reachableFromSettlement,
        approachableFromSettlement, walkPercolationStats } = mod.percolation;

/** Empreinte de la carte d'identifiants: FNV-1a 32 sur les int32, petit-boutiste. */
function empreinteIds(ids) {
  let h = 0x811c9dc5;
  for (let i = 0; i < ids.length; i += 1) {
    const v = ids[i] | 0;
    for (let b = 0; b < 4; b += 1) {
      h ^= (v >>> (b * 8)) & 0xff;
      h = Math.imul(h, 0x01000193) >>> 0;
    }
  }
  return h >>> 0;
}

const L = [];
const emit = (s = "") => L.push(s);

emit("// GENERE AUTOMATIQUEMENT - ne pas editer a la main.");
emit("// Source: tools/migration/gen-percolation-vectors.mjs");
emit("//");
emit("// Comparent les identifiants de composante, pas seulement leur nombre: ils");
emit("// sont attribues dans l'ordre de balayage, et tout le reste s'y refere.");
emit("//");
emit("// Si un cas ne passe plus: soit le portage a devie, soit la reference a");
emit("// change et il faut regenerer. Ne jamais corriger un vecteur a la main.");
emit("");
emit("// clang-format off");
emit("");
emit(`static constexpr int32 PercoW = ${W};`);
emit(`static constexpr int32 PercoH = ${H};`);
emit("");

// --- Composantes ------------------------------------------------------------
emit("struct FPercoWorldVector {");
emit("\tuint32 Seed;");
emit("\tint32 ComponentCount;");
emit("\tuint32 IdsFingerprint;");
emit("\tint32 WalkableTiles;");
emit("\tint32 LargestTiles;");
emit("\tint32 FirstSize;   // index dans PercoSizes");
emit("};");

const tailles = [];
const mondes = [];
for (const seed of SEEDS) {
  const sim = monterNavSim(mod, seed, W, H);
  const comp = walkComponents(sim);
  const premier = tailles.length;
  for (const s of comp.sizes) tailles.push(s);
  let walkable = 0;
  let largest = 0;
  for (const s of comp.sizes) { walkable += s; if (s > largest) largest = s; }
  mondes.push({ seed, sim, comp, premier, walkable, largest });
}

emit("static const int32 PercoSizes[] = {");
emit(`\t${tailles.join(", ")}`);
emit("};");
emit("");
emit("static const FPercoWorldVector PercoWorlds[] = {");
for (const m of mondes) {
  emit(`\t{ ${m.seed}u, ${m.comp.count}, 0x${empreinteIds(m.comp.ids).toString(16)}u, ${m.walkable}, ${m.largest}, ${m.premier} },`);
}
emit("};");
emit("");

// --- Hameau, atteignabilite, statistiques -----------------------------------
// Plusieurs positions de hameau par graine, dont au moins une BLOQUEE: c'est
// le cas qui exerce le balayage en carres concentriques.

emit("struct FPercoSettlementVector {");
emit("\tuint32 Seed;");
emit("\tint32 SX; int32 SY;");
emit("\tint32 SettlementBlocked;   // 1 si la case du hameau est bloquee");
emit("\tint32 HomeComponent;");
emit("\tint32 Components; int32 Islands; int32 HomeTiles;");
emit("\tuint64 WalkableFracBits; uint64 MainFracBits;");
emit("\tint32 HomeIsLargest;");
emit("};");
emit("static const FPercoSettlementVector PercoSettlements[] = {");

const sondes = [];
for (const m of mondes) {
  const { sim, seed } = m;

  // Une case libre au centre, une case d'eau, un coin — trois regimes.
  const candidats = [];
  const centre = { x: (W >> 1), y: (H >> 1) };
  candidats.push(centre);
  let eau = null;
  for (let y = 0; y < H && !eau; y += 1) {
    for (let x = 0; x < W; x += 1) {
      if (sim.tiles[y * W + x].type === "water") { eau = { x, y }; break; }
    }
  }
  if (eau) candidats.push(eau);
  candidats.push({ x: 1, y: 1 });

  for (const c of candidats) {
    sim.settlement = { x: c.x, y: c.y };
    const home = settlementWalkComponent(sim);
    const stats = walkPercolationStats(sim);
    const bloque = sim.footBlockedAt(c.x, c.y) ? 1 : 0;
    emit(`\t{ ${seed}u, ${c.x}, ${c.y}, ${bloque}, ${home}, ${stats.components}, ${stats.islands}, ` +
         `${stats.homeTiles}, ${bits(stats.walkableFrac)}, ${bits(stats.mainFrac)}, ${stats.homeIsLargest ? 1 : 0} },`);

    // Sondes d'atteignabilite sur une grille reguliere, pour ce hameau.
    for (let y = 2; y < H; y += 13) {
      for (let x = 3; x < W; x += 11) {
        sondes.push({
          seed, sx: c.x, sy: c.y, x, y,
          reach: reachableFromSettlement(sim, x, y) ? 1 : 0,
          approach: approachableFromSettlement(sim, x, y) ? 1 : 0,
        });
      }
    }
  }
  delete sim.settlement;
}
emit("};");
emit("");

emit("struct FPercoProbeVector { uint32 Seed; int32 SX; int32 SY; int32 X; int32 Y; int32 Reachable; int32 Approachable; };");
emit("static const FPercoProbeVector PercoProbes[] = {");
for (const p of sondes) {
  emit(`\t{ ${p.seed}u, ${p.sx}, ${p.sy}, ${p.x}, ${p.y}, ${p.reach}, ${p.approach} },`);
}
emit("};");

mkdirSync(dirname(SORTIE), { recursive: true });
writeFileSync(SORTIE, L.join("\n") + "\n", "utf8");
console.error(
  `Ecrit: ${SORTIE} — ${mondes.length} mondes, ` +
  `${mondes.reduce((n, m) => n + m.comp.count, 0)} composantes, ${sondes.length} sondes`);
