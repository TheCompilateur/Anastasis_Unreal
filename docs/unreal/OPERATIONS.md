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
  `tools/unreal/scheduled-verify.ps1` Set-Location to the project root (Task Scheduler otherwise starts in System32 — that was the 2026-09-13 03:00 VERIFY_FAIL with TESTS_PASS and no editor log), then runs two independent checks, neither skipped because the other failed, both appended to `Saved/CanonicalVerification/scheduled-verify.log` (`latest.json` is overwritten each verify run by `anastasis-unreal.ps1` and is not history; this log is). On VERIFY_FAIL the last 60 lines of verify output are kept, so the inner `VERIFY::FAIL` / `BUILD::FAIL` reason is not discarded:
  1. `verify` (build + dedicated Editor + PIE smoke, DEBUG markers, module origin) — `VERIFY_PASS`/`VERIFY_FAIL`.
  2. `report-tests.ps1` (the full `Anastasis` Automation suite, ~15 min budget) — cross-references `tools/unreal/known-expected-failures.txt` and reports three categories, not two: `PASS`, `KNOWN_EXPECTED_FAILURE`, `FAIL`. A bare Unreal "Success" is never counted as a real pass when the test is a marked known divergence. Logged as `TESTS_PASS`/`TESTS_FAIL` plus the full per-test breakdown.

  Both checks now run nightly; before this, only #1 ran, so the classified Automation suite (and any regression among the 4 marked known-failure tests going quietly unmarked) was never actually checked on a schedule.
