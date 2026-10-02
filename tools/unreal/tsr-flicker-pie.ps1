param([string]$Label = 'latest', [int]$Frames = 24, [string]$Views = '', [string]$States = '', [int]$TimeoutSec = 1800)
# TSR_FLICKER_001 : scintillement de l'herbe et des branches, images successives en PIE.
# Pilote tsr-flicker-pie.py (camera fixe, vent actif, `Shot` image apres image), puis
# tsr-flicker-metrics.py si Python systeme a numpy + Pillow.
# Sortie : Saved\TsrFlickerEvidence\<Label>\<vue>_<etat>\fNN.png + tsr-flicker.json + metrics.json
#   -States 'd0=r.TSR.ThinGeometryDetection 0|d1=r.TSR.ThinGeometryDetection 1|d0b=r.TSR.ThinGeometryDetection 0'
#     (defaut du script) ; un autre A/B se donne sous la meme forme.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$out = Join-Path $Root "Saved\TsrFlickerEvidence\$Label"
New-Item -ItemType Directory -Force $out | Out-Null
$log = Join-Path $out 'tsr-flicker-pie.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_TSR_OUT = $out
$env:ANASTASIS_TSR_FRAMES = "$Frames"
$env:ANASTASIS_TSR_VIEWS = $Views
$env:ANASTASIS_TSR_STATES = $States
$py = (Join-Path $Root 'tools\unreal\tsr-flicker-pie.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-windowed', '-resx=1600', '-resy=900', '-nosplash', '-NoLiveCoding',
  # Hors premier plan, l'editeur coupe le rendu des viewports (cf. capture-sky.ps1).
  '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'TSR_FLICKER::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'TSR_FLICKER ' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
if (-not (Select-String -Path $log -Pattern 'TSR_FLICKER COMPLETE' -Quiet)) { throw 'TSR_FLICKER::FAIL capture incomplete' }
& python (Join-Path $Root 'tools\unreal\tsr-flicker-metrics.py') $out
Write-Output ('TSR_FLICKER::DONE out=' + $out)
