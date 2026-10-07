param([int]$TimeoutSec = 1200)
# Preuve PIE de l architecture a l echelle humaine (ARCHITECTURE_SCALE_001) : pilote architecture-pie.py.
# Editeur discret (Start-AnastasisEditor), rendu garde hors focus. Sortie : Saved/ArchitectureEvidence/pie/.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$out = Join-Path $Root 'Saved\ArchitectureEvidence\pie'
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -Filter *.png -ErrorAction SilentlyContinue | Remove-Item
$log = Join-Path $out 'architecture-pie.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_ARCH_OUT = $out
$py = (Join-Path $Root 'tools\unreal\architecture-pie.py').Replace('\', '/')
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
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'ARCH_PIE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'ARCH_PIE |ANASTASIS_ARCH ' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
if (-not (Select-String -Path $log -Pattern 'ARCH_PIE PASS' -Quiet)) { throw 'ARCH_PIE::FAIL voir ' + $log }
Write-Output ('ARCH_PIE::PASS out=' + $out)
