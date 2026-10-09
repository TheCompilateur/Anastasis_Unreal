# A/B/A in one editor, at fixed seed, cameras and light. No asset is saved.
param([string]$Label='latest', [int]$TimeoutSec=1200)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\unreal\editor-launch.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..')).TrimEnd('\')
$editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$out=Join-Path $root "Saved\PonticSlopeEcologyEvidence\$Label"
New-Item -ItemType Directory -Force $out | Out-Null
$log=Join-Path $out 'capture.log'
if(Test-Path $log){Remove-Item -LiteralPath $log}
$env:ANASTASIS_PONTIC_SLOPE_OUT=$out
$py=(Join-Path $PSScriptRoot 'capture.py').Replace('\','/')
$launchArgs=@(('"'+(Join-Path $root 'Anastasis_UnrealV2.uproject')+'"'),
  '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
  '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
  ('-abslog="'+$log+'"'),('-ExecCmds="py '+$py+'"'))
$process=Start-AnastasisEditor $editor $launchArgs
$process | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$process.Refresh()
if(-not $process.HasExited){Stop-Process -Id $process.Id -Force; throw 'PONTIC_SLOPE_CAPTURE::TIMEOUT'}
Select-String -Path $log -Pattern 'PONTIC_SLOPE_' | ForEach-Object {$_.Line}
if(-not (Select-String -Path $log -Pattern 'PONTIC_SLOPE_CAPTURE PASS' -Quiet)){
  throw 'PONTIC_SLOPE_CAPTURE::FAIL see capture.log'
}
Write-Output ('PONTIC_SLOPE_CAPTURE::PASS '+$out)
