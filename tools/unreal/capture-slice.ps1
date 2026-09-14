# CamLoc / CamRot : preuve rapprochee (mission tree-visuals). Vide = cadrage historique.
# PreCmds : CVars a poser avant le script, comme probe-demo.ps1. C'est ce qui permet un
# A/B ou SEULE la CVar change -- meme carte, meme graine, meme soleil, meme exposition,
# meme camera. Sans cela, comparer deux materiaux de sol demanderait deux builds, et la
# comparaison porterait alors sur deux binaires differents au lieu d'un seul reglage.
# Les deux sont orthogonaux : l'un deplace la camera, l'autre change une CVar.
param([ValidateSet('0','1','2')][string]$Mode='1',[Parameter(Mandatory=$true)][string]$Out,[string]$RebuildMaterial='0',[ValidateSet('auto','world','slice')][string]$Cam='auto',[string]$CamLoc='',[string]$CamRot='',[string]$PreCmds='',[int]$TimeoutSec=300)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$shot=Join-Path $dir $Out
if(Test-Path $shot){Remove-Item $shot}
$log=Join-Path $dir ($Out -replace '\.png$','.log')
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_SLICE_SHOT=$shot
$env:ANASTASIS_SLICE_MODE=$Mode
$env:ANASTASIS_SLICE_REBUILD_MATERIAL=$RebuildMaterial
$env:ANASTASIS_SLICE_CAM=$Cam
# Vide = comportement historique inchange. Renseigne = preuve rapprochee.
$env:ANASTASIS_SLICE_CAM_LOC=$CamLoc
$env:ANASTASIS_SLICE_CAM_ROT=$CamRot
$py=(Join-Path $Root 'tools\unreal\observe-slice.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="'+$(if($PreCmds){$PreCmds+','}else{''})+'py '+$py+'"')
)
$p=Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){
 Stop-Process -Id $p.Id -Force
 throw 'CAPTURE::FAIL editeur bloque'
}
Select-String -Path $log -Pattern 'SLICE_|ANASTASIS_TERRAIN ' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
if(!(Test-Path $shot)){throw 'CAPTURE::FAIL pas de capture'}
Write-Output ('CAPTURE::PASS ' + $shot)
