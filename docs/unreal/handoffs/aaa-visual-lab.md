# HANDOFF: aaa-visual-lab

## MISSION

Laboratoire visuel isolé de 30 m. Prouver si le pipeline actuel peut porter une scène haut de gamme, sans réécrire la map, les PNJ, le ciel, l'eau ou la forêt.

## FILES_OWNED

- `tools/unreal/aaa-visual-lab.py`
- `tools/unreal/aaa-visual-lab.ps1`
- `docs/unreal/AAA_VISUAL_TARGET_LAB.md`
- `docs/unreal/handoffs/aaa-visual-lab.md`
- `AGENTS.md` (une ligne d'index)
- à la première exécution éditeur seulement : `/Game/Anastasis/LookDev/AAA_Lab/**`

## COMMIT

PENDING

## MEC

- BUILD: PASS (worktree, cible éditeur, 14 actions). Pas de C++ de mission : le module du projet seulement.
- TESTS: aucun. Pas de changement C++.
- ÉDITEUR: ouvert sur `/Engine/Maps/Entry`, MCP 8559. Bloqué dans `QueryTargets` derrière l'UBT d'un autre worktree. RAM libre 1,21 Go / 15,90 Go. Processus arrêté. Log : `Saved/SliceEvidence/aaa-visual-lab/lab.log`. Pas de ligne `AAA_LAB_COMPLETE`.
- COMMANDES, quand la RAM libre dépasse 4 Go et qu'aucun `UnrealBuildTool` ne tient le verrou :

```powershell
cd C:\dev\ANASTASIS_WORKTREES\aaa-visual-lab
tools\unreal\anastasis-unreal.ps1 build
tools\unreal\aaa-visual-lab.ps1 -OutDir C:\dev\ANASTASIS_WORKTREES\aaa-visual-lab\Saved\SliceEvidence\aaa-visual-lab-001
```

- `python -m py_compile tools/unreal/aaa-visual-lab.py` : OK.
- Le dossier `Saved/SliceEvidence/aaa-visual-lab` existe déjà (log du run interrompu). Le prochain `-OutDir` doit être un dossier neuf.

## SCN

Le niveau n'existe pas encore sur le disque. Le script le crée depuis `/Engine/Maps/Entry` et quitte s'il n'est pas dans `AAA_Lab`. Caméras écrites dans le script : A (380, -620, 165) → (40, -80, 90) ; B (980, -1280, 250) → (40, 180, 110) ; C (1750, -2100, 780) → (0, 280, 90).

## PLY

NOT_IMPLEMENTED. Le mannequin du labo est une boîte de 180 cm, hors du système PNJ.

## INTEGRATION_RISK

- `AGENTS.md` est un fichier que d'autres agents éditent : une seule ligne ajoutée.
- Les `.uasset` du labo n'existent qu'après l'éditeur. Les intégrer avant ce run n'apporte que la recette.
- Aucun matériau ni mesh de production n'est réécrit.

## STOP

- Pas de captures `cam_a/b/c`.
- Pas de `pipeline.json` runtime : l'anti-aliasing, le screen percentage et le flag Nanite projet ne sont pas dans `DefaultEngine.ini`, donc non affirmés.
- Pas de FPS du labo.
- Pas de modification de Lumen, du ciel, de l'herbe, des PNJ, de la rivière, de la map.
