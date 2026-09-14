param(
  [Parameter(Mandatory = $true)][string]$Out,
  [string]$View = 'MID',
  [ValidateSet('0', '1')][string]$Mode = '1',
  [int]$TimeoutSec = 300)
$ErrorActionPreference = 'Stop'
# Root deduit de l'emplacement du script : la preuve doit porter sur les binaires
# du worktree qui la produit, jamais sur ceux du canonique.
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$shot = Join-Path $dir $Out
if (Test-Path $shot) { Remove-Item $shot }
$log = Join-Path $dir ($Out -replace '\.png$', '.log')
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_SHORE_SHOT = $shot
$env:ANASTASIS_SHORE_VIEW = $View
$env:ANASTASIS_SHORE_MODE = $Mode
$py = (Join-Path $Root 'tools\unreal\shore-capture.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-windowed', '-resx=1280', '-resy=720', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'SHORE_CAPTURE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'SHORE_|ANASTASIS_SHORELINE|ANASTASIS_TERRAIN ' |
  ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
if (!(Test-Path $shot)) { throw 'SHORE_CAPTURE::FAIL pas de capture' }
Write-Output ('SHORE_CAPTURE::PASS ' + $shot)
