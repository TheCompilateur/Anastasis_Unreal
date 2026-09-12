# Lance la suite d'automation ANASTASIS et rapporte honnetement trois categories
# distinctes : PASS / KNOWN_EXPECTED_FAILURE / FAIL.
#
# Le lanceur d'Unreal rapporte "Success" pour un test marque AddExpectedError ou
# @unittest.expectedFailure. Agreger ces Success avec les vrais PASS donne une
# suite verte trompeuse. Ce script croise les resultats avec
# tools/unreal/known-expected-failures.txt et refuse de les confondre.
#
# Sortie : 0 si aucun FAIL, 1 sinon.
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

$pass = @(); $expected = @(); $fail = @(); $broken = @()
Select-String -Path $log -Pattern 'Test Completed\. Result=\{(\w+)\}.*Path=\{([^}]+)\}' | ForEach-Object {
  $m = $_.Matches[0]
  $result = $m.Groups[1].Value
  $path = $m.Groups[2].Value
  if ($known.ContainsKey($path)) {
    # Success ici = la divergence attendue s'est bien produite.
    if ($result -eq 'Success') { $expected += $path } else { $broken += $path }
  } elseif ($result -eq 'Success') { $pass += $path } else { $fail += $path }
}

Write-Output ("PASS                  : " + $pass.Count)
Write-Output ("KNOWN_EXPECTED_FAILURE: " + $expected.Count)
foreach ($e in $expected) { Write-Output ("    " + $e + "`n        cause: " + $known[$e]) }
Write-Output ("FAIL                  : " + ($fail.Count + $broken.Count))
foreach ($f in $fail) { Write-Output ("    " + $f) }
foreach ($b in $broken) { Write-Output ("    " + $b + "  (attendu en echec connu mais rapporte en echec reel : revoir le registre)") }
$total = $pass.Count + $expected.Count + $fail.Count + $broken.Count
Write-Output ("TOTAL                 : " + $total)
if ($fail.Count + $broken.Count -gt 0) { Write-Output 'TESTS::FAIL'; exit 1 }
Write-Output 'TESTS::PASS (echecs connus exclus, jamais comptes comme PASS)'
