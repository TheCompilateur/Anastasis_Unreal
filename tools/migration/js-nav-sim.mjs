// Le monde vu par la navigation, cote JS — et rien d'autre.
//
// Partage par les generateurs de vecteurs de navigation et de percolation. Deux
// copies de ce montage divergeraient tot ou tard sans que personne le voie, et
// les deux batteries compareraient alors des mondes differents en croyant
// comparer deux portages.
//
// Ce montage reproduit exactement ce que le C++ construit: `generateWorld`,
// puis `blocked` sur l'eau, puis `rebuildMoveCosts`. Pas de `Simulation`: ni la
// navigation ni la percolation n'en dependent, et s'y reduire garantit qu'on
// compare bien deux fois la meme chose.

import { join } from "node:path";
import { pathToFileURL } from "node:url";

export const REF_PAR_DEFAUT = "C:/dev/Jeux IV Kingdoms";

/**
 * @param ref chemin du depot de reference
 * @returns { generateWorld, rebuildMoveCosts, footBlockedAt, moveCostAt, findPath, percolation }
 */
export async function chargerReference(ref = REF_PAR_DEFAUT) {
  const racine = ref.split(String.fromCharCode(92)).join("/");
  const url = (rel) => pathToFileURL(join(racine, rel)).href;
  const monde = await import(url("src/sim/world.js"));
  const nav = await import(url("src/sim/navGrid.js"));
  const chemin = await import(url("src/sim/pathfinding.js"));
  const perco = await import(url("src/sim/percolation.js"));
  return {
    racine,
    generateWorld: monde.generateWorld,
    rebuildMoveCosts: nav.rebuildMoveCosts,
    footBlockedAt: nav.footBlockedAt,
    moveCostAt: nav.moveCostAt,
    findPath: chemin.findPath,
    percolation: perco,
  };
}

/**
 * Monte le `sim` minimal dont la navigation a besoin.
 *
 * `tileTraversalCost` reprend `simulation.js` sans la congestion des
 * transports: elle appartient a la vague 5, et vaut 1 tant qu'aucune charrette
 * ne roule. Le C++ a la meme lacune, au meme endroit.
 */
export function monterNavSim(mod, seed, w, h) {
  const world = mod.generateWorld(seed, w, h);
  const sim = { w, h, tiles: world.tiles, navVersion: 0 };
  sim.blocked = new Uint8Array(w * h);
  for (let i = 0; i < w * h; i += 1) {
    if (sim.tiles[i].type === "water") sim.blocked[i] = 1;
  }
  sim.blockedAt = (x, y) => (x < 0 || y < 0 || x >= w || y >= h ? true : sim.blocked[y * w + x] === 1);
  sim.tileAt = (x, y) => (x < 0 || y < 0 || x >= w || y >= h ? null : sim.tiles[y * w + x]);
  mod.rebuildMoveCosts(sim);
  sim.footBlockedAt = (x, y) => mod.footBlockedAt(sim, x, y);
  sim.tileTraversalCost = (x, y, baseCost = 10) => {
    const mult = mod.moveCostAt(sim, x, y);
    if (!Number.isFinite(mult)) return Infinity;
    return Math.max(3, baseCost * mult);
  };
  return sim;
}

/** Premiere case libre trouvee autour de (x, y), en carres concentriques. */
export function caseLibreProche(sim, x, y) {
  const taille = Math.max(sim.w, sim.h);
  for (let r = 0; r < taille; r += 1) {
    for (let dy = -r; dy <= r; dy += 1) {
      for (let dx = -r; dx <= r; dx += 1) {
        if (Math.max(Math.abs(dx), Math.abs(dy)) !== r) continue;
        const cx = x + dx;
        const cy = y + dy;
        if (cx < 0 || cy < 0 || cx >= sim.w || cy >= sim.h) continue;
        if (!sim.footBlockedAt(cx, cy)) return { x: cx + 0.5, y: cy + 0.5 };
      }
    }
  }
  return null;
}
