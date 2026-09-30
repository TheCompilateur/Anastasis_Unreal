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
#   agent-worktree.ps1 mcp        -Mission world-slice-007
#   agent-worktree.ps1 prune      -Mission world-slice-007
param(
  [Parameter(Mandatory = $true)]
  [ValidateSet('create', 'status', 'finish', 'integrate', 'preflight', 'postflight', 'mcp', 'prune')]
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
function Handoff-Path($m) { return (Join-Path (Path-Of $m) "docs\unreal\handoffs\$m.md") }

# git sans que PowerShell 5.1 ne transforme son stderr en erreur fatale : `fetch`, `merge`,
# `worktree` ecrivent leur progression sur stderr, et sous 'Stop' le script mourait en
# plein versement. Rend le code de sortie et toute la sortie, en texte.
function Invoke-Git {
  $ErrorActionPreference = 'Continue'
  $out = @(& git @args 2>&1 | ForEach-Object { "$_" })
  return [PSCustomObject]@{ Code = $LASTEXITCODE; Out = $out }
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
    if ((Invoke-Git -C $Canonical merge-base --is-ancestor main $branch).Code -ne 0) {
      Write-Output 'FAIL: pas d avance rapide possible (main a avance). Dans le worktree :'
      Write-Output "    git rebase main ; tools\unreal\agent-worktree.ps1 finish -Mission $Mission"
      exit 1
    }

    # 2. Les controles de finish, rejoues sur l'arbre qui va devenir main. Avance rapide :
    #    cet arbre est celui de la branche. main apporte souvent un nouveau script entre la
    #    preuve et le versement (deux lanceurs Unreal directs en une heure le 2026-09-30).
    $tree = Join-Path ([IO.Path]::GetTempPath()) ("anastasis-integrate-" + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $tree | Out-Null
    try {
      $tar = Join-Path $tree 'tree.tar'
      # Fichier, pas un pipe : PowerShell 5.1 corrompt un flux binaire entre deux exe natifs.
      $a = Invoke-Git -C $Canonical archive --format=tar -o $tar $branch AGENTS.md tools/unreal
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
    $env:ANASTASIS_INTEGRATION = '1'
    try {
      if ($current -eq 'main') {
        # L integrateur ne doit jamais perturber le travail en vol d un autre agent :
        # refus precis, seulement si l integration touche un fichier modifie ici.
        $dirty = @((Invoke-Git -C $Canonical status --porcelain --untracked-files=no).Out)
        $incoming = @((Invoke-Git -C $Canonical diff --name-only "main..$branch").Out)
        $localPaths = @($dirty | ForEach-Object { $_.Substring(3).Trim('"') })
        $overlap = @($incoming | Where-Object { $localPaths -contains $_ })
        if ($overlap.Count -gt 0) {
          Write-Output 'FAIL: l integration ecraserait du travail en cours dans la racine canonique.'
          $overlap | ForEach-Object { Write-Output ('    ' + $_) }
          Write-Output 'Fais atterrir ce travail (commit) avant d integrer.'
          exit 1
        }
        $before = @((Invoke-Git -C $Canonical status --porcelain).Out)
        $m = Invoke-Git -C $Canonical merge --ff-only $branch
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
          exit 1
        }
      } else {
        # main n est extraite nulle part ici : on deplace la ref, sans toucher la copie de
        # travail ni la branche extraite. fetch refuse de lui-meme un non-fast-forward, et une
        # branche extraite dans un autre worktree.
        Write-Output "NOTE: le canonique est sur '$current', pas sur main : copie de travail laissee intacte."
        $f = Invoke-Git -C $Canonical fetch . "$($branch):main"
        if ($f.Code -ne 0) {
          $f.Out | ForEach-Object { Write-Output ('    ' + $_) }
          Fail 'FAIL: deplacement de main refuse, rien n a change.'
        }
      }
    } finally { Remove-Item Env:ANASTASIS_INTEGRATION -ErrorAction SilentlyContinue }
    $mainAfter = (Invoke-Git -C $Canonical rev-parse main).Out[0]
    Write-Output "MAIN_AFTER::$mainAfter"
    Write-Output "INTEGRATED::$branch ($ahead commit(s))"
    Write-Output 'Ensuite : git push origin main (le pre-push compile le canonique), puis'
    Write-Output "          tools\unreal\agent-worktree.ps1 prune -Mission $Mission"
  }

  'prune' {
    # Etape 6 de la passe (docs/unreal/OPERATIONS.md) : worktree, branche et enregistrement
    # MCP local d une mission entierement dans main. `git branch -d` ne sert pas ici : il
    # compare a la branche extraite du canonique, pas a main.
    Require-Mission
    $branch = Branch-Of $Mission
    $path = Path-Of $Mission
    if ((Invoke-Git -C $Canonical rev-parse --verify --quiet $branch).Code -ne 0) { Fail "FAIL: branche introuvable -> $branch" }
    $outside = [int](Invoke-Git -C $Canonical rev-list --count "main..$branch").Out[0]
    if ($outside -gt 0) { Fail "FAIL: $outside commit(s) de $branch absents de main : integrer d abord" }
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
