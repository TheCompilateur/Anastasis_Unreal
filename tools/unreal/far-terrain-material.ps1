# CONTINENTAL_001 -- (re)genere M_AnastasisFarTerrain, le materiau des montagnes lointaines, dans un editeur
# dedie, discret, qui se ferme. Voir far-terrain-material.py (source d'autorite de l'asset).
param([int]$TimeoutSec=900)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\FarTerrainEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'far-terrain-material.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_FAR_TERRAIN_QUIT='1'
$py=(Join-Path $Root 'tools\unreal\far-terrain-material.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '/Engine/Maps/Entry',
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'FAR_TERRAIN_MATERIAL::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'far-terrain-material|FAR_TERRAIN' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'FAR_TERRAIN_MATERIAL::PASS' -Quiet)){ throw ('FAR_TERRAIN_MATERIAL::FAIL voir ' + $log) }
