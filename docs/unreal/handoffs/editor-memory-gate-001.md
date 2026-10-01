# HANDOFF: editor-memory-gate-001

## MISSION

Empecher les agents de lancer plus d'editeurs Unreal que la machine (16 Go) ne peut porter.
Constat du 2026-10-01 : 13 alertes Windows 2004 « memoire virtuelle insuffisante » depuis le
2026-09-30 (12 le 30), toutes avec deux ou trois editeurs a 8-13 Go ; 4 crashs `UnrealEditor.exe`
en trois jours ; au moment du diagnostic, 0,9 Go de RAM libre et trois editeurs plus un build.

## FILES_OWNED

- `tools/unreal/editor-launch.ps1` -- `Invoke-AnastasisEditorGated`, `Get-AnastasisEditorLoad`,
  type `AnastasisMemory` (GlobalMemoryStatusEx) ; `Start-AnastasisEditor` passe par la porte.
- `AGENTS.md` -- section « Porte memoire », ligne d'index d'`editor-launch.ps1`.
- `docs/unreal/PIEGES_UNREAL.md` -- entree `EDITOR_GATE::WAIT` / `TIMEOUT`.
- `.claude/skills/anastasis-mission/SKILL.md` -- ligne de la table des echecs de `finish`.

## COMMIT

voir `git log agent/editor-memory-gate-001`

## MEC

- BUILD: aucun C++ touche ; voir `finish`.
- TESTS: voir `finish` (`report-tests` passe lui-meme par la porte).
- Banc isole de la porte (script jetable, scratchpad), PowerShell 5.1, sans lancer Unreal :
  - `timeout` : `-MaxEditors 0 -TimeoutMinutes 0.05` -> une ligne `EDITOR_GATE::WAIT`, puis
    `EDITOR_GATE::TIMEOUT ... editeurs=4/0 ram_dispo=1.1/3 Go marge_engagee=7.6/8 Go` leve en 3,3 s ;
    le bloc de lancement n'a pas tourne.
  - `open` : seuils a zero -> le bloc tourne, un seul objet rendu (`count=1`), aucune ligne parasite.
  - `bypass` : `ANASTASIS_EDITOR_GATE=0` -> lance malgre `-MaxEditors 0`.
  - concurrence : processus A tient la porte 4 s pendant son « lancement » (11:54:26.3 -> 11:54:30.5) ;
    processus B demarre a 11:54:28.0 et ne lance qu'a 11:54:30.7. Verifier-puis-lancer est atomique.
  - chemin reel : `Start-AnastasisEditor PING.EXE -n 2 127.0.0.1` -> `System.Diagnostics.Process`,
    `count=1`, `ExitCode=0` ; `$FilePath` / `$ArgumentList` resolus dans le bloc.
  - mesure a 11:54 : 4 editeurs ouverts, 1,5 Go de RAM disponible, 8 Go de marge : la porte aurait
    retenu tout nouveau lancement.

## SCN

Aucune scene.

## PLY

Aucun.

## INTEGRATION_RISK

- Defauts (2 editeurs, 3 Go, 8 Go) choisis sur les alertes du 30/09, pas mesures sur une campagne.
  Si Alexandre travaille avec son propre editeur ouvert, un seul editeur d'agent passe a la fois :
  c'est voulu.
- Un `report-tests` ou une capture peut maintenant attendre jusqu'a 45 min avant de lancer son
  editeur ; leurs propres delais demarrent apres le lancement, donc ne sont pas consommes par l'attente.
- Le verrou `Global\AnastasisEditorGate` n'est tenu que pendant la creation du processus.

## STOP

Ne revendique pas une machine sans OOM : la porte ne regle ni la VRAM, ni les compilations
(`cl.exe` jusqu'a 2,5 Go chacun), ni les sessions Claude / Cursor ouvertes. Les seuils ne bornent pas
un editeur deja lance qui grossit.
