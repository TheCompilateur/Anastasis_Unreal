# Banc d'essai de `agent-worktree.ps1 integrate` et `prune`, sur un depot jetable.
#
# Ne touche jamais au vrai main : copie AGENTS.md, tools/unreal et le hook
# reference-transaction dans un depot neuf sous %TEMP%, et y fait tourner une copie du
# script dont seules les constantes $Canonical et $WorktreeRoot changent.
# A relancer apres toute modification de agent-worktree.ps1. Une ligne FAIL = regression.
#
# Usage : tools\unreal\test-agent-worktree.ps1   (teste la racine qui contient ce script)
param([string]$Source = ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')))
$ErrorActionPreference = 'Continue'
$base = Join-Path ([IO.Path]::GetTempPath()) 'anastasis-test-agent-worktree'
if (Test-Path $base) { Remove-Item $base -Recurse -Force }
$repo = Join-Path $base 'canon'; $wtRoot = Join-Path $base 'worktrees'; $harness = Join-Path $base 'harness'
New-Item -ItemType Directory -Force $repo, $wtRoot, $harness | Out-Null

# Script sous test : copie, seules les deux constantes de chemin changent.
Copy-Item "$Source\tools\unreal\mcp-port.ps1", "$Source\tools\unreal\tools-index.ps1", "$Source\tools\unreal\editor-launch.ps1" $harness
(Get-Content "$Source\tools\unreal\agent-worktree.ps1" -Raw).
  Replace("`$Canonical = 'C:\dev\ANASTASIS_UNREAL'", "`$Canonical = '$repo'").
  Replace("`$WorktreeRoot = 'C:\dev\ANASTASIS_WORKTREES'", "`$WorktreeRoot = '$wtRoot'") |
  Set-Content "$harness\agent-worktree.ps1" -Encoding UTF8
function AW { $o = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$harness\agent-worktree.ps1" @args 2>&1 | ForEach-Object { "$_" }; [PSCustomObject]@{ Code = $LASTEXITCODE; Out = ($o -join "`n") } }
function G { & git -C $repo @args 2>&1 | ForEach-Object { "$_" } }
function Check($name, $cond, $detail) { Write-Output ("{0,-6} {1}" -f $(if ($cond) { 'PASS' } else { 'FAIL' }), $name); if (-not $cond -and $detail) { Write-Output "       $detail" } }

# Depot : AGENTS.md + tools/unreal + le vrai hook reference-transaction.
Copy-Item "$Source\AGENTS.md" $repo
New-Item -ItemType Directory -Force "$repo\tools\unreal", "$repo\tools\git-hooks" | Out-Null
Copy-Item "$Source\tools\unreal\*" "$repo\tools\unreal" -Recurse
Copy-Item "$Source\tools\git-hooks\reference-transaction" "$repo\tools\git-hooks"
G init -q -b main | Out-Null
G config user.email bench@local | Out-Null; G config user.name bench | Out-Null
G config core.autocrlf false | Out-Null
G add -A | Out-Null; G commit -q -m base | Out-Null
G config core.hooksPath tools/git-hooks | Out-Null

function NewBranch($name, [scriptblock]$change) {
  G branch "agent/$name" main | Out-Null
  $wt = Join-Path $base "tmp-$name"
  G worktree add -q $wt "agent/$name" | Out-Null
  & $change $wt
  & git -C $wt add -A 2>&1 | Out-Null; & git -C $wt commit -q -m $name 2>&1 | Out-Null
  G worktree remove $wt | Out-Null
}

# 0. Le hook est actif : un merge direct de main sans ANASTASIS_INTEGRATION est refuse.
NewBranch 'hook' { param($w) Set-Content "$w\hook.txt" 'x' }
$head0 = (G rev-parse main)
G merge -q --ff-only agent/hook | Out-Null
Check 'hook actif : merge direct refuse' ((G rev-parse main) -eq $head0)
G checkout -q -f main | Out-Null; G clean -fdq | Out-Null

# 1. Canonique sur main, propre : avance rapide, copie de travail mise a jour.
NewBranch 't1' { param($w) Set-Content "$w\doc1.md" 'un' }
$r = AW integrate -Mission t1
Check 'S1 canonique sur main : integre' ($r.Code -eq 0 -and (G rev-parse main) -eq (G rev-parse agent/t1)) $r.Out
Check 'S1 copie de travail a jour, propre' ((Test-Path "$repo\doc1.md") -and -not (G status --porcelain))

# 2. Canonique sur une autre branche, avec un fichier suivi modifie : main bouge, rien d'autre.
G checkout -q -b autre | Out-Null
Add-Content "$repo\AGENTS.md" 'travail en cours d un autre agent'
NewBranch 't2' { param($w) Set-Content "$w\doc2.md" 'deux' }
$autreAvant = (G rev-parse autre); $dirtyAvant = (G status --porcelain) -join '|'
$r = AW integrate -Mission t2
Check 'S2 canonique hors main : main avance' ($r.Code -eq 0 -and (G rev-parse main) -eq (G rev-parse agent/t2)) $r.Out
Check 'S2 branche extraite intacte' ((G rev-parse autre) -eq $autreAvant -and (G branch --show-current) -eq 'autre')
Check 'S2 copie de travail intacte' (((G status --porcelain) -join '|') -eq $dirtyAvant -and -not (Test-Path "$repo\doc2.md"))
Check 'S2 message explicite' ($r.Out -match "le canonique est sur 'autre'")

# 3. Pas d'avance rapide (main a bouge depuis la branche) : refus, rien ne change.
G branch agent/t3 'main~1' | Out-Null
$wt = Join-Path $base 'tmp-t3'; G worktree add -q $wt agent/t3 | Out-Null
Set-Content "$wt\doc3.md" 'trois'; & git -C $wt add -A 2>&1 | Out-Null; & git -C $wt commit -q -m t3 2>&1 | Out-Null; G worktree remove $wt | Out-Null
$mainAvant = (G rev-parse main)
$r = AW integrate -Mission t3
Check 'S3 non fast-forward refuse' ($r.Code -ne 0 -and (G rev-parse main) -eq $mainAvant -and $r.Out -match 'pas d avance rapide') $r.Out

# 4. Lanceur Unreal direct dans l'arbre a integrer : refus.
G checkout -q -f main | Out-Null
NewBranch 't4' { param($w)
  # En deux morceaux : ecrite d'un bloc, cette ligne ferait echouer Find-RawEditorLaunch sur CE fichier.
  Set-Content "$w\tools\unreal\bad-capture.ps1" ('$p=Start-' + 'Process $Editor -ArgumentList $a -PassThru')
  (Get-Content "$w\AGENTS.md" -Raw).Replace('| `mcp-port.ps1` |', "| ``bad-capture.ps1`` | test |`n| ``mcp-port.ps1`` |") | Set-Content "$w\AGENTS.md" -NoNewline }
$mainAvant = (G rev-parse main)
$r = AW integrate -Mission t4
Check 'S4 lanceur direct refuse' ($r.Code -ne 0 -and (G rev-parse main) -eq $mainAvant -and $r.Out -match 'bad-capture.ps1:1') $r.Out

# 5. Script non indexe dans l'arbre a integrer : refus.
NewBranch 't5' { param($w) Set-Content "$w\tools\unreal\orphelin.py" 'x' }
$r = AW integrate -Mission t5
Check 'S5 script non indexe refuse' ($r.Code -ne 0 -and (G rev-parse main) -eq $mainAvant -and $r.Out -match 'MISSING orphelin.py') $r.Out

# 6. Deja integre : rien a faire, code 0.
$r = AW integrate -Mission t1
Check 'S6 deja integre : NOTHING_TO_INTEGRATE' ($r.Code -eq 0 -and $r.Out -match 'NOTHING_TO_INTEGRATE') $r.Out

# 7. prune : refuse une branche non integree ; supprime worktree + branche d'une branche integree.
$r = AW prune -Mission t3
Check 'S7 prune refuse une branche hors main' ($r.Code -ne 0 -and (G rev-parse --verify --quiet agent/t3)) $r.Out
G branch agent/t7 main | Out-Null
G worktree add -q (Join-Path $wtRoot 't7') agent/t7 | Out-Null
Set-Content (Join-Path $wtRoot 't7\doc7.md') 'sept'; & git -C (Join-Path $wtRoot 't7') add -A 2>&1 | Out-Null; & git -C (Join-Path $wtRoot 't7') commit -q -m t7 2>&1 | Out-Null
$r1 = AW integrate -Mission t7
$r = AW prune -Mission t7
Check 'S7 prune apres integration' ($r1.Code -eq 0 -and $r.Code -eq 0 -and -not (Test-Path (Join-Path $wtRoot 't7')) -and -not (G rev-parse --verify --quiet agent/t7)) ($r1.Out + "`n" + $r.Out)

# Mission complete sous $wtRoot : worktree, fiche de passation, commits.
function NewMission($name, [scriptblock[]]$changes) {
  G branch "agent/$name" main | Out-Null
  $w = Join-Path $wtRoot $name
  G worktree add -q $w "agent/$name" | Out-Null
  New-Item -ItemType Directory -Force "$w\docs\unreal\handoffs" | Out-Null
  Set-Content "$w\docs\unreal\handoffs\$name.md" "# HANDOFF: $name"
  & git -C $w add -A 2>&1 | Out-Null; & git -C $w commit -q -m "$name fiche" 2>&1 | Out-Null
  foreach ($c in $changes) { & $c $w; & git -C $w add -A 2>&1 | Out-Null; & git -C $w commit -q -m $name 2>&1 | Out-Null }
  return $w
}

# 8. finish sans changement Unreal : ni build ni tests, HANDOFF_READY et marqueur sur le commit.
$w8 = NewMission 'b1' @({ param($w) Set-Content "$w\doc-b1.md" 'b1' })
$r = AW finish -Mission b1
$marker = Join-Path $wtRoot '.handoff\b1.txt'
Check 'S8 finish docs seulement : build et tests sautes' ($r.Code -eq 0 -and $r.Out -match 'TESTS::SKIP' -and $r.Out -match 'HANDOFF_READY::YES') $r.Out
Check 'S8 marqueur HANDOFF_READY = commit de la branche' ((Test-Path $marker) -and ((Get-Content $marker -Raw).Trim() -eq (G rev-parse agent/b1)))

# 9. finish avec un changement Unreal : le build est tente (il echoue sur ce depot sans moteur).
$w9 = NewMission 'u1' @({ param($w) New-Item -ItemType Directory -Force "$w\Source" | Out-Null; Set-Content "$w\Source\x.cpp" '// x' })
$r = AW finish -Mission u1
Check 'S9 finish C++ : le build est tente, pas de HANDOFF_READY' ($r.Code -ne 0 -and $r.Out -match 'UNREAL_CHANGE::OUI' -and $r.Out -notmatch 'HANDOFF_READY::YES' -and -not (Test-Path (Join-Path $wtRoot '.handoff\u1.txt'))) $r.Out

# 10. integrate-batch : b1 et b2 (deux commits dependants) prets, b3 sans finish, b4 en
#     conflit avec b1 : main avance d'un coup avec b1 et b2, b3 et b4 sont ecartees.
$w2 = NewMission 'b2' @({ param($w) Set-Content "$w\doc-b2.md" 'premier' }, { param($w) Set-Content "$w\doc-b2.md" 'second' })
$null = AW finish -Mission b2
$null = NewMission 'b3' @({ param($w) Set-Content "$w\doc-b3.md" 'b3' })
$null = NewMission 'b4' @({ param($w) Set-Content "$w\doc-b1.md" 'autre contenu' })
$null = AW finish -Mission b4
$mainAvant = (G rev-parse main)
$r = AW integrate-batch -Missions 'b1,b2,b3,b4'
$apres = (G rev-parse main)
Check 'S10 lot : main avance' ($r.Code -eq 0 -and $apres -ne $mainAvant -and $r.Out -match 'BATCH_INTEGRATED::b1, b2') $r.Out
Check 'S10 lot : contenu de b1 et b2 dans main, dans l ordre' (((G show 'main:doc-b1.md') -eq 'b1') -and ((G show 'main:doc-b2.md') -eq 'second'))
Check 'S10 lot : b3 ecartee (pas de finish)' ($r.Out -match 'BATCH_REJECTED::b3 : pas de HANDOFF_READY')
Check 'S10 lot : b4 ecartee (conflit), main sans son contenu' ($r.Out -match 'BATCH_REJECTED::b4 : conflit' -and (G show 'main:doc-b1.md') -eq 'b1')
Check 'S10 lot : un seul portail, sans build (docs seulement)' (([regex]::Matches($r.Out, 'TESTS::SKIP')).Count -eq 1)

# 11. prune apres un lot : les commits sont dans main par contenu (copies), pas par identite.
$r = AW prune -Mission b1
Check 'S11 prune reconnait une mission versee par lot' ($r.Code -eq 0 -and -not (G rev-parse --verify --quiet agent/b1) -and -not (Test-Path $marker)) $r.Out

# 12. Branche modifiee apres finish : refusee par le lot.
& git -C $w2 commit -q --allow-empty -m 'apres finish' 2>&1 | Out-Null
$r = AW integrate-batch -Missions 'b2'
Check 'S12 lot : commit posterieur a finish refuse' ($r.Code -ne 0 -and $r.Out -match 'BATCH_REJECTED::b2 : pas de HANDOFF_READY') $r.Out

Remove-Item $base -Recurse -Force -ErrorAction SilentlyContinue
