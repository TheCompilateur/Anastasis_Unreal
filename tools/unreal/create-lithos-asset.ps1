param([int]$TimeoutSec=900)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$Project=Join-Path $Root 'Anastasis_UnrealV2.uproject'
$py=(Join-Path $Root 'tools/unreal/create_lithos_asset.py').Replace('\','/')
$log=Join-Path $Root 'Saved\LithosEvidence\create_lithos.log'
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
if(Test-Path $log){Remove-Item $log}
$launchArgs=@(
  ('"'+$Project+'"'),
  '-windowed','-resx=1280','-resy=720','-nosplash','-NoLiveCoding',
  ('-abslog="'+$log+'"'),
  ('-ExecCmds="py '+$py+'"')
)
$p=Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if(-not $p.HasExited){
  Stop-Process -Id $p.Id -Force
  throw 'LITHOS_MESH::FAIL editeur bloque'
}
if($p.ExitCode -ne 0){ throw "LITHOS_MESH::FAIL exit=$($p.ExitCode)" }
$hit=Select-String -Path $log -Pattern 'RESULT::PASS' -ErrorAction SilentlyContinue
if(-not $hit){ throw 'LITHOS_MESH::FAIL pas de RESULT::PASS' }
Write-Output 'LITHOS_MESH::PASS'
