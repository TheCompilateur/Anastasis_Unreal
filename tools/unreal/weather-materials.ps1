param([switch]$Rebuild, [int]$TimeoutSec = 600)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
# WATER_LOOK_001 -- forge M_AnastasisWater (Single Layer Water). Voir weather-materials.py.
# Meme mecanique que shore-water.ps1 : -nullrhi, l'editeur ecrit un asset, ne regarde rien.
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'weather-materials.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_WATER_REBUILD = if ($Rebuild) { '1' } else { '0' }
$py = (Join-Path $Root 'tools\unreal\weather-materials.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  # Carte neutre : Lvl_AnastasisSlice incarnerait le monde et tiendrait le materiau (cf. create-ground-cover.ps1).
  '/Engine/Maps/Entry',
  '-nullrhi', '-unattended', '-nosound', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'WATER_MATERIAL::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'WEATHER_MATERIALS|WATER_MATERIAL|TREE_ASSET' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
# Un cablage rate ou une exception remontent en LogPython Error ; une compilation ratee n'est
# qu'un Warning, d'ou la recherche apres le marqueur WATER_MATERIAL_COMPILE.
$lines = @(Get-Content $log)
if (@($lines | Where-Object { $_ -match 'LogPython: Error|WATER_MATERIAL_FAIL|WATER_MATERIAL_WIRING_INCOMPLETE|Traceback' }).Count) { throw 'WATER_MATERIAL::FAIL erreur Python (voir le log)' }
if (@($lines | Where-Object { $_ -match 'Failed to compile|\[SM[56]\].*error' }).Count) { throw 'WEATHER_MATERIALS::FAIL shader compilation' }
$asset = Join-Path $Root 'Content\Anastasis\Materials\M_AnastasisWater.uasset'
if (!(Test-Path $asset)) { throw 'WATER_MATERIAL::FAIL asset absent' }
if (!(Select-String -Path $log -Pattern 'WEATHER_MATERIALS PASS' -Quiet)) { throw 'WEATHER_MATERIALS::FAIL' }
Write-Output ('WEATHER_MATERIALS::PASS ' + $asset)
