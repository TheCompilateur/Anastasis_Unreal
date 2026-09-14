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
# une reussite a partir d'un postflight qui a echoue. Le script exige donc que
# le run se soit termine : roster annonce entierement rapporte, marqueur de fin
# atteint, aucune erreur critique, code de sortie du lanceur nul.
#
# Sortie : 0 si aucun FAIL et si le run est alle au bout, 1 sinon.
param([string]$Filter = 'Anastasis')
$ErrorActionPreference = 'Stop'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Engine = 'C:\Program Files\Epic Games\UE_5.8'
$Evidence = Join-Path $Root 'Saved/CanonicalVerification'
New-Item -ItemType Directory -Force $Evidence | Out-Null
$log = Join-Path $Evidence 'report-tests.log'
if (Test-Path $log) { Remove-Item $log }

$known = @{}
Get-Content (Join-Path $PSScriptRoot 'known-expected-failures.txt') | ForEach-Object {
  $line = $_.Trim()
  if ($line -and -not $line.StartsWith('#')) {
    $parts = $line.Split('|')
    $known[$parts[0].Trim()] = ($parts[2]).Trim()
  }
}

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

$lines = @()
if (Test-Path $log) { $lines = @(Get-Content -LiteralPath $log) }

# Le roster : ce que le lanceur a reellement trouve pour ce filtre. Il le liste
# en clair juste apres l'annonce du compte, un chemin par ligne indentee.
$declared = $null
$roster = New-Object System.Collections.Generic.List[string]
for ($i = 0; $i -lt $lines.Count; $i++) {
  if ($lines[$i] -match 'LogAutomationCommandLine: Display: Found (\d+) automation tests based on') {
    $declared = [int]$Matches[1]
    for ($j = $i + 1; $j -lt $lines.Count; $j++) {
      if ($lines[$j] -match "LogAutomationCommandLine: Display: `t(\S.*?)\s*$") { $roster.Add($Matches[1]) } else { break }
    }
    break
  }
}

$pass = @(); $expected = @(); $fail = @(); $broken = @()
$reported = @{}; $started = @{}
foreach ($line in $lines) {
  if ($line -match 'Test Started\. .*Path=\{([^}]+)\}') { $started[$Matches[1]] = $true; continue }
  if ($line -match 'Test Completed\. Result=\{(\w+)\}.*Path=\{([^}]+)\}') {
    $result = $Matches[1]
    $path = $Matches[2]
    $reported[$path] = $true
    if ($known.ContainsKey($path)) {
      # Success ici = la divergence attendue s'est bien produite.
      if ($result -eq 'Success') { $expected += $path } else { $broken += $path }
    } elseif ($result -eq 'Success') { $pass += $path } else { $fail += $path }
  }
}

# Integrite du run. Chaque entree est une raison de ne pas croire au tableau
# ci-dessus, meme si aucune ligne FAIL n'y figure.
$incomplete = @()
if ($lines.Count -eq 0) {
  $incomplete += "aucun log produit : le lanceur n'a rien ecrit dans $log"
} elseif ($null -eq $declared) {
  $incomplete += "le lanceur n'a jamais annonce de liste de tests (Found N automation tests) : la suite n'a pas demarre"
}

$endMarker = @($lines | Where-Object { $_ -match 'TEST COMPLETE\. EXIT CODE: (-?\d+)' })[-1]
if (-not $endMarker) {
  if ($lines.Count -gt 0) { $incomplete += 'marqueur de fin absent : la file d automation n a jamais ete videe (TEST COMPLETE. EXIT CODE)' }
} elseif ($endMarker -match 'TEST COMPLETE\. EXIT CODE: (-?\d+)' -and [int]$Matches[1] -ne 0) {
  $incomplete += ('le lanceur d automation a termine sur EXIT CODE ' + $Matches[1])
}

# Une mort de l'editeur laisse plusieurs de ces marqueurs a la suite : une seule
# ligne de diagnostic suffit, la plus parlante etant l'assertion elle-meme.
$fatal = $null
foreach ($sig in @('Assertion failed:', 'appError called', '=== Critical error: ===', 'GIsCriticalError=1')) {
  if ($null -ne $fatal) { break }
  $fatal = @($lines | Where-Object { $_.Contains($sig) })[0]
}
if ($null -ne $fatal) { $incomplete += ('erreur critique dans le log : ' + $fatal.Trim()) }

if ($null -ne $launcherExit -and $launcherExit -ne 0) {
  $incomplete += ('UnrealEditor-Cmd est sorti avec le code ' + $launcherExit)
}

$missing = @()
if ($roster.Count -gt 0) {
  $missing = @($roster | Where-Object { -not $reported.ContainsKey($_) })
  if ($missing.Count -gt 0) {
    $incomplete += ($missing.Count.ToString() + ' test(s) annonce(s) par le lanceur n ont jamais rapporte de resultat')
  }
} elseif ($null -ne $declared -and $declared -ne $reported.Count) {
  $incomplete += ('le lanceur a annonce ' + $declared + ' test(s), ' + $reported.Count + ' ont rapporte un resultat')
}

Write-Output ("PASS                  : " + $pass.Count)
Write-Output ("KNOWN_EXPECTED_FAILURE: " + $expected.Count)
foreach ($e in $expected) { Write-Output ("    " + $e + "`n        cause: " + $known[$e]) }
Write-Output ("FAIL                  : " + ($fail.Count + $broken.Count))
foreach ($f in $fail) { Write-Output ("    " + $f) }
foreach ($b in $broken) { Write-Output ("    " + $b + "  (attendu en echec connu mais rapporte en echec reel : revoir le registre)") }
$total = $pass.Count + $expected.Count + $fail.Count + $broken.Count
Write-Output ("TOTAL                 : " + $total)
if ($null -ne $declared) { Write-Output ("ANNONCES PAR LE LANCEUR: " + $declared) }

if ($incomplete.Count -gt 0) {
  Write-Output ''
  Write-Output 'RUN_INCOMPLET :: le tableau ci-dessus ne couvre pas toute la suite.'
  foreach ($r in $incomplete) { Write-Output ("    " + $r) }
  if ($missing.Count -gt 0) {
    Write-Output ''
    Write-Output 'Tests sans resultat :'
    $shown = 0
    foreach ($m in $missing) {
      if ($shown -ge 20) { Write-Output ("    ... et " + ($missing.Count - 20) + " autre(s), voir le log"); break }
      $mark = if ($started.ContainsKey($m)) { '  <- demarre, jamais termine : suspect du crash' } else { '' }
      Write-Output ("    " + $m + $mark)
      $shown++
    }
  }
  Write-Output ''
  Write-Output ('Log complet : ' + $log)
  Write-Output 'TESTS::FAIL'
  exit 1
}

if ($fail.Count + $broken.Count -gt 0) { Write-Output 'TESTS::FAIL'; exit 1 }
Write-Output 'TESTS::PASS (echecs connus exclus, jamais comptes comme PASS)'
