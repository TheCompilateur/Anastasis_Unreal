param([int]$TimeoutSec=480)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\LithosEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_LITHOS_OUT=$dir
$py=(Join-Path $Root 'tools/unreal/capture-lithos-views.py').Replace('\','/')
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
 throw 'LITHOS_CAPTURE::FAIL editeur bloque'
}
Select-String -Path $log -Pattern 'LITHOS_|ANASTASIS_LITHOS' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
$needed=@('A_aerial_before.png','B_aerial_after.png','C_oblique_after.png','D_human_cliff.png','E_human_talus.png')
foreach($name in $needed){
 $shot=Join-Path $dir $name
 if(!(Test-Path $shot)){throw "LITHOS_CAPTURE::FAIL missing $name"}
}
Write-Output ('LITHOS_CAPTURE::PASS ' + $dir)
