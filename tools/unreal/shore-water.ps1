param([switch]$Rebuild, [int]$TimeoutSec = 300)
$ErrorActionPreference = 'Stop'
# Root deduit de l'emplacement du script, comme capture-slice.ps1 : un agent doit
# pouvoir forger SON asset dans SON worktree.
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'shore-water.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_SHORE_REBUILD = if ($Rebuild) { '1' } else { '0' }
$py = (Join-Path $Root 'tools\unreal\shore-water.py').Replace('\', '/')
# -nullrhi : forger un materiau ne demande aucun rendu. Sans RHI l'editeur ne
# reserve pas son pool de textures (7 Go annonces au log d'un lancement normal) et
# tient dans une machine ou d'autres agents ont deja leurs editeurs ouverts -- un
# premier essai a fini en Fatal error OnOutOfMemory dans FAssetDataGatherer.
# C'est aussi plus honnete : ce script ne regarde rien, il ecrit un asset.
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-nullrhi', '-unattended', '-nosound', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-Process $Editor -ArgumentList $launchArgs -PassThru
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'SHORE_MATERIAL::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'SHORE_MATERIAL' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
$asset = Join-Path $Root 'Content\Anastasis\Materials\M_AnastasisShoreWater.uasset'
if (!(Test-Path $asset)) { throw 'SHORE_MATERIAL::FAIL asset absent' }
Write-Output ('SHORE_MATERIAL::PASS ' + $asset)
