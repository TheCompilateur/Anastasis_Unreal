# hydra-001 — preuves visuelles

Captures HighResShot 1920×1080, seed 12345, `anastasis.Terrain.Surface 2`,
forge on. Avant = `anastasis.Dressing.Hydrology 0`. Après = 1.

| fichier | cadrage |
|---|---|
| torrent_before.png / torrent_after.png | oblique, site torrent |
| valley_before.png / valley_after.png | oblique, site rivière |
| inflow_before.png / inflow_after.png | oblique, entrée de lac |
| aerial_after.png | vue aérienne monde |
| oblique_after.png | oblique rivière |
| human_torrent.png | hauteur humaine, torrent |
| human_inflow.png | hauteur humaine, embouchure |

Sites mesurés (UU), seed 12345 — meilleur exemple intérieur, pas le centroïde :

- torrent (6350, 1050, 275)
- valley (5350, 1550, 275)
- inflow (8150, 1450, 275)

Les PNG sont produits par `tools/unreal/hydra-capture.ps1`. HighResShot exige un
viewport qui redessine ; relancer si le GPU est déjà pris par un autre éditeur.
