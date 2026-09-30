# hydro-network-001 — preuves visuelles

HighResShot 1600×1600 ramenées à 1200×1200, seed 12345, échelle 5 (1.9 km), Human_Geography_V2
actif, brouillard coupé (vue de carte). Suffixe `_0` = `anastasis.Terrain.Drainage 0` (eau
d'avant), `_1` = réseau. Même build, même caméra : la seule variable est la CVar.

Produit par :

```powershell
tools\unreal\hydro-network-capture.ps1 -Label final -States "0,1"
tools\unreal\hydro-network-capture.ps1 -Label debug1 -States "1" -Debug 1 -Shots <top+confluence>
tools\unreal\hydro-network-capture.ps1 -Label debug3 -States "1" -Debug 3 -Shots <top+confluence>
tools\unreal\hydro-network-capture.ps1 -Label hgoff -States "1" -PreCmds "anastasis.Terrain.HumanGeography 0"
```

| Fichier | Vue |
|---|---|
| `aerial_top_0/1` | zénithale, monde entier (haut = +X, droite = +Y) |
| `aerial_oblique_0/1` | oblique depuis le sud |
| `close_confluence_0/1` | plaine centrale : affluents → rivière principale → lac écrit → exutoire nord |
| `close_mainriver_0/1` | rivière principale, élargissement vers l'aval, confluences |
| `close_lake_1` | lac écrit de HG : un affluent, un exutoire par la brèche du rempart |
| `close_brookmouth_0/1` | vallée B : confluence en Y, ruisseau écrit jusqu'à la mer sud-ouest |
| `close_upland_outlet_0/1` | lacs d'altitude (niveau relevé, cuvette, grève) et leur exutoire vers la plaine |
| `debug_width_top`, `debug_width_confluence` | `anastasis.Drainage.Debug 1` : largeur, bleu étroit → rouge large |
| `debug_velocity_top` | `anastasis.Drainage.Debug 3` : vitesse, bleu lent (plaine) → rouge rapide (versants, sorties de lac) |
| `original_forms_top` | même couche, `anastasis.Terrain.HumanGeography 0` |
| `network_seed12345.json` | réseau exporté (`anastasis.Drainage.Dump`) : points `[x, y, z_eau, largeur, profondeur, vitesse, berge, pente, ordre]` en uu / m·s⁻¹ |

Ce que ces images ne prouvent pas : l'aspect de l'eau (matériau inchangé), la marche à pied
(aucune session joueur), un autre seed que 12345.

## Passe 2

| Fichier | Vue |
|---|---|
| `pass2_aerial_top`, `pass2_aerial_oblique` | lacs et mers arrondis, cuvettes comblées, initiation aire × pente² |
| `pass2_close_upland_outlet`, `pass2_close_lake` | lacs d'altitude et lac écrit arrondis |
| `pass2_original_forms_top` | même passe, `anastasis.Terrain.HumanGeography 0` |

`network_seed12345.json` est celui de la passe 2.
