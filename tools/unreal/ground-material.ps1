param([switch]$Rebuild, [switch]$ReimportTextures, [int]$TimeoutSec = 600)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
# Root deduit du script, comme capture-slice.ps1 et probe-demo.ps1 : un agent doit
# pouvoir generer les assets de SON worktree, pas de la racine canonique.
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir = Join-Path $Root 'Saved\SliceEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'ground-material.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_GROUND_REBUILD = if ($Rebuild) { '1' } else { '0' }
$env:ANASTASIS_GROUND_TEXTURES_REIMPORT = if ($ReimportTextures) { '1' } else { '0' }
$py = (Join-Path $Root 'tools\unreal\ground-material.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-unattended', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'GROUND_MATERIAL::FAIL editeur bloque' }

Select-String -Path $log -Pattern 'GROUND_MATERIAL' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }

# Le script Python LEVE sur toute liaison ratee. Une exception remonte dans le log en
# LogPython Error : la chercher explicitement, sinon un materiau a moitie cable serait
# rapporte comme un succes -- il compile, il est juste faux.
$failed = Select-String -Path $log -Pattern 'LogPython: Error|GROUND_MATERIAL.*FAILED|Traceback'
if ($failed) {
  $failed | ForEach-Object { Write-Output $_.Line }
  throw 'GROUND_MATERIAL::FAIL script python en erreur'
}
# Un materiau qui ne compile pas ne leve rien : le moteur ecrit un Warning "Failed to
# compile Material", sauve l'asset et rend le Default Material. Seuls comptent ceux qui
# suivent RECOMPILE_BEGIN -- avant, ce sont les etats intermediaires du graphe.
$lines = Get-Content $log
$begin = ($lines | Select-String -Pattern 'GROUND_MATERIAL RECOMPILE_BEGIN' | Select-Object -Last 1)
if ($begin) {
  $after = $lines[($begin.LineNumber)..($lines.Count - 1)]
  $broken = $after | Select-String -Pattern 'Failed to compile Material.*AnastasisGround' -Context 0,3
  if ($broken) {
    $broken | ForEach-Object { Write-Output $_.Line; $_.Context.PostContext | ForEach-Object { Write-Output $_ } }
    throw 'GROUND_MATERIAL::FAIL M_AnastasisGround ne compile pas'
  }
}
if (-not (Select-String -Path $log -Pattern 'GROUND_MATERIAL COMPLETE')) {
  throw 'GROUND_MATERIAL::FAIL pas de marqueur COMPLETE'
}
Write-Output ('GROUND_MATERIAL::PASS log=' + $log)
