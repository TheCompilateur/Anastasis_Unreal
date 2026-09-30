# WORLD_DRESSING_01 -- captures des lieux composes (AnastasisPlaces), lieux actives puis coupes.
# Voir places-capture.py. Sortie : Saved\PlacesEvidence\<Label>\<lieu>_<vue>_<on|off>.png
param([string]$Label='latest', [switch]$OnOnly, [switch]$All, [int]$TimeoutSec=1500)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\PlacesEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_PLACES_OUT=$dir
$env:ANASTASIS_PLACES_AB = if($OnOnly){'0'}else{'1'}
$env:ANASTASIS_PLACES_ALL = if($All){'1'}else{'0'}
$py=(Join-Path $Root 'tools\unreal\places-capture.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 # Hors premier plan, l'editeur coupe le rendu des viewports et HighResShot n'est jamais
 # servi (cf. capture-terrain-relief.ps1). Surcharge en memoire, rien d'ecrit dans Saved/Config.
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'CAPTURE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'ANASTASIS_PLACES |ANASTASIS_PLACE |PLACES_CAPTURE|PLACES_SHOT_MISSING' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'PLACES_CAPTURE_COMPLETE' -Quiet)){ throw 'CAPTURE::FAIL capture incomplete' }
Write-Output ('CAPTURE::PASS ' + $dir)
