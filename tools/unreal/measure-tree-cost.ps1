param([int]$TimeoutSec=300)
# Mesure du cout de la vegetation sur l'incarnation reelle. LECTURE SEULE :
# aucun asset ni niveau n'est ecrit. Voir tools/unreal/measure-tree-cost.py.
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$dir=Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'tree-cost.log'
if(Test-Path $log){Remove-Item $log}
# Chemin en slashes : un '\t' dans un argument -script est lu comme une tabulation.
$py=(Join-Path $Root 'tools/unreal/measure-tree-cost.py').Replace('\','/')
$p=Start-Process $Editor -ArgumentList @(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '-run=pythonscript',('-script="'+$py+'"'),
 '-unattended','-nopause','-nosplash','-NoLiveCoding',('-abslog="'+$log+'"')) -PassThru -WindowStyle Hidden
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){Stop-Process -Id $p.Id -Force; throw 'COST::FAIL editeur bloque'}
Select-String -Path $log -Pattern 'COST ' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]','') -replace '^LogPython: ','' }
if(-not (Select-String -Path $log -Pattern 'COST COMPLETE' -Quiet)){throw 'COST::FAIL mesure incomplete'}
Write-Output 'COST::PASS'
