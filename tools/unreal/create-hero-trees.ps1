# Quatre arbres heros a taille reelle + enveloppe de canopee. Voir create-hero-trees.py.
# N'ecrit pas SM_Tree_* ni les materiaux de vegetation.
param([int]$TimeoutSec=1800)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\HeroTreeEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'create-hero-trees.log'
if(Test-Path $log){Remove-Item $log}
$env:ANASTASIS_HERO_TREES_QUIT='1'
$py=(Join-Path $Root 'tools\unreal\create-hero-trees.py').Replace('\','/')
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
if(-not $p.HasExited){ Stop-Process -Id $p.Id -Force; throw 'HERO_TREES::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'HERO_TREES' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') }
if(-not (Select-String -Path $log -Pattern 'HERO_TREES::PASS' -Quiet)){ throw 'HERO_TREES::FAIL voir ' + $log }
