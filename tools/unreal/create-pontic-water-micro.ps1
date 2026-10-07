# Regenerate four owned meshes on /Engine/Maps/Entry, without loading the embodied world.
param([int]$TimeoutSec=1200)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\PonticWaterMicroEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'create-pontic-water-micro.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_PONTIC_MICRO_QUIT='1'
$py=(Join-Path $Root 'tools\unreal\create-pontic-water-micro.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '/Engine/Maps/Entry','-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){Stop-Process -Id $p.Id -Force; throw 'PONTIC_WATER_MICRO_ASSETS::FAIL editor timeout'}
Select-String -Path $log -Pattern 'PONTIC_WATER_MICRO_ASSETS|create-ground-cover.*SAVED' |
  ForEach-Object { $_.Line } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'PONTIC_WATER_MICRO_ASSETS PASS' -Quiet)){
  throw 'PONTIC_WATER_MICRO_ASSETS::FAIL see ' + $log
}
