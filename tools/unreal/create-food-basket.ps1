param([switch]$Rebuild, [int]$TimeoutSec = 900)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir = Join-Path $Root 'Saved\FoodBasketEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'create-food-basket.log'
if (Test-Path $log) { Remove-Item -LiteralPath $log }
$env:ANASTASIS_FOOD_BASKET_REBUILD = if ($Rebuild) { '1' } else { '0' }
$py = (Join-Path $Root 'tools\unreal\create-food-basket.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-nullrhi',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'FOOD_BASKET::FAIL editor timeout' }
$failures = @(Select-String -LiteralPath $log -Pattern 'LogPython: Error|Traceback|FOOD_BASKET.*FAIL')
if ($failures.Count) {
  $failures | ForEach-Object { Write-Output $_.Line }
  throw 'FOOD_BASKET::FAIL python error'
}
if (-not (Select-String -LiteralPath $log -Pattern 'FOOD_BASKET_ASSETS COMPLETE assets=1')) {
  throw 'FOOD_BASKET::FAIL missing completion marker'
}
Select-String -LiteralPath $log -Pattern 'FOOD_BASKET_ASSET |FOOD_BASKET_ASSETS COMPLETE' |
  ForEach-Object { Write-Output $_.Line }
Write-Output ('FOOD_BASKET::PASS log=' + $log)
