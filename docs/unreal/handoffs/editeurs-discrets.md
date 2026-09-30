# HANDOFF: editeurs-discrets

## MISSION

Que les editeurs Unreal lances par les agents ne gênent plus Alexandre, qui travaille sur la meme
machine : pas de fenetre au premier plan, pas de vol de focus.

Cause : le 2026-09-29 entre 15h40 et 16h25, six editeurs d'agents (dont deux `report-tests`) ont ete
tues par Alexandre, qui fermait les fenetres apparues devant lui (confirme par lui le 2026-09-30).
Signature : `Window '... - Unreal Editor' being destroyed` puis `Cmd: QUIT_EDITOR`. Aucun script ni
agent n'envoyait de fermeture (verifie : ni taskkill, ni CloseMainWindow, ni computer-use).

## FILES_OWNED

- `tools/unreal/editor-launch.ps1` (nouveau) -- `Start-AnastasisEditor` : CreateProcess avec
  SW_SHOWNOACTIVATE (premiere fenetre sans focus) ; `-Cmd.exe` garde Start-Process -WindowStyle Hidden ;
  lance le gardien ; rend un `System.Diagnostics.Process` (handle garde : ExitCode lisible)
- `tools/unreal/editor-window-guard.ps1` (nouveau) -- fenetres du processus hors ecran, au fond, sans
  activation, y compris les tardives ; rend le focus a la fenetre de l'utilisateur (AttachThreadInput)
- 17 lancements convertis dans 16 scripts : `anastasis-unreal.ps1` (verify, editor), `report-tests.ps1`,
  `capture-slice`, `capture-places`, `capture-reed-form`, `capture-shore-reeds`, `capture-terrain-forge`,
  `capture-terrain-relief`, `capture-tree-lineup`, `ground-material`, `measure-tree-cost`, `probe-demo`,
  `shore-capture`, `shore-water`, `world-dressing-01`, puis `capture-horizon` et `capture-refugee-props`
  (arrives de main pendant l'integration, chacun avec un Start-Process) : 18 lancements, 17 scripts
- `tools/unreal/agent-worktree.ps1` -- `finish` refuse tout lancement d'Unreal hors `Start-AnastasisEditor`
  (`Find-RawEditorLaunch`). Teste : 0 dans ce worktree, 17 sur 17 sur main avant integration.
- `AGENTS.md` -- section « Éditeurs discrets », index des deux nouveaux scripts, table des commandes

## COMMIT

Voir `git log agent/editeurs-discrets`.

## MEC

- BUILD: PASS -- `BUILD::PASS` (premier build du worktree)
- TESTS: PASS -- `report-tests.ps1` en mode discret : PASS 131, KNOWN_EXPECTED_FAILURE 4, FAIL 0,
  TOTAL 135 / 135 annonces. Journal du gardien : premier plan reste a l'utilisateur tout le run ;
  editeur, Journal des messages, dialogues, notification sortis de l'ecran.
- VERIFY: PASS -- `VERIFY::PASS` en mode discret (PIE actif, modules d'origine lus via `Process.Modules`
  sur l'objet rendu par le lanceur). 12 fenetres, toutes deplacees, aucun vol de focus.
- CAPTURE: `capture-slice.ps1 -Mode 2` hors ecran -> `CAPTURE::PASS`, image rendue.
  Comparaison au pixel (ecart > 16/255) : discret B vs visible 0,25 % des pixels, memes moyennes
  (31,1 / 43,2 / 46,8) ; discret A vs discret B 3,61 %, A vs visible 3,45 % -> A est la variance d'un
  premier run, le mode discret ne change pas le rendu.
- Focus : un vol observe (10:46:23, capture), rendu par le gardien en 0,85 s (`GIVE_BACK ok=True`).
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build` / `verify`
  - `tools\unreal\report-tests.ps1`
  - `tools\unreal\capture-slice.ps1 -Mode 2 -Out discret-mode2.png` (x2) et, avec
    `ANASTASIS_EDITOR_VISIBLE=1`, `-Out visible-mode2.png`
  - `ANASTASIS_EDITOR_GUARD_LOG=<fichier>` pour chaque run

## SCN

N/A -- aucune scene, aucun asset.

## PLY

N/A -- PLAYER non touche.

## INTEGRATION_RISK

- Essai rejete, documente dans `editor-launch.ps1` : `CREATE_NO_WINDOW` pour `UnrealEditor-Cmd` bloque le
  `Build.bat -Mode=ValidatePlatforms` lance au demarrage ; report-tests expirait a 15 min sans un test.
- Une boite de dialogue modale est desormais hors ecran : un editeur qui en attend une expire au
  timeout de son script au lieu d'etre vue. `ANASTASIS_EDITOR_VISIBLE=1` pour la voir.
- La fenetre principale peut rester ~1,6 s a l'ecran quand l'editeur tarde a traiter le deplacement.
- Tout nouveau lanceur doit utiliser `Start-AnastasisEditor` : `finish` l'impose dans `tools/unreal/`.
  Une branche integree sans passer par `finish` (ou une integration de plusieurs branches) peut encore
  en apporter un ; relancer `Find-RawEditorLaunch` sur le resultat.
- Hors perimetre, observe : un run temoin a echoue par `E_OUTOFMEMORY` a la creation du swapchain
  D3D12 alors que trois editeurs d'agents et deux builds tournaient en parallele. L'editeur de
  `env-realism-001` (PID 652) est reste bloque sur une fenetre « Error » pendant toute la mission.

## STOP

Ne revendique pas : le comportement avec plusieurs ecrans ou un ecran a gauche du principal (le gardien
vise 5000 px a gauche du bureau virtuel, donc hors de tout ecran) ; les lanceurs hors `tools/unreal/`
(scripts ad hoc des sessions Codex).
