# Rapport de sante ANÁSTASIS. N'invente aucun PASS : une preuve absente ou
# perimee est UNKNOWN / STALE, jamais PASS.
#
# Usage : tools\unreal\anastasis-unreal.ps1 health
#         (ou ce fichier directement)
#
# Exit : 0 GREEN|YELLOW, 1 RED, 2 le script lui-meme a echoue.
$ErrorActionPreference = 'Stop'
$Canonical = 'C:\dev\ANASTASIS_UNREAL'
$WorktreeRoot = 'C:\dev\ANASTASIS_WORKTREES'
$Root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$isCanonical = ($Root -eq $Canonical)
$isWorktree = $Root.StartsWith($WorktreeRoot + '\', [StringComparison]::OrdinalIgnoreCase)
if (-not ($isCanonical -or $isWorktree)) {
  throw "FAIL: health must run from $Canonical or a worktree under $WorktreeRoot (got $Root)"
}
Set-Location -LiteralPath $Root
$Project = Join-Path $Root 'Anastasis_UnrealV2.uproject'
$Engine = 'C:\Program Files\Epic Games\UE_5.8'
$Evidence = Join-Path $Root 'Saved\CanonicalVerification'
$KnownFailFile = Join-Path $PSScriptRoot 'known-expected-failures.txt'
$KnownLogFile = Join-Path $PSScriptRoot 'known-log-patterns.txt'

function Fingerprint {
  $paths = @(Get-ChildItem "$Root\Source", "$Root\Config" -Recurse -File) + @(Get-Item $Project)
  $lines = $paths | Sort-Object FullName | ForEach-Object {
    $_.FullName.Substring($Root.Length) + ':' + (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
  }
  $sha = [Security.Cryptography.SHA256]::Create()
  try {
    return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($lines -join "`n")))).Replace('-', '')
  } finally { $sha.Dispose() }
}

function Load-KnownMap($path) {
  $map = @{}
  if (-not (Test-Path $path)) { return $map }
  Get-Content $path | ForEach-Object {
    $line = $_.Trim()
    if ($line -and -not $line.StartsWith('#')) {
      $parts = $line.Split('|')
      if ($parts.Count -ge 1) { $map[$parts[0].Trim()] = $true }
    }
  }
  return $map
}

function Dim($status, $detail) {
  return [PSCustomObject]@{ Status = $status; Detail = $detail }
}

$errors = @()
$currentFp = Fingerprint
$head = ((& git -C $Root rev-parse HEAD) | Out-String).Trim()
$branch = ((& git -C $Root branch --show-current) | Out-String).Trim()
$dirtyTracked = @(& git -C $Root status --porcelain --untracked-files=no)
$untracked = @(& git -C $Root status --porcelain --untracked-files=all | Where-Object { $_.StartsWith('??') })
$worktreeState = 'CLEAN'
if ($dirtyTracked.Count -gt 0) { $worktreeState = 'DIRTY' }
elseif ($untracked.Count -gt 0) { $worktreeState = 'CLEAN+UNTRACKED' }

# --- PROJECT FILE ---
$projectStatus = Dim 'FAIL' 'uproject missing'
if (Test-Path $Project) {
  try {
    $up = Get-Content $Project -Raw | ConvertFrom-Json
    $v = Get-Content "$Engine\Engine\Build\Build.version" -Raw | ConvertFrom-Json
    $need = @('AnastasisSim', 'Anastasis_UnrealV2') | Where-Object { $_ -notin @($up.Modules.Name) }
    if ($up.EngineAssociation -ne '5.8') { $projectStatus = Dim 'FAIL' "EngineAssociation=$($up.EngineAssociation)" }
    elseif ($v.MajorVersion -ne 5 -or $v.MinorVersion -ne 8 -or $v.PatchVersion -ne 2 -or $v.Changelist -ne 56702186) {
      $projectStatus = Dim 'FAIL' "engine $($v.MajorVersion).$($v.MinorVersion).$($v.PatchVersion) CL $($v.Changelist)"
    }
    elseif ($need.Count -gt 0) { $projectStatus = Dim 'FAIL' ("missing " + ($need -join ',')) }
    else { $projectStatus = Dim 'PASS' 'Anastasis_UnrealV2.uproject + UE 5.8.2 CL 56702186' }
  } catch {
    $projectStatus = Dim 'FAIL' $_.Exception.Message
  }
}

# --- C++ / EDITOR TARGET ---
$builtShaPath = Join-Path $Evidence 'built-source.sha256'
$buildLog = Join-Path $Evidence 'build.log'
$cppStatus = Dim 'UNKNOWN' 'no built-source.sha256'
$editorTargetStatus = Dim 'UNKNOWN' 'no Editor target evidence'
if (Test-Path $builtShaPath) {
  $builtSha = (Get-Content $builtShaPath -Raw).Trim()
  # sha is written only after BUILD::PASS. Do not require build.log text: verify
  # re-tees that log, so a concurrent health read would falsely report UNKNOWN.
  if ($builtSha -eq $currentFp) {
    $cppStatus = Dim 'PASS' 'fingerprint matches built-source.sha256'
    $editorTargetStatus = Dim 'PASS' 'Anastasis_UnrealV2Editor Win64 Development'
  } else {
    $cppStatus = Dim 'STALE' 'source/config changed since last BUILD::PASS'
    $editorTargetStatus = Dim 'STALE' 'Editor target evidence is for a previous fingerprint'
  }
} elseif (Test-Path $buildLog) {
  $fail = Select-String -Path $buildLog -Pattern 'BUILD::FAIL|Result: Failed' -Quiet
  if ($fail) {
    $cppStatus = Dim 'FAIL' 'build.log reports failure'
    $editorTargetStatus = Dim 'FAIL' 'Editor target failed'
  }
}

# --- GAME TARGET ---
$gameExe = Join-Path $Root 'Binaries\Win64\Anastasis_UnrealV2.exe'
$gameShaPath = Join-Path $Evidence 'built-game-source.sha256'
$gameLog = Join-Path $Evidence 'build-game.log'
$gameStatus = Dim 'UNKNOWN' 'no Game target evidence (operator build is Editor-only)'
if (Test-Path $gameShaPath) {
  $gameSha = (Get-Content $gameShaPath -Raw).Trim()
  if ($gameSha -eq $currentFp) { $gameStatus = Dim 'PASS' 'Anastasis_UnrealV2 Win64 Development' }
  else { $gameStatus = Dim 'STALE' 'source changed since last GAME_BUILD::PASS' }
} elseif (Test-Path $gameExe) {
  $gameStatus = Dim 'STALE' 'Anastasis_UnrealV2.exe exists but has no fingerprint (not produced by build-game)'
} elseif (Test-Path $gameLog) {
  if (Select-String -Path $gameLog -Pattern 'Result: Failed|GAME_BUILD::FAIL' -Quiet) {
    $gameStatus = Dim 'FAIL' 'build-game.log reports failure'
  }
}

# --- EDITOR BOOT / MAP / MODULES ---
$latestPath = Join-Path $Evidence 'latest.json'
$bootStatus = Dim 'UNKNOWN' 'no latest.json'
$mapStatus = Dim 'UNKNOWN' 'no verify evidence'
$moduleStatus = Dim 'UNKNOWN' 'no verify evidence'
$editorLogPath = $null
if (Test-Path $latestPath) {
  $latest = Get-Content $latestPath -Raw | ConvertFrom-Json
  $editorLogPath = [string]$latest.EditorLog
  $shaMatch = ([string]$latest.SourceSHA256 -eq $currentFp)
  if ([string]$latest.Result -eq 'PASS' -and $shaMatch) {
    $bootStatus = Dim 'PASS' ([string]$latest.VerifiedAt)
    $mapStatus = Dim 'PASS' '/Game/FirstPerson/Lvl_FirstPerson (verify CANONICAL_MAP_LOAD)'
    $mods = @($latest.Modules)
    $badOrigin = @($mods | Where-Object { -not ([string]$_.Path).StartsWith($Root + '\', [StringComparison]::OrdinalIgnoreCase) })
    if ($mods.Count -eq 2 -and $badOrigin.Count -eq 0) {
      $moduleStatus = Dim 'PASS' 'AnastasisSim + Anastasis_UnrealV2 from this root'
    } else {
      $moduleStatus = Dim 'FAIL' "count=$($mods.Count) badOrigin=$($badOrigin.Count)"
    }
  } elseif ([string]$latest.Result -eq 'PASS' -and -not $shaMatch) {
    $bootStatus = Dim 'STALE' 'latest.json PASS is for a previous fingerprint'
    $mapStatus = Dim 'STALE' 'map load evidence is for a previous fingerprint'
    $moduleStatus = Dim 'STALE' 'module load evidence is for a previous fingerprint'
  } else {
    $bootStatus = Dim 'FAIL' "latest.json Result=$($latest.Result)"
    $mapStatus = Dim 'FAIL' 'verify did not pass'
    $moduleStatus = Dim 'FAIL' 'verify did not pass'
  }
}
$startupMap = Join-Path $Root 'Content\FirstPerson\Lvl_FirstPerson.umap'
$sliceMap = Join-Path $Root 'Content\Anastasis\Maps\Lvl_AnastasisSlice.umap'
$mapExists = (Test-Path $startupMap)
if (-not $mapExists) {
  $mapStatus = Dim 'FAIL' 'Content/FirstPerson/Lvl_FirstPerson.umap missing'
}

# --- AUTOMATION ---
$reportLog = Join-Path $Evidence 'report-tests.log'
$autoStatus = Dim 'UNKNOWN' 'no report-tests.log'
$passN = 0; $kefN = 0; $failN = 0; $totalN = 0
$knownFails = Load-KnownMap $KnownFailFile
if (Test-Path $reportLog) {
  $pass = @(); $expected = @(); $fail = @(); $broken = @()
  Select-String -Path $reportLog -Pattern 'Test Completed\. Result=\{(\w+)\}.*Path=\{([^}]+)\}' | ForEach-Object {
    $result = $_.Matches[0].Groups[1].Value
    $path = $_.Matches[0].Groups[2].Value
    if ($knownFails.ContainsKey($path)) {
      if ($result -eq 'Success') { $expected += $path } else { $broken += $path }
    } elseif ($result -eq 'Success') { $pass += $path }
    else { $fail += $path }
  }
  $passN = $pass.Count; $kefN = $expected.Count; $failN = $fail.Count + $broken.Count
  $totalN = $passN + $kefN + $failN
  $logTime = (Get-Item $reportLog).LastWriteTimeUtc
  $sourceNewer = @(Get-ChildItem "$Root\Source" -Recurse -File | Where-Object { $_.LastWriteTimeUtc -gt $logTime })
  if ($totalN -eq 0) { $autoStatus = Dim 'UNKNOWN' 'report-tests.log has no Test Completed lines' }
  elseif ($failN -gt 0) { $autoStatus = Dim 'FAIL' "$passN PASS / $kefN KEF / $failN FAIL" }
  elseif ($sourceNewer.Count -gt 0) { $autoStatus = Dim 'STALE' "$passN PASS / $kefN KEF / 0 FAIL ; Source newer than log" }
  else { $autoStatus = Dim 'PASS' "$passN PASS / $kefN KEF / 0 FAIL (KEF never counted as PASS)" }
}

# --- LOGS ---
$knownErr = 0; $newErr = 0; $knownWarn = 0; $newWarn = 0; $critical = 0
$newErrLines = @(); $newWarnLines = @()
$knownLog = Load-KnownMap $KnownLogFile
$logStatus = Dim 'UNKNOWN' 'no editor log'
if ($editorLogPath -and (Test-Path $editorLogPath)) {
  Get-Content $editorLogPath | ForEach-Object {
    $line = $_
    $isErr = $line.Contains('Error:')
    $isWarn = $line.Contains('Warning:')
    $isCrit = $line.Contains('Fatal error') -or $line.Contains('Assertion failed') -or $line.Contains('Ensure condition failed')
    if ($isCrit) { $critical += 1 }
    if (-not ($isErr -or $isWarn)) { return }
    $known = $false
    foreach ($pat in $knownLog.Keys) { if ($line.Contains($pat)) { $known = $true; break } }
    if ($isErr) {
      if ($known) { $knownErr += 1 } else { $newErr += 1; if ($newErrLines.Count -lt 8) { $newErrLines += $line } }
    } elseif ($isWarn) {
      if ($known) { $knownWarn += 1 } else { $newWarn += 1; if ($newWarnLines.Count -lt 8) { $newWarnLines += $line } }
    }
  }
  $logStatus = Dim 'PASS' 'classified against known-log-patterns.txt'
  if ($critical -gt 0 -or $newErr -gt 0) { $logStatus = Dim 'FAIL' "critical=$critical newErrors=$newErr" }
  elseif ($newWarn -gt 0) { $logStatus = Dim 'YELLOW' "newWarnings=$newWarn" }
} elseif ($bootStatus.Status -eq 'STALE' -and $editorLogPath) {
  $logStatus = Dim 'STALE' 'editor log belongs to a previous fingerprint'
}

# --- CONFIG DRIFT ---
$drift = @()
$engineIni = Get-Content (Join-Path $Root 'Config\DefaultEngine.ini')
$editorIni = Get-Content (Join-Path $Root 'Config\DefaultEditor.ini')
$startup = ($engineIni | Where-Object { $_.StartsWith('EditorStartupMap=') } | Select-Object -First 1)
$gameMap = ($engineIni | Where-Object { $_.StartsWith('GameDefaultMap=') } | Select-Object -First 1)
$simple = ($editorIni | Where-Object { $_.StartsWith('SimpleMapName=') } | Select-Object -First 1)
if ($simple -match 'FirstPersonExampleMap') {
  $drift += 'DefaultEditor.ini SimpleMapName points at missing template map FirstPersonExampleMap (startup map is Lvl_FirstPerson)'
}
if (-not $mapExists) { $drift += 'canonical umap missing' }
if (-not (Test-Path $sliceMap)) { $drift += 'Lvl_AnastasisSlice.umap missing (observation map)' }
$driftStatus = 'NONE'
if ($drift.Count -gt 0) { $driftStatus = ($drift -join '; ') }

# --- NIGHTLY (canonical task, informational) ---
$nightly = 'UNKNOWN'
$nightlyBad = $false
try {
  $info = Get-ScheduledTaskInfo -TaskName 'Anastasis-ScheduledVerify' -ErrorAction Stop
  $nightly = "LastTaskResult=$($info.LastTaskResult) LastRun=$($info.LastRunTime) Next=$($info.NextRunTime)"
  if ($info.LastTaskResult -ne 0) { $nightlyBad = $true }
} catch {
  $nightly = 'UNKNOWN (task Anastasis-ScheduledVerify not readable)'
}

# --- VERDICT ---
$dims = @($projectStatus, $cppStatus, $editorTargetStatus, $bootStatus, $moduleStatus, $mapStatus, $autoStatus)
$hasFail = @($dims | Where-Object { $_.Status -eq 'FAIL' }).Count -gt 0 -or $critical -gt 0 -or $newErr -gt 0
$hasUnknown = @($dims | Where-Object { $_.Status -in @('UNKNOWN', 'STALE') }).Count -gt 0 -or $gameStatus.Status -in @('UNKNOWN', 'STALE', 'FAIL')
if ($hasFail -or $gameStatus.Status -eq 'FAIL') { $verdict = 'RED' }
elseif ($hasUnknown -or $newWarn -gt 0 -or $drift.Count -gt 0 -or $worktreeState -eq 'DIRTY' -or $nightlyBad) { $verdict = 'YELLOW' }
else { $verdict = 'GREEN' }

$role = 'CANONICAL_ROOT'
if ($isWorktree) { $role = 'AGENT_WORKTREE' }

$report = @"
ANASTASIS UNREAL - PROJECT HEALTH

ROLE             $role
ROOT             $Root
ENGINE           UE 5.8.2 CL 56702186
GIT BRANCH       $branch
GIT HEAD         $head
WORKTREE         $worktreeState
FINGERPRINT      $currentFp

PROJECT FILE     $($projectStatus.Status)  $($projectStatus.Detail)
C++ BUILD        $($cppStatus.Status)  $($cppStatus.Detail)
EDITOR TARGET    $($editorTargetStatus.Status)  $($editorTargetStatus.Detail)
GAME TARGET      $($gameStatus.Status)  $($gameStatus.Detail)
EDITOR BOOT      $($bootStatus.Status)  $($bootStatus.Detail)
MODULE LOAD      $($moduleStatus.Status)  $($moduleStatus.Detail)
CANON MAP        $($mapStatus.Status)  $($mapStatus.Detail)

AUTOMATION       $($autoStatus.Status)  $($autoStatus.Detail)

LOG ERRORS
Known            $knownErr
New              $newErr

LOG WARNINGS
Known            $knownWarn
New              $newWarn

CRITICAL LOG     $critical
CONFIG DRIFT     $driftStatus
NIGHTLY TASK     $nightly
STARTUP MAP      $startup
GAME DEFAULT MAP $gameMap
EDITOR SIMPLEMAP $simple
SLICE MAP EXISTS $(Test-Path $sliceMap)

VERDICT          $verdict
"@

if ($newErrLines.Count -gt 0) {
  $report += "`nNEW ERRORS`n" + (($newErrLines | ForEach-Object { '    ' + $_ }) -join "`n") + "`n"
}
if ($newWarnLines.Count -gt 0) {
  $report += "`nNEW WARNINGS`n" + (($newWarnLines | ForEach-Object { '    ' + $_ }) -join "`n") + "`n"
}

Write-Output $report
New-Item -ItemType Directory -Force $Evidence | Out-Null
Set-Content -Path (Join-Path $Evidence 'health.txt') -Value $report -Encoding utf8

if ($verdict -eq 'RED') { exit 1 }
exit 0
