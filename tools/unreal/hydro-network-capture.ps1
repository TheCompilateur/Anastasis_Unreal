# HYDRO_NETWORK_001 -- captures et export du reseau de drainage, avant / apres.
# Voir hydro-network-capture.py. Sortie : Saved\HydroNetworkEvidence\<Label>\
#   <vue>_<etat>.png, drainage_<etat>_grid.json, capture.log
# -States "0,1" : valeurs successives de anastasis.Terrain.Drainage.
# -Shots <json> : [[nom,[x,y,z],[cible x,y,z]],...] ; defaut : zenithale, oblique, gros plans.
# -Debug 1..4 : lignes anastasis.Drainage.Debug (largeur, profondeur, vitesse, ordre).
# -PreCmds "a 1;b 2" : commandes console avant chaque incarnation.
param([string]$Label='default', [string]$States='0,1', [string]$Shots='', [int]$Debug=0, [string]$PreCmds='', [int]$TimeoutSec=600)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\HydroNetworkEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_HYDRO_OUT=$dir
$env:ANASTASIS_HYDRO_STATES=$States
$env:ANASTASIS_HYDRO_DEBUG="$Debug"
$env:ANASTASIS_HYDRO_PRECMDS=$PreCmds
# Resolve-Path suit l'emplacement PowerShell ; [IO.Path]::GetFullPath lirait le repertoire du processus.
$env:ANASTASIS_HYDRO_SHOTS= if ($Shots) { (Resolve-Path $Shots).Path } else { '' }
$py=(Join-Path $Root 'tools\unreal\hydro-network-capture.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 # Hors premier plan, l'editeur coupe le rendu des viewports (cf. capture-terrain-relief.ps1).
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){
 Stop-Process -Id $p.Id -Force
 throw 'CAPTURE::FAIL editeur bloque'
}
Select-String -Path $log -Pattern 'ANASTASIS_DRAINAGE|HYDRO_CAPTURE' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
if(-not (Select-String -Path $log -Pattern 'HYDRO_CAPTURE_COMPLETE' -Quiet)){throw 'CAPTURE::FAIL run incomplet'}
Write-Output ('CAPTURE::PASS ' + $dir)
