# VILLAGER_PNG_001 -- importe les portraits traites (SourceArt/Characters/PNG), ecrit M_AnastasisVillager
# et la population du registre, puis relit et verifie. Editeur dedie, discret, qui se ferme.
# Voir import-villagers.py (source d'autorite de ces assets).
param([int]$TimeoutSec=1500)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\VillagerEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'import-villagers.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_VILLAGERS_QUIT='1'
$py=(Join-Path $Root 'tools\unreal\import-villagers.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 # Carte vide : la carte de demarrage incarne le monde et tient le registre ouvert.
 '/Engine/Maps/Entry',
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'VILLAGERS_IMPORT::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'VILLAGERS_IMPORT' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'VILLAGERS_IMPORT::PASS' -Quiet)){ throw 'VILLAGERS_IMPORT::FAIL voir ' + $log }
# Le graphe se compile a chaque branchement : seuls comptent les echecs APRES la compilation finale.
$lines=Get-Content $log
$mark=($lines | Select-String -Pattern 'VILLAGERS_IMPORT MATERIAL_COMPILE_FINAL' | Select-Object -Last 1).LineNumber
if(-not $mark){ throw 'VILLAGERS_IMPORT::FAIL materiau jamais compile' }
$late=$lines[$mark..($lines.Count-1)] | Select-String -Pattern 'M_AnastasisVillager.*Failed to compile'
if($late){ throw 'VILLAGERS_IMPORT::FAIL M_AnastasisVillager ne compile pas : voir ' + $log }
$early=@($lines[0..($mark-1)] | Select-String -Pattern 'M_AnastasisVillager.*Failed to compile').Count
Write-Output ('VILLAGERS_IMPORT::MATERIAL_OK echecs_intermediaires=' + $early)
