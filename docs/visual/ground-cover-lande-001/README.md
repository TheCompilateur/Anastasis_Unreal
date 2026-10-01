# GROUND_COVER_001 / lande (H6) — preuves visuelles

Capture `tools\unreal\capture-ground-cover.ps1 -Label lande-v5 -States on,off,on2` (worktree
`ground-cover-lande-001`), `Lvl_AnastasisSlice`, `EmbodyCanonical(12345)`, HighResShot 1600×900.
`*_on` = herbe et lande, `*_off` = `anastasis.Dressing.GroundCover 0`, mêmes caméras (`cameras.json`).

```
ANASTASIS_GROUND_COVER enabled=1 tall=507198 short=416894 sedge=63773 heath=64836 heather=17777
  placed=1070478 near=707148 far=363330 chunks=1207 outside_valley=754907 shadows=1
  candidates=2560000 refused_slope=37193 refused_canopy=303516 truncated=0 missing_meshes=0
ANASTASIS_GROUND_SLOPES dry_candidates_by_slope 0-10=1442584 10-20=537577 20-30=122705
  30-45=111957 45-60=32582 60+=4611 steep_refused_canopy=31435
  lande_above_floor_m=[0.6 14.9 34.8] valley_floor_z=785
```

| Vue | Ce qu'elle montre |
|---|---|
| `lande_eye` | versant boisé clair (tuile 18.0, 21.1, la tuile de lande la plus fournie) : callunes à épis mauves et touffes d'éboulis entre les troncs |
| `lisiere_eye` | le versant ouest de la vallée A reste nu (voir Limites) |
| `hors_vallee_eye`, `prairie_eye` | non-régression de la prairie |
| `v1_lande_eye_on` | v1 : lande à ~5 % de couverture, invisible ; callune mesurée depuis la nappe (posée à 1 m sous le sol partout) |

## Ce que les mesures ont décidé

- **Où sont les pentes** (histogramme ci-dessus) : 234 662 candidates sèches entre 20 et 45°,
  37 193 au-delà — les versants ne sont pas des falaises.
- **Callune** : la lande est posée entre 0,6 et 34,8 m au-dessus du fond de vallée (p10 / p90) ;
  la bande de callune est 8 → 28 m. v1 (15-50 m depuis la nappe) : 6 140 callunes ; v4 : 17 777.
- **Couverture** : touffe d'éboulis rayon 46 cm, 230 lames (v1 : 32 cm, 150 lames, ~12 % du
  versant couvert, invisible), densité 0,9 → 0,3 avec la pente, ombre des couronnes à 50 %.

## Coût

GPU (`GetFrameTimingsMs`) : +1,1 à 2,6 ms avec toute l'herbe et la lande. Durée de frame : `on`,
`off` et `on2` se chevauchent (machine partagée, `off` parfois plus lent que `on`) ; aucun écart
mesurable au-delà du bruit (`ground-cover.json`).

## Limites

- Le versant ouest de la vallée A (`lisiere_eye`) reste nu : sa pente mesurée ou son habitat le
  sortent de la lande ; non diagnostiqué tuile par tuile.
- Les versants boisés gardent peu de lande sous les couronnes : c'est le rôle du sous-bois (H5).
