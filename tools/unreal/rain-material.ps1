param([switch]$Rebuild, [int]$TimeoutSec = 600)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
# RAIN_001 -- forge M_AnastasisRain et SM_AnastasisRainStreak. Voir rain-material.py.
# Meme mecanique que water-look.ps1 : -nullrhi, carte neutre, l'editeur ecrit deux assets.
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'rain-material.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_RAIN_REBUILD = if ($Rebuild) { '1' } else { '0' }
$py = (Join-Path $Root 'tools\unreal\rain-material.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '/Engine/Maps/Entry',
  '-nullrhi', '-unattended', '-nosound', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'RAIN_MATERIAL::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'RAIN_MATERIAL|RAIN_MESH' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
$lines = @(Get-Content $log)
if (@($lines | Where-Object { $_ -match 'LogPython: Error|RAIN_MATERIAL_FAIL|RAIN_MATERIAL_WIRING_INCOMPLETE|Traceback' }).Count) { throw 'RAIN_MATERIAL::FAIL erreur Python (voir le log)' }
$at = -1; for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match 'RAIN_MATERIAL_COMPILE') { $at = $i } }
if ($at -ge 0 -and @($lines[$at..($lines.Count - 1)] | Where-Object { $_ -match 'Failed to compile|\[SM[56]\].*error' }).Count) { throw 'RAIN_MATERIAL::FAIL compilation du materiau' }
foreach ($a in @('Content\Anastasis\Weather\M_AnastasisRain.uasset', 'Content\Anastasis\Weather\SM_AnastasisRainStreak.uasset')) {
  if (!(Test-Path (Join-Path $Root $a))) { throw ('RAIN_MATERIAL::FAIL asset absent ' + $a) }
}
Write-Output 'RAIN_MATERIAL::PASS'
