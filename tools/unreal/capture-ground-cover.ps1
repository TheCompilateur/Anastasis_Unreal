# GROUND_COVER_001 -- A/B de la strate herbacee aux memes cameras, dans la meme session.
# -States "on,off" (defaut) ; "on,noshadow,off" mesure aussi le cout des ombres. Voir ground-cover-capture.py.
# GROUND_TEXTURE_001 : "on,on_notex,bare,bare_notex" = A/B des textures photo du sol, herbe posee puis retiree.
# Sortie : Saved\GroundCoverEvidence\<Label>\<vue>_<on|off>.png + ground-cover.json
param([string]$Label='latest', [string]$States='on,off', [int]$TimeoutSec=1500)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\GroundCoverEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_GROUND_OUT=$dir
$env:ANASTASIS_GROUND_STATES=$States
$py=(Join-Path $Root 'tools\unreal\ground-cover-capture.py').Replace('\','/')
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
Write-Output ('CAPTURE::PASS ' + $dir)
