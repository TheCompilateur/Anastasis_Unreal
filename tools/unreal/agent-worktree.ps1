# Protocole multi-agent ANASTASIS. Git worktree + quelques regles, rien de plus.
#
# Doctrine :
#   CANONICAL_MAIN  C:\dev\ANASTASIS_UNREAL  -- integration seulement
#   AGENT_WORK      branche agent/<mission> + worktree dedie
#   COMMIT          unite de passation
#   INTEGRATOR      seul ecrivain pendant l'integration
#   VERIFY/SEAL     exige une racine canonique quiescente
#
# Usage :
#   agent-worktree.ps1 create     -Mission world-slice-007
#   agent-worktree.ps1 status
#   agent-worktree.ps1 finish     -Mission world-slice-007
#   agent-worktree.ps1 integrate  -Mission world-slice-007
#   agent-worktree.ps1 preflight
#   agent-worktree.ps1 postflight
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('create', 'status', 'finish', 'integrate', 'preflight', 'postflight')]
  [string]$Command,
  [string]$Mission,
  [string]$From = 'main'
)
$ErrorActionPreference = 'Stop'

$Canonical = 'C:\dev\ANASTASIS_UNREAL'
$WorktreeRoot = 'C:\dev\ANASTASIS_WORKTREES'
$QuiescenceFile = Join-Path $Canonical 'Saved\CanonicalVerification\quiescence.json'

function Fail($msg) { Write-Output $msg; exit 1 }

function Require-Mission {
  if (-not $Mission) { Fail 'FAIL: -Mission est requis' }
  if ($Mission -notmatch '^[a-z0-9][a-z0-9._-]*$') {
    Fail "FAIL: nom de mission invalide '$Mission' (minuscules, chiffres, . _ -)"
  }
}

function Branch-Of($m) { return "agent/$m" }
function Path-Of($m) { return (Join-Path $WorktreeRoot $m) }

# Fichiers dont une modification pendant une fenetre de verify invalide la preuve.
function Source-Fingerprint {
  $paths = @()
  foreach ($d in @('Source', 'Config', 'Content', 'tools')) {
    $full = Join-Path $Canonical $d
    if (Test-Path $full) {
      $paths += Get-ChildItem $full -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '\\__pycache__\\' }
    }
  }
  $paths += Get-Item (Join-Path $Canonical 'Anastasis_UnrealV2.uproject')
  $lines = $paths | Sort-Object FullName | ForEach-Object {
    $_.FullName.Substring($Canonical.Length) + ':' + $_.Length + ':' + $_.LastWriteTimeUtc.Ticks
  }
  $sha = [Security.Cryptography.SHA256]::Create()
  try {
    return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($lines -join "`n")))).Replace('-', '')
  } finally { $sha.Dispose() }
}

function Canonical-State {
  $head = (& git -C $Canonical rev-parse HEAD).Trim()
  $dirty = @(& git -C $Canonical status --porcelain --untracked-files=no)
  $untracked = @(& git -C $Canonical status --porcelain --untracked-files=all | Where-Object { $_.StartsWith('??') })
  return [PSCustomObject]@{
    Head        = $head
    Dirty       = $dirty
    Untracked   = $untracked
    Fingerprint = (Source-Fingerprint)
  }
}

switch ($Command) {

  'create' {
    Require-Mission
    $branch = Branch-Of $Mission
    $path = Path-Of $Mission
    if (Test-Path $path) { Fail "FAIL: le worktree existe deja -> $path" }
    $exists = & git -C $Canonical rev-parse --verify --quiet $branch
    New-Item -ItemType Directory -Force $WorktreeRoot | Out-Null
    if ($exists) {
      Write-Output "NOTE: la branche $branch existe deja, reprise en l'etat"
      & git -C $Canonical worktree add $path $branch
    } else {
      & git -C $Canonical worktree add -b $branch $path $From
    }
    if ($LASTEXITCODE -ne 0) { Fail 'FAIL: git worktree add' }
    Write-Output ''
    Write-Output "WORKTREE_CREATED::$path"
    Write-Output "BRANCH::$branch"
    Write-Output "BASE::$From"
    Write-Output ''
    Write-Output 'Developpe ici, jamais dans la racine canonique. Premier build :'
    Write-Output "  cd `"$path`""
    Write-Output '  tools\unreal\anastasis-unreal.ps1 build'
  }

  'status' {
    Write-Output "CANONICAL::$Canonical"
    $rows = @()
    $mainHead = (& git -C $Canonical rev-parse main).Trim()
    foreach ($line in (& git -C $Canonical worktree list --porcelain) -split "`n") {
      if ($line.StartsWith('worktree ')) { $wt = $line.Substring(9).Trim().Replace('/', '\') }
      elseif ($line.StartsWith('branch ')) {
        $br = $line.Substring(7).Trim() -replace '^refs/heads/', ''
        $dirty = @(& git -C $wt status --porcelain --untracked-files=all)
        $ab = (& git -C $Canonical rev-list --left-right --count "main...$br").Trim() -split '\s+'
        $rows += [PSCustomObject]@{
          Mission  = Split-Path $wt -Leaf
          Branch   = $br
          Modifies = $dirty.Count
          Behind   = $ab[0]
          Ahead    = $ab[1]
          Path     = $wt
        }
      }
    }
    $rows | Format-Table -AutoSize
    Write-Output "MAIN_HEAD::$mainHead"
    $unmerged = @(& git -C $Canonical branch --no-merged main --list 'agent/*')
    if ($unmerged.Count -gt 0) {
      Write-Output 'BRANCHES_NON_INTEGREES::'
      $unmerged | ForEach-Object { Write-Output ('    ' + $_.Trim()) }
    } else {
      Write-Output 'BRANCHES_NON_INTEGREES::aucune'
    }
  }

  'finish' {
    Require-Mission
    $path = Path-Of $Mission
    if (-not (Test-Path $path)) { Fail "FAIL: worktree introuvable -> $path" }
    Write-Output "=== Portail de fin de mission : $Mission ==="
    & (Join-Path $path 'tools\unreal\anastasis-unreal.ps1') build
    if ($LASTEXITCODE -ne 0) { Fail 'FAIL: build' }
    & (Join-Path $path 'tools\unreal\report-tests.ps1')
    if ($LASTEXITCODE -ne 0) { Fail 'FAIL: des tests sont en echec reel' }
    $dirty = @(& git -C $path status --porcelain --untracked-files=all)
    Write-Output ''
    if ($dirty.Count -gt 0) {
      Write-Output 'RESTE_A_COMMITER::'
      $dirty | ForEach-Object { Write-Output ('    ' + $_) }
      Write-Output ''
      Write-Output 'Un commit est l unite de passation : commit tout avant de passer la main.'
      exit 1
    }
    Write-Output 'HANDOFF_READY::YES'
    Write-Output "Passation : tools\unreal\agent-worktree.ps1 integrate -Mission $Mission"
  }

  'integrate' {
    Require-Mission
    $branch = Branch-Of $Mission
    $state = Canonical-State
    if ($state.Dirty.Count -gt 0) {
      Write-Output 'FAIL: la racine canonique porte des modifications non commitees.'
      Write-Output 'L integrateur est le seul ecrivain : fais atterrir ou range ce travail d abord.'
      $state.Dirty | ForEach-Object { Write-Output ('    ' + $_) }
      exit 1
    }
    & git -C $Canonical merge --ff-only $branch
    if ($LASTEXITCODE -ne 0) {
      Write-Output "FAIL: pas d avance rapide possible. Rebase la branche sur main dans son worktree :"
      Write-Output "    cd `"$(Path-Of $Mission)`"; git rebase main"
      exit 1
    }
    Write-Output "INTEGRATED::$branch"
    Write-Output ("MAIN_HEAD::" + (& git -C $Canonical rev-parse --short HEAD).Trim())
  }

  'preflight' {
    $state = Canonical-State
    Write-Output "CANONICAL_HEAD::$($state.Head)"
    $ok = $true
    if ($state.Dirty.Count -gt 0) {
      $ok = $false
      Write-Output 'MUTATION_NON_INTEGREE::OUI'
      $state.Dirty | ForEach-Object { Write-Output ('    ' + $_) }
    } else {
      Write-Output 'MUTATION_NON_INTEGREE::NON'
    }
    if ($state.Untracked.Count -gt 0) {
      Write-Output "NON_SUIVIS::$($state.Untracked.Count) (a classer avant un seal)"
      $state.Untracked | ForEach-Object { Write-Output ('    ' + $_) }
    } else {
      Write-Output 'NON_SUIVIS::aucun'
    }
    $unmerged = @(& git -C $Canonical branch --no-merged main --list 'agent/*')
    if ($unmerged.Count -gt 0) {
      Write-Output 'BRANCHES_AGENT_NON_INTEGREES::'
      $unmerged | ForEach-Object { Write-Output ('    ' + $_.Trim()) }
    } else {
      Write-Output 'BRANCHES_AGENT_NON_INTEGREES::aucune'
    }
    New-Item -ItemType Directory -Force (Split-Path $QuiescenceFile) | Out-Null
    [PSCustomObject]@{
      OpenedAt    = (Get-Date -Format 'o')
      Head        = $state.Head
      Fingerprint = $state.Fingerprint
    } | ConvertTo-Json | Set-Content $QuiescenceFile -Encoding utf8
    Write-Output "FENETRE_OUVERTE::$QuiescenceFile"
    if ($ok) { Write-Output 'PREFLIGHT::PASS' } else { Write-Output 'PREFLIGHT::FAIL'; exit 1 }
  }

  'postflight' {
    if (-not (Test-Path $QuiescenceFile)) { Fail 'FAIL: aucune fenetre ouverte, lance preflight d abord' }
    $opened = Get-Content $QuiescenceFile -Raw | ConvertFrom-Json
    $state = Canonical-State
    Write-Output "FENETRE_OUVERTE_A::$($opened.OpenedAt)"
    $drift = $false
    if ($state.Head -ne $opened.Head) {
      $drift = $true
      Write-Output "HEAD_A_CHANGE::$($opened.Head) -> $($state.Head)"
    }
    if ($state.Fingerprint -ne $opened.Fingerprint) {
      $drift = $true
      Write-Output 'SOURCE_OU_CONFIG_A_CHANGE::OUI'
    }
    if ($drift) {
      Write-Output 'POSTFLIGHT::FAIL  la preuve de verify ne porte pas sur ce qui est sur le disque'
      exit 1
    }
    Write-Output 'POSTFLIGHT::PASS  aucune mutation pendant la fenetre'
  }
}
