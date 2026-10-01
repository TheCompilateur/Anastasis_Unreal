# HANDOFF: guard-async

## MISSION

Le gardien de fenêtres poste le déplacement sans attendre le thread de l'éditeur, et ne renvoie une fenêtre encore à l'écran qu'une fois toutes les 2 secondes.

## FILES_OWNED

- `tools/unreal/editor-window-guard.ps1`
- `docs/unreal/PIEGES_UNREAL.md`
- `docs/unreal/handoffs/guard-async.md`

## COMMIT

`fix(agents): gardien -- deplacement asynchrone, une relance par fenetre toutes les 2 s`

## MEC

- BUILD: portail `finish -Mission guard-async` au versement
- TESTS: suite `Anastasis` du même portail
- COMMANDS:
  - `tools\unreal\agent-worktree.ps1 finish -Mission guard-async`

## SCN

Aucune scène. Le correctif ne lance pas Unreal : `SetWindowPos` avec `SWP_ASYNCWINDOWPOS`, et un délai de 2 s entre deux déplacements de la même fenêtre.

## PLY

Aucun.

## INTEGRATION_RISK

- Même fichier que la porte mémoire : `editor-window-guard.ps1` est lancé par `Start-AnastasisEditor`. Le rebase sur `main` a pris les deux.
- Une fenêtre fantôme de Windows (« Ne répond pas ») n'appartient pas à l'éditeur : le gardien ne peut pas la déplacer tant que le thread ne répond pas.

## STOP

Ne revendique pas la disparition des fantômes pendant un gel. Ne change pas le gameplay.
