# Generate only SM_Pontic_Horsetail_02 on an empty map in this worktree.
param([int]$TimeoutSec=1200)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir=Join-Path $Root 'Saved\PonticHorsetailV2Evidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'create-pontic-horsetail-v2.log'
if(Test-Path $log){Remove-Item -LiteralPath $log}
$py=(Join-Path $Root 'tools\unreal\create-pontic-horsetail-v2.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '/Engine/Maps/Entry','-nullrhi','-nosound','-unattended','-nopause','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){Stop-Process -Id $p.Id -Force; throw 'PONTIC_HORSETAIL_V2::FAIL editor timeout'}
Select-String -Path $log -Pattern 'PONTIC_HORSETAIL_V2|create-ground-cover.*SAVED' |
  ForEach-Object { $_.Line } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'PONTIC_HORSETAIL_V2 PASS' -Quiet)){
  throw 'PONTIC_HORSETAIL_V2::FAIL see ' + $log
}
