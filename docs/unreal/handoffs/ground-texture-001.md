# HANDOFF: ground-texture-001

## MISSION

Lever la limite n°1 de GROUND_SURFACE_001 (« aucune texture ») : détail photo CC0 sous le
mètre dans `M_AnastasisGround`, sans toucher aux albédos calés ni au C++. Fiche :
`docs/unreal/GROUND_TEXTURE_001.md`.

## FILES_OWNED

- `tools/unreal/ground-textures.py` (nouveau, Python système)
- `tools/unreal/ground-material.py`, `tools/unreal/ground-material.ps1`
- `tools/unreal/ground-cover-capture.py`, `tools/unreal/capture-ground-cover.ps1` (états `*_notex`, `bare`)
- `Content/Anastasis/Materials/M_AnastasisGround.uasset`, `MI_AnastasisGround.uasset`
- `Content/Anastasis/Materials/GroundTextures/T_Ground_*` (8, nouveaux)
- `AGENTS.md` (index), `docs/unreal/GROUND_TEXTURE_001.md`, `docs/visual/ground-texture-001/`

## COMMIT

6778d36 (travail) + ce commit (fiche)

## MEC

- BUILD: PASS (worktree, aucun changement C++)
- TESTS: `finish` -> PASS 180 / KNOWN_EXPECTED_FAILURE 4 / FAIL 0 (184 annonces), HANDOFF_READY::YES
- COMMANDS:
  - `python tools/unreal/ground-textures.py`
  - `tools\unreal\ground-material.ps1 -Rebuild` → `GROUND_MATERIAL::PASS`, 924 instr. pixel, 10 samplers
  - `tools\unreal\ground-material.ps1 -ReimportTextures`
  - `tools\unreal\capture-ground-cover.ps1 -Label tex-v2 -States on,on_notex,bare,bare_notex` → `CAPTURE::PASS`

## SCN

A/B 8 vues × 4 états, mêmes caméras : `Saved/GroundCoverEvidence/tex-v1` (damier constaté),
`tex-v2` (corrigé). Extraits dans `docs/visual/ground-texture-001/`.

## PLY

NOT_IMPLEMENTED (hors mission).

## INTEGRATION_RISK

- `M_AnastasisGround` / `MI_AnastasisGround` sont des binaires LFS : toute autre mission qui
  les régénère en parallèle entre en conflit sans fusion possible ; régénérer depuis le script.
- `ground-material.py -Rebuild` exige `Saved/GroundTextures/packed` **seulement** si les
  textures manquent ; elles sont versionnées, donc un rebuild ordinaire n'a pas besoin de
  réseau.
- `ground-cover-capture.py` : états ajoutés, comportement des états existants inchangé.

## STOP

Ne revendique pas : réglage du relief procédural (dunes encore visibles), distinction
Stone/Ruin, variantes anti-répétition, performance en jeu (mesures éditeur seulement).
