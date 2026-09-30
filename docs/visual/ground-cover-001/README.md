# GROUND_COVER_001 — preuves visuelles

Capture `tools\unreal\capture-ground-cover.ps1 -Label v3` (worktree `ground-cover-001`), carte
`Lvl_AnastasisSlice`, `EmbodyCanonical(12345)`, HighResShot 1600×900, éditeur dédié discret.
Même session, mêmes caméras (`cameras.json`) : `*_on` = `anastasis.Dressing.GroundCover 1`,
`*_off` = `0`. Seule l'herbe change.

```
ANASTASIS_GROUND_COVER enabled=1 tall=136447 short=79033 sedge=52672 placed=268152
  candidates=630309 refused_mask=64355 refused_ground=0 refused_water=48595 refused_slope=48864
  refused_canopy=17955 refused_density=182388 crowns=16424 clearings=1 truncated=0
  missing_meshes=0 plan_ms=701.1 total_ms=1302.7
```

| Vue | Ce qu'elle montre |
|---|---|
| `prairie_eye` | vallée A à 1,7 m : prairie haute, hauteurs mêlées, épis |
| `prairie_low` | à 60 cm du sol, vers la rivière |
| `riviere_eye` | bande de laîches (H3) le long de la berge, prairie basse derrière |
| `lisiere_eye` | la prairie s'arrête au pied du versant > 20° (lande = passe suivante) |
| `vallee_b_eye` | vallée B |
| `oblique` | à 35 m : l'herbe est coupée à 112 m, de haut elle ne compte pas |
| `v1_prairie_eye_on`, `v2_prairie_eye_on` | itérations : v1 touffes isolées et noires (normale retournée sur la face arrière), v2 plus denses mais brun sec |

Coût, frame p50 / p95 en ms pendant l'attente de chaque vue (`ground-cover.json`). La machine
était partagée avec les éditeurs d'autres agents : ordre de grandeur, pas un budget.

| Vue | off | on |
|---|---|---|
| prairie_eye | 14.2 / 22.6 | 17.4 / 37.9 |
| prairie_low | 15.0 / 19.9 | 20.1 / 48.7 |
| riviere_eye | 12.6 / 17.8 | 19.8 / 41.0 |
| lisiere_eye | 10.7 / 14.2 | 24.7 / 103.5 |
| vallee_b_eye | 12.0 / 17.3 | 17.1 / 38.7 |
| oblique | 11.9 / 16.6 | 17.5 / 38.9 |

Jugement contre EZ5 (`pontique-etat-zero-5-lisieres-transitions`) : à hauteur d'homme, de 5 à
~100 m, la vallée se lit comme une prairie de fin d'été (vert sourd, épis dorés, taches, rive
humide distincte). Écarts restants : le sol pâle de `MI_AnastasisGround` perce entre les
touffes ; aucune fleur ; versants nus au-delà de 20°. Le hameau (`hameau_eye`, non versé ici)
est sur un replat sableux hors du masque de vallée : aucune herbe n'y pousse, la clairière
piétinée n'y change rien sur la carte réelle.
