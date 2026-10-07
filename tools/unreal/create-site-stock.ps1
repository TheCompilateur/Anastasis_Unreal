param([switch]$Rebuild, [int]$TimeoutSec = 900)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$OtherEditors = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe' OR Name = 'UnrealEditor-Cmd.exe'")
if ($OtherEditors.Count -gt 0) {
  throw ('SITE_STOCK::WAIT another editor owns the machine: ' + (($OtherEditors | Select-Object -ExpandProperty ProcessId) -join ','))
}
$dir = Join-Path $Root 'Saved\SiteStockEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'create-site-stock.log'
if (Test-Path $log) { Remove-Item -LiteralPath $log }
$env:ANASTASIS_SITE_STOCK_REBUILD = if ($Rebuild) { '1' } else { '0' }
$py = (Join-Path $Root 'tools\unreal\create-site-stock.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-unattended', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'SITE_STOCK::FAIL editor timeout' }
$failures = @(Select-String -LiteralPath $log -Pattern 'LogPython: Error|Traceback|SITE_STOCK.*FAILED')
if ($failures.Count) {
  $failures | ForEach-Object { Write-Output $_.Line }
  throw 'SITE_STOCK::FAIL python error'
}
if (-not (Select-String -LiteralPath $log -Pattern 'SITE_STOCK_ASSETS COMPLETE assets=2')) {
  throw 'SITE_STOCK::FAIL missing completion marker'
}
Select-String -LiteralPath $log -Pattern 'SITE_STOCK_ASSET |SITE_STOCK_ASSETS COMPLETE' |
  ForEach-Object { Write-Output $_.Line }
Write-Output ('SITE_STOCK::PASS log=' + $log)
