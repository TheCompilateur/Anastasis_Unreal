# TERRAIN_RELIEF_001 — étape 1 : terrasses et escarpements coupés

Seed 12345, monde 96×96, forge subdiv 4, dressing masqué. Même build, seules
`anastasis.Terrain.Forge.Terraces` / `.Escarpments` changent :
`before` = 1/1 (forge d'origine), `after` = 0/0 (nouveau défaut).

Reproduire : `tools\unreal\capture-terrain-relief.ps1`.

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
