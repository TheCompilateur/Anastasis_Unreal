# ARCHITECTURE_SCALE_001 (architecture-crusade-001) -- ecrit /Game/Anastasis/VillageArchitecture : sept
# maisonnees a l'echelle humaine (corps + assise), le kit de 23 pieces et M_AnastasisArchitecture.
# Editeur dedie sur la carte vide du moteur, discret, qui se ferme. Voir create-village-architecture.py.
#   -Rebuild   regenere aussi le materiau (les meshes sont toujours reecrits : le generateur fait foi)
param([switch]$Rebuild, [int]$TimeoutSec = 2400)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$dir = Join-Path $Root 'Saved\ArchitectureEvidence'
New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir 'create-village-architecture.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_ARCH_REBUILD = if ($Rebuild) { '1' } else { '0' }
$env:ANASTASIS_ARCH_GEOMETRY_ONLY = '0'
$py = (Join-Path $Root 'tools\unreal\create-village-architecture.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  # Carte vide : la carte de demarrage incarne le monde et tiendrait les assets a reecrire.
  '/Engine/Maps/Entry',
  '-unattended', '-nosplash', '-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$p | Wait-Process -Timeout $TimeoutSec -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'ARCH::FAIL editeur bloque' }

$lines = Get-Content $log
$lines | Select-String -Pattern 'ARCH[ _]' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }

$failed = $lines | Select-String -Pattern 'ARCH FAILED|ARCH_SCALE_FAIL|Traceback'
if ($failed) { $failed | ForEach-Object { Write-Output $_.Line }; throw 'ARCH::FAIL script python en erreur' }
# Un materiau qui ne compile pas n'est qu'un Warning : refuser tout echec apres le marqueur de compilation.
$mark = ($lines | Select-String -Pattern 'ARCH_MATERIAL_COMPILE_BEGIN' | Select-Object -First 1)
if ($mark) {
  $late = $lines[($mark.LineNumber - 1)..($lines.Count - 1)] | Select-String -Pattern 'M_AnastasisArchitecture.*Failed to compile|Failed to compile Material.*M_AnastasisArchitecture'
  if ($late) { $late | ForEach-Object { Write-Output $_.Line }; throw 'ARCH::FAIL M_AnastasisArchitecture ne compile pas' }
}
if (-not ($lines | Select-String -Pattern 'ARCH COMPLETE')) { throw 'ARCH::FAIL pas de marqueur COMPLETE' }
Write-Output ('ARCH::PASS log=' + $log)
