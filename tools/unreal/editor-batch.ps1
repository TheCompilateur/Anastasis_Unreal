param(
  [string[]]$Proofs,
  # Affiche le registre tel qu'il est lu, sans demarrer d'editeur.
  [switch]$List,
  # Prepare jobs.json et s'arrete avant l'editeur (banc d'essai).
  [switch]$DryRun,
  # Marge sur la somme des delais des preuves (demarrage de l'editeur, chargements de carte).
  [int]$SlackSec = 900
)
# Plusieurs preuves PIE du registre proofs.txt dans UN SEUL editeur (EDITOR_QUEUE_001) :
# un demarrage, une attente de porte memoire, au lieu d'un par preuve. Pilote editor-batch.py.
# Editeur discret (Start-AnastasisEditor), racine = celle de ce script (worktree ou _integration).
# Sortie : Saved/EditorBatch/<horodatage>/ (editor-batch.log, jobs.json).
#   PROOF::PASS <nom> (<s>)   PROOF::FAIL <nom> <raison>   EDITOR_BATCH::PASS|FAIL
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'editor-launch.ps1')
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$Editor = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'

# Registre : nom | script | reussite | echec | delai | variables. Separateur « | » ENTOURE
# D'ESPACES : un motif regex garde ses alternatives `a|b` collees.
$registry = [ordered]@{}
foreach ($line in (Get-Content (Join-Path $PSScriptRoot 'proofs.txt') -Encoding UTF8)) {
  if (-not $line.Trim() -or $line.TrimStart().StartsWith('#')) { continue }
  $f = @($line -split '\s\|\s' | ForEach-Object { $_.Trim() })
  if ($f.Count -ne 6) { throw "EDITOR_BATCH::FAIL ligne de registre illisible ($($f.Count) champs au lieu de 6) : $line" }
  $vars = [ordered]@{}
  if ($f[5] -and $f[5] -ne '-') {
    foreach ($pair in $f[5].Split(';')) {
      $kv = $pair.Split([char[]]'=', 2)
      $v = $kv[1].Trim()
      if ($v.StartsWith('@')) { $v = (Join-Path $Root $v.Substring(1)).Replace('/', '\') }
      $vars[$kv[0].Trim()] = $v
    }
  }
  $registry[$f[0]] = [PSCustomObject]@{
    Name = $f[0]; Script = (Join-Path $Root $f[1]).Replace('\', '/')
    Pass = $f[2]; Fail = $(if ($f[3] -eq '-') { '' } else { $f[3] }); Timeout = [int]$f[4]; Env = $vars
  }
}

if ($List) {
  foreach ($p in $registry.Values) {
    Write-Output ("PROOF_REGISTERED::$($p.Name) script=$($p.Script) pass=/$($p.Pass)/ fail=/$($p.Fail)/ timeout=$($p.Timeout)s " +
      $(if (Test-Path $p.Script) { 'script=present' } else { 'script=ABSENT' }))
    foreach ($k in $p.Env.Keys) { Write-Output "    $k=$($p.Env[$k])" }
  }
  exit 0
}
$names = @($Proofs | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ } | Select-Object -Unique)
$unknown = @($names | Where-Object { -not $registry.Contains($_) })
if ($unknown.Count -gt 0) { throw ('EDITOR_BATCH::FAIL preuve(s) absente(s) de proofs.txt : ' + ($unknown -join ', ')) }
if ($names.Count -eq 0) { Write-Output 'EDITOR_BATCH::PASS 0/0 (aucune preuve demandee)'; exit 0 }

$out = Join-Path $Root ('Saved\EditorBatch\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $out | Out-Null
$jobs = @($names | ForEach-Object {
  $proof = $registry[$_]
  foreach ($v in $proof.Env.Values) { if ($v -like "$Root*" -and -not (Test-Path $v)) { New-Item -ItemType Directory -Force $v | Out-Null } }
  [ordered]@{ name = $proof.Name; script = $proof.Script; timeout = $proof.Timeout; env = $proof.Env }
})
$jobsFile = Join-Path $out 'jobs.json'
ConvertTo-Json -InputObject $jobs -Depth 4 | Set-Content $jobsFile -Encoding UTF8
$log = Join-Path $out 'editor-batch.log'
if ($DryRun) {
  Write-Output ("EDITOR_BATCH::DRYRUN $($names.Count) preuve(s) : " + ($names -join ', ') + " -> $jobsFile")
  exit 0
}
$env:ANASTASIS_EDITOR_BATCH_JOBS = $jobsFile
$py = (Join-Path $PSScriptRoot 'editor-batch.py').Replace('\', '/')
$launchArgs = @(
  ('"' + (Join-Path $Root 'Anastasis_UnrealV2.uproject') + '"'),
  '-windowed', '-resx=1280', '-resy=720', '-nosplash', '-NoLiveCoding',
  '-ini:EditorSettings:[/Script/UnrealEd.EditorPerformanceSettings]:bThrottleCPUWhenNotForeground=False',
  ('-abslog="' + $log + '"'),
  ('-ExecCmds="py ' + $py + '"')
)
Write-Output ("EDITOR_BATCH::START $($names.Count) preuve(s) dans un editeur : " + ($names -join ', '))
$budget = ($names | ForEach-Object { $registry[$_].Timeout } | Measure-Object -Sum).Sum + $SlackSec
$proc = Start-AnastasisEditor $Editor $launchArgs
$proc | Wait-Process -Timeout $budget -ErrorAction SilentlyContinue
$proc.Refresh()
if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force; Write-Output "EDITOR_BATCH::FAIL editeur bloque au-dela de $budget s" }

# Verdict par travail, sur la tranche du log entre son debut et sa fin.
$lines = if (Test-Path $log) { @(Get-Content $log -Encoding UTF8) } else { @() }
$failed = 0
foreach ($n in $names) {
  $proof = $registry[$n]
  $b = -1; $e = -1
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($b -lt 0 -and $lines[$i] -match "EDITOR_BATCH_JOB_BEGIN $([regex]::Escape($n))$") { $b = $i }
    elseif ($b -ge 0 -and $lines[$i] -match "EDITOR_BATCH_JOB_END $([regex]::Escape($n)) ") { $e = $i; break }
  }
  if ($b -lt 0) { $failed++; Write-Output "PROOF::FAIL $n jamais demarree (editeur ferme ou porte memoire ?)"; continue }
  if ($e -lt 0) { $failed++; Write-Output "PROOF::FAIL $n jamais terminee (editeur ferme en cours ?)"; continue }
  $slice = $lines[$b..$e]
  $end = $lines[$e]
  $secs = if ($end -match 'seconds=([0-9.]+)') { $Matches[1] } else { '?' }
  $badLine = if ($proof.Fail) { @($slice | Where-Object { $_ -match $proof.Fail }) | Select-Object -First 1 } else { $null }
  if ($end -notmatch 'reason=quit') { $failed++; Write-Output "PROOF::FAIL $n $(($end -split 'reason=')[1])" }
  elseif ($badLine) { $failed++; Write-Output ("PROOF::FAIL $n " + ($badLine -replace '^\[[^\]]*\]\[[ 0-9]*\]', '')) }
  elseif (-not (@($slice | Where-Object { $_ -match $proof.Pass }).Count)) { $failed++; Write-Output "PROOF::FAIL $n motif de reussite absent ($($proof.Pass))" }
  else { Write-Output "PROOF::PASS $n (${secs}s)" }
}
$ok = $names.Count - $failed
Write-Output "LOG::$log"
if ($failed -gt 0) { Write-Output "EDITOR_BATCH::FAIL $ok/$($names.Count)"; exit 1 }
Write-Output "EDITOR_BATCH::PASS $ok/$($names.Count)"
