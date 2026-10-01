# VILLAGER_BODY_3D_001 -- mesure les os du mannequin et ecrit M_AnastasisVillagerBody (tenue des corps 3D).
# Editeur dedie, discret, qui se ferme. Voir create-villager-body.py (source d'autorite de cet asset).
param([int]$TimeoutSec=1500)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\VillagerEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'create-villager-body.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_VILLAGER_BODY_QUIT='1'
$py=(Join-Path $Root 'tools\unreal\create-villager-body.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '/Engine/Maps/Entry',
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'VILLAGER_BODY::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'VILLAGER_BODY' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'VILLAGER_BODY::PASS' -Quiet)){ throw 'VILLAGER_BODY::FAIL voir ' + $log }
# Le graphe se compile a chaque branchement : seuls comptent les echecs APRES la compilation finale.
$lines=Get-Content $log
$mark=($lines | Select-String -Pattern 'VILLAGER_BODY MATERIAL_COMPILE_FINAL' | Select-Object -Last 1).LineNumber
if(-not $mark){ throw 'VILLAGER_BODY::FAIL materiau jamais compile' }
$late=$lines[$mark..($lines.Count-1)] | Select-String -Pattern 'M_AnastasisVillagerBody.*Failed to compile'
if($late){ throw 'VILLAGER_BODY::FAIL M_AnastasisVillagerBody ne compile pas : voir ' + $log }
Write-Output 'VILLAGER_BODY::MATERIAL_OK'
