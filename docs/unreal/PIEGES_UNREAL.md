# Pièges Unreal d'ANÁSTASIS

Ce qui ne se découvre qu'en échouant. Chaque entrée part de **ce que tu vois** — c'est par là qu'on
cherche — puis donne la cause et la parade. Chacune a coûté au moins une mission.

Les outils se décrivent eux-mêmes (MCP `list_toolsets`, en-têtes de `tools/unreal/`, index d'`AGENTS.md`) ;
ce fichier ne les recopie pas. Il ne garde que ce qu'aucun outil n'expose.

Ajouter une entrée quand un piège a coûté du temps et que rien dans le dépôt ne l'aurait signalé.
Retirer une entrée quand un outil le rend impossible, en le disant.

---

## Éditeur tué, lancement, fenêtres

### « Démarré, jamais terminé : suspect du crash » — ce n'est souvent pas un crash
**Tu vois** dans le log : `LogStall: Shutdown...`, puis `Window '… - Unreal Editor' being destroyed`,
puis `Cmd: QUIT_EDITOR` / `CloseEditor()`, ~20 s après le démarrage.
**Cause** : une fermeture propre de la fenêtre principale — Alexandre travaille sur la même machine et
fermait les éditeurs qui s'ouvraient devant lui (2026-09-29, six runs perdus dont deux `report-tests`).
Un `Stop-Process` ne laisse aucune de ces lignes ; `quit_editor()` commence par `Cmd: QUIT_EDITOR`.
**Parade** : lancer Unreal par `Start-AnastasisEditor` (voir `AGENTS.md`, « Éditeurs discrets ») ; `finish`
refuse un `Start-Process` d'Unreal. Devant cette signature : relancer, ne pas chercher de régression.

### `UnrealEditor-Cmd` figé 12 s après le boot, plus une ligne pendant 15 min
**Tu vois** : dernier log `InternalLoadLibrary: 'TurnkeySupport'`, un `cmd.exe` enfant exécutant
`Build.bat -Mode=ValidatePlatforms`, timeout du lanceur.
**Cause** : processus console créé avec `CREATE_NO_WINDOW` ; le `Build.bat` que l'éditeur lance au
démarrage se bloque dans cette console sans fenêtre (2026-09-30).
**Parade** : un exécutable console garde `Start-Process -WindowStyle Hidden` — c'est ce que fait
`Start-AnastasisEditor` pour `-Cmd.exe`. Tuer l'orphelin `cmd.exe` s'il est le tien.

### Une boîte de dialogue invisible fait expirer le script
**Tu vois** : l'éditeur ne progresse plus, sans erreur, jusqu'au timeout.
**Cause** : depuis les éditeurs discrets, une modale est hors écran, personne ne la voit.
**Parade** : relancer avec `ANASTASIS_EDITOR_VISIBLE=1`, et `ANASTASIS_EDITOR_GUARD_LOG=<fichier>` pour voir
quelles fenêtres sont apparues.

### `Failed to create swapchain … E_OUTOFMEMORY`
**Tu vois** : l'éditeur reste à la frame 0 après cette erreur D3D12.
**Cause** : mémoire vidéo saturée par plusieurs éditeurs et builds d'agents en parallèle (2026-09-30 : trois
éditeurs, deux builds). Sans rapport avec ton changement.
**Parade** : `Get-CimInstance Win32_Process -Filter "Name LIKE '%Unreal%'"` — la ligne de commande dit à quel
worktree appartient chaque éditeur. Attendre une fenêtre calme, relancer. Ne pas tuer l'éditeur d'un autre.

### `Unable to build while Live Coding is active` alors que tu as passé `-NoLiveCoding`
**Tu vois** : `Result: Failed (OtherCompilationError)`, exit 6.
**Cause** : le verrou est **global à la machine** : n'importe quel `UnrealEditor.exe` ouvert, même d'un autre
worktree, même lancé avec `-NoLiveCoding`.
**Parade** : attendre une fenêtre calme qui **tient** (une campagne de captures enchaîne des éditeurs, un pid
toutes les 1–2 min), puis builder ; ou `Build.bat … -WaitMutex -NoHotReloadFromIDE`.

---

## Captures

### `HighResShot` demandé, aucun fichier, log à ~3 images/s
**Cause** : sans le focus, `UEditorEngine::Tick` coupe le rendu des viewports quand
`bThrottleCPUWhenNotForeground` est vrai. `-unattended` ne suffit pas.
**Parade** : `-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False`
(classe `config=EditorSettings`, pas EditorPerProjectUserSettings ; inaccessible depuis Python).

### `HighResShot` jamais servi sur une scène statique
**Cause** : la capture est servie au prochain redessin du viewport ; une scène qui ne bouge plus ne se
redessine pas.
**Parade** : invalider les viewports à chaque tick (`redraw()` de `capture-tree-lineup.py`) ; attendre
l'apparition du fichier, pas une durée fixe (plus de 6 s observées).

### La capture PIE photographie l'éditeur, ou part en plein fondu de caméra
**Tu vois** : le JSON à côté est juste (`map=UEDPIE_0_…`), l'image non.
**Cause** : `bInRestrictToGameViewport` n'est lu que si `bShowUI == true` (`GameViewportClient.cpp`) ; le
délai avant capture est un 0,5 s fixe.
**Parade** : **regarder chaque image avant de la classer en preuve** ; préférer la capture viewport éditeur
(`observe-slice.py`, chemin des preuves scellées). Détail : `docs/unreal/ATMOSPHERE_002.md`.

### Deux captures du même état ne sont pas identiques au pixel
**Mesuré** : ~3,6 % des pixels à plus de 16/255 d'un run à l'autre (anticrénelage temporel, bruit Lumen),
et un premier run parfois plus sombre. Un A/B doit comparer à cette variance, pas à zéro.

### Les caméras en coordonnées absolues visent un coin du monde
**Cause** : depuis `terrain-relief-001` une tuile fait 4 m (`TileWorldSize` 400 uu) ; les scripts écrits avant
(`astral-observe.py`, `observe-slice.py`, `capture-tree-lineup.py`, `terrain-forge-capture.py`) ont des caméras
calées sur l'ancienne échelle. `capture-terrain-relief.ps1` montre la mise à l'échelle.

---

## Build

### Build vert dans ton worktree, `main` qui ne compile plus après versement
**Cause** : UBT compile en non-unity les fichiers que `git status` voit modifiés. Deux `.cpp` qui définissent le
même symbole (`WriteJson`, 2026-09-13) ou une locale qui masque une constante d'un autre `.cpp` (`DeepWater`,
C4459, 2026-09-14) ne se rencontrent qu'en unity — dans l'arbre propre.
**Parade** : le build qui compte est celui d'un arbre **propre** : worktree neuf depuis `main`, ou le pre-push
sur un canonique propre.

---

## Tests et logs

### `TESTS::PASS` avec une partie de la suite
**Cause** historique : un éditeur mort au milieu de la file n'écrit rien pour les tests suivants.
**Parade** : `report-tests.ps1` exige le roster `Found N automation tests`, le marqueur `TEST COMPLETE`, l'absence
de fatal et l'`ExitCode` ; il rapporte `RUN_INCOMPLET` sinon. Ne jamais recompter à la main.

### Vingt `LogAutomationTest: Error: Condition failed` au démarrage
**Cause** : tests moteur sensibles à la culture de l'éditeur (FR). Bruit, classé dans `known-log-patterns.txt`.

### `Success` qui n'est pas un PASS
Les tests marqués `AddExpectedError` / `@unittest.expectedFailure` rapportent `Success`. Ils sont
KNOWN_EXPECTED_FAILURE, jamais PASS (`AGENTS.md`, registre `known-expected-failures.txt`).

### Un test marqué KNOWN_EXPECTED_FAILURE sort `Fail` alors que Python dit `OK (expected failures=1)`
**Tu vois** dans la même milliseconde : `LogPython: Error: RuntimeError: <_overlapped.Overlapped object …> still
has pending operation at deallocation`, puis `Result={Fail}` et `GIsCriticalError=1`.
**Cause** : le contrôleur d'automation attribue toute ligne `LogPython: Error` au test en cours. L'erreur vient de
la boucle `asyncio` de l'éditeur, pas du test (2026-09-30, machine à 1,8 Go de RAM libre ; non reproduit au run
suivant).
**Parade** : lire la sortie Python du test lui-même avant de conclure. Si elle dit `OK`, relancer la suite ; ne
**jamais** retirer le marqueur ni toucher au registre pour ça. Si l'erreur `asyncio` revient sur une machine
calme, c'est un sujet à part entière.

---

## Python éditeur

### `save_asset` ne sauve rien, sans erreur
**Cause** : `get_editor_property` sur une structure rend une **copie**. Muter la copie ne salit pas l'asset, et
`save_asset` d'un asset propre ne fait rien.
**Parade** : réécrire chaque structure mutée dans son tableau, puis réassigner le tableau
(`set_ruin_variant.py`, repris par `set_presentation_meshes.py` et `set_tree_grammar.py`). Vérifier en relisant
l'asset, pas le log.

### Un script Python qui lève à l'import laisse l'éditeur ouvert jusqu'au timeout
**Parade** : `quit_editor()` dans le `except`.

### Un chemin passé à `-script` perd ses `\t`, `\n`…
**Cause** : lu comme séquence d'échappement. **Parade** : chemins en `/` (`measure-tree-cost.ps1`).

### Un acteur de capture se retrouve dans le niveau
**Cause** : `save_current_level()` fige tout ce qui a été posé. Les scripts de capture ne sauvent **jamais** le
niveau (`capture-tree-lineup.py`) ; l'embodiment remplit des composants `RF_Transient`.

---

## PowerShell 5.1 et git

### Le script meurt sur la progression de git
**Tu vois** : `NativeCommandError` sur « Preparing worktree », « From . », etc.
**Cause** : sous `$ErrorActionPreference='Stop'`, le stderr d'un exe natif redirigé devient une erreur fatale.
**Parade** : `Invoke-Git` (`agent-worktree.ps1`) ou `$ErrorActionPreference='Continue'` localement + `2>&1`.
`agent-worktree.ps1 create` historique : l'erreur s'affiche, le worktree existe — vérifier avant de relancer.

### Arguments cassés entre PowerShell et un exe natif
- un `"` dans `git commit -m @'…'@` éclate le message en pathspecs → `git commit -F <fichier>` ;
- les `"` d'un JSON passé en argument disparaissent → passer par un fichier ou par bash ;
- un flux binaire entre deux exe (`git archive | tar`) est corrompu → fichier intermédiaire.

### `[IO.Path]::GetFullPath($relatif)` pointe dans la racine canonique
**Cause** : résolu contre le répertoire du **processus**, pas contre `Set-Location`.
**Parade** : `(Resolve-Path $rel).Path`, ou `Join-Path $PSScriptRoot`.

---

## Intégration

### `integrate` et `git branch -d` supposent que le canonique est sur `main`
Il ne l'est pas toujours (2026-09-29 : branche d'un autre agent, C++ non commité). Depuis le 2026-09-30,
`integrate` déplace `main` seule et `prune` remplace `git branch -d`. Banc : `tools/unreal/test-agent-worktree.ps1`.

### Un versement refusé par le hook a quand même écrit la copie de travail
**Cause** : `reference-transaction` refuse en phase `prepared`, après l'écriture. **Parade** : vérifier
`git status --porcelain` après tout refus ; `integrate` liste les entrées salies. Ne jamais `git clean -fd`
dans le canonique : cela emporte le travail non commité des autres.

### Le push est refusé par le pre-push alors que le code compile
**Tu vois** : `git push` échoue sur une erreur de `anastasis-unreal.ps1` (bloc `catch`), et `build.log` du
canonique commence par `Build.bat is already running, waiting for existing script to terminate`.
**Cause** : un autre agent compilait le canonique au même instant (il versait sa propre mission).
**Parade** : `tools\unreal\anastasis-unreal.ps1 build` dans le canonique ; `BUILD::PASS` → repousser. Si la
réponse est « Everything up-to-date », l'autre agent a poussé `main` avec tes commits : vérifier
`git ls-remote origin refs/heads/main`.

### `main` bouge pendant ta preuve
Plusieurs versements par heure. `integrate` refuse tout non-fast-forward : rebaser, refaire `finish`, reverser.
Relire ce que `main` a apporté — un nouveau script de `tools/unreal/` arrive souvent avec.
