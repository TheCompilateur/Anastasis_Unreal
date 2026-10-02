param([int]$TimeoutSec = 1500)
# Preuve PIE du joueur minimal (player-minimal-001) : pilote player-pie.py.
# Editeur discret (Start-AnastasisEditor). Aucun asset sauve.
# Sortie : Saved/PlayerEvidence/pie/ (player-pie.json, log).
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$out = Join-Path $Root 'Saved\PlayerEvidence\pie'
New-Item -ItemType Directory -Force $out | Out-Null
$log = Join-Path $out 'player-pie.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_PLAYER_PIE_OUT = $out
$py = (Join-Path $Root 'tools\unreal\player-pie.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-windowed', '-resx=1280', '-resy=720', '-nosplash', '-NoLiveCoding',
  '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'PLAYER_PIE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'PLAYER_PIE |PLAYER_PIE_STATUS|ANASTASIS_PLAYER |ANASTASIS_SIM advance' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
if (-not (Select-String -Path $log -Pattern 'PLAYER_PIE PASS' -Quiet)) { throw 'PLAYER_PIE::FAIL voir ' + $log }
Write-Output ('PLAYER_PIE::PASS out=' + $out)
