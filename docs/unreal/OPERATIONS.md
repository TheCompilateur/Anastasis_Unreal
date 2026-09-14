# Local operations and recovery

CANONICAL_ROOT: C:\dev\ANASTASIS_UNREAL
From this root: `powershell -ExecutionPolicy Bypass -File tools/unreal/anastasis-unreal.ps1 status|build|build-game|verify|health|editor` (select one command).
`verify` builds incrementally, opens a dedicated Editor, loads FirstPerson, runs PIE, checks DEBUG markers, source/config fingerprints and DLL origins, then closes its session (editor wall-clock budget 12 minutes; 4 minutes timed out on a cold worktree DDC while other agents had UnrealEditor-Cmd running). Output: Saved/CanonicalVerification/latest.json. This does not implement PLAYER or certify all tests. Culture-sensitive engine smoke failures are classified in AUTOMATION_TRIAGE.md.
`health` reads those proofs and prints a PASS/STALE/UNKNOWN/FAIL report. It never promotes a missing proof to PASS. Cartography: docs/unreal/PROJECT_HEALTH.md.
`build-game` is the Game target (`Anastasis_UnrealV2` Win64 Development). `build` remains Editor-only so the pre-push gate stays a compile check.

Git history and all 542 LFS assets are local; no remote, alternate object directory, linked worktree or cloud placeholder is required. Preserve .git/lfs/objects with Git history for recovery; a Git bundle alone omits LFS payloads. Source/Config/Content were hash-compared before infrastructure edits. Local .cursor settings and workspace file were copied unchanged but remain untracked; they are not part of the build/PIE operator contract. Historical reports retain their original paths as provenance.

Previous OneDrive project remains present, marked PREVIOUS_CANONICAL / READ_ONLY_BACKUP_CANDIDATE / DO_NOT_DEVELOP. Its Source/Content/Config were not changed. It is not a current backup of future local commits. No relocation by deletion was performed.

ONEDRIVE_DEPENDENCY: NONE for the canonical Git/LFS/build/Editor/PIE path. Generated state is regenerated locally.

BACKUP: `origin` = https://github.com/TheCompilateur/Anastasis_Unreal (private), Git history + LFS objects pushed 2026-09-12. Push after future canonical commits to keep it current; nothing pushes automatically.

## Observer le monde

Ouvrir l'editeur suffit. `EditorStartupMap` et `GameDefaultMap` pointent sur
`/Game/Anastasis/Maps/Lvl_AnastasisSlice`, et `LoadLevelAtStartup=ProjectDefault`
est fige explicitement dans `Config/DefaultEditorPerProjectUserSettings.ini` --
sans quoi l'editeur rouvre le dernier niveau ouvert et ignore ce defaut.

Le niveau ne contient aucune verite de monde : il porte un
`AAnastasisWorldEmbodiment` vide, plus la lumiere, la camera et le post-process.
Le terrain est regenere depuis la seed a chaque chargement -- par `OnConstruction`
en monde editeur, par `BeginPlay` en jeu. Les composants qu'il remplit sont
`RF_Transient` : sauvegarder le niveau ne fige jamais les instances dedans, et le
niveau reste vide de verite de monde. Le GameMode ne spawne un embodiment que si
le niveau n'en place pas deja un (`ShouldSpawnEmbodiment`), sinon `Lvl_AnastasisSlice`
en empilait deux a la meme origine.

Leviers, tous des CVars, aucune n'est necessaire pour voir la carte :

| CVar | Defaut | Effet |
|---|---|---|
| `anastasis.Terrain.Surface` | `2` | `2` surface sur toute l'emprise, `1` tranche scellee 32x32, `0` dalles DEBUG |
| `anastasis.WorldView.Seed` | `12345` | seed de `GenerateWorld(seed, 96, 96)` |
| `anastasis.WorldView.Width` / `.Height` | `96` | crop applique au monde canonique |
| `anastasis.WorldView.CropX` / `.CropY` | `0` | origine du crop |
| `anastasis.Visual.Mode` | `1` (DEBUG) | `0` n'incarne rien, `2` PLAYER non implemente |

Une CVar n'est lue qu'a l'incarnation : la changer en cours de PIE demande un
stop/Play, ou un bouton `Show...` dans le panneau Details de l'embodiment.

En PIE : `Anastasis.World.Status`, `Anastasis.World.Snapshot`,
`Anastasis.World.Capture <bookmark>`. Le probe est un `UWorldSubsystem` amorce
sur `OnWorldBeginPlay` -- hors PIE il ne sert a rien.

Capture reproductible hors session : `tools/unreal/capture-slice.ps1 -Mode 2 -Out x.png`.

## Automated gates

Two tiers, matched to how slow each check is:

- **Pre-push gate (fast, blocking).** `tools/git-hooks/pre-push` runs `anastasis-unreal.ps1 build` (incremental compile, no editor) and refuses the push on `BUILD::FAIL`, **puis passe la main a `git lfs pre-push`**. Ce second appel n'est pas optionnel : `core.hooksPath` masque `.git/hooks`, donc le pre-push que Git LFS installe lui-meme ne tourne plus. Il avait ete omis a la creation de ce dossier — tout push referencait alors des objets LFS que le remote n'avait jamais recus et GitHub le refusait (`GH008`, `pre-receive hook declined`). Le 2026-09-13, 570 objets / 219 Mo ont du etre remis a la main par `git lfs push --all origin main`. Enabled per-clone via a local git config (`core.hooksPath` is not itself versioned):
  ```
  git config core.hooksPath tools/git-hooks
  ```
  Already set in this working copy. Re-run after a fresh clone.

- **Scheduled full verify (slow, non-blocking).** Registered as the `Anastasis-ScheduledVerify` Task Scheduler entry, daily at 3am:
  ```powershell
  $Action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument '-NoProfile -ExecutionPolicy Bypass -File "C:\dev\ANASTASIS_UNREAL\tools\unreal\scheduled-verify.ps1"'
  $Trigger = New-ScheduledTaskTrigger -Daily -At 3am
  Register-ScheduledTask -TaskName 'Anastasis-ScheduledVerify' -Action $Action -Trigger $Trigger -Description 'Nightly full canonical verify (build + Editor + PIE smoke) and classified Automation suite'
  ```
  `tools/unreal/scheduled-verify.ps1` Set-Location to the project root (Task Scheduler otherwise starts in System32 — that was the 2026-09-13 03:00 VERIFY_FAIL with TESTS_PASS and no editor log), then runs two independent checks, neither skipped because the other failed, both appended to `Saved/CanonicalVerification/scheduled-verify.log` (`latest.json` is overwritten each verify run by `anastasis-unreal.ps1` and is not history; this log is). On VERIFY_FAIL the last 60 lines of verify output are kept, so the inner `VERIFY::FAIL` / `BUILD::FAIL` reason is not discarded:
  1. `verify` (build + dedicated Editor + PIE smoke, DEBUG markers, module origin) — `VERIFY_PASS`/`VERIFY_FAIL`.
  2. `report-tests.ps1` (the full `Anastasis` Automation suite, ~15 min budget) — cross-references `tools/unreal/known-expected-failures.txt` and reports three categories, not two: `PASS`, `KNOWN_EXPECTED_FAILURE`, `FAIL`. A bare Unreal "Success" is never counted as a real pass when the test is a marked known divergence. Logged as `TESTS_PASS`/`TESTS_FAIL` plus the full per-test breakdown.

  Both checks now run nightly; before this, only #1 ran, so the classified Automation suite (and any regression among the 4 marked known-failure tests going quietly unmarked) was never actually checked on a schedule.

## Integration — qui a le droit de deplacer `main`

**Regle : aucun agent ne deplace `main` depuis sa propre branche. Seule une passe d'integration le fait.**

Le 2026-09-13, quatre acteurs differents ont fast-forwarde `main` depuis leur branche (`multi-agent-control-001`,
`terrain-surface-world`, `visual-build-002`, `dressing-on-surface`). Personne n'a compile la combinaison. Un
`WriteJson` defini a l'identique dans `AnastasisWorldProbePhase3.cpp` et `AnastasisWorldProbeSubsystem.cpp` a
survecu a ces quatre fast-forwards : chaque agent compilait sa branche dans son worktree, ou UBT passe en
non-unity les fichiers que `git status` voit sales. La collision n'existait que dans l'assemblage — et
l'assemblage n'etait l'affaire de personne.

### La passe

1. Worktree neuf, que personne n'occupe, depuis `main` :
   ```
   git worktree add C:/dev/ANASTASIS_WORKTREES/trunk-integration -b agent/trunk-integration main
   ```
2. Fusionner chaque branche vivante. `git merge-tree --write-tree main <branche>` dit a sec lesquelles
   conflitent, sans rien muter.
3. **Compiler.** `anastasis-unreal.ps1 build` depuis le worktree d'integration. Une fusion textuelle propre
   ne prouve rien : c'est cette etape, et elle seule, qui a attrape `WriteJson`.
4. Verser : `git -C <racine-canonique> merge --ff-only agent/trunk-integration`. Refuser si ce n'est pas un
   fast-forward — sinon quelqu'un a bouge `main` pendant la passe et il faut la refaire.
5. Pousser immediatement (voir ci-dessous).
6. Elaguer branches et worktrees dont tous les commits sont dans `main` :
   `git rev-list --count main..<branche>` == 0 et `git -C <worktree> status --porcelain` vide.

### Avant de compiler ou de verser

Aucun editeur Unreal ouvert sur la racine canonique : il tient
`Binaries/Win64/UnrealEditor-Anastasis_UnrealV2.dll` en verrou d'ecriture, le link echoue en `LNK1104` et le
gate refuse le push. Un worktree d'agent a ses propres `Binaries` et ne gene pas. Depuis 2026-09-13,
`BuildCanonical` court-circuite quand sources et DLL sont inchangees, ce qui evite le relink inutile — mais
un vrai changement de source demande toujours la racine libre.

### Push

`origin` n'est jamais automatique. Le 2026-09-13 il avait pris 16 commits de retard : une journee entiere de
travail n'existant que sur un seul disque. Pousser apres chaque passe d'integration, pas quand on y pense.
