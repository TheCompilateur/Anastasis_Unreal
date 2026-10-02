# Quatre lectures, un editeur apres l'autre : le sol, puis les primitifs (couronne, ecorce, pierre).
# Le grain du sol meurt vers 40 m, le trait d'eau accroche, la couronne cesse d'etre une boule.
param([int]$TimeoutSec = 900)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
& (Join-Path $PSScriptRoot 'ground-material.ps1') -Rebuild -TimeoutSec $TimeoutSec
if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $null) { throw "POLY_REALISM::FAIL ground exit $LASTEXITCODE" }

$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'poly-realism-materials.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_TREE_MATERIALS_ONLY = '1'
$py = (Join-Path $Root 'tools\unreal\create_tree_asset.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-unattended', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'POLY_REALISM::FAIL editeur materiaux bloque' }
Select-String -Path $log -Pattern 'RESULT::|MATERIAL saved|BARK_MATERIAL saved|ROCK_MATERIAL saved|Error|Traceback' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
$lines = Get-Content $log
$mark = ($lines | Select-String -Pattern 'MATERIAL saved' | Select-Object -First 1)
if (-not $mark) { throw 'POLY_REALISM::FAIL materiaux incomplets' }
$after = $lines[($mark.LineNumber - 1)..($lines.Count - 1)]
$broken = $after | Select-String -Pattern 'Failed to compile Material|LogPython: Error|Traceback'
if ($broken) { $broken | ForEach-Object { Write-Output $_.Line }; throw 'POLY_REALISM::FAIL script' }
if (-not (Select-String -Path $log -Pattern 'RESULT::PASS materials_only=1' -Quiet)) { throw 'POLY_REALISM::FAIL materiaux incomplets' }
Write-Output 'POLY_REALISM::PASS'
