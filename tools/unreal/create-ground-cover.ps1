# GROUND_COVER_001 -- (re)genere les trois touffes d'herbe et M_AnastasisGrass dans un editeur
# dedie, discret, qui se ferme. Voir create-ground-cover.py (source d'autorite des assets).
param([int]$TimeoutSec=1500)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\GroundCoverEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'create-ground-cover.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_GROUND_COVER_QUIT='1'
$py=(Join-Path $Root 'tools\unreal\create-ground-cover.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 # Carte vide du moteur, pas la carte de demarrage : Lvl_AnastasisSlice incarne le monde et
 # charge 268 000 touffes, qui verrouillent les assets a regenerer (ensure ObjectTools:4045)
 # et ont fait manquer de memoire l'editeur au troisieme run (v3, 2026-09-30).
 '/Engine/Maps/Entry',
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'GROUND_COVER_ASSETS::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'create-ground-cover|GROUND_COVER_ASSETS' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'GROUND_COVER_ASSETS::PASS' -Quiet)){ throw 'GROUND_COVER_ASSETS::FAIL voir ' + $log }
