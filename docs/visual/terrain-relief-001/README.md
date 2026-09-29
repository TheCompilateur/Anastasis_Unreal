# TERRAIN_RELIEF_001 — étape 1 : terrasses et escarpements coupés

Seed 12345, monde 96×96, forge subdiv 4, dressing masqué. Même build, seules
`anastasis.Terrain.Forge.Terraces` / `.Escarpments` changent :
`before` = 1/1 (forge d'origine), `after` = 0/0 (nouveau défaut).

Reproduire : `tools\unreal\capture-terrain-relief.ps1 -Step 1` (épingle `Bicubic 0`,
`Sharpen 1`, les défauts de l'époque).

| Vue | Lecture |
|---|---|
| `C_slope_*` | Le grand versant vert (droite) : l'escalier de banquettes disparaît. |
| `A_overview_*` | Même effet sur les versants du rempart de bordure ; 1,9 % des pixels changent. |
| `B_ground_*` | Œil à 1,7 m sur le bassin habitable, vers le rempart ouest : **quasi identique**. Le mur de lames et de parois n'est pas dû aux terrasses. |

Mesure sur le maillage rendu (`Anastasis.Terrain.Forge.NoStaircase`, terre émergée) :

| | before | after |
|---|---|---|
| rugosité (moy. \|Δ²Z\|, uu) | 22.29 | 18.98 (−15 %) |
| sommets > 60° | 20.83 % | 20.53 % |
| pente p99 | 81.6° | 81.4° |

Ce que l'étape 1 ne règle pas, et pourquoi : un sommet de terre sur cinq reste rendu
au-delà de 60°. La cause est l'exagération verticale (×3.6 sur des tuiles de 1 m), et
les lames qui longent l'eau viennent de l'amplification du Laplacien à côté des chenaux
creusés. Ce sont les étapes 2 et 3.

---

# Étape 2 : altitude bicubique, affûtage coupé — `step2/`

`before` = `Bicubic 0`, `Sharpen 1` (l'« after » de l'étape 1), `after` = `1`, `0` (nouveau
défaut). Reproduire : `tools\unreal\capture-terrain-relief.ps1 -Step 2`.

- **Bicubic** : Catmull-Rom sur 4×4 tuiles, borné au min/max de la cellule. Passe par
  chaque tuile, pente continue d'une cellule à l'autre, aucun extremum nouveau.
- **Sharpen** : l'amplification du Laplacien (×1.35 / ×0.95) et l'affûtage du point haut.

| Vue | Lecture |
|---|---|
| `C_slope_*` | La frange de lames qui bordait lacs et chenaux disparaît ; les versants sont arrondis, plus pliés sur la grille. 14 % des pixels changent. |
| `A_overview_*` | Même lecture à l'échelle de la carte ; 4,8 % des pixels changent. |
| `B_ground_*` | Même œil qu'à l'étape 1 : les lames au premier plan s'effacent, **la paroi reste**. |

Mesure (`Anastasis.Terrain.Forge.NoSpikes`, terre émergée) :

| | before | after |
|---|---|---|
| lames (max local > 50 uu au-dessus de ses 8 voisins) | 651 | 44 (−93 %) |
| pli de grille (\|Z''\| sur ligne de tuile / entre) | 2.97 | 1.39 |
| rugosité (moy. \|Δ²Z\|, uu) | 18.98 | 9.86 (−48 %) |
| sommets > 60° | 20.53 % | 18.36 % |
| pente p99 | 81.4° | 80.3° |

Effet de bord mesuré : le point le plus bas du maillage passe de −339 à +93 uu. Les
sommets de terre que l'affûtage enfonçait sous la nappe (creux sans eau) ont disparu.
Le bassin habitable se déplace : (2100, 6350) → (6650, 3550). Les vues de cette page
restent calées sur l'ancien, pour que before/after regardent le même endroit.

Contrat de chunk : le stencil bicubique déborde de deux tuiles, le halo passe de 1 à 2
(`AnastasisTerrainForge::HaloTiles`). `ChunkSeam` : erreur de Laplacien au bord 0.

Ce qui reste : 18 % de la terre au-delà de 60° — l'exagération ×3.6 sur 1 m (étape 3).
Les lacs gardent des bords droits et des parois : masque d'eau pris à la tuile la plus
proche et exagération différente de part et d'autre de la rive (étape 4).
