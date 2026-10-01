# HANDOFF: skills-procedures

## MISSION

Écrire une fois les procédures que chaque agent refait : cycle de mission (worktree → push), inspection de
l'éditeur vivant par MCP, preuve visuelle A/B. Sous forme de skills Claude Code, lisibles par tout agent.

Choix : pas une « bible des outils Unreal ». Les outils se décrivent eux-mêmes (MCP `list_toolsets`, en-têtes
de scripts, index d'AGENTS.md) ; un skill ne garde que l'ordre et les décisions, se charge seulement quand la
tâche s'y prête, et renvoie à AGENTS.md (règles) et PIEGES_UNREAL.md (pièges) au lieu de les recopier.

## FILES_OWNED

- `.claude/skills/anastasis-mission/SKILL.md` -- create, build, fiche, finish (table des échecs), boucle
  rebase/finish/integrate quand main bouge, push et sa collision, prune
- `.claude/skills/anastasis-editeur-mcp/SKILL.md` -- port par racine, `get_session_snapshot` / `project_dir`
  d'abord, méta-outils, groupes utiles (noms relevés sur le serveur vivant), règles lecture / écriture
- `.claude/skills/anastasis-capture/SKILL.md` + `compare.py` -- A/B à une variable, regarder, mesurer,
  variance de référence 3,6 %
- `AGENTS.md` -- section « Procédures (skills) »
- `docs/unreal/PIEGES_UNREAL.md` -- piège « push refusé par le pre-push alors que le code compile »

## COMMIT

Voir `git log agent/skills-procedures`.

## MEC

- BUILD / TESTS : voir `finish` (aucun C++, aucun script de `tools/unreal/` modifié).
- MCP : déroulé du skill exécuté dans la session qui l'a écrit, client Claude Code réel sur 8000 :
  `get_session_snapshot` → `project_dir = C:/dev/ANASTASIS_UNREAL/`, `world = Lvl_AnastasisSlice`,
  136 acteurs ; `list_toolsets` → noms de groupes reportés tels quels.
- `compare.py` : image contre elle-même 0,00 % ; rectangle de 1,02 % de l'image → 1,02 % mesuré ;
  tailles différentes → refus, code 2.
- En-têtes des trois SKILL.md : `name` = nom du dossier, `description` présente.

## SCN

N/A

## PLY

N/A

## INTEGRATION_RISK

- **Découverte des skills non vérifiée en direct** : `claude -p` a échoué (« OAuth session expired » de la CLI).
  Preuve attendue : la prochaine session Claude Code ouverte dans le dépôt liste `anastasis-mission`,
  `anastasis-editeur-mcp`, `anastasis-capture`.
- Codex ne charge pas les skills Claude ; il les lit via le renvoi d'AGENTS.md.
- Un skill dérive comme toute doc : AGENTS.md demande de le corriger dans le même commit que la procédure.
  Rien ne le contrôle mécaniquement.

## STOP

Ne revendique pas : un skill par script de capture ; la couverture de la passe d'intégration multi-branches.
