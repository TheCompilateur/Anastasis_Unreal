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

---

# Étape 3 : érosion thermique sur la hauteur rendue — `step3/`

`before` = `TalusDeg 0` (l'« after » de l'étape 2), `after` = `TalusDeg 40`,
`ErosionIterations 300` (nouveau défaut). Reproduire :
`tools\unreal\capture-terrain-relief.ps1 -Step 3`.

Au-delà du talus, la matière glisse vers les voisins plus bas ; en dessous, rien ne
bouge. Conservative, l'eau figée. Elle travaille **après** l'exagération, sur la pente
que le joueur voit.

**L'exagération reste à ×3.6** — c'était une piste du plan, les mesures l'ont écartée.
Balayage exagération × talus × itérations : avec érosion, 0 % de la terre intérieure
dépasse 60°, à ×3.6 comme à ×2.4. Baisser l'exagération ne fait donc que rabaisser le
relief (point haut 1156 uu à ×3.6, 986 à ×2.8, 903 à ×2.4).

| Vue | Lecture |
|---|---|
| `B_ground_*` | Œil sur le **nouveau** bassin (6650, 3550) : la paroi noire dans l'ombre devient un versant éclairé et praticable. Les taches sombres sur le sol éclairé sont le motif « léopard » du matériau (cf. `ground-001/E_regression_leopard_bump.png`), qui se voyait peu tant que le versant était dans l'ombre. |
| `C_slope_*` | L'intérieur des terres est arrondi et continu. Les parois sombres qui restent bordent toutes l'eau. 23 % des pixels changent. |
| `A_overview_*` | Pas de damier d'érosion. Rivières et lacs gardent des berges verticales en dents de scie : c'est l'étape 4. |

Mesure (`Anastasis.Terrain.Forge.NoCliffs`, terre émergée) :

| | before | after |
|---|---|---|
| > 60° à plus d'une tuile de l'eau | 14.24 % | **0.00 %** |
| > 60° total | 18.36 % | 3.13 % (tout sur les berges) |
| > 45° total | 32.3 % | 3.6 % |
| lames | 44 | 10 |
| point haut (uu) | 1353 | 1156 |
| masse (Σ Z) | — | Δ = 1e-6 uu |
| déplacement max (uu) | — | 770 |
| durée de `Apply` | ~85 ms | ~1.1 s |

Couplage non traité (hors mandat) : le matériau de sol ne peint la roche de pente
qu'entre 68° et 80° rendus (`SlopeRockStart/End` 0.62 / 0.82). L'intérieur étant
désormais sous 40°, cette roche n'apparaît plus que sur les berges.
