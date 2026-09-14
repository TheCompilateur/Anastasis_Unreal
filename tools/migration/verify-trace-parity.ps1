# Preuve de bout en bout de la moitie Unreal du harnais differentiel.
#
# Le test C++ ecrit une trace d'empreintes; `compare-digests.mjs` — l'outil du
# harnais, pas un comparateur ecrit pour l'occasion — la compare a la trace JS
# des memes echantillons. Les deux doivent etre declarees identiques.
#
# Une preuve qui ne vit que dans un commentaire se perime sans que personne le
# voie. Celle-ci se rejoue:
#
#   tools\migration\verify-trace-parity.ps1
#
# Sortie: 0 si identiques, 1 sinon.

$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Engine = 'C:\Program Files\Epic Games\UE_5.8'
$Trace = Join-Path $Root 'Saved\CanonicalVerification\trace-cpp.jsonl'
$Reference = Join-Path $PSScriptRoot 'fixtures\trace-synthetique.jsonl'

if (-not (Test-Path $Reference)) {
  throw "Trace JS de reference absente. La regenerer: node tools/migration/gen-trace-vectors.mjs"
}

# Le test d'automation ecrit la trace. On le lance seul: la suite entiere
# couterait une minute pour une preuve qui en demande dix secondes.
if (Test-Path $Trace) { Remove-Item $Trace }
$log = Join-Path $Root 'Saved\CanonicalVerification\trace-parity.log'
$args = @(
  ('"' + $Root + '\Anastasis_UnrealV2.uproject"'),
  '-unattended','-nopause','-nosplash','-nullrhi','-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  '-ExecCmds="Automation RunTests Anastasis.Sim.Parite.Trace;Quit"',
  '-testexit="Automation Test Queue Empty"'
)
$p = Start-Process "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgumentList $args -WindowStyle Hidden -PassThru
$p | Wait-Process -Timeout 300 -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'TRACE::FAIL lanceur bloque' }

if (-not (Test-Path $Trace)) {
  throw "TRACE::FAIL le test n'a pas ecrit $Trace (voir $log)"
}

# C'est le comparateur du harnais qui tranche, pas ce script.
Write-Host "--- verdict du harnais ---"
& node (Join-Path $PSScriptRoot 'compare-digests.mjs') $Trace $Reference
$code = $LASTEXITCODE
if ($code -eq 0) { Write-Host "TRACE::PASS trace Unreal identique a la trace JS" }
else { Write-Host "TRACE::FAIL divergence (code $code)" }
exit $code
