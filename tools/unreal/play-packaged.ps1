param([string]$ReleaseRoot = 'C:\dev\ANASTASIS_RELEASES\Playable')

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$pointer = Join-Path $ReleaseRoot 'latest.json'
if (-not (Test-Path -LiteralPath $pointer)) { throw "GAME::ABSENT $pointer" }
$latest = Get-Content -LiteralPath $pointer -Raw | ConvertFrom-Json
$exe = [IO.Path]::GetFullPath((Join-Path $ReleaseRoot $latest.exe))
$root = [IO.Path]::GetFullPath($ReleaseRoot).TrimEnd('\') + '\'
if (-not $exe.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw 'GAME::FAIL chemin hors releases' }
if (-not (Test-Path -LiteralPath $exe)) { throw "GAME::ABSENT $exe" }
$previousVisible = $env:ANASTASIS_EDITOR_VISIBLE
$env:ANASTASIS_EDITOR_VISIBLE = '1'
try {
  $proc = Start-AnastasisEditor $exe @('-windowed', '-resx=1280', '-resy=720', '-nosplash')
  Write-Output "GAME::START_REQUESTED pid=$($proc.Id) commit=$($latest.commit)"
  Write-Output "GAME_EXE::$exe"
} finally {
  if ($null -eq $previousVisible) {
    Remove-Item Env:ANASTASIS_EDITOR_VISIBLE -ErrorAction SilentlyContinue
  } else {
    $env:ANASTASIS_EDITOR_VISIBLE = $previousVisible
  }
}
