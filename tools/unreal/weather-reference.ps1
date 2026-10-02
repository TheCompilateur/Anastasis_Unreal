# Fixed weather reference: direct editor, numerical contract then 5 fixed poses x 5 states.
# Outputs Saved/SkyEvidence/<Label>. No asset writes; completion is not artistic approval.
param([string]$Label='weather-reference', [int]$TimeoutSec=900, [ValidateSet('reference','visibility')][string]$Mode='reference')
$ErrorActionPreference='Stop'
if (Test-Path 'C:/dev/ANASTASIS_WORKTREES/.handoff/MAIN.lock') {
  throw 'MAIN_LOCK::TENU - attendre la fin du lot avant observation'
}
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\SkyEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_SKY_OUT=$dir
$py=(Join-Path $Root ('tools\unreal\weather-' + $Mode + '.py')).Replace('\','/')
$exec = 'py ' + $py
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 # Hors premier plan, l'editeur coupe le rendu des viewports et HighResShot n'est jamais
 # servi (cf. capture-terrain-relief.ps1). Surcharge en memoire, rien d'ecrit dans Saved/Config.
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="'+$exec+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'CAPTURE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'SKY_CAPTURE|SKY_STATE|SKY_SHOT_OK|SKY_VIEWS|SKY_RELIEF|ANASTASIS_SKY |ANASTASIS_ATMOSPHERE applied|ANASTASIS_MIST ' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'SKY_CAPTURE_COMPLETE' -Quiet)){ throw 'CAPTURE::FAIL capture incomplete' }
if (!(Select-String -Path $log -Pattern 'WEATHER_CONTRACT PASS' -Quiet)) { throw 'WEATHER_CONTRACT::FAIL' }
if (Select-String -Path $log -Pattern 'WEATHER_CONTRACT FAIL|Failed to compile|LogPython: Error|Traceback' -Quiet) { throw 'WEATHER_REFERENCE::FAIL see log' }
Write-Output ('WEATHER_REFERENCE::CAPTURED ' + $dir)
