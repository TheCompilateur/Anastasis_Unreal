// Parite du foyer — `src/life/domestic.js`, ce que la maison met en jeu.
//
// `sleepQuality` ne lit que des identifiants (interieur, foyer, abri) : c'est
// la seule fonction du module a entrees scalaires. Le reste (findOpenShelter,
// countShelterOccupants, assignSheltersDaily) lit le village entier et se prouve
// par les tests d'assemblage Anastasis.Sim.Village.Maison.*.

import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const ICI = dirname(fileURLToPath(import.meta.url));
const RACINE = join(ICI, "..", "..", "..");

// 0 dehors sans toit | 1 dehors avec toit | 2 dans son foyer | 3 chez autrui
// 4 dans son abri | 5 dehors avec abri | 6 dans un batiment sans toit ni abri
const LIEUX = [
  { inside: null, home: null, shelter: null },
  { inside: null, home: "b1", shelter: null },
  { inside: "b1", home: "b1", shelter: null },
  { inside: "b2", home: "b1", shelter: null },
  { inside: "b1", home: null, shelter: "b1" },
  { inside: null, home: null, shelter: "b1" },
  { inside: "b3", home: null, shelter: null },
];

export default {
  module: "src/life/domestic.js",
  out: join(RACINE, "Source", "AnastasisSim", "Private", "Tests", "AnastasisDomesticVectors.inl"),

  cases: [
    {
      name: "SleepQuality",
      comment: "sleepQuality(npc) : interieur, foyer, abri",
      args: ["string", "string", "string"],
      ret: "double",
      inputs: LIEUX.map((l) => [l.inside ?? "", l.home ?? "", l.shelter ?? ""]),
      call: (mod, [inside, home, shelter]) => mod.sleepQuality({
        inside: inside ? { buildingId: inside } : null,
        home: home ? { id: home } : null,
        shelter: shelter ? { id: shelter } : null,
      }),
    },
  ],
};
