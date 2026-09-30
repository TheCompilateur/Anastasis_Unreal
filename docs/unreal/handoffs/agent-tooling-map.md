# HANDOFF: agent-tooling-map

## MISSION

Qu'un agent deploye dans le projet trouve vite les outils Unreal, et parle a SON editeur :
brancher le serveur MCP de l'editeur cote agent, donner un port par racine, cartographier
`tools/unreal/` et empecher cette carte de deriver.

Constat de depart : le plugin `ModelContextProtocol` etait active (port 8000, recherche d'outils,
`AnastasisInspectTools`) mais aucun client n'y etait relie (`mcpServers` vide pour le projet, pas de
`.mcp.json`). Tous les editeurs visaient 8000. AGENTS.md ne citait que 4 des 36 fichiers de
`tools/unreal/`.

## FILES_OWNED

- `.mcp.json` (nouveau) -- serveur `unreal`, `http://localhost:8000/mcp` (port du canonique)
- `tools/unreal/mcp-port.ps1` (nouveau) -- `Get-AnastasisMcpPort` : 8000 canonique, 8100-8899 par mission (FNV-1a)
- `tools/unreal/tools-index.ps1` (nouveau) -- `Test-AnastasisToolsIndex` : MISSING / STALE entre dossier et index
- `tools/unreal/anastasis-unreal.ps1` -- `editor` passe `-ModelContextProtocolPort`, `status`/`editor` affichent `MCP_URL::`
- `tools/unreal/agent-worktree.ps1` -- `create` enregistre le port (Claude Code, portee locale) ; nouvelle commande `mcp` ;
  `finish` refuse si l'index derive
- `tools/unreal/project-health.ps1` -- ligne `TOOLS INDEX`, derive = YELLOW
- `AGENTS.md` -- sections « Éditeur vivant : MCP Unreal » et « Index de `tools/unreal/` » ; table des commandes ;
  correction de la phrase sur le refus hors racine (les worktrees sont acceptes)
- `Content/Python/anastasis_toolset/toolsets/inspect.py` -- `get_session_snapshot` renvoie `project_dir`
- `Content/Python/anastasis_toolset/tests/test_inspect.py` -- cle `project_dir` exigee + test qu'elle contient le `.uproject`

## COMMIT

Voir `git log agent/agent-tooling-map`.

## MEC

- BUILD: PASS -- `BUILD::PASS` (Editor Win64 Development, premier build du worktree, 161 s)
- TESTS: PASS -- `report-tests.ps1` : PASS 81, KNOWN_EXPECTED_FAILURE 4, FAIL 0, TOTAL 85 / 85 annonces (run complet).
  Nouveau : `test_get_session_snapshot_project_dir_holds_the_uproject` Success.
- MCP bout en bout : editeur du worktree lance par `anastasis-unreal.ps1 editor`, a ecoute sur 127.0.0.1:8498.
  `call_tool AnastasisInspectTools.get_session_snapshot` a renvoye
  `project_dir = C:/dev/ANASTASIS_WORKTREES/agent-tooling-map/`, `world = Lvl_AnastasisSlice`, engine 5.8.2-56702186.
  `claude mcp get unreal` depuis le worktree : Local config, `http://localhost:8498/mcp`, Connected.
- Index : `Test-AnastasisToolsIndex` MISSING [] STALE [] ; fixture negative -> `orphan.py` MISSING, `ghost.ps1` STALE.
  `health` : `TOOLS INDEX PASS`.
- COMMANDS:
  - `tools\unreal\anastasis-unreal.ps1 build`
  - `tools\unreal\report-tests.ps1`
  - `tools\unreal\anastasis-unreal.ps1 editor` puis client MCP (initialize, tools/list, call_tool)
  - `tools\unreal\agent-worktree.ps1 mcp -Mission agent-tooling-map` (deux fois : idempotent)

## SCN

N/A -- aucun changement de scene ni d'asset.

## PLY

N/A -- PLAYER non touche.

## INTEGRATION_RISK

- Les editeurs batch (`verify`, `report-tests`, captures) gardent 8000. Observe pendant la mission : 8000 tenu par un
  editeur de worktree Codex (`.codex\worktrees\asset-map-002`). Le controle `project_dir` reste obligatoire (AGENTS.md).
- Collision possible de port entre deux missions (800 valeurs) ; aucune parmi les 18 racines actuelles.
- Les worktrees crees avant cette mission n'ont pas d'enregistrement local : `agent-worktree.ps1 mcp -Mission <m>`.
- Codex n'est pas couvert (config MCP globale dans `~/.codex/config.toml`, hors depot).
- `.mcp.json` : Claude Code demande d'approuver le serveur a la premiere session dans le canonique.
- AGENTS.md et `agent-worktree.ps1` sont des fichiers chauds.
- Hors perimetre, observe : pendant la mission, la fenetre principale de plusieurs editeurs (les miens, et deux runs
  `report-tests`) a ete detruite de l'exterieur (`Window 'Anastasis_UnrealV2 - Unreal Editor' being destroyed` puis
  `QUIT_EDITOR`), ~20 s apres le demarrage. Aucun script du depot ne ferme de fenetre. Cause non identifiee. Un run
  `report-tests` interrompu ainsi est bien rapporte RUN_INCOMPLET, jamais PASS.

## STOP

Ne revendique pas : que tous les groupes d'outils MCP du moteur fonctionnent ; un port pour les editeurs batch ;
la couverture Codex. Ne touche ni au C++, ni aux assets, ni a la config du plugin.
