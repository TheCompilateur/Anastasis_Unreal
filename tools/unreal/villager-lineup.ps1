param([Parameter(Mandatory=$true)][string]$Label,[int]$TimeoutSec=600)
# VILLAGER_PNG_001 -- planche de population dans Unreal (vrais acteurs, vrai materiau). Meme forme
# que capture-tree-lineup.ps1 : l'editeur est lance, le script cadre et capture, puis se ferme.
# Niveau jamais sauve -- voir tools/unreal/villager-lineup.py. Sortie : Saved/VillagerEvidence/<Label>/
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root ('Saved\VillagerEvidence\' + $Label)
New-Item -ItemType Directory -Force $dir | Out-Null
Get-ChildItem $dir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item
$log=Join-Path $dir 'villager-lineup.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_VILLAGER_LINEUP_OUT=$dir
$py=(Join-Path $Root 'tools/unreal/villager-lineup.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
 # Sans le focus, l'editeur coupe le rendu de ses viewports et HighResShot n'est jamais servi.
 '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
 ('-abslog="'+$log+'"'),
 ('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'VILLAGER_LINEUP::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'VILLAGER_LINEUP' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
if(-not (Select-String -Path $log -Pattern 'VILLAGER_LINEUP::PASS' -Quiet)){ throw 'VILLAGER_LINEUP::FAIL voir ' + $log }
Write-Output ('VILLAGER_LINEUP::OUT ' + $dir)
