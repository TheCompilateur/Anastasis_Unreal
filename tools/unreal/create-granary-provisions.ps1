param([int]$TimeoutSec = 900)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir = Join-Path $Root 'Saved\GranaryProvisionsEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'create-granary-provisions.log'
if (Test-Path $log) { Remove-Item -LiteralPath $log }
$py = (Join-Path $Root 'tools\unreal\create-granary-provisions.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-unattended', '-nosplash', '-nosound', '-NoLiveCoding', '-nullrhi',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'GRANARY_PROVISIONS::FAIL editor timeout' }
$failures = @(Select-String -LiteralPath $log -Pattern 'LogPython: Error|Traceback|GRANARY_PROVISIONS.*FAIL')
if ($failures.Count) {
  $failures | ForEach-Object { Write-Output $_.Line }
  throw 'GRANARY_PROVISIONS::FAIL python error'
}
if (-not (Select-String -LiteralPath $log -Pattern 'GRANARY_PROVISIONS_ASSETS COMPLETE assets=1')) {
  throw 'GRANARY_PROVISIONS::FAIL missing completion marker'
}
Select-String -LiteralPath $log -Pattern 'GRANARY_PROVISIONS_ASSET |GRANARY_PROVISIONS_ASSETS COMPLETE' |
  ForEach-Object { Write-Output $_.Line }
Write-Output ('GRANARY_PROVISIONS::PASS log=' + $log)
