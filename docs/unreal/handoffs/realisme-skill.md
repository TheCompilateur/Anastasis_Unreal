# HANDOFF: realisme-skill

## MISSION

Rendre la recherche « réalisme Unreal » d'Alexandre utilisable par les agents. Un skill
`anastasis-realisme` explique comment Unreal fait chaque domaine du rendu, ce que le projet en a déjà
décidé et mesuré, et comment prouver une amélioration. Un registre classe chaque recommandation de la
recherche (`APPLIQUÉ`, `ÉQUIVALENT`, `REJETÉ`, `OUVERT`, `HORS_PÉRIMÈTRE`, `FAUX`). Il comporte aussi une
procédure pour ingérer les recherches suivantes.

Documentation seulement : aucun fichier C++, Python, script, asset ni ini n'est modifié.

## FILES_OWNED

- `.claude/skills/anastasis-realisme/SKILL.md`
- `.claude/skills/anastasis-realisme/registre.md`
- `.claude/skills/anastasis-realisme/fiches/` : `eclairage.md`, `atmosphere.md`, `terrain.md`, `sol.md`,
  `vegetation.md`, `eau.md`, `post-traitement.md`, `performance.md`
- `docs/recherche/realisme-unreal/README.md`
- `docs/recherche/realisme-unreal/RU-001_resume-executif.pdf` (source, 102 Ko)
- `AGENTS.md` : une ligne dans la table des skills
- `docs/unreal/handoffs/realisme-skill.md`

## COMMIT

Voir `git log` de `agent/realisme-skill`.

## MEC

- BUILD: NOT_APPLICABLE (aucun code touché)
- TESTS: NOT_APPLICABLE
- Session cloud Linux, sans Unreal ni PowerShell : `agent-worktree.ps1 finish` **non exécuté**. Aucun
  script `tools/unreal/` n'est ajouté, donc l'index n'est pas concerné.
- Valeurs citées relues dans le code de `main` @ `b8c6d06` : `Config/DefaultEngine.ini`,
  `AnastasisAtmosphereProfile.h`, `AnastasisWorldAtmosphere.cpp` (exposition fixe EV100 14),
  `create_tree_asset.py` (Nanite off, LOD), et les documents cités dans chaque fiche.
- Paramètres des scripts cités vérifiés dans leurs `param()` : `riverbank-capture.ps1 -Profile`,
  `capture-ground-cover.ps1 -States`, `capture-sky.ps1 -Preset`, `capture-terrain-relief.ps1 -Step`,
  `capture-tree-lineup.ps1 -Set`, `capture-slice.ps1 -PreCmds`.
- Nouveautés UE 5.8 citées par la recherche (Lumen Lite, Mesh Terrain, PVE) recoupées avec les notes de
  version publiques.

## SCN

NOT_APPLICABLE

## PLY

NOT_APPLICABLE

## INTEGRATION_RISK

- Les fiches citent des valeurs de `main` au 2026-10-01. Une mission qui change une de ces valeurs
  corrige la fiche dans le même commit (règle écrite dans `SKILL.md`).
- `agent/env-realism-002` (fond de l'eau, pluie au sol, aubes) n'est pas versée ; `fiches/eau.md` la
  mentionne en Ouvert.

## STOP

- Ne revendique aucune amélioration visuelle : aucune capture, aucun réglage.
- Ne tranche pas la direction artistique (pontique contre méditerranéen) ni le budget GPU : les deux sont
  signalés à Alexandre dans `SKILL.md`, section 6.
- Les statuts `FAUX` du registre reposent sur la connaissance du moteur et les notes de version, pas sur
  un essai en éditeur.
