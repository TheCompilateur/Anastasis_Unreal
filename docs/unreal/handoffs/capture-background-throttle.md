# HANDOFF: capture-background-throttle

## MISSION

Les captures éditeur échouaient au hasard : un `UnrealEditor` lancé pendant qu'Alexandre
travaille dans une autre fenêtre n'a pas le focus, et `UEditorEngine::Tick`
(EditorEngine.cpp, override « Background Process ») coupe alors tout rendu de viewport
quand `bThrottleCPUWhenNotForeground` est vrai (défaut). ~3 images/s, `HighResShot` jamais
servi. `-unattended` ne suffit pas. Correctif déjà validé dans `capture-terrain-relief.ps1`
(terrain-relief-001) et repris par `capture-shore-reeds.ps1` / `capture-reed-form.ps1` ;
étendu ici aux cinq lanceurs de capture qui ne l'avaient pas.

## FILES_OWNED

- tools/unreal/capture-slice.ps1
- tools/unreal/capture-terrain-forge.ps1
- tools/unreal/capture-tree-lineup.ps1
- tools/unreal/probe-demo.ps1
- tools/unreal/shore-capture.ps1

## COMMIT

Voir `git log agent/capture-background-throttle`.

## MEC

- Une ligne par lanceur : `-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False`
  (classe `config=EditorSettings`, surcharge en mémoire, rien écrit dans Saved/Config).
- Analyse syntaxique PowerShell des cinq scripts : 0 erreur.
- `capture-terrain-forge.ps1` exécuté de bout en bout : `CAPTURE::PASS`, 4 captures, la
  surcharge figure dans la ligne de commande du journal, des centaines d'images entre deux
  captures (contre ~3/s sans).
- Hors périmètre, non modifiés : `ground-material.ps1`, `measure-tree-cost.ps1`,
  `shore-water.ps1` (aucune capture), `report-tests.ps1`, `anastasis-unreal.ps1`.

## SCN

Pas de capture versionnée : la preuve est le `CAPTURE::PASS` ci-dessus.

## PLY

Sans objet.

## INTEGRATION_RISK

- Aucun code C++ touché.
- Les cadrages codés en uu de `capture-terrain-forge.py`, `observe-slice.py`,
  `capture-tree-lineup.py`, `astral-observe.py` datent du monde à 1 m/tuile ; avec
  `TileWorldSize` 400 et `SpatialScale`, ils visent un coin du monde. Non traité ici.

## STOP

Ne revendique pas des cadrages justes : seulement que l'éditeur rend et capture sans focus.
