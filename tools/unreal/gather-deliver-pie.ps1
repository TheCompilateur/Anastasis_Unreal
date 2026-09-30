param([int]$TimeoutSec = 900)
# Preuve PIE du fermier au grenier (gather-deliver-001) : pilote gather-deliver-pie.py.
# Editeur discret (Start-AnastasisEditor), rendu garde hors focus pour les captures.
# Sortie : Saved/SliceEvidence/gather-deliver/ (images, gather-deliver.json, log).
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$out = Join-Path $Root 'Saved\SliceEvidence\gather-deliver'
New-Item -ItemType Directory -Force $out | Out-Null
$log = Join-Path $out 'gather-deliver-pie.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_GATHER_OUT = $out
$py = (Join-Path $Root 'tools\unreal\gather-deliver-pie.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-windowed', '-resx=1600', '-resy=900', '-nosplash', '-NoLiveCoding',
  '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'GATHER_DELIVER::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'GATHER_DELIVER |ANASTASIS_VILLAGE (first farmer|npc|building)' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
Write-Output ('GATHER_DELIVER::DONE out=' + $out)
