# HANDOFF: integration-proof-closure-001

## MISSION

Empêcher `integrate-batch` de verser implicitement le code d'une mission ancêtre sans admettre cette mission et recueillir ses preuves déclarées.

## FILES_OWNED

- `tools/unreal/agent-worktree.ps1`
- `tools/unreal/test-agent-worktree.ps1`
- `.claude/skills/anastasis-mission/SKILL.md`
- `docs/unreal/handoffs/integration-proof-closure-001.md`

## COMMIT

Le commit qui contient cette fiche sur `agent/integration-proof-closure-001`.

## MEC

- Analyse syntaxique PowerShell des deux scripts : `PARSER::PASS`.
- `git diff --check` : PASS.
- `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/unreal/test-agent-worktree.ps1` : tous les contrôles S1 à S23 affichent PASS, code de sortie 0, y compris après rebase sur `main` contenant `integration-targeted-gate-001`. Le banc utilise désormais un dépôt temporaire unique par exécution et copie le script de `soil-contact-capture` référencé par le registre.
- S22 : une descendante seule est refusée ; l'ordre ancêtre puis descendante est admis.
- S23 : une descendante seule est refusée quand la fiche de l'ancêtre est déjà sur `main` par contenu mais que son commit de code manque.

## PROOFS

PROOFS: (aucune)

## SCN

UNKNOWN : aucun éditeur ni asset impliqué.

## PLY

UNKNOWN : aucun parcours joueur.

## INTEGRATION_RISK

- `integration-targeted-gate-001` modifie aussi `tools/unreal/agent-worktree.ps1`. Cette branche a été rebasée sans conflit sur `e681e629`, qui contient cette mission ; le banc complet a été rejoué. Si `main` avance encore avant versement, comparer l'arbre assemblé. Le garde intervient avant le cherry-pick de chaque mission du lot et ne déplace pas `main` à lui seul.
- Le contrôle reconnaît les fiches des commits importés et les branches `agent/*` ancêtres dont des commits restent absents par contenu. Une mission ancêtre doit être admise plus tôt dans le lot, ou versée séparément, pour que ses preuves soient recueillies.
- Les preuves `river-use-pie` et `terrain-access-pie` omises dans un lot antérieur restent à rejouer sur l'arbre intégré ; ce correctif ne les qualifie pas rétroactivement.

## STOP

Pas de preuve PIE, de build Unreal ou de validation joueur revendiqués. Le portail `integrate` d'une mission seule n'est pas modifié par cette mission.
