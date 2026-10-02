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

# Depot : AGENTS.md + tools/unreal + le vrai hook reference-transaction, et ce que lit le
# controle des ecarts de `finish` (check-ecarts.mjs : le registre, la table des masques).
Copy-Item "$Source\AGENTS.md" $repo
New-Item -ItemType Directory -Force "$repo\tools\unreal", "$repo\tools\git-hooks" | Out-Null
Copy-Item "$Source\tools\unreal\*" "$repo\tools\unreal" -Recurse
Copy-Item "$Source\tools\git-hooks\reference-transaction" "$repo\tools\git-hooks"
New-Item -ItemType Directory -Force "$repo\tools\migration\scenarios", "$repo\Source\AnastasisSim" | Out-Null
Copy-Item "$Source\tools\migration\check-ecarts.mjs" "$repo\tools\migration"
Copy-Item "$Source\tools\migration\scenarios\masks.mjs" "$repo\tools\migration\scenarios"
Copy-Item "$Source\Source\AnastasisSim\ECARTS.md" "$repo\Source\AnastasisSim"
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
Check 'S8 marqueur HANDOFF_READY = commit de la branche, mode nounreal' ((Test-Path $marker) -and ((Get-Content $marker -Raw).Trim() -eq ((G rev-parse agent/b1) + ' nounreal')))

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

# --- EDITOR_QUEUE_001 ---------------------------------------------------------------------
$lockFile = Join-Path $wtRoot '.handoff\MAIN.lock'
G checkout -q -f main | Out-Null; G clean -fdq | Out-Null

# 13. Verrou de main tenu par un processus vivant (ce banc) : integrate refuse, main intacte.
NewBranch 'l1' { param($w) Set-Content "$w\doc-l1.md" 'l1' }
New-Item -ItemType Directory -Force (Split-Path $lockFile) | Out-Null
[PSCustomObject]@{ Holder = 'integrate-batch:banc'; Pid = $PID; Since = (Get-Date -Format 'o') } | ConvertTo-Json -Compress | Set-Content $lockFile
$mainAvant = (G rev-parse main)
$r = AW integrate -Mission l1
Check 'S13 verrou tenu : integrate refuse, main intacte' ($r.Code -ne 0 -and $r.Out -match 'MAIN_LOCK::TENU par integrate-batch:banc' -and (G rev-parse main) -eq $mainAvant) $r.Out
$r = AW status
Check 'S13 status montre le verrou' ($r.Out -match 'MAIN_LOCK::TENU integrate-batch:banc') $r.Out

# 14. Verrou perime (processus disparu) : repris, integre, puis rendu.
[PSCustomObject]@{ Holder = 'mort'; Pid = 999999; Since = (Get-Date -Format 'o') } | ConvertTo-Json -Compress | Set-Content $lockFile
$r = AW integrate -Mission l1
Check 'S14 verrou perime repris, integre' ($r.Code -eq 0 -and $r.Out -match 'verrou de main perime \(mort\)' -and (G rev-parse main) -eq (G rev-parse agent/l1)) $r.Out
Check 'S14 verrou rendu apres integrate' (-not (Test-Path $lockFile))

# 15. Une mission dont la suite attend le lot (queued) : integrate la renvoie au lot.
NewBranch 'q1' { param($w) Set-Content "$w\doc-q1.md" 'q1' }
Set-Content (Join-Path $wtRoot '.handoff\q1.txt') ((G rev-parse agent/q1) + ' queued')
$mainAvant = (G rev-parse main)
$r = AW integrate -Mission q1
Check 'S15 mission queued : integrate refuse, renvoie au lot' ($r.Code -ne 0 -and $r.Out -match 'attend le lot' -and (G rev-parse main) -eq $mainAvant -and -not (Test-Path $lockFile)) $r.Out

# 16. Fiche qui declare une preuve absente du registre : finish refuse.
$null = NewMission 'p1' @({ param($w) Add-Content "$w\docs\unreal\handoffs\p1.md" 'PROOFS: inconnue-pie' })
$r = AW finish -Mission p1
Check 'S16 preuve inconnue : finish refuse' ($r.Code -ne 0 -and $r.Out -match 'absente\(s\) de tools/unreal/proofs.txt : inconnue-pie' -and -not (Test-Path (Join-Path $wtRoot '.handoff\p1.txt'))) $r.Out

# 17. Preuve connue declaree, docs seulement : finish passe et la cite ; status la donne prete.
$null = NewMission 'p2' @({ param($w) Add-Content "$w\docs\unreal\handoffs\p2.md" 'PROOFS: village-weather-pie' })
$r = AW finish -Mission p2
Check 'S17 preuve connue : finish passe et la cite' ($r.Code -eq 0 -and $r.Out -match 'PROOFS::village-weather-pie' -and $r.Out -match 'HANDOFF_READY::YES \(nounreal\)') $r.Out
$r = AW status
Check 'S17 status : p2 prete pour le lot, commande donnee' ($r.Out -match 'PRETES_POUR_LE_LOT::.*p2 \(nounreal\)' -and $r.Out -match 'integrate-batch -Missions .*p2') $r.Out

# 18. Registre des preuves : chaque ligne se lit (six champs), chaque script existe ; un lot se
#     prepare sans editeur (-DryRun) ; une preuve inconnue est refusee avant tout demarrage.
$eb = "$repo\tools\unreal\editor-batch.ps1"
$list = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $eb -List 2>&1 | ForEach-Object { "$_" })
$registered = @($list | Where-Object { $_ -like 'PROOF_REGISTERED::*' })
Check 'S18 registre lisible, scripts presents' ($LASTEXITCODE -eq 0 -and $registered.Count -ge 1 -and -not ($registered -match 'script=ABSENT')) ($list -join "`n")
$names = ($registered | ForEach-Object { ($_ -split '::')[1].Split(' ')[0] }) -join ','
$dry = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $eb -Proofs $names -DryRun 2>&1 | ForEach-Object { "$_" }) -join "`n"
Check 'S18 lot prepare sans editeur (-DryRun)' ($LASTEXITCODE -eq 0 -and $dry -match "EDITOR_BATCH::DRYRUN $($registered.Count) preuve") $dry
$bad = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $eb -Proofs 'inconnue-pie' -DryRun 2>&1 | ForEach-Object { "$_" }) -join "`n"
Check 'S18 preuve inconnue refusee avant l editeur' ($bad -match 'absente\(s\) de proofs.txt : inconnue-pie') $bad

# 19. Porte memoire equitable : un ticket plus ancien et vivant passe d'abord ; un ticket mort ne
#     bloque personne. Capacite et memoire neutralisees, $Launch ne lance rien.
. "$harness\editor-launch.ps1"
$env:ANASTASIS_EDITOR_QUEUE_DIR = Join-Path $base 'gate-queue'
New-Item -ItemType Directory -Force $env:ANASTASIS_EDITOR_QUEUE_DIR | Out-Null
$older = Join-Path $env:ANASTASIS_EDITOR_QUEUE_DIR ('1-{0:D19}-{1}.ticket' -f 1, $PID)
New-Item -ItemType File $older | Out-Null
$msg = ''
try { $null = Invoke-AnastasisEditorGated -Launch { 'lance' } -MaxEditors 99 -MinRamGB 0 -MinCommitGB 0 -TimeoutMinutes 0.04 -PollSeconds 1 6>&1 } catch { $msg = "$_" }
Check 'S19 file : un ticket plus ancien et vivant passe d abord' ($msg -match 'EDITOR_GATE::TIMEOUT' -and $msg -match 'file=1 devant') $msg
Remove-Item $older
$dead = Join-Path $env:ANASTASIS_EDITOR_QUEUE_DIR ('1-{0:D19}-{1}.ticket' -f 1, 999999)
New-Item -ItemType File $dead | Out-Null
$got = Invoke-AnastasisEditorGated -Launch { 'lance' } -MaxEditors 99 -MinRamGB 0 -MinCommitGB 0 -TimeoutMinutes 0.04 -PollSeconds 1
Check 'S19 file : ticket d un lanceur mort ignore et retire, pas de ticket laisse' ($got -eq 'lance' -and -not (Test-Path $dead) -and -not @(Get-ChildItem $env:ANASTASIS_EDITOR_QUEUE_DIR -Filter '*.ticket').Count) $got
Remove-Item Env:ANASTASIS_EDITOR_QUEUE_DIR

# --- RETEST_RULE_001 ----------------------------------------------------------------------
# Le build echoue toujours sur ce depot sans moteur : un portail rejoue se voit (UNREAL_CHANGE::OUI,
# pas de HANDOFF_READY). Les preuves `proved` sont posees a la main, comme en S15.
G checkout -q -f main | Out-Null; G clean -fdq | Out-Null
function Prove($m) { Set-Content (Join-Path $wtRoot ".handoff\$m.txt") ((G rev-parse "agent/$m") + ' proved') }
function SourceFile($name) { return { param($w) New-Item -ItemType Directory -Force "$w\Source" | Out-Null; Set-Content "$w\Source\$name.cpp" "// $name" }.GetNewClosure() }
function AdvanceMain($name, [scriptblock]$change) { NewBranch $name $change; $null = AW integrate -Mission $name }

# 20. Arbres Unreal identiques : main n'a bouge qu'en docs depuis la preuve.
$wr1 = NewMission 'r1' @((SourceFile 'r1'))
$null = NewMission 'r2' @((SourceFile 'r2'))
Prove 'r1'; Prove 'r2'
AdvanceMain 'd20' { param($w) Set-Content "$w\doc-d20.md" 'd20' }
$old1 = (G rev-parse agent/r1)
& git -C $wr1 rebase -q main 2>&1 | Out-Null
$new1 = (G rev-parse agent/r1)
$r = AW finish -Mission r1
Check 'S20 finish apres rebase, arbres identiques : RETEST::SKIP, ni build ni suite' ($r.Code -eq 0 -and $new1 -ne $old1 -and $r.Out -match "RETEST::SKIP \(arbres Unreal identiques a $($old1.Substring(0, 7))\)" -and $r.Out -notmatch 'UNREAL_CHANGE::OUI|BUILD::PASS' -and $r.Out -match 'HANDOFF_READY::YES \(proved\)') $r.Out
Check 'S20 marqueur reporte sur le commit rebase, mode conserve' ((Get-Content (Join-Path $wtRoot '.handoff\r1.txt') -Raw).Trim() -eq "$new1 proved")
$mainAvant = (G rev-parse main)
$r = AW integrate -Mission r2
Check 'S20 integrate sans avance rapide, arbres identiques : rejouee sur main et versee' ($r.Code -eq 0 -and $r.Out -match 'RETEST::SKIP' -and $r.Out -notmatch 'UNREAL_CHANGE::OUI' -and (G rev-parse main) -ne $mainAvant -and (G show 'main:Source/r2.cpp') -eq '// r2' -and (G show 'main:doc-d20.md') -eq 'd20') $r.Out
$null = NewMission 'r3' @((SourceFile 'r3'))
Prove 'r3'
AdvanceMain 'd21' { param($w) Set-Content "$w\doc-d21.md" 'd21' }
$r = AW integrate-batch -Missions 'r3'
Check 'S20 lot, arbres identiques : RETEST::SKIP, verse sans portail' ($r.Code -eq 0 -and $r.Out -match 'RETEST::SKIP \(arbres Unreal identiques a \w{7}, finish -Prove de r3\)' -and $r.Out -notmatch 'BUILD::PASS|TESTS::(PASS|QUEUED)' -and $r.Out -match 'BATCH_INTEGRATED::r3' -and (G show 'main:Source/r3.cpp') -eq '// r3') $r.Out

# 21. Un seul fichier Source/ change sur main depuis la preuve : portail complet, jamais de reprise.
#     r1 a ete prouvee avant que r2 et r3 n'arrivent dans main.
$mainAvant = (G rev-parse main)
$r = AW integrate -Mission r1
Check 'S21 integrate, Source/ change depuis la preuve : RETEST::REQUIS, main intacte' ($r.Code -ne 0 -and $r.Out -match 'RETEST::REQUIS \(\d+ fichier\(s\) Unreal differents' -and $r.Out -notmatch 'RETEST::SKIP' -and (G rev-parse main) -eq $mainAvant) $r.Out
$null = NewMission 'r4' @((SourceFile 'r4'))
Prove 'r4'
AdvanceMain 'd22' (SourceFile 'main22')
$mainAvant = (G rev-parse main)
$r = AW integrate-batch -Missions 'r4'
Check 'S21 lot, Source/ change depuis la preuve : portail rejoue (build tente), main intacte' ($r.Code -ne 0 -and $r.Out -match 'UNREAL_CHANGE::OUI' -and $r.Out -notmatch 'RETEST::SKIP' -and (G rev-parse main) -eq $mainAvant) $r.Out
& git -C $wr1 rebase -q main 2>&1 | Out-Null
$r = AW finish -Mission r1
Check 'S21 finish apres rebase, Source/ change : build tente, pas de HANDOFF_READY' ($r.Code -ne 0 -and $r.Out -match 'UNREAL_CHANGE::OUI' -and $r.Out -notmatch 'RETEST::SKIP|HANDOFF_READY::YES' -and (Get-Content (Join-Path $wtRoot '.handoff\r1.txt') -Raw).Trim() -eq "$new1 proved") $r.Out

Remove-Item $base -Recurse -Force -ErrorAction SilentlyContinue
