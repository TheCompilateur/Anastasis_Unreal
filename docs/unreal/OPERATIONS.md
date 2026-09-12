# Local operations and recovery

From the canonical root: `powershell -ExecutionPolicy Bypass -File tools/unreal/anastasis-unreal.ps1 status|build|verify|editor` (select one command).

`verify` runs UBT's incremental check, starts its own Editor, loads FirstPerson, requests real PIE, checks current DEBUG markers, source/config fingerprint and module origins, then closes only its session. Logs and current hashes: Saved/CanonicalVerification/latest.json. It is a smoke gate, not a complete test suite or PLAYER acceptance. `editor` only requests opening; it does not certify launch. Scripts refuse any working directory outside the canonical root.

Local Git includes Source, Config, uproject, docs, tools and Content. Unreal binary assets use locally installed Git LFS. No remote exists. `.git/lfs/objects` is essential to recovery: a Git bundle alone does not contain LFS payloads. Preserve a complete verified local repository copy before any future relocation; do not move during this mission. Generated Binaries, Intermediate, Saved, DerivedDataCache and IDE/Python caches are excluded. Preexisting .cursor settings and workspace file are left outside the baseline; they remain on disk.

Git author for infrastructure baseline: Codex <codex@localhost>, repository-local only. The baseline contains preexisting project work; authorship of that work is not attributed to Codex.

ONEDRIVE_RISK: HIGH (operational exposure; no demonstrated data corruption).
Observed before Git initialization: Intermediate 3.488 GB; .vs 2.012 GB; Binaries 84.8 MB; Content 155.4 MB. All 553 Content files expose reparse attributes. Existing sibling copies required an explicit authority ruling. No conflict-named Content files found; sync status and active locks are not established. Gitignore does not exclude files from OneDrive syncing. Generated-file churn and the new local LFS/Git store remain in the sync-managed tree.
Future recommendation: separately authorize a verified copy to a dedicated local development root, update the root guard, verify LFS objects and source hashes, rerun verify, then change authority. No relocation or cleanup was executed.
