param([int]$TimeoutSec = 900, [int]$Npcs = 12)
# Preuve PIE des portraits d'habitants (VILLAGER_PNG_001) : pilote villager-pie.py.
# Editeur discret (Start-AnastasisEditor), rendu garde hors focus pour les captures.
# Sortie : Saved/VillagerEvidence/pie/ (images, villager-pie.json, log).
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$out = Join-Path $Root 'Saved\VillagerEvidence\pie'
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -Filter *.png -ErrorAction SilentlyContinue | Remove-Item
$log = Join-Path $out 'villager-pie.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_VILLAGER_PIE_OUT = $out
$env:ANASTASIS_VILLAGER_PIE_NPCS = "$Npcs"
$py = (Join-Path $Root 'tools\unreal\villager-pie.py').Replace('\', '/')
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
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'VILLAGER_PIE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'VILLAGER_PIE |ANASTASIS_VILLAGE villager' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
if (-not (Select-String -Path $log -Pattern 'VILLAGER_PIE PASS' -Quiet)) { throw 'VILLAGER_PIE::FAIL voir ' + $log }
Write-Output ('VILLAGER_PIE::PASS out=' + $out)
