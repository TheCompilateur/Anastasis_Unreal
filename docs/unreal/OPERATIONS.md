# Local operations and recovery

CANONICAL_ROOT: C:\dev\ANASTASIS_UNREAL
From this root: `powershell -ExecutionPolicy Bypass -File tools/unreal/anastasis-unreal.ps1 status|build|verify|editor` (select one command).
`verify` builds incrementally, opens a dedicated Editor, loads FirstPerson, runs PIE, checks DEBUG markers, source/config fingerprints and DLL origins, then closes its session. Output: Saved/CanonicalVerification/latest.json. This does not implement PLAYER or certify all tests. Culture-sensitive engine smoke failures are classified in AUTOMATION_TRIAGE.md.

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
Le terrain est regenere depuis la seed a chaque chargement, par `OnConstruction`
en monde editeur et par `BeginPlay` en jeu. Les composants qu'il remplit sont
`RF_Transient` : sauvegarder le niveau ne fige jamais les 9216 instances dedans.

Leviers, tous des CVars, aucune n'est necessaire pour voir la carte :

| CVar | Defaut | Effet |
|---|---|---|
| `anastasis.Terrain.Surface` | `1` | `1` surface continue, `0` cubes DEBUG |
| `anastasis.WorldView.Seed` | `12345` | seed de `GenerateWorld(seed, 96, 96)` |
| `anastasis.WorldView.Width` / `.Height` | `96` | crop applique au monde canonique |
| `anastasis.WorldView.CropX` / `.CropY` | `0` | origine du crop |
| `anastasis.Visual.Mode` | `1` (DEBUG) | `0` n'incarne rien, `2` PLAYER non implemente |

En PIE : `Anastasis.World.Status`, `Anastasis.World.Snapshot`,
`Anastasis.World.Capture <bookmark>`. Le probe est un `UWorldSubsystem` amorce
sur `OnWorldBeginPlay` -- hors PIE il ne sert a rien.

Capture reproductible hors session : `tools/unreal/capture-slice.ps1 -Mode 1 -Out x.png`.

## Automated gates

Two tiers, matched to how slow each check is:

- **Pre-push gate (fast, blocking).** `tools/git-hooks/pre-push` runs `anastasis-unreal.ps1 build` (incremental compile, no editor) and refuses the push on `BUILD::FAIL`. Enabled per-clone via a local git config (`core.hooksPath` is not itself versioned):
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
  `tools/unreal/scheduled-verify.ps1` runs two independent checks, neither skipped because the other failed, both appended to `Saved/CanonicalVerification/scheduled-verify.log` (`latest.json` is overwritten each verify run by `anastasis-unreal.ps1` and is not history; this log is):
  1. `verify` (build + dedicated Editor + PIE smoke, DEBUG markers, module origin) — `VERIFY_PASS`/`VERIFY_FAIL`.
  2. `report-tests.ps1` (the full `Anastasis` Automation suite, ~15 min budget) — cross-references `tools/unreal/known-expected-failures.txt` and reports three categories, not two: `PASS`, `KNOWN_EXPECTED_FAILURE`, `FAIL`. A bare Unreal "Success" is never counted as a real pass when the test is a marked known divergence. Logged as `TESTS_PASS`/`TESTS_FAIL` plus the full per-test breakdown.

  Both checks now run nightly; before this, only #1 ran, so the classified Automation suite (and any regression among the 4 marked known-failure tests going quietly unmarked) was never actually checked on a schedule.
