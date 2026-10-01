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

`feat(lookdev): isoler un laboratoire visuel de 30 m` puis le commit des `.uasset` du labo.

## MEC

- BUILD: PASS (worktree, cible éditeur). Pas de C++ de mission.
- ÉDITEUR: `AAA_LAB::CAPTURE_COMPLETE` sur `/Engine/Maps/Entry`, MCP 8559. Log `Saved/SliceEvidence/aaa-visual-lab-001/lab.log`. Ligne `AAA_LAB_COMPLETE cameras=3`.
- AUDIT: 115 meshes, B 34, C 79, D 2, A 0. Nanite production : 0. Labo : Nanite sur sol, pierre, mur, poutre, dalles.
- DÉCAL: non posé (`DecalBlendMode` protégé).
- TESTS: voir `finish`.
- COMMANDES :

```powershell
cd C:\dev\ANASTASIS_WORKTREES\aaa-visual-lab
tools\unreal\anastasis-unreal.ps1 build
tools\unreal\aaa-visual-lab.ps1 -OutDir C:\dev\ANASTASIS_WORKTREES\aaa-visual-lab\Saved\SliceEvidence\aaa-visual-lab-001
```

- `python -m py_compile tools/unreal/aaa-visual-lab.py` : OK.
- Le dossier `Saved/SliceEvidence/aaa-visual-lab` existe déjà (log du run interrompu). Le prochain `-OutDir` doit être un dossier neuf.

## SCN

`/Game/Anastasis/LookDev/AAA_Lab/Lvl_AAA_VisualLab`, 30 m. Caméras : A (380, -620, 165) → (40, -80, 90) ; B (980, -1280, 250) → (40, 180, 110) ; C (1750, -2100, 780) → (0, 280, 90). Captures `cam_a.png`, `cam_b.png`, `cam_c.png`. Le sous-système d'interaction du village s'initialise dans tout monde du module : le log signale des SmartObject sans schéma. Aucun PNJ n'est placé.

## PLY

NOT_IMPLEMENTED. Le mannequin du labo est une boîte de 180 cm, hors du système PNJ.

## INTEGRATION_RISK

- `AGENTS.md` est un fichier que d'autres agents éditent : une seule section ajoutée, rebasée sur `main`.
- Les `.uasset` sont confinés à `/Game/Anastasis/LookDev/AAA_Lab/`.
- Aucun matériau ni mesh de production n'est réécrit.

## STOP

- Pas de FPS du labo. Les CVars runtime n'ont pas été relues (`ConsoleManager` absent du module Python).
- Le décal stain n'est pas livré.
- Le sol du labo est noir sur `cam_a` : pixel (960, 810) = RGB 0,0,0. Le script demande ensuite un matériau two-sided et Nanite off sur `SM_AAA_Ground_30m`. Ce rebake n'est pas dans les `.uasset` tant qu'un éditeur n'a pas rejoué le script.
- Pas de modification de Lumen, du ciel, de l'herbe, des PNJ, de la rivière, de la map.
