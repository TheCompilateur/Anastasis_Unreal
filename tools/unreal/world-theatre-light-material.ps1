# WORLD_THEATRE v2.1 -- (re)genere M_WorldTheatreDistance et M_WorldTheatreDepthProbe (/Game/WorldTheatre) dans un
# editeur dedie, discret, qui se ferme. Voir world-theatre-light-material.py (source d'autorite des deux assets).
# Refuse tout « Failed to compile » de ces materiaux apres le marqueur de compilation : un graphe casse ne leve rien.
param([int]$TimeoutSec=900)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\WorldTheatreEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'world-theatre-light-material.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_THEATRE_MATERIAL_QUIT='1'
$py=(Join-Path $Root 'tools\unreal\world-theatre-light-material.py').Replace('\','/')
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
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'WORLD_THEATRE_LIGHT_MATERIAL::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'world-theatre-light-material|WORLD_THEATRE_(LIGHT_)?MATERIAL' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
$lines = Get-Content $log
$mark = ($lines | Select-String 'WORLD_THEATRE_MATERIAL_COMPILE_BEGIN' | Select-Object -First 1).LineNumber
$broken = if ($mark) { $lines[$mark..($lines.Count-1)] | Select-String 'Failed to compile Material.*WorldTheatre' } else { $null }
if ($broken) { $broken | ForEach-Object { $_.Line }; throw 'WORLD_THEATRE_LIGHT_MATERIAL::FAIL un materiau ne compile pas' }
if(-not (Select-String -Path $log -Pattern 'WORLD_THEATRE_LIGHT_MATERIAL::PASS' -Quiet)){ throw ('WORLD_THEATRE_LIGHT_MATERIAL::FAIL voir ' + $log) }
