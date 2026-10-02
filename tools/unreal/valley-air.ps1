param([string]$Label='valley-air-diagnostic',[int]$TimeoutSec=900,[ValidateSet('diagnostic','candidate')][string]$Mode='diagnostic')
$ErrorActionPreference='Stop'
if(Test-Path 'C:/dev/ANASTASIS_WORKTREES/.handoff/MAIN.lock'){throw 'MAIN_LOCK::TENU'}
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$dir=Join-Path $Root "Saved/SkyEvidence/$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
$env:ANASTASIS_SKY_OUT=$dir
$env:ANASTASIS_VALLEY_MODE=$Mode
$env:ANASTASIS_EDITOR_MAX='1'
$py=(Join-Path $PSScriptRoot 'valley-air.py').Replace('\','/')
$launchArgs=@(('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),'-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding','-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',('-abslog="'+$log+'"'),('-ExecCmds="py '+$py+'"'))
$p=Start-AnastasisEditor 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe' $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){Stop-Process -Id $p.Id -Force; throw 'VALLEY_AIR::TIMEOUT'}
if(!(Select-String -Path $log -Pattern 'SKY_CAPTURE_COMPLETE' -Quiet)){throw 'VALLEY_AIR::INCOMPLETE'}
if(Select-String -Path $log -Pattern 'VALLEY_AIR_FAIL|LogPython: Error|Traceback|SKY_SHOT_MISSING|SKY_CAPTURE_TIMEOUT' -Quiet){throw 'VALLEY_AIR::FAIL'}
Write-Output "VALLEY_AIR::CAPTURED exit=$($p.ExitCode) path=$dir (completion only)"
