param([string]$Mission = 'phaseg')
$ErrorActionPreference = 'Stop'
$Root = 'C:\dev\ANASTASIS_UNREAL'
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir ('probe-demo-' + $Mission + '.log')
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_PROBE_MISSION = $Mission
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-windowed', '-resx=1280', '-resy=720', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  '-ExecCmds="py C:/dev/ANASTASIS_UNREAL/tools/unreal/probe-demo.py"'
)
$p = Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout 150 -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'PROBE_DEMO::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'PROBE_DEMO_|ANASTASIS_WORLD_|CAPTURE::' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
Write-Output ('PROBE_DEMO::DONE log=' + $log)
