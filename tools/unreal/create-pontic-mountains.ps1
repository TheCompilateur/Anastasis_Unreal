# PONTIC_MOUNTAINS_001: generate the eight real StaticMesh mountain assets.
param([int]$TimeoutSec=900)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\PonticMountainEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'create-pontic-mountains.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_PONTIC_MOUNTAINS_QUIT='1'
$py=(Join-Path $Root 'tools\unreal\create-pontic-mountains.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '/Engine/Maps/Entry',
 '-nullrhi','-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'PONTIC_MOUNTAINS::FAIL editeur bloque' }
if(!(Test-Path $log)){ throw 'PONTIC_MOUNTAINS::FAIL aucun journal editeur' }
Select-String -Path $log -Pattern 'PONTIC_MOUNTAIN_ASSET|PONTIC_MOUNTAINS::' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'PONTIC_MOUNTAINS::PASS count=8' -Quiet)){
  throw ('PONTIC_MOUNTAINS::FAIL voir ' + $log)
}
