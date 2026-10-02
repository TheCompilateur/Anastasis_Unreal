param([switch]$Rebuild, [int]$TimeoutSec = 900)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'building-aging.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_AGING_REBUILD = if ($Rebuild) { '1' } else { '0' }
$py = (Join-Path $Root 'tools\unreal\create-building-aging.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-unattended', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'AGING::FAIL editeur bloque' }

Select-String -Path $log -Pattern 'AGING' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }

$failed = Select-String -Path $log -Pattern 'LogPython: Error|AGING.*FAILED|Traceback'
if ($failed) {
  $failed | ForEach-Object { Write-Output $_.Line }
  throw 'AGING::FAIL script python en erreur'
}
if (Select-String -Path $log -Pattern 'Failed to compile|Material.*(error|Error)') {
  throw 'AGING::FAIL le materiau ne compile pas (un Failed to compile n est qu un Warning)'
}
if (-not (Select-String -Path $log -Pattern 'AGING COMPLETE')) {
  throw 'AGING::FAIL pas de marqueur COMPLETE'
}
Write-Output ('AGING::PASS log=' + $log)
