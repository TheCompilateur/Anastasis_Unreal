# EYE_PLANE_001 : meme cadrage a 1,7 m, anastasis.Depth.EyePlane 0 puis 1.
# Sortie : Saved\EyePlaneEvidence\<Label>\eye_off.png, eye_on.png, eye-plane.json
param([string]$Label='latest', [int]$TimeoutSec=900)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\EyePlaneEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_EYE_OUT=$dir
$py=(Join-Path $Root 'tools\unreal\eye-plane-capture.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'CAPTURE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'EYE_VIEW|EYE_STATE|EYE_SHOT|EYE_CAPTURE|ANASTASIS_EYE_PLANE' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'EYE_CAPTURE_COMPLETE' -Quiet)){ throw 'CAPTURE::FAIL capture incomplete' }
Write-Output ('CAPTURE::PASS ' + $dir)
