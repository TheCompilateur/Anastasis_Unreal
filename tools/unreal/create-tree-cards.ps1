# LEAFCARDS_001 -- (re)genere l'atlas importe (T_Leaf_Atlas), M_AnastasisFoliageCard et
# SM_Tree_HolmOak_Card_01 dans un editeur dedie, discret, qui se ferme. Voir create-tree-cards.py
# (source d'autorite des assets). Atlas : python tools/unreal/leaf-atlas.py (hors Unreal), a lancer avant.
param([int]$TimeoutSec=1500)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\TreeCardsEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'create-tree-cards.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_TREE_CARDS_QUIT='1'
$py=(Join-Path $Root 'tools\unreal\create-tree-cards.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 # Carte vide du moteur : Lvl_AnastasisSlice incarne le monde et verrouille les assets a regenerer.
 '/Engine/Maps/Entry',
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'TREE_CARDS_ASSETS::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'create-tree-cards|TREE_CARDS_ASSETS|FOLIAGE_CARD_COMPILE' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'TREE_CARDS_ASSETS::PASS' -Quiet)){ throw ('TREE_CARDS_ASSETS::FAIL voir ' + $log) }
