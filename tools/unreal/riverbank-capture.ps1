# RIVERBANK_LIFE_001 -- l'eau et ses rives a hauteur d'homme, par etats, aux memes cameras.
# -States "water,flat" (defaut) : eau WATER_LOOK puis eau d'avant, l'ecart GPU = cout de Single
# Layer Water. "nobanks,banks" : rives vivantes coupees puis actives. Voir riverbank-capture.py.
# Sortie : Saved\RiverbankEvidence\<Label>\<vue>_<etat>.png + cameras.json + metrics.json + network_<etat>.json
# -Profile : un ProfileGPU par vue ; les lignes de passe sont recopiees dans profile.txt.
param([string]$Label='latest', [string]$States='water,flat', [int]$TimeoutSec=1500, [switch]$Profile)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\RiverbankEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_RIVERBANK_OUT=$dir
$env:ANASTASIS_RIVERBANK_STATES=$States
$env:ANASTASIS_RIVERBANK_PROFILE= if ($Profile) { '1' } else { '0' }
$py=(Join-Path $Root 'tools\unreal\riverbank-capture.py').Replace('\','/')
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
Select-String -Path $log -Pattern 'ANASTASIS_WATER_LOOK|ANASTASIS_RIVERBANK|RIVERBANK_CAPTURE|RIVERBANK_SHOT' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'RIVERBANK_CAPTURE_COMPLETE' -Quiet)){ throw 'CAPTURE::FAIL capture incomplete' }
if($Profile){
 # Chaque ProfileGPU s'imprime quelques frames apres sa demande : on garde, par vue, ce qui
 # suit son marqueur jusqu'a la prise de vue.
 $out=@(); $cur=$null
 foreach($l in Get-Content $log){
  if($l -match 'RIVERBANK_PROFILE_BEGIN (\S+)'){ $cur=$Matches[1]; $out+="== $cur"; continue }
  if($l -match 'RIVERBANK_SHOT_OK'){ $cur=$null; continue }
  if($cur -and $l -match 'LogRHI'){ $out+=($l -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
 }
 $out | Set-Content -Encoding utf8 (Join-Path $dir 'profile.txt')
}
Write-Output ('CAPTURE::PASS ' + $dir)
