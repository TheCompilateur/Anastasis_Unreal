# HANDOFF: aaa-visual-lab

## MISSION

Laboratoire visuel isolé de 30 m. Prouver si le pipeline actuel peut porter une scène haut de gamme, sans réécrire la map, les PNJ, le ciel, l'eau ou la forêt.

## FILES_OWNED

- `tools/unreal/aaa-visual-lab.py`
- `tools/unreal/aaa-visual-lab.ps1`
- `docs/unreal/AAA_VISUAL_TARGET_LAB.md`
- `docs/unreal/handoffs/aaa-visual-lab.md`
- `AGENTS.md` (une section d'index)
- `/Game/Anastasis/LookDev/AAA_Lab/**`

## COMMIT

`feat(lookdev): isoler un laboratoire visuel de 30 m` et les commits qui suivent sur `agent/aaa-visual-lab`.

## MEC

- BUILD: relancé par `finish`. Pas de C++ de mission.
- ÉDITEUR: `AAA_LAB::CAPTURE_COMPLETE` sur `/Engine/Maps/Entry`, MCP 8559. Dossier `Saved/SliceEvidence/aaa-visual-lab-012/`. Ligne `AAA_LAB_COMPLETE cameras=3`.
- SOL: dalle `SM_AAA_Ground_30m`, photo `T_Ground_Worked` (1,3 m) via `MI_AAA_Soil`. La pierre lit `T_Ground_Rock`. Le sinus micro n'est plus la normale du sol.
- CONTACT: `SM_AAA_Contact` / `MI_AAA_Contact`, trois taches (pied de mur, bout de poutre, flaque). Ombre de contact du soleil à 0,2.
- ANNEAU (recette, pas régénérée) : `SM_Tree_Broadleaf_Canopy_01` à 393 cm en (−310, −40) ; huit `SM_Grass_MeadowTall_01` autour de (160, −420) ; planche 5 du mur à 42 % de la hauteur ; `SM_House_Refuge_01` à 440 cm en (−600, 70), matériaux du composant remplacés par `MI_AAA_Wood`. Le sous-bois d’un mètre reste au fond. Aucun asset de production n’est réécrit. Capture 013 non lancée (RAM libre sous 4,2 Go). La map sauvée reste celle du run 012.
- AUDIT: 115 meshes, B 34, C 79, D 2, A 0. Nanite production : 0. Labo : Nanite sur pierre, mur, poutre, dalles.
- DÉCAL: non posé (`DecalBlendMode` protégé).
- TESTS: voir `finish`.
- COMMANDES :

```powershell
cd C:\dev\ANASTASIS_WORKTREES\aaa-visual-lab
tools\unreal\anastasis-unreal.ps1 build
tools\unreal\aaa-visual-lab.ps1 -OutDir C:\dev\ANASTASIS_WORKTREES\aaa-visual-lab\Saved\SliceEvidence\aaa-visual-lab-012
```

- `python -m py_compile tools/unreal/aaa-visual-lab.py` : OK.

## SCN

`/Game/Anastasis/LookDev/AAA_Lab/Lvl_AAA_VisualLab`, 30 m. Caméras : A (380, -620, 165) → (40, -80, 90) ; B (980, -1280, 250) → (40, 180, 110) ; C (1750, -2100, 780) → (0, 280, 90). Le sous-système d'interaction du village s'initialise dans tout monde du module : le log signale des SmartObject sans schéma. Aucun PNJ n'est placé.

## PLY

NOT_IMPLEMENTED. Le mannequin du labo est une boîte de 180 cm, hors du système PNJ.

## INTEGRATION_RISK

- `AGENTS.md` est un fichier que d'autres agents éditent : une seule section ajoutée.
- Les `.uasset` sont confinés à `/Game/Anastasis/LookDev/AAA_Lab/`.
- Aucun matériau ni mesh de production n'est réécrit. Les photos de sol sont lues, pas réimportées.

## STOP

- Pas de FPS du labo. Les CVars runtime n'ont pas été relues (`ConsoleManager` absent du module Python).
- Le décal stain n'est pas livré. Le contact est un mesh.
- Le bois du labo garde un fil procédural court.
- L’anneau proche est dans le script. Il n’a pas été vu : pas de dossier `aaa-visual-lab-013`, pas de hauteurs `HERO` mesurées.
- Les captures gardent les icônes de lumière et le cadre de sélection.
- Pas de modification de Lumen, du ciel, de l'herbe, des PNJ, de la rivière, de la map.
