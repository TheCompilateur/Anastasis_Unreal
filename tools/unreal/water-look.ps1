param([switch]$Rebuild, [int]$TimeoutSec = 600)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
# WATER_LOOK_001 -- forge M_AnastasisWater (Single Layer Water). Voir water-look.py.
# Meme mecanique que shore-water.ps1 : -nullrhi, l'editeur ecrit un asset, ne regarde rien.
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'water-look.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_WATER_REBUILD = if ($Rebuild) { '1' } else { '0' }
$py = (Join-Path $Root 'tools\unreal\water-look.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-nullrhi', '-unattended', '-nosound', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'WATER_MATERIAL::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'WATER_MATERIAL' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
$asset = Join-Path $Root 'Content\Anastasis\Materials\M_AnastasisWater.uasset'
if (!(Test-Path $asset)) { throw 'WATER_MATERIAL::FAIL asset absent' }
Write-Output ('WATER_MATERIAL::PASS ' + $asset)
