# HANDOFF: editor-memory-gate-001

## MISSION

Empecher les agents de lancer plus d'editeurs Unreal que la machine (16 Go) ne peut porter.
Constat du 2026-10-01 : 13 alertes Windows 2004 « memoire virtuelle insuffisante » depuis le
2026-09-30 (12 le 30), toutes avec deux ou trois editeurs a 8-13 Go ; 4 crashs `UnrealEditor.exe`
en trois jours ; au moment du diagnostic, 0,9 Go de RAM libre et trois editeurs plus un build.

## FILES_OWNED

- `tools/unreal/editor-launch.ps1` -- `Invoke-AnastasisEditorGated`, `Get-AnastasisEditorLoad`,
  type `AnastasisMemory` (GlobalMemoryStatusEx) ; `Start-AnastasisEditor` passe par la porte.
- `tools/unreal/editor-launch.ps1` -- `Find-WorktreeEditor` : editeurs encore ouverts sur une racine.
- `tools/unreal/agent-worktree.ps1` -- `finish` refuse la passation tant qu'un editeur est ouvert
  sur le worktree (un editeur interactif garde ses 8-13 Go hors ecran apres la mission).
- `AGENTS.md` -- section « Porte memoire », ligne d'index d'`editor-launch.ps1`, ligne `finish`.
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
  - premier editeur toujours admis (apres redemarrage : 0 Unreal, 0,6 Go de RAM disponible, sessions
    Claude 6,1 Go + Cursor 3,7 Go). Charge simulee (`Get-AnastasisEditorLoad` remplace) :
    0 ed./0,6 Go/2 Go -> lance ; 1/5/20 -> lance ; 1/0,6/20 -> attend (ram) ; 1/5/4 -> attend (marge) ;
    2/10/30 -> attend (editeurs) ; 5/1,2/32,7 -> attend. 6/6 conformes.
  - `test-agent-worktree.ps1` : 13/13 PASS. Son code de sortie 1 est celui du dernier `git rev-parse
    --verify --quiet` (S7 verifie justement que la branche n'existe plus), pas un echec.
  - `Find-WorktreeEditor` contre les editeurs vivants a 12:2x : `river-look-001` -> pid 21808,
    `C:/dev/ANASTASIS_WORKTREES/understory-001/` (barres avant) -> pid 15572, canonique -> pid 43632 ;
    `river-look-00` et `understory` (prefixes) -> 0 ; `editor-memory-gate-001` -> 0.
  - porte en conditions reelles, pendant le `finish` de cette mission : `report-tests` retenu,
    `EDITOR_GATE::WAIT editeurs=3/2 (pids 15572,21808,43632) ram_dispo=2.3/3 Go`.
  - mesure a 11:54 : 4 editeurs ouverts, 1,5 Go de RAM disponible, 8 Go de marge : la porte aurait
    retenu tout nouveau lancement.

## SCN

Aucune scene.

## PLY

Aucun.

## INTEGRATION_RISK

- Apres le redemarrage du 2026-10-01, cinq editeurs d'agents relances en quelques minutes : tant que
  cette porte n'est pas sur `main`, rien ne les retient.
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
