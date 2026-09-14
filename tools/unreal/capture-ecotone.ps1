param(
  [int]$TimeoutSec = 480,
  [switch]$EyesOnly,
  [switch]$Boards
)
# Preuve visuelle ECOTONE_FORGE_001. L'editeur cadre, capture, se ferme.
# Le niveau n'est jamais sauve.
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir = Join-Path $Root 'Saved\EcotoneEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$env:ANASTASIS_ECOTONE_OUT = $dir
if ($EyesOnly) { $env:ANASTASIS_ECOTONE_EYES_ONLY = '1' } else { $env:ANASTASIS_ECOTONE_EYES_ONLY = '0' }
$log = Join-Path $dir 'capture.log'
if (Test-Path $log) { Remove-Item $log }
$scriptName = if ($Boards) { 'capture-ecotone-boards.py' } else { 'capture-ecotone.py' }
$py = (Join-Path $Root ('tools/unreal/' + $scriptName)).Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-windowed', '-resx=1280', '-resy=720', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force
  throw 'ECOTONE_CAPTURE::FAIL editeur bloque'
}
Select-String -Path $log -Pattern 'ECOTONE_CAPTURE' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
$needed = if ($Boards) {
  @('context_forest.png', 'context_shore.png', 'context_rock.png')
} elseif ($EyesOnly) {
  @('B_forest_eye.png', 'B_shore_eye.png', 'B_rock_eye.png')
} else {
  @('lineup.png', 'A_macro.png', 'B_macro.png', 'A_forest_eye.png', 'B_forest_eye.png', 'A_shore_eye.png', 'B_shore_eye.png', 'A_rock_eye.png', 'B_rock_eye.png')
}
$missing = @($needed | Where-Object { -not (Test-Path (Join-Path $dir $_)) })
if ($missing.Count -gt 0) {
  throw ('ECOTONE_CAPTURE::FAIL manquants: ' + ($missing -join ', '))
}
Write-Output ('ECOTONE_CAPTURE::PASS ' + $dir)
