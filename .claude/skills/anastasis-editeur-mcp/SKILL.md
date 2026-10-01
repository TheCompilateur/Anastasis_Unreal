---
name: anastasis-editeur-mcp
description: Inspecter l'éditeur Unreal vivant d'ANÁSTASIS par MCP (outils mcp__unreal__*) — s'assurer qu'on parle au bon éditeur, trouver un groupe d'outils, appeler AnastasisInspectTools. À charger avant tout appel mcp__unreal__* et dès qu'on veut lire l'état d'une scène, d'un acteur ou d'une tuile sans passer par des scripts.
---

# Inspecter l'éditeur vivant par MCP

Contexte : `AGENTS.md`, section « Éditeur vivant : MCP Unreal ».

## 1. Avoir un éditeur, sur le bon port

Chaque racine a son port : 8000 pour le canonique, 8100–8899 pour un worktree (dérivé du nom de mission).

```powershell
tools\unreal\anastasis-unreal.ps1 status    # MCP_URL:: de cette racine
tools\unreal\anastasis-unreal.ps1 editor    # lance l'éditeur de cette racine sur son port, hors écran, sans focus
```

- Premier boot d'un worktree : 2 à 10 min avant que le port écoute. Ne pas conclure au blocage.
- Le client prend son port au **démarrage de la session** : `.mcp.json` (8000) dans le canonique, portée
  locale posée par `agent-worktree.ps1 create` dans un worktree. Worktree créé avant 2026-09-29 :
  `agent-worktree.ps1 mcp -Mission <m>`, puis nouvelle session.
- Outils `mcp__unreal__*` absents ou « Failed to connect » : aucun éditeur n'écoute sur ce port. Ce n'est
  jamais une raison de tuer l'éditeur d'un autre agent.

## 2. Vérifier à qui on parle — toujours, en premier

```
mcp__unreal__call_tool
  toolset_name: anastasis_toolset.toolsets.inspect.AnastasisInspectTools
  tool_name:    get_session_snapshot
```

Comparer `project_dir` à ta racine (`C:/dev/ANASTASIS_WORKTREES/<mission>/` ou `C:/dev/ANASTASIS_UNREAL/`).
**S'il ne correspond pas, tu inspectes l'éditeur d'un autre agent : ne rien en conclure sur ton code.**
Le port 8000 est parfois tenu par un éditeur batch ou par un worktree Codex.

`pie` dit si une session de jeu tourne ; plusieurs outils d'inspection du monde n'ont de sens qu'en PIE.

## 3. Trouver l'outil

La recherche d'outils est active : 3 méta-outils seulement.

1. `mcp__unreal__list_toolsets` — une ligne par groupe ; repérer le bon.
2. `mcp__unreal__describe_toolset` sur ce seul groupe — noms et schémas.
3. `mcp__unreal__call_tool` avec `toolset_name` + `tool_name` + `arguments`.

Groupes utiles :

| Groupe | Pour |
|---|---|
| `anastasis_toolset.toolsets.inspect.AnastasisInspectTools` | lecture seule ANÁSTASIS : snapshot, acteurs (`list_level_actors`, `inspect_actor`), `inspect_tile`, `inspect_settlement`, `inspect_visual_scene_state`, `verify_world_contract` |
| `EditorToolset.LogsToolset` | lire le journal de l'éditeur, régler la verbosité d'une catégorie |
| `EditorToolset.EditorAppToolset` | CVars, sélection, caméra du viewport, PIE |
| `AutomationTestToolset.AutomationTestToolset` | lancer des tests depuis l'éditeur (`DiscoverTests` d'abord) |
| `editor_toolset.toolsets.scene.SceneTools` | niveau chargé : charger un niveau, placer / retirer des acteurs, caméra |
| `editor_toolset.toolsets.object.ObjectTools` | lire / modifier les propriétés d'un objet ou d'une classe |
| `editor_toolset.toolsets.actor.ActorTools`, `.asset.AssetTools`, `.material.MaterialTools`, `.static_mesh.StaticMeshTools`… | modifier acteurs et assets (`list_toolsets` pour la liste complète, ~22 groupes) |
| `editor_toolset.toolsets.programmatic.ProgrammaticToolset` | enchaîner plusieurs appels d'outils dans un petit script, en un aller-retour |

## 4. Règles

- Lire avant d'écrire. Les groupes `editor_toolset.*` **modifient** l'éditeur : uniquement dans l'éditeur
  de TON worktree, jamais dans le canonique ni chez un autre agent, et rien n'est sauvé sans intention.
- Une valeur lue par MCP est une observation, pas une preuve de passation : la preuve reste `finish`
  (build + `report-tests`) et les scripts de capture.
- `list_selected_actors` renvoie un `unreal.Array`, pas une `list`, et une `ValueError` d'un outil peut
  remonter en simple script error (deux KNOWN_EXPECTED_FAILURE du registre).
- Ce qui transite par ce plugin est « Licensed Technology » au sens de l'EULA Unreal (section 6(e)).
