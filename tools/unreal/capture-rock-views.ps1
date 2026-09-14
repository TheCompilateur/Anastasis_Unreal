# ROCK_FORGE_001 -- drives tools/unreal/capture-rock-views.py.
#
# A separate launcher from tools/unreal/capture-slice.ps1 on purpose: that script is
# being modified on claude/anastasis-ground-materials-727cc2, and this mission must not
# add a second writer to a file another agent already holds.
param(
    [ValidateSet('0', '1', '2')][string]$Mode = '1',
    [int]$TimeoutSec = 900
)
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'

$dir = Join-Path $Root 'Saved\RockEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
Get-ChildItem $dir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
$log = Join-Path $dir 'rock_views.log'
if (Test-Path $log) { Remove-Item $log }

$env:ANASTASIS_ROCK_SHOTDIR = $dir
$env:ANASTASIS_ROCK_MODE = $Mode

$py = (Join-Path $Root 'tools\unreal\capture-rock-views.py').Replace('\', '/')
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
    throw 'ROCK_CAPTURE::FAIL editor hung'
}

if (Test-Path $log) {
    Select-String -Path $log -Pattern 'ROCK_VIEW ' | ForEach-Object {
        $_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', ''
    }
}
$shots = Get-ChildItem $dir -Filter *.png -ErrorAction SilentlyContinue
Write-Output ("ROCK_CAPTURE shots=" + $shots.Count)
foreach ($s in $shots) { Write-Output ("  " + $s.Name + " " + $s.Length + " bytes") }
if ($shots.Count -lt 8) { throw ('ROCK_CAPTURE::FAIL expected 8 shots, got ' + $shots.Count) }
Write-Output 'ROCK_CAPTURE::PASS'
