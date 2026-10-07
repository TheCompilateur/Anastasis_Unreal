# SITE_FROM_SIM_001 — la simulation choisit le site de départ

Chantier 3 d'IRON_CRUSADE_001, accordé par Alexandre le 2026-10-07. Base : `main` = `3f945847`, plus le
commit de `geo-measure-001` (la preuve `settlement-sensitivity-pie`).

## Le défaut

`GEO_MEASURE_001` a mesuré que le site d'ouverture du village était choisi en lisant le maillage
**rendu** (`AnastasisSettlementSurvey::Read`). Basculer une CVar de rendu le déplaçait :

| CVar basculée | Déplacement du village |
|---|---|
| `anastasis.Terrain.Drainage 0` | 490 m |
| `anastasis.Terrain.HumanGeography 0` | 670 m |

Une mission visuelle pouvait donc changer, sans le savoir, la trajectoire entière de la société. Si le
terrain n'était pas prêt dans les 10 s, aucun village n'était posé.

## Le changement

- `AnastasisSettlementSurvey::ReadSimulation` construit les entrées de la politique de site à partir des
  **tuiles de la simulation** :
  - altitude (relief canonique, échelle 5, tuile de 20 m) ;
  - type, humidité et ressources de la tuile ;
  - blocage au pied du village.

  C'est une fonction pure de (graine, monde, village) : aucune CVar de rendu n'est lue.
- La politique `AnastasisSettlementSite::Choose` est **inchangée**.
- Le sondage rendu reste lu, mais seulement comme **observation** (`MergeRenderObservation`). Il donne la
  concordance de l'eau, plus deux nouvelles observations : `rendered_slope_deg` et `water_access_rendered`.
  Il ne choisit plus.
- Si le terrain rendu n'est pas prêt dans les 10 s, le village est quand même posé, depuis la simulation.
  Seule l'observation manque.
- Le rapport porte `selection_source` (`simulation` ou `rendered_relief`).
- `anastasis.Village.SiteSource` vaut 1 par défaut (simulation). La valeur 0 rétablit l'ancien
  comportement, pour une A/B ou un retour arrière.
- `SiteInputs` est le point unique utilisé par l'hôte et par les deux sondes (`TerrainAccessProbe`,
  `RiverUseProbe`). Une sonde ne re-choisit plus un autre site que le village.

### Calibrage du relief

Les seuils de pente de la politique (8°, 12° et 18°) avaient été calés sur le relief forgé, exagéré
verticalement. J'ai balayé un facteur de relief appliqué à l'altitude simulée (test
`Anastasis.SettlementSite.FromSimulation`, monde canonique) :

| Facteur | Pente des terres (médiane / 90ᵉ centile) | Sites éligibles | Site |
|---|---|---|---|
| **1,0** (relief de la simulation) | 3,18° / 23,42° | **1 250** | **(39 ; 42)** |
| 1,5 | 4,76° / 32,21° | 330 | (50 ; 29) |
| 2,0 | 6,33° / 39,70° | 0 | — |
| 2,5 à 3,6 | 7,9 à 11,3° / 45,9 à 55,9° | 0 | — |

Au-delà de 1,5, l'eau devient inatteignable. La forge n'exagère pas les rives ; un facteur uniforme,
lui, les exagère. **Retenu : 1,0**, le relief propre de la simulation. C'est la seule valeur qui
n'invente rien. `anastasis.Terrain.Forge.Exaggerate` n'est jamais lu.

## Preuves (worktree `site-from-sim-001`, 2026-10-07)

- Build : `BUILD::PASS`.
- `report-tests.ps1 -Filter Anastasis.SettlementSite` : **PASS 5**, KNOWN_EXPECTED_FAILURE 0, FAIL 0.
  - Quatre tests existants, plus `FromSimulation`.
  - `FromSimulation` vérifie un site éligible, sec, en pente ≤ 8°, avec l'eau à portée.
  - Les CVars `Drainage`, `HumanGeography` et `Forge` à 0 ne changent ni le site ni le score.
  - Une observation rendue extrême (pente de 45° partout, eau partout) ne change pas le choix.
  - `SiteSource 0` rend les entrées rendues.
- `editor-batch.ps1`, un seul éditeur, `Saved/EditorBatch/20261007-192033/` :

| Preuve | Verdict | Mesure |
|---|---|---|
| `settlement-sensitivity-pie` | PASS | `ref` (39 ; 42), `Drainage 0` (39 ; 42) **STABLE**, `HumanGeography 0` (39 ; 42) **STABLE**, `ref2` (39 ; 42) |
| `settlement-site-pie` | PASS | 12 contrôles : site choisi, pente ≤ 8°, aire, eau ≤ 300 m, nourriture et bois ≤ 600 m, puits au site, 12 habitants, mouvement, 3 captures |
| `npc-life-pie` | PASS | foyer et travail ; 8 boissons ; chantier de 22 pièces achevé, propriétaire attribué ; 32 matériaux livrés |
| `villager-pie` | PASS | 12 habitants, portraits distincts |
| `geography-concordance-pie` | PASS | observation intacte : 833 cases en désaccord (569 / 264), comme avant |
| `terrain-access-pie` | PASS | voir ci-dessous |
| `village-fabric-pie` | PASS | |
| `river-use-pie` | FAIL `no_meaningful_journey` | voir ci-dessous ; échouait déjà avant (`route_crosses_steep_ground`) |

### Ce que voit le joueur au nouveau site

| Mesure | Avant (site rendu, (74 ; 36)) | Après (site simulé, (39 ; 42)) |
|---|---|---|
| Pente rendue sous le site | 6,1° | 5,3° |
| Chemin maison → eau, pente max | 26,2° (ANOMALY) | 2,9° (aucune anomalie) |
| Chemin maison → champ, pente max | 30,7° (ANOMALY) | 4,8° (aucune anomalie) |
| Segments du trajet observé en anomalie | 2 568 sur 5 944 | **0 sur 5 344** |
| L'eau visée est-elle visible ? | — | **non** (`water_access_rendered = 0`) |

- **Le site simulé est meilleur pour la marche.** Aucun segment observé ne dépasse 18° ni ne touche
  l'eau rendue.
- **Mais l'eau du village n'est pas visible.** Le village boit à une eau de la simulation, l'une des
  569 cases que le drainage rendu assèche (GEO_MEASURE_001). C'est la décision 2 en attente : le
  drainage rendu doit-il respecter l'eau de la simulation ?
- `river-use-pie` : l'habitant boit tout de suite à la rive simulée, à une tuile (soif 80 → 18,6), sans
  marcher vers la berge rendue, à environ 60 m. Même écart, vu autrement.
- Le chemin champ → grenier de la fixture est `UNKNOWN_NO_SEMANTIC_PATH` : le premier seuil du grenier
  n'est pas atteint. L'instrument prévient qu'un autre seuil peut fonctionner, et `npc-life-pie` montre
  que les fermiers livrent.

## Limites

- Une graine, une carte.
  - Dans le top 5, des candidats ont une pente simulée de 1,2 à 1,5° mais une pente rendue de 23 à 33°.
    Ici, le meilleur site est sain ; sur une autre carte, rien ne le garantit.
  - Ce qui garantirait la cohérence, c'est que le rendu ne crée pas de pente là où la simulation n'en a
    pas : la même famille de décision que le drainage.
- La politique de site vit toujours dans l'hôte Unreal (`WorldView`), pas dans `AnastasisSim`. Elle ne lit
  plus que la simulation, mais n'est pas sous contrat de parité. Les autres décisions d'ouverture
  (`SeedOpening*`, attribution de la maison) restent dans l'hôte : c'est la suite de C3.
- Comportement de jeu changé : le village de départ est ailleurs. Une preuve qui supposerait l'ancien
  site (74 ; 36) le verrait.
