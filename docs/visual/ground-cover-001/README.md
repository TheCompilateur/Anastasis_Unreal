# GROUND_COVER_001 — preuves visuelles

Capture `tools\unreal\capture-ground-cover.ps1 -Label v6 -States on,off,on2` (worktree
`ground-cover-001`), carte `Lvl_AnastasisSlice`, `EmbodyCanonical(12345)`, HighResShot 1600×900,
éditeur dédié discret. Même session, mêmes caméras (`cameras.json`) : `*_on` =
`anastasis.Dressing.GroundCover 1`, `*_off` = `0`. Seule l'herbe change.

```
ANASTASIS_GROUND_COVER enabled=1 tall=507198 short=416763 sedge=63666 placed=987627
  near=652862 far=334765 chunks=683 outside_valley=690560 shadows=1 candidates=2560000
  refused_mask=0 refused_ground=56232 refused_water=251752 refused_slope=271855
  refused_canopy=272081 refused_density=720453 crowns=16424 clearings=1 truncated=0
  missing_meshes=0 plan_ms=475.8 total_ms=1865.2
```

| Vue | Ce qu'elle montre |
|---|---|
| `prairie_eye` | vallée A à 1,7 m : prairie haute, hauteurs mêlées, épis |
| `prairie_low` | à 60 cm du sol, vers la rivière |
| `riviere_eye` | bande de laîches (H3) le long de la berge, prairie basse derrière |
| `lisiere_eye` | la prairie s'arrête au pied du versant > 20° (lande = passe suivante) |
| `vallee_b_eye` | vallée B |
| `hors_vallee_eye` | **hors de la vallée écrite** (tuile 17.8, 8.7, choisie parmi les touffes posées) : prairie et laîches du ruisseau sud-ouest |
| `oblique` | à 35 m : l'herbe est coupée à 108 m, de haut elle ne compte pas |
| `v1/v2/v4_prairie_eye_on` | itérations : v1 touffes isolées et noires (normale retournée sur la face arrière), v2 plus denses mais brun sec, v4 toute la carte en six HISM géants |

## Coût

Frame p50 (ms) pendant l'attente de chaque vue (`ground-cover.json`), même session ; `on2`
répète `on` en fin de série pour mesurer la dérive de la machine (partagée avec d'autres
éditeurs). Le GPU (`GetFrameTimingsMs`, temps de stat unit) monte de ~1,5 ms avec l'herbe.

| Vue | off | on | on2 | v4 on (six HISM, toute la carte) |
|---|---|---|---|---|
| prairie_eye | 10.7 | 14.9 | 13.1 | 26.2 |
| prairie_low | 11.2 | 12.7 | 12.9 | — |
| riviere_eye | 11.5 | 13.3 | 13.1 | — |
| lisiere_eye | 10.3 | 12.8 | 12.2 | 23.7 |
| vallee_b_eye | 9.9 | 12.8 | 12.9 | — |
| hors_vallee_eye | 12.2 | 13.4 | 13.6 | — |
| hameau_eye | 10.1 | 10.5 | 10.7 | 22.9 |

Ce que les mesures ont montré, dans l'ordre :

- v4 (toute la carte, un HISM par famille et par tier) : +12 ms dans **chaque** vue, hameau
  compris où presque rien n'est visible, pour +1,5 ms de GPU. Le coût suivait le nombre total
  d'instances (parcours des arbres de clusters), pas ce qui est à l'écran.
- v5 : couper les ombres de l'herbe ne changeait rien (GPU identique) ; la durée de frame de
  l'éditeur, elle, variait plus avec la charge des autres processus qu'avec la scène.
- v6 : HISM découpés en tuiles de 160 m avec distance d'affichage de primitive — +1,5 à 2 ms.

## Jugement contre EZ5

À hauteur d'homme, de 5 à ~100 m, la carte se lit comme une prairie de fin d'été (vert sourd,
épis dorés, taches, rive humide distincte), dans les vallées comme en dehors. Écarts restants :
le sol de `MI_AnastasisGround` perce entre les touffes (pâle en vallée, sableux ailleurs) ;
versants > 20° nus ; aucune fleur. Le hameau est sur un replat sableux où l'herbe est rare.
