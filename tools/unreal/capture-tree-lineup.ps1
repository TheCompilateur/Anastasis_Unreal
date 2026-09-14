param([Parameter(Mandatory=$true)][string]$Out,[int]$TimeoutSec=300)
# Planche de stature de la grammaire d'arbres. Meme forme que capture-slice.ps1 :
# l'editeur est lance, le script Python cadre et capture, puis se ferme lui-meme.
# Le niveau n'est jamais sauve -- voir tools/unreal/capture-tree-lineup.py.
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$shot=Join-Path $dir $Out
if(Test-Path $shot){Remove-Item $shot}
$log=Join-Path $dir ($Out -replace '\.png$','.log')
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_LINEUP_SHOT=$shot
# Chemin en slashes : un '\t' dans un argument -script est interprete comme une
# tabulation par la ligne de commande de l'editeur et le fichier reste introuvable.
$py=(Join-Path $Root 'tools/unreal/capture-tree-lineup.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){
 Stop-Process -Id $p.Id -Force
 throw 'LINEUP::FAIL editeur bloque'
}
Select-String -Path $log -Pattern 'LINEUP' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
if(!(Test-Path $shot)){throw 'LINEUP::FAIL pas de capture'}
Write-Output ('LINEUP::PASS ' + $shot)
