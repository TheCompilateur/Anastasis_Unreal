param([int]$TimeoutSec = 1200)
# Preuve PIE des corps 3D des habitants (VILLAGER_BODY_3D_001) : pilote villager-body-pie.py.
# Editeur discret (Start-AnastasisEditor), rendu garde hors focus pour les captures.
# Sortie : Saved/VillagerEvidence/body/ (images, villager-body-pie.json, log).
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$out = Join-Path $Root 'Saved\VillagerEvidence\body'
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -Filter *.png -ErrorAction SilentlyContinue | Remove-Item
$log = Join-Path $out 'villager-body-pie.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_VILLAGER_BODY_OUT = $out
$py = (Join-Path $Root 'tools\unreal\villager-body-pie.py').Replace('\', '/')
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
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'VILLAGER_BODY_PIE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'VILLAGER_BODY_PIE |ANASTASIS_VILLAGE villager' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
if (-not (Select-String -Path $log -Pattern 'VILLAGER_BODY_PIE PASS' -Quiet)) { throw 'VILLAGER_BODY_PIE::FAIL voir ' + $log }
Write-Output ('VILLAGER_BODY_PIE::PASS out=' + $out)
