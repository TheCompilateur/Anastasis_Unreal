# Local operations and recovery

CANONICAL_ROOT: C:\dev\ANASTASIS_UNREAL
From this root: `powershell -ExecutionPolicy Bypass -File tools/unreal/anastasis-unreal.ps1 status|build|verify|editor` (select one command).
`verify` builds incrementally, opens a dedicated Editor, loads FirstPerson, runs PIE, checks DEBUG markers, source/config fingerprints and DLL origins, then closes its session. Output: Saved/CanonicalVerification/latest.json. This does not implement PLAYER or certify all tests. Culture-sensitive engine smoke failures are classified in AUTOMATION_TRIAGE.md.

Git history and all 542 LFS assets are local; no remote, alternate object directory, linked worktree or cloud placeholder is required. Preserve .git/lfs/objects with Git history for recovery; a Git bundle alone omits LFS payloads. Source/Config/Content were hash-compared before infrastructure edits. Local .cursor settings and workspace file were copied unchanged but remain untracked; they are not part of the build/PIE operator contract. Historical reports retain their original paths as provenance.

Previous OneDrive project remains present, marked PREVIOUS_CANONICAL / READ_ONLY_BACKUP_CANDIDATE / DO_NOT_DEVELOP. Its Source/Content/Config were not changed. It is not a current backup of future local commits. No relocation by deletion was performed.

ONEDRIVE_DEPENDENCY: NONE for the canonical Git/LFS/build/Editor/PIE path. Generated state is regenerated locally.

BACKUP: `origin` = https://github.com/TheCompilateur/Anastasis_Unreal (private), Git history + LFS objects pushed 2026-09-12. Push after future canonical commits to keep it current; nothing pushes automatically.

## Automated gates

Two tiers, matched to how slow each check is:

- **Pre-push gate (fast, blocking).** `tools/git-hooks/pre-push` runs `anastasis-unreal.ps1 build` (incremental compile, no editor) and refuses the push on `BUILD::FAIL`. Enabled per-clone via a local git config (`core.hooksPath` is not itself versioned):
  ```
  git config core.hooksPath tools/git-hooks
  ```
  Already set in this working copy. Re-run after a fresh clone.

- **Scheduled full verify (slow, non-blocking).** `tools/unreal/scheduled-verify.ps1` runs the full `verify` (build + dedicated Editor + PIE smoke, ~4+ min) and appends one `PASS`/`FAIL` line per run to `Saved/CanonicalVerification/scheduled-verify.log` (`latest.json` is overwritten each run, not history). Not wired to a Task Scheduler entry yet — register one to run it periodically, e.g.:
  ```powershell
  $Action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument '-NoProfile -ExecutionPolicy Bypass -File "C:\dev\ANASTASIS_UNREAL\tools\unreal\scheduled-verify.ps1"'
  $Trigger = New-ScheduledTaskTrigger -Daily -At 3am
  Register-ScheduledTask -TaskName 'Anastasis-ScheduledVerify' -Action $Action -Trigger $Trigger -Description 'Nightly full canonical verify (build + Editor + PIE smoke)'
  ```
  A pre-existing failure recorded in AUTOMATION_TRIAGE.md is not caught by either gate — `verify` checks DEBUG markers and module origin, not the full Automation test suite.
