# Forge the project-owned lunar texture and sky material in an isolated worktree.
param([switch]$Rebuild, [ValidateSet(0,1)][int]$Version = 0, [int]$TimeoutSec = 900)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
if ($root -eq 'C:\dev\ANASTASIS_UNREAL') { throw 'COSMIC_MATERIAL::FAIL canonical root is integration only' }
$dir = Join-Path $root 'Saved\CosmicSkyEvidence'
New-Item -ItemType Directory -Path $dir -Force | Out-Null
$log = Join-Path $dir 'material.log'
$env:ANASTASIS_COSMIC_REBUILD = if ($Rebuild) { '1' } else { '0' }
$env:ANASTASIS_COSMIC_VERSION = [string]$Version
$script = (Join-Path $PSScriptRoot 'cosmic-sky-material.py').Replace('\', '/')
$argsList = @(
  ('"' + (Join-Path $root 'Anastasis_UnrealV2.uproject') + '"'),
  '-run=pythonscript', ('-script="' + $script + '"'),
  '-unattended', '-nopause', '-nosound', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"')
)
$p = Start-AnastasisEditor 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $argsList
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'COSMIC_MATERIAL::FAIL editor timeout' }
if ($p.ExitCode -ne 0) { throw ('COSMIC_MATERIAL::FAIL editor exit code ' + $p.ExitCode + '; inspect ' + $log) }
if (-not (Select-String -LiteralPath $log -Pattern 'COSMIC_MATERIAL_DONE' -Quiet)) { throw 'COSMIC_MATERIAL::FAIL incomplete run' }
if (Select-String -LiteralPath $log -Pattern 'COSMIC_MATERIAL_FAIL|Failed to compile|\[SM[56]\].*error' -Quiet) {
  throw ('COSMIC_MATERIAL::FAIL see ' + $log)
}
$skyTexture = if ($Version -eq 1) { 'T_CosmicRiverV1.uasset' } else { 'T_CosmicRiver.uasset' }
$skyMaterial = if ($Version -eq 1) { 'M_AnastasisCosmicSkyV1.uasset' } else { 'M_AnastasisCosmicSky.uasset' }
foreach ($asset in @('Content\Anastasis\Celestial\T_MoonLavender.uasset',
                     ('Content\Anastasis\Celestial\' + $skyTexture),
                     ('Content\Anastasis\Celestial\' + $skyMaterial))) {
  if (-not (Test-Path (Join-Path $root $asset))) { throw ('COSMIC_MATERIAL::FAIL missing ' + $asset) }
}
Write-Output 'COSMIC_MATERIAL::PASS'
