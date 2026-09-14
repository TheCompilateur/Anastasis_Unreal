param([int]$TimeoutSec = 300)
# Construit /Game/Anastasis/Ecotone/* et M_AnastasisStone. Idempotent.
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $Root 'Saved\EcotoneEvidence\create-assets.log'
New-Item -ItemType Directory -Force (Split-Path $log) | Out-Null
if (Test-Path $log) { Remove-Item $log }
$py = (Join-Path $Root 'tools/unreal/create_ecotone_assets.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-unattended', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-run=pythonscript'),
  ('-script="' + $py + '"')
)
$p = Start-Process $Editor -ArgumentList $launchArgs -PassThru -NoNewWindow
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force
  throw 'ECOTONE_ASSETS::FAIL editeur bloque'
}
Select-String -Path $log -Pattern 'create_ecotone_assets|RESULT::' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
if ($p.ExitCode -ne 0) { throw "ECOTONE_ASSETS::FAIL exit=$($p.ExitCode)" }
if (-not (Select-String -Path $log -Pattern 'RESULT::PASS' -Quiet)) {
  throw 'ECOTONE_ASSETS::FAIL pas de RESULT::PASS'
}
Write-Output 'ECOTONE_ASSETS::PASS'
