# Sol pilote : memes cameras/geometries, SoilHistory 0/1/0.
# -Rebuild regenere le materiau puis capture dans le meme editeur.
# Sortie : Saved/SoilEvidence/<Label>/ (PNG, cameras.json, ground-cover.json, log).
param([string]$Label='latest', [string]$States='soil_before,soil_after,soil_control', [int]$TimeoutSec=1500, [switch]$Rebuild, [switch]$Deploy)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../unreal/editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\SoilEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_GROUND_OUT=$dir
$env:ANASTASIS_GROUND_STATES=$States
$py=(Join-Path $Root 'tools\soil-crusade\capture.py').Replace('\','/')
if($Rebuild){$py=(Join-Path $Root 'tools/soil-crusade/rebuild-capture.py').Replace('\','/')}
if($Rebuild -and $Deploy){throw 'Choose Rebuild OR Deploy'}
if($Deploy){$py=(Join-Path $Root 'tools/soil-crusade/deploy-capture.py').Replace('\','/')}
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 # Hors premier plan, l'editeur coupe le rendu des viewports et HighResShot n'est jamais servi.
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'CAPTURE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'ANASTASIS_GROUND_COVER |GROUND_CAPTURE|GROUND_SHOT' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'GROUND_CAPTURE_COMPLETE' -Quiet)){ throw 'CAPTURE::FAIL capture incomplete' }
if(Select-String -Path $log -Pattern 'SOIL_REBUILD_FAILED|SOIL_DEPLOY_FAILED|GROUND_MATERIAL FAILED' -Quiet){throw 'SOIL_MATERIAL::FAIL'}
$lines=Get-Content $log
$begin=$lines | Select-String -Pattern 'GROUND_MATERIAL RECOMPILE_BEGIN' | Select-Object -Last 1
if($begin){
 $after=$lines[($begin.LineNumber)..($lines.Count-1)]
 if($after | Select-String -Pattern 'Failed to compile Material.*AnastasisGround' -Quiet){throw 'SOIL_MATERIAL::FAIL shader compile'}
}
if($p.ExitCode -ne 0 -or (Select-String -Path $log -Pattern 'Fatal error!|Unhandled Exception:' -Quiet)){
 Write-Output ('CAPTURE::COMPLETE ' + $dir)
 throw ('EDITOR_EXIT::FAIL exit=' + $p.ExitCode + ' (images complete; not a clean run)')
}
Write-Output ('CAPTURE::PASS ' + $dir)
