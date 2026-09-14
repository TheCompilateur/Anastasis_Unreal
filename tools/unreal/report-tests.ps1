# Lance la suite d'automation ANASTASIS et rapporte honnetement trois categories
# distinctes : PASS / KNOWN_EXPECTED_FAILURE / FAIL.
#
# Le lanceur d'Unreal rapporte "Success" pour un test marque AddExpectedError ou
# @unittest.expectedFailure. Agreger ces Success avec les vrais PASS donne une
# suite verte trompeuse. Ce script croise les resultats avec
# tools/unreal/known-expected-failures.txt et refuse de les confondre.
#
# Un quatrieme cas, qui n'est pas une categorie de test mais une categorie de
# run : la suite n'est pas allee au bout. Un crash de l'editeur au milieu de la
# file laisse un log ou les tests suivants sont simplement absents. Compter les
# lignes "Test Completed" survivantes et les declarer vertes, c'est rapporter
# une reussite a partir d'un postflight qui a echoue. La lecture du log, y
# compris ses preuves de completude, vit dans automation-log.ps1.
#
# Sortie : 0 si aucun FAIL et si le run est alle au bout, 1 sinon.
param([string]$Filter = 'Anastasis')
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'automation-log.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Engine = 'C:\Program Files\Epic Games\UE_5.8'
$Evidence = Join-Path $Root 'Saved/CanonicalVerification'
New-Item -ItemType Directory -Force $Evidence | Out-Null
$log = Join-Path $Evidence 'report-tests.log'
if (Test-Path $log) { Remove-Item $log }

$known = Read-KnownExpectedFailures (Join-Path $PSScriptRoot 'known-expected-failures.txt')

$launchArgs = @(
  ('"' + $Root + '\Anastasis_UnrealV2.uproject"'),
  '-unattended','-nopause','-nosplash','-NoLiveCoding',
  ('-abslog="' + $log + '"'),
  '-LogCmds="LogAutomationTest Log"',
  ('-ExecCmds="Automation RunTests ' + $Filter + ';Quit"'),
  '-testexit="Automation Test Queue Empty"'
)
$p = Start-Process "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgumentList $launchArgs -WindowStyle Hidden -PassThru
$p | Wait-Process -Timeout 900 -ErrorAction SilentlyContinue
$p.Refresh()
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; throw 'TESTS::FAIL lanceur bloque' }
$launcherExit = $null
try { $launcherExit = $p.ExitCode } catch { $launcherExit = $null }

$run = Read-AutomationLog -LogPath $log -Known $known -LauncherExitCode $launcherExit

Write-Output ("PASS                  : " + $run.Pass.Count)
Write-Output ("KNOWN_EXPECTED_FAILURE: " + $run.Expected.Count)
foreach ($e in $run.Expected) { Write-Output ("    " + $e + "`n        cause: " + $known[$e]) }
Write-Output ("FAIL                  : " + $run.FailCount)
foreach ($f in $run.Fail) { Write-Output ("    " + $f) }
foreach ($b in $run.Broken) { Write-Output ("    " + $b + "  (attendu en echec connu mais rapporte en echec reel : revoir le registre)") }
Write-Output ("TOTAL                 : " + $run.Total)
if ($null -ne $run.Declared) { Write-Output ("ANNONCES PAR LE LANCEUR: " + $run.Declared) }

if ($run.Incomplete.Count -gt 0) {
  Write-Output ''
  Write-Output 'RUN_INCOMPLET :: le tableau ci-dessus ne couvre pas toute la suite.'
  foreach ($r in $run.Incomplete) { Write-Output ("    " + $r) }
  if ($run.Missing.Count -gt 0) {
    Write-Output ''
    Write-Output 'Tests sans resultat :'
    $shown = 0
    foreach ($m in $run.Missing) {
      if ($shown -ge 20) { Write-Output ("    ... et " + ($run.Missing.Count - 20) + " autre(s), voir le log"); break }
      $mark = if ($run.Started.ContainsKey($m)) { '  <- demarre, jamais termine : suspect du crash' } else { '' }
      Write-Output ("    " + $m + $mark)
      $shown++
    }
  }
  Write-Output ''
  Write-Output ('Log complet : ' + $log)
  Write-Output 'TESTS::FAIL'
  exit 1
}

if ($run.FailCount -gt 0) { Write-Output 'TESTS::FAIL'; exit 1 }
Write-Output 'TESTS::PASS (echecs connus exclus, jamais comptes comme PASS)'
