# Lecture d'un log d'automation Unreal, partagee par report-tests.ps1 (la porte
# qui bloque une passation) et project-health.ps1 (le rapport qui decrit l'etat).
#
# Une seule grammaire du log pour deux vocabulaires de verdict : les deux
# scripts lisaient les memes lignes chacun de leur cote, et le durcissement
# n'avait ete fait que d'un cote. Ce fichier existe pour que cela ne puisse plus
# diverger.
#
# Le point dur : un log ne dit pas seulement ce qui a echoue, il dit aussi s'il
# est complet. Un editeur qui meurt au milieu de la file n'ecrit rien pour les
# tests suivants ; les compter comme inexistants revient a rapporter une
# reussite a partir d'une preuve absente.

function Read-KnownExpectedFailures {
  param([Parameter(Mandatory)][string]$Path)
  $map = @{}
  if (-not (Test-Path $Path)) { return $map }
  Get-Content $Path | ForEach-Object {
    $line = $_.Trim()
    if ($line -and -not $line.StartsWith('#')) {
      $parts = $line.Split('|')
      $map[$parts[0].Trim()] = if ($parts.Count -ge 3) { ($parts[2]).Trim() } else { '' }
    }
  }
  return $map
}

function Read-AutomationLog {
  param(
    [Parameter(Mandatory)][AllowEmptyString()][string]$LogPath,
    [hashtable]$Known = @{},
    $LauncherExitCode = $null
  )

  $lines = @()
  if ($LogPath -and (Test-Path $LogPath)) { $lines = @(Get-Content -LiteralPath $LogPath) }

  # Le roster : ce que le lanceur a reellement trouve pour ce filtre. Il le
  # liste en clair juste apres l'annonce du compte, un chemin par ligne indentee.
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
      if ($Known.ContainsKey($path)) {
        # Success ici = la divergence attendue s'est bien produite.
        if ($result -eq 'Success') { $expected += $path } else { $broken += $path }
      } elseif ($result -eq 'Success') { $pass += $path } else { $fail += $path }
    }
  }

  # Integrite du run. Chaque entree est une raison de ne pas croire au tableau
  # ci-dessus, meme si aucune ligne FAIL n'y figure.
  $incomplete = @()
  if ($lines.Count -eq 0) {
    $incomplete += "aucun log lisible : rien a lire dans $LogPath"
  } elseif ($null -eq $declared) {
    $incomplete += "le lanceur n'a jamais annonce de liste de tests (Found N automation tests) : la suite n'a pas demarre"
  }

  $endMarker = @($lines | Where-Object { $_ -match 'TEST COMPLETE\. EXIT CODE: (-?\d+)' })[-1]
  if (-not $endMarker) {
    if ($lines.Count -gt 0) { $incomplete += 'marqueur de fin absent : la file d automation n a jamais ete videe (TEST COMPLETE. EXIT CODE)' }
  } elseif ($endMarker -match 'TEST COMPLETE\. EXIT CODE: (-?\d+)' -and [int]$Matches[1] -ne 0) {
    $incomplete += ('le lanceur d automation a termine sur EXIT CODE ' + $Matches[1])
  }

  # Une mort de l'editeur laisse plusieurs de ces marqueurs a la suite : une
  # seule ligne de diagnostic suffit, la plus parlante etant l'assertion.
  $fatal = $null
  foreach ($sig in @('Assertion failed:', 'appError called', '=== Critical error: ===', 'GIsCriticalError=1')) {
    if ($null -ne $fatal) { break }
    $fatal = @($lines | Where-Object { $_.Contains($sig) })[0]
  }
  if ($null -ne $fatal) { $incomplete += ('erreur critique dans le log : ' + $fatal.Trim()) }

  if ($null -ne $LauncherExitCode -and $LauncherExitCode -ne 0) {
    $incomplete += ('UnrealEditor-Cmd est sorti avec le code ' + $LauncherExitCode)
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

  return [PSCustomObject]@{
    Declared   = $declared
    Pass       = @($pass)
    Expected   = @($expected)
    Fail       = @($fail)
    Broken     = @($broken)
    Missing    = @($missing)
    Started    = $started
    Total      = $pass.Count + $expected.Count + $fail.Count + $broken.Count
    FailCount  = $fail.Count + $broken.Count
    Incomplete = @($incomplete)
  }
}
