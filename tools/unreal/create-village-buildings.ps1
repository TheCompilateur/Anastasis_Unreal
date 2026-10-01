param([switch]$Rebuild, [int]$TimeoutSec = 900)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'village-buildings.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_BUILDINGS_REBUILD = if ($Rebuild) { '1' } else { '0' }
$env:ANASTASIS_BUILDINGS_GEOMETRY_ONLY = '0'
$py = (Join-Path $Root 'tools\unreal\create-village-buildings.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-unattended', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'BUILDINGS::FAIL editeur bloque' }

Select-String -Path $log -Pattern 'BUILDINGS' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }

$failed = Select-String -Path $log -Pattern 'LogPython: Error|BUILDINGS.*FAILED|Traceback'
if ($failed) {
  $failed | ForEach-Object { Write-Output $_.Line }
  throw 'BUILDINGS::FAIL script python en erreur'
}
if (-not (Select-String -Path $log -Pattern 'BUILDINGS COMPLETE')) {
  throw 'BUILDINGS::FAIL pas de marqueur COMPLETE'
}
Write-Output ('BUILDINGS::PASS log=' + $log)
