# HANDOFF: prepush-filehash-fallback-001

## MISSION

Rendre le gate pre-push robuste quand `powershell.exe` lance
`anastasis-unreal.ps1` depuis l'environnement `sh` de Git.

## FILES_OWNED

- tools/unreal/anastasis-unreal.ps1
- docs/unreal/handoffs/prepush-filehash-fallback-001.md

## COMMIT

BRANCH_HEAD

## MEC

- STATUS: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\unreal\anastasis-unreal.ps1 status`
- BUILD: DEFERRED_TO_CANONICAL_PRE_PUSH
- HOOK-ENV STATUS: `C:\Program Files\Git\usr\bin\sh.exe -lc 'cd /c/dev/ANASTASIS_WORKTREES/prepush-filehash-fallback-001 && powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/unreal/anastasis-unreal.ps1 status'`
- DIFF CHECK: `git diff --check`

## SCN

NOT_ATTEMPTED

## PLY

NOT_ATTEMPTED

## INTEGRATION_RISK

- `tools/unreal/anastasis-unreal.ps1` est appele par le hook pre-push et par les
  operations build/verify/editor.
- Le correctif remplace `Get-FileHash` par SHA256 .NET local pour ne plus
  dependre de l'autoload PowerShell Utility dans l'environnement Git hook.
- Aucun comportement Unreal, C++, Content ou Config n'est modifie.

## STOP

Ne revendique pas une nouvelle verification Editor/PIE, une correction de test
Automation, ni une preuve joueur.
