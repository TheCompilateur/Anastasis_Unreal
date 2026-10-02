param([string]$Label = 'latest', [string]$States = '', [int]$TimeoutSec = 2400)
# FOREST_COST_001 : cout GPU de la vegetation, strate par strate, aux memes cameras.
# Pilote vegetation-cost-capture.py (strates masquees a l'execution, rien de sauve).
# Sortie : Saved\VegetationCostEvidence\<Label>\vegetation-cost.json + <vue>_<etat>.png
#   -States 'all,notrees,all2' : sous-ensemble ordonne (defaut : all,notrees,nounder,nograss,bare,all2)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$out = Join-Path $Root "Saved\VegetationCostEvidence\$Label"
New-Item -ItemType Directory -Force $out | Out-Null
$log = Join-Path $out 'capture.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_VEGCOST_OUT = $out
$env:ANASTASIS_VEGCOST_STATES = $States
$py = (Join-Path $Root 'tools\unreal\vegetation-cost-capture.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-windowed', '-resx=1280', '-resy=720', '-nosplash', '-NoLiveCoding',
  # Hors premier plan, l'editeur coupe le rendu des viewports (cf. capture-sky.ps1).
  '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'VEGCOST::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'VEGCOST_' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') } | Select-Object -Unique
if (-not (Select-String -Path $log -Pattern 'VEGCOST_CAPTURE_COMPLETE' -Quiet)) { throw 'VEGCOST::FAIL capture incomplete' }
Write-Output ('VEGCOST::PASS ' + $out)
