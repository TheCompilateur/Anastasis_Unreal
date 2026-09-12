# Local operations and recovery

CANONICAL_ROOT: C:\dev\ANASTASIS_UNREAL
From this root: `powershell -ExecutionPolicy Bypass -File tools/unreal/anastasis-unreal.ps1 status|build|verify|editor` (select one command).
`verify` builds incrementally, opens a dedicated Editor, loads FirstPerson, runs PIE, checks DEBUG markers, source/config fingerprints and DLL origins, then closes its session. Output: Saved/CanonicalVerification/latest.json. This does not implement PLAYER or certify all tests. Culture-sensitive engine smoke failures are classified in AUTOMATION_TRIAGE.md.

Git history and all 542 LFS assets are local; no remote, alternate object directory, linked worktree or cloud placeholder is required. Preserve .git/lfs/objects with Git history for recovery; a Git bundle alone omits LFS payloads. Source/Config/Content were hash-compared before infrastructure edits. Local .cursor settings and workspace file were copied unchanged but remain untracked; they are not part of the build/PIE operator contract. Historical reports retain their original paths as provenance.

Previous OneDrive project remains present, marked PREVIOUS_CANONICAL / READ_ONLY_BACKUP_CANDIDATE / DO_NOT_DEVELOP. Its Source/Content/Config were not changed. It is not a current backup of future local commits. No relocation by deletion was performed.

ONEDRIVE_DEPENDENCY: NONE for the canonical Git/LFS/build/Editor/PIE path. Generated state is regenerated locally. Dedicated independent backup remains future work.
