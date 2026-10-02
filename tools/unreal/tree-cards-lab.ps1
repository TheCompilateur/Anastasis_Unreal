# LEAFCARDS_001 -- A/B du chene vert : lames (SM_Tree_HolmOak_01) contre cartes (SM_Tree_HolmOak_Card_01).
# Voir tree-cards-lab.py. Sortie : Saved\TreeCardsEvidence\<Label>\<vue>.png + capture.log.
# Le niveau n'est jamais sauve ; l'editeur est dedie, discret, et se ferme.
param([string]$Label='lab', [string]$MeshB='', [int]$TimeoutSec=420)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root "Saved\TreeCardsEvidence\$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'capture.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_TREE_CARDS_OUT=$dir
$env:ANASTASIS_TREE_CARDS_MESH_B=$MeshB
$py=(Join-Path $Root 'tools/unreal/tree-cards-lab.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 # Sans le focus, le viewport ne rend plus et HighResShot n'est jamais servi.
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'TREE_CARDS::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'TREE_CARDS' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
if(-not (Select-String -Path $log -Pattern 'TREE_CARDS_COMPLETE' -Quiet)){ throw ('TREE_CARDS::FAIL voir ' + $log) }
Write-Output ('TREE_CARDS::PASS ' + $dir)
