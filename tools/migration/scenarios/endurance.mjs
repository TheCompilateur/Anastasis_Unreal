// SCENARIO `endurance` — le village de `Anastasis.Sim.Village.Endurance`, en JS.
//
// Le test C++ (Source/AnastasisSim/Private/Tests/AnastasisVillageEnduranceTests.cpp)
// pose, sur le monde de la graine 12345 :
//   - le champ genere (nourriture > 0) le plus proche du centre de la carte ;
//   - un grenier a une distance de Chebyshev 3 a 4 de ce champ, un puits a 5-8,
//     une maison a 5-9, chacun sur la premiere case libre (bord de carte exclu,
//     pas de ressource, pas d'obstacle au pied) dont une porte rejoint le champ ;
//   - deux fermiers au grenier (faim 10 puis 15), trois sans-metier (faim 20,
//     30, 40), tous a la premiere porte du grenier ; energie 85, social 80,
//     loisir 80, hygiene 80, soif 10, sante 95, moral 60 ;
//   - pas de temps 1/60, depart a DAY_LENGTH * 0.42 du jour 1.
//
// La recette est reprise telle quelle, avec les fonctions publiques de la
// reference (`addBuilding`, `spawnNpc`, `findPath`, `footBlockedAt`). UNE
// difference, imposee par la reference et non choisie: le monde.
//
//   Le C++ genere un monde 96 x 96 (`Sim.Reset(12345u, 96, 96)`). Une sauvegarde
//   JS ne peut pas porter cette taille: `deserialize` LEVE si (w, h) n'est pas
//   `resolveWorldExtent(graine)`, soit 108 x 114 pour 12345, et il pose la
//   couronne du village autour de `settlement` avant d'appliquer `tileDiff`.
//   Le scenario vit donc sur le monde que la sauvegarde JS decrit; c'est ce
//   monde que le lecteur C++ (sim-state-reader-001) devra reconstruire, et le
//   « centre » est celui de CE monde. Les positions obtenues sont ecrites dans
//   le scenario (`recette`).

export const meta = {
  name: "endurance",
  description: "Puits, maison, grenier ; deux fermiers au grenier, trois sans-metier. Recette du test C++ Anastasis.Sim.Village.Endurance, sur le monde de la sauvegarde JS (graine 12345).",
  seed: 12345,
  dt: 1 / 60,
  // La reference, au runtime, draine 2 travaux de minuit par tick
  // (processDayDeferred) — comme le C++. `flush` viderait la file au tick de
  // minuit: c'est ce que fait la trace sans scenario, pas le jeu.
  dayDeferred: "tick",
  // Perimetre: ce que le premier lecteur C++ projettera (P3_PLAN.md §3,
  // sim-state-reader-001). Le reste de `serialize` est hors perimetre.
  sections: ["seed", "rng", "w", "h", "time", "day", "tileDiff", "buildings", "actors", "mealReservations"],
  masques: [
    "kosmos", "sagas", "urbanIntent", "lodLogique", "separationFoule", "animaux", "transport",
    "economieJournaliere",
    "minuit.collective", "minuit.socialOrders", "minuit.colonyDoctrine", "minuit.growthChapter",
    "minuit.founderCharter", "minuit.colonySites", "minuit.transportProjects", "minuit.roadEvolution",
    "minuit.watchPosts", "minuit.lifeDaily", "minuit.careers", "minuit.founders", "minuit.romanCouncils",
    "minuit.memory.partiel", "minuit.animalsDaily", "minuit.immigration",
  ],
  // Neutralise par l'etat, pas par un masque: dit dans le scenario.
  etatVide: [
    { systeme: "animaux", etat: "sim.animals = [] (resetWorldBase n'en pose aucun ; animalsDaily masque n'en ajoute pas)" },
    { systeme: "transport", etat: "aucune charrette, aucun travail de transport au depart" },
    { systeme: "kosmos", etat: "aucune instance `life.kosmos1204.lot0.pending`" },
    { systeme: "lodLogique", etat: "5 habitants, sous `LOGICAL_LOD.minPopulation` (18)" },
    { systeme: "village", etat: "pas de camp, pas de marche, pas de structure de village (resetWorldBase sans site)" },
  ],
};

const BESOINS = { energy: 85, social: 80, leisure: 80, hygiene: 80, thirst: 10, health: 95, morale: 60 };

/**
 * Construit le village sur `sim` (deja charge par `deserialize`). `essai(fn)`
 * rejoue `fn` sur une copie jetable de l'etat courant: la reference n'a pas de
 * `removeBuilding`, et le test C++ retire un batiment dont aucune porte ne
 * rejoint le champ. On essaie donc sur la copie, et on ne pose sur `sim` que
 * l'emplacement retenu — la pose est deterministe, la copie et `sim` donnent
 * le meme batiment.
 */
export function construire(sim, { ref, essai }) {
  const cx = sim.w / 2;
  const cy = sim.h / 2;

  // Le champ genere le plus proche du centre (premier dans l'ordre des tuiles
  // en cas d'egalite, comme le `<` strict du C++).
  let champ = null;
  let meilleur = Infinity;
  for (const tile of sim.tiles) {
    if (tile.resource !== "food" || !(tile.amount > 0)) continue;
    const d = ref.dist({ x: tile.x, y: tile.y }, { x: cx, y: cy });
    if (d < meilleur) {
      meilleur = d;
      champ = tile;
    }
  }
  if (!champ) throw new Error("aucun champ genere sur ce monde");
  const but = { x: champ.x + 0.5, y: champ.y + 0.5 };

  const poserPres = (type, rMin, rMax) => {
    for (let r = rMin; r <= rMax; r += 1) {
      for (let dy = -r; dy <= r; dy += 1) {
        for (let dx = -r; dx <= r; dx += 1) {
          if (Math.max(Math.abs(dx), Math.abs(dy)) !== r) continue;
          const x = champ.x + dx;
          const y = champ.y + dy;
          if (x < 2 || y < 2 || x > sim.w - 3 || y > sim.h - 3) continue;
          const tile = sim.tiles[y * sim.w + x];
          if (tile.resource || sim.footBlockedAt(x, y)) continue;
          const ok = essai((copie) => {
            const b = copie.addBuilding(type, x, y);
            return (b.accessPoints || []).some((porte) => ref.findPath(copie, porte, but) !== null);
          });
          if (ok) return sim.addBuilding(type, x, y);
        }
      }
    }
    throw new Error(`${type} : aucun emplacement entre ${rMin} et ${rMax}`);
  };

  const grenier = poserPres("granary", 3, 4);
  const puits = poserPres("well", 5, 8);
  const maison = poserPres("house", 5, 9);
  const porte = grenier.accessPoints[0];

  const fermiers = [];
  for (let k = 0; k < 2; k += 1) {
    const npc = sim.spawnNpc(porte.x, porte.y, { jobId: "farmer", hunger: 10 + 5 * k, ...BESOINS });
    if (npc.workplace?.id !== grenier.id) throw new Error(`${npc.id} : poste ${npc.workplace?.id}, attendu ${grenier.id}`);
    fermiers.push(npc);
  }
  const sansMetier = [];
  for (let k = 0; k < 3; k += 1) {
    sansMetier.push(sim.spawnNpc(porte.x, porte.y, { jobId: "settler", hunger: 20 + 10 * k, ...BESOINS }));
  }

  return {
    monde: { w: sim.w, h: sim.h, centre: { x: cx, y: cy } },
    champ: { x: champ.x, y: champ.y, amount: champ.amount, cropId: champ.cropId ?? null },
    batiments: [grenier, puits, maison].map((b) => ({ id: b.id, type: b.type, x: b.x, y: b.y, portes: b.accessPoints.length })),
    porteDuGrenier: { x: porte.x, y: porte.y },
    habitants: [...fermiers, ...sansMetier].map((n) => ({
      id: n.id, jobId: n.jobId, poste: n.workplace?.id ?? null, x: n.x, y: n.y,
      faim: n.hunger, energie: n.energy, soif: n.thirst, social: n.social, loisir: n.leisure, hygiene: n.hygiene, sante: n.health, moral: n.morale,
    })),
  };
}
