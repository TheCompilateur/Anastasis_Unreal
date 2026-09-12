# Migration 005 — 2026-09-12

CANONICAL_ROOT: C:\dev\ANASTASIS_UNREAL
PREVIOUS_CANONICAL: C:\Users\alex_\OneDrive\Documents\Unreal Projects\Anastasis_UnrealV2 5.8
SOURCE_BASELINE: 566f65cc8a86ddf3290acd7ff294c87cc5e02685

Before promotion: independent byte copy of all 663 tracked and 8 untracked files plus the complete .git directory, including LFS objects. No hardlinks, symlinks, junctions, reparse points, Git alternates, external LFS storage or remotes. Initial full file SHA256 parity PASS; initial git diff empty. Git fsck passes (one harmless unreachable blob retained); LFS fsck PASS, 542 assets. Free space before copy: 102,096,556,032 bytes.

Binaries/Intermediate/Saved/DerivedDataCache/.vs and generated solution files were not migrated. Destination clean UBT build ran 20 actions successfully, including both module DLLs. New Editor session loaded destination DLLs, FirstPerson and PIE; markers DEBUG, source=96x96, crop=(0,0) 96x96, tiles=9216, instances=9216. See evidence/clean-build-005.txt and clean-seal-005.json. Promotion only followed these gates; operator verify repeated after promotion.

Only identity/operations docs and operator path guards changed. Source/Config/Content/uproject and eight untracked local settings compared unchanged against the original manifest after promotion. Historical evidence remains historical; fresh proofs are explicitly suffixed 005. The original remains on disk: its root marker, canonical state and operator guard record retirement. No filesystem permission change, deletion, source modification or remote creation.

ONEDRIVE_DEPENDENCY: NONE for repository, LFS, build and smoke runtime. Historical source references are not execution dependencies. Untracked personal settings copied verbatim, excluded from the smoke tooling contract and from commits.
