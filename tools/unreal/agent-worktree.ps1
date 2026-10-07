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
#   agent-worktree.ps1 finish     -Mission world-slice-007          (build seul ; -Prove : + suite ici)
#   agent-worktree.ps1 integrate  -Mission world-slice-007          (mission deja prouvee seulement)
#
# RETEST_RULE_001 (2026-10-01) : une preuve se refait quand ce qu'elle juge a change, pas quand main a
# bouge. Si les arbres Unreal (Source/, Config/, Content/, Plugins/, .uproject) du commit que finish a
# prouve sont identiques a ceux de l'arbre rebase ou empile, ni build ni suite : RETEST::SKIP, la
# preuve du commit marque est reprise telle quelle -- ce n'est pas un nouveau PASS. Sinon, portail complet.
#   agent-worktree.ps1 integrate-batch -Missions mission-a,mission-b   (la voie normale : verrou de main,
#                                       un build, une suite, toutes les preuves PIE dans un editeur)
#
# EDITOR_QUEUE_001 (2026-10-01) : une file, un editeur. Les agents ne demarrent plus d'editeur pour
# se prouver ; l'integrateur rejoue suite et preuves de tout un lot. Voir AGENTS.md.
#   agent-worktree.ps1 preflight
#   agent-worktree.ps1 postflight
#   agent-worktree.ps1 mcp        -Mission world-slice-007
#   agent-worktree.ps1 prune      -Mission world-slice-007
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('create', 'status', 'finish', 'integrate', 'integrate-batch', 'preflight', 'postflight', 'mcp', 'prune')]
  [string]$Command,
  [string]$Mission,
  [string[]]$Missions,
  [string]$From = 'main',
  # finish / integrate-batch : build et tests Unreal meme sans changement Unreal.
  [switch]$Full,
  # integrate-batch : suite d'automation ciblee (1 a 30 cas) au lieu de toute la suite.
  # Le verdict est explicitement TARGETED_PASS, jamais une preuve de suite complete.
  [string]$TestFilter,
  # finish : lancer la suite ICI, dans un editeur a soi (l'ancien finish). Par defaut, la suite et
  # les preuves PIE attendent le lot (EDITOR_QUEUE_001) : un seul editeur pour tout le monde.
  [switch]$Prove
)
$ErrorActionPreference = 'Stop'
if ($TestFilter -and $Command -ne 'integrate-batch') {
  Write-Output 'FAIL: -TestFilter est reserve a integrate-batch'
  exit 1
}

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
function Handoff-Path($m) { return (Join-Path (Path-Of $m) "docs\unreal\handoffs\$m.md") }

# HANDOFF_READY, garde par mission : le commit sur lequel finish a passe. integrate-batch
# n'admet une branche que si son commit actuel est celui-la. Hors des depots (aucun git).
$HandoffDir = Join-Path $WorktreeRoot '.handoff'
function Handoff-Marker($m) { return (Join-Path $HandoffDir "$m.txt") }

# Le marqueur dit AUSSI ce que finish a prouve (EDITOR_QUEUE_001) : `<sha> <mode>`.
#   proved    build + suite passes dans le worktree (finish -Prove, ou -Full)
#   queued    build passe ; suite et preuves PIE attendent le lot (integrate-batch)
#   nounreal  rien que le build ou la suite puissent juger
# Un marqueur d'avant (sha seul) vaut `proved`.
function Read-HandoffMarker($m) {
  $file = Handoff-Marker $m
  if (-not (Test-Path -LiteralPath $file)) { return $null }
  $parts = @((Get-Content -LiteralPath $file -Raw).Trim() -split '\s+')
  return [PSCustomObject]@{ Sha = $parts[0]; Mode = $(if ($parts.Count -gt 1) { $parts[1] } else { 'proved' }) }
}
function Write-HandoffMarker($m, [string]$sha, [string]$mode) {
  New-Item -ItemType Directory -Force $HandoffDir | Out-Null
  "$sha $mode" | Set-Content (Handoff-Marker $m) -Encoding ascii
}

# Verrou de main (EDITOR_QUEUE_001). Un lot dure 20 a 40 min (porte memoire, build, suite, preuves) ;
# sans verrou, un `integrate` d'un autre agent avancait main pendant ce temps et le lot entier
# etait perdu (« main a bouge pendant le lot ») -- deux fois le 2026-10-01. Pris par integrate et
# integrate-batch, rendu a la fin ; un verrou dont le processus n'existe plus est repris.
$MainLock = Join-Path $HandoffDir 'MAIN.lock'
$script:LockHeld = $false
function Read-MainLock {
  if (-not (Test-Path -LiteralPath $MainLock)) { return $null }
  try { $l = Get-Content -LiteralPath $MainLock -Raw | ConvertFrom-Json } catch { return [PSCustomObject]@{ Holder = 'illisible'; Alive = $false } }
  $proc = if ($l.Pid) { Get-Process -Id $l.Pid -ErrorAction SilentlyContinue } else { $null }
  # Un pid recycle n'est pas le titulaire : son processus a demarre apres la prise du verrou.
  $alive = $proc -and (-not $proc.StartTime -or $proc.StartTime -le [datetime]$l.Since)
  return [PSCustomObject]@{ Holder = $l.Holder; Pid = $l.Pid; Since = $l.Since; Alive = [bool]$alive }
}
function Enter-MainLock([string]$holder) {
  New-Item -ItemType Directory -Force $HandoffDir | Out-Null
  for ($try = 0; $try -lt 2; $try++) {
    try {
      $fs = [IO.File]::Open($MainLock, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
      $bytes = [Text.Encoding]::UTF8.GetBytes(([PSCustomObject]@{ Holder = $holder; Pid = $PID; Since = (Get-Date -Format 'o') } | ConvertTo-Json -Compress))
      $fs.Write($bytes, 0, $bytes.Length)
      $fs.Close()
      $script:LockHeld = $true
      Write-Output "MAIN_LOCK::PRIS $holder"
      return
    } catch {
      $l = Read-MainLock
      if ($l -and $l.Alive) {
        Fail ("FAIL: MAIN_LOCK::TENU par $($l.Holder) (pid $($l.Pid), depuis $($l.Since)). Un versement est en cours : " +
          'main ne bougera pas sous lui. Attendre BATCH_INTEGRATED, ou confier la mission au lot suivant.')
      }
      Write-Output "NOTE: verrou de main perime ($(if ($l) { $l.Holder } else { '?' })) : repris"
      Remove-Item -LiteralPath $MainLock -Force -ErrorAction SilentlyContinue
    }
  }
  Fail 'FAIL: verrou de main impossible a prendre'
}
function Exit-MainLock {
  if ($script:LockHeld) { Remove-Item -LiteralPath $MainLock -Force -ErrorAction SilentlyContinue; $script:LockHeld = $false }
}

# Preuves PIE qu'une mission declare dans sa fiche : une ligne `PROOFS: a, b` (noms de
# tools/unreal/proofs.txt). `(aucune)`, `aucune` ou `-` : rien. Lue dans le commit de la branche.
function Get-DeclaredProofs([string]$repo, [string]$rev, [string]$m) {
  $text = (Invoke-Git -C $repo show "$($rev):docs/unreal/handoffs/$m.md").Out
  $names = @()
  foreach ($line in $text) {
    if ($line -match '^\s*PROOFS\s*:\s*(.+)$') {
      $names += @($Matches[1] -split ',' | ForEach-Object { $_.Trim().Trim('`') } |
        Where-Object { $_ -and $_ -notmatch '^\(?aucune\)?$' -and $_ -ne '-' })
    }
  }
  return @($names | Select-Object -Unique)
}
# RELAY_ADMISSION_001 : une mission de relais rejoue sur main les commits de missions dont l'agent
# est absent. Sa fiche les nomme (`RELAIS: a, b`) ; le lot ne les admet par elle que si toutes leurs
# preuves declarees figurent dans son propre `PROOFS:` -- la garde des missions heritees reste entiere.
function Get-DeclaredRelay([string]$repo, [string]$rev, [string]$m) {
  $text = (Invoke-Git -C $repo show "$($rev):docs/unreal/handoffs/$m.md").Out
  $names = @()
  foreach ($line in $text) {
    if ($line -match '^\s*RELAIS\s*:\s*(.+)$') {
      $names += @($Matches[1] -split ',' | ForEach-Object { $_.Trim().Trim('`') } | Where-Object { $_ -match '^[a-z0-9][a-z0-9._-]*$' })
    }
  }
  return @($names | Select-Object -Unique)
}
function Get-RegisteredProofs([string]$root) {
  $file = Join-Path $root 'tools\unreal\proofs.txt'
  if (-not (Test-Path $file)) { return @() }
  return @(Get-Content $file -Encoding UTF8 | Where-Object { $_.Trim() -and -not $_.TrimStart().StartsWith('#') } |
    ForEach-Object { ($_ -split '\s\|\s')[0].Trim() })
}
# proofs.txt se fusionne par union (.gitattributes, INTEGRATION_SPEED_001) : deux missions qui
# ajoutent chacune une ligne ne se heurtent plus. Le prix : une ligne modifiee des deux cotes reste
# en deux versions. Un nom inscrit deux fois se voit ici, au finish et au lot.
function Get-DuplicateProofs([string]$root) {
  return @(Get-RegisteredProofs $root | Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name })
}

# Un changement « Unreal » : ce que le build ou la suite d'automation peuvent juger. Une
# branche qui ne touche que docs/, tools/migration/ ou des scripts n'a rien a leur montrer,
# et leur passage coutait 15 a 20 min de porte memoire et d'editeur par mission
# (2026-10-01 : des agents bloques plus de 30 min a integrer).
function Test-UnrealPath([string]$p) {
  return ($p -match '^(Source|Config|Content|Plugins)/' -or $p -match '\.uproject$')
}

# git sans que PowerShell 5.1 ne transforme son stderr en erreur fatale : `fetch`, `merge`,
# `worktree` ecrivent leur progression sur stderr, et sous 'Stop' le script mourait en
# plein versement. Rend le code de sortie et toute la sortie, en texte.
function Invoke-Git {
  $ErrorActionPreference = 'Continue'
  $out = @(& git @args 2>&1 | ForEach-Object { "$_" })
  return [PSCustomObject]@{ Code = $LASTEXITCODE; Out = $out }
}

# Fichiers Unreal qui different entre deux arbres (`a..b` : arbre de a contre arbre de b). Sans
# detection de renommage : un Source/x deplace vers docs/x ne montrait que docs/x.
function Unreal-Changes([string]$repo, [string]$range) {
  return @((Invoke-Git -C $repo diff --no-renames --name-only $range).Out | Where-Object { $_ -and (Test-UnrealPath $_) })
}

# RETEST_RULE_001. Le commit $proved (marque par finish) et $rev ont-ils les memes arbres Unreal ?
# Faux si $proved n'existe plus dans le depot (rebase puis gc) : on ne reprend pas une preuve
# qu'on ne peut plus comparer.
function Test-SameUnrealTrees([string]$repo, [string]$proved, [string]$rev) {
  if (-not $proved) { return $false }
  if ((Invoke-Git -C $repo cat-file -e "$($proved)^{commit}").Code -ne 0) { return $false }
  return (@(Unreal-Changes $repo "$proved..$rev").Count -eq 0)
}
function Write-RetestSkip([string]$proved, [string]$mode, [string]$who) {
  Write-Output "RETEST::SKIP (arbres Unreal identiques a $($proved.Substring(0, 7))$who)"
  Write-Output "BUILD::SKIP  TESTS::SKIP  -- preuve reprise du commit $($proved.Substring(0, 7)) ($mode), rien n a ete rejoue : ce n est pas un nouveau PASS"
}

# INTEGRATION_SPEED_001. Le lot sans le lot : empile dans l'ordre, sur des commits flottants
# (`git merge-tree` + `commit-tree`, ni copie de travail ni verrou), les commits que chaque branche
# apporte par contenu -- exactement ce que fait le cherry-pick d'integrate-batch. Une mission en
# conflit est ecartee et ne compte pas pour la suivante, comme au lot. ~1 s par commit : le 2026-10-07,
# quatre missions annoncees pretes ont ete ecartees du lot pour conflit apres 25 min de portail.
function Get-StackPreview([string]$repo, [string]$base, [string[]]$missions) {
  $cur = (Invoke-Git -C $repo rev-parse $base).Out[0]
  $res = @()
  foreach ($m in $missions) {
    $commits = @((Invoke-Git -C $repo cherry $cur (Branch-Of $m)).Out | Where-Object { $_ -like '+ *' } | ForEach-Object { $_.Substring(2).Trim() })
    $tip = $cur
    $conflicts = @()
    foreach ($c in $commits) {
      $mt = Invoke-Git -C $repo merge-tree --write-tree --name-only --no-messages "--merge-base=$c^" $tip $c
      if ($mt.Code -ne 0) { $conflicts = @($mt.Out | Select-Object -Skip 1 | Where-Object { $_ } | Select-Object -Unique); break }
      $tip = (Invoke-Git -C $repo commit-tree $mt.Out[0] -p $tip -m preview).Out[0]
    }
    $new = $commits.Count
    if ($conflicts.Count -eq 0) {
      # Versee par un lot dont l'empilement a retouche le diff : `cherry` ne la reconnait plus, mais
      # rejouee elle ne change rien. Elle est dans main, par contenu.
      if ($new -gt 0 -and (Invoke-Git -C $repo rev-parse "$tip^{tree}").Out[0] -eq (Invoke-Git -C $repo rev-parse "$cur^{tree}").Out[0]) { $new = 0 }
      $cur = $tip
    }
    $res += [PSCustomObject]@{ Mission = $m; New = $new; Conflicts = $conflicts }
  }
  return $res
}

# Dossier du worktree d'integration : _integration par defaut. ANASTASIS_INTEGRATION_DIR le deplace
# quand ce chemin devient inscriptible (2026-10-02 : la machine refusait d'y recreer
# tools/unreal/editor-window-guard.ps1, meme dans un dossier neuf, sans processus ni regle identifies).
function Integration-Path {
  if ($env:ANASTASIS_INTEGRATION_DIR) { return [IO.Path]::GetFullPath($env:ANASTASIS_INTEGRATION_DIR).TrimEnd('\') }
  return (Join-Path $WorktreeRoot '_integration')
}
# Le worktree d'integration persistant, remis sur main (integrate-batch, et integrate quand il
# faut rebaser). Ses Binaries/ et Intermediate/ (ignores) survivent : build incremental.
# Appele sous le verrou de main seulement : un seul utilisateur a la fois.
function Reset-IntegrationTree([string]$integ, [string]$ib) {
  if (-not (Test-Path -LiteralPath $integ)) {
    $w = Invoke-Git -C $Canonical worktree add -B $ib $integ main
    if ($w.Code -ne 0) { $w.Out | ForEach-Object { Write-Output ('    ' + $_) }; Fail 'FAIL: worktree d integration' }
    return
  }
  $o = @(Find-WorktreeEditor $integ)
  if ($o.Count -gt 0) { Fail "FAIL: editeur ouvert sur $integ" }
  $null = Invoke-Git -C $integ cherry-pick --abort
  $c = Invoke-Git -C $integ checkout -q -f -B $ib main
  if ($c.Code -ne 0) { $c.Out | ForEach-Object { Write-Output ('    ' + $_) }; Fail 'FAIL: remise a zero du worktree d integration' }
  # Sans -x : Binaries/ et Intermediate/ (ignores) restent, le build reste incremental.
  $null = Invoke-Git -C $integ clean -fdq
}

# Le build et la suite d'un arbre, ou rien s'il n'y a pas de changement Unreal.
# Sans $runTests (finish par defaut) : le build seul, la suite attend le lot -- pas d'editeur.
# Ecrit son compte rendu et pose $script:GateOk et $script:GateMode (proved | queued | nounreal).
function Invoke-UnrealGate([string]$root, [string[]]$unreal, [bool]$runTests = $true) {
  $script:GateOk = $false
  $script:GateMode = 'nounreal'
  if ($unreal.Count -eq 0 -and -not $Full) {
    Write-Output 'UNREAL_CHANGE::NON (ni Source/, ni Config/, ni Content/, ni Plugins/, ni .uproject)'
    Write-Output 'BUILD::SKIP  TESTS::SKIP  -- rien a montrer au build ni a la suite (-Full pour les forcer)'
    $script:GateOk = $true
    return
  }
  if ($unreal.Count -gt 0) { Write-Output "UNREAL_CHANGE::OUI ($($unreal.Count) fichier(s), par ex. $($unreal[0]))" }
  & (Join-Path $root 'tools\unreal\anastasis-unreal.ps1') build
  if ($LASTEXITCODE -ne 0) { Write-Output 'FAIL: build'; return }
  if (-not $runTests) {
    Write-Output 'TESTS::QUEUED -- la suite et les preuves PIE tournent au lot (integrate-batch), dans UN editeur pour tout le lot'
    $script:GateMode = 'queued'
    $script:GateOk = $true
    return
  }
  if ($TestFilter) {
    Write-Output "TEST_SCOPE::TARGETED filter=$TestFilter max_cases=30"
    $testOut = @(& (Join-Path $root 'tools\unreal\report-tests.ps1') -Filter $TestFilter 2>&1 | ForEach-Object { "$_" })
    $testCode = $LASTEXITCODE
    $cases = $null
    foreach ($line in $testOut) {
      if ($line -match '^TOTAL\s*:\s*(\d+)') { $cases = [int]$Matches[1] }
      if ($line -notmatch '^TESTS::PASS') { Write-Output $line }
    }
    if ($testCode -ne 0) { Write-Output 'FAIL: des tests cibles sont en echec reel'; return }
    if ($null -eq $cases -or $cases -lt 1 -or $cases -gt 30) {
      Write-Output "FAIL: suite ciblee hors limite (cas=$cases, attendu 1..30)"
      return
    }
    Write-Output "TESTS::TARGETED_PASS cas=$cases filter=$TestFilter -- suite complete non executee"
  } else {
    & (Join-Path $root 'tools\unreal\report-tests.ps1')
    if ($LASTEXITCODE -ne 0) { Write-Output 'FAIL: des tests sont en echec reel'; return }
  }
  $script:GateMode = 'proved'
  $script:GateOk = $true
}

# Deplace refs/heads/main, et elle seule, par avance rapide vers $rev (integrate et
# integrate-batch). Ne touche la copie de travail du canonique que s'il est sur main.
# Ecrit son compte rendu et pose $script:MoveOk.
# CANONICAL_FRESH_001 (regle absolue d'Alexandre, 2026-10-07) : la racine canonique est toujours a
# jour, binaires compris. Les lots compilent dans le worktree d'integration ; sans ceci, l'editeur
# d'Alexandre ouvert sur le canonique chargeait des DLL du 2026-10-02 alors que main etait du 7.
# Perime = la DLL de jeu est plus ancienne que le dernier commit de main qui touche Source/,
# Plugins/ ou le .uproject (date de commit : celle du versement).
function Test-CanonicalBinaries {
  $dll = Join-Path $Canonical 'Binaries\Win64\UnrealEditor-Anastasis_UnrealV2.dll'
  # '--' entre quotes : nu, PowerShell l'avale comme fin des parametres de Invoke-Git.
  $last = (Invoke-Git -C $Canonical log -1 --format=%ct main '--' Source Plugins '*.uproject').Out | Where-Object { $_ -match '^\d+$' } | Select-Object -First 1
  if (-not $last) { return [PSCustomObject]@{ Stale = $false; Why = 'aucun commit Unreal' } }
  $lastUtc = [DateTimeOffset]::FromUnixTimeSeconds([int64]$last).UtcDateTime
  if (-not (Test-Path $dll)) { return [PSCustomObject]@{ Stale = $true; Why = 'DLL absente' } }
  $built = (Get-Item $dll).LastWriteTimeUtc
  if ($built -lt $lastUtc) { return [PSCustomObject]@{ Stale = $true; Why = "DLL du $($built.ToLocalTime().ToString('yyyy-MM-dd HH:mm')), main Unreal du $($lastUtc.ToLocalTime().ToString('yyyy-MM-dd HH:mm'))" } }
  return [PSCustomObject]@{ Stale = $false; Why = "DLL du $($built.ToLocalTime().ToString('yyyy-MM-dd HH:mm'))" }
}
# Apres chaque deplacement de main : recompile le canonique s'il est perime. Jamais sous un editeur
# ouvert sur le canonique (Live Coding tient les DLL) : on le dit, avec le pid, et le prochain
# versement ou `status` le rappelle. ANASTASIS_CANONICAL_BUILD=0 : banc d'essai seulement.
function Update-CanonicalBuild {
  if ($env:ANASTASIS_CANONICAL_BUILD -eq '0') { Write-Output 'CANONICAL_BUILD::SKIP (ANASTASIS_CANONICAL_BUILD=0)'; return }
  if ((Invoke-Git -C $Canonical branch --show-current).Out[0] -ne 'main') { Write-Output 'CANONICAL_BUILD::SKIP (canonique hors main : sa copie de travail n a pas bouge)'; return }
  $state = Test-CanonicalBinaries
  if (-not $state.Stale) { Write-Output "CANONICAL_BUILD::A_JOUR ($($state.Why))"; return }
  $uproject = Join-Path $Canonical 'Anastasis_UnrealV2.uproject'
  $open = @(Get-CimInstance Win32_Process -Filter "Name like 'UnrealEditor%'" -ErrorAction SilentlyContinue | Where-Object { $_.CommandLine -and $_.CommandLine.Contains($uproject) })
  if ($open.Count -gt 0) {
    Write-Output "CANONICAL_BUILD::DIFFERE editeur ouvert sur le canonique (pid $(($open | ForEach-Object { $_.ProcessId }) -join ', ')) -- $($state.Why). Le fermer puis : tools\unreal\anastasis-unreal.ps1 build"
    return
  }
  $out = @(& (Join-Path $Canonical 'tools\unreal\anastasis-unreal.ps1') build 2>&1 | ForEach-Object { "$_" })
  if ($LASTEXITCODE -eq 0) { Write-Output "CANONICAL_BUILD::PASS ($($state.Why) -> recompile)" }
  else {
    $out | Select-Object -Last 15 | ForEach-Object { Write-Output "    $_" }
    Write-Output 'CANONICAL_BUILD::FAIL main a avance mais le canonique ne compile pas : a traiter avant tout autre lot'
  }
}

function Move-Main([string]$rev) {
  $script:MoveOk = $false
  $current = (Invoke-Git -C $Canonical branch --show-current).Out[0]
  $env:ANASTASIS_INTEGRATION = '1'
  try {
    if ($current -eq 'main') {
      # L integrateur ne doit jamais perturber le travail en vol d un autre agent :
      # refus precis, seulement si l integration touche un fichier modifie ici.
      $dirty = @((Invoke-Git -C $Canonical status --porcelain --untracked-files=no).Out)
      $incoming = @((Invoke-Git -C $Canonical diff --name-only "main..$rev").Out)
      $localPaths = @($dirty | ForEach-Object { $_.Substring(3).Trim('"') })
      $overlap = @($incoming | Where-Object { $localPaths -contains $_ })
      if ($overlap.Count -gt 0) {
        Write-Output 'FAIL: l integration ecraserait du travail en cours dans la racine canonique.'
        $overlap | ForEach-Object { Write-Output ('    ' + $_) }
        Write-Output 'Fais atterrir ce travail (commit) avant d integrer.'
        return
      }
      $before = @((Invoke-Git -C $Canonical status --porcelain).Out)
      $m = Invoke-Git -C $Canonical merge --ff-only $rev
      if ($m.Code -ne 0) {
        $m.Out | ForEach-Object { Write-Output ('    ' + $_) }
        # Un refus du hook arrive en phase 'prepared', la copie de travail deja ecrite.
        $after = @((Invoke-Git -C $Canonical status --porcelain).Out)
        $new = @($after | Where-Object { $before -notcontains $_ })
        if ($new.Count -gt 0) {
          Write-Output "FAIL: versement refuse ET racine canonique salie ($($new.Count) entrees). A restaurer :"
          $new | Select-Object -First 20 | ForEach-Object { Write-Output ('    ' + $_) }
        } else {
          Write-Output 'FAIL: versement refuse, racine canonique inchangee.'
        }
        return
      }
    } else {
      # main n est extraite nulle part ici : on deplace la ref, sans toucher la copie de
      # travail ni la branche extraite. fetch refuse de lui-meme un non-fast-forward, et une
      # branche extraite dans un autre worktree.
      Write-Output "NOTE: le canonique est sur '$current', pas sur main : copie de travail laissee intacte."
      $f = Invoke-Git -C $Canonical fetch . "$($rev):main"
      if ($f.Code -ne 0) {
        $f.Out | ForEach-Object { Write-Output ('    ' + $_) }
        Write-Output 'FAIL: deplacement de main refuse, rien n a change.'
        return
      }
    }
  } finally { Remove-Item Env:ANASTASIS_INTEGRATION -ErrorAction SilentlyContinue }
  $script:MoveOk = $true
}

. (Join-Path $PSScriptRoot 'mcp-port.ps1')
. (Join-Path $PSScriptRoot 'tools-index.ps1')
. (Join-Path $PSScriptRoot 'editor-launch.ps1')

# Enregistre, en portee locale Claude Code, le serveur MCP de l'editeur de CE worktree.
# La portee locale (cle = chemin du worktree dans ~/.claude.json) prime sur le .mcp.json
# du projet, qui vise 8000, le port du canonique. Codex n'a pas d'equivalent par projet.
function Register-Mcp($m) {
  $path = Path-Of $m
  $url = "http://localhost:$(Get-AnastasisMcpPort $path)/mcp"
  Write-Output "MCP_URL::$url"
  if (-not (Get-Command claude -ErrorAction SilentlyContinue)) {
    Write-Output 'MCP_CLIENT::NON_ENREGISTRE (claude introuvable). A la main, depuis le worktree :'
    Write-Output "    claude mcp add --scope local --transport http unreal $url"
    return
  }
  # PowerShell 5.1 : sous 'Stop', le stderr d'un exe natif devient une erreur fatale.
  # Or `remove` ecrit sur stderr des qu'il n'y a rien a retirer, le cas d'un create.
  $ErrorActionPreference = 'Continue'
  Push-Location -LiteralPath $path
  try {
    & claude mcp remove --scope local unreal 2>$null | Out-Null
    & claude mcp add --scope local --transport http unreal $url 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) { Write-Output 'MCP_CLIENT::ENREGISTRE (Claude Code, portee locale)' }
    else { Write-Output "MCP_CLIENT::ECHEC claude mcp add (code $LASTEXITCODE)" }
  } finally { Pop-Location }
}

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

# Le verrou de main est rendu quoi qu'il arrive : `exit` (Fail) execute aussi ce finally.
try {
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
    Write-Output ''
    Write-Output 'Fiche de passation requise avant finish :'
    Write-Output "  Copy-Item docs\unreal\handoffs\_TEMPLATE.md docs\unreal\handoffs\$Mission.md"
    Write-Output ''
    Register-Mcp $Mission
    Write-Output '  Editeur de ce worktree sur ce port : tools\unreal\anastasis-unreal.ps1 editor'
  }

  'mcp' {
    Require-Mission
    if (-not (Test-Path (Path-Of $Mission))) { Fail "FAIL: worktree introuvable -> $(Path-Of $Mission)" }
    Register-Mcp $Mission
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
    $cb = Test-CanonicalBinaries
    Write-Output ('CANONICAL_BINAIRES::' + $(if ($cb.Stale) { "PERIMES ($($cb.Why)) -- tools\unreal\anastasis-unreal.ps1 build dans la racine, editeur ferme" } else { "A_JOUR ($($cb.Why))" }))
    $unmerged = @(& git -C $Canonical branch --no-merged main --list 'agent/*')
    if ($unmerged.Count -gt 0) {
      Write-Output 'BRANCHES_NON_INTEGREES::'
      $unmerged | ForEach-Object { Write-Output ('    ' + $_.Trim()) }
    } else {
      Write-Output 'BRANCHES_NON_INTEGREES::aucune'
    }
    # EDITOR_QUEUE_001 : ce que l'integrateur a a verser, et si un lot tient main.
    $lock = Read-MainLock
    Write-Output ('MAIN_LOCK::' + $(if ($lock -and $lock.Alive) { "TENU $($lock.Holder) depuis $($lock.Since)" } else { 'libre' }))
    $readyRows = @()
    foreach ($r in $rows) {
      if ($r.Ahead -eq '0' -or -not $r.Branch.StartsWith('agent/')) { continue }
      $mk = Read-HandoffMarker $r.Mission
      if ($mk -and $mk.Sha -eq (& git -C $Canonical rev-parse $r.Branch).Trim()) { $readyRows += [PSCustomObject]@{ Mission = $r.Mission; Mode = $mk.Mode } }
    }
    # INTEGRATION_SPEED_001 : n'annoncer pour le lot que ce qui s'y empilera vraiment. Une mission
    # en conflit avec une autre du lot, mais propre seule sur main, attend le lot suivant ; en
    # conflit avec main, son agent rebase. Deja dans main par contenu : prune.
    $clean = @(); $next = @(); $rebase = @(); $done = @()
    if ($readyRows.Count -gt 0) {
      foreach ($p in (Get-StackPreview $Canonical 'main' @($readyRows | ForEach-Object { $_.Mission }))) {
        $mode = ($readyRows | Where-Object { $_.Mission -eq $p.Mission } | Select-Object -First 1).Mode
        if ($p.New -eq 0) { $done += $p.Mission }
        elseif ($p.Conflicts.Count -eq 0) { $clean += "$($p.Mission) ($mode)" }
        elseif ((Get-StackPreview $Canonical 'main' @($p.Mission))[0].Conflicts.Count -eq 0) { $next += $p.Mission }
        else { $rebase += "$($p.Mission) ($($p.Conflicts -join ' '))" }
      }
    }
    if ($clean.Count -gt 0) {
      Write-Output ('PRETES_POUR_LE_LOT::' + ($clean -join ', '))
      Write-Output ('    tools\unreal\agent-worktree.ps1 integrate-batch -Missions ' + (($clean | ForEach-Object { ($_ -split ' ')[0] }) -join ','))
    } else {
      Write-Output 'PRETES_POUR_LE_LOT::aucune'
    }
    if ($next.Count -gt 0) { Write-Output ('LOT_SUIVANT::' + ($next -join ', ') + ' (propres seules sur main, en conflit avec une mission du lot)') }
    if ($rebase.Count -gt 0) {
      Write-Output 'A_REBASER:: (en conflit avec main : dans le worktree, git rebase main puis finish)'
      $rebase | ForEach-Object { Write-Output "    $_" }
    }
    if ($done.Count -gt 0) { Write-Output ('DEJA_DANS_MAIN::' + ($done -join ', ') + ' (prune)') }
    exit 0
  }

  'finish' {
    Require-Mission
    $path = Path-Of $Mission
    if (-not (Test-Path $path)) { Fail "FAIL: worktree introuvable -> $path" }
    $handoff = Handoff-Path $Mission
    if (-not (Test-Path $handoff)) {
      Write-Output "FAIL: fiche de passation manquante -> $handoff"
      Write-Output "Copie puis remplis : docs\unreal\handoffs\_TEMPLATE.md"
      Write-Output "Champs requis : MISSION, FILES_OWNED, COMMIT, MEC, SCN, PLY, INTEGRATION_RISK"
      exit 1
    }
    Write-Output "=== Portail de fin de mission : $Mission ==="
    $index = Test-AnastasisToolsIndex $path
    if (($index.Missing.Count + $index.Stale.Count) -gt 0) {
      Write-Output 'FAIL: index de tools/unreal/ dans AGENTS.md desynchronise'
      $index.Missing | ForEach-Object { Write-Output "    MISSING $_  (present, non indexe)" }
      $index.Stale | ForEach-Object { Write-Output "    STALE   $_  (indexe, absent)" }
      exit 1
    }
    $raw = @(Find-RawEditorLaunch $path)
    if ($raw.Count -gt 0) {
      Write-Output 'FAIL: Unreal lance sans Start-AnastasisEditor (fenetre au premier plan devant Alexandre, AGENTS.md)'
      $raw | ForEach-Object { Write-Output "    $_" }
      exit 1
    }
    # Un editeur laisse ouvert garde sa memoire apres la mission, hors ecran, et bloque la
    # porte memoire de tous les autres agents (AGENTS.md, « Porte memoire »).
    $open = @(Find-WorktreeEditor $path)
    if ($open.Count -gt 0) {
      Write-Output 'FAIL: editeur Unreal encore ouvert sur ce worktree -- le fermer avant de passer la main'
      $open | ForEach-Object { Write-Output "    $_" }
      Write-Output '    Fermeture propre : quit_editor() par MCP, ou Stop-Process -Id <pid> (c est le tien : son chemin est ce worktree).'
      exit 1
    }
    # Les preuves PIE que la fiche declare (`PROOFS: a, b`) doivent exister au registre : le lot
    # les rejouera, et une preuve inconnue y ferait echouer TOUT le lot.
    $declared = @(Get-DeclaredProofs $path 'HEAD' $Mission)
    $known = @(Get-RegisteredProofs $path)
    $unknownProofs = @($declared | Where-Object { $known -notcontains $_ })
    if ($unknownProofs.Count -gt 0) {
      Write-Output ('FAIL: preuve(s) declaree(s) absente(s) de tools/unreal/proofs.txt : ' + ($unknownProofs -join ', '))
      Write-Output '    Inscrire la preuve au registre (nom | script | reussite | echec | delai | variables), ou la retirer de PROOFS:'
      exit 1
    }
    $dup = @(Get-DuplicateProofs $path)
    if ($dup.Count -gt 0) {
      Write-Output ('FAIL: tools/unreal/proofs.txt inscrit deux fois : ' + ($dup -join ', '))
      Write-Output '    La fusion par union (.gitattributes) garde les deux versions d une ligne modifiee des deux cotes : n en garder qu une.'
      exit 1
    }
    Write-Output ('PROOFS::' + $(if ($declared.Count) { $declared -join ', ' } else { '(aucune)' }))
    # Ecarts de portage (docs\migration\PROTOCOLE_ECARTS.md) : une mission qui touche le C++ de
    # Source\AnastasisSim nomme dans sa fiche ce qu'elle ne porte pas fidelement. Avant le
    # build : un oubli echoue en quelques secondes, pas apres dix minutes de compilation.
    if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
      Fail 'FAIL: node introuvable -- requis par tools\migration\check-ecarts.mjs (protocole des ecarts)'
    }
    & node (Join-Path $path 'tools\migration\check-ecarts.mjs') -base main -handoff $handoff
    if ($LASTEXITCODE -ne 0) { Fail 'FAIL: protocole des ecarts -- voir docs\migration\PROTOCOLE_ECARTS.md' }
    # STATE_ORACLE_001 : un champ d'etat de la simulation est lu par StateDigest (l'oracle des tests
    # de determinisme et de non-ecriture) ou classe dans tools\migration\state-fields.json. Quelques ms.
    # -base main : seul ce que la branche ajoute echoue ; un champ deja verse par une autre sort en WARN.
    & node (Join-Path $path 'tools\migration\check-state-fields.mjs') -base main
    if ($LASTEXITCODE -ne 0) { Fail 'FAIL: champs d etat non lus par StateDigest -- voir tools\migration\check-state-fields.mjs' }
    # Build seulement si la branche change quelque chose qu'il juge ; la suite, seulement avec
    # -Prove ou -Full (EDITOR_QUEUE_001 : sinon elle attend le lot, un editeur pour tous).
    $mb = (Invoke-Git -C $path merge-base main HEAD).Out[0]
    $unreal = @(Unreal-Changes $path "$mb..HEAD")
    # RETEST_RULE_001 : finish apres un rebase. Le commit marque a deja passe ce portail ; si
    # ses arbres Unreal sont ceux de HEAD, le rejouer ne jugerait rien de nouveau. Le mode
    # prouve est conserve : un `queued` reste `queued` (la suite attend toujours le lot), et
    # -Prove n'est satisfait que par un `proved`. -Full force le portail.
    $head = (Invoke-Git -C $path rev-parse HEAD).Out[0]
    $prev = Read-HandoffMarker $Mission
    $reuse = $unreal.Count -gt 0 -and -not $Full -and $prev -and $prev.Sha -ne $head -and
      ($prev.Mode -eq 'proved' -or ($prev.Mode -eq 'queued' -and -not $Prove)) -and
      (Test-SameUnrealTrees $path $prev.Sha $head)
    if ($reuse) {
      Write-RetestSkip $prev.Sha $prev.Mode ''
      $script:GateOk = $true
      $script:GateMode = $prev.Mode
    } else {
      Invoke-UnrealGate $path $unreal ([bool]($Prove -or $Full))
    }
    if (-not $script:GateOk) { exit 1 }
    $dirty = @(& git -C $path status --porcelain --untracked-files=all)
    Write-Output ''
    if ($dirty.Count -gt 0) {
      Write-Output 'RESTE_A_COMMITER::'
      $dirty | ForEach-Object { Write-Output ('    ' + $_) }
      Write-Output ''
      Write-Output 'Un commit est l unite de passation : commit tout avant de passer la main.'
      exit 1
    }
    Write-HandoffMarker $Mission $head $script:GateMode
    Write-Output "HANDOFF_READY::YES ($($script:GateMode))"
    if ($script:GateMode -eq 'queued') {
      Write-Output 'Passation : la mission attend le prochain lot. L integrateur la verse avec les autres :'
      Write-Output "     tools\unreal\agent-worktree.ps1 integrate-batch -Missions $Mission,<autres>"
      Write-Output '     (suite + preuves PIE declarees, un seul editeur pour tout le lot). Ne pas lancer d editeur pour se prouver.'
    } else {
      Write-Output "Passation : tools\unreal\agent-worktree.ps1 integrate-batch -Missions $Mission,<autres>"
    }
  }

  'integrate' {
    # Deplace refs/heads/main, et elle seule, par avance rapide vers agent/<mission>.
    #
    # Avant 2026-09-30 : `git merge --ff-only` dans le canonique, donc sur la branche
    # EXTRAITE. Le 2026-09-29 le canonique etait sur la branche d'un autre agent, avec son
    # C++ non commite : integrate aurait avance cette branche et reecrit sa copie de travail.
    # Et sans ANASTASIS_INTEGRATION=1, le hook reference-transaction refusait toute
    # avance de main -- apres avoir deja ecrit la copie de travail.
    Require-Mission
    $branch = Branch-Of $Mission
    if ((Invoke-Git -C $Canonical rev-parse --verify --quiet $branch).Code -ne 0) { Fail "FAIL: branche introuvable -> $branch" }
    $mainBefore = (Invoke-Git -C $Canonical rev-parse main).Out[0]
    $current = (Invoke-Git -C $Canonical branch --show-current).Out[0]
    Write-Output "CANONICAL_BRANCH::$current"
    Write-Output "MAIN_BEFORE::$mainBefore"

    # 1. Rien a verser ? Puis avance rapide seulement : sinon main a bouge depuis la
    #    preuve de finish. Dans cet ordre : une branche deja versee, suivie d'autres
    #    versements, n'est plus un ancetre-de-main mais n'a plus rien a apporter.
    $ahead = [int](Invoke-Git -C $Canonical rev-list --count "main..$branch").Out[0]
    if ($ahead -eq 0) { Write-Output "NOTHING_TO_INTEGRATE::$branch deja dans main"; exit 0 }
    # Une mission dont la suite attend le lot n'a rien prouve d'executable : elle passe par le lot.
    $marker = Read-HandoffMarker $Mission
    $tipNow = (Invoke-Git -C $Canonical rev-parse $branch).Out[0]
    if ($marker -and $marker.Mode -eq 'queued' -and $marker.Sha -eq $tipNow) {
      Fail "FAIL: $Mission attend le lot (TESTS::QUEUED) : tools\unreal\agent-worktree.ps1 integrate-batch -Missions $Mission"
    }
    Enter-MainLock "integrate:$Mission"
    $target = $branch
    if ((Invoke-Git -C $Canonical merge-base --is-ancestor main $branch).Code -ne 0) {
      # RETEST_RULE_001 : main a avance. Plutot que renvoyer l'agent rebaser et se reprouver, la
      # branche est rejouee sur main dans le worktree d'integration ; si le resultat ne demande
      # rien de neuf au build ni a la suite, elle est versee telle quelle. Il faut une preuve
      # de finish sur le commit actuel de la branche.
      if (-not $marker -or $marker.Sha -ne $tipNow -or $Full) {
        Write-Output 'FAIL: pas d avance rapide possible (main a avance). Dans le worktree :'
        Write-Output "    git rebase main ; tools\unreal\agent-worktree.ps1 finish -Mission $Mission"
        exit 1
      }
      Write-Output 'NOTE: main a avance : la branche est rejouee sur main (worktree d integration)'
      $integ = Integration-Path
      Reset-IntegrationTree $integ 'integration/batch'
      $integBase = (Invoke-Git -C $integ rev-parse HEAD).Out[0]
      $commits = @((Invoke-Git -C $integ cherry HEAD $branch).Out | Where-Object { $_ -like '+ *' } | ForEach-Object { $_.Substring(2).Trim() })
      if ($commits.Count -eq 0) { Write-Output "NOTHING_TO_INTEGRATE::$branch deja dans main (par contenu)"; exit 0 }
      $p = Invoke-Git -C $integ cherry-pick @commits
      if ($p.Code -ne 0) {
        $null = Invoke-Git -C $integ cherry-pick --abort
        $null = Invoke-Git -C $integ reset -q --hard main
        Write-Output 'FAIL: conflit avec main. Dans le worktree :'
        Write-Output "    git rebase main ; tools\unreal\agent-worktree.ps1 finish -Mission $Mission"
        exit 1
      }
      $target = (Invoke-Git -C $integ rev-parse HEAD).Out[0]
      $unreal = @(Unreal-Changes $integ "$integBase..$target")
      if ($unreal.Count -eq 0) {
        Write-Output 'UNREAL_CHANGE::NON (ni Source/, ni Config/, ni Content/, ni Plugins/, ni .uproject)'
        Write-Output 'BUILD::SKIP  TESTS::SKIP  -- rien a montrer au build ni a la suite'
      } elseif ($marker.Mode -eq 'proved' -and (Test-SameUnrealTrees $integ $marker.Sha $target)) {
        Write-RetestSkip $marker.Sha $marker.Mode ''
      } else {
        $diff = @(Unreal-Changes $integ "$($marker.Sha)..$target")
        $why = if ($marker.Mode -ne 'proved') { "preuve $($marker.Mode)" } else { "$($diff.Count) fichier(s) Unreal differents de $($marker.Sha.Substring(0, 7)), par ex. $($diff[0])" }
        Write-Output "RETEST::REQUIS ($why) : main a change ce que la preuve jugeait. Dans le worktree :"
        Write-Output "    git rebase main ; tools\unreal\agent-worktree.ps1 finish -Mission $Mission ; puis integrate-batch"
        exit 1
      }
      $ahead = $commits.Count
    }

    # 2. Les controles de finish, rejoues sur l'arbre qui va devenir main. Avance rapide :
    #    cet arbre est celui de la branche. main apporte souvent un nouveau script entre la
    #    preuve et le versement (deux lanceurs Unreal directs en une heure le 2026-09-30).
    $tree = Join-Path ([IO.Path]::GetTempPath()) ("anastasis-integrate-" + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $tree | Out-Null
    try {
      $tar = Join-Path $tree 'tree.tar'
      # Fichier, pas un pipe : PowerShell 5.1 corrompt un flux binaire entre deux exe natifs.
      $a = Invoke-Git -C $Canonical archive --format=tar -o $tar $target AGENTS.md tools/unreal
      if ($a.Code -ne 0) { Fail ("FAIL: git archive`n" + ($a.Out -join "`n")) }
      & tar.exe -xf $tar -C $tree
      if ($LASTEXITCODE -ne 0) { Fail 'FAIL: extraction de l arbre a integrer' }
      $index = Test-AnastasisToolsIndex $tree
      $raw = @(Find-RawEditorLaunch $tree)
    } finally { Remove-Item -LiteralPath $tree -Recurse -Force -ErrorAction SilentlyContinue }
    if (($index.Missing.Count + $index.Stale.Count) -gt 0) {
      Write-Output 'FAIL: index de tools/unreal/ desynchronise dans l arbre a integrer'
      $index.Missing | ForEach-Object { Write-Output "    MISSING $_" }
      $index.Stale | ForEach-Object { Write-Output "    STALE   $_" }
      exit 1
    }
    if ($raw.Count -gt 0) {
      Write-Output 'FAIL: Unreal lance sans Start-AnastasisEditor dans l arbre a integrer'
      $raw | ForEach-Object { Write-Output "    $_" }
      exit 1
    }
    Write-Output 'CHECKS::PASS index tools/unreal, lancements Unreal'

    # 3. Deplacer main.
    Move-Main $target
    if (-not $script:MoveOk) { exit 1 }
    $mainAfter = (Invoke-Git -C $Canonical rev-parse main).Out[0]
    Write-Output "MAIN_AFTER::$mainAfter"
    Write-Output "INTEGRATED::$branch ($ahead commit(s)$(if ($target -ne $branch) { ', rejoues sur main' }))"
    Update-CanonicalBuild
    Write-Output 'Ensuite : git push origin main (le pre-push compile le canonique), puis'
    Write-Output "          tools\unreal\agent-worktree.ps1 prune -Mission $Mission"
  }

  'integrate-batch' {
    # File d'integration groupee (2026-10-01). Avant : chaque mission refaisait build + suite
    # complete, puis `integrate` refusait des que main avait bouge -- avec beaucoup d'agents,
    # chacun repartait au debut a chaque versement d'un autre. Ici : les missions pretes sont
    # empilees sur main dans un worktree d'integration persistant, UN SEUL portail juge le
    # sommet, et main avance d'un coup. Une seule session integratrice a la fois.
    #
    #   1. admission : la branche a passe finish SUR SON COMMIT ACTUEL (marqueur .handoff) ;
    #   2. empilement : ses commits absents (par contenu) du sommet, copies dans l'ordre ;
    #      un conflit ecarte la mission, les autres continuent ;
    #   3. portail : index tools/unreal, lancements Unreal, puis build + suite seulement si
    #      le lot touche Unreal (-Full pour les forcer) ;
    #   4. main avance par avance rapide sur le sommet ; si main a bouge pendant le lot, rien
    #      ne bouge et il suffit de relancer.
    $list = @($Missions | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    if ($TestFilter -and $TestFilter -notmatch '^[A-Za-z0-9_.+]+$') { Fail 'FAIL: -TestFilter contient des caracteres invalides' }
    if ($list.Count -eq 0) { Fail 'FAIL: -Missions mission-a,mission-b est requis' }
    foreach ($m in $list) {
      if ($m -notmatch '^[a-z0-9][a-z0-9._-]*$') { Fail "FAIL: nom de mission invalide '$m'" }
    }
    # Le verrou AVANT de lire main : personne ne la deplacera sous le lot.
    Enter-MainLock ('integrate-batch:' + ($list -join ','))
    $mainBefore = (Invoke-Git -C $Canonical rev-parse main).Out[0]
    Write-Output "MAIN_BEFORE::$mainBefore"

    # 1. Admission.
    $ready = @()
    $rejected = @()
    foreach ($m in $list) {
      $b = Branch-Of $m
      if ((Invoke-Git -C $Canonical rev-parse --verify --quiet $b).Code -ne 0) { $rejected += "$m : branche introuvable"; continue }
      $tip = (Invoke-Git -C $Canonical rev-parse $b).Out[0]
      $marker = Read-HandoffMarker $m
      $proved = if ($marker) { $marker.Sha } else { '' }
      if ($proved -ne $tip) {
        $rejected += "$m : pas de HANDOFF_READY sur son commit actuel $($tip.Substring(0, 7)) (relancer finish)"
        continue
      }
      $ready += $m
    }

    # 2. Empilement, dans le worktree d'integration (persistant : ses Binaries survivent,
    #    le build y est incremental d'un lot a l'autre).
    $integ = Integration-Path
    $ib = 'integration/batch'
    if ($ready.Count -gt 0) { Reset-IntegrationTree $integ $ib }
    $applied = @()
    foreach ($m in $ready) {
      $b = Branch-Of $m
      $before = (Invoke-Git -C $integ rev-parse HEAD).Out[0]
      # Ce que la branche apporte et que le sommet n'a pas deja, PAR CONTENU : une branche
      # empilee sur une autre du lot ne repasse pas les commits de sa base.
      $commits = @((Invoke-Git -C $integ cherry HEAD $b).Out | Where-Object { $_ -like '+ *' } | ForEach-Object { $_.Substring(2).Trim() })
      if ($commits.Count -eq 0) { Write-Output "NOTHING_TO_INTEGRATE::$b"; continue }
      # Une branche empilee peut apporter les commits (et la fiche) d'une autre mission.
      # Sans ce controle, ses preuves PIE ne sont pas ajoutees a $owner plus bas.
      $changed = Invoke-Git -C $integ log --format= --name-only --no-walk @commits
      if ($changed.Code -ne 0) {
        $rejected += "$m : provenance des commits impossible a lire"
        continue
      }
      $alreadyApplied = @($applied | ForEach-Object { $_.Mission })
      $inherited = @($changed.Out | ForEach-Object {
        if ($_ -match '^docs/unreal/handoffs/([a-z0-9][a-z0-9._-]*)\.md$') { $Matches[1] }
      } | Where-Object { $_ -ne $m -and $alreadyApplied -notcontains $_ })
      # La fiche d'un ancetre peut deja etre sur main par contenu alors que son code manque
      # encore. Rechercher aussi les branches ancetres dont au moins un commit reste a verser.
      $ancestors = Invoke-Git -C $integ branch --merged $b --format='%(refname:short)' 'agent/*'
      if ($ancestors.Code -ne 0) {
        $rejected += "$m : ascendance des missions impossible a lire"
        continue
      }
      $ancestryOk = $true
      foreach ($ancestor in $ancestors.Out) {
        if ($ancestor -eq $b -or $ancestor -notmatch '^agent/([a-z0-9][a-z0-9._-]*)$') { continue }
        $other = $Matches[1]
        if ($alreadyApplied -contains $other) { continue }
        $pending = Invoke-Git -C $integ cherry HEAD $ancestor
        if ($pending.Code -ne 0) { $ancestryOk = $false; break }
        if (@($pending.Out | Where-Object { $_ -like '+ *' }).Count -gt 0) { $inherited += $other }
      }
      if (-not $ancestryOk) {
        $rejected += "$m : commits des missions ancetres impossibles a comparer"
        continue
      }
      $inherited = @($inherited | Sort-Object -Unique)
      # Relais declare : chaque mission heritee nommee dans RELAIS: est admise par la mission
      # qui la porte, si ses preuves sont reprises dans le PROOFS: du relais ($owner les recueille).
      if ($inherited.Count -gt 0) {
        $relayList = @(Get-DeclaredRelay $Canonical $b $m)
        $relayProofs = @(Get-DeclaredProofs $Canonical $b $m)
        $kept = @(); $uncovered = @()
        foreach ($i in $inherited) {
          if ($relayList -notcontains $i) { $kept += $i; continue }
          # La fiche de la mission portee, telle que le relais la verse ; a defaut, celle de sa branche.
          $theirs = @(Get-DeclaredProofs $Canonical $b $i)
          if ($theirs.Count -eq 0 -and (Invoke-Git -C $Canonical rev-parse --verify --quiet (Branch-Of $i)).Code -eq 0) { $theirs = @(Get-DeclaredProofs $Canonical (Branch-Of $i) $i) }
          $miss = @($theirs | Where-Object { $relayProofs -notcontains $_ })
          if ($miss.Count -gt 0) { $uncovered += "$i (preuves non reprises : $($miss -join ', '))" }
          else { Write-Output "RELAY_ADMITTED::$m porte $i (preuves : $(if ($theirs.Count) { $theirs -join ', ' } else { 'aucune' }))" }
        }
        if ($uncovered.Count -gt 0) {
          $rejected += "$m : relais incomplet, PROOFS: ne reprend pas les preuves de $($uncovered -join ' ; ')"
          continue
        }
        $inherited = $kept
      }
      if ($inherited.Count -gt 0) {
        $rejected += "$m : commits de mission(s) heritee(s) non versees avant elle : $($inherited -join ', ')"
        continue
      }
      $p = Invoke-Git -C $integ cherry-pick @commits
      if ($p.Code -ne 0) {
        $null = Invoke-Git -C $integ cherry-pick --abort
        $null = Invoke-Git -C $integ reset -q --hard $before
        $rejected += "$m : conflit avec main ou une mission precedente du lot (rebase dans son worktree, puis finish)"
        continue
      }
      $applied += [PSCustomObject]@{ Mission = $m; Commits = $commits.Count }
    }
    foreach ($r in $rejected) { Write-Output "BATCH_REJECTED::$r" }
    if ($applied.Count -eq 0) { Write-Output 'BATCH::RIEN_A_VERSER'; exit 1 }
    Write-Output ('BATCH_STACKED::' + (($applied | ForEach-Object { "$($_.Mission) ($($_.Commits))" }) -join ', '))

    # 3. Le portail, une fois, sur l'arbre qui va devenir main.
    $index = Test-AnastasisToolsIndex $integ
    if (($index.Missing.Count + $index.Stale.Count) -gt 0) {
      Write-Output 'FAIL: index de tools/unreal/ desynchronise dans le lot'
      $index.Missing | ForEach-Object { Write-Output "    MISSING $_" }
      $index.Stale | ForEach-Object { Write-Output "    STALE   $_" }
      exit 1
    }
    $raw = @(Find-RawEditorLaunch $integ)
    if ($raw.Count -gt 0) {
      Write-Output 'FAIL: Unreal lance sans Start-AnastasisEditor dans le lot'
      $raw | ForEach-Object { Write-Output "    $_" }
      exit 1
    }
    $dup = @(Get-DuplicateProofs $integ)
    if ($dup.Count -gt 0) { Write-Output ('FAIL: tools/unreal/proofs.txt du lot inscrit deux fois : ' + ($dup -join ', ') + ' (union des deux versions d une ligne : rien n a bouge, main intacte)'); exit 1 }
    Write-Output 'CHECKS::PASS index tools/unreal, lancements Unreal'
    # Le lot sert tous les agents : ses editeurs passent en tete de la file de la porte memoire.
    $env:ANASTASIS_EDITOR_PRIORITY = '0'
    $owner = @{}
    foreach ($a in $applied) {
      foreach ($pr in (Get-DeclaredProofs $Canonical (Branch-Of $a.Mission) $a.Mission)) {
        if (-not $owner.ContainsKey($pr)) { $owner[$pr] = @() }
        $owner[$pr] += $a.Mission
      }
    }
    $top = (Invoke-Git -C $integ rev-parse HEAD).Out[0]
    $unreal = @(Unreal-Changes $integ "$mainBefore..$top")
    # RETEST_RULE_001 : une mission du lot dont `finish -Prove` a juge exactement ces arbres
    # Unreal (typiquement une mission seule, rejouee sur un main qui n'a bouge qu'en docs) : la
    # suite ne jugerait rien de neuf. Seul `proved` compte : `queued` n'a pas vu la suite.
    $reusedFrom = $null
    if ($unreal.Count -gt 0 -and -not $Full) {
      foreach ($a in $applied) {
        $mk = Read-HandoffMarker $a.Mission
        if ($mk -and $mk.Mode -eq 'proved' -and (Test-SameUnrealTrees $integ $mk.Sha $top)) { $reusedFrom = [PSCustomObject]@{ Mission = $a.Mission; Sha = $mk.Sha }; break }
      }
    }
    $built = $false
    if ($reusedFrom) {
      Write-Output "UNREAL_CHANGE::OUI ($($unreal.Count) fichier(s), par ex. $($unreal[0]))"
      Write-RetestSkip $reusedFrom.Sha 'proved' ", finish -Prove de $($reusedFrom.Mission)"
    } else {
      Invoke-UnrealGate $integ $unreal
      if (-not $script:GateOk) { Write-Output 'BATCH::FAIL rien n a bouge (main intact)'; exit 1 }
      $built = $script:GateMode -ne 'nounreal'
    }

    # 3b. Les preuves PIE declarees par les missions du lot, toutes dans UN editeur
    #     (EDITOR_QUEUE_001). Une preuve en echec designe sa mission.
    if ($owner.Count -gt 0) {
      # Les preuves tournent sur les binaires du worktree d'integration : sans build dans CE lot
      # (RETEST::SKIP ou lot sans changement Unreal), ils peuvent dater d'un lot precedent.
      if (-not $built) {
        Write-Output 'BUILD::RUN pour les preuves PIE : binaires du worktree d integration a jour (incremental)'
        & (Join-Path $integ 'tools\unreal\anastasis-unreal.ps1') build
        if ($LASTEXITCODE -ne 0) { Write-Output 'BATCH::FAIL build avant les preuves PIE, rien n a bouge (main intact)'; exit 1 }
      }
      $known = @(Get-RegisteredProofs $integ)
      $missing = @($owner.Keys | Where-Object { $known -notcontains $_ })
      if ($missing.Count -gt 0) { Write-Output ('BATCH::FAIL preuve(s) absente(s) du registre du lot : ' + ($missing -join ', ')); exit 1 }
      $proofOut = @(& (Join-Path $integ 'tools\unreal\editor-batch.ps1') -Proofs @($owner.Keys) 2>&1 | ForEach-Object { "$_" })
      $proofCode = $LASTEXITCODE
      $proofOut | ForEach-Object { Write-Output $_ }
      if ($proofCode -ne 0) {
        foreach ($line in $proofOut) {
          if ($line -match '^PROOF::FAIL (\S+)') { Write-Output "BATCH_PROOF_FAIL::$($Matches[1]) (mission $($owner[$Matches[1]] -join ', '))" }
        }
        Write-Output 'BATCH::FAIL preuves PIE en echec, rien n a bouge (main intact) : relancer le lot sans la mission designee'
        exit 1
      }
    } else {
      Write-Output 'PROOFS::aucune preuve PIE declaree par le lot'
    }

    # 4. Avance rapide de main sur le sommet du lot.
    $mainNow = (Invoke-Git -C $Canonical rev-parse main).Out[0]
    if ($mainNow -ne $mainBefore) {
      Write-Output "FAIL: main a bouge pendant le lot ($($mainBefore.Substring(0, 7)) -> $($mainNow.Substring(0, 7))) : relancer integrate-batch"
      exit 1
    }
    Move-Main $top
    if (-not $script:MoveOk) { exit 1 }
    $mainAfter = (Invoke-Git -C $Canonical rev-parse main).Out[0]
    Write-Output "MAIN_AFTER::$mainAfter"
    Write-Output ('BATCH_INTEGRATED::' + (($applied | ForEach-Object { $_.Mission }) -join ', '))
    Update-CanonicalBuild
    Write-Output 'Ensuite : git push origin main, puis pour chaque mission versee :'
    $applied | ForEach-Object { Write-Output "          tools\unreal\agent-worktree.ps1 prune -Mission $($_.Mission)" }
  }

  'prune' {
    # Etape 6 de la passe (docs/unreal/OPERATIONS.md) : worktree, branche et enregistrement
    # MCP local d une mission entierement dans main. `git branch -d` ne sert pas ici : il
    # compare a la branche extraite du canonique, pas a main.
    Require-Mission
    $branch = Branch-Of $Mission
    $path = Path-Of $Mission
    if ((Invoke-Git -C $Canonical rev-parse --verify --quiet $branch).Code -ne 0) { Fail "FAIL: branche introuvable -> $branch" }
    # Par CONTENU (`git cherry`) : integrate-batch verse des copies des commits de la
    # branche, pas les commits eux-memes ; une copie dans main vaut versement.
    $outside = @((Invoke-Git -C $Canonical cherry main $branch).Out | Where-Object { $_ -like '+ *' }).Count
    if ($outside -gt 0) {
      # Un lot peut verser une copie au diff retouche (union de proofs.txt, contexte deplace) :
      # `cherry` ne la reconnait plus. Si rejouer la branche sur main ne change rien, elle y est.
      $pv = @(Get-StackPreview $Canonical 'main' @($Mission))[0]
      if ($pv -and $pv.New -eq 0 -and $pv.Conflicts.Count -eq 0) {
        Write-Output "PRUNE::PAR_CONTENU ($outside commit(s) non reconnus par git cherry ; rejoues sur main, ils ne changent rien)"
      } else {
        Fail "FAIL: $outside commit(s) de $branch absents de main : integrer d abord"
      }
    }
    if (Test-Path -LiteralPath $path) {
      $dirty = @((Invoke-Git -C $path status --porcelain --untracked-files=all).Out)
      if ($dirty.Count -gt 0) {
        Write-Output "FAIL: worktree non propre, rien n est supprime -> $path"
        $dirty | Select-Object -First 20 | ForEach-Object { Write-Output ('    ' + $_) }
        exit 1
      }
      if (Get-Command claude -ErrorAction SilentlyContinue) {
        $ErrorActionPreference = 'Continue'
        Push-Location -LiteralPath $path
        try { & claude mcp remove --scope local unreal 2>$null | Out-Null } finally { Pop-Location }
        $ErrorActionPreference = 'Stop'
      }
      $w = Invoke-Git -C $Canonical worktree remove $path
      if ($w.Code -ne 0) {
        $w.Out | ForEach-Object { Write-Output ('    ' + $_) }
        Fail 'FAIL: git worktree remove (un editeur ouvert sur ce worktree verrouille ses fichiers ?)'
      }
      Write-Output "WORKTREE_REMOVED::$path"
    }
    $b = Invoke-Git -C $Canonical branch -D $branch
    if ($b.Code -ne 0) { Fail ("FAIL: git branch -D`n" + ($b.Out -join "`n")) }
    Remove-Item -LiteralPath (Handoff-Marker $Mission) -ErrorAction SilentlyContinue
    Write-Output "BRANCH_DELETED::$branch"
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
} finally { Exit-MainLock }
