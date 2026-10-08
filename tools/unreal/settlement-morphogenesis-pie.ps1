param([int]$TimeoutSec = 2400, [int]$Days = 36, [switch]$Keep)
# Preuve PIE de la morphogenese du peuplement (SETTLEMENT_MORPHOGENESIS_001) : pilote
# settlement-morphogenesis-pie.py. Editeur discret (Start-AnastasisEditor), rendu garde hors focus.
# Sortie : Saved/SettlementEvidence/pie/ (images, status-j0.json, status-apres.json, rapport, log).
# -Keep : l'editeur reste ouvert apres le verdict (PIE en place) ; le lanceur rend la main sans le fermer.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$out = Join-Path $Root 'Saved\SettlementEvidence\pie'
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -Filter *.png -ErrorAction SilentlyContinue | Remove-Item
$log = Join-Path $out 'settlement-morphogenesis-pie.log'
if (Test-Path $log) { Remove-Item $log }
$env:ANASTASIS_MORPH_OUT = $out
$env:ANASTASIS_MORPH_DAYS = "$Days"
$env:ANASTASIS_MORPH_KEEP = if ($Keep) { '1' } else { '0' }
$py = (Join-Path $Root 'tools\unreal\settlement-morphogenesis-pie.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-windowed', '-resx=1600', '-resy=900', '-nosplash', '-NoLiveCoding',
  '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
$p = Start-AnastasisEditor $Editor $launchArgs
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  if ($Keep -and (Test-Path $log) -and (Select-String -Path $log -Pattern 'MORPH_PIE (PASS|FAIL)' -Quiet)) { break }
  Start-Sleep -Seconds 5
  $p.Refresh()
}
$p.Refresh()
if (-not $p.HasExited -and -not $Keep) { Stop-Process -Id $p.Id -Force; throw 'MORPH_PIE::FAIL editeur bloque' }
Select-String -Path $log -Pattern 'MORPH_PIE |ANASTASIS_SETTLEMENT (bio|traffic|paths|reform)' | ForEach-Object { ($_.Line -replace '^\[[^\]]*\]\[[ 0-9]*\]', '') }
if ($Keep -and -not $p.HasExited) { Write-Output ('MORPH_PIE::EDITOR_KEPT pid=' + $p.Id) }
if (-not (Select-String -Path $log -Pattern 'MORPH_PIE PASS' -Quiet)) { throw 'MORPH_PIE::FAIL voir ' + $log }
Write-Output ('MORPH_PIE::PASS out=' + $out)
