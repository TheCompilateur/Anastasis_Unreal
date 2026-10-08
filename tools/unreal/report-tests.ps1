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
# Suite sans rendu (HEADLESS_TESTS_001). Aucun test de la suite ne lit une image
# ni le GPU : ceux de Source/AnastasisSim n'ont meme pas Engine, les autres creent
# leur propre monde (UWorld::CreateWorld) ou chargent des assets. Les lancer dans
# un editeur avec rendu, sur la carte de demarrage, payait le rendu, les shaders et
# l'incarnation du monde pour rien, et 8 a 13 Go de la porte memoire. -Mode :
#
#   auto     (defaut) -nullrhi sur /Engine/Maps/Entry. Si ce run n'est pas alle au
#            bout, la suite entiere est rejouee avec rendu, et ce second run fait
#            foi (TEST_MODE::REPLI_GPU). Si des tests echouent sans rendu, eux seuls
#            sont rejoues avec rendu : un test qui y passe ne compte PASS que s'il est
#            inscrit a rhi-tests.txt (il depend du rendu, c'est su) ; sinon il reste
#            FAIL, nomme HEADLESS_ECART:: -- dependance au rendu pas encore declaree,
#            ou test instable : une repetition qui passe ne prouve rien a elle seule.
#   headless un seul run sans rendu, sans repli.
#   gpu      le run d'avant HEADLESS_TESTS_001 : editeur avec rendu, carte de demarrage.
#
# ANASTASIS_TESTS_MODE=<mode> remplace le defaut quand -Mode n'est pas donne : la
# trappe de l'integrateur si le mode sans rendu se met a mentir (gpu).
#
# Chaque run ecrit sa duree et le pic memoire du processus UnrealEditor-Cmd lui-meme
# (pas ses ShaderCompileWorker) : TEST_MODE::<mode> duree= pic_ws= pic_prive=. C'est
# la mesure qui dira si la porte memoire peut traiter un run sans rendu a part.
#
# Logs : Saved/CanonicalVerification/report-tests.log pour le premier run (celui que
# lit project-health.ps1), report-tests-gpu.log pour un repli.
#
# Sortie : 0 si aucun FAIL et si le run qui fait foi est alle au bout, 1 sinon.
param(
  [string]$Filter = 'Anastasis',
  [ValidateSet('auto', 'headless', 'gpu')][string]$Mode = 'auto',
  [int]$TimeoutSec = 900
)
$ErrorActionPreference = 'Stop'
if (-not $PSBoundParameters.ContainsKey('Mode') -and $env:ANASTASIS_TESTS_MODE) { $Mode = $env:ANASTASIS_TESTS_MODE }
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
. (Join-Path $PSScriptRoot 'automation-log.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Engine = 'C:\Program Files\Epic Games\UE_5.8'
$Evidence = Join-Path $Root 'Saved/CanonicalVerification'
New-Item -ItemType Directory -Force $Evidence | Out-Null
$log = Join-Path $Evidence 'report-tests.log'
$gpuLog = Join-Path $Evidence 'report-tests-gpu.log'
if (Test-Path $gpuLog) { Remove-Item $gpuLog }

$known = Read-KnownExpectedFailures (Join-Path $PSScriptRoot 'known-expected-failures.txt')
# Meme format que known-expected-failures.txt : `<chemin du test> | rhi | <ce qu'il lit du rendu>`.
$rhiTests = Read-KnownExpectedFailures (Join-Path $PSScriptRoot 'rhi-tests.txt')

# Un run de la suite. Rend la lecture du log (Read-AutomationLog), la duree et les pics memoire.
function Invoke-TestRun([string]$RunFilter, [bool]$Headless, [string]$LogPath) {
  if (Test-Path $LogPath) { Remove-Item $LogPath }
  $launchArgs = @(('"' + $Root + '\Anastasis_UnrealV2.uproject"'))
  if ($Headless) { $launchArgs += @('/Engine/Maps/Entry', '-nullrhi', '-nosound') }
  $launchArgs += @(
    '-unattended','-nopause','-nosplash','-NoLiveCoding',
    ('-abslog="' + $LogPath + '"'),
    '-LogCmds="LogAutomationTest Log"',
    ('-ExecCmds="Automation RunTests ' + $RunFilter + ';Quit"'),
    '-testexit="Automation Test Queue Empty"'
  )
  $p = Start-AnastasisEditor "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $launchArgs
  $t0 = Get-Date
  $peakWs = [long]0
  $peakPriv = [long]0
  while (-not $p.HasExited) {
    try {
      $p.Refresh()
      if ($p.PeakWorkingSet64 -gt $peakWs) { $peakWs = $p.PeakWorkingSet64 }
      if ($p.PrivateMemorySize64 -gt $peakPriv) { $peakPriv = $p.PrivateMemorySize64 }
    } catch { }
    if (((Get-Date) - $t0).TotalSeconds -ge $TimeoutSec) { Stop-Process -Id $p.Id -Force; throw 'TESTS::FAIL lanceur bloque' }
    Start-Sleep -Seconds 2
  }
  $exit = $null
  try { $exit = $p.ExitCode } catch { $exit = $null }
  $label = if ($Headless) { 'HEADLESS' } else { 'GPU' }
  Write-Output ('TEST_MODE::{0} duree={1:N0}s pic_ws={2:N1} Go pic_prive={3:N1} Go (processus principal)' -f `
    $label, ((Get-Date) - $t0).TotalSeconds, ($peakWs / 1GB), ($peakPriv / 1GB))
  return (Read-AutomationLog -LogPath $LogPath -Known $known -LauncherExitCode $exit)
}

# Verdict par test d'un run : pass / expected / fail / broken.
function Get-Verdicts($r) {
  $v = @{}
  foreach ($t in $r.Pass) { $v[$t] = 'pass' }
  foreach ($t in $r.Expected) { $v[$t] = 'expected' }
  foreach ($t in $r.Fail) { $v[$t] = 'fail' }
  foreach ($t in $r.Broken) { $v[$t] = 'broken' }
  return $v
}

$first = $null
foreach ($o in @(Invoke-TestRun $Filter ($Mode -ne 'gpu') $log)) {
  if ($o -is [string]) { Write-Output $o } else { $first = $o }
}
$run = $first

if ($Mode -eq 'auto' -and $first.Incomplete.Count -gt 0) {
  Write-Output 'TEST_MODE::REPLI_GPU suite complete -- le run sans rendu n est pas alle au bout :'
  foreach ($r in $first.Incomplete) { Write-Output ('    ' + $r) }
  Write-Output ('    log sans rendu : ' + $log)
  foreach ($o in @(Invoke-TestRun $Filter $false $gpuLog)) {
    if ($o -is [string]) { Write-Output $o } else { $run = $o }
  }
  $log = $gpuLog
} elseif ($Mode -eq 'auto' -and $first.FailCount -gt 0) {
  $suspects = @($first.Fail) + @($first.Broken)
  Write-Output ('TEST_MODE::REPLI_GPU ' + $suspects.Count + ' test(s) en echec sans rendu, rejoue(s) avec rendu')
  # Au-dela de 40 noms, la ligne de commande approche la limite de Windows, et un echec aussi
  # large dit que le mode sans rendu lui-meme est en cause : la suite entiere est rejouee.
  $rerun = if ($suspects.Count -le 40) { $suspects -join '+' } else { $Filter }
  $second = $null
  foreach ($o in @(Invoke-TestRun $rerun $false $gpuLog)) {
    if ($o -is [string]) { Write-Output $o } else { $second = $o }
  }
  $v2 = Get-Verdicts $second
  $pass = @($first.Pass); $expected = @($first.Expected); $fail = @(); $broken = @()
  foreach ($t in $suspects) {
    $after = $v2[$t]
    $before = if ($first.Fail -contains $t) { 'fail' } else { 'broken' }
    if (($after -eq 'pass' -or $after -eq 'expected') -and $rhiTests.ContainsKey($t)) {
      Write-Output ('RHI_TEST::' + $t + ' -- echoue sans rendu, ' + $after.ToUpper() + ' avec rendu (rhi-tests.txt : ' + $rhiTests[$t] + ')')
      if ($after -eq 'pass') { $pass += $t } else { $expected += $t }
      continue
    }
    if ($after -eq 'pass' -or $after -eq 'expected') {
      Write-Output ('HEADLESS_ECART::' + $t + ' -- echoue sans rendu, ' + $after.ToUpper() + ' avec rendu ; ni inscrit a rhi-tests.txt ni prouve stable : reste FAIL')
    } elseif ($null -eq $after) {
      Write-Output ('HEADLESS_ECART::' + $t + ' -- aucun resultat au run avec rendu : reste FAIL')
    }
    if ($before -eq 'fail') { $fail += $t } else { $broken += $t }
  }
  $incomplete = @()
  foreach ($r in $second.Incomplete) { $incomplete += ('repli avec rendu : ' + $r) }
  $run = [PSCustomObject]@{
    Declared   = $first.Declared
    Pass       = @($pass)
    Expected   = @($expected)
    Fail       = @($fail)
    Broken     = @($broken)
    Missing    = @($second.Missing)
    Started    = $second.Started
    Total      = $first.Total
    FailCount  = $fail.Count + $broken.Count
    Incomplete = @($incomplete)
  }
  if ($incomplete.Count -gt 0) { $log = $gpuLog }
}

# Une entree du registre qui passe sans rendu n'a plus lieu d'etre.
if ($Mode -ne 'gpu') {
  foreach ($t in $first.Pass + $first.Expected) {
    if ($rhiTests.ContainsKey($t)) { Write-Output ('RHI_TEST::INUTILE ' + $t + ' -- passe sans rendu : retirer sa ligne de rhi-tests.txt') }
  }
}

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
