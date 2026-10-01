param([int]$TimeoutSec = 900, [string]$Type = 'house', [int]$Builders = 2)
# Preuve PIE du chantier (build-001) : pilote build-site-pie.py.
# Editeur discret (Start-AnastasisEditor), rendu garde hors focus pour les captures.
# Sortie : Saved/SliceEvidence/build-site/ (images, build-site.json, log).
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$out = Join-Path $Root 'Saved\SliceEvidence\build-site'
New-Item -ItemType Directory -Force $out | Out-Null
$log = Join-Path $out 'build-site-pie.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_BUILD_OUT = $out
$env:ANASTASIS_BUILD_TYPE = $Type
$env:ANASTASIS_BUILD_BUILDERS = [string]$Builders
$py = (Join-Path $Root 'tools\unreal\build-site-pie.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-windowed', '-resx=1600', '-resy=900', '-nosplash', '-NoLiveCoding',
  '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'BUILD_SITE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'BUILD_SITE |ANASTASIS_VILLAGE (first site|building)' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
Write-Output ('BUILD_SITE::DONE out=' + $out)
