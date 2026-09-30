# Ciel : A/B de l'atmosphere sur Lvl_AnastasisSlice -- couche de realisme
# (anastasis.Atmosphere.Realism), horloge du ciel (anastasis.Sky.Hour / Sky.Day) et meteo
# (anastasis.Sky.Weather). Voir capture-sky.py pour la syntaxe des etats.
# Sortie : Saved\SkyEvidence\<Label>\<vue>_<etat>.png + sky.json
#   -States 'h06=anastasis.Sky.Hour 6|h12=anastasis.Sky.Hour 12|h22=anastasis.Sky.Hour 22'
#   -PreCmds : CVars posees avant le script, communes a tous les etats.
param([string]$Label='latest', [string]$States='1|0', [string]$PreCmds='', [int]$TimeoutSec=1500)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\SkyEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_SKY_OUT=$dir
$env:ANASTASIS_SKY_STATES=$States
$py=(Join-Path $Root 'tools\unreal\capture-sky.py').Replace('\','/')
$exec = if ($PreCmds) { $PreCmds + ',py ' + $py } else { 'py ' + $py }
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
Select-String -Path $log -Pattern 'SKY_CAPTURE|SKY_STATE|SKY_VIEWS|SKY_RELIEF|ANASTASIS_SKY |ANASTASIS_ATMOSPHERE applied|ANASTASIS_MIST ' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'SKY_CAPTURE_COMPLETE' -Quiet)){ throw 'CAPTURE::FAIL capture incomplete' }
Write-Output ('CAPTURE::PASS ' + $dir)
