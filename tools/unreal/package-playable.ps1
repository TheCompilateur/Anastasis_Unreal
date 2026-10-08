param(
  [string]$ReleaseRoot = 'C:\dev\ANASTASIS_RELEASES\Playable',
  [ValidateRange(0, 180)][int]$WaitMinutes = 45
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = 'C:\dev\ANASTASIS_UNREAL'
$Project = Join-Path $ProjectRoot 'Anastasis_UnrealV2.uproject'
$BuildOperator = Join-Path $ProjectRoot 'tools\unreal\anastasis-unreal.ps1'
$Engine = 'C:\Program Files\Epic Games\UE_5.8'
$RunUAT = Join-Path $Engine 'Engine\Build\BatchFiles\RunUAT.bat'
$MainLock = 'C:\dev\ANASTASIS_WORKTREES\.handoff\MAIN.lock'
$ReleaseRoot = [IO.Path]::GetFullPath($ReleaseRoot).TrimEnd('\')
$ReleaseParent = 'C:\dev\ANASTASIS_RELEASES\'
if (-not $ReleaseRoot.StartsWith($ReleaseParent, [StringComparison]::OrdinalIgnoreCase)) {
  throw 'PACKAGE::FAIL ReleaseRoot doit etre sous C:\dev\ANASTASIS_RELEASES'
}
. (Join-Path $PSScriptRoot 'editor-launch.ps1')

function Fail([string]$Message) { throw "PACKAGE::FAIL $Message" }
function GitHead {
  $value = (& git -C $ProjectRoot rev-parse --verify HEAD 2>&1)
  if ($LASTEXITCODE -ne 0) { Fail "git HEAD: $value" }
  return ($value | Select-Object -First 1).Trim()
}
function Assert-CanonicalState([string]$ExpectedHead) {
  $branch = (& git -C $ProjectRoot branch --show-current 2>&1)
  if ($LASTEXITCODE -ne 0 -or $branch -ne 'main') { Fail 'la racine canonique doit etre sur main' }
  $dirty = @(& git -C $ProjectRoot status --porcelain --untracked-files=no 2>&1)
  if ($LASTEXITCODE -ne 0 -or $dirty.Count -gt 0) { Fail 'fichiers suivis modifies dans la racine canonique' }
  if (Test-Path -LiteralPath $MainLock) { Fail 'MAIN_LOCK tenu ou stale : attendre l integrateur' }
  if ((GitHead) -ne $ExpectedHead) { Fail 'main a bouge pendant la creation du paquet' }
}
function Wait-ForMainUnlock {
  $deadline = (Get-Date).AddMinutes($WaitMinutes)
  $lastReport = [datetime]::MinValue
  while ($true) {
    $locked = Test-Path -LiteralPath $MainLock
    if (-not $locked) { return }
    if ((Get-Date) -ge $deadline) { Fail 'MAIN_LOCK encore tenu ; relancer plus tard' }
    if ((Get-Date) -ge $lastReport.AddMinutes(1)) {
      Write-Output 'PACKAGE::WAIT MAIN_LOCK'
      $lastReport = Get-Date
    }
    Start-Sleep -Seconds 20
  }
}
function Write-Latest([string]$Head, [string]$ExeRelative) {
  $pointer = Join-Path $ReleaseRoot 'latest.json'
  $temp = Join-Path $ReleaseRoot ('.latest-' + [guid]::NewGuid().ToString('N') + '.json')
  [pscustomobject]@{
    commit = $Head
    exe = ('releases/' + $Head + '/' + $ExeRelative.Replace('\', '/'))
    publishedUtc = (Get-Date).ToUniversalTime().ToString('o')
  } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $temp -Encoding UTF8
  if (Test-Path -LiteralPath $pointer) {
    [IO.File]::Replace($temp, $pointer, $null)
  } else {
    [IO.File]::Move($temp, $pointer)
  }
}
function Publish-Staging([string]$Head, [string]$Staging, [string]$GameMap) {
  $releaseDir = Join-Path (Join-Path $ReleaseRoot 'releases') $Head
  $exeRelative = 'Windows\Anastasis_UnrealV2.exe'
  $launcher = Join-Path $Staging $exeRelative
  $inner = Join-Path $Staging 'Windows\Anastasis_UnrealV2\Binaries\Win64\Anastasis_UnrealV2.exe'
  if (-not (Test-Path -LiteralPath $launcher) -or -not (Test-Path -LiteralPath $inner)) {
    Fail "lanceur ou binaire interne absent : $Staging"
  }
  $cooked = @(Get-ChildItem -LiteralPath $Staging -File -Recurse |
    Where-Object { $_.Extension -in @('.pak', '.ucas') })
  if ($cooked.Count -eq 0) { Fail "aucune donnee cuite dans $Staging" }
  Assert-CanonicalState $Head
  [pscustomobject]@{
    commit = $Head
    engine = '5.8.2 CL 56702186'
    configuration = 'Win64 Development'
    gameMap = $GameMap
    exeRelative = $exeRelative
    packagedUtc = (Get-Date).ToUniversalTime().ToString('o')
  } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $Staging 'release.json') -Encoding UTF8
  Move-Item -LiteralPath $Staging -Destination $releaseDir
  Assert-CanonicalState $Head
  Write-Latest $Head $exeRelative
  Write-Output "PACKAGE::PASS $Head"
  Write-Output "GAME_EXE::$(Join-Path $releaseDir $exeRelative)"
  Write-Output "LATEST::$(Join-Path $ReleaseRoot 'latest.json')"
}

if (-not (Test-Path -LiteralPath $Project) -or -not (Test-Path -LiteralPath $RunUAT) -or
    -not (Test-Path -LiteralPath $BuildOperator)) {
  Fail 'projet canonique, operateur de build ou RunUAT absent'
}
$mutex = New-Object System.Threading.Mutex($false, 'Global\AnastasisPlayablePackaging')
$held = $false
try {
  try { $held = $mutex.WaitOne(0) } catch [System.Threading.AbandonedMutexException] { $held = $true }
  if (-not $held) { Fail 'un empaquetage est deja en cours' }
  Wait-ForMainUnlock
  $head = GitHead
  Assert-CanonicalState $head
  $association = (Get-Content -LiteralPath $Project -Raw | ConvertFrom-Json).EngineAssociation
  $version = Get-Content -LiteralPath (Join-Path $Engine 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
  if ($association -ne '5.8' -or $version.MajorVersion -ne 5 -or $version.MinorVersion -ne 8 -or
      $version.PatchVersion -ne 2 -or $version.Changelist -ne 56702186) { Fail 'identite moteur inattendue' }
  $mapLines = @(Get-Content -LiteralPath (Join-Path $ProjectRoot 'Config\DefaultEngine.ini') |
    Where-Object { $_ -match '^GameDefaultMap=' })
  if ($mapLines.Count -ne 1) { Fail 'GameDefaultMap absent ou ambigu' }
  $mapMatch = [regex]::Match($mapLines[0], '^GameDefaultMap=(/Game/[^.]+)')
  if (-not $mapMatch.Success) { Fail 'GameDefaultMap invalide' }
  $gameMap = $mapMatch.Groups[1].Value
  $mapFile = Join-Path $ProjectRoot ('Content\' + $gameMap.Substring(6).Replace('/', '\') + '.umap')
  if (-not (Test-Path -LiteralPath $mapFile)) { Fail "carte de jeu absente : $mapFile" }
  $releaseDir = Join-Path (Join-Path $ReleaseRoot 'releases') $head
  $manifest = Join-Path $releaseDir 'release.json'
  if (Test-Path -LiteralPath $manifest) {
    $existing = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    $existingExe = Join-Path $releaseDir $existing.exeRelative
    if ($existing.commit -eq $head -and (Test-Path -LiteralPath $existingExe)) {
      Write-Latest $head $existing.exeRelative
      Write-Output "PACKAGE::REUSED $head"
      Write-Output "GAME_EXE::$existingExe"
      exit 0
    }
    Fail "release existante incomplete : $releaseDir"
  }

  $stagingRoot = Join-Path $ReleaseRoot 'staging'
  if (Test-Path -LiteralPath $stagingRoot) {
    $recoverable = @(Get-ChildItem -LiteralPath $stagingRoot -Directory -Filter "$head-*" |
      Sort-Object LastWriteTime -Descending | Where-Object {
        $candidateLog = Join-Path $_.FullName 'BuildCookRun.log'
        (Test-Path -LiteralPath $candidateLog) -and
        ((Get-Content -LiteralPath $candidateLog -Tail 8) -match 'AutomationTool exiting with ExitCode=0 \(Success\)').Count -gt 0
      })
    if ($recoverable.Count -gt 0) {
      Write-Output "PACKAGE::RECOVER $($recoverable[0].FullName)"
      Publish-Staging $head $recoverable[0].FullName $gameMap
      exit 0
    }
  }

  New-Item -ItemType Directory -Force -Path (Join-Path $ReleaseRoot 'releases') | Out-Null
  New-Item -ItemType Directory -Force -Path $stagingRoot | Out-Null
  $staging = Join-Path $stagingRoot ($head + '-' + [guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Path $staging | Out-Null
  $log = Join-Path $staging 'BuildCookRun.log'
  $editorBuildLog = Join-Path $staging 'EditorBuild.log'
  $gameBuildLog = Join-Path $staging 'GameBuild.log'
  $uatArgs = @(
    'BuildCookRun', "-project=$Project", '-noP4', '-platform=Win64',
    '-clientconfig=Development', '-skipbuild', '-cook', '-cookall', "-map=$gameMap", '-stage',
    '-pak', '-archive', "-archivedirectory=$staging", '-unattended', '-utf8output'
  )
  Write-Output "PACKAGE::BUILD $head"
  Write-Output "PACKAGE::MAP $gameMap"
  Write-Output "PACKAGE::LOG $log"
  $uatCode = Invoke-AnastasisEditorGated -MaxEditors 1 -TimeoutMinutes $WaitMinutes -Launch {
    Assert-CanonicalState $head
    & $BuildOperator build *> $editorBuildLog
    if ($LASTEXITCODE -ne 0) { Fail "Editor build code $LASTEXITCODE ; log : $editorBuildLog" }
    & $BuildOperator build-game *> $gameBuildLog
    if ($LASTEXITCODE -ne 0) { Fail "Game build code $LASTEXITCODE ; log : $gameBuildLog" }
    Assert-CanonicalState $head
    & $RunUAT @uatArgs *> $log
    return $LASTEXITCODE
  }
  if ($uatCode -ne 0) { Fail "BuildCookRun code $uatCode ; log : $log" }

  Publish-Staging $head $staging $gameMap
} finally {
  if ($held) { $mutex.ReleaseMutex() }
  $mutex.Dispose()
}
