param([int]$TimeoutSec=900)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'docs\visual\hydra-001'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_HYDRA_OUT=$dir
$py=(Join-Path $Root 'tools\unreal\hydra-capture.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){
 Stop-Process -Id $p.Id -Force
 throw 'CAPTURE::FAIL editeur bloque'
}
Select-String -Path $log -Pattern 'HYDRA_|ANASTASIS_HYDROLOGY' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
$needed=@(
 'torrent_before.png','torrent_after.png',
 'valley_before.png','valley_after.png',
 'inflow_before.png','inflow_after.png',
 'aerial_after.png','oblique_after.png',
 'human_torrent.png','human_inflow.png'
)
foreach($name in $needed){
 $shot=Join-Path $dir $name
 if(!(Test-Path $shot)){throw "CAPTURE::FAIL missing $name"}
}
Write-Output ('CAPTURE::PASS ' + $dir)
