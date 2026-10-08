# WATER_NETWORK_001 — l'eau de la simulation suit son réseau

Décision d'Alexandre, 2026-10-08 : « l'eau doit respecter son réseau ». Base : `main` = `4d57ad49`, plus
les commits de `geo-measure-001` et `site-from-sim-001`.

## Le défaut

- **Deux eaux.** La simulation tenait son eau de l'hydrologie de la référence JS (`hydrology.js`) :
  des tranchées creusées sous le niveau de la mer, 27 plans d'eau, la plupart sans exutoire
  (HYDRO_NETWORK_001). Le joueur voyait une autre eau : le réseau de drainage que le rendu calcule sur
  le relief forgé (`AnastasisDrainage` : rivières qui descendent, lacs à un exutoire, Strahler).
- **L'écart mesuré** (GEO_MEASURE_001) : 833 centres de tuiles sur 9 216 en désaccord, dont 75 % produits
  par le réseau de drainage.
- **Ses effets en jeu** :
  - des habitants buvaient à une rive invisible, à 60 à 99 m de la berge visible ;
  - ils marchaient dans des rivières qu'on voit ;
  - le village de départ (`site-from-sim-001`) visait une eau qu'on ne voit pas.

## Le changement

1. **Réglages explicites.** La forge (`AnastasisTerrainForge::FSettings`, `Apply(..., Settings)`) et le
   drainage (`FParams::ForgeExaggeration`) reçoivent leurs réglages par paramètre.
   - `FromConsole()` lit les CVars, comme avant : c'est ce que le rendu utilise, son comportement ne
     change pas.
   - `Canonical()` donne leurs valeurs par défaut, figées.
2. **Géographie canonique** (`WorldView/AnastasisCanonicalGeography`). C'est la chaîne même de
   l'incarnation (TerrainSurface → TerrainForge, avec Human_Geography → Drainage → nappe des lacs et
   rubans de rivière), lancée sur le monde canonique entier :
   - recette fixe : échelle 5, Human_Geography actif, WaterLook actif, forge par défaut ;
   - sans UWorld et sans aucune CVar de rendu ;
   - sortie : un masque « eau du réseau au centre de la tuile », même critère que la concordance
     (eau au niveau du sol ou au-dessus, à 1 cm près) ;
   - mis en cache par graine.
3. **La simulation prend cette eau** (écart n°51, EXTENSION, A_TRANCHER) :
   - `AnastasisWorld::RestampWater` réécrit les types d'eau et de terre ;
   - `Shore` et `Wetness` sont recalculés avec les formules de la génération : les points où l'on boit
     suivent ;
   - `FAnastasisSimulation::ApplyWaterMask` relie ensuite le village à nouveau (grille de navigation).
   - L'hôte l'appelle au reset (`ResetCanonical`) quand `anastasis.Sim.WaterNetwork` vaut 1 (défaut).
   - `GenerateWorld` et `Reset` restent ceux de la référence, au bit près : la parité n'est pas touchée.
4. **Code partagé.** L'échantillonneur de maillage (`WorldView/AnastasisProjectedMesh.h`) est commun au
   sondage du site et à la géographie canonique.

Les CVars de rendu changent toujours ce qui est **dessiné**. `anastasis.Terrain.Drainage 0` montre l'ancienne
eau, pour une A/B ; elles ne changent plus ce que les habitants boivent ni ce qu'ils contournent.

5. **Le site de départ lit le relief drainé canonique** (`ReadSimulation(..., Canonical)`). Altitude et pente
   au centre de la tuile, mesurées comme le sondage rendu : neuf échantillons à ±0,4 tuile. Sans cela, le
   site choisi sur l'eau du réseau, (22 ; 17), était posé sur une pente **rendue** de 21,8° pour une pente
   simulée de 1,4°, et `material-courier-pie` y échouait (voir plus bas). L'eau, la marche et les
   ressources restent celles de la simulation.

## Preuves (worktree `water-network-001`, 2026-10-08)

**Tests ciblés** :
- commande : `report-tests.ps1 -Filter 'Anastasis.WaterNetwork+Anastasis.Sim.Monde.Eau+Anastasis.SettlementSite+Anastasis.Sim.Tick+Anastasis.Terrain'` ;
- résultat : **PASS 58**, KNOWN_EXPECTED_FAILURE 0, FAIL 0, run complet ;
- les 29 tests `Anastasis.Terrain` (forge, drainage, géographie dessinée) passent : le rendu n'a pas changé
  avec les réglages explicites.

| Mesure (`Anastasis.WaterNetwork.Canonical`) | Valeur |
|---|---|
| eau du réseau aux centres | 893 tuiles, 19 rivières, 8 lacs |
| eau JS | 1 198 tuiles, dont 629 en commun |
| tuiles changées dans la simulation | **833**, exactement le désaccord mesuré par GEO_MEASURE_001 sur le rendu réel |
| six CVars de rendu basculées (Drainage, HumanGeography, Exaggerate, Subdiv, Scale, WaterLook) | masque **identique** |
| calcul | 2,0 à 3,4 s, une fois par graine |
| site sur le relief drainé | (36 ; 20), pente 0,61°, eau visible à 40 m, 840 sites éligibles |

**Preuves PIE** : `editor-batch.ps1`, un éditeur, `Saved/EditorBatch/20261008-130148/` :

| Preuve | Verdict | Mesure |
|---|---|---|
| `geography-concordance-pie` | PASS | **0 désaccord sur 9 216** (`AGREEMENT_AT_CENTRES`) ; 833 avant |
| `settlement-sensitivity-pie` | PASS | `ref`, `Drainage 0`, `HumanGeography 0` et `ref2` : tous en (36 ; 20), **STABLE** |
| `settlement-site-pie` | PASS | |
| `npc-life-pie` | PASS | |
| `material-courier-pie` | PASS | |
| `terrain-access-pie` | PASS | |
| `villager-pie` | PASS | |
| `village-fabric-pie` | PASS | |
| `river-use-pie` | FAIL `no_meaningful_journey` | voir ci-dessous |

**A/B de `material-courier-pie`** (même binaire, CVars posées par `[SystemSettings]` le temps du run, puis
retirées) :

| Eau | Site | Relief lu par le site | Verdict |
|---|---|---|---|
| JS | (74 ; 36), rendu | rendu | PASS |
| JS | (39 ; 42), simulation | tuiles | PASS |
| réseau | (22 ; 17) | tuiles | **FAIL** : livré, rien construit |
| réseau | (36 ; 20) | canonique drainé | PASS |

**`river-use-pie`** : l'habitant boit à 5 m de son point de départ, à 60 m de l'eau rendue. Ce n'est plus un
désaccord d'eau, puisque l'eau est la même partout. C'est la règle de la référence `AtDrinkSpot` : on boit
dès que `Shore ≥ ShoreReach` (0,3), c'est-à-dire jusqu'à environ 3 tuiles (60 m) de l'eau
(`AnastasisVillage.h:366`). La changer est un écart de parité, à décider à part.

## Limites

- **Rivières étroites.** Elles sont lues au centre des tuiles de 20 m. Une rivière de 6 m qui passe entre
  deux centres n'est pas de l'eau pour la simulation : elle devient un gué que l'on traverse à pied.
- **Types non refaits.** Les types tirés de l'humidité de fond (champs, forêt) ne sont pas recalculés.
  `FlowX` / `FlowZ` / `FlowAmt` restent ceux de la référence.
- **Calcul en double.** Le calcul canonique refait le travail de l'incarnation (forge et drainage, environ
  1 à 3 s au premier reset d'une graine). Le partager quand les réglages de rendu sont ceux de la recette
  est une optimisation possible, non faite.
