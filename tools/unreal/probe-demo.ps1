param([string]$Mission = 'phaseg', [string]$PreCmds = '', [string]$Bookmark = 'OVERVIEW', [int]$TimeoutSec = 300)
$ErrorActionPreference = 'Stop'
# Le root est deduit de l'emplacement du script, comme capture-slice.ps1 : un agent
# doit pouvoir prouver SON worktree. Coder en dur C:\dev\ANASTASIS_UNREAL faisait
# tourner la preuve sur les binaires du canonique, donc sur un autre code que celui
# qu'on croyait mesurer.
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir ('probe-demo-' + $Mission + '.log')
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_PROBE_MISSION = $Mission
$env:ANASTASIS_PROBE_BOOKMARK = $Bookmark
$py = (Join-Path $Root 'tools\unreal\probe-demo.py').Replace('\', '/')
# PreCmds : CVars a poser avant le demarrage (ex. 'anastasis.Atmosphere 0' pour une
# comparaison A/B). Les commandes s'executent dans l'ordre, la derniere lance le script.
$exec = if ($PreCmds) { $PreCmds + ',py ' + $py } else { 'py ' + $py }
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-windowed', '-resx=1280', '-resy=720', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="' + $exec + '"')
)
$p = Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'PROBE_DEMO::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'PROBE_DEMO_|ANASTASIS_WORLD_|ANASTASIS_ATMOSPHERE|CAPTURE::' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
Write-Output ('PROBE_DEMO::DONE log=' + $log)
