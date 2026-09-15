# Vues du monde entier. Lanceur de tools/unreal/capture-world-views.py.
param(
    [ValidateSet('0', '1', '2')][string]$Mode = '2',
    [int]$TimeoutSec = 900
)
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'

$dir = Join-Path $Root 'Saved\WorldEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
Get-ChildItem $dir -Filter *.png -ErrorAction SilentlyContinue | Remove-Item -Force
$log = Join-Path $dir 'world_views.log'
if (Test-Path $log) { Remove-Item $log }

$env:ANASTASIS_WORLD_SHOTDIR = $dir
$env:ANASTASIS_WORLD_MODE = $Mode

# Chemin en slashes : un antislash suivi de t devient une TABULATION avant que le
# commandlet ne le voie. Meme precaution que observe-slice.py.
$py = (Join-Path $Root 'tools\unreal\capture-world-views.py').Replace('\', '/')
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
    throw 'WORLD_CAPTURE::FAIL editor hung'
}

if (Test-Path $log) {
    Select-String -Path $log -Pattern 'WORLD_VIEW ' | ForEach-Object {
        $_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', ''
    }
}
$shots = Get-ChildItem $dir -Filter *.png -ErrorAction SilentlyContinue
Write-Output ("WORLD_CAPTURE shots=" + $shots.Count)
foreach ($s in $shots) { Write-Output ("  " + $s.Name + " " + $s.Length + " bytes") }
if ($shots.Count -lt 1) { throw ('WORLD_CAPTURE::FAIL aucune capture') }
Write-Output 'WORLD_CAPTURE::PASS'
