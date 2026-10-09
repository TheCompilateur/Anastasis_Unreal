# Validate one saved asset contract in a fresh, non-rendering editor process.
param([string]$Contract='docs/unreal/assets/contracts/pontic-horsetail-v2.json', [int]$TimeoutSec=480)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$contractPath=[IO.Path]::GetFullPath((Join-Path $Root $Contract))
if(-not $contractPath.StartsWith($Root + '\',[StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $contractPath)){
  throw 'ASSET_CONTRACT::FAIL contract outside worktree or missing'
}
$dir=Join-Path $Root 'Saved\AssetContractEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log=Join-Path $dir 'validate-asset-contract.log'
if(Test-Path $log){Remove-Item -LiteralPath $log}
$env:ANASTASIS_ASSET_CONTRACT=$contractPath
$py=(Join-Path $Root 'tools\unreal\asset-contract-validate.py').Replace('\','/')
$launchArgs=@(
 ('"'+(Join-Path $Root 'Anastasis_UnrealV2.uproject')+'"'),
 '/Engine/Maps/Entry','-nullrhi','-nosound','-unattended','-nopause','-nosplash','-NoLiveCoding',
 ('-abslog="'+$log+'"'),('-ExecCmds="py '+$py+'"')
)
$p=Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){Stop-Process -Id $p.Id -Force; throw 'ASSET_CONTRACT::FAIL editor timeout'}
Select-String -Path $log -Pattern 'ASSET_CONTRACT (ASSET|PASS|FAIL)' |
  ForEach-Object { $_.Line } | Select-Object -Unique
if(-not (Select-String -Path $log -Pattern 'ASSET_CONTRACT PASS' -Quiet)){
  throw 'ASSET_CONTRACT::FAIL see ' + $log
}
