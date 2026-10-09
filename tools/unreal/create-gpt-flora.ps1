# GPT_FLORA_001 -- (re)genere l'atlas importe (T_GptFlora_Atlas), M_AnastasisGptFoliage et les treize
# SM_Gpt_<Espece> dans un editeur dedie, discret, qui se ferme. Voir create-gpt-flora.py (source d'autorite
# des assets). Spec et atlas : python tools/unreal/gpt-flora-fit.py (hors Unreal), a lancer avant.
param([int]$TimeoutSec=1800)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\GptFloraEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'create-gpt-flora.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_GPT_FLORA_QUIT='1'
$env:ANASTASIS_GPT_FLORA_GRAMMAR='1'   # cable aussi les arbres dans DA_AnastasisPresentation (set_tree_grammar.py)
$py=(Join-Path $Root 'tools\unreal\create-gpt-flora.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 # Carte vide du moteur : Lvl_AnastasisSlice incarne le monde et verrouille les assets a regenerer.
 '/Engine/Maps/Entry',
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
# L'editeur peut rester suspendu a sa fermeture (Python) APRES avoir tout ecrit : le verdict est au log. Une fois
# les deux marqueurs lus, on lui laisse 60 s pour sortir seul, puis on l'arrete, au lieu d'attendre le delai plein.
$start=Get-Date; $doneAt=$null
while(-not $p.HasExited -and ((Get-Date)-$start).TotalSeconds -lt $TimeoutSec){
  Start-Sleep -Seconds 5
  if(-not $doneAt -and (Test-Path $log) -and
     (Select-String -Path $log -Pattern 'GPT_FLORA_ASSETS::(PASS|FAIL)' -Quiet) -and
     (Select-String -Path $log -Pattern 'set_tree_grammar\] RESULT::' -Quiet)){ $doneAt=Get-Date }
  if($doneAt -and ((Get-Date)-$doneAt).TotalSeconds -gt 60){ break }
}
$p.Refresh()
if(-not $p.HasExited){
  if($doneAt){ Stop-Process -Id $p.Id -Force }
  else { Stop-Process -Id $p.Id -Force; throw 'GPT_FLORA_ASSETS::FAIL editeur bloque' }
}
Select-String -Path $log -Pattern 'create-gpt-flora|GPT_FLORA_ASSETS|FOLIAGE_CARD_COMPILE|create-tree-cards|set_tree_grammar' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'GPT_FLORA_ASSETS::PASS' -Quiet)){ throw ('GPT_FLORA_ASSETS::FAIL voir ' + $log) }
if(-not (Select-String -Path $log -Pattern 'set_tree_grammar\] RESULT::PASS' -Quiet)){ throw ('GPT_FLORA_GRAMMAR::FAIL voir ' + $log) }
